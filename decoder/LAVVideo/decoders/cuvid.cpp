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

#include "stdafx.h"
#include "cuvid.h"

#include "moreuuids.h"

#include "parsers/H264SequenceParser.h"
#include "parsers/MPEG2HeaderParser.h"
#include "parsers/VC1HeaderParser.h"
#include "parsers/HEVCSequenceParser.h"

#include "Media.h"

////////////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////////////

ILAVDecoder *CreateDecoderCUVID() {
  return new CDecCuvid();
}

////////////////////////////////////////////////////////////////////////////////
// CUVID codec map
////////////////////////////////////////////////////////////////////////////////

static struct {
  AVCodecID ffcodec;
  cudaVideoCodec cudaCodec;
} cuda_codecs[] = {
  { AV_CODEC_ID_MPEG1VIDEO, cudaVideoCodec_MPEG1 },
  { AV_CODEC_ID_MPEG2VIDEO, cudaVideoCodec_MPEG2 },
  { AV_CODEC_ID_VC1,        cudaVideoCodec_VC1   },
  { AV_CODEC_ID_H264,       cudaVideoCodec_H264  },
  { AV_CODEC_ID_MPEG4,      cudaVideoCodec_MPEG4 },
  { AV_CODEC_ID_HEVC,       cudaVideoCodec_HEVC  },
  { AV_CODEC_ID_VP9,        cudaVideoCodec_VP9   },
};

////////////////////////////////////////////////////////////////////////////////
// Compatibility tables
////////////////////////////////////////////////////////////////////////////////

#define LEVEL_C_LOW_LIMIT 0x0A20

static DWORD LevelCBlacklist[] = {
  0x0A22, 0x0A67,     // Geforce 315, no VDPAU at all
  0x0A68, 0x0A69,     // Geforce G105M, only B
  0x0CA0, 0x0CA7,     // Geforce GT 330, only A
  0x0CAC,             // Geforce GT 220, no VDPAU
  0x10C3              // Geforce 8400GS, only A
};

static DWORD LevelCWhitelist[] = {
  0x06C0,             // Geforce GTX 480
  0x06C4,             // Geforce GTX 465
  0x06CA,             // Geforce GTX 480M
  0x06CD,             // Geforce GTX 470
  0x08A5,             // Geforce 320M

  0x06D8, 0x06DC,     // Quadro 6000
  0x06D9,             // Quadro 5000
  0x06DA,             // Quadro 5000M
  0x06DD,             // Quadro 4000

  0x06D1,             // Tesla C2050 / C2070
  0x06D2,             // Tesla M2070
  0x06DE,             // Tesla T20 Processor
  0x06DF,             // Tesla M2070-Q
};

static BOOL IsLevelC(DWORD deviceId)
{
  int idx = 0;
  if (deviceId >= LEVEL_C_LOW_LIMIT) {
    for(idx = 0; idx < countof(LevelCBlacklist); idx++) {
      if (LevelCBlacklist[idx] == deviceId)
        return FALSE;
    }
    return TRUE;
  } else {
    for(idx = 0; idx < countof(LevelCWhitelist); idx++) {
      if (LevelCWhitelist[idx] == deviceId)
        return TRUE;
    }
    return FALSE;
  }
}

////////////////////////////////////////////////////////////////////////////////
// CUVID decoder implementation
////////////////////////////////////////////////////////////////////////////////

CDecCuvid::CDecCuvid(void)
  : CDecBase()
{
  ZeroMemory(&cuda, sizeof(cuda));
  ZeroMemory(&m_VideoFormat, sizeof(m_VideoFormat));
  ZeroMemory(&m_DXVAExtendedFormat, sizeof(m_DXVAExtendedFormat));
  ZeroMemory(&m_VideoDecoderInfo, sizeof(m_VideoDecoderInfo));
}

CDecCuvid::~CDecCuvid(void)
{
  DestroyDecoder(true);
}

STDMETHODIMP CDecCuvid::DestroyDecoder(bool bFull)
{
  if (m_cudaCtxLock) cuda.cuvidCtxLock(m_cudaCtxLock, 0);

  if (m_AnnexBConverter) {
    SAFE_DELETE(m_AnnexBConverter);
  }

  if (m_hDecoder) {
    cuda.cuvidDestroyDecoder(m_hDecoder);
    m_hDecoder = 0;
  }

  if (m_hParser) {
    cuda.cuvidDestroyVideoParser(m_hParser);
    m_hParser = 0;
  }

  if (m_pbRawNV12) {
    cuda.cuMemFreeHost(m_pbRawNV12);
    m_pbRawNV12 = nullptr;
    m_cRawNV12 = 0;
  }

  if (m_cudaCtxLock) cuda.cuvidCtxUnlock(m_cudaCtxLock, 0);

  if(bFull) {
    if (m_cudaCtxLock) {
      cuda.cuvidCtxLockDestroy(m_cudaCtxLock);
      m_cudaCtxLock = 0;
    }

    if (m_cudaContext) {
      cuda.cuCtxDestroy(m_cudaContext);
      m_cudaContext = 0;
    }

    SafeRelease(&m_pD3DDevice9);
    SafeRelease(&m_pD3D9);

    FreeLibrary(cuda.cudaLib);
    FreeLibrary(cuda.cuvidLib);

    ZeroMemory(&cuda, sizeof(cuda));
  }

  return S_OK;
}

#define STRINGIFY(X) #X

