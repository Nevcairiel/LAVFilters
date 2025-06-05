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

#include "stdafx.h"
#include "d3d11va.h"
#include "ID3DVideoMemoryConfiguration.h"
#include "dxva2/dxva_common.h"
#include "moreuuids.h"

#include <d3d11_1.h>

#define FF_DXVA2_WORKAROUND_NVIDIA_HEVC_420P12 3

ILAVDecoder *CreateDecoderD3D11()
{
    return new CDecD3D11();
}

HRESULT VerifyD3D11Device(DWORD &dwIndex, DWORD dwDeviceId)
{
    HRESULT hr = S_OK;
    DXGI_ADAPTER_DESC desc;

    HMODULE dxgi = LoadLibrary(L"dxgi.dll");
    if (dxgi == nullptr)
    {
        hr = E_FAIL;
        goto done;
    }

    PFN_CREATE_DXGI_FACTORY1 mCreateDXGIFactory1 = (PFN_CREATE_DXGI_FACTORY1)GetProcAddress(dxgi, "CreateDXGIFactory1");
    if (mCreateDXGIFactory1 == nullptr)
    {
        hr = E_FAIL;
        goto done;
    }

    IDXGIAdapter *pDXGIAdapter = nullptr;
    IDXGIFactory1 *pDXGIFactory = nullptr;

    hr = mCreateDXGIFactory1(IID_IDXGIFactory1, (void **)&pDXGIFactory);
    if (FAILED(hr))
        goto done;

    // check the adapter specified by dwIndex
    hr = pDXGIFactory->EnumAdapters(dwIndex, &pDXGIAdapter);
    if (FAILED(hr))
        goto done;

    // if it matches the device id, then all is well and we're done
    pDXGIAdapter->GetDesc(&desc);
    if (desc.DeviceId == dwDeviceId)
        goto done;

    SafeRelease(&pDXGIAdapter);

    // try to find a device that matches this device id
    UINT i = 0;
    while (SUCCEEDED(pDXGIFactory->EnumAdapters(i, &pDXGIAdapter)))
    {
        pDXGIAdapter->GetDesc(&desc);
        SafeRelease(&pDXGIAdapter);

        if (desc.DeviceId == dwDeviceId)
        {
            dwIndex = i;
            goto done;
        }
        i++;
    }

    // if none is found, fail
    hr = E_FAIL;

done:
    SafeRelease(&pDXGIAdapter);
    SafeRelease(&pDXGIFactory);
    FreeLibrary(dxgi);
    return hr;
}

////////////////////////////////////////////////////////////////////////////////
// D3D11 decoder implementation
////////////////////////////////////////////////////////////////////////////////

static const D3D_FEATURE_LEVEL s_D3D11Levels[] = {
    D3D_FEATURE_LEVEL_12_1,
    D3D_FEATURE_LEVEL_12_0,
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
};

static int s_GetD3D11FeatureLevels(int max_fl, const D3D_FEATURE_LEVEL **out)
{
    static const int levels_len = countof(s_D3D11Levels);

    int start = 0;
    for (; start < levels_len; start++)
    {
        if (s_D3D11Levels[start] <= max_fl)
            break;
    }
    *out = &s_D3D11Levels[start];
    return levels_len - start;
}

static DXGI_FORMAT d3d11va_map_sw_to_hw_format(enum AVPixelFormat pix_fmt)
{
    switch (pix_fmt)
    {
    case AV_PIX_FMT_YUV444P: return DXGI_FORMAT_AYUV;
    case AV_PIX_FMT_YUV444P10: return DXGI_FORMAT_Y410;
    case AV_PIX_FMT_YUV444P12:
    case AV_PIX_FMT_YUV444P14:
    case AV_PIX_FMT_YUV444P16: return DXGI_FORMAT_Y416;

    case AV_PIX_FMT_YUV422P: return DXGI_FORMAT_YUY2;
    case AV_PIX_FMT_YUV422P10: return DXGI_FORMAT_Y210;
    case AV_PIX_FMT_YUV422P12:
    case AV_PIX_FMT_YUV422P14:
    case AV_PIX_FMT_YUV422P16: return DXGI_FORMAT_Y216;

    case AV_PIX_FMT_YUV420P12:
    case AV_PIX_FMT_YUV420P14:
    case AV_PIX_FMT_YUV420P16:
    case AV_PIX_FMT_P012:
    case AV_PIX_FMT_P016: return DXGI_FORMAT_P016;
    case AV_PIX_FMT_YUV420P10:
    case AV_PIX_FMT_P010: return DXGI_FORMAT_P010;
    case AV_PIX_FMT_NV12:
    default: return DXGI_FORMAT_NV12;
    }
}

static LAVPixelFormat d3d11va_map_hw_to_lav_format(DXGI_FORMAT dxgiFormat)
{
    switch (dxgiFormat)
    {
    case DXGI_FORMAT_AYUV: return LAVPixFmt_AYUV;
    case DXGI_FORMAT_Y410: return LAVPixFmt_Y410;
    case DXGI_FORMAT_Y416: return LAVPixFmt_Y416;

    case DXGI_FORMAT_YUY2: return LAVPixFmt_YUY2;
    case DXGI_FORMAT_Y210: return LAVPixFmt_Y216;
    case DXGI_FORMAT_Y216: return LAVPixFmt_Y216;

    case DXGI_FORMAT_P010: return LAVPixFmt_P016;
    case DXGI_FORMAT_P016: return LAVPixFmt_P016;
    case DXGI_FORMAT_NV12: return LAVPixFmt_NV12;
    default: ASSERT(0); return LAVPixFmt_NV12;
    }
}

CDecD3D11::CDecD3D11(void)
    : CDecAvcodec()
{
    ZeroMemory(&m_FrameQueue, sizeof(m_FrameQueue));
}

CDecD3D11::~CDecD3D11(void)
{
    DestroyDecoder(true);

    if (m_pAllocator)
        m_pAllocator->DecoderDestruct();
    SafeRelease(&m_pAllocator);
}

STDMETHODIMP CDecD3D11::DestroyDecoder(bool bFull)
{
    for (int i = 0; i < D3D11_QUEUE_SURFACES; i++)
    {
        ReleaseFrame(&m_FrameQueue[i]);
    }

    if (m_pOutputViews)
    {
        for (int i = 0; i < m_nOutputViews; i++)
        {
            SafeRelease(&m_pOutputViews[i]);
        }
        av_freep(&m_pOutputViews);
        m_nOutputViews = 0;
    }

    SafeRelease(&m_pDecoder);
    SafeRelease(&m_pD3D11StagingTexture);
    av_buffer_unref(&m_pFramesCtx);

    CDecAvcodec::DestroyDecoder();

    if (bFull)
    {
        av_buffer_unref(&m_pDevCtx);

        if (dx.d3d11lib)
        {
            FreeLibrary(dx.d3d11lib);
            dx.d3d11lib = nullptr;
        }

        if (dx.dxgilib)
        {
            FreeLibrary(dx.dxgilib);
            dx.dxgilib = nullptr;
        }
    }

    return S_OK;
}

