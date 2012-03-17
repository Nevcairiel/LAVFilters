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
 */
#include "stdafx.h"
#include "DecodeThread.h"

#include "LAVVideo.h"

#include <Shlwapi.h>


CDecodeThread::CDecodeThread(CLAVVideo *pLAVVideo)
  : m_pLAVVideo(pLAVVideo)
  , m_pDecoder(NULL)
  , m_bHWDecoder(FALSE)
  , m_bHWDecoderFailed(FALSE)
  , m_bThreadSafe(FALSE)
  , m_Codec(CODEC_ID_NONE)
  , m_evDeliver(FALSE)
  , m_evSample(FALSE)
  , m_evDecodeDone(FALSE)
  , m_evEOSDone(FALSE)
  , m_evInput(TRUE)
{
  WCHAR fileName[1024];
  GetModuleFileName(NULL, fileName, 1024);
  m_processName = PathFindFileName (fileName);

  memset(&m_ThreadCallContext, 0, sizeof(m_ThreadCallContext));

  CAMThread::Create();
  m_evInput.Reset();
}

CDecodeThread::~CDecodeThread(void)
{
  Close();
  CAMThread::CallWorker(CMD_EXIT);
  CAMThread::Close();
}

STDMETHODIMP CDecodeThread::CreateDecoder(const CMediaType *pmt, CodecID codec)
{
  CAutoLock decoderLock(this);

  if (!CAMThread::ThreadExists())
    return E_UNEXPECTED;

  HRESULT hr = S_OK;
  {
    CAutoLock lock(&m_ThreadCritSec);
    m_ThreadCallContext.pmt = pmt;
    m_ThreadCallContext.codec = codec;
  }
  hr = CAMThread::CallWorker(CMD_CREATE_DECODER);

  return hr;
}

STDMETHODIMP CDecodeThread::InitAllocator(IMemAllocator **ppAlloc)
{
  CAutoLock decoderLock(this);

  if (!CAMThread::ThreadExists())
    return E_UNEXPECTED;

  if (!m_pDecoder)
    return S_FALSE;

  HRESULT hr = S_OK;
  {
    CAutoLock lock(&m_ThreadCritSec);
    m_ThreadCallContext.allocator = ppAlloc;
  }
  hr = CAMThread::CallWorker(CMD_INIT_ALLOCATOR);
  return hr;
}

STDMETHODIMP CDecodeThread::PostConnect(IPin *pPin)
{
  CAutoLock decoderLock(this);
  HRESULT hr = S_OK;

  if (!CAMThread::ThreadExists())
    return E_UNEXPECTED;

  {
    CAutoLock lock(&m_ThreadCritSec);
    m_ThreadCallContext.pin = pPin;
  }
  hr = CAMThread::CallWorker(CMD_POST_CONNECT);
  return hr;
}

STDMETHODIMP CDecodeThread::Close()
{
  CAutoLock decoderLock(this);

  if (!CAMThread::ThreadExists())
    return E_UNEXPECTED;

  CAMThread::CallWorker(CMD_CLOSE_DECODER);
  return S_OK;
}

STDMETHODIMP CDecodeThread::Decode(IMediaSample *pSample)
{
  CAutoLock decoderLock(this);

  if (!CAMThread::ThreadExists())
    return E_UNEXPECTED;

  m_evDeliver.Reset();
  m_evSample.Reset();

  if (!m_bThreadSafe)
    m_evDecodeDone.Reset();

  pSample->AddRef();
  m_Samples.Push(pSample);

  // Wake the worker thread (if it was waiting)
  // Needs to be done after inserting data into the queue, or it might cause a deadlock
  m_evInput.Set();

  // If we don't have thread safe buffers, we need to synchronize
  // with the worker thread and deliver them when they are available
  // and then let it know that we did so
  if (!m_bThreadSafe) {
    while (!m_evDecodeDone.Check()) {
      m_evSample.Wait();
      if (ProcessOutput() == S_OK)
        m_evDeliver.Set();
    }
  }

  ProcessOutput();

  return S_OK;
}

