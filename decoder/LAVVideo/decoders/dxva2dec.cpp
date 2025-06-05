/*
 *      Copyright (C) 2011-2021 Hendrik Leppkes
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
 *
 *  Basic concept based on VLC DXVA2 decoder, licensed under GPLv2
 */

#include "stdafx.h"
#include "dxva2dec.h"
#include "dxva2/dxva_common.h"
#include "dxva2/DXVA2SurfaceAllocator.h"
#include "moreuuids.h"
#include "Media.h"

#include <Shlwapi.h>

#include <dxva2api.h>
#include <evr.h>
#include "libavcodec/dxva2.h"

extern "C"
{
    #include "libavutil/imgutils.h"
    #define FF_DXVA2_WORKAROUND_SCALING_LIST_ZIGZAG 1
    #define FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO 2
    #define FF_DXVA2_WORKAROUND_NVIDIA_HEVC_420P12 3
}

////////////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////////////

ILAVDecoder *CreateDecoderDXVA2()
{
    return new CDecDXVA2();
}

ILAVDecoder *CreateDecoderDXVA2Native()
{
    CDecDXVA2 *dec = new CDecDXVA2();
    dec->SetNativeMode(TRUE);
    return dec;
}

HRESULT VerifyD3D9Device(DWORD &dwIndex, DWORD dwDeviceId)
{
    HRESULT hr = S_OK;

    IDirect3D9 *pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D)
        return E_FAIL;

    D3DADAPTER_IDENTIFIER9 d3dai = {0};

    // Check the combination of adapter and device id
    hr = pD3D->GetAdapterIdentifier(dwIndex, 0, &d3dai);
    if (hr == D3D_OK && d3dai.DeviceId == dwDeviceId)
        goto done;

    // find an adapter with the specified device id
    for (UINT i = 0; i < pD3D->GetAdapterCount(); i++)
    {
        hr = pD3D->GetAdapterIdentifier(i, 0, &d3dai);
        if (hr == D3D_OK && d3dai.DeviceId == dwDeviceId)
        {
            dwIndex = i;
            goto done;
        }
    }

    // fail otherwise
    hr = E_FAIL;

done:
    SafeRelease(&pD3D);
    return hr;
}

// List of PCI Device ID of ATI cards with UVD or UVD+ decoding block.
static DWORD UVDDeviceID[] = {0x94C7, // ATI Radeon HD 2350
                              0x94C1, // ATI Radeon HD 2400 XT
                              0x94CC, // ATI Radeon HD 2400 Series
                              0x958A, // ATI Radeon HD 2600 X2 Series
                              0x9588, // ATI Radeon HD 2600 XT
                              0x9405, // ATI Radeon HD 2900 GT
                              0x9400, // ATI Radeon HD 2900 XT
                              0x9611, // ATI Radeon 3100 Graphics
                              0x9610, // ATI Radeon HD 3200 Graphics
                              0x9614, // ATI Radeon HD 3300 Graphics
                              0x95C0, // ATI Radeon HD 3400 Series (and others)
                              0x95C5, // ATI Radeon HD 3400 Series (and others)
                              0x95C4, // ATI Radeon HD 3400 Series (and others)
                              0x94C3, // ATI Radeon HD 3410
                              0x9589, // ATI Radeon HD 3600 Series (and others)
                              0x9598, // ATI Radeon HD 3600 Series (and others)
                              0x9591, // ATI Radeon HD 3600 Series (and others)
                              0x9501, // ATI Radeon HD 3800 Series (and others)
                              0x9505, // ATI Radeon HD 3800 Series (and others)
                              0x9507, // ATI Radeon HD 3830
                              0x9513, // ATI Radeon HD 3850 X2
                              0x950F, // ATI Radeon HD 3850 X2
                              0x0000};

static int IsAMDUVD(DWORD dwDeviceId)
{
    for (int i = 0; UVDDeviceID[i] != 0; i++)
    {
        if (UVDDeviceID[i] == dwDeviceId)
            return 1;
    }
    return 0;
}

static D3DFORMAT get_dxva_surface_format(AVCodecContext *ctx)
{
    if (ctx->sw_pix_fmt == AV_PIX_FMT_YUV420P || ctx->sw_pix_fmt == AV_PIX_FMT_YUVJ420P || ctx->sw_pix_fmt == AV_PIX_FMT_NV12)
        return (D3DFORMAT)FOURCC_NV12;
    else if (ctx->sw_pix_fmt == AV_PIX_FMT_YUV420P10 || ctx->sw_pix_fmt == AV_PIX_FMT_P010)
        return (D3DFORMAT)FOURCC_P010;

    ASSERT(ctx->sw_pix_fmt == AV_PIX_FMT_NONE);
    return (D3DFORMAT)FOURCC_NV12;
}


////////////////////////////////////////////////////////////////////////////////
// DXVA2 decoder implementation
////////////////////////////////////////////////////////////////////////////////

CDecDXVA2::CDecDXVA2(void)
    : CDecAvcodec()
{
    ZeroMemory(&dx, sizeof(dx));
    ZeroMemory(&m_pSurfaces, sizeof(m_pSurfaces));
    ZeroMemory(&m_pRawSurface, sizeof(m_pRawSurface));
    ZeroMemory(&m_FrameQueue, sizeof(m_FrameQueue));
    ZeroMemory(&m_DXVAVideoDecoderConfig, sizeof(m_DXVAVideoDecoderConfig));
}

CDecDXVA2::~CDecDXVA2(void)
{
    DestroyDecoder(true);
    if (m_pDXVA2Allocator)
        m_pDXVA2Allocator->DecoderDestruct();
}

STDMETHODIMP CDecDXVA2::DestroyDecoder(bool bFull, bool bNoAVCodec)
{
    for (int i = 0; i < DXVA2_QUEUE_SURFACES; i++)
    {
        ReleaseFrame(&m_FrameQueue[i]);
    }

    m_pCallback->ReleaseAllDXVAResources();
    for (int i = 0; i < m_NumSurfaces; i++)
    {
        SafeRelease(&m_pSurfaces[i].d3d);
    }
    m_NumSurfaces = 0;

    SafeRelease(&m_pDecoder);

    if (!bNoAVCodec)
    {
        CDecAvcodec::DestroyDecoder();
    }

    if (bFull)
    {
        FreeD3DResources();
    }

    return S_OK;
}

STDMETHODIMP CDecDXVA2::FreeD3DResources()
{
    SafeRelease(&m_pDXVADecoderService);
    if (m_pD3DDevMngr && m_hDevice != INVALID_HANDLE_VALUE)
        m_pD3DDevMngr->CloseDeviceHandle(m_hDevice);
    m_hDevice = INVALID_HANDLE_VALUE;
    SafeRelease(&m_pD3DDevMngr);
    SafeRelease(&m_pD3DDev);
    SafeRelease(&m_pD3D);

    if (dx.d3dlib)
    {
        FreeLibrary(dx.d3dlib);
        dx.d3dlib = nullptr;
    }

    if (dx.dxva2lib)
    {
        FreeLibrary(dx.dxva2lib);
        dx.dxva2lib = nullptr;
    }

    return S_OK;
}

STDMETHODIMP CDecDXVA2::InitAllocator(IMemAllocator **ppAlloc)
{
    HRESULT hr = S_OK;
    if (!m_bNative)
        return E_NOTIMPL;

    m_pDXVA2Allocator = new CDXVA2SurfaceAllocator(this, &hr);
    if (!m_pDXVA2Allocator)
    {
        return E_OUTOFMEMORY;
    }
    if (FAILED(hr))
    {
        SAFE_DELETE(m_pDXVA2Allocator);
        return hr;
    }

    return m_pDXVA2Allocator->QueryInterface(__uuidof(IMemAllocator), (void **)ppAlloc);
}

