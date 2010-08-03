#include "stdafx.h"
#include "LAVFOutputPin.h"
#include "LAVFSplitter.h"
#include "utils.h"
#include "moreuuids.h"

CLAVFOutputPin::CLAVFOutputPin(std::vector<CMediaType>& mts, LPCWSTR pName, CBaseFilter *pFilter, CCritSec *pLock, HRESULT *phr, int nBuffers)
  : CBaseOutputPin(NAME("CLAVFOutputPin"), pFilter, pLock, phr, pName)
  , m_hrDeliver(S_OK)
  , m_fFlushing(false)
  , m_eEndFlush(TRUE)
{
  m_mts = mts;
  m_nBuffers = max(nBuffers, 1);
}

CLAVFOutputPin::CLAVFOutputPin(LPCWSTR pName, CBaseFilter *pFilter, CCritSec *pLock, HRESULT *phr, int nBuffers)
  : CBaseOutputPin(NAME("CLAVFOutputPin"), pFilter, pLock, phr, pName)
  , m_hrDeliver(S_OK)
  , m_fFlushing(false)
  , m_eEndFlush(TRUE)
{
  m_nBuffers = max(nBuffers, 1);
}

CLAVFOutputPin::~CLAVFOutputPin()
{
}

STDMETHODIMP CLAVFOutputPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  if (riid == __uuidof(IMediaSeeking)) {
    return m_pFilter->QueryInterface(riid, ppv);
  }
  return 
    __super::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CLAVFOutputPin::CheckConnect(IPin* pPin) {
  int iPosition = 0;
  CMediaType mt;
  while(S_OK == GetMediaType(iPosition++, &mt)) {
    mt.InitMediaType();
  }
  return __super::CheckConnect(pPin);
}

HRESULT CLAVFOutputPin::DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProperties)
{
  CheckPointer(pAlloc, E_POINTER);
  CheckPointer(pProperties, E_POINTER);

  HRESULT hr = S_OK;

  pProperties->cBuffers = m_nBuffers;
  pProperties->cbBuffer = max(m_mt.lSampleSize, 1);

  // TODO
  /* if(m_mt.subtype == MEDIASUBTYPE_Vorbis && m_mt.formattype == FORMAT_VorbisFormat)
  {
  // oh great, the oggds vorbis decoder assumes there will be two at least, stupid thing...\r
  pProperties->cBuffers = max(pProperties->cBuffers, 2);
  } */

  // Sanity checks
  ALLOCATOR_PROPERTIES Actual;
  if(FAILED(hr = pAlloc->SetProperties(pProperties, &Actual))) return hr;
  if(Actual.cbBuffer < pProperties->cbBuffer) return E_FAIL;
  ASSERT(Actual.cBuffers == pProperties->cBuffers);

  return S_OK;
}

HRESULT CLAVFOutputPin::CheckMediaType(const CMediaType* pmt)
{
  std::vector<CMediaType>::iterator it;
  for(it = m_mts.begin(); it != m_mts.end(); it++)
  {
    if(*pmt == *it)
      return S_OK;
  }

  return E_INVALIDARG;
}

HRESULT CLAVFOutputPin::GetMediaType(int iPosition, CMediaType* pmt)
{
  CAutoLock cAutoLock(m_pLock);

  if(iPosition < 0) return E_INVALIDARG;
  if((unsigned int)iPosition >= m_mts.size()) return VFW_S_NO_MORE_ITEMS;

  *pmt = m_mts[iPosition];

  return S_OK;
}

HRESULT CLAVFOutputPin::Active()
{
  CAutoLock cAutoLock(m_pLock);

  if(m_Connected)
    Create();

  return __super::Active();
}

HRESULT CLAVFOutputPin::Inactive()
{
  CAutoLock cAutoLock(m_pLock);

  if(ThreadExists())
    CallWorker(CMD_EXIT);

  return __super::Inactive();
}

HRESULT CLAVFOutputPin::DeliverBeginFlush()
{
  m_eEndFlush.Reset();
  m_fFlushed = false;
  m_fFlushing = true;
  m_hrDeliver = S_FALSE;
  m_queue.Clear();
  HRESULT hr = IsConnected() ? GetConnected()->BeginFlush() : S_OK;
  if(hr != S_OK) m_eEndFlush.Set();
  return hr;
}

HRESULT CLAVFOutputPin::DeliverEndFlush()
{
  if(!ThreadExists()) return S_FALSE;
  HRESULT hr = IsConnected() ? GetConnected()->EndFlush() : S_OK;
  m_hrDeliver = S_OK;
  m_fFlushing = false;
  m_fFlushed = true;
  m_eEndFlush.Set();
  return hr;
}

HRESULT CLAVFOutputPin::DeliverNewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
  if(m_fFlushing) return S_FALSE;
  m_rtStart = tStart;
  if(!ThreadExists()) return S_FALSE;

  return __super::DeliverNewSegment(tStart, tStop, dRate);
}