// ILAVDecoder
STDMETHODIMP CDecD3D11::Init()
{
    // D3D11 decoding requires Windows 8 or newer
    if (!IsWindows8OrNewer())
        return E_NOINTERFACE;

    dx.d3d11lib = LoadLibrary(L"d3d11.dll");
    if (dx.d3d11lib == nullptr)
    {
        DbgLog((LOG_TRACE, 10, L"Cannot open d3d11.dll"));
        return E_FAIL;
    }

    dx.mD3D11CreateDevice = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(dx.d3d11lib, "D3D11CreateDevice");
    if (dx.mD3D11CreateDevice == nullptr)
    {
        DbgLog((LOG_TRACE, 10, L"D3D11CreateDevice not available"));
        return E_FAIL;
    }

    dx.dxgilib = LoadLibrary(L"dxgi.dll");
    if (dx.dxgilib == nullptr)
    {
        DbgLog((LOG_TRACE, 10, L"Cannot open dxgi.dll"));
        return E_FAIL;
    }

    dx.mCreateDXGIFactory1 = (PFN_CREATE_DXGI_FACTORY1)GetProcAddress(dx.dxgilib, "CreateDXGIFactory1");
    if (dx.mCreateDXGIFactory1 == nullptr)
    {
        DbgLog((LOG_TRACE, 10, L"CreateDXGIFactory1 not available"));
        return E_FAIL;
    }

    return S_OK;
}

STDMETHODIMP CDecD3D11::Check()
{
    // attempt creating a hardware device with video support
    // by passing nullptr to the device parameter, no actual device will be created and only support will be checked

    // do probing agains level 11.1 only, to avoid complex checking logic here
    const D3D_FEATURE_LEVEL *levels = NULL;
    int level_count = s_GetD3D11FeatureLevels(D3D_FEATURE_LEVEL_11_1, &levels);

    HRESULT hr =
        dx.mD3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                                       levels, level_count, D3D11_SDK_VERSION, nullptr, nullptr, nullptr);
    return hr;
}

STDMETHODIMP CDecD3D11::InitAllocator(IMemAllocator **ppAlloc)
{
    HRESULT hr = S_OK;
    if (m_bReadBackFallback)
        return E_NOTIMPL;

    if (m_pAllocator == nullptr)
    {
        m_pAllocator = new CD3D11SurfaceAllocator(this, &hr);
        if (!m_pAllocator)
        {
            return E_OUTOFMEMORY;
        }
        if (FAILED(hr))
        {
            SAFE_DELETE(m_pAllocator);
            return hr;
        }

        // Hold a reference on the allocator
        m_pAllocator->AddRef();
    }

    // return the proper interface
    return m_pAllocator->QueryInterface(__uuidof(IMemAllocator), (void **)ppAlloc);
}

STDMETHODIMP CDecD3D11::CreateD3D11Device(UINT nDeviceIndex, ID3D11Device **ppDevice, DXGI_ADAPTER_DESC *pDesc)
{
    ID3D11Device *pD3D11Device = nullptr;

    // create DXGI factory
    IDXGIAdapter *pDXGIAdapter = nullptr;
    IDXGIFactory1 *pDXGIFactory = nullptr;
    HRESULT hr = dx.mCreateDXGIFactory1(IID_IDXGIFactory1, (void **)&pDXGIFactory);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 10, L"-> DXGIFactory creation failed"));
        goto fail;
    }

    // find the adapter
enum_adapter:
    hr = pDXGIFactory->EnumAdapters(nDeviceIndex, &pDXGIAdapter);
    if (FAILED(hr))
    {
        if (nDeviceIndex != 0)
        {
            DbgLog(
                (LOG_ERROR, 10, L"-> Requested DXGI device %d not available, falling back to default", nDeviceIndex));
            nDeviceIndex = 0;
            hr = pDXGIFactory->EnumAdapters(0, &pDXGIAdapter);
        }

        if (FAILED(hr))
        {
            DbgLog((LOG_ERROR, 10, L"-> Failed to enumerate a valid DXGI device"));
            goto fail;
        }
    }

    // Create a device with video support, and BGRA support for Direct2D interoperability (drawing UI, etc)
    UINT nCreationFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL max_level = D3D_FEATURE_LEVEL_12_1;
    D3D_FEATURE_LEVEL d3dFeatureLevel;

    do
    {
        const D3D_FEATURE_LEVEL *levels = NULL;
        int level_count = s_GetD3D11FeatureLevels(max_level, &levels);
        hr = dx.mD3D11CreateDevice(pDXGIAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, nCreationFlags, levels, level_count,
                                   D3D11_SDK_VERSION, &pD3D11Device, &d3dFeatureLevel, nullptr);

        if (SUCCEEDED(hr))
            break;

        // 12.0+ devices fail on Windows 8.1, try without it
        if (max_level >= D3D_FEATURE_LEVEL_12_0)
        {
            max_level = D3D_FEATURE_LEVEL_11_1;
            continue;
        }

        break;
    } while (true);

    if (FAILED(hr))
    {
        if (nDeviceIndex != 0)
        {
            DbgLog((
                LOG_ERROR, 10,
                L"-> Failed to create a D3D11 device with video support on requested device %d, re-trying with default",
                nDeviceIndex));
            SafeRelease(&pDXGIAdapter);
            nDeviceIndex = 0;
            goto enum_adapter;
        }

        DbgLog((LOG_ERROR, 10, L"-> Failed to create a D3D11 device with video support"));
        goto fail;
    }

    DbgLog((LOG_TRACE, 10, L"-> Created D3D11 device with feature level %d.%d", d3dFeatureLevel >> 12,
            (d3dFeatureLevel >> 8) & 0xF));

    // enable multithreaded protection
    ID3D10Multithread *pMultithread = nullptr;
    hr = pD3D11Device->QueryInterface(&pMultithread);
    if (SUCCEEDED(hr))
    {
        pMultithread->SetMultithreadProtected(TRUE);
        SafeRelease(&pMultithread);
    }

    // store adapter info
    if (pDesc)
    {
        ZeroMemory(pDesc, sizeof(*pDesc));
        pDXGIAdapter->GetDesc(pDesc);
    }

    // return device
    *ppDevice = pD3D11Device;

fail:
    SafeRelease(&pDXGIFactory);
    SafeRelease(&pDXGIAdapter);
    return hr;
}

