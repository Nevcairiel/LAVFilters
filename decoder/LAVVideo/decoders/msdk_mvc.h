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
#include "IMediaSideData.h"

#include "mfxvideo.h"
#include "mfxmvc.h"

#include <deque>
#include <vector>

#define ASYNC_DEPTH 8
#define ASYNC_QUEUE_SIZE (ASYNC_DEPTH + 2)

// 10s timestamp offset to avoid negative timestamps
#define TIMESTAMP_OFFSET 100000000i64

#define MFX_IMPL_VIA_MASK 0x0F00

typedef struct _MVCBuffer {
  mfxFrameSurface1 surface = { 0 };
  bool queued = false;
  mfxSyncPoint sync = nullptr;
} MVCBuffer;

typedef struct _MVCGOP {
  std::deque<mfxU64> timestamps;
  std::deque<MediaSideData3DOffset> offsets;
} MVCGOP;

class CDecMSDKMVC : public CDecBase
{
public:
  CDecMSDKMVC();
  virtual ~CDecMSDKMVC();

  // ILAVDecoder
  STDMETHODIMP InitDecoder(AVCodecID codec, const CMediaType *pmt);
  STDMETHODIMP Decode(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity, IMediaSample *pSample);
  STDMETHODIMP Flush();
  STDMETHODIMP EndOfStream();
  STDMETHODIMP GetPixelFormat(LAVPixelFormat *pPix, int *pBpp) { if (pPix) *pPix = LAVPixFmt_NV12; if (pBpp) *pBpp = 8; return S_OK; }
  STDMETHODIMP_(BOOL) IsInterlaced(BOOL bAllowGuess) { return FALSE; }
  STDMETHODIMP_(const WCHAR*) GetDecoderName() { return (m_mfxImpl != MFX_IMPL_SOFTWARE) ? L"msdk mvc hw" : L"msdk mvc"; }
  STDMETHODIMP HasThreadSafeBuffers() { return S_OK; }

  // CDecBase
  STDMETHODIMP Init();

private:
  void DestroyDecoder(bool bFull);
  STDMETHODIMP AllocateMVCExtBuffers();

  MVCBuffer * GetBuffer();
  MVCBuffer * FindBuffer(mfxFrameSurface1 * pSurface);
  void ReleaseBuffer(mfxFrameSurface1 * pSurface);

  HRESULT HandleOutput(MVCBuffer * pOutputBuffer);
  HRESULT DeliverOutput(MVCBuffer * pBaseView, MVCBuffer * pExtraView);

  HRESULT ParseSEI(const BYTE *buffer, size_t size, mfxU64 timestamp);
  HRESULT ParseMVCNestedSEI(const BYTE *buffer, size_t size, mfxU64 timestamp);
  HRESULT ParseUnregUserDataSEI(const BYTE *buffer, size_t size, mfxU64 timestamp);
  HRESULT ParseOffsetMetadata(const BYTE *buffer, size_t size, mfxU64 timestamp);

  void AddFrameToGOP(mfxU64 timestamp);
  BOOL RemoveFrameFromGOP(MVCGOP * pGOP, mfxU64 timestamp);
  void GetOffsetSideData(LAVFrame *pFrame, mfxU64 timestamp);

  static void msdk_buffer_destruct(LAVFrame *pFrame);

private:

  mfxSession m_mfxSession = nullptr;
  mfxVersion m_mfxVersion = { 0 };
  mfxIMPL    m_mfxImpl = 0;

  BOOL                 m_bDecodeReady   = FALSE;
  mfxVideoParam        m_mfxVideoParams = { 0 };

  mfxExtBuffer        *m_mfxExtParam[1] = { 0 };
  mfxExtMVCSeqDesc     m_mfxExtMVCSeq   = { 0 };

  CCritSec m_BufferCritSec;
  std::vector<MVCBuffer *> m_BufferQueue;

  GrowableArray<BYTE>  m_buff;
  int                  m_nMP4NALUSize = 0;

  MVCBuffer           *m_pOutputQueue[ASYNC_QUEUE_SIZE] = { 0 };
  int                  m_nOutputQueuePosition = 0;

  std::deque<MVCGOP>    m_GOPs;
  MediaSideData3DOffset m_PrevOffset;
};
