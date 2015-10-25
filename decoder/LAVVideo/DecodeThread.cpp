/*
 *      Copyright (C) 2010-2015 Hendrik Leppkes
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
{
  WCHAR fileName[1024];
  GetModuleFileName(nullptr, fileName, 1024);
  m_processName = PathFindFileName (fileName);

  m_TempSample[0] = nullptr;
  m_TempSample[1] = nullptr;

  CAMThread::Create();
  m_evInput.Reset();
}

CDecodeThread::~CDecodeThread(void)
{
  Close();
  CAMThread::CallWorker(CMD_EXIT);
  CAMThread::Close();
}

STDMETHODIMP CDecodeThread::CreateDecoder(const CMediaType *pmt, AVCodecID codec)
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

  if (!CAMThread::ThreadExists() || !m_pDecoder)
    return E_UNEXPECTED;

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

  if (!CAMThread::ThreadExists() || !m_pDecoder)
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

bool CDecodeThread::HasSample()
{
  CAutoLock lock(&m_SampleCritSec);
  return m_NextSample != nullptr;
}

void CDecodeThread::PutSample(IMediaSample *pSample)
{
  CAutoLock lock(&m_SampleCritSec);
  ASSERT(m_NextSample == nullptr);
  // Provide Sample to worker thread
  m_NextSample = pSample;

  // Wake worker thread
  m_evInput.Set();
}

IMediaSample* CDecodeThread::GetSample()
{
  CAutoLock lock(&m_SampleCritSec);

  // Take the sample out of the buffer
  IMediaSample *pSample = m_NextSample;
  m_NextSample = nullptr;

  // Reset input event (no more input)
  m_evInput.Reset();

  return pSample;
}

void CDecodeThread::ReleaseSample()
{
  CAutoLock lock(&m_SampleCritSec);
  // Free any sample that was still queued
  SafeRelease(&m_NextSample);

  // Reset input event (no more input)
  m_evInput.Reset();
}

STDMETHODIMP CDecodeThread::Decode(IMediaSample *pSample)
{
  CAutoLock decoderLock(this);

  if (!CAMThread::ThreadExists() || !m_pDecoder)
    return E_UNEXPECTED;

  // Wait until the queue is empty
  while(HasSample())
    Sleep(1);

  // Re-init the decoder, if requested
  // Doing this inside the worker thread alone causes problems
  // when switching from non-sync to sync, so ensure we're in sync.
  if (m_bDecoderNeedsReInit) {
    CAMThread::CallWorker(CMD_REINIT);
    while (!m_evEOSDone.Check()) {
      m_evSample.Wait();
      ProcessOutput();
    }
  }

  m_evDeliver.Reset();
  m_evSample.Reset();
  m_evDecodeDone.Reset();

  pSample->AddRef();

  // Send data to worker thread, and wake it (if it was waiting)
  PutSample(pSample);

  // If we don't have thread safe buffers, we need to synchronize
  // with the worker thread and deliver them when they are available
  // and then let it know that we did so
  if (m_bSyncToProcess) {
    while (!m_evDecodeDone.Check()) {
      m_evSample.Wait();
      ProcessOutput();
    }
  }

  ProcessOutput();

  return S_OK;
}

STDMETHODIMP CDecodeThread::Flush()
{
  CAutoLock decoderLock(this);

  if (!CAMThread::ThreadExists() || !m_pDecoder)
    return E_UNEXPECTED;

  CAMThread::CallWorker(CMD_FLUSH);
  return S_OK;
}

STDMETHODIMP CDecodeThread::EndOfStream()
{
  CAutoLock decoderLock(this);

  if (!CAMThread::ThreadExists() || !m_pDecoder)
    return E_UNEXPECTED;

  m_evDeliver.Reset();
  m_evSample.Reset();

  CAMThread::CallWorker(CMD_EOS);

  while (!m_evEOSDone.Check()) {
    m_evSample.Wait();
    ProcessOutput();
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
  if (hr == S_OK && m_bSyncToProcess)
    m_evDeliver.Set();
  return hr;
}

STDMETHODIMP CDecodeThread::ClearQueues()
{
  // Release input sample
  ReleaseSample();

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
  BOOL bReinit = FALSE;

  SetThreadName(-1, "LAVVideo Decode Thread");

  HANDLE hWaitEvents[2] = { GetRequestHandle(), m_evInput };
  while(1) {
    if (!bEOS && !bReinit) {
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

          m_ThreadCallContext.pmt = nullptr;
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

          m_ThreadCallContext.allocator = nullptr;
        }
        break;
      case CMD_POST_CONNECT:
        {
          CAutoLock lock(&m_ThreadCritSec);
          hr = PostConnectInternal(m_ThreadCallContext.pin);
          Reply(hr);

          m_ThreadCallContext.pin = nullptr;
        }
        break;
      case CMD_REINIT:
        {
          CMediaType &mt = m_pLAVVideo->GetInputMediaType();
          CreateDecoderInternal(&mt, m_Codec);
          m_TempSample[1] = m_NextSample;
          m_NextSample = m_FailedSample;
          m_FailedSample = nullptr;
          bReinit = TRUE;
          m_evEOSDone.Reset();
          Reply(S_OK);
          m_bDecoderNeedsReInit = FALSE;
        }
        break;
      default:
        ASSERT(0);
      }
    }

    if (m_bDecoderNeedsReInit) {
      m_evInput.Reset();
      continue;
    }

    if (bReinit && !m_NextSample) {
      if (m_TempSample[0]) {
        m_NextSample = m_TempSample[0];
        m_TempSample[0] = nullptr;
      } else if (m_TempSample[1]) {
        m_NextSample = m_TempSample[1];
        m_TempSample[1] = nullptr;
      } else {
        bReinit = FALSE;
        m_evEOSDone.Set();
        m_evSample.Set();
        continue;
      }
    }

    IMediaSample *pSample = GetSample();
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

    // Release the sample
    SafeRelease(&pSample);

    // Indicates we're done decoding this sample
    m_evDecodeDone.Set();

    // Set the Sample Event to unblock any waiting threads
    m_evSample.Set();
  }

  return 0;
}

#define HWFORMAT_ENABLED \
   ((codec == AV_CODEC_ID_H264 && m_pLAVVideo->GetHWAccelCodec(HWCodec_H264))                                                       \
|| ((codec == AV_CODEC_ID_VC1 || codec == AV_CODEC_ID_WMV3) && m_pLAVVideo->GetHWAccelCodec(HWCodec_VC1))                           \
|| ((codec == AV_CODEC_ID_MPEG2VIDEO || codec == AV_CODEC_ID_MPEG1VIDEO) && m_pLAVVideo->GetHWAccelCodec(HWCodec_MPEG2) && (!(GetDecodeFlags() & LAV_VIDEO_DEC_FLAG_DVD) || m_pLAVVideo->GetHWAccelCodec(HWCodec_MPEG2DVD)))            \
|| (codec == AV_CODEC_ID_MPEG4 && m_pLAVVideo->GetHWAccelCodec(HWCodec_MPEG4)) \
|| (codec == AV_CODEC_ID_HEVC && m_pLAVVideo->GetHWAccelCodec(HWCodec_HEVC)))

#define HWRESOLUTION_ENABLED \
  ((pBMI->biHeight <= 576 && pBMI->biWidth <= 1024 && m_pLAVVideo->GetHWAccelResolutionFlags() & LAVHWResFlag_SD)     \
|| ((pBMI->biHeight > 576 || pBMI->biWidth > 1024) && pBMI->biHeight <= 1200 && pBMI->biWidth <= 1920 && m_pLAVVideo->GetHWAccelResolutionFlags() & LAVHWResFlag_HD)    \
|| ((pBMI->biHeight > 1200 || pBMI->biWidth > 1920) && m_pLAVVideo->GetHWAccelResolutionFlags() & LAVHWResFlag_UHD))

STDMETHODIMP CDecodeThread::CreateDecoderInternal(const CMediaType *pmt, AVCodecID codec)
{
  DbgLog((LOG_TRACE, 10, L"CDecodeThread::CreateDecoderInternal(): Creating new decoder for codec %S", avcodec_get_name(codec)));
  HRESULT hr = S_OK;
  BOOL bWMV9 = FALSE;

  BOOL bHWDecBlackList = _wcsicmp(m_processName.c_str(), L"dllhost.exe") == 0 || _wcsicmp(m_processName.c_str(), L"explorer.exe") == 0 || _wcsicmp(m_processName.c_str(), L"ReClockHelper.dll") == 0;
  DbgLog((LOG_TRACE, 10, L"-> Process is %s, blacklist: %d", m_processName.c_str(), bHWDecBlackList));

  BITMAPINFOHEADER *pBMI = nullptr;
  videoFormatTypeHandler(*pmt, &pBMI);

  // Try reusing the current HW decoder
  if (m_pDecoder && m_bHWDecoder && !m_bHWDecoderFailed && HWFORMAT_ENABLED && HWRESOLUTION_ENABLED) {
    DbgLog((LOG_TRACE, 10, L"-> Trying to re-use old HW Decoder"));
    hr = m_pDecoder->InitDecoder(codec, pmt);
    goto done;
  }
  SAFE_DELETE(m_pDecoder);

  LAVHWAccel hwAccel = m_pLAVVideo->GetHWAccel();
  if (!bHWDecBlackList &&  hwAccel != HWAccel_None && !m_bHWDecoderFailed && HWFORMAT_ENABLED && HWRESOLUTION_ENABLED)
  {
    DbgLog((LOG_TRACE, 10, L"-> Trying Hardware Codec %d", hwAccel));
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
    DbgLog((LOG_TRACE, 10, L"-> No HW Codec, using Software"));
    m_bHWDecoder = FALSE;
    if (m_pLAVVideo->GetUseMSWMV9Decoder() && (codec == AV_CODEC_ID_VC1 || codec == AV_CODEC_ID_WMV3) && !m_bWMV9Failed) {
      if (IsWindows7OrNewer())
        m_pDecoder = CreateDecoderWMV9MFT();
      else
        m_pDecoder = CreateDecoderWMV9();
      bWMV9 = TRUE;
    } else
      m_pDecoder = CreateDecoderAVCodec();
  }
  DbgLog((LOG_TRACE, 10, L"-> Created Codec '%s'", m_pDecoder->GetDecoderName()));

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
  if (FAILED(hr)) {
    SAFE_DELETE(m_pDecoder);
    if (m_bHWDecoder) {
      DbgLog((LOG_TRACE, 10, L"-> Hardware decoder failed to initialize, re-trying with software..."));
      m_bHWDecoderFailed = TRUE;
      goto softwaredec;
    }
    if (bWMV9) {
      DbgLog((LOG_TRACE, 10, L"-> WMV9 DMO decoder failed, trying avcodec instead..."));
      m_bWMV9Failed = TRUE;
      bWMV9 = FALSE;
      goto softwaredec;
    }
    return hr;
  }

  m_Codec = codec;
  m_bSyncToProcess = m_pDecoder->SyncToProcessThread() == S_OK || (m_pLAVVideo->GetDecodeFlags() & LAV_VIDEO_DEC_FLAG_DVD);

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

    // Store the failed sample for re-try in a moment
    m_FailedSample = pSample;
    m_FailedSample->AddRef();

    // Schedule a re-init when the main thread goes there the next time
    m_bDecoderNeedsReInit = TRUE;

    // Make room in the sample buffer, to ensure the main thread can get in
    m_TempSample[0] = GetSample();
  }

  return S_OK;
}

// ILAVVideoCallback
STDMETHODIMP CDecodeThread::Deliver(LAVFrame *pFrame)
{
  m_Output.Push(pFrame);
  m_evSample.Set();
  if (m_bSyncToProcess) {
    m_evDeliver.Wait();
  }
  return S_OK;
}

STDMETHODIMP CDecodeThread::AllocateFrame(LAVFrame **ppFrame) { return m_pLAVVideo->AllocateFrame(ppFrame); }
STDMETHODIMP CDecodeThread::ReleaseFrame(LAVFrame **ppFrame) { return m_pLAVVideo->ReleaseFrame(ppFrame); }
STDMETHODIMP_(LPWSTR) CDecodeThread::GetFileExtension() { return m_pLAVVideo->GetFileExtension(); }
STDMETHODIMP_(DWORD) CDecodeThread::GetDecodeFlags() { return m_pLAVVideo->GetDecodeFlags(); }
STDMETHODIMP_(CMediaType&) CDecodeThread::GetInputMediaType() { return m_pLAVVideo->GetInputMediaType(); }
STDMETHODIMP CDecodeThread::GetLAVPinInfo(LAVPinInfo &info) { return m_pLAVVideo->GetLAVPinInfo(info); }
STDMETHODIMP_(CBasePin*) CDecodeThread::GetOutputPin() { return m_pLAVVideo->GetOutputPin(); }
STDMETHODIMP_(CMediaType&) CDecodeThread::GetOutputMediaType() { return m_pLAVVideo->GetOutputMediaType(); }
STDMETHODIMP CDecodeThread::DVDStripPacket(BYTE*& p, long& len) { return m_pLAVVideo->DVDStripPacket(p, len); }
STDMETHODIMP_(LAVFrame*) CDecodeThread::GetFlushFrame() { return m_pLAVVideo->GetFlushFrame(); }
STDMETHODIMP CDecodeThread::ReleaseAllDXVAResources() { return m_pLAVVideo->ReleaseAllDXVAResources(); }
STDMETHODIMP_(DWORD) CDecodeThread::GetGPUDeviceIndex() { return m_pLAVVideo->GetGPUDeviceIndex(); }