STDMETHODIMP CDecD3D11::PostConnect(IPin *pPin)
{
    DbgLog((LOG_TRACE, 10, L"CDecD3D11::PostConnect()"));
    HRESULT hr = S_OK;

    ID3D11DecoderConfiguration *pD3D11DecoderConfiguration = nullptr;
    hr = pPin->QueryInterface(&pD3D11DecoderConfiguration);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 10, L"-> ID3D11DecoderConfiguration not available, using fallback mode"));
    }

    // Release old D3D resources, we're about to re-init
    m_pCallback->ReleaseAllDXVAResources();

    // free the decoder to force a re-init down the line
    SafeRelease(&m_pDecoder);

    // and the old device
    av_buffer_unref(&m_pDevCtx);

    // device id
    UINT nDevice = m_pSettings->GetHWAccelDeviceIndex(HWAccel_D3D11, nullptr);

    // in automatic mode use the device the renderer gives us
    if (nDevice == LAVHWACCEL_DEVICE_DEFAULT && pD3D11DecoderConfiguration)
    {
        nDevice = pD3D11DecoderConfiguration->GetD3D11AdapterIndex();
    }
    else
    {
        // if a device is specified manually, fallback to copy-back and use the selected device
        SafeRelease(&pD3D11DecoderConfiguration);

        // use the configured device
        if (nDevice == LAVHWACCEL_DEVICE_DEFAULT)
            nDevice = 0;
    }

    // create the device
    ID3D11Device *pD3D11Device = nullptr;
    hr = CreateD3D11Device(nDevice, &pD3D11Device, &m_AdapterDesc);
    if (FAILED(hr))
    {
        goto fail;
    }

    // allocate and fill device context
    m_pDevCtx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    AVD3D11VADeviceContext *pDeviceContext = (AVD3D11VADeviceContext *)((AVHWDeviceContext *)m_pDevCtx->data)->hwctx;
    pDeviceContext->device = pD3D11Device;

    // finalize the context
    int ret = av_hwdevice_ctx_init(m_pDevCtx);
    if (ret < 0)
    {
        av_buffer_unref(&m_pDevCtx);
        goto fail;
    }

    // enable multithreaded protection
    ID3D10Multithread *pMultithread = nullptr;
    hr = pDeviceContext->device_context->QueryInterface(&pMultithread);
    if (SUCCEEDED(hr))
    {
        pMultithread->SetMultithreadProtected(TRUE);
        SafeRelease(&pMultithread);
    }

    // check if the connection supports native mode
    if (pD3D11DecoderConfiguration)
    {
        CMediaType mt = m_pCallback->GetOutputMediaType();
        if ((m_SurfaceFormat == DXGI_FORMAT_NV12 && mt.subtype != MEDIASUBTYPE_NV12) ||
            (m_SurfaceFormat == DXGI_FORMAT_P010 && mt.subtype != MEDIASUBTYPE_P010) ||
            (m_SurfaceFormat == DXGI_FORMAT_P016 && mt.subtype != MEDIASUBTYPE_P016) ||
            (m_SurfaceFormat == DXGI_FORMAT_AYUV && mt.subtype != MEDIASUBTYPE_AYUV) ||
            (m_SurfaceFormat == DXGI_FORMAT_Y410 && mt.subtype != MEDIASUBTYPE_Y410) ||
            (m_SurfaceFormat == DXGI_FORMAT_Y416 && mt.subtype != MEDIASUBTYPE_Y416) ||
            (m_SurfaceFormat == DXGI_FORMAT_YUY2 && mt.subtype != MEDIASUBTYPE_YUY2) ||
            (m_SurfaceFormat == DXGI_FORMAT_Y210 && mt.subtype != MEDIASUBTYPE_Y210) ||
            (m_SurfaceFormat == DXGI_FORMAT_Y216 && mt.subtype != MEDIASUBTYPE_Y216))
        {
            DbgLog((LOG_ERROR, 10, L"-> Connection is not the appropriate pixel format for D3D11 Native"));

            SafeRelease(&pD3D11DecoderConfiguration);
        }
    }

    // verify hardware support
    {
        int level = 0;
        if (m_pAVCtx->codec_id == AV_CODEC_ID_HEVC)
        {
            int64_t value = 0;
            if (av_opt_get_int(m_pAVCtx->priv_data, "rext_profile", 0, &value) >= 0)
                level = value;
        }

        GUID guidConversion = GUID_NULL;
        hr = FindVideoServiceConversion(m_pAVCtx->codec_id, m_pAVCtx->profile, level, m_SurfaceFormat, &guidConversion);
        if (FAILED(hr))
        {
            goto fail;
        }

        // get decoder configuration
        D3D11_VIDEO_DECODER_DESC desc = {0};
        desc.Guid = guidConversion;
        desc.OutputFormat = m_SurfaceFormat;
        desc.SampleWidth = m_pAVCtx->coded_width;
        desc.SampleHeight = m_pAVCtx->coded_height;

        D3D11_VIDEO_DECODER_CONFIG decoder_config = {0};
        hr = FindDecoderConfiguration(&desc, &decoder_config);
        if (FAILED(hr))
        {
            goto fail;
        }

        // test creating a texture
        D3D11_TEXTURE2D_DESC texDesc = {0};
        texDesc.Width = m_pAVCtx->coded_width;
        texDesc.Height = m_pAVCtx->coded_height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = GetBufferCount();
        texDesc.Format = m_SurfaceFormat;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;
        texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

        hr = pD3D11Device->CreateTexture2D(&texDesc, nullptr, nullptr);
        if (FAILED(hr))
        {
            goto fail;
        }
    }

    // Notice the connected pin that we're sending D3D11 textures
    if (pD3D11DecoderConfiguration)
    {
        hr = pD3D11DecoderConfiguration->ActivateD3D11Decoding(pDeviceContext->device, pDeviceContext->device_context,
                                                               pDeviceContext->lock_ctx, 0);
        SafeRelease(&pD3D11DecoderConfiguration);

        m_bReadBackFallback = FAILED(hr);
    }
    else
    {
        m_bReadBackFallback = true;
    }

    return S_OK;

fail:
    SafeRelease(&pD3D11DecoderConfiguration);
    return E_FAIL;
}

STDMETHODIMP CDecD3D11::BreakConnect()
{
    if (m_bReadBackFallback)
        return S_FALSE;

    // release any resources held by the core
    m_pCallback->ReleaseAllDXVAResources();

    // flush all buffers out of the decoder to ensure the allocator can be properly de-allocated
    if (m_pAVCtx && avcodec_is_open(m_pAVCtx))
        avcodec_flush_buffers(m_pAVCtx);

    return S_OK;
}

STDMETHODIMP CDecD3D11::InitDecoder(AVCodecID codec, const CMediaType *pmt, const MediaSideDataFFMpeg *pSideData)
{
    HRESULT hr = S_OK;
    DbgLog((LOG_TRACE, 10, L"CDecD3D11::InitDecoder(): Initializing D3D11 decoder"));

    // Destroy old decoder
    DestroyDecoder(false);

    // reset stream compatibility
    m_bFailHWDecode = false;

    m_DisplayDelay = D3D11_QUEUE_SURFACES;

    // Reduce display delay for DVD decoding for lower decode latency
    if (m_pCallback->GetDecodeFlags() & LAV_VIDEO_DEC_FLAG_DVD)
        m_DisplayDelay /= 2;

    // Initialize ffmpeg
    hr = CDecAvcodec::InitDecoder(codec, pmt, pSideData);
    if (FAILED(hr))
        return hr;

    if (check_dxva_codec_profile(m_pAVCtx, AV_PIX_FMT_D3D11))
    {
        DbgLog((LOG_TRACE, 10, L"-> Incompatible profile detected, falling back to software decoding"));
        return E_FAIL;
    }

    // initialize surface format to ensure the default media type is set properly
    m_SurfaceFormat = d3d11va_map_sw_to_hw_format(m_pAVCtx->sw_pix_fmt);
    m_dwSurfaceWidth = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_width);
    m_dwSurfaceHeight = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_height);

    return S_OK;
}