HRESULT CLAVFOutputPin::QueueEndOfStream()
{
  return QueuePacket(NULL); // NULL means EndOfStream
}

HRESULT CLAVFOutputPin::QueuePacket(Packet *pPacket)
{
  if(!ThreadExists()) return S_FALSE;

  while(S_OK == m_hrDeliver 
    && (!(static_cast<CLAVFSplitter*>(m_pFilter))->IsAnyPinDrying()
    || m_queue.Size() > MAX_PACKETS_IN_QUEUE))
    Sleep(1);

  DbgLog((LOG_ERROR, 0, TEXT("Queue length (stream %d): %d"), m_streamId, m_queue.Size()));

  if(S_OK != m_hrDeliver)
    return m_hrDeliver;

  m_queue.Queue(pPacket);

  return m_hrDeliver;
}

bool CLAVFOutputPin::IsDiscontinuous()
{
  return m_mt.majortype == MEDIATYPE_Text
    || m_mt.majortype == MEDIATYPE_ScriptCommand
    || m_mt.majortype == MEDIATYPE_Subtitle 
    || m_mt.subtype == MEDIASUBTYPE_DVD_SUBPICTURE 
    || m_mt.subtype == MEDIASUBTYPE_CVD_SUBPICTURE 
    || m_mt.subtype == MEDIASUBTYPE_SVCD_SUBPICTURE;
}

DWORD CLAVFOutputPin::ThreadProc()
{
  //SetThreadName((DWORD)-1, "CLAVFOutputPin");
  m_hrDeliver = S_OK;
  m_fFlushing = m_fFlushed = false;
  m_eEndFlush.Set();

  while(1) {
    Sleep(1);

    DWORD cmd;
    if(CheckRequest(&cmd)) {
      m_hThread = NULL;
      cmd = GetRequest();
      Reply(S_OK);
      ASSERT(cmd == CMD_EXIT);
      return 0;
    }

    int cnt = 0;
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
          m_hrDeliver = hr;
          break;
        }
      }
    } while(--cnt > 0);
  }
  return 0;
}

HRESULT CLAVFOutputPin::DeliverPacket(Packet *pPacket)
{
  HRESULT hr = S_OK;
  IMediaSample *pSample = NULL;

  long nBytes = pPacket->GetDataSize();

  if(nBytes == 0) {
    goto done;
  }

  CHECK_HR(hr = GetDeliveryBuffer(&pSample, NULL, NULL, 0));

  // Resize buffer if it is too small
  if(nBytes > pSample->GetSize()) {
    pSample->Release(); 
    ALLOCATOR_PROPERTIES props, actual;
    CHECK_HR(hr = m_pAllocator->GetProperties(&props));
    // Give us 1.5 times the requested size, so we don't resize every time
    props.cbBuffer = nBytes*3/2; 
    if(props.cBuffers > 1) {
      CHECK_HR(hr = __super::DeliverBeginFlush());
      CHECK_HR(hr = __super::DeliverEndFlush());
    }
    CHECK_HR(hr = m_pAllocator->Decommit());
    CHECK_HR(hr = m_pAllocator->SetProperties(&props, &actual));
    CHECK_HR(hr = m_pAllocator->Commit());
    CHECK_HR(hr = GetDeliveryBuffer(&pSample, NULL, NULL, 0));
  }

  if(pPacket->pmt) {
    pSample->SetMediaType(pPacket->pmt);
    pPacket->bDiscontinuity = true;

    CAutoLock cAutoLock(m_pLock);
    m_mts.clear();
    m_mts.push_back(*(pPacket->pmt));
  }

  bool fTimeValid = pPacket->rtStart != Packet::INVALID_TIME;
  ASSERT(!pPacket->bSyncPoint || fTimeValid);

  // Fill the sample
  BYTE* pData = NULL;
  if(FAILED(hr = pSample->GetPointer(&pData)) || !pData) goto done;
  memcpy(pData, pPacket->GetData(), nBytes);

  CHECK_HR(hr = pSample->SetActualDataLength(nBytes));
  CHECK_HR(hr = pSample->SetTime(fTimeValid ? &pPacket->rtStart : NULL, fTimeValid ? &pPacket->rtStop : NULL));
  CHECK_HR(hr = pSample->SetMediaTime(NULL, NULL));
  CHECK_HR(hr = pSample->SetDiscontinuity(pPacket->bDiscontinuity));
  CHECK_HR(hr = pSample->SetSyncPoint(pPacket->bSyncPoint));
  CHECK_HR(hr = pSample->SetPreroll(fTimeValid && pPacket->rtStart < 0));
  // Deliver
  CHECK_HR(hr = Deliver(pSample));

done:
  delete pPacket;
  if (pSample) {
    pSample->Release();
  }
  return hr;
}
