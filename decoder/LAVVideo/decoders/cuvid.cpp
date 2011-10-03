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

#include "stdafx.h"
#include "cuvid.h"

#include "moreuuids.h"

#include "parsers/H264SequenceParser.h"
#include "parsers/MPEG2HeaderParser.h"

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
  CodecID ffcodec;
  cudaVideoCodec cudaCodec;
} cuda_codecs[] = {
  { CODEC_ID_MPEG2VIDEO, cudaVideoCodec_MPEG2 },
  { CODEC_ID_VC1,        cudaVideoCodec_VC1   },
  { CODEC_ID_H264,       cudaVideoCodec_H264  },
};

////////////////////////////////////////////////////////////////////////////////
// CUVID decoder implementation
////////////////////////////////////////////////////////////////////////////////

CDecCuvid::CDecCuvid(void)
  : CDecBase()
  , m_pD3D(NULL), m_pD3DDevice(NULL)
  , m_cudaContext(0), m_cudaCtxLock(0)
  , m_hParser(0), m_hDecoder(0), m_hStream(0)
  , m_bForceSequenceUpdate(FALSE)
  , m_bInterlaced(FALSE)
  , m_bFlushing(FALSE)
  , m_pbRawNV12(NULL), m_cRawNV12(0)
  , m_rtAvgTimePerFrame(AV_NOPTS_VALUE)
  , m_AVC1Converter(NULL)
  , m_bDoubleRateDeint(FALSE)
  , m_bFormatIncompatible(FALSE)
  , m_bUseTimestampQueue(FALSE)
  , m_bWaitForKeyframe(FALSE)
  , m_iFullRange(-1)
  , m_hwnd(NULL)
{
  ZeroMemory(&cuda, sizeof(cuda));
  ZeroMemory(&m_VideoFormat, sizeof(m_VideoFormat));
  ZeroMemory(&m_DXVAExtendedFormat, sizeof(m_DXVAExtendedFormat));
}

CDecCuvid::~CDecCuvid(void)
{
  DestroyDecoder(true);
  DestroyWindow(m_hwnd);
  m_hwnd = 0;
  UnregisterClass(L"cuvidDummyHWNDClass", NULL);
}

STDMETHODIMP CDecCuvid::DestroyDecoder(bool bFull)
{
  if (m_AVC1Converter) {
    SAFE_DELETE(m_AVC1Converter);
  }

  if (m_hDecoder) {
    cuda.cuvidDestroyDecoder(m_hDecoder);
    m_hDecoder = 0;
  }

  if (m_hParser) {
    cuda.cuvidDestroyVideoParser(m_hParser);
    m_hParser = 0;
  }

  if (m_hStream) {
    cuda.cuStreamDestroy(m_hStream);
    m_hStream = 0;
  }

  if (m_pbRawNV12) {
    cuda.cuMemFreeHost(m_pbRawNV12);
    m_pbRawNV12 = NULL;
    m_cRawNV12 = 0;
  }

  if(bFull) {
    if (m_cudaCtxLock) {
      cuda.cuvidCtxLockDestroy(m_cudaCtxLock);
      m_cudaCtxLock = 0;
    }

    if (m_cudaContext) {
      cuda.cuCtxDestroy(m_cudaContext);
      m_cudaContext = 0;
    }

    SafeRelease(&m_pD3DDevice);
    SafeRelease(&m_pD3D);

    FreeLibrary(cuda.cudaLib);
    FreeLibrary(cuda.cuvidLib);
  }

  return S_OK;
}

