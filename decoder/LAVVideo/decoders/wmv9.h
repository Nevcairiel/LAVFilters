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
#include "DecBase.h"

#include <dmo.h>

#include <vector>

typedef struct _Buffer {
  BYTE *buffer;
  size_t size;
  bool used;
} Buffer;

class CDecWMV9 : public CDecBase
{
public:
  CDecWMV9(void);
  virtual ~CDecWMV9(void);

  // ILAVDecoder
  STDMETHODIMP InitDecoder(CodecID codec, const CMediaType *pmt);
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
  IMediaObject *m_pDMO;
  CMediaType mtIn;
  CMediaType mtOut;

  DWORD m_pRawBufferSize;

  BOOL m_bInterlaced;
  AVRational m_StreamAR;

  LAVPixelFormat m_OutPixFmt;

  CCritSec m_BufferCritSec;
  std::vector<Buffer *> m_BufferQueue;
};
