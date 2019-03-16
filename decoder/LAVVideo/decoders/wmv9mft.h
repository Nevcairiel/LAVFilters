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
#include "DecBase.h"

#include <mfidl.h>

#include <vector>
#include <queue>

////////////////////////////////////////////////////////////////////////////////
// Dynlink types
////////////////////////////////////////////////////////////////////////////////
typedef HRESULT STDAPICALLTYPE tMFStartup(ULONG Version, DWORD dwFlags);
typedef HRESULT STDAPICALLTYPE tMFShutdown();

typedef HRESULT STDAPICALLTYPE tMFCreateMediaType(IMFMediaType**  ppMFType);
typedef HRESULT STDAPICALLTYPE tMFCreateSample(IMFSample **ppIMFSample);
typedef HRESULT STDAPICALLTYPE tMFCreateAlignedMemoryBuffer(DWORD cbMaxLength, DWORD cbAligment, IMFMediaBuffer **ppBuffer);

typedef HRESULT STDAPICALLTYPE tMFAverageTimePerFrameToFrameRate(UINT64 unAverageTimePerFrame, UINT32 *punNumerator, UINT32 *punDenominator);

#define MFMETHOD(name) tMF##name *##name

////////////////////////////////////////////////////////////////////////////////
// Class
////////////////////////////////////////////////////////////////////////////////

class CVC1HeaderParser;

class CDecWMV9MFT : public CDecBase
{
  typedef struct _Buffer {
    IMFMediaBuffer *pBuffer = nullptr;
    DWORD size              = 0;
    bool used               = false;
  } Buffer;

public:
  CDecWMV9MFT(void);
  virtual ~CDecWMV9MFT(void);

  // ILAVDecoder
  STDMETHODIMP InitDecoder(AVCodecID codec, const CMediaType *pmt);
  STDMETHODIMP Decode(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity, IMediaSample *pSample);
  STDMETHODIMP Flush();
  STDMETHODIMP EndOfStream();
  STDMETHODIMP GetPixelFormat(LAVPixelFormat *pPix, int *pBpp);
  STDMETHODIMP_(BOOL) IsInterlaced(BOOL bAllowGuess);
  STDMETHODIMP_(const WCHAR*) GetDecoderName() { return L"wmv9 mft"; }
  STDMETHODIMP HasThreadSafeBuffers() { return S_OK; }

  // CDecBase
  STDMETHODIMP Init();

private:
  STDMETHODIMP DestroyDecoder(bool bFull);
  STDMETHODIMP ProcessOutput();
  STDMETHODIMP SelectOutputType();

  IMFMediaBuffer * CreateMediaBuffer(const BYTE * pData, DWORD dwDataLen);

  static void wmv9_buffer_destruct(LAVFrame *pFrame);

  IMFMediaBuffer *GetBuffer(DWORD dwRequiredSize);
  void ReleaseBuffer(IMFMediaBuffer *pBuffer);

private:
  IMFTransform *m_pMFT = nullptr;

  BOOL m_bInterlaced     = TRUE;

  LAVPixelFormat m_OutPixFmt = LAVPixFmt_None;
  AVCodecID m_nCodecId       = AV_CODEC_ID_NONE;

  CCritSec m_BufferCritSec;
  std::vector<Buffer *> m_BufferQueue;

  BOOL m_bNeedKeyFrame             = TRUE;
  BOOL m_bManualReorder            = FALSE;
  BOOL m_bReorderBufferValid       = FALSE;
  REFERENCE_TIME m_rtReorderBuffer = AV_NOPTS_VALUE;
  std::queue<REFERENCE_TIME> m_timestampQueue;

  CVC1HeaderParser *m_vc1Header = nullptr;

  struct {
    HMODULE mfplat = NULL;
    MFMETHOD(Startup);
    MFMETHOD(Shutdown);
    MFMETHOD(CreateAlignedMemoryBuffer);
    MFMETHOD(CreateSample);
    MFMETHOD(CreateMediaType);
    MFMETHOD(AverageTimePerFrameToFrameRate);
  } MF;
};
