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

#pragma once

#include "decoders/ILAVDecoder.h"
#include "SynchronizedQueue.h"

class CLAVVideo;

class CDecodeThread : public ILAVVideoCallback, protected CAMThread, protected CCritSec
{
public:
  CDecodeThread(CLAVVideo *pLAVVideo);
  ~CDecodeThread();

  // Parts of ILAVDecoder
  STDMETHODIMP_(const WCHAR*) GetDecoderName() { return m_pDecoder ? m_pDecoder->GetDecoderName() : NULL; }
  STDMETHODIMP_(long) GetBufferCount() { return m_pDecoder ? m_pDecoder->GetBufferCount() : 4; }
  STDMETHODIMP_(BOOL) IsInterlaced() { return m_pDecoder ? m_pDecoder->IsInterlaced() : TRUE; }
  STDMETHODIMP GetPixelFormat(LAVPixelFormat *pPix, int *pBpp) { ASSERT(m_pDecoder); return m_pDecoder->GetPixelFormat(pPix, pBpp); }
  STDMETHODIMP_(REFERENCE_TIME) GetFrameDuration() { ASSERT(m_pDecoder); return m_pDecoder->GetFrameDuration(); }
  STDMETHODIMP HasThreadSafeBuffers() { return m_bThreadSafe ? S_OK : S_FALSE; }


  STDMETHODIMP CreateDecoder(const CMediaType *pmt, CodecID codec);
  STDMETHODIMP Close();

  STDMETHODIMP Decode(IMediaSample *pSample);
  STDMETHODIMP Flush();
  STDMETHODIMP EndOfStream();

  STDMETHODIMP InitAllocator(IMemAllocator **ppAlloc);
  STDMETHODIMP PostConnect(IPin *pPin);

  STDMETHODIMP_(BOOL) IsHWDecoderActive() { return m_bHWDecoder; }

  // ILAVVideoCallback
  STDMETHODIMP AllocateFrame(LAVFrame **ppFrame);
  STDMETHODIMP ReleaseFrame(LAVFrame **ppFrame);
  STDMETHODIMP Deliver(LAVFrame *pFrame);
  STDMETHODIMP_(LPWSTR) GetFileExtension();
  STDMETHODIMP_(BOOL) FilterInGraph(PIN_DIRECTION dir, const GUID &clsid);
  STDMETHODIMP_(BOOL) VC1IsDTS();
  STDMETHODIMP_(BOOL) H264IsAVI();
  STDMETHODIMP_(BOOL) IsLAVSplitter();
  STDMETHODIMP_(BOOL) IsVistaOrNewer();
  STDMETHODIMP_(CMediaType&) GetInputMediaType();
  STDMETHODIMP GetLAVPinInfo(LAVPinInfo &info);
  STDMETHODIMP_(CBasePin*) GetOutputPin();

protected:
  DWORD ThreadProc();

private:
  STDMETHODIMP CreateDecoderInternal(const CMediaType *pmt, CodecID codec);
  STDMETHODIMP PostConnectInternal(IPin *pPin);

  STDMETHODIMP DecodeInternal(IMediaSample *pSample);
  STDMETHODIMP ClearQueues();
  STDMETHODIMP ProcessOutput();

  bool HasSample() { CAutoLock lock(&m_SampleCritSec); return m_NextSample != NULL; }
  void PutSample(IMediaSample *pSample) { CAutoLock lock(&m_SampleCritSec); m_NextSample = pSample; }
  IMediaSample* GetSample() { CAutoLock lock(&m_SampleCritSec); return m_NextSample; }
  void ReleaseSample() { CAutoLock lock(&m_SampleCritSec); SafeRelease(&m_NextSample); }

private:
  enum {CMD_CREATE_DECODER, CMD_CLOSE_DECODER, CMD_FLUSH, CMD_EOS, CMD_EXIT, CMD_INIT_ALLOCATOR, CMD_POST_CONNECT};

  CLAVVideo    *m_pLAVVideo;
  ILAVDecoder  *m_pDecoder;

  CodecID      m_Codec;

  BOOL         m_bHWDecoder;
  BOOL         m_bHWDecoderFailed;

  BOOL         m_bThreadSafe;
  CAMEvent     m_evInput;
  CAMEvent     m_evDeliver;
  CAMEvent     m_evSample;
  CAMEvent     m_evDecodeDone;
  CAMEvent     m_evEOSDone;

  CCritSec     m_ThreadCritSec;
  struct {
    const CMediaType *pmt;
    CodecID codec;
    IMemAllocator **allocator;
    IPin *pin;
  } m_ThreadCallContext;
  CSynchronizedQueue<LAVFrame *> m_Output;

  CCritSec     m_SampleCritSec;
  IMediaSample *m_NextSample;

  std::wstring m_processName;
};
