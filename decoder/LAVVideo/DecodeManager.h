/*
 *      Copyright (C) 2010-2019 Hendrik Leppkes
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

class CDecodeManager : protected CCritSec
{
public:
  CDecodeManager(CLAVVideo *pLAVVideo);
  ~CDecodeManager();

  static ILAVDecoder * CreateHWAccelDecoder(LAVHWAccel hwAccel);

  // Decoder management
  STDMETHODIMP CreateDecoder(const CMediaType *pmt, AVCodecID codec);
  STDMETHODIMP Close();

  // Media control
  STDMETHODIMP Decode(IMediaSample *pSample);
  STDMETHODIMP Flush();
  STDMETHODIMP EndOfStream();

  // Allocator/memory management
  STDMETHODIMP InitAllocator(IMemAllocator **ppAlloc);
  STDMETHODIMP PostConnect(IPin *pPin);
  STDMETHODIMP BreakConnect();

  // HWAccel Query
  STDMETHODIMP_(BOOL) IsHWDecoderActive() { return m_bHWDecoder; }
  STDMETHODIMP GetHWAccelActiveDevice(BSTR *pstrDeviceName) { return m_pDecoder ? m_pDecoder->GetHWAccelActiveDevice(pstrDeviceName) : E_UNEXPECTED; }

  // ILAVDecoder (partial)
  STDMETHODIMP_(const WCHAR*) GetDecoderName() { return m_pDecoder ? m_pDecoder->GetDecoderName() : nullptr; }
  STDMETHODIMP_(long) GetBufferCount(long *pMaxBuffers) { return m_pDecoder ? m_pDecoder->GetBufferCount(pMaxBuffers) : 4; }
  STDMETHODIMP_(BOOL) IsInterlaced(BOOL bAllowGuess) { return m_pDecoder ? m_pDecoder->IsInterlaced(bAllowGuess) : TRUE; }
  STDMETHODIMP GetPixelFormat(LAVPixelFormat *pPix, int *pBpp) { ASSERT(m_pDecoder); return m_pDecoder->GetPixelFormat(pPix, pBpp); }
  STDMETHODIMP_(REFERENCE_TIME) GetFrameDuration() { ASSERT(m_pDecoder); return m_pDecoder->GetFrameDuration(); }
  STDMETHODIMP HasThreadSafeBuffers() { return m_pDecoder ? m_pDecoder->HasThreadSafeBuffers() : S_FALSE; }
  STDMETHODIMP SetDirectOutput(BOOL bDirect) { return m_pDecoder ? m_pDecoder->SetDirectOutput(bDirect) : S_FALSE; }

private:
  CLAVVideo    *m_pLAVVideo = nullptr;
  ILAVDecoder  *m_pDecoder  = nullptr;

  AVCodecID    m_Codec      = AV_CODEC_ID_NONE;

  BOOL         m_bHWDecoder = FALSE;
  BOOL         m_bHWDecoderFailed = FALSE;

  BOOL         m_bWMV9Failed = FALSE;

  std::wstring m_processName;
};
