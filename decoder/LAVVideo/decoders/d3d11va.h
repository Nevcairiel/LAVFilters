/*
 *      Copyright (C) 2017-2021 Hendrik Leppkes
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

extern "C"
{
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_d3d11va.h"
#include "libavcodec/d3d11va.h"
#include "libavcodec/h265_rext_profiles.h"
}

#define D3D11_QUEUE_SURFACES 4

typedef HRESULT(WINAPI *PFN_CREATE_DXGI_FACTORY1)(REFIID riid, void **ppFactory);

class CDecD3D11 : public CDecAvcodec
{
  public:
    CDecD3D11(void);
    virtual ~CDecD3D11(void);

    // ILAVDecoder
    STDMETHODIMP Check();
    STDMETHODIMP InitDecoder(AVCodecID codec, const CMediaType *pmt, const MediaSideDataFFMpeg *pSideData);
    STDMETHODIMP GetPixelFormat(LAVPixelFormat *pPix, int *pBpp);
    STDMETHODIMP Flush();
    STDMETHODIMP EndOfStream();

    STDMETHODIMP InitAllocator(IMemAllocator **ppAlloc);
    STDMETHODIMP PostConnect(IPin *pPin);
    STDMETHODIMP BreakConnect();
    STDMETHODIMP_(long) GetBufferCount(long *pMaxBuffers = nullptr);
    STDMETHODIMP_(const WCHAR *) GetDecoderName()
    {
        return m_bReadBackFallback ? (m_bDirect ? L"d3d11 cb direct" : L"d3d11 cb") : L"d3d11 native";
    }
    STDMETHODIMP HasThreadSafeBuffers() { return S_FALSE; }
    STDMETHODIMP SetDirectOutput(BOOL bDirect)
    {
        m_bDirect = bDirect;
        return S_OK;
    }
    STDMETHODIMP_(DWORD) GetHWAccelNumDevices();
    STDMETHODIMP GetHWAccelDeviceInfo(DWORD dwIndex, BSTR *pstrDeviceName, DWORD *dwDeviceIdentifier);
    STDMETHODIMP GetHWAccelActiveDevice(BSTR *pstrDeviceName);

    // CDecBase
    STDMETHODIMP Init();

  protected:
    HRESULT AdditionaDecoderInit();
    HRESULT PostDecode();

    HRESULT HandleDXVA2Frame(LAVFrame *pFrame);
    HRESULT DeliverD3D11Frame(LAVFrame *pFrame);
    HRESULT DeliverD3D11Readback(LAVFrame *pFrame);
    HRESULT DeliverD3D11ReadbackDirect(LAVFrame *pFrame);

    BOOL IsHardwareAccelerator() { return TRUE; }

  private:
    STDMETHODIMP DestroyDecoder(bool bFull);

    STDMETHODIMP ReInitD3D11Decoder(AVCodecContext *c);

    STDMETHODIMP CreateD3D11Device(UINT nDeviceIndex, ID3D11Device **ppDevice, DXGI_ADAPTER_DESC *pDesc);
    STDMETHODIMP CreateD3D11Decoder();
    STDMETHODIMP AllocateFramesContext(int width, int height, DXGI_FORMAT format, int nSurfaces,
                                       AVBufferRef **pFramesCtx);

    STDMETHODIMP FindVideoServiceConversion(AVCodecID codec, int profile, int level, DXGI_FORMAT &surface_format, GUID *input);
    STDMETHODIMP FindDecoderConfiguration(const D3D11_VIDEO_DECODER_DESC *desc, D3D11_VIDEO_DECODER_CONFIG *pConfig);

    STDMETHODIMP FillHWContext(AVD3D11VAContext *ctx);

    STDMETHODIMP FlushDisplayQueue(BOOL bDeliver);

    static enum AVPixelFormat get_d3d11_format(struct AVCodecContext *s, const enum AVPixelFormat *pix_fmts);
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
    DXGI_FORMAT m_SurfaceFormat = DXGI_FORMAT_UNKNOWN;

    BOOL m_bReadBackFallback = FALSE;
    BOOL m_bDirect = FALSE;
    BOOL m_bFailHWDecode = FALSE;

    BOOL m_bP016ToP010Fallback = FALSE;

    ID3D11Texture2D *m_pD3D11StagingTexture = nullptr;

    LAVFrame *m_FrameQueue[D3D11_QUEUE_SURFACES];
    int m_FrameQueuePosition = 0;
    int m_DisplayDelay = D3D11_QUEUE_SURFACES;

    struct
    {
        HMODULE d3d11lib;
        PFN_D3D11_CREATE_DEVICE mD3D11CreateDevice;

        HMODULE dxgilib;
        PFN_CREATE_DXGI_FACTORY1 mCreateDXGIFactory1;
    } dx = {0};

    DXGI_ADAPTER_DESC m_AdapterDesc = {0};

    friend class CD3D11SurfaceAllocator;
};
