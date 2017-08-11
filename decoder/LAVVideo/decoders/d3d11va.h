/*
*      Copyright (C) 2017 Hendrik Leppkes
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
#include "avcodec.h"

#include <d3d11.h>
#include <dxgi.h>

#include "d3d11/D3D11SurfaceAllocator.h"

extern "C" {
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_d3d11va.h"
#include "libavcodec/d3d11va.h"
}

#define D3D11_QUEUE_SURFACES 4

class CDecD3D11 : public CDecAvcodec
{
public:
  CDecD3D11(void);
  virtual ~CDecD3D11(void);

  // ILAVDecoder
  STDMETHODIMP InitDecoder(AVCodecID codec, const CMediaType *pmt);
  STDMETHODIMP GetPixelFormat(LAVPixelFormat *pPix, int *pBpp);
  STDMETHODIMP Flush();
  STDMETHODIMP EndOfStream();

  STDMETHODIMP InitAllocator(IMemAllocator **ppAlloc);
  STDMETHODIMP PostConnect(IPin *pPin);
  STDMETHODIMP_(long) GetBufferCount();
  STDMETHODIMP_(const WCHAR*) GetDecoderName() { return m_bReadBackFallback ? L"d3d11 cb" : L"d3d11 native"; }
  STDMETHODIMP HasThreadSafeBuffers() { return S_FALSE; }

protected:
  HRESULT AdditionaDecoderInit();
  HRESULT PostDecode();

  HRESULT HandleDXVA2Frame(LAVFrame *pFrame);
  HRESULT DeliverD3D11Readback(LAVFrame *pFrame);

private:
  STDMETHODIMP DestroyDecoder(bool bFull, bool bNoAVCodec = false);

  STDMETHODIMP ReInitD3D11Decoder(AVCodecContext *c);

  STDMETHODIMP CreateD3D11Decoder();
  STDMETHODIMP AllocateFramesContext(int width, int height, AVPixelFormat format, int nSurfaces, AVBufferRef **pFramesCtx);

  STDMETHODIMP FindVideoServiceConversion(AVCodecID codec, int profile, DXGI_FORMAT surface_format, GUID *input);
  STDMETHODIMP FindDecoderConfiguration(const D3D11_VIDEO_DECODER_DESC *desc, D3D11_VIDEO_DECODER_CONFIG *pConfig);

  STDMETHODIMP FillHWContext(AVD3D11VAContext *ctx);

  STDMETHODIMP FlushDisplayQueue(BOOL bDeliver);

  static enum AVPixelFormat get_d3d11_format(struct AVCodecContext *s, const enum AVPixelFormat * pix_fmts);
  static int get_d3d11_buffer(struct AVCodecContext *c, AVFrame *pic, int flags);

private:
  CD3D11SurfaceAllocator *m_pAllocator = nullptr;

  AVBufferRef *m_pDevCtx = nullptr;
  AVBufferRef *m_pFramesCtx = nullptr;

  D3D11_VIDEO_DECODER_CONFIG m_DecoderConfig;
  ID3D11VideoDecoder *m_pDecoder = nullptr;

  int m_nOutputViews = 0;
  ID3D11VideoDecoderOutputView **m_pOutputViews = nullptr;

  DWORD m_dwSurfaceWidth = 0;
  DWORD m_dwSurfaceHeight = 0;
  DWORD m_dwSurfaceCount = 0;
  AVPixelFormat m_DecodePixelFormat = AV_PIX_FMT_NONE;
  DXGI_FORMAT m_SurfaceFormat = DXGI_FORMAT_UNKNOWN;

  BOOL m_bReadBackFallback = FALSE;
  BOOL m_bFailHWDecode = FALSE;

  LAVFrame* m_FrameQueue[D3D11_QUEUE_SURFACES];
  int       m_FrameQueuePosition = 0;
  int       m_DisplayDelay = D3D11_QUEUE_SURFACES;

  friend class CD3D11SurfaceAllocator;
};