STDMETHODIMP CDecDXVA2::PostConnect(IPin *pPin)
{
    HRESULT hr = S_OK;

    if (!m_bNative && m_pD3DDevMngr)
        return S_OK;

    DbgLog((LOG_TRACE, 10, L"CDecDXVA2::PostConnect()"));

    IMFGetService *pGetService = nullptr;
    hr = pPin->QueryInterface(__uuidof(IMFGetService), (void **)&pGetService);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 10, L"-> IMFGetService not available"));
        goto done;
    }

    // Release old D3D resources, we're about to re-init
    m_pCallback->ReleaseAllDXVAResources();
    FreeD3DResources();

    // Get the Direct3D device manager.
    hr = pGetService->GetService(MR_VIDEO_ACCELERATION_SERVICE, __uuidof(IDirect3DDeviceManager9),
                                 (void **)&m_pD3DDevMngr);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 10, L"-> D3D Device Manager not available"));
        goto done;
    }

    hr = SetD3DDeviceManager(m_pD3DDevMngr);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 10, L"-> Setting D3D Device Manager failed"));
        goto done;
    }

    if (m_bNative)
    {
        if (!m_pDecoder)
        {
            // If this is the first call, re-align surfaces, as the requirements may only be known now
            m_dwSurfaceWidth = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_width);
            m_dwSurfaceHeight = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_height);
        }

        CMediaType mt = m_pCallback->GetOutputMediaType();
        if ((m_eSurfaceFormat == FOURCC_NV12 && mt.subtype != MEDIASUBTYPE_NV12) ||
            (m_eSurfaceFormat == FOURCC_P010 && mt.subtype != MEDIASUBTYPE_P010) ||
            (m_eSurfaceFormat == FOURCC_P016 && mt.subtype != MEDIASUBTYPE_P016))
        {
            DbgLog((LOG_ERROR, 10, L"-> Connection is not the appropriate pixel format for DXVA2 Native"));
            hr = E_FAIL;
            goto done;
        }
        hr = DXVA2NotifyEVR();
    }

done:
    SafeRelease(&pGetService);
    if (FAILED(hr))
    {
        FreeD3DResources();
    }
    return hr;
}

HRESULT CDecDXVA2::DXVA2NotifyEVR()
{
    HRESULT hr = S_OK;
    IMFGetService *pGetService = nullptr;
    IDirectXVideoMemoryConfiguration *pVideoConfig = nullptr;

    hr = m_pCallback->GetOutputPin()->GetConnected()->QueryInterface(&pGetService);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 10, L"-> IMFGetService not available"));
        goto done;
    }

    // Configure EVR for receiving DXVA2 samples
    hr = pGetService->GetService(MR_VIDEO_ACCELERATION_SERVICE, __uuidof(IDirectXVideoMemoryConfiguration),
                                 (void **)&pVideoConfig);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 10, L"-> IDirectXVideoMemoryConfiguration not available"));
        goto done;
    }

    // Notify the EVR about the format we're sending
    DXVA2_SurfaceType surfaceType;
    for (DWORD iTypeIndex = 0;; iTypeIndex++)
    {
        hr = pVideoConfig->GetAvailableSurfaceTypeByIndex(iTypeIndex, &surfaceType);
        if (FAILED(hr))
        {
            hr = S_OK;
            break;
        }

        if (surfaceType == DXVA2_SurfaceType_DecoderRenderTarget)
        {
            hr = pVideoConfig->SetSurfaceType(DXVA2_SurfaceType_DecoderRenderTarget);
            break;
        }
    }
done:
    SafeRelease(&pGetService);
    SafeRelease(&pVideoConfig);
    return hr;
}

STDMETHODIMP CDecDXVA2::LoadDXVA2Functions()
{
    // Load D3D9 library
    dx.d3dlib = LoadLibrary(L"d3d9.dll");
    if (dx.d3dlib == nullptr)
    {
        DbgLog((LOG_TRACE, 10, L"-> Loading d3d9.dll failed"));
        return E_FAIL;
    }

    dx.direct3DCreate9Ex = (pDirect3DCreate9Ex *)GetProcAddress(dx.d3dlib, "Direct3DCreate9Ex");

    // Load DXVA2 library
    dx.dxva2lib = LoadLibrary(L"dxva2.dll");
    if (dx.dxva2lib == nullptr)
    {
        DbgLog((LOG_TRACE, 10, L"-> Loading dxva2.dll failed"));
        return E_FAIL;
    }

    dx.createDeviceManager = (pCreateDeviceManager9 *)GetProcAddress(dx.dxva2lib, "DXVA2CreateDirect3DDeviceManager9");
    if (dx.createDeviceManager == nullptr)
    {
        DbgLog((LOG_TRACE, 10, L"-> DXVA2CreateDirect3DDeviceManager9 unavailable"));
        return E_FAIL;
    }

    return S_OK;
}

static const struct
{
    unsigned id;
    char name[32];
} vendors[] = {{VEND_ID_ATI, "ATI"}, {VEND_ID_NVIDIA, "NVIDIA"}, {VEND_ID_INTEL, "Intel"}, {0, ""}};

HRESULT CDecDXVA2::CreateD3DDeviceManager(IDirect3DDevice9 *pDevice, UINT *pReset, IDirect3DDeviceManager9 **ppManager)
{
    UINT resetToken = 0;
    IDirect3DDeviceManager9 *pD3DManager = nullptr;
    HRESULT hr = dx.createDeviceManager(&resetToken, &pD3DManager);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 10, L"-> DXVA2CreateDirect3DDeviceManager9 failed"));
        goto done;
    }

    hr = pD3DManager->ResetDevice(pDevice, resetToken);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 10, L"-> ResetDevice failed"));
        goto done;
    }

    *ppManager = pD3DManager;
    (*ppManager)->AddRef();

    *pReset = resetToken;
done:
    SafeRelease(&pD3DManager);
    return hr;
}

HRESULT CDecDXVA2::CreateDXVAVideoService(IDirect3DDeviceManager9 *pManager, IDirectXVideoDecoderService **ppService)
{
    HRESULT hr = S_OK;

    IDirectXVideoDecoderService *pService = nullptr;

    hr = pManager->OpenDeviceHandle(&m_hDevice);
    if (FAILED(hr))
    {
        m_hDevice = INVALID_HANDLE_VALUE;
        DbgLog((LOG_ERROR, 10, L"-> OpenDeviceHandle failed"));
        goto done;
    }

    hr = pManager->GetVideoService(m_hDevice, IID_IDirectXVideoDecoderService, (void **)&pService);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 10, L"-> Acquiring VideoDecoderService failed"));
        goto done;
    }

    (*ppService) = pService;

done:
    return hr;
}