STDMETHODIMP CDecodeThread::Flush()
{
  CAutoLock decoderLock(this);

  if (!CAMThread::ThreadExists())
    return E_UNEXPECTED;

  CAMThread::CallWorker(CMD_FLUSH);
  return S_OK;
}

STDMETHODIMP CDecodeThread::EndOfStream()
{
  CAutoLock decoderLock(this);

  if (!CAMThread::ThreadExists())
    return E_UNEXPECTED;

  m_evDeliver.Reset();
  m_evSample.Reset();

  CAMThread::CallWorker(CMD_EOS);

  while (!m_evEOSDone.Check()) {
    m_evSample.Wait();
    if (ProcessOutput() == S_OK && !m_bThreadSafe)
      m_evDeliver.Set();
  }

  ProcessOutput();

  return S_OK;
}

STDMETHODIMP CDecodeThread::ProcessOutput()
{
  HRESULT hr = S_FALSE;
  while (LAVFrame *pFrame = m_Output.Pop()) {
    hr = S_OK;
    m_pLAVVideo->Deliver(pFrame);
  }
  return hr;
}

STDMETHODIMP CDecodeThread::ClearQueues()
{
  // Release input samples
  {
    CAutoLock lock(&m_Samples);
    while (IMediaSample *pSample = m_Samples.Pop())
      pSample->Release();
  }
  // Release output samples
  {
    CAutoLock lock(&m_Output);
    while (LAVFrame *pFrame = m_Output.Pop()) {
      ReleaseFrame(&pFrame);
    }
  }

  return S_OK;
}

DWORD CDecodeThread::ThreadProc()
{
  HRESULT hr;
  DWORD cmd;

  BOOL bEOS = FALSE;

  SetThreadName(-1, "LAVVideo Decode Thread");

  HANDLE hWaitEvents[2] = { GetRequestHandle(), m_evInput };
  while(1) {
    if (!bEOS) {
      // Wait for either an input sample, or an request
      WaitForMultipleObjects(2, hWaitEvents, FALSE, INFINITE);
    }

    if (CheckRequest(&cmd)) {
      switch (cmd) {
      case CMD_CREATE_DECODER:
        {
          CAutoLock lock(&m_ThreadCritSec);
          hr = CreateDecoderInternal(m_ThreadCallContext.pmt, m_ThreadCallContext.codec);
          Reply(hr);

          m_ThreadCallContext.pmt = NULL;
        }
        break;
      case CMD_CLOSE_DECODER:
        {
          ClearQueues();
          SAFE_DELETE(m_pDecoder);
          Reply(S_OK);
        }
        break;
      case CMD_FLUSH:
        {
          ClearQueues();
          m_pDecoder->Flush();
          Reply(S_OK);
        }
        break;
      case CMD_EOS:
        {
          bEOS = TRUE;
          m_evEOSDone.Reset();
          Reply(S_OK);
        }
        break;
      case CMD_EXIT:
        {
          Reply(S_OK);
          return 0;
        }
        break;
      case CMD_INIT_ALLOCATOR:
        {
          CAutoLock lock(&m_ThreadCritSec);
          hr = m_pDecoder->InitAllocator(m_ThreadCallContext.allocator);
          Reply(hr);

          m_ThreadCallContext.allocator = NULL;
        }
        break;
      case CMD_POST_CONNECT:
        {
          CAutoLock lock(&m_ThreadCritSec);
          hr = PostConnectInternal(m_ThreadCallContext.pin);
          Reply(hr);

          m_ThreadCallContext.pin = NULL;
        }
        break;
      default:
        ASSERT(0);
      }
    }

    IMediaSample *pSample = NULL;
    {
      CAutoLock sampleLock(&m_Samples);
      pSample = m_Samples.Pop();
      if (!pSample || m_Samples.Empty())
        m_evInput.Reset();
    }
    if (!pSample) {
      // Process the EOS now that the sample queue is empty
      if (bEOS) {
        bEOS = FALSE;
        m_pDecoder->EndOfStream();
        m_evEOSDone.Set();
        m_evSample.Set();
      }
      continue;
    }

    DecodeInternal(pSample);
    pSample->Release();

    // Indicates we're done decoding this sample
    if (!m_bThreadSafe)
      m_evDecodeDone.Set();

    // Set the Sample Event to unblock any waiting threads
    m_evSample.Set();
  }

  return 0;
}

