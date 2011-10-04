/*
 *      Copyright (C) 2011 Hendrik Leppkes
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

#define MAX_DECODE_FRAMES 20
#define DISPLAY_DELAY	4
#define USE_ASYNC_COPY 1

#define CUDA_FORCE_API_VERSION 3010
#include "cuvid/cuda.h"
#include "cuvid/nvcuvid.h"
#include "cuvid/cuda_dynlink.h"

#include "parsers/AVC1AnnexBConverter.h"

#include <queue>

#define CUMETHOD(name) t##name *##name

class CDecCuvid : public CDecBase
{
public:
  CDecCuvid(void);
  virtual ~CDecCuvid(void);

  // ILAVDecoder
  STDMETHODIMP InitDecoder(CodecID codec, const CMediaType *pmt);
  STDMETHODIMP Decode(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity);
  STDMETHODIMP Flush();
  STDMETHODIMP EndOfStream();
  STDMETHODIMP GetPixelFormat(LAVPixelFormat *pPix, int *pBpp);
  STDMETHODIMP_(REFERENCE_TIME) GetFrameDuration();

  // CDecBase
  STDMETHODIMP Init();

private:
  STDMETHODIMP LoadCUDAFuncRefs();
  STDMETHODIMP DestroyDecoder(bool bFull);

  STDMETHODIMP CreateCUVIDDecoder(cudaVideoCodec codec, DWORD dwWidth, DWORD dwHeight, DWORD dwDisplayWidth, DWORD dwDisplayHeight, RECT rcDisplayArea);
  STDMETHODIMP DecodeSequenceData();

  // CUDA Callbacks
  static int CUDAAPI HandleVideoSequence(void *obj, CUVIDEOFORMAT *cuvidfmt);
  static int CUDAAPI HandlePictureDecode(void *obj, CUVIDPICPARAMS *cuvidpic);
  static int CUDAAPI HandlePictureDisplay(void *obj, CUVIDPARSERDISPINFO *cuviddisp);

  STDMETHODIMP Display(CUVIDPARSERDISPINFO *cuviddisp);
  STDMETHODIMP Deliver(CUVIDPARSERDISPINFO *cuviddisp, int field = 0);

  STDMETHODIMP FlushParser();

  STDMETHODIMP CheckH264Sequence(const BYTE *buffer, int buflen);

  HWND GetDummyHWND();

private:
  struct {
    HMODULE cudaLib;
    CUMETHOD(cuInit);
    CUMETHOD(cuCtxCreate);
    CUMETHOD(cuCtxDestroy);
    CUMETHOD(cuCtxPushCurrent);
    CUMETHOD(cuCtxPopCurrent);
    CUMETHOD(cuD3D9CtxCreate);
    CUMETHOD(cuMemAllocHost);
    CUMETHOD(cuMemFreeHost);
    CUMETHOD(cuMemcpyDtoH);
    CUMETHOD(cuMemcpyDtoHAsync);
    CUMETHOD(cuStreamCreate);
    CUMETHOD(cuStreamDestroy);
    CUMETHOD(cuStreamQuery);

    HMODULE cuvidLib;
    CUMETHOD(cuvidCtxLockCreate);
    CUMETHOD(cuvidCtxLockDestroy);
    CUMETHOD(cuvidCtxLock);
    CUMETHOD(cuvidCtxUnlock);
    CUMETHOD(cuvidCreateVideoParser);
    CUMETHOD(cuvidParseVideoData);
    CUMETHOD(cuvidDestroyVideoParser);
    CUMETHOD(cuvidCreateDecoder);
    CUMETHOD(cuvidDecodePicture);
    CUMETHOD(cuvidDestroyDecoder);
    CUMETHOD(cuvidMapVideoFrame);
    CUMETHOD(cuvidUnmapVideoFrame);
  } cuda;

  IDirect3D9             *m_pD3D;
  IDirect3DDevice9       *m_pD3DDevice;

  CUcontext              m_cudaContext;
  CUvideoctxlock         m_cudaCtxLock;

  CUvideoparser          m_hParser;
  CUVIDEOFORMATEX        m_VideoParserExInfo;

  CUvideodecoder         m_hDecoder;
  CUVIDDECODECREATEINFO  m_VideoDecoderInfo;

  CUVIDEOFORMAT          m_VideoFormat;

  CUVIDPARSERDISPINFO    m_DisplayQueue[DISPLAY_DELAY];
  int                    m_DisplayPos;

  CUstream               m_hStream;

  BOOL                   m_bForceSequenceUpdate;
  BOOL                   m_bInterlaced;
  BOOL                   m_bDoubleRateDeint;
  BOOL                   m_bFlushing;
  REFERENCE_TIME         m_rtAvgTimePerFrame;
  BOOL                   m_bWaitForKeyframe;
  BOOL                   m_iFullRange;

  DXVA2_ExtendedFormat   m_DXVAExtendedFormat;

  BYTE                   *m_pbRawNV12;
  int                    m_cRawNV12;

  CAVC1AnnexBConverter   *m_AVC1Converter;

  BOOL                   m_bFormatIncompatible;
  BOOL                   m_bNeedSequenceCheck;

  BOOL                   m_bUseTimestampQueue;
  std::queue<REFERENCE_TIME> m_timestampQueue;

  HWND                   m_hwnd;
};