HRESULT CDecDXVA2::FindVideoServiceConversion(AVCodecID codec, int profile, D3DFORMAT suggestedOutput, GUID *input, D3DFORMAT *output)
{
    HRESULT hr = S_OK;

    UINT count = 0;
    GUID *input_list = nullptr;

    /* Gather the format supported by the decoder */
    hr = m_pDXVADecoderService->GetDecoderDeviceGuids(&count, &input_list);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"-> GetDecoderDeviceGuids failed with hr: %X", hr));
        goto done;
    }

    DbgLog((LOG_TRACE, 10, L"-> Enumerating supported DXVA2 modes (count: %d)", count));
    for (unsigned i = 0; i < count; i++)
    {
        const GUID *g = &input_list[i];
        const dxva_mode_t *mode = get_dxva_mode_from_guid(g);
        if (mode)
        {
            DbgLog((LOG_TRACE, 10, L"  -> %S", mode->name));
        }
        else
        {
            DbgLog((LOG_TRACE, 10, L"  -> Unknown GUID (%s)", WStringFromGUID(*g).c_str()));
        }
    }

    /* Iterate over our priority list */
    for (unsigned i = 0; dxva_modes[i].name; i++)
    {
        const dxva_mode_t *mode = &dxva_modes[i];
        if (!check_dxva_mode_compatibility(mode, codec, profile, 0, (suggestedOutput == FOURCC_NV12)))
            continue;

        BOOL supported = FALSE;
        for (const GUID *g = &input_list[0]; !supported && g < &input_list[count]; g++)
        {
            supported = IsEqualGUID(*mode->guid, *g);
        }
        if (!supported)
            continue;

        DbgLog((LOG_TRACE, 10, L"-> Trying to use '%S'", mode->name));
        UINT out_count = 0;
        D3DFORMAT *out_list = nullptr;
        hr = m_pDXVADecoderService->GetDecoderRenderTargets(*mode->guid, &out_count, &out_list);
        if (FAILED(hr))
        {
            DbgLog((LOG_TRACE, 10, L"-> Retrieving render targets failed with hr: %X", hr));
            continue;
        }

        BOOL matchingFormat = FALSE;
        D3DFORMAT format = D3DFMT_UNKNOWN;
        DbgLog((LOG_TRACE, 10, L"-> Enumerating render targets (count: %d)", out_count));
        for (unsigned j = 0; j < out_count; j++)
        {
            const D3DFORMAT f = out_list[j];
            DbgLog((LOG_TRACE, 10, L"  -> %d is supported (%4.4S)", f, (const char *)&f));
            if (suggestedOutput == f || (suggestedOutput == FOURCC_P010 && f == FOURCC_P016))
            {
                matchingFormat = TRUE;
                format = f;
            }
        }
        if (matchingFormat)
        {
            DbgLog((LOG_TRACE, 10, L"-> Found matching output format, finished setup with render target %d (%4.4S)", format, (const char *)&format));
            *input = *mode->guid;
            *output = format;

            SAFE_CO_FREE(out_list);
            SAFE_CO_FREE(input_list);

            return S_OK;
        }

        SAFE_CO_FREE(out_list);
    }

done:
    SAFE_CO_FREE(input_list);
    return E_FAIL;
}

HRESULT CDecDXVA2::InitD3DAdapterIdentifier(UINT lAdapter)
{
    ASSERT(m_pD3D);

    D3DADAPTER_IDENTIFIER9 d3dai = {0};
    HRESULT hr = m_pD3D->GetAdapterIdentifier(lAdapter, 0, &d3dai);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"-> Querying of adapter identifier %d failed with hr: %X", lAdapter, hr));
        return E_FAIL;
    }

    const char *vendor = "Unknown";
    for (int i = 0; vendors[i].id != 0; i++)
    {
        if (vendors[i].id == d3dai.VendorId)
        {
            vendor = vendors[i].name;
            break;
        }
    }

    DbgLog((LOG_TRACE, 10, L"-> Running on adapter %d, %S, vendor 0x%04X(%S), device 0x%04X", lAdapter,
            d3dai.Description, d3dai.VendorId, vendor, d3dai.DeviceId));
    m_dwVendorId = d3dai.VendorId;
    m_dwDeviceId = d3dai.DeviceId;

    return S_OK;
}

/**
 * This function is only called in non-native mode
 * Its responsibility is to initialize D3D, create a device and a device manager
 * and call SetD3DDeviceManager with it.
 */
HRESULT CDecDXVA2::InitD3D(UINT lAdapter)
{
    HRESULT hr = S_OK;

    m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!m_pD3D)
    {
        DbgLog((LOG_ERROR, 10, L"-> Failed to acquire IDirect3D9"));
        return E_FAIL;
    }

    // populate the adapter identifier values
    hr = InitD3DAdapterIdentifier(lAdapter);

    // if the requested adapter failed, try again
    if (FAILED(hr) && lAdapter != D3DADAPTER_DEFAULT)
    {
        lAdapter = D3DADAPTER_DEFAULT;
        hr = InitD3DAdapterIdentifier(lAdapter);
    }

    if (FAILED(hr))
    {
        return hr;
    }

    D3DPRESENT_PARAMETERS d3dpp = {0};
    D3DDISPLAYMODE d3ddm = {0};

    m_pD3D->GetAdapterDisplayMode(lAdapter, &d3ddm);

    d3dpp.Windowed = TRUE;
    d3dpp.BackBufferWidth = 640;
    d3dpp.BackBufferHeight = 480;
    d3dpp.BackBufferCount = 0;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.Flags = D3DPRESENTFLAG_VIDEO;

    hr = m_pD3D->CreateDevice(lAdapter, D3DDEVTYPE_HAL, GetShellWindow(),
                              D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
                              &d3dpp, &m_pD3DDev);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"-> Creation of device failed with hr: %X", hr));
        return E_FAIL;
    }

    return S_OK;
}

HRESULT CDecDXVA2::InitD3DEx(UINT lAdapter)
{
    HRESULT hr = S_OK;

    if (dx.direct3DCreate9Ex == nullptr)
    {
        DbgLog((LOG_ERROR, 10, L"-> Direct3DCreate9Ex not available"));
        return E_NOINTERFACE;
    }

    IDirect3D9Ex *pD3D9Ex = nullptr;
    hr = dx.direct3DCreate9Ex(D3D_SDK_VERSION, &pD3D9Ex);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 10, L"-> Failed to acquire IDirect3D9Ex"));
        return E_NOINTERFACE;
    }
    m_pD3D = dynamic_cast<IDirect3D9 *>(pD3D9Ex);

    // populate the adapter identifier values
    hr = InitD3DAdapterIdentifier(lAdapter);

    // if the requested adapter failed, try again
    if (FAILED(hr) && lAdapter != D3DADAPTER_DEFAULT)
    {
        lAdapter = D3DADAPTER_DEFAULT;
        hr = InitD3DAdapterIdentifier(lAdapter);
    }

    if (FAILED(hr))
    {
        SafeRelease(&m_pD3D);
        return hr;
    }

    D3DPRESENT_PARAMETERS d3dpp = {0};
    D3DDISPLAYMODEEX d3ddm = {0};
    d3ddm.Size = sizeof(D3DDISPLAYMODEEX);

    pD3D9Ex->GetAdapterDisplayModeEx(lAdapter, &d3ddm, NULL);

    d3dpp.Windowed = TRUE;
    d3dpp.BackBufferWidth = 640;
    d3dpp.BackBufferHeight = 480;
    d3dpp.BackBufferCount = 0;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.Flags = D3DPRESENTFLAG_VIDEO;

    IDirect3DDevice9Ex *pD3D9DeviceEx = nullptr;
    hr = pD3D9Ex->CreateDeviceEx(lAdapter, D3DDEVTYPE_HAL, GetShellWindow(),
                                 D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
                                 &d3dpp, NULL, &pD3D9DeviceEx);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"-> Creation of device failed with hr: %X", hr));
        SafeRelease(&m_pD3D);
        return E_FAIL;
    }
    m_pD3DDev = dynamic_cast<IDirect3DDevice9 *>(pD3D9DeviceEx);

    return S_OK;
}

HRESULT CDecDXVA2::RetrieveVendorId(IDirect3DDeviceManager9 *pDevManager)
{
    HANDLE hDevice = 0;
    IDirect3D9 *pD3D = nullptr;
    IDirect3DDevice9 *pDevice = nullptr;

    HRESULT hr = pDevManager->OpenDeviceHandle(&hDevice);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"-> Failed to open device handle with hr: %X", hr));
        goto done;
    }

    hr = pDevManager->LockDevice(hDevice, &pDevice, TRUE);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"-> Failed to lock device with hr: %X", hr));
        goto done;
    }

    hr = pDevice->GetDirect3D(&pD3D);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"-> Failed to get D3D object hr: %X", hr));
        goto done;
    }

    D3DDEVICE_CREATION_PARAMETERS devParams;
    hr = pDevice->GetCreationParameters(&devParams);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"-> Failed to get device creation params hr: %X", hr));
        goto done;
    }

    D3DADAPTER_IDENTIFIER9 adIdentifier;
    hr = pD3D->GetAdapterIdentifier(devParams.AdapterOrdinal, 0, &adIdentifier);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"-> Failed to get adapter identified hr: %X", hr));
        goto done;
    }

    m_dwVendorId = adIdentifier.VendorId;
    m_dwDeviceId = adIdentifier.DeviceId;
    memcpy(m_cDeviceName, adIdentifier.Description, sizeof(m_cDeviceName));

