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
#include "avcodec.h"

#define DXVA2_MAX_SURFACES 64
#define DXVA2_QUEUE_SURFACES 2

typedef HRESULT WINAPI pCreateDeviceManager9(UINT *pResetToken, IDirect3DDeviceManager9 **);

typedef struct {
  LPDIRECT3DSURFACE9 d3d;
  uint64_t age;
  long ref;
} d3d_surface_t;

class CDecDXVA2 : public CDecAvcodec
{
public:
  CDecDXVA2(void);
  virtual ~CDecDXVA2(void);

  // ILAVDecoder
  STDMETHODIMP InitDecoder(CodecID codec, const CMediaType *pmt);
  STDMETHODIMP GetPixelFormat(LAVPixelFormat *pPix, int *pBpp);
  STDMETHODIMP Flush();
  STDMETHODIMP EndOfStream();

  // CDecBase
  STDMETHODIMP Init();

protected:
  HRESULT AdditionaDecoderInit();
  HRESULT PostDecode();
  HRESULT HandleDXVA2Frame(LAVFrame *pFrame);
  HRESULT DeliverDXVA2Frame(LAVFrame *pFrame);

  bool CopyFrame(LAVFrame *pFrame);

private:
  STDMETHODIMP DestroyDecoder(bool bFull);
  STDMETHODIMP LoadDXVA2Functions();

  HRESULT CreateD3DDeviceManager(IDirect3DDevice9 *pDevice, UINT *pReset, IDirect3DDeviceManager9 **ppManager);
  HRESULT CreateDXVAVideoService(IDirect3DDevice9 *pDevice, IDirect3DDeviceManager9 *pManager, IDirectXVideoDecoderService **ppService);
  HRESULT FindVideoServiceConversion(CodecID codec, GUID *input, D3DFORMAT *output);

  static int get_dxva2_buffer(struct AVCodecContext *c, AVFrame *pic);
  static void release_dxva2_buffer(struct AVCodecContext *c, AVFrame *pic);
  d3d_surface_t *FindSurface(LPDIRECT3DSURFACE9 pSurface);

private:
  struct {
    HMODULE dxva2lib;
    pCreateDeviceManager9 *createDeviceManager;
  } dx;

  IDirect3D9              *m_pD3D;
  IDirect3DDevice9        *m_pD3DDev;
  IDirect3DDeviceManager9 *m_pD3DDevMngr;
  UINT                    m_pD3DResetToken;

  IDirectXVideoDecoderService *m_pDXVADecoderService;
  IDirectXVideoDecoder        *m_pDecoder;
  DXVA2_ConfigPictureDecode   m_DXVAVideoDecoderConfig;

  int                m_NumSurfaces;
  d3d_surface_t      m_pSurfaces[DXVA2_MAX_SURFACES];
  uint64_t           m_CurrentSurfaceAge;

  LPDIRECT3DSURFACE9 m_pRawSurface[DXVA2_MAX_SURFACES];

  BOOL m_bInterlaced;
  CodecID m_nCodecId;

  DXVA2_ExtendedFormat m_DXVAExtendedFormat;

  LAVFrame* m_FrameQueue[DXVA2_QUEUE_SURFACES];
  int       m_FrameQueuePosition;
};