HRESULT CDecD3D11::AdditionaDecoderInit()
{
    AVD3D11VAContext *ctx = av_d3d11va_alloc_context();

    if (m_pDecoder)
    {
        FillHWContext(ctx);
    }

    m_pAVCtx->thread_count = 1;
    m_pAVCtx->thread_type = 0;
    m_pAVCtx->hwaccel_context = ctx;
    m_pAVCtx->get_format = get_d3d11_format;
    m_pAVCtx->get_buffer2 = get_d3d11_buffer;
    m_pAVCtx->opaque = this;

    m_pAVCtx->slice_flags |= SLICE_FLAG_ALLOW_FIELD;

    // disable error concealment in hwaccel mode, it doesn't work either way
    m_pAVCtx->error_concealment = 0;
    av_opt_set_int(m_pAVCtx, "enable_er", 0, AV_OPT_SEARCH_CHILDREN);

    return S_OK;
}

STDMETHODIMP CDecD3D11::FillHWContext(AVD3D11VAContext *ctx)
{
    AVD3D11VADeviceContext *pDeviceContext = (AVD3D11VADeviceContext *)((AVHWDeviceContext *)m_pDevCtx->data)->hwctx;

    ctx->decoder = m_pDecoder;
    ctx->video_context = pDeviceContext->video_context;
    ctx->cfg = &m_DecoderConfig;
    ctx->surface_count = m_nOutputViews;
    ctx->surface = m_pOutputViews;

    ctx->context_mutex = pDeviceContext->lock_ctx;

    ctx->workaround = 0;

    if (m_AdapterDesc.VendorId == VEND_ID_NVIDIA)
        ctx->workaround = FF_DXVA2_WORKAROUND_NVIDIA_HEVC_420P12;

    return S_OK;
}

STDMETHODIMP_(long) CDecD3D11::GetBufferCount(long *pMaxBuffers)
{
    long buffers = 0;

    // Native decoding should use 16 buffers to enable seamless codec changes
    if (!m_bReadBackFallback)
        buffers = 16;
    else
    {
        // Buffers based on max ref frames
        if (m_nCodecId == AV_CODEC_ID_H264 || m_nCodecId == AV_CODEC_ID_HEVC)
            buffers = 16;
        else if (m_nCodecId == AV_CODEC_ID_VP9 || m_nCodecId == AV_CODEC_ID_AV1)
            buffers = 8;
        else
            buffers = 2;
    }

    // 4 extra buffers for handling and safety
    buffers += 4;

    if (m_bReadBackFallback)
    {
        buffers += m_DisplayDelay;
    }

    if (m_pCallback->GetDecodeFlags() & LAV_VIDEO_DEC_FLAG_DVD)
    {
        buffers += 4;
    }

    if (pMaxBuffers)
    {
        // cap at 127, because it needs to fit into the 7-bit DXVA structs
        *pMaxBuffers = 127;

        // VC-1 and VP9 decoding has stricter requirements (decoding flickers otherwise)
        if (m_nCodecId == AV_CODEC_ID_VC1 || m_nCodecId == AV_CODEC_ID_VP9 || m_nCodecId == AV_CODEC_ID_AV1)
            *pMaxBuffers = 32;
    }

    return buffers;
}

STDMETHODIMP CDecD3D11::FlushDisplayQueue(BOOL bDeliver)
{
    for (int i = 0; i < m_DisplayDelay; ++i)
    {
        if (m_FrameQueue[m_FrameQueuePosition])
        {
            if (bDeliver)
            {
                DeliverD3D11Frame(m_FrameQueue[m_FrameQueuePosition]);
                m_FrameQueue[m_FrameQueuePosition] = nullptr;
            }
            else
            {
                ReleaseFrame(&m_FrameQueue[m_FrameQueuePosition]);
            }
        }
        m_FrameQueuePosition = (m_FrameQueuePosition + 1) % m_DisplayDelay;
    }

    return S_OK;
}

STDMETHODIMP CDecD3D11::Flush()
{
    CDecAvcodec::Flush();

    // Flush display queue
    FlushDisplayQueue(FALSE);

    return S_OK;
}

STDMETHODIMP CDecD3D11::EndOfStream()
{
    CDecAvcodec::EndOfStream();

    // Flush display queue
    FlushDisplayQueue(TRUE);

    return S_OK;
}

HRESULT CDecD3D11::PostDecode()
{
    if (m_bFailHWDecode)
    {
        DbgLog((LOG_TRACE, 10, L"::PostDecode(): HW Decoder failed, falling back to software decoding"));
        return E_FAIL;
    }
    return S_OK;
}

enum AVPixelFormat CDecD3D11::get_d3d11_format(struct AVCodecContext *s, const enum AVPixelFormat *pix_fmts)
{
    CDecD3D11 *pDec = (CDecD3D11 *)s->opaque;
    const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++)
    {
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*p);

        if (!desc || !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
            break;

        if (*p == AV_PIX_FMT_D3D11)
        {
            HRESULT hr = pDec->ReInitD3D11Decoder(s);
            if (FAILED(hr))
            {
                pDec->m_bFailHWDecode = TRUE;
                continue;
            }
            else
            {
                break;
            }
        }
    }
    return *p;
}

int CDecD3D11::get_d3d11_buffer(struct AVCodecContext *c, AVFrame *frame, int flags)
{
    CDecD3D11 *pDec = (CDecD3D11 *)c->opaque;
    HRESULT hr = S_OK;

    if (frame->format != AV_PIX_FMT_D3D11)
    {
        DbgLog((LOG_ERROR, 10, L"D3D11 buffer request, but not D3D11 pixfmt"));
        pDec->m_bFailHWDecode = TRUE;
        return -1;
    }

    hr = pDec->ReInitD3D11Decoder(c);
    if (FAILED(hr))
    {
        pDec->m_bFailHWDecode = TRUE;
        return -1;
    }

    if (pDec->m_bReadBackFallback && pDec->m_pFramesCtx)
    {
        int ret = av_hwframe_get_buffer(pDec->m_pFramesCtx, frame, 0);
        frame->width = c->coded_width;
        frame->height = c->coded_height;
        return ret;
    }
    else if (pDec->m_bReadBackFallback == false && pDec->m_pAllocator)
    {
        IMediaSample *pSample = nullptr;
        hr = pDec->m_pAllocator->GetBuffer(&pSample, nullptr, nullptr, 0);
        if (SUCCEEDED(hr))
        {
            CD3D11MediaSample *pD3D11Sample = dynamic_cast<CD3D11MediaSample *>(pSample);

            // fill the frame from the sample, including a reference to the sample
            if (FAILED(pD3D11Sample->GetAVFrameBuffer(frame)))
            {
                pD3D11Sample->Release();
                return -1;
            }

            frame->width = c->coded_width;
            frame->height = c->coded_height;

            // the frame holds the sample now, can release the direct interface
            pD3D11Sample->Release();
            return 0;
        }
    }

    return -1;
}