done:
    SafeRelease(&pD3D);
    SafeRelease(&pDevice);
    if (hDevice && hDevice != INVALID_HANDLE_VALUE)
    {
        pDevManager->UnlockDevice(hDevice, FALSE);
        pDevManager->CloseDeviceHandle(hDevice);
    }
    return hr;
}

HRESULT CDecDXVA2::CheckHWCompatConditions(GUID decoderGuid)
{
    if (m_dwSurfaceWidth == 0 || m_dwSurfaceHeight == 0)
        return E_UNEXPECTED;

    int width_mbs = m_dwSurfaceWidth / 16;
    int height_mbs = m_dwSurfaceHeight / 16;
    int max_ref_frames_dpb41 = min(11, 32768 / (width_mbs * height_mbs));
    if (m_dwVendorId == VEND_ID_ATI)
    {
        if (IsAMDUVD(m_dwDeviceId))
        {
            if (m_pAVCtx->codec_id == AV_CODEC_ID_H264 && m_pAVCtx->refs > max_ref_frames_dpb41)
            {
                DbgLog((LOG_TRACE, 10, L"-> Too many reference frames for AMD UVD/UVD+ H.264 decoder"));
                return E_FAIL;
            }
            else if ((m_pAVCtx->codec_id == AV_CODEC_ID_VC1 || m_pAVCtx->codec_id == AV_CODEC_ID_MPEG2VIDEO) &&
                     (m_dwSurfaceWidth > 1920 || m_dwSurfaceHeight > 1200))
            {
                DbgLog((LOG_TRACE, 10, L"-> VC-1 Resolutions above FullHD are not supported by the UVD/UVD+ decoder"));
                return E_FAIL;
            }
            else if (m_pAVCtx->codec_id == AV_CODEC_ID_WMV3)
            {
                DbgLog((LOG_TRACE, 10, L"-> AMD UVD/UVD+ is currently not compatible with WMV3"));
                return E_FAIL;
            }
        }
    }
    else if (m_dwVendorId == VEND_ID_INTEL)
    {
        if (decoderGuid == DXVADDI_Intel_ModeH264_E && m_pAVCtx->codec_id == AV_CODEC_ID_H264 &&
            m_pAVCtx->refs > max_ref_frames_dpb41)
        {
            DbgLog((LOG_TRACE, 10, L"-> Too many reference frames for Intel H.264 decoder implementation"));
            return E_FAIL;
        }
    }
    return S_OK;
}

/**
 * Called from both native and non-native mode
 * Initialize all the common DXVA2 interfaces and device handles
 */
HRESULT CDecDXVA2::SetD3DDeviceManager(IDirect3DDeviceManager9 *pDevManager)
{
    HRESULT hr = S_OK;
    ASSERT(pDevManager);

    m_pD3DDevMngr = pDevManager;

    RetrieveVendorId(pDevManager);

    // This should really be null here, but since we're overwriting it, make sure its actually released
    SafeRelease(&m_pDXVADecoderService);

    hr = CreateDXVAVideoService(m_pD3DDevMngr, &m_pDXVADecoderService);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"-> Creation of DXVA2 Decoder Service failed with hr: %X", hr));
        goto done;
    }

    // If the decoder was initialized already, check if we can use this device
    if (m_pAVCtx)
    {
        DbgLog((LOG_TRACE, 10, L"-> Checking hardware for format support..."));

        GUID input = GUID_NULL;
        D3DFORMAT output;
        hr = FindVideoServiceConversion(m_pAVCtx->codec_id, m_pAVCtx->profile, get_dxva_surface_format(m_pAVCtx), &input, &output);
        if (FAILED(hr))
        {
            DbgLog((LOG_TRACE, 10, L"-> No decoder device available that can decode codec '%S' to a matching output",
                    avcodec_get_name(m_pAVCtx->codec_id)));
            goto done;
        }

        m_eSurfaceFormat = output;

        if (FAILED(CheckHWCompatConditions(input)))
        {
            hr = E_FAIL;
            goto done;
        }

        DXVA2_VideoDesc desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.SampleWidth = m_dwSurfaceWidth;
        desc.SampleHeight = m_dwSurfaceHeight;
        desc.Format = output;

        DXVA2_ConfigPictureDecode config;
        hr = FindDecoderConfiguration(input, &desc, &config);
        if (FAILED(hr))
        {
            DbgLog((LOG_TRACE, 10, L"-> No decoder configuration available for codec '%S'",
                    avcodec_get_name(m_pAVCtx->codec_id)));
            goto done;
        }

        LPDIRECT3DSURFACE9 pSurfaces[DXVA2_MAX_SURFACES] = {0};
        UINT numSurfaces = max(config.ConfigMinRenderTargetBuffCount, 1);
        hr = m_pDXVADecoderService->CreateSurface(m_dwSurfaceWidth, m_dwSurfaceHeight, numSurfaces, output,
                                                  D3DPOOL_DEFAULT, 0, DXVA2_VideoDecoderRenderTarget, pSurfaces,
                                                  nullptr);
        if (FAILED(hr))
        {
            DbgLog((LOG_TRACE, 10, L"-> Creation of surfaces failed with hr: %X", hr));
            goto done;
        }

        IDirectXVideoDecoder *decoder = nullptr;
        hr = m_pDXVADecoderService->CreateVideoDecoder(input, &desc, &config, pSurfaces, numSurfaces, &decoder);

        // Release resources, decoder and surfaces
        SafeRelease(&decoder);
        int i = DXVA2_MAX_SURFACES;
        while (i > 0)
        {
            SafeRelease(&pSurfaces[--i]);
        }

        if (FAILED(hr))
        {
            DbgLog((LOG_TRACE, 10, L"-> Creation of decoder failed with hr: %X", hr));
            goto done;
        }
    }

done:
    return hr;
}

// ILAVDecoder
STDMETHODIMP CDecDXVA2::Init()
{
    DbgLog((LOG_TRACE, 10, L"CDecDXVA2::Init(): Trying to open DXVA2 decoder"));
    HRESULT hr = S_OK;

    // Initialize all D3D interfaces in non-native mode
    if (!m_bNative)
    {
        // load DLLs and functions
        if (FAILED(hr = LoadDXVA2Functions()))
        {
            DbgLog((LOG_ERROR, 10, L"-> Failed to load DXVA2 DLL functions"));
            return E_FAIL;
        }

        // determin the adapter the user requested
        UINT lAdapter = m_pSettings->GetHWAccelDeviceIndex(HWAccel_DXVA2CopyBack, nullptr);
        if (lAdapter == LAVHWACCEL_DEVICE_DEFAULT)
            lAdapter = D3DADAPTER_DEFAULT;

        DWORD dwDeviceIndex = m_pCallback->GetGPUDeviceIndex();
        if (dwDeviceIndex != DWORD_MAX)
        {
            lAdapter = (UINT)dwDeviceIndex;
        }

        // initialize D3D
        hr = InitD3DEx(lAdapter);
        if (hr == E_NOINTERFACE)
        {
            // D3D9Ex failed, try plain D3D
            hr = InitD3D(lAdapter);
        }

        if (FAILED(hr))
        {
            DbgLog((LOG_TRACE, 10, L"-> D3D Initialization failed with hr: %X", hr));
            return hr;
        }

        // create device manager for the device
        hr = CreateD3DDeviceManager(m_pD3DDev, &m_pD3DResetToken, &m_pD3DDevMngr);
        if (FAILED(hr))
        {
            DbgLog((LOG_TRACE, 10, L"-> Creation of Device manager failed with hr: %X", hr));
            return E_FAIL;
        }

        // set it as the active device manager
        hr = SetD3DDeviceManager(m_pD3DDevMngr);
        if (FAILED(hr))
        {
            DbgLog((LOG_TRACE, 10, L"-> SetD3DDeviceManager failed with hr: %X", hr));
            return E_FAIL;
        }
    }

    // Init the ffmpeg parts
    // This is our main software decoder, unable to fail!
    CDecAvcodec::Init();

    return S_OK;
}

