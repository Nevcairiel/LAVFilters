/*
 *      Copyright (C) 2011 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 */

#pragma once
#include "DecBase.h"

#define MAX_DECODE_FRAMES 20
#define DISPLAY_DELAY	2
#define USE_ASYNC_COPY 1

#define CUDA_FORCE_API_VERSION 3010
#include "cuvid/cuda.h"
#include "cuvid/nvcuvid.h"
#include "cuvid/cuda_dynlink.h"

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

private:
  struct {
    HMODULE cudaLib;
    CUMETHOD(cuInit);
    CUMETHOD(cuCtxCreate);
    CUMETHOD(cuCtxDestroy);
    CUMETHOD(cuCtxPushCurrent);
    CUMETHOD(cuCtxPopCurrent);
    CUMETHOD(cuD3D9CtxCreate);

    HMODULE cuvidLib;
    CUMETHOD(cuvidCtxLockCreate);
    CUMETHOD(cuvidCtxLockDestroy);
    CUMETHOD(cuvidCreateVideoParser);
    CUMETHOD(cuvidParseVideoData);
    CUMETHOD(cuvidDestroyVideoParser);
    CUMETHOD(cuvidCreateDecoder);
    CUMETHOD(cuvidDecodePicture);
    CUMETHOD(cuvidDestroyDecoder);
  } cuda;

  IDirect3D9             *m_pD3D;
  IDirect3DDevice9       *m_pD3DDevice;

  CUcontext              m_cudaContext;
  CUvideoctxlock         m_cudaCtxLock;

  CUvideoparser          m_hParser;
  CUvideodecoder         m_hDecoder;
  CUVIDDECODECREATEINFO  m_VideoDecoderInfo;

  CUVIDPARSERDISPINFO    m_DisplayQueue[DISPLAY_DELAY];
  int                    m_DisplayPos;
};