STDMETHODIMP CDecD3D11::ReInitD3D11Decoder(AVCodecContext *c)
{
    HRESULT hr = S_OK;

    // Don't allow decoder creation during first init
    if (m_bInInit)
        return S_FALSE;

    // sanity check that we have a device
    if (m_pDevCtx == nullptr)
        return E_FAIL;

    // we need an allocator at this point
    if (m_bReadBackFallback == false && m_pAllocator == nullptr)
        return E_FAIL;

    DXGI_FORMAT surfaceFormatToTest = d3d11va_map_sw_to_hw_format(c->sw_pix_fmt);
    if (surfaceFormatToTest == DXGI_FORMAT_P016 && m_bP016ToP010Fallback)
        surfaceFormatToTest = DXGI_FORMAT_P010;

    if (m_pDecoder == nullptr || m_dwSurfaceWidth != dxva_align_dimensions(c->codec_id, c->coded_width) ||
        m_dwSurfaceHeight != dxva_align_dimensions(c->codec_id, c->coded_height) ||
        m_SurfaceFormat != surfaceFormatToTest)
    {
        AVD3D11VADeviceContext *pDeviceContext =
            (AVD3D11VADeviceContext *)((AVHWDeviceContext *)m_pDevCtx->data)->hwctx;
        DbgLog((LOG_TRACE, 10, L"No D3D11 Decoder or image dimensions changed -> Re-Allocating resources"));

        // if we're not in readback mode, we need to flush all the frames
        if (m_bReadBackFallback == false)
            if (m_pDecoder)
                avcodec_flush_buffers(c);
            else
                FlushDisplayQueue(TRUE);

        pDeviceContext->lock(pDeviceContext->lock_ctx);
        hr = CreateD3D11Decoder();
        pDeviceContext->unlock(pDeviceContext->lock_ctx);
        if (FAILED(hr))
            return hr;

        // Update the frames context in the allocator
        if (m_bReadBackFallback == false)
        {
            // decommit the allocator
            m_pAllocator->Decommit();

            // verify we were able to decommit all its resources
            if (m_pAllocator->DecommitInProgress())
            {
                DbgLog((LOG_TRACE, 10, L"WARNING! D3D11 Allocator is still busy, trying to flush downstream"));
                m_pCallback->ReleaseAllDXVAResources();
                m_pCallback->GetOutputPin()->GetConnected()->BeginFlush();
                m_pCallback->GetOutputPin()->GetConnected()->EndFlush();
                if (m_pAllocator->DecommitInProgress())
                {
                    DbgLog(
                        (LOG_TRACE, 10, L"WARNING! Flush had no effect, decommit of the allocator still not complete"));
                    m_pAllocator->ForceDecommit();
                }
                else
                {
                    DbgLog((LOG_TRACE, 10, L"Flush was successful, decommit completed!"));
                }
            }

            // re-commit it to update its frame reference
            m_pAllocator->Commit();
        }
    }

    return S_OK;
}

STDMETHODIMP CDecD3D11::FindVideoServiceConversion(AVCodecID codec, int profile, int level, DXGI_FORMAT &surface_format,
                                                   GUID *input)
{
    AVD3D11VADeviceContext *pDeviceContext = (AVD3D11VADeviceContext *)((AVHWDeviceContext *)m_pDevCtx->data)->hwctx;
    HRESULT hr = S_OK;

    UINT nProfiles = pDeviceContext->video_device->GetVideoDecoderProfileCount();
    GUID *guid_list = (GUID *)av_malloc_array(nProfiles, sizeof(*guid_list));

    m_bP016ToP010Fallback = false;

    DbgLog((LOG_TRACE, 10, L"-> Enumerating supported D3D11 modes (count: %d)", nProfiles));
    for (UINT i = 0; i < nProfiles; i++)
    {
        hr = pDeviceContext->video_device->GetVideoDecoderProfile(i, &guid_list[i]);
        if (FAILED(hr))
        {
            DbgLog((LOG_ERROR, 10, L"Error retrieving decoder profile"));
            av_free(guid_list);
            return hr;
        }

#ifdef DEBUG
        const dxva_mode_t *mode = get_dxva_mode_from_guid(&guid_list[i]);
        if (mode)
        {
            DbgLog((LOG_TRACE, 10, L"  -> %S", mode->name));
        }
        else
        {
            DbgLog((LOG_TRACE, 10, L"  -> Unknown GUID (%s)", WStringFromGUID(guid_list[i]).c_str()));
        }
#endif
    }

    /* Iterate over our priority list */
    for (unsigned i = 0; dxva_modes[i].name; i++)
    {
        const dxva_mode_t *mode = &dxva_modes[i];
        if (!check_dxva_mode_compatibility(mode, codec, profile, level, (surface_format == DXGI_FORMAT_NV12)))
            continue;

        BOOL supported = FALSE;
        for (UINT g = 0; !supported && g < nProfiles; g++)
        {
            supported = IsEqualGUID(*mode->guid, guid_list[g]);
        }
        if (!supported)
            continue;

        DbgLog((LOG_TRACE, 10, L"-> Trying to use '%S'", mode->name));
        hr = pDeviceContext->video_device->CheckVideoDecoderFormat(mode->guid, surface_format, &supported);
        // some high bitdepth decoders only accept P010, despite the memory layout otherwise being identical
        if (SUCCEEDED(hr) && !supported && surface_format == DXGI_FORMAT_P016)
        {
            hr = pDeviceContext->video_device->CheckVideoDecoderFormat(mode->guid, DXGI_FORMAT_P010, &supported);

            if (SUCCEEDED(hr) && supported)
            {
                surface_format = DXGI_FORMAT_P010;
                m_bP016ToP010Fallback = true;
            }
        }
        if (SUCCEEDED(hr) && supported)
        {
            *input = *mode->guid;

            av_free(guid_list);
            return S_OK;
        }
    }

    av_free(guid_list);
    return E_FAIL;
}

STDMETHODIMP CDecD3D11::FindDecoderConfiguration(const D3D11_VIDEO_DECODER_DESC *desc,
                                                 D3D11_VIDEO_DECODER_CONFIG *pConfig)
{
    AVD3D11VADeviceContext *pDeviceContext = (AVD3D11VADeviceContext *)((AVHWDeviceContext *)m_pDevCtx->data)->hwctx;
    HRESULT hr = S_OK;

    UINT nConfig = 0;
    hr = pDeviceContext->video_device->GetVideoDecoderConfigCount(desc, &nConfig);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 10, L"Unable to retrieve decoder configuration count"));
        return E_FAIL;
    }

    int best_score = -1;
    D3D11_VIDEO_DECODER_CONFIG best_config;
    for (UINT i = 0; i < nConfig; i++)
    {
        D3D11_VIDEO_DECODER_CONFIG config = {0};
        hr = pDeviceContext->video_device->GetVideoDecoderConfig(desc, i, &config);
        if (FAILED(hr))
            continue;

        DbgLog((LOG_ERROR, 10, "-> Configuration Record %d: ConfigBitstreamRaw = %d", i, config.ConfigBitstreamRaw));

        int score;
        if (config.ConfigBitstreamRaw == 1)
            score = 1;
        else if (m_pAVCtx->codec_id == AV_CODEC_ID_H264 && config.ConfigBitstreamRaw == 2)
            score = 2;
        else if (m_pAVCtx->codec_id == AV_CODEC_ID_VP9) // hack for broken AMD drivers
            score = 0;
        else
            continue;
        if (IsEqualGUID(config.guidConfigBitstreamEncryption, DXVA2_NoEncrypt))
            score += 16;
        if (score > best_score)
        {
            best_score = score;
            best_config = config;
        }
    }

    if (best_score < 0)
    {
        DbgLog((LOG_TRACE, 10, L"-> No matching configuration available"));
        return E_FAIL;
    }

    *pConfig = best_config;
    return S_OK;
}

