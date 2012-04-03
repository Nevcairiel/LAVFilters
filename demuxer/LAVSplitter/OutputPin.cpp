/*
 *      Copyright (C) 2010-2012 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  Initial design and concept by Gabest and the MPC-HC Team, copyright under GPLv2
 *  Contributions by Ti-BEN from the XBMC DSPlayer Project, also under GPLv2
 */

#include "stdafx.h"
#include "BaseDemuxer.h"
#include "OutputPin.h"
#include "LAVSplitter.h"
#include "moreuuids.h"

#include "PacketAllocator.h"

CLAVOutputPin::CLAVOutputPin(std::vector<CMediaType>& mts, LPCWSTR pName, CBaseFilter *pFilter, CCritSec *pLock, HRESULT *phr, CBaseDemuxer::StreamType pinType, const char* container, int nBuffers)
  : CBaseOutputPin(NAME("lavf dshow output pin"), pFilter, pLock, phr, pName)
  , m_hrDeliver(S_OK)
  , m_fFlushing(false)
  , m_eEndFlush(TRUE)
  , m_containerFormat(container)
  , m_newMT(NULL)
  , m_pinType(pinType)
  , m_Parser(this, container)
  , m_rtPrev(Packet::INVALID_TIME)
  , m_bPacketAllocator(FALSE)
{
  m_mts = mts;
  m_nBuffers = max(nBuffers, 1);

  SetQueueSizes();
}

CLAVOutputPin::~CLAVOutputPin()
{
  CAMThread::CallWorker(CMD_EXIT);
  CAMThread::Close();
  SAFE_DELETE(m_newMT);
}

void CLAVOutputPin::SetQueueSizes()
{
  int factor = 1;

  if (m_mts.begin()->subtype == MEDIASUBTYPE_DOLBY_TRUEHD) {
    DbgLog((LOG_TRACE, 10, L"Increasing Audio Queue size for TrueHD"));
    factor = 25;
  } else if (m_mts.begin()->subtype == MEDIASUBTYPE_HDMV_LPCM_AUDIO || m_mts.begin()->subtype == MEDIASUBTYPE_BD_LPCM_AUDIO) {
    factor = 5;
  }  else if (m_mts.begin()->majortype == MEDIATYPE_Audio) {
    factor = 2;
  }

  m_dwQueueLow  = MIN_PACKETS_IN_QUEUE * factor;
  m_dwQueueHigh = MAX_PACKETS_IN_QUEUE * factor;
}

HRESULT CLAVOutputPin::GetQueueSize(int& samples, int& size)
{
  CAutoLock lock(&m_queue);
  samples = (int)m_queue.Size();
  size = (int)m_queue.DataSize();
  return S_OK;
}

STDMETHODIMP CLAVOutputPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  return 
    QI(IMediaSeeking)
    QI(ILAVPinInfo)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CLAVOutputPin::DecideAllocator(IMemInputPin * pPin, IMemAllocator **ppAlloc)
{
  HRESULT hr = NOERROR;
  *ppAlloc = NULL;

  // get downstream prop request
  // the derived class may modify this in DecideBufferSize, but
  // we assume that he will consistently modify it the same way,
  // so we only get it once
  ALLOCATOR_PROPERTIES prop;
  ZeroMemory(&prop, sizeof(prop));

  // whatever he returns, we assume prop is either all zeros
  // or he has filled it out.
  pPin->GetAllocatorRequirements(&prop);

  // if he doesn't care about alignment, then set it to 1
  if (prop.cbAlign == 0) {
    prop.cbAlign = 1;
  }

  *ppAlloc = new CPacketAllocator(NAME("CPacketAllocator"), NULL, &hr);
  (*ppAlloc)->AddRef();
  if (SUCCEEDED(hr)) {
    DbgLog((LOG_TRACE, 10, L"Trying to use our CPacketAllocator"));
    m_bPacketAllocator = TRUE;
    hr = DecideBufferSize(*ppAlloc, &prop);
    if (SUCCEEDED(hr)) {
      DbgLog((LOG_TRACE, 10, L"-> DecideBufferSize Success"));
      hr = pPin->NotifyAllocator(*ppAlloc, FALSE);
      if (SUCCEEDED(hr)) {
        DbgLog((LOG_TRACE, 10, L"-> NotifyAllocator Success"));
        return NOERROR;
      }
    }
  }

  if (*ppAlloc) {
    (*ppAlloc)->Release();
    *ppAlloc = NULL;
  }

  m_bPacketAllocator = FALSE;

  /* Try the allocator provided by the input pin */
  hr = pPin->GetAllocator(ppAlloc);
  if (SUCCEEDED(hr)) {

    hr = DecideBufferSize(*ppAlloc, &prop);
    if (SUCCEEDED(hr)) {
      hr = pPin->NotifyAllocator(*ppAlloc, FALSE);
      if (SUCCEEDED(hr)) {
        return NOERROR;
      }
    }
  }

  /* If the GetAllocator failed we may not have an interface */
  if (*ppAlloc) {
    (*ppAlloc)->Release();
    *ppAlloc = NULL;
  }
  return hr;
}

