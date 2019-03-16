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

#define MAX_DECODE_FRAMES 20
#define DISPLAY_DELAY	4
#define MAX_PIC_INDEX 64

#include "cuvid/dynlink_cuda.h"
#include "cuvid/dynlink_nvcuvid.h"

#define CUDA_INIT_D3D9
#include "cuvid/dynlink_cuda_d3d.h"

#include "parsers/AnnexBConverter.h"

#include <queue>

#define CUMETHOD(name) t##name *##name

class CDecCuvid : public CDecBase
{
public:
  CDecCuvid(void);
  virtual ~CDecCuvid(void);

  // ILAVDecoder
  STDMETHODIMP InitDecoder(AVCodecID codec, const CMediaType *pmt);
  STDMETHODIMP Decode(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity, IMediaSample *pSample);
  STDMETHODIMP Flush();
  STDMETHODIMP EndOfStream();
  STDMETHODIMP GetPixelFormat(LAVPixelFormat *pPix, int *pBpp);
  STDMETHODIMP_(REFERENCE_TIME) GetFrameDuration();
  STDMETHODIMP_(BOOL) IsInterlaced(BOOL bAllowGuess);
  STDMETHODIMP_(const WCHAR*) GetDecoderName() { return L"cuvid"; }
  STDMETHODIMP GetHWAccelActiveDevice(BSTR *pstrDeviceName);

  // CDecBase
  STDMETHODIMP Init();

private:
  STDMETHODIMP LoadCUDAFuncRefs();
  STDMETHODIMP DestroyDecoder(bool bFull);

  STDMETHODIMP InitD3D9(int best_device, DWORD requested_device);

  STDMETHODIMP CreateCUVIDDecoder(cudaVideoCodec codec, DWORD dwWidth, DWORD dwHeight, int nBitdepth, bool bProgressiveSequence);
  STDMETHODIMP DecodeSequenceData();

  // CUDA Callbacks
  static int CUDAAPI HandleVideoSequence(void *obj, CUVIDEOFORMAT *cuvidfmt);
  static int CUDAAPI HandlePictureDecode(void *obj, CUVIDPICPARAMS *cuvidpic);
  static int CUDAAPI HandlePictureDisplay(void *obj, CUVIDPARSERDISPINFO *cuviddisp);

  STDMETHODIMP Display(CUVIDPARSERDISPINFO *cuviddisp);
  STDMETHODIMP Deliver(CUVIDPARSERDISPINFO *cuviddisp, int field = 0);

  CUVIDPARSERDISPINFO* GetNextFrame();

  STDMETHODIMP FlushParser();

  STDMETHODIMP CheckH264Sequence(const BYTE *buffer, int buflen);
  STDMETHODIMP CheckHEVCSequence(const BYTE *buffer, int buflen, int *bitdepth);

  int GetMaxGflopsGraphicsDeviceId();

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
    CUMETHOD(cuDeviceGetCount);
    CUMETHOD(cuDriverGetVersion);
    CUMETHOD(cuDeviceGetName);
    CUMETHOD(cuDeviceComputeCapability);
    CUMETHOD(cuDeviceGetAttribute);

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
#ifdef _M_AMD64
    CUMETHOD(cuvidMapVideoFrame64);
    CUMETHOD(cuvidUnmapVideoFrame64);
#endif
  } cuda;

  IDirect3D9             *m_pD3D9       = nullptr;
  IDirect3DDevice9       *m_pD3DDevice9 = nullptr;

  CUcontext              m_cudaContext = 0;
  CUvideoctxlock         m_cudaCtxLock = 0;

  CUvideoparser          m_hParser     = 0;
  CUVIDEOFORMATEX        m_VideoParserExInfo;

  CUvideodecoder         m_hDecoder    = 0;
  CUVIDDECODECREATEINFO  m_VideoDecoderInfo;

  CUVIDEOFORMAT          m_VideoFormat;

  CUVIDPARSERDISPINFO    m_DisplayQueue[DISPLAY_DELAY];
  int                    m_DisplayPos = 0;

  CUVIDPICPARAMS         m_PicParams[MAX_PIC_INDEX];

  BOOL                   m_bVDPAULevelC = FALSE;
  char                   m_cudaDeviceName[256] = { 0 };

  BOOL                   m_bForceSequenceUpdate = FALSE;
  BOOL                   m_bInterlaced          = FALSE;
  BOOL                   m_bDoubleRateDeint     = FALSE;
  BOOL                   m_bFlushing            = FALSE;
  REFERENCE_TIME         m_rtAvgTimePerFrame    = AV_NOPTS_VALUE;
  REFERENCE_TIME         m_rtPrevDiff           = AV_NOPTS_VALUE;
  BOOL                   m_bWaitForKeyframe     = FALSE;
  int                    m_iFullRange           = -1;

  DXVA2_ExtendedFormat   m_DXVAExtendedFormat;

  BYTE                   *m_pbRawNV12 = nullptr;
  int                    m_cRawNV12   = 0;

  CAnnexBConverter       *m_AnnexBConverter = nullptr;

  BOOL                   m_bFormatIncompatible = FALSE;
  BOOL                   m_bNeedSequenceCheck  = FALSE;

  BOOL                   m_bUseTimestampQueue  = FALSE;
  std::queue<REFERENCE_TIME> m_timestampQueue;

  int                    m_nSoftTelecine  = 0;
  BOOL                   m_bTFF           = TRUE;
  BOOL                   m_bARPresent     = TRUE;
  BOOL                   m_bEndOfSequence = FALSE;

  int                    m_DisplayDelay   = DISPLAY_DELAY;
};