STDMETHODIMP CDecD3D11::CreateD3D11Decoder()
{
    HRESULT hr = S_OK;
    AVD3D11VADeviceContext *pDeviceContext = (AVD3D11VADeviceContext *)((AVHWDeviceContext *)m_pDevCtx->data)->hwctx;

    // release the old decoder, it needs to be re-created
    SafeRelease(&m_pDecoder);

    // find a decoder configuration
    GUID profileGUID = GUID_NULL;
    DXGI_FORMAT surface_format = d3d11va_map_sw_to_hw_format(m_pAVCtx->sw_pix_fmt);

    // codec sub-level
    int level = 0;
    if (m_pAVCtx->codec_id == AV_CODEC_ID_HEVC)
    {
        int64_t value = 0;
        if (av_opt_get_int(m_pAVCtx->priv_data, "rext_profile", 0, &value) >= 0)
            level = value;
    }

    hr = FindVideoServiceConversion(m_pAVCtx->codec_id, m_pAVCtx->profile, level, surface_format, &profileGUID);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 10, L"-> No video service profile found"));
        return hr;
    }

    // get decoder configuration
    D3D11_VIDEO_DECODER_DESC desc = {0};
    desc.Guid = profileGUID;
    desc.OutputFormat = surface_format;
    desc.SampleWidth = m_pAVCtx->coded_width;
    desc.SampleHeight = m_pAVCtx->coded_height;

    D3D11_VIDEO_DECODER_CONFIG decoder_config = {0};
    hr = FindDecoderConfiguration(&desc, &decoder_config);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 10, L"-> No valid video decoder configuration found"));
        return hr;
    }

    m_DecoderConfig = decoder_config;

    // update surface properties
    m_dwSurfaceWidth = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_width);
    m_dwSurfaceHeight = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_height);
    m_SurfaceFormat = surface_format;

    if (m_bReadBackFallback == false && m_pAllocator)
    {
        ALLOCATOR_PROPERTIES properties;
        hr = m_pAllocator->GetProperties(&properties);
        if (FAILED(hr))
            return hr;

        m_dwSurfaceCount = properties.cBuffers;
    }
    else
    {
        m_dwSurfaceCount = GetBufferCount();
    }

    // allocate a new frames context for the dimensions and format
    hr = AllocateFramesContext(m_dwSurfaceWidth, m_dwSurfaceHeight, m_SurfaceFormat, m_dwSurfaceCount,
                               &m_pFramesCtx);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 10, L"-> Error allocating frames context"));
        return hr;
    }

    // release any old output views and allocate memory for the new ones
    if (m_pOutputViews)
    {
        for (int i = 0; i < m_nOutputViews; i++)
        {
            SafeRelease(&m_pOutputViews[i]);
        }
        av_freep(&m_pOutputViews);
    }

    m_pOutputViews = (ID3D11VideoDecoderOutputView **)av_calloc(m_dwSurfaceCount, sizeof(*m_pOutputViews));
    m_nOutputViews = m_dwSurfaceCount;

    // allocate output views for the frames
    AVD3D11VAFramesContext *pFramesContext = (AVD3D11VAFramesContext *)((AVHWFramesContext *)m_pFramesCtx->data)->hwctx;
    for (int i = 0; i < m_nOutputViews; i++)
    {
        D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc = {0};
        viewDesc.DecodeProfile = profileGUID;
        viewDesc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
        viewDesc.Texture2D.ArraySlice = i;

        hr = pDeviceContext->video_device->CreateVideoDecoderOutputView(pFramesContext->texture, &viewDesc,
                                                                        &m_pOutputViews[i]);
        if (FAILED(hr))
        {
            DbgLog((LOG_ERROR, 10, L"-> Failed to create video decoder output views"));
            return E_FAIL;
        }
    }

    // flush textures to black
    if (surface_format == DXGI_FORMAT_NV12 || surface_format == DXGI_FORMAT_P010 || surface_format == DXGI_FORMAT_P016)
    {
        D3D11_FEATURE_DATA_D3D11_OPTIONS d3d11Options{};
        pDeviceContext->device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &d3d11Options,
                                                    sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS));

        // XXX: The ClearView path does not function properly on Intel GPUs for P010
        // Investigation has shown that contrary to the documentation, Intel transforms the color information,
        // instead of treating the values like integral floats, as required.
        ID3D11DeviceContext1 *pDeviceContext1 = nullptr;
        if (m_AdapterDesc.VendorId != VEND_ID_INTEL && d3d11Options.ClearView &&
            SUCCEEDED(hr = pDeviceContext->device_context->QueryInterface(&pDeviceContext1)))
        {
            for (int i = 0; i < m_nOutputViews; i++)
            {
                // clear the Luma channel to zero and Chroma channel to half
                // for both P010/P016, the full 16-bit range value needs to be used due to the nature of their memory layout
                float fChromaBlack = (surface_format == DXGI_FORMAT_NV12) ? 128.0f : 32768.0f;
                const FLOAT ClearYUV[4] = {0.0f, fChromaBlack, fChromaBlack, 0.0f};
                pDeviceContext1->ClearView(m_pOutputViews[i], ClearYUV, nullptr, 0);
            }
            SafeRelease(&pDeviceContext1);
        }
        else
        {
            D3D11_TEXTURE2D_DESC FlushTexDesc{};
            FlushTexDesc.Width = m_dwSurfaceWidth;
            FlushTexDesc.Height = m_dwSurfaceHeight;
            FlushTexDesc.MipLevels = 1;
            FlushTexDesc.ArraySize = 1;
            FlushTexDesc.Format = surface_format;
            FlushTexDesc.SampleDesc.Count = 1;
            FlushTexDesc.Usage = D3D11_USAGE_DEFAULT;
            FlushTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
            FlushTexDesc.CPUAccessFlags = 0;
            FlushTexDesc.MiscFlags = 0;

            ID3D11Texture2D *pFlushTexture = NULL;
            if (SUCCEEDED(pDeviceContext->device->CreateTexture2D(&FlushTexDesc, NULL, &pFlushTexture)))
            {
                D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Format = surface_format;

                ID3D11RenderTargetView *pRTV = nullptr;

                // clear the Luma channel to zero
                rtvDesc.Format = (surface_format == DXGI_FORMAT_NV12) ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R16_UNORM;
                if (SUCCEEDED(hr = pDeviceContext->device->CreateRenderTargetView(pFlushTexture, &rtvDesc, &pRTV)))
                {
                    const FLOAT ClearYUV[4] = { 0.0f };
                    pDeviceContext->device_context->ClearRenderTargetView(pRTV, ClearYUV);
                    SafeRelease(&pRTV);
                }

                // clear the Chroma channel to half
                rtvDesc.Format = (surface_format == DXGI_FORMAT_NV12) ? DXGI_FORMAT_R8G8_UNORM : DXGI_FORMAT_R16G16_UNORM;
                if (SUCCEEDED(hr = pDeviceContext->device->CreateRenderTargetView(pFlushTexture, &rtvDesc, &pRTV)))
                {
                    const FLOAT ClearYUV[4] = { 0.5f, 0.5f, 0.0f, 0.0f };
                    pDeviceContext->device_context->ClearRenderTargetView(pRTV, ClearYUV);
                    SafeRelease(&pRTV);
                }

                // update all surfaces with the flush color
                for (unsigned i = 0; i < m_dwSurfaceCount; i++)
                {
                    pDeviceContext->device_context->CopySubresourceRegion(pFramesContext->texture, i, 0, 0, 0,
                                                                          pFlushTexture, 0, NULL);
                }
                SafeRelease(&pFlushTexture);
            }
        }

        // flush all pending work
        pDeviceContext->device_context->Flush();
    }

    // create the decoder
    hr = pDeviceContext->video_device->CreateVideoDecoder(&desc, &decoder_config, &m_pDecoder);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 10, L"-> Failed to create video decoder object"));
        return E_FAIL;
    }

    FillHWContext((AVD3D11VAContext *)m_pAVCtx->hwaccel_context);

    return S_OK;
}