#define HWFORMAT_ENABLED \
   ((codec == CODEC_ID_H264 && m_pLAVVideo->GetHWAccelCodec(HWCodec_H264))                                                    \
|| ((codec == CODEC_ID_VC1 || codec == CODEC_ID_WMV3) && m_pLAVVideo->GetHWAccelCodec(HWCodec_VC1))                           \
|| ((codec == CODEC_ID_MPEG2VIDEO || codec == CODEC_ID_MPEG1VIDEO) && m_pLAVVideo->GetHWAccelCodec(HWCodec_MPEG2))            \
|| (codec == CODEC_ID_MPEG4 && m_pLAVVideo->GetHWAccelCodec(HWCodec_MPEG4)))

STDMETHODIMP CDecodeThread::CreateDecoderInternal(const CMediaType *pmt, CodecID codec)
{
  HRESULT hr = S_OK;

  BOOL bHWDecBlackList = _wcsicmp(m_processName.c_str(), L"dllhost.exe") == 0 || _wcsicmp(m_processName.c_str(), L"explorer.exe") == 0 || _wcsicmp(m_processName.c_str(), L"ReClockHelper.dll") == 0;
  DbgLog((LOG_TRACE, 10, L"-> Process is %s, blacklist: %d", m_processName.c_str(), bHWDecBlackList));

  // Try reusing the current HW decoder
  if (m_pDecoder && m_bHWDecoder && !m_bHWDecoderFailed && HWFORMAT_ENABLED) {
    hr = m_pDecoder->InitDecoder(codec, pmt);
    goto done;
  }
  SAFE_DELETE(m_pDecoder);

  LAVHWAccel hwAccel = m_pLAVVideo->GetHWAccel();
  if (!bHWDecBlackList &&  hwAccel != HWAccel_None && !m_bHWDecoderFailed && HWFORMAT_ENABLED)
  {
    if (hwAccel == HWAccel_CUDA)
      m_pDecoder = CreateDecoderCUVID();
    else if (hwAccel == HWAccel_QuickSync)
      m_pDecoder = CreateDecoderQuickSync();
    else if (hwAccel == HWAccel_DXVA2CopyBack)
      m_pDecoder = CreateDecoderDXVA2();
    else if (hwAccel == HWAccel_DXVA2Native)
      m_pDecoder = CreateDecoderDXVA2Native();
    m_bHWDecoder = TRUE;
  }

softwaredec:
  // Fallback for software
  if (!m_pDecoder) {
    m_bHWDecoder = FALSE;
    if (m_pLAVVideo->GetUseMSWMV9Decoder() && (codec == CODEC_ID_VC1 || codec == CODEC_ID_WMV3))
      m_pDecoder = CreateDecoderWMV9();
    else
      m_pDecoder = CreateDecoderAVCodec();
  }

  hr = m_pDecoder->InitInterfaces(static_cast<ILAVVideoSettings *>(m_pLAVVideo), static_cast<ILAVVideoCallback *>(this));
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Init Interfaces failed (hr: 0x%x)", hr));
    goto done;
  }

  hr = m_pDecoder->InitDecoder(codec, pmt);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Init Decoder failed (hr: 0x%x)", hr));
    goto done;
  }