STDMETHODIMP CDecDXVA2::InitDecoder(AVCodecID codec, const CMediaType *pmt, const MediaSideDataFFMpeg *pSideData)
{
    HRESULT hr = S_OK;
    DbgLog((LOG_TRACE, 10, L"CDecDXVA2::InitDecoder(): Initializing DXVA2 decoder"));

    // Hack-ish check to avoid re-creating the full decoder when only the aspect ratio changes.
    // Re-creating the DXVA2 decoder can lead to issues like missing frames or a several second delay
    if (m_pDecoder)
    {
        CMediaType mediaTypeCheck = m_MediaType;
        if (mediaTypeCheck.formattype == FORMAT_VideoInfo2 && pmt->formattype == FORMAT_VideoInfo2)
        {
            VIDEOINFOHEADER2 *vih2Old = (VIDEOINFOHEADER2 *)mediaTypeCheck.Format();
            VIDEOINFOHEADER2 *vih2New = (VIDEOINFOHEADER2 *)pmt->Format();

            vih2Old->dwPictAspectRatioX = vih2New->dwPictAspectRatioX;
            vih2Old->dwPictAspectRatioY = vih2New->dwPictAspectRatioY;

            if (mediaTypeCheck == *pmt)
            {
                DbgLog((LOG_TRACE, 10, L"-> Skipping re-init because media type is unchanged."));
                m_MediaType = *pmt;

                // flush the decoder so we can resume decoding properly (before a re-init, EndOfStream would be called,
                // making this necessary)
                avcodec_flush_buffers(m_pAVCtx);
                return S_OK;
            }
        }
    }

    DestroyDecoder(false);

    m_DisplayDelay = DXVA2_QUEUE_SURFACES;

    // Intel GPUs don't like the display and performance goes way down, so disable it.
    if (m_dwVendorId == VEND_ID_INTEL)
        m_DisplayDelay = 0;

    // Reduce display delay for DVD decoding for lower decode latency
    if (m_pCallback->GetDecodeFlags() & LAV_VIDEO_DEC_FLAG_DVD)
        m_DisplayDelay /= 2;

    m_bFailHWDecode = FALSE;

    DbgLog((LOG_TRACE, 10, L"-> Creation of DXVA2 decoder successful, initializing ffmpeg"));
    hr = CDecAvcodec::InitDecoder(codec, pmt, pSideData);
    if (FAILED(hr))
    {
        return hr;
    }

    if (check_dxva_codec_profile(m_pAVCtx, AV_PIX_FMT_DXVA2_VLD))
    {
        DbgLog((LOG_TRACE, 10, L"-> Incompatible profile detected, falling back to software decoding"));
        return E_FAIL;
    }

    // If we have a DXVA Decoder, check if its capable
    // If we don't have one yet, it may be handed to us later, and compat is checked at that point
    GUID input = GUID_NULL;
    D3DFORMAT output = D3DFMT_UNKNOWN;
    if (m_pDXVADecoderService)
    {
        hr = FindVideoServiceConversion(codec, m_pAVCtx->profile, get_dxva_surface_format(m_pAVCtx), &input, &output);
        if (FAILED(hr))
        {
            DbgLog((LOG_TRACE, 10, L"-> No decoder device available that can decode codec '%S' to a matching output",
                    avcodec_get_name(codec)));
            return E_FAIL;
        }
    }
    else
    {
        output = get_dxva_surface_format(m_pAVCtx);
    }

    m_dwSurfaceWidth = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_width);
    m_dwSurfaceHeight = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_height);
    m_eSurfaceFormat = output;
    m_DecoderPixelFormat = m_pAVCtx->sw_pix_fmt;

    if (FAILED(CheckHWCompatConditions(input)))
    {
        return E_FAIL;
    }

    m_MediaType = *pmt;

    return S_OK;
}

STDMETHODIMP_(long) CDecDXVA2::GetBufferCount(long *pMaxBuffers)
{
    long buffers = 0;

    // Native decoding should use 16 buffers to enable seamless codec changes
    if (m_bNative)
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

    if (!m_bNative)
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

HRESULT CDecDXVA2::FindDecoderConfiguration(const GUID &input, const DXVA2_VideoDesc *pDesc,
                                            DXVA2_ConfigPictureDecode *pConfig)
{
    CheckPointer(pConfig, E_INVALIDARG);
    CheckPointer(pDesc, E_INVALIDARG);
    HRESULT hr = S_OK;
    UINT cfg_count = 0;
    DXVA2_ConfigPictureDecode *cfg_list = nullptr;
    hr = m_pDXVADecoderService->GetDecoderConfigurations(input, pDesc, nullptr, &cfg_count, &cfg_list);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"-> GetDecoderConfigurations failed with hr: %X", hr));
        return E_FAIL;
    }

    DbgLog((LOG_TRACE, 10, L"-> We got %d decoder configurations", cfg_count));
    int best_score = -1;
    DXVA2_ConfigPictureDecode best_cfg;
    for (unsigned i = 0; i < cfg_count; i++)
    {
        DXVA2_ConfigPictureDecode *cfg = &cfg_list[i];

        DbgLog((LOG_ERROR, 10, "-> Configuration Record %d: ConfigBitstreamRaw = %d", i, cfg->ConfigBitstreamRaw));

        int score;
        if (cfg->ConfigBitstreamRaw == 1)
            score = 1;
        else if (m_pAVCtx->codec_id == AV_CODEC_ID_H264 && cfg->ConfigBitstreamRaw == 2)
            score = 2;
        else if (m_pAVCtx->codec_id == AV_CODEC_ID_VP9) // hack for broken AMD drivers
            score = 0;
        else
            continue;
        if (IsEqualGUID(cfg->guidConfigBitstreamEncryption, DXVA2_NoEncrypt))
            score += 16;
        if (score > best_score)
        {
            best_score = score;
            best_cfg = *cfg;
        }
    }
    SAFE_CO_FREE(cfg_list);
    if (best_score < 0)
    {
        DbgLog((LOG_TRACE, 10, L"-> No matching configuration available"));
        return E_FAIL;
    }

    *pConfig = best_cfg;
    return S_OK;
}