static AVPixelFormat s_GetAVD3D11PixelFormat(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_NV12: return AV_PIX_FMT_NV12;
    case DXGI_FORMAT_P010: return AV_PIX_FMT_P010;
    case DXGI_FORMAT_P016: return AV_PIX_FMT_P016;

    case DXGI_FORMAT_YUY2: return AV_PIX_FMT_YUYV422;
    case DXGI_FORMAT_Y210: return AV_PIX_FMT_Y210;
    case DXGI_FORMAT_Y216: return AV_PIX_FMT_Y216;

    case DXGI_FORMAT_AYUV: return AV_PIX_FMT_VUYX;
    case DXGI_FORMAT_Y410: return AV_PIX_FMT_XV30;
    case DXGI_FORMAT_Y416: return AV_PIX_FMT_XV48;
    }

    ASSERT(0);
    return AV_PIX_FMT_NV12;
}

STDMETHODIMP CDecD3D11::AllocateFramesContext(int width, int height, DXGI_FORMAT format, int nSurfaces,
                                              AVBufferRef **ppFramesCtx)
{
    ASSERT(m_pAVCtx);
    ASSERT(m_pDevCtx);
    ASSERT(ppFramesCtx);

    // unref any old buffer
    av_buffer_unref(ppFramesCtx);
    SafeRelease(&m_pD3D11StagingTexture);

    // allocate a new frames context for the device context
    *ppFramesCtx = av_hwframe_ctx_alloc(m_pDevCtx);
    if (*ppFramesCtx == nullptr)
        return E_OUTOFMEMORY;

    AVHWFramesContext *pFrames = (AVHWFramesContext *)(*ppFramesCtx)->data;
    pFrames->format = AV_PIX_FMT_D3D11;
    pFrames->sw_format = s_GetAVD3D11PixelFormat(format);
    pFrames->width = width;
    pFrames->height = height;
    pFrames->initial_pool_size = nSurfaces;

    AVD3D11VAFramesContext *pFramesHWContext = (AVD3D11VAFramesContext *)pFrames->hwctx;
    pFramesHWContext->BindFlags |= D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;
    pFramesHWContext->MiscFlags |= D3D11_RESOURCE_MISC_SHARED;

    int ret = av_hwframe_ctx_init(*ppFramesCtx);
    if (ret < 0)
    {
        av_buffer_unref(ppFramesCtx);
        return E_FAIL;
    }

    return S_OK;
}

HRESULT CDecD3D11::HandleDXVA2Frame(LAVFrame *pFrame)
{
    if (pFrame->flags & LAV_FRAME_FLAG_FLUSH)
    {
        if (m_bReadBackFallback)
        {
            FlushDisplayQueue(TRUE);
        }
        Deliver(pFrame);
        return S_OK;
    }

    ASSERT(pFrame->format == LAVPixFmt_D3D11);

    if (m_bReadBackFallback == false || m_DisplayDelay == 0)
    {
        DeliverD3D11Frame(pFrame);
    }
    else
    {
        LAVFrame *pQueuedFrame = m_FrameQueue[m_FrameQueuePosition];
        m_FrameQueue[m_FrameQueuePosition] = pFrame;

        m_FrameQueuePosition = (m_FrameQueuePosition + 1) % m_DisplayDelay;

        if (pQueuedFrame)
        {
            DeliverD3D11Frame(pQueuedFrame);
        }
    }

    return S_OK;
}

HRESULT CDecD3D11::DeliverD3D11Frame(LAVFrame *pFrame)
{
    if (m_bReadBackFallback)
    {
        if (m_bDirect)
            DeliverD3D11ReadbackDirect(pFrame);
        else
            DeliverD3D11Readback(pFrame);
    }
    else
    {
        AVFrame *pAVFrame = (AVFrame *)pFrame->priv_data;
        pFrame->data[0] = pAVFrame->data[3];
        pFrame->data[1] = pFrame->data[2] = pFrame->data[3] = nullptr;

        GetPixelFormat(&pFrame->format, &pFrame->bpp);

        Deliver(pFrame);
    }

    return S_OK;
}

HRESULT CDecD3D11::DeliverD3D11Readback(LAVFrame *pFrame)
{
    AVFrame *src = (AVFrame *)pFrame->priv_data;
    AVFrame *dst = av_frame_alloc();

    int ret = av_hwframe_transfer_data(dst, src, 0);
    if (ret < 0)
    {
        ReleaseFrame(&pFrame);
        av_frame_free(&dst);
        return E_FAIL;
    }

    // free the source frame
    av_frame_free(&src);

    // and store the dst frame in LAVFrame
    pFrame->priv_data = dst;
    GetPixelFormat(&pFrame->format, &pFrame->bpp);

    ASSERT(getFFPixelFormatFromLAV(pFrame->format, pFrame->bpp) == dst->format);

    for (int i = 0; i < 4; i++)
    {
        pFrame->data[i] = dst->data[i];
        pFrame->stride[i] = dst->linesize[i];
    }

    return Deliver(pFrame);
}

struct D3D11DirectPrivate
{
    AVBufferRef *pDeviceContex;
    ID3D11Texture2D *pStagingTexture;
};