#define GET_PROC_EX(name, lib)                         \
  cuda.name = (t##name *)GetProcAddress(lib, #name); \
  if (cuda.name == NULL) {                           \
    DbgLog((LOG_ERROR, 10, L"-> Failed to load function \"%s\"", TEXT(#name))); \
    return E_FAIL; \
  }

#define GET_PROC_CUDA(name) GET_PROC_EX(name, cuda.cudaLib)
#define GET_PROC_CUVID(name) GET_PROC_EX(name, cuda.cuvidLib)


STDMETHODIMP CDecCuvid::LoadCUDAFuncRefs()
{
  // Load CUDA functions
  cuda.cudaLib = LoadLibrary(L"nvcuda.dll");
  if (cuda.cudaLib == NULL) {
    DbgLog((LOG_TRACE, 10, L"-> Loading nvcuda.dll failed"));
    return E_FAIL;
  }

  GET_PROC_CUDA(cuInit);
  GET_PROC_CUDA(cuCtxCreate);
  GET_PROC_CUDA(cuCtxDestroy);
  GET_PROC_CUDA(cuCtxPushCurrent);
  GET_PROC_CUDA(cuCtxPopCurrent);
  GET_PROC_CUDA(cuD3D9CtxCreate);
  GET_PROC_CUDA(cuMemAllocHost);
  GET_PROC_CUDA(cuMemFreeHost);
  GET_PROC_CUDA(cuMemcpyDtoH);
  GET_PROC_CUDA(cuMemcpyDtoHAsync);
  GET_PROC_CUDA(cuStreamCreate);
  GET_PROC_CUDA(cuStreamDestroy);
  GET_PROC_CUDA(cuStreamQuery);

  // Load CUVID function
  cuda.cuvidLib = LoadLibrary(L"nvcuvid.dll");
  if (cuda.cuvidLib == NULL) {
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
  GET_PROC_CUVID(cuvidMapVideoFrame);
  GET_PROC_CUVID(cuvidUnmapVideoFrame);

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

HWND CDecCuvid::GetDummyHWND()
{
  if (!m_hwnd)
  {
    WNDCLASS wndclass;

    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = DefWindowProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = NULL;
    wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH) GetStockObject (WHITE_BRUSH);
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = L"cuvidDummyHWNDClass";

    if (!RegisterClass(&wndclass)) {
      DbgLog((LOG_ERROR, 10, L"Creating dummy HWND failed"));
      return 0;
    }

    m_hwnd = CreateWindow(L"cuvidDummyHWNDClass",
      TEXT("CUVIDDummyWindow"),
      WS_OVERLAPPEDWINDOW,
      0,                   // Initial X
      0,                   // Initial Y
      0,                   // Width
      0,                   // Height
      NULL,
      NULL,
      NULL,
      NULL);
  }
  return m_hwnd;
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
  int best_device = 0;
  int device = best_device;

  m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
  if (!m_pD3D) {
    DbgLog((LOG_ERROR, 10, L"-> Failed to acquire IDirect3D9"));
    return E_FAIL;
  }

  D3DADAPTER_IDENTIFIER9 d3dId;
  D3DPRESENT_PARAMETERS d3dpp;
  D3DDISPLAYMODE d3ddm;
  unsigned uAdapterCount = m_pD3D->GetAdapterCount();
  for (unsigned lAdapter=0; lAdapter<uAdapterCount; lAdapter++) {
    DbgLog((LOG_TRACE, 10, L"-> Trying D3D Adapter %d..", lAdapter));

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    m_pD3D->GetAdapterDisplayMode(lAdapter, &d3ddm);

    d3dpp.Windowed               = TRUE;
    d3dpp.BackBufferWidth        = 640;
    d3dpp.BackBufferHeight       = 480;
    d3dpp.BackBufferCount        = 1;
    d3dpp.BackBufferFormat       = d3ddm.Format;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    d3dpp.Flags                  = D3DPRESENTFLAG_VIDEO;

    IDirect3DDevice9 *pDev = NULL;
    CUcontext cudaCtx = 0;
    hr = m_pD3D->CreateDevice(lAdapter, D3DDEVTYPE_HAL, GetDummyHWND(), D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &d3dpp, &pDev);
    if (SUCCEEDED(hr)) {
      m_pD3D->GetAdapterIdentifier(lAdapter, 0, &d3dId);
      cuStatus = cuda.cuD3D9CtxCreate(&cudaCtx, &device, CU_CTX_SCHED_BLOCKING_SYNC, pDev);
      if (cuStatus == CUDA_SUCCESS) {
        DbgLog((LOG_TRACE, 10, L"-> Created D3D Device on adapter %S (%d), using CUDA device %d", d3dId.Description, lAdapter, device));

        /*BOOL isLevelC = IsLevelC(d3dId.DeviceId);
        DbgLog((LOG_TRACE, 10, L"InitCUDA(): D3D Device with Id 0x%x is level C: %d", d3dId.DeviceId, isLevelC));

        if (m_bVDPAULevelC && !isLevelC) {
          DbgLog((LOG_TRACE, 10, L"InitCUDA(): We already had a Level C+ device, this one is not, skipping"));
          continue;
        } */

        // Release old resources
        SafeRelease(&m_pD3DDevice);
        if (m_cudaContext)
          cuda.cuCtxDestroy(m_cudaContext);

        // Store resources
        m_pD3DDevice = pDev;
        m_cudaContext = cudaCtx;
        //m_bVDPAULevelC = isLevelC;
        // Is this the one we want?
        if (device == best_device)
          break;
      } else {
        DbgLog((LOG_TRACE, 10, L"-> D3D Device on adapter %d is not CUDA capable", lAdapter));
        SafeRelease(&pDev);
      }
    }
  }

  cuStatus = CUDA_SUCCESS;

  if (!m_pD3DDevice) {
    DbgLog((LOG_TRACE, 10, L"-> No D3D device available, building non-D3D context on device %d", best_device));
    SafeRelease(&m_pD3D);
    cuStatus = cuda.cuCtxCreate(&m_cudaContext, CU_CTX_SCHED_BLOCKING_SYNC, best_device);

    /*int major, minor;
    cuDeviceComputeCapability(&major, &minor, best_device);
    m_bVDPAULevelC = (major >= 2);
    DbgLog((LOG_TRACE, 10, L"InitCUDA(): pure CUDA context of device with compute %d.%d", major, minor)); */
  }

  if (cuStatus == CUDA_SUCCESS) {
    // Switch to a floating context
    CUcontext curr_ctx = NULL;
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

STDMETHODIMP CDecCuvid::InitDecoder(CodecID codec, const CMediaType *pmt)
{
  DbgLog((LOG_TRACE, 10, L"CDecCuvid::InitDecoder(): Initializing CUVID decoder"));
  CUresult cuStatus = CUDA_SUCCESS;
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

  m_bUseTimestampQueue = (cudaCodec == cudaVideoCodec_VC1 && m_pCallback->VC1IsDTS());
  m_bWaitForKeyframe = m_bUseTimestampQueue;

  // Create the CUDA Video Parser
  CUVIDPARSERPARAMS oVideoParserParameters;
  ZeroMemory(&oVideoParserParameters, sizeof(CUVIDPARSERPARAMS));
  oVideoParserParameters.CodecType              = cudaCodec;
  oVideoParserParameters.ulMaxNumDecodeSurfaces = MAX_DECODE_FRAMES;
  oVideoParserParameters.ulMaxDisplayDelay      = DISPLAY_DELAY;
  oVideoParserParameters.pUserData              = this;
  oVideoParserParameters.pfnSequenceCallback    = CDecCuvid::HandleVideoSequence;    // Called before decoding frames and/or whenever there is a format change
  oVideoParserParameters.pfnDecodePicture       = CDecCuvid::HandlePictureDecode;    // Called when a picture is ready to be decoded (decode order)
  oVideoParserParameters.pfnDisplayPicture      = CDecCuvid::HandlePictureDisplay;   // Called whenever a picture is ready to be displayed (display order)
  oVideoParserParameters.ulErrorThreshold       = m_bUseTimestampQueue ? 100 : 0;

  memset(&m_VideoParserExInfo, 0, sizeof(CUVIDEOFORMATEX));

  if (pmt->formattype == FORMAT_MPEG2Video && (pmt->subtype == MEDIASUBTYPE_AVC1 || pmt->subtype == MEDIASUBTYPE_avc1 || pmt->subtype == MEDIASUBTYPE_CCV1)) {
    MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)pmt->Format();
    m_AVC1Converter = new CAVC1AnnexBConverter();
    m_AVC1Converter->SetNALUSize(2);

    BYTE *annexBextra = NULL;
    int size = 0;
    m_AVC1Converter->Convert(&annexBextra, &size, (BYTE *)mp2vi->dwSequenceHeader, mp2vi->cbSequenceHeader);
    if (annexBextra && size) {
      memcpy(m_VideoParserExInfo.raw_seqhdr_data, annexBextra, size);
      m_VideoParserExInfo.format.seqhdr_data_length = size;
      av_freep(&annexBextra);
    }

    m_AVC1Converter->SetNALUSize(mp2vi->dwFlags);
  } else {
    getExtraData(*pmt, m_VideoParserExInfo.raw_seqhdr_data, &m_VideoParserExInfo.format.seqhdr_data_length);
  }

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
      if (mpeg2parser.hdr.chroma >= 2) {
        DbgLog((LOG_TRACE, 10, L"  -> Sequence header indicates incompatible chroma sampling (chroma: %d)", mpeg2parser.hdr.chroma));
        return VFW_E_UNSUPPORTED_VIDEO;
      }
    }
  } else {
    m_bNeedSequenceCheck = (cudaCodec == cudaVideoCodec_H264);
  }

  oVideoParserParameters.pExtVideoInfo = &m_VideoParserExInfo;
  CUresult oResult = cuda.cuvidCreateVideoParser(&m_hParser, &oVideoParserParameters);
  if (oResult != CUDA_SUCCESS) {
    DbgLog((LOG_ERROR, 10, L"-> Creating parser for type %d failed with code %d", cudaCodec, oResult));
    return E_FAIL;
  }

  {
    cuda.cuvidCtxLock(m_cudaCtxLock, 0);
    oResult = cuda.cuStreamCreate(&m_hStream, 0);
    cuda.cuvidCtxUnlock(m_cudaCtxLock, 0);
    if (oResult != CUDA_SUCCESS) {
      DbgLog((LOG_ERROR, 10, L"::InitCodec(): Creating stream failed"));
      return E_FAIL;
    }
  }

  BITMAPINFOHEADER *bmi = NULL;
  videoFormatTypeHandler(pmt->Format(), pmt->FormatType(), &bmi);

  {
    RECT rcDisplayArea = {0, 0, bmi->biWidth, bmi->biHeight};
    hr = CreateCUVIDDecoder(cudaCodec, bmi->biWidth, bmi->biHeight, bmi->biWidth, bmi->biHeight, rcDisplayArea);
    if (FAILED(hr)) {
      DbgLog((LOG_ERROR, 10, L"-> Creating CUVID decoder failed"));
      return hr;
    }
  }

  m_bForceSequenceUpdate = TRUE;

  DecodeSequenceData();

  return S_OK;
}

STDMETHODIMP CDecCuvid::CreateCUVIDDecoder(cudaVideoCodec codec, DWORD dwWidth, DWORD dwHeight, DWORD dwDisplayWidth, DWORD dwDisplayHeight, RECT rcDisplayArea)
{
  DbgLog((LOG_TRACE, 10, L"CDecCuvid::CreateCUVIDDecoder(): Creating CUVID decoder instance"));
  HRESULT hr = S_OK;

  cuda.cuvidCtxLock(m_cudaCtxLock, 0);
  CUVIDDECODECREATEINFO *dci = &m_VideoDecoderInfo;

  if (m_hDecoder) {
    cuda.cuvidDestroyDecoder(m_hDecoder);
    m_hDecoder = 0;
  }
  ZeroMemory(dci, sizeof(*dci));
  dci->ulWidth             = dwWidth;
  dci->ulHeight            = dwHeight;
  dci->ulNumDecodeSurfaces = MAX_DECODE_FRAMES;
  dci->CodecType           = codec;
  dci->ChromaFormat        = cudaVideoChromaFormat_420;
  dci->OutputFormat        = cudaVideoSurfaceFormat_NV12;
  dci->DeinterlaceMode     = (cudaVideoDeinterlaceMode)m_pSettings->GetHWAccelDeintMode();
  dci->ulNumOutputSurfaces = 1;

  dci->ulTargetWidth       = dwDisplayWidth;
  dci->ulTargetHeight      = dwDisplayHeight;

  dci->display_area.left   = (short)rcDisplayArea.left;
  dci->display_area.right  = (short)rcDisplayArea.right;
  dci->display_area.top    = (short)rcDisplayArea.top;
  dci->display_area.bottom = (short)rcDisplayArea.bottom;

  dci->ulCreationFlags     = (m_pD3DDevice && m_pSettings->GetHWAccelDeintHQ()) ? cudaVideoCreate_PreferDXVA : cudaVideoCreate_PreferCUVID;
  dci->vidLock             = m_cudaCtxLock;

  // create the decoder
  CUresult oResult = cuda.cuvidCreateDecoder(&m_hDecoder, dci);
  if (oResult != CUDA_SUCCESS) {
    DbgLog((LOG_ERROR, 10, L"-> Creation of decoder for type %d failed with code %d", dci->CodecType, oResult));
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

int CUDAAPI CDecCuvid::HandleVideoSequence(void *obj, CUVIDEOFORMAT *cuvidfmt)
{
  DbgLog((LOG_TRACE, 10, L"CDecCuvid::HandleVideoSequence(): New Video Sequence"));
  CDecCuvid *filter = static_cast<CDecCuvid *>(obj);

  CUVIDDECODECREATEINFO *dci = &filter->m_VideoDecoderInfo;

  if ((cuvidfmt->codec != dci->CodecType)
    || (cuvidfmt->coded_width != dci->ulWidth)
    || (cuvidfmt->coded_height != dci->ulHeight)
    || (cuvidfmt->display_area.right != dci->ulTargetWidth)
    || (cuvidfmt->display_area.bottom != dci->ulTargetHeight)
    || (cuvidfmt->chroma_format != dci->ChromaFormat)
    || filter->m_bForceSequenceUpdate)
  {
    filter->m_bForceSequenceUpdate = FALSE;
    RECT rcDisplayArea = {cuvidfmt->display_area.left, cuvidfmt->display_area.top, cuvidfmt->display_area.right, cuvidfmt->display_area.bottom};
    filter->CreateCUVIDDecoder(cuvidfmt->codec, cuvidfmt->coded_width, cuvidfmt->coded_height, cuvidfmt->display_area.right, cuvidfmt->display_area.bottom, rcDisplayArea);
  }

  filter->m_bInterlaced = !cuvidfmt->progressive_sequence;
  filter->m_bDoubleRateDeint = FALSE;
  if (filter->m_bInterlaced && cuvidfmt->frame_rate.numerator && cuvidfmt->frame_rate.denominator) {
    double dFrameTime = 10000000.0 / ((double)cuvidfmt->frame_rate.numerator / cuvidfmt->frame_rate.denominator);
    if (filter->m_pSettings->GetHWAccelDeintOutput() == HWDeintOutput_FramePerField && filter->m_VideoDecoderInfo.DeinterlaceMode != cudaVideoDeinterlaceMode_Weave) {
      filter->m_bDoubleRateDeint = TRUE;
      dFrameTime /= 2.0;
    }
    filter->m_rtAvgTimePerFrame = REFERENCE_TIME(dFrameTime + 0.5);
  } else {
    filter->m_rtAvgTimePerFrame = AV_NOPTS_VALUE;
  }
  filter->m_VideoFormat = *cuvidfmt;

  if (cuvidfmt->chroma_format != cudaVideoChromaFormat_420) {
    DbgLog((LOG_TRACE, 10, L"CDecCuvid::HandleVideoSequence(): Incompatible Chroma Format detected"));
    filter->m_bFormatIncompatible = TRUE;
  }

  DXVA2_ExtendedFormat fmt;
  fmt.value = 0;

  if (filter->m_iFullRange != -1)
    fmt.NominalRange = filter->m_iFullRange ? DXVA2_NominalRange_0_255 : DXVA2_NominalRange_16_235;

  // Color Primaries
  switch(cuvidfmt->video_signal_description.color_primaries) {
  case AVCOL_PRI_BT709:
    fmt.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
    break;
  case AVCOL_PRI_BT470M:
    fmt.VideoPrimaries = DXVA2_VideoPrimaries_BT470_2_SysM;
    break;
  case AVCOL_PRI_BT470BG:
    fmt.VideoPrimaries = DXVA2_VideoPrimaries_BT470_2_SysBG;
    break;
  case AVCOL_PRI_SMPTE170M:
    fmt.VideoPrimaries = DXVA2_VideoPrimaries_SMPTE170M;
    break;
  case AVCOL_PRI_SMPTE240M:
    fmt.VideoPrimaries = DXVA2_VideoPrimaries_SMPTE240M;
    break;
  }

  // Color Space / Transfer Matrix
  switch (cuvidfmt->video_signal_description.matrix_coefficients) {
  case AVCOL_SPC_BT709:
    fmt.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT709;
    break;
  case AVCOL_SPC_BT470BG:
  case AVCOL_SPC_SMPTE170M:
    fmt.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT601;
    break;
  case AVCOL_SPC_SMPTE240M:
    fmt.VideoTransferMatrix = DXVA2_VideoTransferMatrix_SMPTE240M;
    break;
  case AVCOL_SPC_YCGCO:
    fmt.VideoTransferMatrix = (DXVA2_VideoTransferMatrix)7;
    break;
  }

  // Color Transfer Function
  switch(cuvidfmt->video_signal_description.transfer_characteristics) {
  case AVCOL_TRC_BT709:
    fmt.VideoTransferFunction = DXVA2_VideoTransFunc_709;
    break;
  case AVCOL_TRC_GAMMA22:
    fmt.VideoTransferFunction = DXVA2_VideoTransFunc_22;
    break;
  case AVCOL_TRC_GAMMA28:
    fmt.VideoTransferFunction = DXVA2_VideoTransFunc_28;
    break;
  case AVCOL_TRC_SMPTE240M:
    fmt.VideoTransferFunction = DXVA2_VideoTransFunc_240M;
    break;
  }

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
    for (int i=0; i<DISPLAY_DELAY; i++) {
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
    flush_pos = (flush_pos + 1) % DISPLAY_DELAY;
  }

  filter->cuda.cuvidCtxLock(filter->m_cudaCtxLock, 0);
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
  filter->m_DisplayPos = (filter->m_DisplayPos + 1) % DISPLAY_DELAY;

  return TRUE;
}

STDMETHODIMP CDecCuvid::Display(CUVIDPARSERDISPINFO *cuviddisp)
{
  if (m_bDoubleRateDeint) {
    if (cuviddisp->progressive_frame) {
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
  HRESULT hr = S_OK;

  CUdeviceptr devPtr = 0;
  unsigned int pitch = 0, width = 0, height = 0;
  CUVIDPROCPARAMS vpp;
  CUresult cuStatus = CUDA_SUCCESS;

  memset(&vpp, 0, sizeof(vpp));
  vpp.progressive_frame = cuviddisp->progressive_frame && !m_pSettings->GetHWAccelDeintForce();

  LAVHWDeintFieldOrder dwFieldOrder = m_pSettings->GetHWAccelDeintFieldOrder();
  if (dwFieldOrder == HWDeintFieldOrder_Auto)
    vpp.top_field_first = cuviddisp->top_field_first;
  else
    vpp.top_field_first = (dwFieldOrder == HWDeintFieldOrder_TopFieldFirst);

  vpp.second_field = (field == 1);

  cuda.cuvidCtxLock(m_cudaCtxLock, 0);
  cuStatus = cuda.cuvidMapVideoFrame(m_hDecoder, cuviddisp->picture_index, &devPtr, &pitch, &vpp);
  if (cuStatus != CUDA_SUCCESS) {
    DbgLog((LOG_CUSTOM1, 1, L"CDecCuvid::Deliver(): cuvidMapVideoFrame failed on index %d", cuviddisp->picture_index));
    goto cuda_fail;
  }

  width = m_VideoDecoderInfo.display_area.right;
  height = m_VideoDecoderInfo.display_area.bottom;
  int size = pitch * height * 3 / 2;

  if(!m_pbRawNV12 || size > m_cRawNV12) {
    if (m_pbRawNV12) {
      cuda.cuMemFreeHost(m_pbRawNV12);
      m_pbRawNV12 = NULL;
      m_cRawNV12 = 0;
    }
    cuStatus = cuda.cuMemAllocHost((void **)&m_pbRawNV12, size);
    if (cuStatus != CUDA_SUCCESS) {
      DbgLog((LOG_CUSTOM1, 1, L"CDecCuvid::Deliver(): cuMemAllocHost failed to allocate %d bytes (%d)", size, cuStatus));
    }
    m_cRawNV12 = size;
  }
  // Copy memory from the device into the staging area
  if (m_pbRawNV12) {
#if USE_ASYNC_COPY
    cuStatus = cuda.cuMemcpyDtoHAsync(m_pbRawNV12, devPtr, size, m_hStream);
    if (cuStatus != CUDA_SUCCESS) {
      DbgLog((LOG_ERROR, 10, L"Async Memory Transfer failed"));
      goto cuda_fail;
    }
    while (CUDA_ERROR_NOT_READY == cuda.cuStreamQuery(m_hStream)) {
      Sleep(1);
    }
#else
    cuStatus = cuda.cuMemcpyDtoH(m_pbRawNV12, devPtr, size);
#endif
  }
  cuda.cuvidUnmapVideoFrame(m_hDecoder, devPtr);
  cuda.cuvidCtxUnlock(m_cudaCtxLock, 0);

  // Setup the LAVFrame
  LAVFrame *pFrame = NULL;
  AllocateFrame(&pFrame);

  if (m_rtAvgTimePerFrame != AV_NOPTS_VALUE) {
    pFrame->avgFrameDuration = m_rtAvgTimePerFrame;
  }

  REFERENCE_TIME rtStart = cuviddisp->timestamp, rtStop = AV_NOPTS_VALUE;
  if (rtStart != AV_NOPTS_VALUE) {
    if (field == 1)
      rtStart += pFrame->avgFrameDuration;

    rtStop = rtStart + pFrame->avgFrameDuration;
    if (field == 2)
      rtStop += pFrame->avgFrameDuration;

    // Sanity check in case the duration is null
    if (rtStop == rtStart)
      rtStop = AV_NOPTS_VALUE;
  }

  pFrame->format = LAVPixFmt_NV12;
  pFrame->width  = width;
  pFrame->height = height;
  pFrame->rtStart = rtStart;
  pFrame->rtStop = rtStop;
  pFrame->repeat = cuviddisp->repeat_first_field;
  pFrame->aspect_ratio.num = m_VideoFormat.display_aspect_ratio.x;
  pFrame->aspect_ratio.den = m_VideoFormat.display_aspect_ratio.y;
  pFrame->ext_format = m_DXVAExtendedFormat;

  // Flag interlaced samples (if not deinterlaced)
  if (cuviddisp->progressive_frame || m_VideoDecoderInfo.DeinterlaceMode != cudaVideoDeinterlaceMode_Weave)
    pFrame->ext_format.SampleFormat = DXVA2_SampleProgressiveFrame;
  else
    pFrame->ext_format.SampleFormat = cuviddisp->top_field_first ? DXVA2_SampleFieldInterleavedEvenFirst : DXVA2_SampleFieldInterleavedOddFirst;

  // Assign the buffer to the LAV Frame bufers
  int Ysize = height * pitch;
  pFrame->data[0] = m_pbRawNV12;
  pFrame->data[1] = m_pbRawNV12+Ysize;
  pFrame->stride[0] = pFrame->stride[1] = pitch;

  m_pCallback->Deliver(pFrame);

  return S_OK;

cuda_fail:
  cuda.cuvidCtxUnlock(m_cudaCtxLock, 0);
  return E_FAIL;
}

STDMETHODIMP CDecCuvid::CheckH264Sequence(const BYTE *buffer, int buflen)
{
  DbgLog((LOG_TRACE, 10, L"CDecCuvid::CheckH264Sequence(): Checking H264 frame for SPS"));
  CH264SequenceParser h264parser;
  h264parser.ParseNALs(buffer, buflen, 0);
  if (h264parser.sps.valid) {
    m_iFullRange = h264parser.sps.full_range;
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

STDMETHODIMP CDecCuvid::Decode(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity)
{
  CUresult result;
  HRESULT hr;

  CUVIDSOURCEDATAPACKET pCuvidPacket;
  ZeroMemory(&pCuvidPacket, sizeof(pCuvidPacket));

  BYTE *pBuffer = NULL;
  if (m_AVC1Converter) {
    int size = 0;
    hr = m_AVC1Converter->Convert(&pBuffer, &size, buffer, buflen);
    if (SUCCEEDED(hr)) {
      pCuvidPacket.payload      = pBuffer;
      pCuvidPacket.payload_size = size;
    }
  } else {
    pCuvidPacket.payload      = buffer;
    pCuvidPacket.payload_size = buflen;
  }

  if (m_bNeedSequenceCheck && m_VideoDecoderInfo.CodecType == cudaVideoCodec_H264) {
    hr = CheckH264Sequence(pCuvidPacket.payload, pCuvidPacket.payload_size);
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
  for (int i=0; i<DISPLAY_DELAY; ++i) {
    if (m_DisplayQueue[m_DisplayPos].picture_index >= 0) {
      m_DisplayQueue[m_DisplayPos].picture_index = -1;
    }
    m_DisplayPos = (m_DisplayPos + 1) % DISPLAY_DELAY;
  }

  m_bFlushing = FALSE;
  m_bWaitForKeyframe = m_bUseTimestampQueue;

  // Re-init decoder after flush
  DecodeSequenceData();

  // Clear timestamp queue
  std::queue<REFERENCE_TIME>().swap(m_timestampQueue);

  return S_OK;
}

STDMETHODIMP CDecCuvid::EndOfStream()
{
  FlushParser();

  // Display all frames left in the queue
  for (int i=0; i<DISPLAY_DELAY; ++i) {
    if (m_DisplayQueue[m_DisplayPos].picture_index >= 0) {
      Display(&m_DisplayQueue[m_DisplayPos]);
      m_DisplayQueue[m_DisplayPos].picture_index = -1;
    }
    m_DisplayPos = (m_DisplayPos + 1) % DISPLAY_DELAY;
  }

  return S_OK;
}

STDMETHODIMP CDecCuvid::GetPixelFormat(LAVPixelFormat *pPix, int *pBpp)
{
  // Output is always NV12
  if (pPix)
    *pPix = LAVPixFmt_NV12;
  if (pBpp)
    *pBpp = 8;
  return S_OK;
}

STDMETHODIMP_(REFERENCE_TIME) CDecCuvid::GetFrameDuration()
{
  return 0;
}
