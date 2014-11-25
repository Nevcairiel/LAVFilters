/*
 *      Copyright (C) 2011-2014 Hendrik Leppkes
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
#include "libavcodec/dxva2.h"

#define DXVA2_MAX_SURFACES 64
#define DXVA2_QUEUE_SURFACES 4
#define DXVA2_SURFACE_BASE_ALIGN 16

typedef HRESULT WINAPI pCreateDeviceManager9(UINT *pResetToken, IDirect3DDeviceManager9 **);

typedef struct {
  int index;
  bool used;
  LPDIRECT3DSURFACE9 d3d;
  uint64_t age;
} d3d_surface_t;

class CDXVA2SurfaceAllocator;

class CDecDXVA2 : public CDecAvcodec
{
public:
  CDecDXVA2(void);
  virtual ~CDecDXVA2(void);

  // ILAVDecoder
  STDMETHODIMP InitDecoder(AVCodecID codec, const CMediaType *pmt);
  STDMETHODIMP GetPixelFormat(LAVPixelFormat *pPix, int *pBpp);
  STDMETHODIMP Flush();
  STDMETHODIMP EndOfStream();

  STDMETHODIMP InitAllocator(IMemAllocator **ppAlloc);
  STDMETHODIMP PostConnect(IPin *pPin);
  STDMETHODIMP_(long) GetBufferCount();
  STDMETHODIMP_(const WCHAR*) GetDecoderName() { return m_bNative ? L"dxva2n" : L"dxva2cb"; }
  STDMETHODIMP HasThreadSafeBuffers() { return m_bNative ? S_FALSE : S_OK; }
  STDMETHODIMP SyncToProcessThread() { return HasThreadSafeBuffers() == S_OK ? S_FALSE : S_OK; }

  // CDecBase
  STDMETHODIMP Init();

  HRESULT SetNativeMode(BOOL bNative) { m_bNative = bNative; return S_OK; }

protected:
  HRESULT AdditionaDecoderInit();
  HRESULT PostDecode();
  HRESULT HandleDXVA2Frame(LAVFrame *pFrame);
  HRESULT DeliverDXVA2Frame(LAVFrame *pFrame);

  bool CopyFrame(LAVFrame *pFrame);

private:
  HRESULT InitD3D();
  STDMETHODIMP DestroyDecoder(bool bFull, bool bNoAVCodec = false);
  STDMETHODIMP FreeD3DResources();
  STDMETHODIMP LoadDXVA2Functions();

  HRESULT CreateD3DDeviceManager(IDirect3DDevice9 *pDevice, UINT *pReset, IDirect3DDeviceManager9 **ppManager);
  HRESULT CreateDXVAVideoService(IDirect3DDeviceManager9 *pManager, IDirectXVideoDecoderService **ppService);
  HRESULT FindVideoServiceConversion(AVCodecID codec, GUID *input, D3DFORMAT *output);
  HRESULT FindDecoderConfiguration(const GUID &input, const DXVA2_VideoDesc *pDesc, DXVA2_ConfigPictureDecode *pConfig);

  HRESULT CreateDXVA2Decoder(int nSurfaces = 0, IDirect3DSurface9 **ppSurfaces = nullptr);
  HRESULT SetD3DDeviceManager(IDirect3DDeviceManager9 *pDevManager);
  HRESULT DXVA2NotifyEVR();
  HRESULT RetrieveVendorId(IDirect3DDeviceManager9 *pDevManager);
  HRESULT CheckHWCompatConditions(GUID decoderGuid);
  HRESULT FillHWContext(dxva_context *ctx);

  HRESULT ReInitDXVA2Decoder(AVCodecContext *c);

  static enum AVPixelFormat get_dxva2_format(struct AVCodecContext *s, const enum AVPixelFormat * pix_fmts);
  static int get_dxva2_buffer(struct AVCodecContext *c, AVFrame *pic, int flags);
  static void free_dxva2_buffer(void *opaque, uint8_t *data);

  STDMETHODIMP FlushDisplayQueue(BOOL bDeliver);
  STDMETHODIMP FlushFromAllocator();

  DWORD GetAlignedDimension(DWORD dim);
  DWORD GetWidth() const { return m_pAVCtx->coded_width > 0 ? m_pAVCtx->coded_width : m_pAVCtx->width; }
  DWORD GetHeight() const { return m_pAVCtx->coded_height > 0 ? m_pAVCtx->coded_height : m_pAVCtx->height; }

private:
  friend class CDXVA2SurfaceAllocator;
  BOOL m_bNative = FALSE;
  CDXVA2SurfaceAllocator *m_pDXVA2Allocator = nullptr;

  struct {
    HMODULE dxva2lib;
    pCreateDeviceManager9 *createDeviceManager;
  } dx;

  IDirect3D9              *m_pD3D          = nullptr;
  IDirect3DDevice9        *m_pD3DDev       = nullptr;
  IDirect3DDeviceManager9 *m_pD3DDevMngr   = nullptr;
  UINT                    m_pD3DResetToken = 0;
  HANDLE                  m_hDevice        = INVALID_HANDLE_VALUE;

  IDirectXVideoDecoderService *m_pDXVADecoderService = nullptr;
  IDirectXVideoDecoder        *m_pDecoder            = nullptr;
  DXVA2_ConfigPictureDecode   m_DXVAVideoDecoderConfig;

  int                m_NumSurfaces       = 0;
  d3d_surface_t      m_pSurfaces[DXVA2_MAX_SURFACES];
  uint64_t           m_CurrentSurfaceAge = 1;

  LPDIRECT3DSURFACE9 m_pRawSurface[DXVA2_MAX_SURFACES];

  BOOL m_bFailHWDecode = FALSE;

  DXVA2_ExtendedFormat m_DXVAExtendedFormat;

  LAVFrame* m_FrameQueue[DXVA2_QUEUE_SURFACES];
  int       m_FrameQueuePosition = 0;

  DWORD     m_dwSurfaceWidth    = 0;
  DWORD     m_dwSurfaceHeight   = 0;
  DWORD     m_dwVendorId        = 0;
  DWORD     m_dwDeviceId        = 0;
  GUID      m_guidDecoderDevice = GUID_NULL;
  int       m_DisplayDelay      = DXVA2_QUEUE_SURFACES;

  CMediaType m_MediaType;
};