HRESULT CDecDXVA2::CreateDXVA2Decoder(int nSurfaces, IDirect3DSurface9 **ppSurfaces)
{
    DbgLog((LOG_TRACE, 10, L"-> CDecDXVA2::CreateDXVA2Decoder"));
    HRESULT hr = S_OK;
    LPDIRECT3DSURFACE9 pSurfaces[DXVA2_MAX_SURFACES];

    if (!m_pDXVADecoderService)
        return E_FAIL;

    DestroyDecoder(false, true);

    GUID input = GUID_NULL;
    D3DFORMAT output;
    FindVideoServiceConversion(m_pAVCtx->codec_id, m_pAVCtx->profile, get_dxva_surface_format(m_pAVCtx),&input, &output);

    if (!nSurfaces)
    {
        m_dwSurfaceWidth = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_width);
        m_dwSurfaceHeight = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_height);
        m_eSurfaceFormat = output;
        m_DecoderPixelFormat = m_pAVCtx->sw_pix_fmt;

        m_NumSurfaces = GetBufferCount();
        hr = m_pDXVADecoderService->CreateSurface(m_dwSurfaceWidth, m_dwSurfaceHeight, m_NumSurfaces - 1, output,
                                                  D3DPOOL_DEFAULT, 0, DXVA2_VideoDecoderRenderTarget, pSurfaces,
                                                  nullptr);
        if (FAILED(hr))
        {
            DbgLog((LOG_TRACE, 10, L"-> Creation of surfaces failed with hr: %X", hr));
            m_NumSurfaces = 0;
            return E_FAIL;
        }
        ppSurfaces = pSurfaces;
    }
    else
    {
        m_NumSurfaces = nSurfaces;
        for (int i = 0; i < m_NumSurfaces; i++)
        {
            ppSurfaces[i]->AddRef();
        }
    }

    if (m_NumSurfaces <= 0)
    {
        DbgLog((LOG_TRACE, 10, L"-> No surfaces? No good!"));
        return E_FAIL;
    }

    // get the device, for ColorFill() to init the surfaces in black
    IDirect3DDevice9 *pDev = nullptr;
    ppSurfaces[0]->GetDevice(&pDev);

    for (int i = 0; i < m_NumSurfaces; i++)
    {
        m_pSurfaces[i].index = i;
        m_pSurfaces[i].d3d = ppSurfaces[i];
        m_pSurfaces[i].age = UINT64_MAX;
        m_pSurfaces[i].used = false;

        // fill the surface in black, to avoid the "green screen" in case the first frame fails to decode.
        if (pDev)
            pDev->ColorFill(ppSurfaces[i], NULL, D3DCOLOR_XYUV(0, 128, 128));
    }

    // and done with the device
    SafeRelease(&pDev);

    DbgLog((LOG_TRACE, 10, L"-> Successfully created %d surfaces (%dx%d)", m_NumSurfaces, m_dwSurfaceWidth,
            m_dwSurfaceHeight));

    DXVA2_VideoDesc desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.SampleWidth = m_dwSurfaceWidth;
    desc.SampleHeight = m_dwSurfaceHeight;
    desc.Format = output;

    hr = FindDecoderConfiguration(input, &desc, &m_DXVAVideoDecoderConfig);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"-> FindDecoderConfiguration failed with hr: %X", hr));
        return hr;
    }

    IDirectXVideoDecoder *decoder = nullptr;
    hr = m_pDXVADecoderService->CreateVideoDecoder(input, &desc, &m_DXVAVideoDecoderConfig, ppSurfaces, m_NumSurfaces,
                                                   &decoder);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"-> CreateVideoDecoder failed with hr: %X", hr));
        return E_FAIL;
    }
    m_pDecoder = decoder;
    m_guidDecoderDevice = input;

    /* fill hwaccel_context */
    FillHWContext((dxva_context *)m_pAVCtx->hwaccel_context);

    memset(m_pRawSurface, 0, sizeof(m_pRawSurface));
    for (int i = 0; i < m_NumSurfaces; i++)
    {
        m_pRawSurface[i] = m_pSurfaces[i].d3d;
    }

    return S_OK;
}

HRESULT CDecDXVA2::FillHWContext(dxva_context *ctx)
{
    ctx->cfg = &m_DXVAVideoDecoderConfig;
    ctx->decoder = m_pDecoder;
    ctx->surface = m_pRawSurface;
    ctx->surface_count = m_NumSurfaces;

    if (m_dwVendorId == VEND_ID_INTEL && m_guidDecoderDevice == DXVADDI_Intel_ModeH264_E)
        ctx->workaround = FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO;
    else if (m_dwVendorId == VEND_ID_ATI && IsAMDUVD(m_dwDeviceId))
        ctx->workaround = FF_DXVA2_WORKAROUND_SCALING_LIST_ZIGZAG;
    else
        ctx->workaround = 0;

    return S_OK;
}