HRESULT CLAVOutputPin::DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProperties)
{
  CheckPointer(pAlloc, E_POINTER);
  CheckPointer(pProperties, E_POINTER);

  HRESULT hr = S_OK;

  pProperties->cBuffers = max(pProperties->cBuffers, (m_bPacketAllocator ? 10 : m_nBuffers));
  pProperties->cbBuffer = max(max(m_mt.lSampleSize, 256000), (ULONG)pProperties->cbBuffer);

  // Vorbis requires at least 2 buffers
  if(m_mt.subtype == MEDIASUBTYPE_Vorbis && m_mt.formattype == FORMAT_VorbisFormat) {
    pProperties->cBuffers = max(pProperties->cBuffers, 2);
  }

  // Sanity checks
  ALLOCATOR_PROPERTIES Actual;
  if(FAILED(hr = pAlloc->SetProperties(pProperties, &Actual))) return hr;
  if(Actual.cbBuffer < pProperties->cbBuffer) return E_FAIL;
  ASSERT(Actual.cBuffers >= pProperties->cBuffers);

  return S_OK;
}

HRESULT CLAVOutputPin::CheckMediaType(const CMediaType* pmt)
{
  std::vector<CMediaType>::iterator it;
  for(it = m_mts.begin(); it != m_mts.end(); ++it)
  {
    if(*pmt == *it)
      return S_OK;
  }

  return E_INVALIDARG;
}

HRESULT CLAVOutputPin::GetMediaType(int iPosition, CMediaType* pmt)
{
  DbgLog((LOG_TRACE, 10, L"CLAVOutputPin::GetMediaType(): %s, position: %d", CBaseDemuxer::CStreamList::ToStringW(m_pinType), iPosition));
  CAutoLock cAutoLock(m_pLock);

  if(iPosition < 0) return E_INVALIDARG;
  if((size_t)iPosition >= m_mts.size()) return VFW_S_NO_MORE_ITEMS;

  *pmt = m_mts[iPosition];

  return S_OK;
}

HRESULT CLAVOutputPin::Active()
{
  DbgLog((LOG_TRACE, 30, L"CLAVOutputPin::Active() - activated %s pin", CBaseDemuxer::CStreamList::ToStringW(m_pinType)));
  CAutoLock cAutoLock(m_pLock);

  if(m_Connected)
    Create();

  return __super::Active();
}

HRESULT CLAVOutputPin::Inactive()
{
  DbgLog((LOG_TRACE, 30, L"CLAVOutputPin::Inactive() - de-activated %s pin", CBaseDemuxer::CStreamList::ToStringW(m_pinType)));
  CAutoLock cAutoLock(m_pLock);

  CAMThread::CallWorker(CMD_EXIT);
  CAMThread::Close();

  // Clear queue when we're going inactive
  m_queue.Clear();

  return __super::Inactive();
}

STDMETHODIMP CLAVOutputPin::Connect(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt)
{
  HRESULT  hr;
  PIN_INFO PinInfo;
  GUID     FilterClsid;

  if (SUCCEEDED (pReceivePin->QueryPinInfo (&PinInfo))) {
    if (SUCCEEDED (PinInfo.pFilter->GetClassID(&FilterClsid))) {
      if (FilterClsid == CLSID_DMOWrapperFilter) {
        (static_cast<CLAVSplitter*>(m_pFilter))->SetFakeASFReader(TRUE);
      }
    }
    PinInfo.pFilter->Release();
  }

  hr = __super::Connect (pReceivePin, pmt);
  (static_cast<CLAVSplitter*>(m_pFilter))->SetFakeASFReader(FALSE);
  return hr;
}

