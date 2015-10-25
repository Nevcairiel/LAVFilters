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

#pragma once
#include "DecBase.h"

#include <dmo.h>

#include <vector>
#include <queue>

class CVC1HeaderParser;

typedef struct _Buffer {
  BYTE *buffer = nullptr;
  size_t size  = 0;
  bool used    = false;
} Buffer;

class CDecWMV9 : public CDecBase
{
public:
  CDecWMV9(void);
  virtual ~CDecWMV9(void);

  // ILAVDecoder
  STDMETHODIMP InitDecoder(AVCodecID codec, const CMediaType *pmt);
  STDMETHODIMP Decode(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity);
  STDMETHODIMP Flush();
  STDMETHODIMP EndOfStream();
  STDMETHODIMP GetPixelFormat(LAVPixelFormat *pPix, int *pBpp);
  STDMETHODIMP_(REFERENCE_TIME) GetFrameDuration();
  STDMETHODIMP_(BOOL) IsInterlaced();
  STDMETHODIMP_(const WCHAR*) GetDecoderName() { return L"wmv9 dmo"; }
  STDMETHODIMP HasThreadSafeBuffers() { return S_OK; }

  // CDecBase
  STDMETHODIMP Init();

private:
  STDMETHODIMP DestroyDecoder(bool bFull);
  STDMETHODIMP ProcessOutput();

  static void wmv9_buffer_destruct(LAVFrame *pFrame);

  BYTE *GetBuffer(size_t size);
  void ReleaseBuffer(BYTE *buffer);

private:
  IMediaObject *m_pDMO = nullptr;
  CMediaType mtIn;
  CMediaType mtOut;

  DWORD m_pRawBufferSize = 0;

  BOOL m_bInterlaced     = TRUE;
  AVRational m_StreamAR = AVRational{0, 0};

  LAVPixelFormat m_OutPixFmt = LAVPixFmt_None;
  AVCodecID m_nCodecId       = AV_CODEC_ID_NONE;

  CCritSec m_BufferCritSec;
  std::vector<Buffer *> m_BufferQueue;

  BOOL m_bNeedKeyFrame             = FALSE;
  BOOL m_bManualReorder            = FALSE;
  BOOL m_bReorderBufferValid       = FALSE;
  REFERENCE_TIME m_rtReorderBuffer = AV_NOPTS_VALUE;
  std::queue<REFERENCE_TIME> m_timestampQueue;

  CVC1HeaderParser *m_vc1Header = nullptr;
};