#define GET_PROC_EX_OPT(name, lib) \
  cuda.name = (t##name *)GetProcAddress(lib, #name);

#define GET_PROC_EX(name, lib)                          \
  GET_PROC_EX_OPT(name, lib)                            \
  if (cuda.name == nullptr) {                           \
    DbgLog((LOG_ERROR, 10, L"-> Failed to load function \"%s\"", TEXT(#name))); \
    return E_FAIL; \
  }

#define GET_PROC_EX_OPT_V2(name, lib) \
  cuda.name = (t##name *)GetProcAddress(lib, STRINGIFY(name##_v2)); \

#define GET_PROC_EX_V2(name, lib)                       \
  GET_PROC_EX_OPT_V2(name, lib)                         \
  if (cuda.name == nullptr) {                           \
    DbgLog((LOG_ERROR, 10, L"-> Failed to load function \"%s\"", TEXT(STRINGIFY(name##_v2)))); \
    return E_FAIL; \
  }

#define GET_PROC_CUDA(name) GET_PROC_EX(name, cuda.cudaLib)
#define GET_PROC_CUDA_V2(name) GET_PROC_EX_V2(name, cuda.cudaLib)
#define GET_PROC_CUVID(name) GET_PROC_EX(name, cuda.cuvidLib)
#define GET_PROC_CUVID_V2(name) GET_PROC_EX_V2(name, cuda.cuvidLib)


STDMETHODIMP CDecCuvid::LoadCUDAFuncRefs()
{
  // Load CUDA functions
  cuda.cudaLib = LoadLibrary(L"nvcuda.dll");
  if (cuda.cudaLib == nullptr) {
    DbgLog((LOG_TRACE, 10, L"-> Loading nvcuda.dll failed"));
    return E_FAIL;
  }

  GET_PROC_CUDA(cuInit);
  GET_PROC_CUDA_V2(cuCtxCreate);
  GET_PROC_CUDA_V2(cuCtxDestroy);
  GET_PROC_CUDA_V2(cuCtxPushCurrent);
  GET_PROC_CUDA_V2(cuCtxPopCurrent);
  GET_PROC_CUDA_V2(cuD3D9CtxCreate);
  GET_PROC_CUDA_V2(cuMemAllocHost);
  GET_PROC_CUDA(cuMemFreeHost);
  GET_PROC_CUDA_V2(cuMemcpyDtoH);
  GET_PROC_CUDA(cuDeviceGetCount);
  GET_PROC_CUDA(cuDriverGetVersion);
  GET_PROC_CUDA(cuDeviceGetName);
  GET_PROC_CUDA(cuDeviceComputeCapability);
  GET_PROC_CUDA(cuDeviceGetAttribute);

  // Load CUVID function
  cuda.cuvidLib = LoadLibrary(L"nvcuvid.dll");
  if (cuda.cuvidLib == nullptr) {
    DbgLog((LOG_TRACE, 10, L"-> Loading nvcuvid.dll failed"));
    return E_FAIL;
  }

  GET_PROC_CUVID(cuvidCtxLockCreate);
  GET_PROC_CUVID(cuvidCtxLockDestroy);
  GET_PROC_CUVID(cuvidCtxLock);
  GET_PROC_CUVID(cuvidCtxUnlock);
  GET_PROC_CUVID(cuvidCreateVideoParser);
  GET_PROC_CUVID(cuvidParseVideoData);
  GET_PROC_CUVID(cuvidDestroyVideoParser);
  GET_PROC_CUVID(cuvidCreateDecoder);
  GET_PROC_CUVID(cuvidDecodePicture);
  GET_PROC_CUVID(cuvidDestroyDecoder);
#ifdef _M_AMD64
  GET_PROC_CUVID(cuvidMapVideoFrame64);
  GET_PROC_CUVID(cuvidUnmapVideoFrame64);
  cuda.cuvidMapVideoFrame = cuda.cuvidMapVideoFrame64;
  cuda.cuvidUnmapVideoFrame = cuda.cuvidUnmapVideoFrame64;
#else
  GET_PROC_CUVID(cuvidMapVideoFrame);
  GET_PROC_CUVID(cuvidUnmapVideoFrame);
#endif

  return S_OK;
}

STDMETHODIMP CDecCuvid::FlushParser()
{
  CUVIDSOURCEDATAPACKET pCuvidPacket;
  memset(&pCuvidPacket, 0, sizeof(pCuvidPacket));

  pCuvidPacket.flags |= CUVID_PKT_ENDOFSTREAM;
  CUresult result = CUDA_SUCCESS;

  cuda.cuvidCtxLock(m_cudaCtxLock, 0);
  __try {
    result = cuda.cuvidParseVideoData(m_hParser, &pCuvidPacket);
  } __except (1) {
    DbgLog((LOG_ERROR, 10, L"cuvidFlushParser(): cuvidParseVideoData threw an exception"));
    result = CUDA_ERROR_UNKNOWN;
  }
  cuda.cuvidCtxUnlock(m_cudaCtxLock, 0);

  return result;
}

// Beginning of GPU Architecture definitions
static int _ConvertSMVer2CoresDrvApi(int major, int minor)
{
  // Defines for GPU Architecture types (using the SM version to determine the # of cores per SM
  typedef struct {
    int SM; // 0xMm (hexidecimal notation), M = SM Major version, and m = SM minor version
    int Cores;
  } sSMtoCores;

  sSMtoCores nGpuArchCoresPerSM[] =
  {
    { 0x10,   8 },
    { 0x11,   8 },
    { 0x12,   8 },
    { 0x13,   8 },
    { 0x20,  32 },
    { 0x21,  48 },
    { 0x30, 192 },
    { 0x32, 192 },
    { 0x35, 192 },
    { 0x37, 192 },
    { 0x50, 128 },
    { 0x52, 128 },
    { 0x61, 128 },
    {   -1,  -1 }
  };

  int index = 0;
  while (nGpuArchCoresPerSM[index].SM != -1) {
    if (nGpuArchCoresPerSM[index].SM == ((major << 4) + minor) ) {
      return nGpuArchCoresPerSM[index].Cores;
    }
    index++;
  }
  DbgLog((LOG_ERROR, 10, L"MapSMtoCores undefined SMversion %d.%d!", major, minor));
  return -1;
}

int CDecCuvid::GetMaxGflopsGraphicsDeviceId()
{
  CUdevice current_device = 0, max_perf_device = 0;
  int device_count     = 0, sm_per_multiproc = 0;
  int best_SM_arch     = 0;
  int64_t max_compute_perf = 0;
  int major = 0, minor = 0, multiProcessorCount, clockRate;
  int bTCC = 0, version;
  char deviceName[256];

  cuda.cuDeviceGetCount(&device_count);
  if (device_count <= 0)
    return -1;

  cuda.cuDriverGetVersion(&version);

  // Find the best major SM Architecture GPU device that are graphics devices
  while ( current_device < device_count ) {
    cuda.cuDeviceGetName(deviceName, 256, current_device);
    cuda.cuDeviceComputeCapability(&major, &minor, current_device);

    if (version >= 3020) {
      cuda.cuDeviceGetAttribute(&bTCC, CU_DEVICE_ATTRIBUTE_TCC_DRIVER, current_device);
    } else {
      // Assume a Tesla GPU is running in TCC if we are running CUDA 3.1
      if (deviceName[0] == 'T') bTCC = 1;
    }
    if (!bTCC) {
      if (major > 0 && major < 9999) {
        best_SM_arch = max(best_SM_arch, major);
      }
    }
    current_device++;
  }

  // Find the best CUDA capable GPU device
  current_device = 0;
  while( current_device < device_count ) {
    cuda.cuDeviceGetAttribute(&multiProcessorCount, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, current_device);
    cuda.cuDeviceGetAttribute(&clockRate, CU_DEVICE_ATTRIBUTE_CLOCK_RATE, current_device);
    cuda.cuDeviceComputeCapability(&major, &minor, current_device);

    if (version >= 3020) {
      cuda.cuDeviceGetAttribute(&bTCC, CU_DEVICE_ATTRIBUTE_TCC_DRIVER, current_device);
    } else {
      // Assume a Tesla GPU is running in TCC if we are running CUDA 3.1
      if (deviceName[0] == 'T') bTCC = 1;
    }

    if (major == 9999 && minor == 9999) {
      sm_per_multiproc = 1;
    } else {
      sm_per_multiproc = _ConvertSMVer2CoresDrvApi(major, minor);
    }

    // If this is a Tesla based GPU and SM 2.0, and TCC is disabled, this is a contendor
    if (!bTCC) // Is this GPU running the TCC driver?  If so we pass on this
    {
      int64_t compute_perf = int64_t(multiProcessorCount * sm_per_multiproc) * clockRate;
      if(compute_perf > max_compute_perf) {
        // If we find GPU with SM major > 2, search only these
        if (best_SM_arch > 2) {
          // If our device = dest_SM_arch, then we pick this one
          if (major == best_SM_arch) {
            max_compute_perf  = compute_perf;
            max_perf_device   = current_device;
          }
        } else {
          max_compute_perf  = compute_perf;
          max_perf_device   = current_device;
        }
      }

#ifdef DEBUG
      cuda.cuDeviceGetName(deviceName, 256, current_device);
      DbgLog((LOG_TRACE, 10, L"CUDA Device (%d): %S, Compute: %d.%d, CUDA Cores: %d, Clock: %d MHz", current_device, deviceName, major, minor, multiProcessorCount * sm_per_multiproc, clockRate / 1000));
#endif
    }
    ++current_device;
  }
  return max_perf_device;
}

// ILAVDecoder
STDMETHODIMP CDecCuvid::Init()
{
  DbgLog((LOG_TRACE, 10, L"CDecCuvid::Init(): Trying to open CUVID device"));
  HRESULT hr = S_OK;
  CUresult cuStatus = CUDA_SUCCESS;

  hr = LoadCUDAFuncRefs();
  if (FAILED(hr)) {
    DbgLog((LOG_ERROR, 10, L"-> Loading CUDA interfaces failed (hr: 0x%x)", hr));
    return hr;
  }

  cuStatus = cuda.cuInit(0);
  if (cuStatus != CUDA_SUCCESS) {
    DbgLog((LOG_ERROR, 10, L"-> cuInit failed (status: %d)", cuStatus));
    return E_FAIL;
  }

  // TODO: select best device
  int best_device = GetMaxGflopsGraphicsDeviceId();

  DWORD dwDeviceIndex = m_pCallback->GetGPUDeviceIndex();
  if (dwDeviceIndex != DWORD_MAX) {
    best_device = (int)dwDeviceIndex;
  }

select_device:
  hr = InitD3D9(best_device, dwDeviceIndex);

  if (hr != S_OK) {
    if (FAILED(hr)) {
      DbgLog((LOG_TRACE, 10, L"-> No D3D device available, building non-D3D context on device %d", best_device));
    }
    cuStatus = cuda.cuCtxCreate(&m_cudaContext, CU_CTX_SCHED_BLOCKING_SYNC, best_device);

    if (cuStatus == CUDA_SUCCESS) {
      int major, minor;
      cuda.cuDeviceComputeCapability(&major, &minor, best_device);
      m_bVDPAULevelC = (major >= 2);
      cuda.cuDeviceGetName(m_cudaDeviceName, sizeof(m_cudaDeviceName), best_device);
      DbgLog((LOG_TRACE, 10, L"InitCUDA(): pure CUDA context of device with compute %d.%d", major, minor));
    }
  }

  if (cuStatus == CUDA_ERROR_INVALID_DEVICE && dwDeviceIndex != DWORD_MAX) {
    DbgLog((LOG_TRACE, 10, L"-> Specific device was requested, but no match was found, re-trying automatic selection"));
    dwDeviceIndex = DWORD_MAX;
    best_device = GetMaxGflopsGraphicsDeviceId();
    goto select_device;
  }

  if (cuStatus == CUDA_SUCCESS) {
    // Switch to a floating context
    CUcontext curr_ctx = nullptr;
    cuStatus = cuda.cuCtxPopCurrent(&curr_ctx);
    if (cuStatus != CUDA_SUCCESS) {
      DbgLog((LOG_ERROR, 10, L"-> Storing context on the stack failed with error %d", cuStatus));
      return E_FAIL;
    }
    cuStatus = cuda.cuvidCtxLockCreate(&m_cudaCtxLock, m_cudaContext);
    if (cuStatus != CUDA_SUCCESS) {
      DbgLog((LOG_ERROR, 10, L"-> Creation of floating context failed with error %d", cuStatus));
      return E_FAIL;
    }
  } else {
    DbgLog((LOG_TRACE, 10, L"-> Creation of CUDA context failed with error %d", cuStatus));
    return E_FAIL;
  }

  return S_OK;
}

STDMETHODIMP CDecCuvid::InitD3D9(int best_device, DWORD requested_device)
{
  HRESULT hr = S_OK;
  CUresult cuStatus = CUDA_SUCCESS;
  int device = 0;

  if (IsWindows10OrNewer()) {
    DbgLog((LOG_ERROR, 10, L"-> D3D9 CUVID interop is not supported on Windows 10"));
    return E_FAIL;
  }

  // Check if D3D mode is enabled/wanted
  if (m_pSettings->GetHWAccelDeintHQ() == FALSE) {
    DbgLog((LOG_ERROR, 10, L"-> HQ mode is turned off, skipping D3D9 init"));
    return S_FALSE;
  }

  if (!m_pD3D9)
    m_pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);

  if (!m_pD3D9) {
    DbgLog((LOG_ERROR, 10, L"-> Failed to acquire IDirect3D9"));
    return E_FAIL;
  }

  D3DADAPTER_IDENTIFIER9 d3dId;
  D3DPRESENT_PARAMETERS d3dpp;
  D3DDISPLAYMODE d3ddm;
  for (unsigned lAdapter = 0; lAdapter < m_pD3D9->GetAdapterCount(); lAdapter++) {
    DbgLog((LOG_TRACE, 10, L"-> Trying D3D Adapter %d..", lAdapter));

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    m_pD3D9->GetAdapterDisplayMode(lAdapter, &d3ddm);

    d3dpp.Windowed               = TRUE;
    d3dpp.BackBufferWidth        = 640;
    d3dpp.BackBufferHeight       = 480;
    d3dpp.BackBufferCount        = 1;
    d3dpp.BackBufferFormat       = d3ddm.Format;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    d3dpp.Flags                  = D3DPRESENTFLAG_VIDEO;

    IDirect3DDevice9 *pDev = nullptr;
    CUcontext cudaCtx = 0;
    hr = m_pD3D9->CreateDevice(lAdapter, D3DDEVTYPE_HAL, GetShellWindow(), D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE, &d3dpp, &pDev);
    if (SUCCEEDED(hr)) {
      m_pD3D9->GetAdapterIdentifier(lAdapter, 0, &d3dId);
      cuStatus = cuda.cuD3D9CtxCreate(&cudaCtx, &device, CU_CTX_SCHED_BLOCKING_SYNC, pDev);
      if (cuStatus == CUDA_SUCCESS) {
        DbgLog((LOG_TRACE, 10, L"-> Created D3D Device on adapter %S (%d), using CUDA device %d", d3dId.Description, lAdapter, device));

        BOOL isLevelC = IsLevelC(d3dId.DeviceId);
        DbgLog((LOG_TRACE, 10, L"InitCUDA(): D3D Device with Id 0x%x is level C: %d", d3dId.DeviceId, isLevelC));

        if (m_bVDPAULevelC && !isLevelC) {
          DbgLog((LOG_TRACE, 10, L"InitCUDA(): We already had a Level C+ device, this one is not, skipping"));
          cuda.cuCtxDestroy(cudaCtx);
          SafeRelease(&pDev);
          continue;
        }

        // Release old resources
        SafeRelease(&m_pD3DDevice9);
        if (m_cudaContext)
          cuda.cuCtxDestroy(m_cudaContext);

        // Store resources
        m_pD3DDevice9 = pDev;
        m_cudaContext = cudaCtx;
        m_bVDPAULevelC = isLevelC;
        cuda.cuDeviceGetName(m_cudaDeviceName, sizeof(m_cudaDeviceName), best_device);
        // Is this the one we want?
        if (device == best_device)
          break;
      }
      else {
        DbgLog((LOG_TRACE, 10, L"-> D3D Device on adapter %d is not CUDA capable", lAdapter));
        SafeRelease(&pDev);
      }
    }
  }

  if (requested_device != DWORD_MAX && device != best_device) {
    DbgLog((LOG_ERROR, 10, L"-> No D3D Device found matching the requested device"));
    SafeRelease(&m_pD3DDevice9);
    SafeRelease(&m_pD3D9);
    if (m_cudaContext) {
      cuda.cuCtxDestroy(m_cudaContext);
      m_cudaContext = 0;
    }

    return E_FAIL;
  }

  if (!m_pD3DDevice9) {
    SafeRelease(&m_pD3D9);
    return E_FAIL;
  }

  return S_OK;
}

STDMETHODIMP CDecCuvid::InitDecoder(AVCodecID codec, const CMediaType *pmt)
{
  DbgLog((LOG_TRACE, 10, L"CDecCuvid::InitDecoder(): Initializing CUVID decoder"));
  HRESULT hr = S_OK;

  if (!m_cudaContext) {
    DbgLog((LOG_TRACE, 10, L"-> InitDecoder called without a cuda context"));
    return E_FAIL;
  }

  // Free old device
  DestroyDecoder(false);

  // Flush Display Queue
  memset(&m_DisplayQueue, 0, sizeof(m_DisplayQueue));
  for (int i=0; i<DISPLAY_DELAY; i++)
    m_DisplayQueue[i].picture_index = -1;
  m_DisplayPos = 0;

  m_DisplayDelay = DISPLAY_DELAY;

  // Reduce display delay for DVD decoding for lower decode latency
  if (m_pCallback->GetDecodeFlags() & LAV_VIDEO_DEC_FLAG_DVD)
    m_DisplayDelay /= 2;

  cudaVideoCodec cudaCodec = (cudaVideoCodec)-1;
  for (int i = 0; i < countof(cuda_codecs); i++) {
    if (cuda_codecs[i].ffcodec == codec) {
      cudaCodec = cuda_codecs[i].cudaCodec;
      break;
    }
  }

  if (cudaCodec == -1) {
    DbgLog((LOG_TRACE, 10, L"-> Codec id %d does not map to a CUVID codec", codec));
    return E_FAIL;
  }

  if (cudaCodec == cudaVideoCodec_MPEG4 && !m_bVDPAULevelC) {
    DbgLog((LOG_TRACE, 10, L"-> Device is not capable to decode this format (not >= Level C)"));
    return E_FAIL;
  }

  m_bUseTimestampQueue = (m_pCallback->GetDecodeFlags() & LAV_VIDEO_DEC_FLAG_ONLY_DTS)
                      || (cudaCodec == cudaVideoCodec_MPEG4 && pmt->formattype != FORMAT_MPEG2Video);
  m_bWaitForKeyframe = m_bUseTimestampQueue;
  m_bInterlaced = TRUE;
  m_bFormatIncompatible = FALSE;
  m_bTFF = TRUE;
  m_rtPrevDiff = AV_NOPTS_VALUE;
  m_bARPresent = TRUE;

  // Create the CUDA Video Parser
  CUVIDPARSERPARAMS oVideoParserParameters;
  ZeroMemory(&oVideoParserParameters, sizeof(CUVIDPARSERPARAMS));
  oVideoParserParameters.CodecType              = cudaCodec;
  oVideoParserParameters.ulMaxNumDecodeSurfaces = MAX_DECODE_FRAMES;
  oVideoParserParameters.ulMaxDisplayDelay      = m_DisplayDelay;
  oVideoParserParameters.pUserData              = this;
  oVideoParserParameters.pfnSequenceCallback    = CDecCuvid::HandleVideoSequence;    // Called before decoding frames and/or whenever there is a format change
  oVideoParserParameters.pfnDecodePicture       = CDecCuvid::HandlePictureDecode;    // Called when a picture is ready to be decoded (decode order)
  oVideoParserParameters.pfnDisplayPicture      = CDecCuvid::HandlePictureDisplay;   // Called whenever a picture is ready to be displayed (display order)
  oVideoParserParameters.ulErrorThreshold       = m_bUseTimestampQueue ? 100 : 0;

  memset(&m_VideoParserExInfo, 0, sizeof(CUVIDEOFORMATEX));

  // Handle AnnexB conversion for H.264 and HEVC
  if (pmt->formattype == FORMAT_MPEG2Video && (pmt->subtype == MEDIASUBTYPE_AVC1 || pmt->subtype == MEDIASUBTYPE_avc1 || pmt->subtype == MEDIASUBTYPE_CCV1 || pmt->subtype == MEDIASUBTYPE_HVC1)) {
    MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)pmt->Format();
    m_AnnexBConverter = new CAnnexBConverter();
    m_AnnexBConverter->SetNALUSize(2);

    BYTE *annexBextra = nullptr;
    int size = 0;

    if (cudaCodec == cudaVideoCodec_H264) {
      m_AnnexBConverter->Convert(&annexBextra, &size, (BYTE *)mp2vi->dwSequenceHeader, mp2vi->cbSequenceHeader);
    } else if (cudaCodec == cudaVideoCodec_HEVC && mp2vi->cbSequenceHeader >= 23) {
      BYTE * bHEVCHeader = (BYTE *)mp2vi->dwSequenceHeader;
      int nal_len_size = (bHEVCHeader[21] & 3) + 1;
      if (nal_len_size != mp2vi->dwFlags) {
        DbgLog((LOG_ERROR, 10, L"hvcC nal length size doesn't match media type"));
      }
      m_AnnexBConverter->ConvertHEVCExtradata(&annexBextra, &size,  (BYTE *)mp2vi->dwSequenceHeader, mp2vi->cbSequenceHeader);
    }

    if (annexBextra && size) {
      size = min(size, sizeof(m_VideoParserExInfo.raw_seqhdr_data));
      memcpy(m_VideoParserExInfo.raw_seqhdr_data, annexBextra, size);
      m_VideoParserExInfo.format.seqhdr_data_length = size;
      av_freep(&annexBextra);
    }

    m_AnnexBConverter->SetNALUSize(mp2vi->dwFlags);
  } else {
    size_t hdr_len = 0;
    getExtraData(*pmt, nullptr, &hdr_len);
    if (hdr_len <= 1024) {
      getExtraData(*pmt, m_VideoParserExInfo.raw_seqhdr_data, &hdr_len);
      m_VideoParserExInfo.format.seqhdr_data_length = (unsigned int)hdr_len;
    }
  }

  int bitdepth = 8;
  m_bNeedSequenceCheck = FALSE;
  if (m_VideoParserExInfo.format.seqhdr_data_length) {
    if (cudaCodec == cudaVideoCodec_H264) {
      hr = CheckH264Sequence(m_VideoParserExInfo.raw_seqhdr_data, m_VideoParserExInfo.format.seqhdr_data_length);
      if (FAILED(hr)) {
        return VFW_E_UNSUPPORTED_VIDEO;
      } else if (hr == S_FALSE) {
        m_bNeedSequenceCheck = TRUE;
      }
    } else if (cudaCodec == cudaVideoCodec_MPEG2) {
      DbgLog((LOG_TRACE, 10, L"-> Scanning extradata for MPEG2 sequence header"));
      CMPEG2HeaderParser mpeg2parser(m_VideoParserExInfo.raw_seqhdr_data, m_VideoParserExInfo.format.seqhdr_data_length);
      if (mpeg2parser.hdr.valid) {
        if (mpeg2parser.hdr.chroma >= 2) {
          DbgLog((LOG_TRACE, 10, L"  -> Sequence header indicates incompatible chroma sampling (chroma: %d)", mpeg2parser.hdr.chroma));
          return VFW_E_UNSUPPORTED_VIDEO;
        }
        m_bInterlaced = mpeg2parser.hdr.interlaced;
      }
    } else if (cudaCodec == cudaVideoCodec_VC1) {
      CVC1HeaderParser vc1Parser(m_VideoParserExInfo.raw_seqhdr_data, m_VideoParserExInfo.format.seqhdr_data_length);
      m_bInterlaced = vc1Parser.hdr.interlaced;
    } else if (cudaCodec == cudaVideoCodec_HEVC) {
      hr = CheckHEVCSequence(m_VideoParserExInfo.raw_seqhdr_data, m_VideoParserExInfo.format.seqhdr_data_length, &bitdepth);
      if (FAILED(hr)) {
        return VFW_E_UNSUPPORTED_VIDEO;
      } else if (hr == S_FALSE) {
        m_bNeedSequenceCheck = TRUE;
      }
    }
  } else {
    m_bNeedSequenceCheck = (cudaCodec == cudaVideoCodec_H264 || cudaCodec == cudaVideoCodec_HEVC);
  }

  oVideoParserParameters.pExtVideoInfo = &m_VideoParserExInfo;
  CUresult oResult = cuda.cuvidCreateVideoParser(&m_hParser, &oVideoParserParameters);
  if (oResult != CUDA_SUCCESS) {
    DbgLog((LOG_ERROR, 10, L"-> Creating parser for type %d failed with code %d", cudaCodec, oResult));
    return E_FAIL;
  }

  BITMAPINFOHEADER *bmi = nullptr;
  videoFormatTypeHandler(pmt->Format(), pmt->FormatType(), &bmi);

  {
    hr = CreateCUVIDDecoder(cudaCodec, bmi->biWidth, bmi->biHeight, bitdepth, !m_bInterlaced);
    if (FAILED(hr)) {
      DbgLog((LOG_ERROR, 10, L"-> Creating CUVID decoder failed"));
      return hr;
    }
  }

  m_bForceSequenceUpdate = TRUE;

  DecodeSequenceData();

  return S_OK;
}

STDMETHODIMP CDecCuvid::CreateCUVIDDecoder(cudaVideoCodec codec, DWORD dwWidth, DWORD dwHeight, int nBitdepth, bool bProgressiveSequence)
{
  DbgLog((LOG_TRACE, 10, L"CDecCuvid::CreateCUVIDDecoder(): Creating CUVID decoder instance"));
  HRESULT hr = S_OK;
  BOOL bDXVAMode = (m_pD3DDevice9 != nullptr);

  cuda.cuvidCtxLock(m_cudaCtxLock, 0);
  CUVIDDECODECREATEINFO *dci = &m_VideoDecoderInfo;

retry:
  if (m_hDecoder) {
    cuda.cuvidDestroyDecoder(m_hDecoder);
    m_hDecoder = 0;
  }
  ZeroMemory(dci, sizeof(*dci));
  dci->ulWidth             = dwWidth;
  dci->ulHeight            = dwHeight;
  dci->ulNumDecodeSurfaces = MAX_DECODE_FRAMES;
  dci->CodecType           = codec;
  dci->bitDepthMinus8      = nBitdepth - 8;
  dci->ChromaFormat        = cudaVideoChromaFormat_420;
  dci->OutputFormat        = nBitdepth > 8 ? cudaVideoSurfaceFormat_P016 : cudaVideoSurfaceFormat_NV12;
  dci->DeinterlaceMode     = (bProgressiveSequence || (m_pSettings->GetDeinterlacingMode() == DeintMode_Disable)) ? cudaVideoDeinterlaceMode_Weave : (cudaVideoDeinterlaceMode)m_pSettings->GetHWAccelDeintMode();
  dci->ulNumOutputSurfaces = 1;

  dci->ulTargetWidth       = dwWidth;
  dci->ulTargetHeight      = dwHeight;

  // can't provide the original values here, or the decoder starts doing weird things - scaling to the size and cropping afterwards
  dci->display_area.right  = (short)dwWidth;
  dci->display_area.bottom = (short)dwHeight;

  dci->ulCreationFlags     = bDXVAMode ? cudaVideoCreate_PreferDXVA : cudaVideoCreate_PreferCUVID;
  dci->vidLock             = m_cudaCtxLock;

  // create the decoder
  CUresult oResult = cuda.cuvidCreateDecoder(&m_hDecoder, dci);
  if (oResult != CUDA_SUCCESS) {
    DbgLog((LOG_ERROR, 10, L"-> Creation of decoder for type %d failed with code %d", dci->CodecType, oResult));
    if (bDXVAMode) {
      DbgLog((LOG_ERROR, 10, L"  -> Retrying in pure CUVID mode"));
      bDXVAMode = FALSE;
      goto retry;
    }
    hr = E_FAIL;
  }
  cuda.cuvidCtxUnlock(m_cudaCtxLock, 0);

  return hr;
}

STDMETHODIMP CDecCuvid::DecodeSequenceData()
{
  CUresult oResult;

  CUVIDSOURCEDATAPACKET pCuvidPacket;
  ZeroMemory(&pCuvidPacket, sizeof(pCuvidPacket));

  pCuvidPacket.payload      = m_VideoParserExInfo.raw_seqhdr_data;
  pCuvidPacket.payload_size = m_VideoParserExInfo.format.seqhdr_data_length;

  if (pCuvidPacket.payload && pCuvidPacket.payload_size) {
    cuda.cuvidCtxLock(m_cudaCtxLock, 0);
    oResult = cuda.cuvidParseVideoData(m_hParser, &pCuvidPacket);
    cuda.cuvidCtxUnlock(m_cudaCtxLock, 0);
  }

  return S_OK;
}

CUVIDPARSERDISPINFO* CDecCuvid::GetNextFrame()
{
  int next = (m_DisplayPos + 1) % m_DisplayDelay;
  return &m_DisplayQueue[next];
}

int CUDAAPI CDecCuvid::HandleVideoSequence(void *obj, CUVIDEOFORMAT *cuvidfmt)
{
  DbgLog((LOG_TRACE, 10, L"CDecCuvid::HandleVideoSequence(): New Video Sequence"));
  CDecCuvid *filter = static_cast<CDecCuvid *>(obj);

  CUVIDDECODECREATEINFO *dci = &filter->m_VideoDecoderInfo;

  // Check if we should be deinterlacing
  bool bShouldDeinterlace = (!cuvidfmt->progressive_sequence && filter->m_pSettings->GetDeinterlacingMode() != DeintMode_Disable && filter->m_pSettings->GetHWAccelDeintMode() != HWDeintMode_Weave);

  // Re-initialize the decoder if needed
  if ((cuvidfmt->codec != dci->CodecType)
    || (cuvidfmt->coded_width != dci->ulWidth)
    || (cuvidfmt->coded_height != dci->ulHeight)
    || (cuvidfmt->chroma_format != dci->ChromaFormat)
    || (cuvidfmt->bit_depth_luma_minus8 != dci->bitDepthMinus8)
    || (bShouldDeinterlace != (dci->DeinterlaceMode != cudaVideoDeinterlaceMode_Weave))
    || filter->m_bForceSequenceUpdate)
  {
    filter->m_bForceSequenceUpdate = FALSE;
    HRESULT hr = filter->CreateCUVIDDecoder(cuvidfmt->codec, cuvidfmt->coded_width, cuvidfmt->coded_height, cuvidfmt->bit_depth_luma_minus8 + 8, cuvidfmt->progressive_sequence != 0);
    if (FAILED(hr))
      filter->m_bFormatIncompatible = TRUE;
  }

  filter->m_bInterlaced = !cuvidfmt->progressive_sequence;
  filter->m_bDoubleRateDeint = bShouldDeinterlace && (filter->m_pSettings->GetHWAccelDeintOutput() == DeintOutput_FramePerField);
  if (filter->m_bInterlaced && cuvidfmt->frame_rate.numerator && cuvidfmt->frame_rate.denominator) {
    double dFrameTime = 10000000.0 / ((double)cuvidfmt->frame_rate.numerator / cuvidfmt->frame_rate.denominator);
    if (filter->m_bDoubleRateDeint && (int)(dFrameTime / 10000.0) == 41) {
      filter->m_bDoubleRateDeint = TRUE;
    }
    if (cuvidfmt->codec != cudaVideoCodec_MPEG4)
      filter->m_rtAvgTimePerFrame = REFERENCE_TIME(dFrameTime + 0.5);
    else
      filter->m_rtAvgTimePerFrame = AV_NOPTS_VALUE; //TODO: base on media type
  } else {
    filter->m_rtAvgTimePerFrame = AV_NOPTS_VALUE;
  }

  // Adjust frame time for double-rate deint
  if (filter->m_bDoubleRateDeint && filter->m_rtAvgTimePerFrame != AV_NOPTS_VALUE)
    filter->m_rtAvgTimePerFrame /= 2;

  filter->m_VideoFormat = *cuvidfmt;

  if (cuvidfmt->chroma_format != cudaVideoChromaFormat_420) {
    DbgLog((LOG_TRACE, 10, L"CDecCuvid::HandleVideoSequence(): Incompatible Chroma Format detected"));
    filter->m_bFormatIncompatible = TRUE;
  }

  fillDXVAExtFormat(filter->m_DXVAExtendedFormat, (filter->m_iFullRange > 0 || cuvidfmt->video_signal_description.video_full_range_flag), cuvidfmt->video_signal_description.color_primaries, cuvidfmt->video_signal_description.matrix_coefficients, cuvidfmt->video_signal_description.transfer_characteristics);

  return TRUE;
}

int CUDAAPI CDecCuvid::HandlePictureDecode(void *obj, CUVIDPICPARAMS *cuvidpic)
{
  CDecCuvid *filter = reinterpret_cast<CDecCuvid *>(obj);

  if (filter->m_bFlushing)
    return FALSE;

  if (filter->m_bWaitForKeyframe) {
    if (cuvidpic->intra_pic_flag)
      filter->m_bWaitForKeyframe = FALSE;
    else {
      // Pop timestamp from the queue, drop frame
      if (!filter->m_timestampQueue.empty()) {
        filter->m_timestampQueue.pop();
      }
      return FALSE;
    }
  }

  int flush_pos = filter->m_DisplayPos;
  for (;;) {
    bool frame_in_use = false;
    for (int i=0; i<filter->m_DisplayDelay; i++) {
      if (filter->m_DisplayQueue[i].picture_index == cuvidpic->CurrPicIdx) {
        frame_in_use = true;
        break;
      }
    }
    if (!frame_in_use) {
      // No problem: we're safe to use this frame
      break;
    }
    // The target frame is still pending in the display queue:
    // Flush the oldest entry from the display queue and repeat
    if (filter->m_DisplayQueue[flush_pos].picture_index >= 0) {
      filter->Display(&filter->m_DisplayQueue[flush_pos]);
      filter->m_DisplayQueue[flush_pos].picture_index = -1;
    }
    flush_pos = (flush_pos + 1) % filter->m_DisplayDelay;
  }

  filter->cuda.cuvidCtxLock(filter->m_cudaCtxLock, 0);
  filter->m_PicParams[cuvidpic->CurrPicIdx] = *cuvidpic;
  __try {
    CUresult cuStatus = filter->cuda.cuvidDecodePicture(filter->m_hDecoder, cuvidpic);
  #ifdef DEBUG
    if (cuStatus != CUDA_SUCCESS) {
      DbgLog((LOG_ERROR, 10, L"CDecCuvid::HandlePictureDecode(): cuvidDecodePicture returned error code %d", cuStatus));
    }
  #endif
  } __except(1) {
    DbgLog((LOG_ERROR, 10, L"CDecCuvid::HandlePictureDecode(): cuvidDecodePicture threw an exception"));
  }
  filter->cuda.cuvidCtxUnlock(filter->m_cudaCtxLock, 0);

  return TRUE;
}

int CUDAAPI CDecCuvid::HandlePictureDisplay(void *obj, CUVIDPARSERDISPINFO *cuviddisp)
{
  CDecCuvid *filter = reinterpret_cast<CDecCuvid *>(obj);

  if (filter->m_bFlushing)
    return FALSE;

  if (filter->m_bUseTimestampQueue) {
    if (filter->m_timestampQueue.empty()) {
      cuviddisp->timestamp = AV_NOPTS_VALUE;
    } else {
      cuviddisp->timestamp = filter->m_timestampQueue.front();
      filter->m_timestampQueue.pop();
    }
  }

  // Drop samples with negative timestamps (preroll) or during flushing
  if (cuviddisp->timestamp != AV_NOPTS_VALUE && cuviddisp->timestamp < 0)
    return TRUE;

  if (filter->m_DisplayQueue[filter->m_DisplayPos].picture_index >= 0) {
    filter->Display(&filter->m_DisplayQueue[filter->m_DisplayPos]);
    filter->m_DisplayQueue[filter->m_DisplayPos].picture_index = -1;
  }
  filter->m_DisplayQueue[filter->m_DisplayPos] = *cuviddisp;
  filter->m_DisplayPos = (filter->m_DisplayPos + 1) % filter->m_DisplayDelay;

  return TRUE;
}

STDMETHODIMP CDecCuvid::Display(CUVIDPARSERDISPINFO *cuviddisp)
{
  BOOL bTreatAsProgressive = m_pSettings->GetDeinterlacingMode() == DeintMode_Disable;

  if (bTreatAsProgressive) {
    cuviddisp->progressive_frame = TRUE;
    m_nSoftTelecine = FALSE;
  } else {
    if (m_VideoFormat.codec == cudaVideoCodec_MPEG2 || m_VideoFormat.codec == cudaVideoCodec_H264) {
      if (cuviddisp->repeat_first_field) {
        m_nSoftTelecine = 2;
      } else if (m_nSoftTelecine) {
        m_nSoftTelecine--;
      }
      if (!m_nSoftTelecine)
        m_bTFF = cuviddisp->top_field_first;
    }

    cuviddisp->progressive_frame = (cuviddisp->progressive_frame && !(m_bInterlaced && m_pSettings->GetDeinterlacingMode() == DeintMode_Aggressive && m_VideoFormat.codec != cudaVideoCodec_VC1 && !m_nSoftTelecine) && !(m_pSettings->GetDeinterlacingMode() == DeintMode_Force));
  }

  LAVDeintFieldOrder fo        = m_pSettings->GetDeintFieldOrder();
  cuviddisp->top_field_first   = (fo == DeintFieldOrder_Auto) ? (m_nSoftTelecine ? m_bTFF : cuviddisp->top_field_first) : (fo == DeintFieldOrder_TopFieldFirst);

  if (m_bDoubleRateDeint) {
    if (cuviddisp->progressive_frame || m_nSoftTelecine) {
      Deliver(cuviddisp, 2);
    } else {
      Deliver(cuviddisp, 0);
      Deliver(cuviddisp, 1);
    }
  } else {
    Deliver(cuviddisp);
  }
  return S_OK;
}

STDMETHODIMP CDecCuvid::Deliver(CUVIDPARSERDISPINFO *cuviddisp, int field)
{
  CUdeviceptr devPtr = 0;
  unsigned int pitch = 0;
  CUVIDPROCPARAMS vpp;
  CUresult cuStatus = CUDA_SUCCESS;

  memset(&vpp, 0, sizeof(vpp));
  vpp.progressive_frame = !m_nSoftTelecine && cuviddisp->progressive_frame;
  vpp.top_field_first = cuviddisp->top_field_first;
  vpp.second_field = (field == 1);

  cuda.cuvidCtxLock(m_cudaCtxLock, 0);
  cuStatus = cuda.cuvidMapVideoFrame(m_hDecoder, cuviddisp->picture_index, &devPtr, &pitch, &vpp);
  if (cuStatus != CUDA_SUCCESS) {
    DbgLog((LOG_CUSTOM1, 1, L"CDecCuvid::Deliver(): cuvidMapVideoFrame failed on index %d", cuviddisp->picture_index));
    goto cuda_fail;
  }

  int size = pitch * m_VideoDecoderInfo.ulTargetHeight * 3 / 2;
  if(!m_pbRawNV12 || size > m_cRawNV12) {
    if (m_pbRawNV12) {
      cuda.cuMemFreeHost(m_pbRawNV12);
      m_pbRawNV12 = nullptr;
      m_cRawNV12 = 0;
    }
    cuStatus = cuda.cuMemAllocHost((void **)&m_pbRawNV12, size);
    if (cuStatus != CUDA_SUCCESS) {
      DbgLog((LOG_CUSTOM1, 1, L"CDecCuvid::Deliver(): cuMemAllocHost failed to allocate %d bytes (%d)", size, cuStatus));
      goto cuda_fail;
    }
    m_cRawNV12 = size;
  }
  // Copy memory from the device into the staging area
  if (m_pbRawNV12) {
    cuStatus = cuda.cuMemcpyDtoH(m_pbRawNV12, devPtr, size);
    if (cuStatus != CUDA_SUCCESS) {
      DbgLog((LOG_ERROR, 10, L"Memory Transfer failed (%d)", cuStatus));
      goto cuda_fail;
    }
  } else {
    // If we don't have our memory, this is bad.
    DbgLog((LOG_ERROR, 10, L"No Valid Staging Memory - failing"));
    goto cuda_fail;
  }
  cuda.cuvidUnmapVideoFrame(m_hDecoder, devPtr);
  cuda.cuvidCtxUnlock(m_cudaCtxLock, 0);


  // Setup the LAVFrame
  LAVFrame *pFrame = nullptr;
  AllocateFrame(&pFrame);

  if (m_rtAvgTimePerFrame != AV_NOPTS_VALUE) {
    pFrame->avgFrameDuration = m_rtAvgTimePerFrame;
    if (m_bDoubleRateDeint && field == 2)
      pFrame->avgFrameDuration *= 2;
  }

  REFERENCE_TIME rtStart = cuviddisp->timestamp, rtStop = AV_NOPTS_VALUE;
  if (rtStart != AV_NOPTS_VALUE) {
    CUVIDPARSERDISPINFO *next = GetNextFrame();
    if (next->picture_index != -1 && next->timestamp != AV_NOPTS_VALUE) {
      m_rtPrevDiff = next->timestamp - cuviddisp->timestamp;
    }

    if (m_rtPrevDiff != AV_NOPTS_VALUE) {
      REFERENCE_TIME rtHalfDiff = m_rtPrevDiff >> 1;
      if (field == 1)
        rtStart += rtHalfDiff;

      rtStop = rtStart + rtHalfDiff;

      if (field == 2 || !m_bDoubleRateDeint)
        rtStop += rtHalfDiff;
    }

    // Sanity check in case the duration is null
    if (rtStop <= rtStart)
      rtStop = AV_NOPTS_VALUE;
  }

  pFrame->format = (m_VideoDecoderInfo.OutputFormat == cudaVideoSurfaceFormat_P016) ? LAVPixFmt_P016 : LAVPixFmt_NV12;
  pFrame->bpp = m_VideoDecoderInfo.bitDepthMinus8 + 8;
  pFrame->width  = m_VideoFormat.display_area.right;
  pFrame->height = m_VideoFormat.display_area.bottom;
  pFrame->rtStart = rtStart;
  pFrame->rtStop = rtStop;
  pFrame->repeat = cuviddisp->repeat_first_field;
  {
    AVRational ar = { m_VideoFormat.display_aspect_ratio.x, m_VideoFormat.display_aspect_ratio.y };
    AVRational arDim = { pFrame->width, pFrame->height };
    if (m_bARPresent || av_cmp_q(ar, arDim) != 0) {
      pFrame->aspect_ratio = ar;
    }
  }
  pFrame->ext_format = m_DXVAExtendedFormat;
  pFrame->interlaced = !cuviddisp->progressive_frame && m_VideoDecoderInfo.DeinterlaceMode == cudaVideoDeinterlaceMode_Weave;
  pFrame->tff = cuviddisp->top_field_first;

  // TODO: This may be wrong for H264 where B-Frames can be references
  pFrame->frame_type = m_PicParams[cuviddisp->picture_index].intra_pic_flag ? 'I' : (m_PicParams[cuviddisp->picture_index].ref_pic_flag ? 'P' : 'B');

  // Assign the buffer to the LAV Frame bufers
  int Ysize = m_VideoDecoderInfo.ulTargetHeight * pitch;
  pFrame->data[0] = m_pbRawNV12;
  pFrame->data[1] = m_pbRawNV12+Ysize;
  pFrame->stride[0] = pFrame->stride[1] = pitch;
  pFrame->flags  |= LAV_FRAME_FLAG_BUFFER_MODIFY;

  if (m_bEndOfSequence)
    pFrame->flags |= LAV_FRAME_FLAG_END_OF_SEQUENCE;

  m_pCallback->Deliver(pFrame);

  return S_OK;

cuda_fail:
  cuda.cuvidUnmapVideoFrame(m_hDecoder, devPtr);
  cuda.cuvidCtxUnlock(m_cudaCtxLock, 0);
  return E_FAIL;
}

STDMETHODIMP CDecCuvid::CheckH264Sequence(const BYTE *buffer, int buflen)
{
  DbgLog((LOG_TRACE, 10, L"CDecCuvid::CheckH264Sequence(): Checking H264 frame for SPS"));
  CH264SequenceParser h264parser;
  h264parser.ParseNALs(buffer, buflen, 0);
  if (h264parser.sps.valid) {
    m_bInterlaced = h264parser.sps.interlaced;
    m_iFullRange = h264parser.sps.full_range;
    m_bARPresent = h264parser.sps.ar_present;
    DbgLog((LOG_TRACE, 10, L"-> SPS found"));
    if (h264parser.sps.profile > 100 || h264parser.sps.chroma != 1 || h264parser.sps.luma_bitdepth != 8 || h264parser.sps.chroma_bitdepth != 8) {
      DbgLog((LOG_TRACE, 10, L"  -> SPS indicates video incompatible with CUVID, aborting (profile: %d, chroma: %d, bitdepth: %d/%d)", h264parser.sps.profile, h264parser.sps.chroma, h264parser.sps.luma_bitdepth, h264parser.sps.chroma_bitdepth));
      return E_FAIL;
    }
    DbgLog((LOG_TRACE, 10, L"-> Video seems compatible with CUVID"));
    return S_OK;
  }
  return S_FALSE;
}

STDMETHODIMP CDecCuvid::CheckHEVCSequence(const BYTE *buffer, int buflen, int *bitdepth)
{
  DbgLog((LOG_TRACE, 10, L"CDecCuvid::CheckHEVCSequence(): Checking HEVC frame for SPS"));
  CHEVCSequenceParser hevcParser;
  hevcParser.ParseNALs(buffer, buflen, 0);
  if (hevcParser.sps.valid) {
    DbgLog((LOG_TRACE, 10, L"-> SPS found"));
    if (hevcParser.sps.chroma > 1 || hevcParser.sps.bitdepth > 12 || !(hevcParser.sps.profile <= FF_PROFILE_HEVC_MAIN_10 || (hevcParser.sps.profile == FF_PROFILE_HEVC_REXT && hevcParser.sps.rext_profile == HEVC_REXT_PROFILE_MAIN_12))) {
      DbgLog((LOG_TRACE, 10, L"  -> SPS indicates video incompatible with CUVID, aborting (profile: %d)", hevcParser.sps.profile));
      return E_FAIL;
    }
    if (bitdepth)
      *bitdepth = hevcParser.sps.bitdepth;
    DbgLog((LOG_TRACE, 10, L"-> Video seems compatible with CUVID"));
    return S_OK;
  }
  return S_FALSE;
}

STDMETHODIMP CDecCuvid::Decode(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity, IMediaSample *pSample)
{
  CUresult result;
  HRESULT hr = S_OK;

  CUVIDSOURCEDATAPACKET pCuvidPacket;
  ZeroMemory(&pCuvidPacket, sizeof(pCuvidPacket));

  BYTE *pBuffer = nullptr;
  if (m_AnnexBConverter) {
    int size = 0;
    hr = m_AnnexBConverter->Convert(&pBuffer, &size, buffer, buflen);
    if (SUCCEEDED(hr)) {
      pCuvidPacket.payload      = pBuffer;
      pCuvidPacket.payload_size = size;
    }
  } else {
    pCuvidPacket.payload      = buffer;
    pCuvidPacket.payload_size = buflen;

    if (m_VideoDecoderInfo.CodecType == cudaVideoCodec_MPEG2) {
      const uint8_t *eosmarker = nullptr;
      const uint8_t *end = buffer + buflen;
      int status = CheckForSequenceMarkers(AV_CODEC_ID_MPEG2VIDEO, buffer, buflen, &m_MpegParserState, &eosmarker);

      // If we found a EOS marker, but its not at the end of the packet, then split the packet
      // to be able to individually decode the frame before the EOS, and then decode the remainder
      if (status & STATE_EOS_FOUND && eosmarker && eosmarker != end) {
        Decode(buffer, (int)(eosmarker - buffer), rtStart, rtStop, bSyncPoint, bDiscontinuity, nullptr);

        rtStart = rtStop = AV_NOPTS_VALUE;
        pCuvidPacket.payload      = eosmarker;
        pCuvidPacket.payload_size = (int)(end - eosmarker);
      } else if (eosmarker) {
        m_bEndOfSequence = TRUE;
      }
    }
  }

  if (m_bNeedSequenceCheck) {
    if (m_VideoDecoderInfo.CodecType == cudaVideoCodec_H264) {
      hr = CheckH264Sequence(pCuvidPacket.payload, pCuvidPacket.payload_size);
    } else if (m_VideoDecoderInfo.CodecType == cudaVideoCodec_HEVC) {
      hr = CheckHEVCSequence(pCuvidPacket.payload, pCuvidPacket.payload_size, nullptr);
    }
    if (FAILED(hr)) {
      m_bFormatIncompatible = TRUE;
    } else if (hr == S_OK) {
      m_bNeedSequenceCheck = FALSE;
    }
  }

  if (rtStart != AV_NOPTS_VALUE) {
    pCuvidPacket.flags     |= CUVID_PKT_TIMESTAMP;
    pCuvidPacket.timestamp  = rtStart;
  }

  if (bDiscontinuity)
    pCuvidPacket.flags     |= CUVID_PKT_DISCONTINUITY;

  if (m_bUseTimestampQueue)
    m_timestampQueue.push(rtStart);

  cuda.cuvidCtxLock(m_cudaCtxLock, 0);
  __try {
    result = cuda.cuvidParseVideoData(m_hParser, &pCuvidPacket);
  } __except(1) {
    DbgLog((LOG_ERROR, 10, L"CDecCuvid::Decode(): cuvidParseVideoData threw an exception"));
  }
  cuda.cuvidCtxUnlock(m_cudaCtxLock, 0);

  av_freep(&pBuffer);

  if (m_bEndOfSequence) {
    EndOfStream();
    m_pCallback->Deliver(m_pCallback->GetFlushFrame());
    m_bEndOfSequence = FALSE;
  }

  if (m_bFormatIncompatible) {
    DbgLog((LOG_ERROR, 10, L"CDecCuvid::Decode(): Incompatible format detected, indicating failure..."));
    return E_FAIL;
  }

  return S_OK;
}

STDMETHODIMP CDecCuvid::Flush()
{
  DbgLog((LOG_TRACE, 10, L"CDecCuvid::Flush(): Flushing CUVID decoder"));
  m_bFlushing = TRUE;

  FlushParser();

  // Flush display queue
  for (int i=0; i<m_DisplayDelay; ++i) {
    if (m_DisplayQueue[m_DisplayPos].picture_index >= 0) {
      m_DisplayQueue[m_DisplayPos].picture_index = -1;
    }
    m_DisplayPos = (m_DisplayPos + 1) % m_DisplayDelay;
  }

  m_bFlushing = FALSE;
  m_bWaitForKeyframe = m_bUseTimestampQueue;

  // Re-init decoder after flush
  DecodeSequenceData();

  // Clear timestamp queue
  std::queue<REFERENCE_TIME>().swap(m_timestampQueue);

  m_nSoftTelecine = 0;

  return __super::Flush();
}

STDMETHODIMP CDecCuvid::EndOfStream()
{
  FlushParser();

  // Display all frames left in the queue
  for (int i=0; i<m_DisplayDelay; ++i) {
    if (m_DisplayQueue[m_DisplayPos].picture_index >= 0) {
      Display(&m_DisplayQueue[m_DisplayPos]);
      m_DisplayQueue[m_DisplayPos].picture_index = -1;
    }
    m_DisplayPos = (m_DisplayPos + 1) % m_DisplayDelay;
  }

  return S_OK;
}

STDMETHODIMP CDecCuvid::GetPixelFormat(LAVPixelFormat *pPix, int *pBpp)
{
  // Output is always NV12
  if (pPix)
    *pPix = (m_VideoDecoderInfo.OutputFormat == cudaVideoSurfaceFormat_P016) ? LAVPixFmt_P016 : LAVPixFmt_NV12;
  if (pBpp)
    *pBpp = m_VideoDecoderInfo.bitDepthMinus8 + 8;
  return S_OK;
}

STDMETHODIMP_(REFERENCE_TIME) CDecCuvid::GetFrameDuration()
{
  return 0;
}

STDMETHODIMP_(BOOL) CDecCuvid::IsInterlaced(BOOL bAllowGuess)
{
  return (m_bInterlaced || m_pSettings->GetDeinterlacingMode() == DeintMode_Force) && (m_VideoDecoderInfo.DeinterlaceMode == cudaVideoDeinterlaceMode_Weave);
}


STDMETHODIMP CDecCuvid::GetHWAccelActiveDevice(BSTR *pstrDeviceName)
{
  CheckPointer(pstrDeviceName, E_POINTER);

  if (strlen(m_cudaDeviceName) == 0)
    return E_UNEXPECTED;

  int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, m_cudaDeviceName, -1, nullptr, 0);
  if (len == 0)
    return E_FAIL;

  *pstrDeviceName = SysAllocStringLen(nullptr, len);
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, m_cudaDeviceName, -1, *pstrDeviceName, len);
  return S_OK;
}