enum AVPixelFormat CDecDXVA2::get_dxva2_format(struct AVCodecContext *c, const enum AVPixelFormat *pix_fmts)
{
    CDecDXVA2 *pDec = (CDecDXVA2 *)c->opaque;
    const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++)
    {
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*p);

        if (!desc || !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
            break;

        if (*p == AV_PIX_FMT_DXVA2_VLD)
        {
            HRESULT hr = pDec->ReInitDXVA2Decoder(c);
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

typedef struct SurfaceWrapper
{
    LPDIRECT3DSURFACE9 surface;
    IMediaSample *sample;
    CDecDXVA2 *pDec;
    IDirectXVideoDecoder *pDXDecoder;
} SurfaceWrapper;

void CDecDXVA2::free_dxva2_buffer(void *opaque, uint8_t *data)
{
    SurfaceWrapper *sw = (SurfaceWrapper *)opaque;
    CDecDXVA2 *pDec = sw->pDec;

    LPDIRECT3DSURFACE9 pSurface = sw->surface;
    for (int i = 0; i < pDec->m_NumSurfaces; i++)
    {
        if (pDec->m_pSurfaces[i].d3d == pSurface)
        {
            pDec->m_pSurfaces[i].used = false;
            break;
        }
    }
    SafeRelease(&pSurface);
    SafeRelease(&sw->pDXDecoder);
    SafeRelease(&sw->sample);
    delete sw;
}

HRESULT CDecDXVA2::ReInitDXVA2Decoder(AVCodecContext *c)
{
    HRESULT hr = S_OK;

    // Don't allow decoder creation during first init
    if (m_bInInit)
        return S_FALSE;

    if (!m_pDecoder || dxva_align_dimensions(c->codec_id, c->coded_width) != m_dwSurfaceWidth ||
        dxva_align_dimensions(c->codec_id, c->coded_height) != m_dwSurfaceHeight ||
        m_DecoderPixelFormat != c->sw_pix_fmt)
    {
        DbgLog((LOG_TRACE, 10, L"No DXVA2 Decoder or image dimensions changed -> Re-Allocating resources"));
        if (!m_pDecoder && m_bNative && !m_pDXVA2Allocator)
        {
            ASSERT(0);
            hr = E_FAIL;
        }
        else if (m_bNative)
        {
            // shortcut to update only the Decoder Pixel Format if needed
            if (m_pDecoder && m_DecoderPixelFormat == AV_PIX_FMT_NONE && m_DecoderPixelFormat != c->sw_pix_fmt)
            {
                GUID input;
                D3DFORMAT output;
                FindVideoServiceConversion(c->codec_id, c->profile, get_dxva_surface_format(m_pAVCtx), &input, &output);

                if (output == m_eSurfaceFormat)
                {
                    m_DecoderPixelFormat = c->sw_pix_fmt;
                    return ReInitDXVA2Decoder(c);
                }
            }

            avcodec_flush_buffers(c);

            m_dwSurfaceWidth = dxva_align_dimensions(c->codec_id, c->coded_width);
            m_dwSurfaceHeight = dxva_align_dimensions(c->codec_id, c->coded_height);
            m_DecoderPixelFormat = c->sw_pix_fmt;

            GUID input;
            FindVideoServiceConversion(c->codec_id, c->profile, get_dxva_surface_format(m_pAVCtx), &input, &m_eSurfaceFormat);

            // Re-Commit the allocator (creates surfaces and new decoder)
            hr = m_pDXVA2Allocator->Decommit();
            if (m_pDXVA2Allocator->DecommitInProgress())
            {
                DbgLog((LOG_TRACE, 10, L"WARNING! DXVA2 Allocator is still busy, trying to flush downstream"));
                m_pCallback->ReleaseAllDXVAResources();
                m_pCallback->GetOutputPin()->GetConnected()->BeginFlush();
                m_pCallback->GetOutputPin()->GetConnected()->EndFlush();
                if (m_pDXVA2Allocator->DecommitInProgress())
                {
                    DbgLog(
                        (LOG_TRACE, 10, L"WARNING! Flush had no effect, decommit of the allocator still not complete"));
                }
                else
                {
                    DbgLog((LOG_TRACE, 10, L"Flush was successful, decommit completed!"));
                }
            }
            hr = m_pDXVA2Allocator->Commit();
        }
        else if (!m_bNative)
        {
            FlushDisplayQueue(TRUE);
            hr = CreateDXVA2Decoder();
        }
    }

    return hr;
}

int CDecDXVA2::get_dxva2_buffer(struct AVCodecContext *c, AVFrame *pic, int flags)
{
    CDecDXVA2 *pDec = (CDecDXVA2 *)c->opaque;
    IMediaSample *pSample = nullptr;

    HRESULT hr = S_OK;

    if (pic->format != AV_PIX_FMT_DXVA2_VLD || (c->codec_id == AV_CODEC_ID_H264 && !H264_CHECK_PROFILE(c->profile)) ||
        (c->codec_id == AV_CODEC_ID_HEVC && !HEVC_CHECK_PROFILE(c->profile)) ||
        (c->codec_id == AV_CODEC_ID_VP9 && !VP9_CHECK_PROFILE(c->profile)))
    {
        DbgLog((LOG_ERROR, 10, L"DXVA2 buffer request, but not dxva2 pixfmt or unsupported profile"));
        pDec->m_bFailHWDecode = TRUE;
        return -1;
    }

    hr = pDec->ReInitDXVA2Decoder(c);
    if (FAILED(hr))
    {
        pDec->m_bFailHWDecode = TRUE;
        return -1;
    }

    if (FAILED(pDec->m_pD3DDevMngr->TestDevice(pDec->m_hDevice)))
    {
        DbgLog((LOG_ERROR, 10, L"Device Lost"));
    }

    int i;
    if (pDec->m_bNative)
    {
        if (!pDec->m_pDXVA2Allocator)
            return -1;

        hr = pDec->m_pDXVA2Allocator->GetBuffer(&pSample, nullptr, nullptr, 0);
        if (FAILED(hr))
        {
            DbgLog((LOG_ERROR, 10, L"DXVA2Allocator returned error, hr: 0x%x", hr));
            return -1;
        }

        ILAVDXVA2Sample *pLavDXVA2 = nullptr;
        hr = pSample->QueryInterface(&pLavDXVA2);
        if (FAILED(hr))
        {
            DbgLog((LOG_ERROR, 10, L"Sample is no LAV DXVA2 sample?????"));
            SafeRelease(&pSample);
            return -1;
        }
        i = pLavDXVA2->GetDXSurfaceId();
        SafeRelease(&pLavDXVA2);
    }
    else
    {
        int old, old_unused;
        for (i = 0, old = 0, old_unused = -1; i < pDec->m_NumSurfaces; i++)
        {
            d3d_surface_t *surface = &pDec->m_pSurfaces[i];
            if (!surface->used && (old_unused == -1 || surface->age < pDec->m_pSurfaces[old_unused].age))
                old_unused = i;
            if (surface->age < pDec->m_pSurfaces[old].age)
                old = i;
        }
        if (old_unused == -1)
        {
            DbgLog((LOG_TRACE, 10, L"No free surface, using oldest"));
            i = old;
        }
        else
        {
            i = old_unused;
        }
    }

    LPDIRECT3DSURFACE9 pSurface = pDec->m_pSurfaces[i].d3d;
    if (!pSurface)
    {
        DbgLog((LOG_ERROR, 10, L"There is a sample, but no D3D Surace? WTF?"));
        SafeRelease(&pSample);
        return -1;
    }

    pDec->m_pSurfaces[i].age = pDec->m_CurrentSurfaceAge++;
    pDec->m_pSurfaces[i].used = true;

    memset(pic->data, 0, sizeof(pic->data));
    memset(pic->linesize, 0, sizeof(pic->linesize));
    memset(pic->buf, 0, sizeof(pic->buf));

    pic->data[0] = pic->data[3] = (uint8_t *)pSurface;
    pic->data[4] = (uint8_t *)pSample;

    SurfaceWrapper *surfaceWrapper = new SurfaceWrapper();
    surfaceWrapper->pDec = pDec;
    surfaceWrapper->sample = pSample;
    surfaceWrapper->surface = pSurface;
    surfaceWrapper->surface->AddRef();
    surfaceWrapper->pDXDecoder = pDec->m_pDecoder;
    surfaceWrapper->pDXDecoder->AddRef();

    pic->buf[0] = av_buffer_create(nullptr, 0, free_dxva2_buffer, surfaceWrapper, 0);

    return 0;
}

HRESULT CDecDXVA2::AdditionaDecoderInit()
{
    /* Create ffmpeg dxva_context, but only fill it if we have a decoder already. */
    dxva_context *ctx = (dxva_context *)av_mallocz(sizeof(dxva_context));

    if (m_pDecoder)
    {
        FillHWContext(ctx);
    }

    m_pAVCtx->thread_count = 1;
    m_pAVCtx->thread_type = 0;
    m_pAVCtx->hwaccel_context = ctx;
    m_pAVCtx->get_format = get_dxva2_format;
    m_pAVCtx->get_buffer2 = get_dxva2_buffer;
    m_pAVCtx->opaque = this;

    m_pAVCtx->slice_flags |= SLICE_FLAG_ALLOW_FIELD;

    // disable error conealment in hwaccel mode, it doesn't work either way
    m_pAVCtx->error_concealment = 0;
    av_opt_set_int(m_pAVCtx, "enable_er", 0, AV_OPT_SEARCH_CHILDREN);

    return S_OK;
}

HRESULT CDecDXVA2::PostDecode()
{
    if (m_bFailHWDecode)
    {
        DbgLog((LOG_TRACE, 10, L"::PostDecode(): HW Decoder failed, falling back to software decoding"));
        return E_FAIL;
    }
    return S_OK;
}

STDMETHODIMP CDecDXVA2::FlushFromAllocator()
{
    if (m_pAVCtx && avcodec_is_open(m_pAVCtx))
        avcodec_flush_buffers(m_pAVCtx);
    FlushDisplayQueue(FALSE);

    return S_OK;
}

STDMETHODIMP CDecDXVA2::Flush()
{
    CDecAvcodec::Flush();
    FlushDisplayQueue(FALSE);

#ifdef DEBUG
    int used = 0;
    for (int i = 0; i < m_NumSurfaces; i++)
    {
        d3d_surface_t *s = &m_pSurfaces[i];
        if (s->used)
        {
            used++;
        }
    }
    if (used > 0)
    {
        DbgLog((LOG_TRACE, 10, L"WARNING! %d frames still in use after flush", used));
    }
#endif

    // This solves an issue with corruption after seeks on AMD systems, see JIRA LAV-5
    if (m_dwVendorId == VEND_ID_ATI && m_nCodecId == AV_CODEC_ID_H264 && m_pDecoder)
    {
        if (m_bNative && m_pDXVA2Allocator)
        {
            // The allocator needs to be locked because flushes can happen async to other graph events
            // and in the worst case the allocator is decommited while we're using it.
            CAutoLock allocatorLock(m_pDXVA2Allocator);
            if (m_pDXVA2Allocator->IsCommited())
                CreateDXVA2Decoder(m_NumSurfaces, m_pRawSurface);
        }
        else if (!m_bNative)
            CreateDXVA2Decoder();
    }

    return S_OK;
}

STDMETHODIMP CDecDXVA2::FlushDisplayQueue(BOOL bDeliver)
{
    for (int i = 0; i < m_DisplayDelay; ++i)
    {
        if (m_FrameQueue[m_FrameQueuePosition])
        {
            if (bDeliver)
            {
                DeliverDXVA2Frame(m_FrameQueue[m_FrameQueuePosition]);
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

STDMETHODIMP CDecDXVA2::EndOfStream()
{
    CDecAvcodec::EndOfStream();

    // Flush display queue
    FlushDisplayQueue(TRUE);

    return S_OK;
}

HRESULT CDecDXVA2::HandleDXVA2Frame(LAVFrame *pFrame)
{
    if (pFrame->flags & LAV_FRAME_FLAG_FLUSH)
    {
        if (!m_bNative)
        {
            FlushDisplayQueue(TRUE);
        }
        Deliver(pFrame);
        return S_OK;
    }
    if (m_bNative || m_DisplayDelay == 0)
    {
        DeliverDXVA2Frame(pFrame);
    }
    else
    {
        LAVFrame *pQueuedFrame = m_FrameQueue[m_FrameQueuePosition];
        m_FrameQueue[m_FrameQueuePosition] = pFrame;

        m_FrameQueuePosition = (m_FrameQueuePosition + 1) % m_DisplayDelay;

        if (pQueuedFrame)
        {
            DeliverDXVA2Frame(pQueuedFrame);
        }
    }

    return S_OK;
}

HRESULT CDecDXVA2::DeliverDXVA2Frame(LAVFrame *pFrame)
{
    if (m_bNative)
    {
        if (!pFrame->data[0] || !pFrame->data[3])
        {
            DbgLog((LOG_ERROR, 10, L"No sample or surface for DXVA2 frame?!?!"));
            ReleaseFrame(&pFrame);
            return S_FALSE;
        }

        GetPixelFormat(&pFrame->format, &pFrame->bpp);
        Deliver(pFrame);
    }
    else
    {
        if (m_bDirect)
        {
            DeliverDirect(pFrame);
        }
        else
        {
            if (CopyFrame(pFrame))
                Deliver(pFrame);
            else
                ReleaseFrame(&pFrame);
        }
    }

    return S_OK;
}

STDMETHODIMP CDecDXVA2::GetPixelFormat(LAVPixelFormat *pPix, int *pBpp)
{
    // Output is always NV12 or P010
    if (pPix)
    {
        if (m_bNative)
            *pPix = LAVPixFmt_DXVA2;
        else
            *pPix =
                (m_eSurfaceFormat == FOURCC_P010 || m_eSurfaceFormat == FOURCC_P016) ? LAVPixFmt_P016 : LAVPixFmt_NV12;
    }
    if (pBpp)
        *pBpp = (m_eSurfaceFormat == FOURCC_P016) ? 16 : ((m_eSurfaceFormat == FOURCC_P010) ? 10 : 8);
    return S_OK;
}

__forceinline bool CDecDXVA2::CopyFrame(LAVFrame *pFrame)
{
    HRESULT hr;
    LPDIRECT3DSURFACE9 pSurface = (LPDIRECT3DSURFACE9)pFrame->data[3];
    GetPixelFormat(&pFrame->format, &pFrame->bpp);

    D3DSURFACE_DESC surfaceDesc;
    pSurface->GetDesc(&surfaceDesc);

    D3DLOCKED_RECT LockedRect;
    hr = pSurface->LockRect(&LockedRect, nullptr, D3DLOCK_READONLY);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"pSurface->LockRect failed (hr: %X)", hr));
        return false;
    }

    // Store AVFrame-based buffers, and reset pFrame
    LAVFrame tmpFrame = *pFrame;
    pFrame->destruct = nullptr;
    pFrame->priv_data = nullptr;

    // side-data shall not be copied to tmpFrame
    tmpFrame.side_data = nullptr;
    tmpFrame.side_data_count = 0;

    // Allocate memory buffers
    hr = AllocLAVFrameBuffers(pFrame, (pFrame->format == LAVPixFmt_P016) ? (LockedRect.Pitch >> 1) : LockedRect.Pitch);
    if (FAILED(hr))
    {
        pSurface->UnlockRect();
        *pFrame = tmpFrame;
        return false;
    }

    // Copy surface onto memory buffers

    const uint8_t *src[4] = {(const uint8_t *)LockedRect.pBits, ((const uint8_t *)LockedRect.pBits) + (LockedRect.Pitch * surfaceDesc.Height), 0, 0};
    const ptrdiff_t src_pitch[4] = {LockedRect.Pitch, LockedRect.Pitch, 0, 0};
    av_image_copy_uc_from(pFrame->data, pFrame->stride, src, src_pitch, (pFrame->format == LAVPixFmt_P016) ? AV_PIX_FMT_P010 : AV_PIX_FMT_NV12, pFrame->width, pFrame->height);

    pSurface->UnlockRect();

    // Free AVFrame based buffers, now that we're done
    FreeLAVFrameBuffers(&tmpFrame);

    return true;
}

static bool direct_lock(LAVFrame *pFrame, LAVDirectBuffer *pBuffer)
{
    ASSERT(pFrame && pBuffer);

    HRESULT hr;
    LPDIRECT3DSURFACE9 pSurface = (LPDIRECT3DSURFACE9)pFrame->data[3];

    D3DSURFACE_DESC surfaceDesc;
    pSurface->GetDesc(&surfaceDesc);

    D3DLOCKED_RECT LockedRect;
    hr = pSurface->LockRect(&LockedRect, nullptr, D3DLOCK_READONLY);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"pSurface->LockRect failed (hr: %X)", hr));
        return false;
    }

    memset(pBuffer, 0, sizeof(*pBuffer));

    pBuffer->data[0] = (BYTE *)LockedRect.pBits;
    pBuffer->data[1] = pBuffer->data[0] + surfaceDesc.Height * LockedRect.Pitch;

    pBuffer->stride[0] = LockedRect.Pitch;
    pBuffer->stride[1] = LockedRect.Pitch;
    return true;
}

