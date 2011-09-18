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

#include "stdafx.h"
#include "cuvid.h"

#include "moreuuids.h"

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
  , m_hParser(0), m_hDecoder(0)
  , m_bForceSequenceUpdate(FALSE)
  , m_bInterlaced(FALSE)
  , m_aspectX(0), m_aspectY(0)
  , m_bFlushing(FALSE)
  , m_pbRawNV12(NULL), m_cRawNV12(0)
{
  ZeroMemory(&cuda, sizeof(cuda));
}

CDecCuvid::~CDecCuvid(void)
{
  DestroyDecoder(true);
}

STDMETHODIMP CDecCuvid::DestroyDecoder(bool bFull)
{
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

  // Load CUVID function
  cuda.cuvidLib = LoadLibrary(L"nvcuvid.dll");
  if (cuda.cuvidLib == NULL) {
    DbgLog((LOG_TRACE, 10, L"-> Loading nvcuvid.dll failed"));
    return E_FAIL;
  }

  GET_PROC_CUVID(cuvidCtxLockCreate);
  GET_PROC_CUVID(cuvidCtxLockDestroy);
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

  __try {
    result = cuda.cuvidParseVideoData(m_hParser, &pCuvidPacket);
  } __except (1) {
    DbgLog((LOG_ERROR, 10, L"cuvidFlushParser(): cuvidParseVideoData threw an exception"));
    result = CUDA_ERROR_UNKNOWN;
  }
  return result;
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
  unsigned uAdapterCount = m_pD3D->GetAdapterCount();
  for (unsigned lAdapter=0; lAdapter<uAdapterCount; lAdapter++) {
    DbgLog((LOG_TRACE, 10, L"-> Trying D3D Adapter %d..", lAdapter));

    ZeroMemory(&d3dpp, sizeof(d3dpp));

    d3dpp.Windowed               = TRUE;
    d3dpp.MultiSampleType        = D3DMULTISAMPLE_NONE;
    d3dpp.BackBufferWidth        = 1;
    d3dpp.BackBufferHeight       = 1;
    d3dpp.BackBufferCount        = 1;
    d3dpp.BackBufferFormat       = D3DFMT_X8R8G8B8;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_COPY;
    d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_IMMEDIATE;
    d3dpp.Flags                  = D3DPRESENTFLAG_VIDEO;

    IDirect3DDevice9 *pDev = NULL;
    CUcontext cudaCtx = 0;
    hr = m_pD3D->CreateDevice(lAdapter, D3DDEVTYPE_HAL, GetDesktopWindow(), D3DCREATE_FPU_PRESERVE | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &d3dpp, &pDev);
    if (SUCCEEDED(hr)) {
      m_pD3D->GetAdapterIdentifier(lAdapter, 0, &d3dId);
      cuStatus = cuda.cuD3D9CtxCreate(&cudaCtx, &device, CU_CTX_SCHED_BLOCKING_SYNC | CU_CTX_MAP_HOST, pDev);
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
    cuStatus = cuda.cuCtxCreate(&m_cudaContext, CU_CTX_SCHED_BLOCKING_SYNC | CU_CTX_MAP_HOST, best_device);

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
  oVideoParserParameters.ulErrorThreshold       = 0;

  memset(&m_VideoParserExInfo, 0, sizeof(CUVIDEOFORMATEX));

  if (pmt->formattype == FORMAT_MPEG2Video && (pmt->subtype == MEDIASUBTYPE_AVC1 || pmt->subtype == MEDIASUBTYPE_avc1 || pmt->subtype == MEDIASUBTYPE_CCV1)) {
    // TODO: AVC1
  } else {
    getExtraData(pmt->Format(), pmt->FormatType(), pmt->FormatLength(), m_VideoParserExInfo.raw_seqhdr_data, &m_VideoParserExInfo.format.seqhdr_data_length);
  }

  oVideoParserParameters.pExtVideoInfo = &m_VideoParserExInfo;
  CUresult oResult = cuda.cuvidCreateVideoParser(&m_hParser, &oVideoParserParameters);
  if (oResult != CUDA_SUCCESS) {
    DbgLog((LOG_ERROR, 10, L"-> Creating parser for type %d failed with code %d", cudaCodec, oResult));
    return E_FAIL;
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
  dci->DeinterlaceMode     = cudaVideoDeinterlaceMode_Adaptive; // TODO: settings
  dci->ulNumOutputSurfaces = 1;

  dci->ulTargetWidth       = dwDisplayWidth;
  dci->ulTargetHeight      = dwDisplayHeight;

  dci->display_area.left   = (short)rcDisplayArea.left;
  dci->display_area.right  = (short)rcDisplayArea.right;
  dci->display_area.top    = (short)rcDisplayArea.top;
  dci->display_area.bottom = (short)rcDisplayArea.bottom;

  dci->ulCreationFlags     = (m_pD3DDevice/* && m_settings.bDXVA*/) ? cudaVideoCreate_PreferDXVA : cudaVideoCreate_PreferCUVID;
  dci->vidLock             = m_cudaCtxLock;

  // create the decoder
  CUresult oResult = cuda.cuvidCreateDecoder(&m_hDecoder, dci);
  if (oResult != CUDA_SUCCESS) {
    DbgLog((LOG_ERROR, 10, L"-> Creation of decoder for type %d failed with code %d", dci->CodecType, oResult));
    return E_FAIL;
  }

  return S_OK;
}

STDMETHODIMP CDecCuvid::DecodeSequenceData()
{
  CUresult oResult;

  CUVIDSOURCEDATAPACKET pCuvidPacket;
  ZeroMemory(&pCuvidPacket, sizeof(pCuvidPacket));

  pCuvidPacket.payload      = m_VideoParserExInfo.raw_seqhdr_data;
  pCuvidPacket.payload_size = m_VideoParserExInfo.format.seqhdr_data_length;

  if (pCuvidPacket.payload && pCuvidPacket.payload_size)
    oResult = cuda.cuvidParseVideoData(m_hParser, &pCuvidPacket);

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

    filter->m_bInterlaced = !cuvidfmt->progressive_sequence;
  }

  filter->m_aspectX = cuvidfmt->display_aspect_ratio.x;
  filter->m_aspectY = cuvidfmt->display_aspect_ratio.y;

  return TRUE;
}

int CUDAAPI CDecCuvid::HandlePictureDecode(void *obj, CUVIDPICPARAMS *cuvidpic)
{
  CDecCuvid *filter = reinterpret_cast<CDecCuvid *>(obj);

  if (filter->m_bFlushing)
    return FALSE;

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

  return TRUE;
}

int CUDAAPI CDecCuvid::HandlePictureDisplay(void *obj, CUVIDPARSERDISPINFO *cuviddisp)
{
  CDecCuvid *filter = reinterpret_cast<CDecCuvid *>(obj);

  // Drop samples with negative timestamps (preroll)
  if (cuviddisp->timestamp < 0)
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
  if (FALSE && m_bInterlaced /*&& m_settings.bFrameDoubling && m_settings.dwDeinterlace != cudaVideoDeinterlaceMode_Weave*/) {
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
  vpp.progressive_frame = cuviddisp->progressive_frame;

  if (TRUE /*m_settings.dwFieldOrder == 0*/)
    vpp.top_field_first = cuviddisp->top_field_first;
  /*else
    vpp.top_field_first = (m_settings.dwFieldOrder == 1);*/

  vpp.second_field = (field == 1);

  cuda.cuCtxPushCurrent(m_cudaContext);

  cuStatus = cuda.cuvidMapVideoFrame(m_hDecoder, cuviddisp->picture_index, &devPtr, &pitch, &vpp);
  if (cuStatus != CUDA_SUCCESS) {
    DbgLog((LOG_CUSTOM1, 1, L"CDecCuvid::Deliver(): cuvidMapVideoFrame failed on index %d", cuviddisp->picture_index));
    return E_FAIL;
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
    cuStatus = cuda.cuMemcpyDtoH(m_pbRawNV12, devPtr, size);
  }
  cuda.cuvidUnmapVideoFrame(m_hDecoder, devPtr);
  cuda.cuCtxPopCurrent(NULL);

  // Setup the LAVFrame
  LAVFrame *pFrame = NULL;
  AllocateFrame(&pFrame);

  pFrame->format = LAVPixFmt_NV12;
  pFrame->width  = width;
  pFrame->height = height;
  pFrame->rtStart = cuviddisp->timestamp;
  pFrame->rtStop = AV_NOPTS_VALUE;

  // Allocate the buffers for the image
  AllocLAVFrameBuffers(pFrame, pitch);

  // Copy the image from the staging area to the buffer
  int Ysize = height * pitch;
  memcpy(pFrame->data[0], m_pbRawNV12, Ysize);
  memcpy(pFrame->data[1], m_pbRawNV12+Ysize, Ysize >> 1);

  m_pCallback->Deliver(pFrame);

  return S_OK;
}

STDMETHODIMP CDecCuvid::Decode(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity)
{
  CUresult result;

  CUVIDSOURCEDATAPACKET pCuvidPacket;
  ZeroMemory(&pCuvidPacket, sizeof(pCuvidPacket));

  // TODO: process AVC1 into H264/AnnexB
  pCuvidPacket.payload      = buffer;
  pCuvidPacket.payload_size = buflen;

  if (rtStart != AV_NOPTS_VALUE) {
    pCuvidPacket.flags     |= CUVID_PKT_TIMESTAMP;
    pCuvidPacket.timestamp  = rtStart;
  }

  if (bDiscontinuity)
    pCuvidPacket.flags     |= CUVID_PKT_DISCONTINUITY;

  __try {
    result = cuda.cuvidParseVideoData(m_hParser, &pCuvidPacket);
  } __except(1) {
    DbgLog((LOG_ERROR, 10, L"CDecCuvid::Decode(): cuvidParseVideoData threw an exception"));
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

  // Re-init decoder after flush
  DecodeSequenceData();

  return S_OK;
}

STDMETHODIMP CDecCuvid::EndOfStream()
{
  FlushParser();

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