HRESULT CLAVOutputPin::DeliverBeginFlush()
{
  DbgLog((LOG_TRACE, 20, L"::DeliverBeginFlush on %s Pin", CBaseDemuxer::CStreamList::ToStringW(m_pinType)));
  m_eEndFlush.Reset();
  m_fFlushed = false;
  m_fFlushing = true;
  m_hrDeliver = S_FALSE;
  m_queue.Clear();
  HRESULT hr = IsConnected() ? GetConnected()->BeginFlush() : S_OK;
  if(hr != S_OK) m_eEndFlush.Set();
  return hr;
}

HRESULT CLAVOutputPin::DeliverEndFlush()
{
  DbgLog((LOG_TRACE, 20, L"::DeliverEndFlush on %s Pin", CBaseDemuxer::CStreamList::ToStringW(m_pinType)));
  if(!ThreadExists()) return S_FALSE;
  HRESULT hr = IsConnected() ? GetConnected()->EndFlush() : S_OK;

  m_Parser.Flush();

  m_hrDeliver = S_OK;
  m_fFlushing = false;
  m_fFlushed = true;

  m_eEndFlush.Set();
  return hr;
}

HRESULT CLAVOutputPin::DeliverNewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
  HRESULT hr = S_OK;
  DbgLog((LOG_TRACE, 20, L"::DeliverNewSegment on %s Pin (rtStart: %I64d; rtStop: %I64d)", CBaseDemuxer::CStreamList::ToStringW(m_pinType), tStart, tStop));
  m_rtPrev = Packet::INVALID_TIME;
  if(m_fFlushing) return S_FALSE;
  m_rtStart = tStart;
  if(!ThreadExists()) return S_FALSE;

  hr = __super::DeliverNewSegment(tStart, tStop, dRate);
  if (hr != S_OK)
    return hr;

  MakeISCRHappy();

  return hr;
}

void CLAVOutputPin::MakeISCRHappy()
{
  if (IsSubtitlePin() && FilterInGraphSafe(this, CLSID_ISCR)) {
    Packet *p = new Packet();
    p->StreamId = m_streamId;
    p->rtStart = -1;
    p->rtStop = 0;
    p->bSyncPoint = FALSE;
    p->SetData(" ", 2);
    QueueFromParser(p);
  }
}

size_t CLAVOutputPin::QueueCount()
{
  return m_queue.Size();
}

HRESULT CLAVOutputPin::QueueEndOfStream()
{
  return QueuePacket(NULL); // NULL means EndOfStream
}

HRESULT CLAVOutputPin::QueuePacket(Packet *pPacket)
{
  if(!ThreadExists()) return S_FALSE;

  CLAVSplitter *pSplitter = static_cast<CLAVSplitter*>(m_pFilter);

  // While everything is good AND no pin is drying AND the queue is full .. sleep
  // The queu has a "soft" limit of MAX_PACKETS_IN_QUEUE, and a hard limit of MAX_PACKETS_IN_QUEUE * 2
  // That means, even if one pin is drying, we'll never exceed MAX_PACKETS_IN_QUEUE * 2
  while(S_OK == m_hrDeliver 
    && ((m_queue.Size() > 2*m_dwQueueHigh || m_queue.DataSize() > (MAX_QUEUE_SIZE*3/2))
    || ((m_queue.Size() > m_dwQueueHigh || m_queue.DataSize() > MAX_QUEUE_SIZE) && !pSplitter->IsAnyPinDrying())))
    Sleep(10);

  if(S_OK != m_hrDeliver) {
    SAFE_DELETE(pPacket);
    return m_hrDeliver;
  }

  {
    CAutoLock lock(&m_csMT);
    if(m_newMT && pPacket) {
      DbgLog((LOG_TRACE, 10, L"::QueuePacket() - Found new Media Type"));
      pPacket->pmt = CreateMediaType(m_newMT);
      SAFE_DELETE(m_newMT);
    }
  }

  m_Parser.Parse(m_mt.subtype, pPacket);

  return m_hrDeliver;
}

bool CLAVOutputPin::IsDiscontinuous()
{
  return m_mt.majortype == MEDIATYPE_Text
    || m_mt.majortype == MEDIATYPE_ScriptCommand
    || m_mt.majortype == MEDIATYPE_Subtitle 
    || m_mt.subtype == MEDIASUBTYPE_DVD_SUBPICTURE 
    || m_mt.subtype == MEDIASUBTYPE_CVD_SUBPICTURE 
    || m_mt.subtype == MEDIASUBTYPE_SVCD_SUBPICTURE;
}