done:
  if (FAILED(hr) && m_bHWDecoder) {
    DbgLog((LOG_TRACE, 10, L"-> Hardware decoder failed to initialize, re-trying with software..."));
    m_bHWDecoderFailed = TRUE;
    SAFE_DELETE(m_pDecoder);
    goto softwaredec;
  }

  m_Codec = codec;
  m_bThreadSafe = m_pDecoder->HasThreadSafeBuffers() == S_OK;

  return hr;
}

STDMETHODIMP CDecodeThread::PostConnectInternal(IPin *pPin)
{
  HRESULT hr = S_OK;
  if (m_pDecoder) {
    hr = m_pDecoder->PostConnect(pPin);
    if (FAILED(hr)) {
      m_bHWDecoderFailed = TRUE;
      CMediaType &mt = m_pLAVVideo->GetInputMediaType();
      hr = CreateDecoderInternal(&mt, m_Codec);
    }
  }
  return hr;
}

STDMETHODIMP CDecodeThread::DecodeInternal(IMediaSample *pSample)
{
  HRESULT hr = S_OK;

  if (!m_pDecoder)
    return E_UNEXPECTED;

  hr = m_pDecoder->Decode(pSample);

  // If a hardware decoder indicates a hard failure, we switch back to software
  // This is used to indicate incompatible media
  if (FAILED(hr) && m_bHWDecoder) {
    DbgLog((LOG_TRACE, 10, L"::Receive(): Hardware decoder indicates failure, switching back to software"));
    m_bHWDecoderFailed = TRUE;

    CMediaType &mt = m_pLAVVideo->GetInputMediaType();
    hr = CreateDecoderInternal(&mt, m_Codec);
    if (FAILED(hr)) {
      DbgLog((LOG_ERROR, 10, L"-> Creating software decoder failed, this is bad."));
      return E_FAIL;
    }

    DbgLog((LOG_TRACE, 10, L"-> Software decoder created, decoding frame again..."));
    hr = m_pDecoder->Decode(pSample);
  }

  return S_OK;
}

// ILAVVideoCallback
STDMETHODIMP CDecodeThread::Deliver(LAVFrame *pFrame)
{
  m_Output.Push(pFrame);
  m_evSample.Set();
  if (!m_bThreadSafe) {
    m_evDeliver.Wait();
  }
  return S_OK;
}

STDMETHODIMP CDecodeThread::AllocateFrame(LAVFrame **ppFrame) { return m_pLAVVideo->AllocateFrame(ppFrame); }
STDMETHODIMP CDecodeThread::ReleaseFrame(LAVFrame **ppFrame) { return m_pLAVVideo->ReleaseFrame(ppFrame); }
STDMETHODIMP_(LPWSTR) CDecodeThread::GetFileExtension() { return m_pLAVVideo->GetFileExtension(); }
STDMETHODIMP_(BOOL) CDecodeThread::FilterInGraph(PIN_DIRECTION dir, const GUID &clsid) { return m_pLAVVideo->FilterInGraph(dir, clsid); }
STDMETHODIMP_(BOOL) CDecodeThread::VC1IsDTS() { return m_pLAVVideo->VC1IsDTS(); }
STDMETHODIMP_(BOOL) CDecodeThread::H264IsAVI() { return m_pLAVVideo->H264IsAVI(); }
STDMETHODIMP_(BOOL) CDecodeThread::IsLAVSplitter() { return m_pLAVVideo->IsLAVSplitter(); }
STDMETHODIMP_(BOOL) CDecodeThread::IsVistaOrNewer() { return m_pLAVVideo->IsVistaOrNewer(); }
STDMETHODIMP_(CMediaType&) CDecodeThread::GetInputMediaType() { return m_pLAVVideo->GetInputMediaType(); }
STDMETHODIMP CDecodeThread::GetLAVPinInfo(LAVPinInfo &info) { return m_pLAVVideo->GetLAVPinInfo(info); }
STDMETHODIMP_(CBasePin*) CDecodeThread::GetOutputPin() { return m_pLAVVideo->GetOutputPin(); }