static bool d3d11_direct_lock(LAVFrame *pFrame, LAVDirectBuffer *pBuffer)
{
    D3D11DirectPrivate *c = (D3D11DirectPrivate *)pFrame->priv_data;
    AVD3D11VADeviceContext *pDeviceContext =
        (AVD3D11VADeviceContext *)((AVHWDeviceContext *)c->pDeviceContex->data)->hwctx;
    D3D11_TEXTURE2D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE map;

    ASSERT(pFrame && pBuffer);

    // lock the device context
    pDeviceContext->lock(pDeviceContext->lock_ctx);

    c->pStagingTexture->GetDesc(&desc);

    // map
    HRESULT hr = pDeviceContext->device_context->Map(c->pStagingTexture, 0, D3D11_MAP_READ, 0, &map);
    if (FAILED(hr))
    {
        pDeviceContext->unlock(pDeviceContext->lock_ctx);
        return false;
    }

    pBuffer->data[0] = (BYTE *)map.pData;
    pBuffer->data[1] = pBuffer->data[0] + desc.Height * map.RowPitch;

    pBuffer->stride[0] = map.RowPitch;
    pBuffer->stride[1] = map.RowPitch;

    return true;
}

static void d3d11_direct_unlock(LAVFrame *pFrame)
{
    D3D11DirectPrivate *c = (D3D11DirectPrivate *)pFrame->priv_data;
    AVD3D11VADeviceContext *pDeviceContext =
        (AVD3D11VADeviceContext *)((AVHWDeviceContext *)c->pDeviceContex->data)->hwctx;

    pDeviceContext->device_context->Unmap(c->pStagingTexture, 0);
    pDeviceContext->unlock(pDeviceContext->lock_ctx);
}

static void d3d11_direct_free(LAVFrame *pFrame)
{
    D3D11DirectPrivate *c = (D3D11DirectPrivate *)pFrame->priv_data;
    av_buffer_unref(&c->pDeviceContex);
    c->pStagingTexture->Release();
    delete c;
}

HRESULT CDecD3D11::DeliverD3D11ReadbackDirect(LAVFrame *pFrame)
{
    AVD3D11VADeviceContext *pDeviceContext = (AVD3D11VADeviceContext *)((AVHWDeviceContext *)m_pDevCtx->data)->hwctx;
    AVFrame *src = (AVFrame *)pFrame->priv_data;

    if (m_pD3D11StagingTexture == nullptr)
    {
        D3D11_TEXTURE2D_DESC texDesc = {0};
        ((ID3D11Texture2D *)src->data[0])->GetDesc(&texDesc);

        texDesc.ArraySize = 1;
        texDesc.Usage = D3D11_USAGE_STAGING;
        texDesc.BindFlags = 0;
        texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        texDesc.MiscFlags = 0;

        HRESULT hr = pDeviceContext->device->CreateTexture2D(&texDesc, nullptr, &m_pD3D11StagingTexture);
        if (FAILED(hr))
        {
            ReleaseFrame(&pFrame);
            return E_FAIL;
        }
    }

    pDeviceContext->lock(pDeviceContext->lock_ctx);
    pDeviceContext->device_context->CopySubresourceRegion(
        m_pD3D11StagingTexture, 0, 0, 0, 0, (ID3D11Texture2D *)src->data[0], (UINT)(intptr_t)src->data[1], nullptr);
    pDeviceContext->unlock(pDeviceContext->lock_ctx);

    av_frame_free(&src);

    D3D11DirectPrivate *c = new D3D11DirectPrivate;
    c->pDeviceContex = av_buffer_ref(m_pDevCtx);
    c->pStagingTexture = m_pD3D11StagingTexture;
    m_pD3D11StagingTexture->AddRef();

    pFrame->priv_data = c;
    pFrame->destruct = d3d11_direct_free;

    GetPixelFormat(&pFrame->format, &pFrame->bpp);

    pFrame->direct = true;
    pFrame->direct_lock = d3d11_direct_lock;
    pFrame->direct_unlock = d3d11_direct_unlock;

    return Deliver(pFrame);
}

STDMETHODIMP CDecD3D11::GetPixelFormat(LAVPixelFormat *pPix, int *pBpp)
{
    // Output is always NV12 or P010
    if (pPix)
        *pPix = m_bReadBackFallback == false ? LAVPixFmt_D3D11 : d3d11va_map_hw_to_lav_format(m_SurfaceFormat);

    if (pBpp)
        *pBpp = (m_SurfaceFormat == DXGI_FORMAT_NV12 || m_SurfaceFormat == DXGI_FORMAT_YUY2 ||
                 m_SurfaceFormat == DXGI_FORMAT_AYUV)
                    ? 8
                : (m_SurfaceFormat == DXGI_FORMAT_P010 || m_SurfaceFormat == DXGI_FORMAT_Y210 ||
                   m_SurfaceFormat == DXGI_FORMAT_Y410)
                    ? 10
                : (m_SurfaceFormat == DXGI_FORMAT_P016 || m_SurfaceFormat == DXGI_FORMAT_Y216 ||
                   m_SurfaceFormat == DXGI_FORMAT_Y416)
                    ? 16
                    : 8;

    return S_OK;
}

STDMETHODIMP_(DWORD) CDecD3D11::GetHWAccelNumDevices()
{
    DWORD nDevices = 0;
    UINT i = 0;
    IDXGIAdapter *pDXGIAdapter = nullptr;
    IDXGIFactory1 *pDXGIFactory = nullptr;

    HRESULT hr = dx.mCreateDXGIFactory1(IID_IDXGIFactory1, (void **)&pDXGIFactory);
    if (FAILED(hr))
        goto fail;

    DXGI_ADAPTER_DESC desc;
    while (SUCCEEDED(pDXGIFactory->EnumAdapters(i, &pDXGIAdapter)))
    {
        pDXGIAdapter->GetDesc(&desc);
        SafeRelease(&pDXGIAdapter);

        // stop when we hit the MS software device
        if (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c)
            break;

        i++;
    }

    nDevices = i;

fail:
    SafeRelease(&pDXGIFactory);
    return nDevices;
}

STDMETHODIMP CDecD3D11::GetHWAccelDeviceInfo(DWORD dwIndex, BSTR *pstrDeviceName, DWORD *dwDeviceIdentifier)
{
    IDXGIAdapter *pDXGIAdapter = nullptr;
    IDXGIFactory1 *pDXGIFactory = nullptr;

    HRESULT hr = dx.mCreateDXGIFactory1(IID_IDXGIFactory1, (void **)&pDXGIFactory);
    if (FAILED(hr))
        goto fail;

    hr = pDXGIFactory->EnumAdapters(dwIndex, &pDXGIAdapter);
    if (FAILED(hr))
        goto fail;

    DXGI_ADAPTER_DESC desc;
    pDXGIAdapter->GetDesc(&desc);

    // stop when we hit the MS software device
    if (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c)
    {
        hr = E_INVALIDARG;
        goto fail;
    }

    if (pstrDeviceName)
        *pstrDeviceName = SysAllocString(desc.Description);

    if (dwDeviceIdentifier)
        *dwDeviceIdentifier = desc.DeviceId;

fail:
    SafeRelease(&pDXGIFactory);
    SafeRelease(&pDXGIAdapter);
    return hr;
}

STDMETHODIMP CDecD3D11::GetHWAccelActiveDevice(BSTR *pstrDeviceName)
{
    CheckPointer(pstrDeviceName, E_POINTER);

    if (m_AdapterDesc.Description[0] == 0)
        return E_UNEXPECTED;

    *pstrDeviceName = SysAllocString(m_AdapterDesc.Description);
    return S_OK;
}