DWORD CLAVOutputPin::ThreadProc()
{
  std::string name = "CLAVOutputPin " + std::string(CBaseDemuxer::CStreamList::ToString(m_pinType));
  SetThreadName(-1, name.c_str());
  // Anything thats not video is low bandwidth and should have a lower priority
  if (m_mt.majortype != MEDIATYPE_Video) {
    SetThreadPriority(m_hThread, THREAD_PRIORITY_BELOW_NORMAL);
  }

  m_hrDeliver = S_OK;
  m_fFlushing = m_fFlushed = false;
  m_eEndFlush.Set();
  bool bFailFlush = false;

  while(1) {
    Sleep(1);

    DWORD cmd;
    if(CheckRequest(&cmd)) {
      cmd = GetRequest();
      Reply(S_OK);
      ASSERT(cmd == CMD_EXIT);
      return 0;
    }

    size_t cnt = 0;
    do {
      Packet *pPacket = NULL;

      // Get a packet from the queue (scoped for lock)
      {
        CAutoLock cAutoLock(&m_queue);
        if((cnt = m_queue.Size()) > 0) {
          pPacket = m_queue.Get();
        }
      }

      // We need to check cnt instead of pPacket, since it can be NULL for EndOfStream
      if(m_hrDeliver == S_OK && cnt > 0) {
        ASSERT(!m_fFlushing);
        m_fFlushed = false;

        // flushing can still start here, to release a blocked deliver call
        HRESULT hr = pPacket ? DeliverPacket(pPacket) : DeliverEndOfStream();

        // .. so, wait until flush finished
        m_eEndFlush.Wait();

        if(hr != S_OK && !m_fFlushed) {
          DbgLog((LOG_TRACE, 10, L"OutputPin::ThreadProc(): Delivery failed on %s pin, hr: %0#.8x", CBaseDemuxer::CStreamList::ToStringW(GetPinType()), hr));
          if (!bFailFlush && hr == S_FALSE) {
            DbgLog((LOG_TRACE, 10, L"OutputPin::ThreadProc(): Trying to revive it by flushing..."));
            GetConnected()->BeginFlush();
            GetConnected()->EndFlush();
            bFailFlush = true;
          } else {
            m_hrDeliver = hr;
          }
          break;
        }
      } else if (pPacket) {
        // in case of stream switches or other events, we may end up here
        SAFE_DELETE(pPacket);
      }
    } while(cnt > 1 && m_hrDeliver == S_OK);
  }
  return 0;
}

HRESULT CLAVOutputPin::DeliverPacket(Packet *pPacket)
{
  HRESULT hr = S_OK;
  IMediaSample *pSample = NULL;

  long nBytes = (long)pPacket->GetDataSize();

  if(nBytes == 0) {
    goto done;
  }

  CHECK_HR(hr = GetDeliveryBuffer(&pSample, NULL, NULL, 0));

  if (m_bPacketAllocator) {
    ILAVMediaSample *pLAVSample = NULL;
    CHECK_HR(hr = pSample->QueryInterface(&pLAVSample));
    CHECK_HR(hr = pLAVSample->SetPacket(pPacket));
    SafeRelease(&pLAVSample);
  } else {
    // Resize buffer if it is too small
    // This can cause a playback hick-up, we should avoid this if possible by setting a big enough buffer size
    if(nBytes > pSample->GetSize()) {
      SafeRelease(&pSample);
      ALLOCATOR_PROPERTIES props, actual;
      CHECK_HR(hr = m_pAllocator->GetProperties(&props));
      // Give us 2 times the requested size, so we don't resize every time
      props.cbBuffer = nBytes*2;
      if(props.cBuffers > 1) {
        CHECK_HR(hr = __super::DeliverBeginFlush());
        CHECK_HR(hr = __super::DeliverEndFlush());
      }
      CHECK_HR(hr = m_pAllocator->Decommit());
      CHECK_HR(hr = m_pAllocator->SetProperties(&props, &actual));
      CHECK_HR(hr = m_pAllocator->Commit());
      CHECK_HR(hr = GetDeliveryBuffer(&pSample, NULL, NULL, 0));
    }

    // Fill the sample
    BYTE* pData = NULL;
    if(FAILED(hr = pSample->GetPointer(&pData)) || !pData) goto done;

    memcpy(pData, pPacket->GetData(), nBytes);
  }

  if(pPacket->pmt) {
    DbgLog((LOG_TRACE, 10, L"::DeliverPacket() - sending new media type to decoder"));
    pSample->SetMediaType(pPacket->pmt);
    pPacket->bDiscontinuity = true;

    CAutoLock cAutoLock(m_pLock);
    m_mts.clear();
    m_mts.push_back(*(pPacket->pmt));
  }

  bool fTimeValid = pPacket->rtStart != Packet::INVALID_TIME;

  CHECK_HR(hr = pSample->SetActualDataLength(nBytes));
  CHECK_HR(hr = pSample->SetTime(fTimeValid ? &pPacket->rtStart : NULL, fTimeValid ? &pPacket->rtStop : NULL));
  CHECK_HR(hr = pSample->SetMediaTime(NULL, NULL));
  CHECK_HR(hr = pSample->SetDiscontinuity(pPacket->bDiscontinuity));
  CHECK_HR(hr = pSample->SetSyncPoint(pPacket->bSyncPoint));
  CHECK_HR(hr = pSample->SetPreroll(fTimeValid && pPacket->rtStart < 0));
  // Deliver
  CHECK_HR(hr = Deliver(pSample));

done:
  if (!m_bPacketAllocator)
    SAFE_DELETE(pPacket);
  SafeRelease(&pSample);
  return hr;
}