static void direct_unlock(LAVFrame *pFrame)
{
    ASSERT(pFrame);
    LPDIRECT3DSURFACE9 pSurface = (LPDIRECT3DSURFACE9)pFrame->data[3];
    pSurface->UnlockRect();
}

bool CDecDXVA2::DeliverDirect(LAVFrame *pFrame)
{
    GetPixelFormat(&pFrame->format, &pFrame->bpp);
    pFrame->direct = true;
    pFrame->direct_lock = direct_lock;
    pFrame->direct_unlock = direct_unlock;

    Deliver(pFrame);

    return true;
}

STDMETHODIMP_(DWORD) CDecDXVA2::GetHWAccelNumDevices()
{
    if (m_bNative)
        return 0;

    if (!m_pD3D)
        return 0;

    return m_pD3D->GetAdapterCount();
}

STDMETHODIMP CDecDXVA2::GetHWAccelDeviceInfo(DWORD dwIndex, BSTR *pstrDeviceName, DWORD *dwDeviceIdentifier)
{
    if (m_bNative)
        return E_UNEXPECTED;

    if (!m_pD3D)
        return E_NOINTERFACE;

    D3DADAPTER_IDENTIFIER9 d3dai = {0};
    HRESULT err = m_pD3D->GetAdapterIdentifier(dwIndex, 0, &d3dai);
    if (err != D3D_OK)
        return E_INVALIDARG;

    if (pstrDeviceName)
    {
        int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, d3dai.Description, -1, nullptr, 0);
        if (len == 0)
            return E_FAIL;

        *pstrDeviceName = SysAllocStringLen(nullptr, len);
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, d3dai.Description, -1, *pstrDeviceName, len);
    }

    if (dwDeviceIdentifier)
        *dwDeviceIdentifier = d3dai.DeviceId;

    return S_OK;
}

STDMETHODIMP CDecDXVA2::GetHWAccelActiveDevice(BSTR *pstrDeviceName)
{
    CheckPointer(pstrDeviceName, E_POINTER);

    if (strlen(m_cDeviceName) == 0)
        return E_UNEXPECTED;

    int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, m_cDeviceName, -1, nullptr, 0);
    if (len == 0)
        return E_FAIL;

    *pstrDeviceName = SysAllocStringLen(nullptr, len);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, m_cDeviceName, -1, *pstrDeviceName, len);
    return S_OK;
}