// IMediaSeeking
STDMETHODIMP CLAVOutputPin::GetCapabilities(DWORD* pCapabilities)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->GetCapabilities(pCapabilities);
}
STDMETHODIMP CLAVOutputPin::CheckCapabilities(DWORD* pCapabilities)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->CheckCapabilities(pCapabilities);
}
STDMETHODIMP CLAVOutputPin::IsFormatSupported(const GUID* pFormat)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->IsFormatSupported(pFormat);
}
STDMETHODIMP CLAVOutputPin::QueryPreferredFormat(GUID* pFormat)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->QueryPreferredFormat(pFormat);
}
STDMETHODIMP CLAVOutputPin::GetTimeFormat(GUID* pFormat)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->GetTimeFormat(pFormat);
}
STDMETHODIMP CLAVOutputPin::IsUsingTimeFormat(const GUID* pFormat)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->IsUsingTimeFormat(pFormat);
}
STDMETHODIMP CLAVOutputPin::SetTimeFormat(const GUID* pFormat)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->SetTimeFormat(pFormat);
}
STDMETHODIMP CLAVOutputPin::GetDuration(LONGLONG* pDuration)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->GetDuration(pDuration);
}
STDMETHODIMP CLAVOutputPin::GetStopPosition(LONGLONG* pStop)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->GetStopPosition(pStop);
}
STDMETHODIMP CLAVOutputPin::GetCurrentPosition(LONGLONG* pCurrent)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->GetCurrentPosition(pCurrent);
}
STDMETHODIMP CLAVOutputPin::ConvertTimeFormat(LONGLONG* pTarget, const GUID* pTargetFormat, LONGLONG Source, const GUID* pSourceFormat)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->ConvertTimeFormat(pTarget, pTargetFormat, Source, pSourceFormat);
}
STDMETHODIMP CLAVOutputPin::SetPositions(LONGLONG* pCurrent, DWORD dwCurrentFlags, LONGLONG* pStop, DWORD dwStopFlags)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->SetPositionsInternal(this, pCurrent, dwCurrentFlags, pStop, dwStopFlags);
}
STDMETHODIMP CLAVOutputPin::GetPositions(LONGLONG* pCurrent, LONGLONG* pStop)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->GetPositions(pCurrent, pStop);
}
STDMETHODIMP CLAVOutputPin::GetAvailable(LONGLONG* pEarliest, LONGLONG* pLatest)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->GetAvailable(pEarliest, pLatest);
}
STDMETHODIMP CLAVOutputPin::SetRate(double dRate)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->SetRate(dRate);
}
STDMETHODIMP CLAVOutputPin::GetRate(double* pdRate)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->GetRate(pdRate);
}
STDMETHODIMP CLAVOutputPin::GetPreroll(LONGLONG* pllPreroll)
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->GetPreroll(pllPreroll);
}

STDMETHODIMP_(DWORD) CLAVOutputPin::GetStreamFlags()
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->GetStreamFlags(m_streamId);
}

STDMETHODIMP_(int) CLAVOutputPin::GetPixelFormat()
{
  return (static_cast<CLAVSplitter*>(m_pFilter))->GetPixelFormat(m_streamId);
}
