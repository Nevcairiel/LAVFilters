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
#include "wmv9mft.h"

#include <mfapi.h>
#include <mferror.h>

#include <wmcodecdsp.h>

#include <propvarutil.h>

#include "parsers/VC1HeaderParser.h"

EXTERN_GUID(CLSID_CWMVDecMediaObject,
  0x82d353df, 0x90bd, 0x4382, 0x8b, 0xc2, 0x3f, 0x61, 0x92, 0xb7, 0x6e, 0x34);


////////////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////////////

ILAVDecoder *CreateDecoderWMV9MFT() {
  return new CDecWMV9MFT();
}

////////////////////////////////////////////////////////////////////////////////
// WMV9 MFT decoder implementation
////////////////////////////////////////////////////////////////////////////////

CDecWMV9MFT::CDecWMV9MFT(void)
: CDecBase()
{
  memset(&MF, 0, sizeof(MF));
}

CDecWMV9MFT::~CDecWMV9MFT(void)
{
  DestroyDecoder(true);
}

STDMETHODIMP CDecWMV9MFT::DestroyDecoder(bool bFull)
{
  SAFE_DELETE(m_vc1Header);
  {
    CAutoLock lock(&m_BufferCritSec);
    for (auto it = m_BufferQueue.begin(); it != m_BufferQueue.end(); it++) {
      SafeRelease(&(*it)->pBuffer);
      delete (*it);
    }
    m_BufferQueue.clear();
  }

  if (bFull) {
    SafeRelease(&m_pMFT);
    if (MF.Shutdown)
      MF.Shutdown();
    FreeLibrary(MF.mfplat);
  }
  return S_OK;
}

#define GET_PROC_MF(name)                         \
  MF.name = (tMF##name *)GetProcAddress(MF.mfplat, "MF" #name); \
  if (MF.name == nullptr) {                           \
    DbgLog((LOG_ERROR, 10, L"-> Failed to load function \"%s\"", TEXT("MF") TEXT(#name))); \
    return E_FAIL; \
  }

// ILAVDecoder
STDMETHODIMP CDecWMV9MFT::Init()
{
  DbgLog((LOG_TRACE, 10, L"CDecWMV9MFT::Init(): Trying to open WMV9 MFT decoder"));
  HRESULT hr = S_OK;

  MF.mfplat = LoadLibrary(L"mfplat.dll");
  if (!MF.mfplat) {
    DbgLog((LOG_TRACE, 10, L"-> Failed to load mfplat.dll"));
    return E_FAIL;
  }

  GET_PROC_MF(Startup);
  GET_PROC_MF(Shutdown);
  GET_PROC_MF(CreateMediaType);
  GET_PROC_MF(CreateSample);
  GET_PROC_MF(CreateAlignedMemoryBuffer);
  GET_PROC_MF(AverageTimePerFrameToFrameRate);

  MF.Startup(MF_VERSION, MFSTARTUP_LITE);

  hr = CoCreateInstance(CLSID_CWMVDecMediaObject, nullptr, CLSCTX_INPROC_SERVER, IID_IMFTransform, (void **)&m_pMFT);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Failed to create MFT object"));
    return hr;
  }

  // Force decoder deinterlacing, dxva and FI to off
  IPropertyStore *pProp = nullptr;
  hr = m_pMFT->QueryInterface(&pProp);
  if (SUCCEEDED(hr)) {
    PROPVARIANT variant;
    InitPropVariantFromBoolean(FALSE, &variant);
    pProp->SetValue(MFPKEY_DECODER_DEINTERLACING, variant);
    pProp->SetValue(MFPKEY_DXVA_ENABLED, variant);
    pProp->SetValue(MFPKEY_FI_ENABLED, variant);
    SafeRelease(&pProp);
  }

  return S_OK;
}

static GUID VerifySubtype(AVCodecID codec, GUID subtype)
{
  if (codec == AV_CODEC_ID_WMV3) {
    return MEDIASUBTYPE_WMV3;
  } else {
    if (subtype == MEDIASUBTYPE_WVC1 || subtype == MEDIASUBTYPE_wvc1)
      return MEDIASUBTYPE_WVC1;
    else if (subtype == MEDIASUBTYPE_WMVA || subtype == MEDIASUBTYPE_wmva)
      return MEDIASUBTYPE_WMVA;
    else // fallback
      return MEDIASUBTYPE_WVC1;
  }
}

STDMETHODIMP CDecWMV9MFT::InitDecoder(AVCodecID codec, const CMediaType *pmt)
{
  HRESULT hr = S_OK;
  DbgLog((LOG_TRACE, 10, L"CDecWMV9MFT::InitDecoder(): Initializing WMV9 MFT decoder"));

  DestroyDecoder(false);

  BITMAPINFOHEADER *pBMI = nullptr;
  REFERENCE_TIME rtAvg = 0;
  DWORD dwARX = 0, dwARY = 0;
  videoFormatTypeHandler(*pmt, &pBMI, &rtAvg, &dwARX, &dwARY);

  size_t extralen = 0;
  BYTE *extra = nullptr;
  getExtraData(*pmt, nullptr, &extralen);
  if (extralen > 0) {
    extra = (BYTE *)av_mallocz(extralen + AV_INPUT_BUFFER_PADDING_SIZE);
    getExtraData(*pmt, extra, &extralen);
  }

  if (codec == AV_CODEC_ID_VC1 && extralen) {
    size_t i = 0;
    for (i = 0; i < (extralen - 4); i++) {
      uint32_t code = AV_RB32(extra + i);
      if ((code & ~0xFF) == 0x00000100)
        break;
    }
    if (i == 0) {
      memmove(extra + 1, extra, extralen);
      *extra = 0;
      extralen++;
    } else if (i > 1) {
      DbgLog((LOG_TRACE, 10, L"-> VC-1 Header at position %u (should be 0 or 1)", i));
    }
  }

  if (extralen > 0) {
    m_vc1Header = new CVC1HeaderParser(extra, extralen, codec);
  }

  /* Create input type */
  m_nCodecId = codec;

  IMFMediaType *pMTIn = nullptr;
  MF.CreateMediaType(&pMTIn);
  
  pMTIn->SetUINT32(MF_MT_COMPRESSED, TRUE);
  pMTIn->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, FALSE);
  pMTIn->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, FALSE);

  pMTIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  pMTIn->SetGUID(MF_MT_SUBTYPE,    VerifySubtype(codec, pmt->subtype));

  MFSetAttributeSize(pMTIn, MF_MT_FRAME_SIZE, pBMI->biWidth, pBMI->biHeight);

  UINT32 rateNum = 0, rateDen = 0;
  MF.AverageTimePerFrameToFrameRate(rtAvg, &rateNum, &rateDen);
  MFSetAttributeRatio(pMTIn, MF_MT_FRAME_RATE, rateNum, rateDen);

  if (dwARX != 0 && dwARY != 0)
  {
    int uParX = 1, uParY = 1;
    av_reduce(&uParX, &uParY, dwARX * pBMI->biHeight, dwARY * pBMI->biWidth, INT_MAX);
    MFSetAttributeRatio(pMTIn, MF_MT_PIXEL_ASPECT_RATIO, uParX, uParY);
  }
  
  pMTIn->SetBlob(MF_MT_USER_DATA, extra, (UINT32)extralen);
  av_freep(&extra);

  hr = m_pMFT->SetInputType(0, pMTIn, 0);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Failed to set input type on MFT"));
    return hr;
  }

  /* Create output type */
  hr = SelectOutputType();
  SafeRelease(&pMTIn);

  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Failed to set output type on MFT"));
    return hr;
  }

  IMFMediaType *pMTOut = nullptr;
  m_pMFT->GetOutputCurrentType(0, &pMTOut);
  m_bInterlaced = MFGetAttributeUINT32(pMTOut, MF_MT_INTERLACE_MODE, MFVideoInterlace_Unknown) > MFVideoInterlace_Progressive;
  SafeRelease(&pMTOut);

  m_bManualReorder = (codec == AV_CODEC_ID_VC1) && !(m_pCallback->GetDecodeFlags() & LAV_VIDEO_DEC_FLAG_ONLY_DTS);

  return S_OK;
}

STDMETHODIMP CDecWMV9MFT::SelectOutputType()
{
  HRESULT hr = S_OK;
  int idx = 0;

  m_OutPixFmt = LAVPixFmt_None;
  IMFMediaType *pMTOut = nullptr;
  while (SUCCEEDED(hr = m_pMFT->GetOutputAvailableType(0, idx++, &pMTOut)) && m_OutPixFmt == LAVPixFmt_None) {
    GUID outSubtype;
    if (SUCCEEDED(pMTOut->GetGUID(MF_MT_SUBTYPE, &outSubtype))) {
      if (outSubtype == MEDIASUBTYPE_NV12) {
        hr = m_pMFT->SetOutputType(0, pMTOut, 0);
        m_OutPixFmt = LAVPixFmt_NV12;
        break;
      } else if (outSubtype == MEDIASUBTYPE_YV12) {
        hr = m_pMFT->SetOutputType(0, pMTOut, 0);
        m_OutPixFmt = LAVPixFmt_YUV420;
        break;
      }
    }
    SafeRelease(&pMTOut);
  }

  return hr;
}

IMFMediaBuffer * CDecWMV9MFT::CreateMediaBuffer(const BYTE * pData, DWORD dwDataLen)
{
  HRESULT hr;
  IMFMediaBuffer *pBuffer = nullptr;
  hr = MF.CreateAlignedMemoryBuffer(dwDataLen, MF_16_BYTE_ALIGNMENT, &pBuffer);
  if (FAILED(hr)) {
    DbgLog((LOG_ERROR, 10, L"Unable to allocate MF Media Buffer, hr: 0x%x", hr));
    goto done;
  }

  BYTE * pOutBuffer = nullptr;
  hr = pBuffer->Lock(&pOutBuffer, NULL, NULL);
  if (FAILED(hr)) {
    SafeRelease(&pBuffer);
    DbgLog((LOG_ERROR, 10, L"Unable to lock MF Media Buffer, hr: 0x%x", hr));
    goto done;
  }

  memcpy(pOutBuffer, pData, dwDataLen);

  pBuffer->Unlock();
  pBuffer->SetCurrentLength(dwDataLen);

done:
  return pBuffer;
}

STDMETHODIMP CDecWMV9MFT::Decode(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity, IMediaSample *pMediaSample)
{
  HRESULT hr = S_OK;
  DWORD dwStatus = 0;

  hr = m_pMFT->GetInputStatus(0, &dwStatus);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> GetInputStatus() failed with hr: 0x%x", hr));
    return S_FALSE;
  }
  if (!(dwStatus & MFT_INPUT_STATUS_ACCEPT_DATA))
    return S_FALSE;

  if (m_vc1Header && (m_bManualReorder || m_bNeedKeyFrame)) {
    AVPictureType pictype = m_vc1Header->ParseVC1PictureType(buffer, buflen);
    if (m_bManualReorder) {
      if (pictype == AV_PICTURE_TYPE_I || pictype == AV_PICTURE_TYPE_P) {
        if (m_bReorderBufferValid)
          m_timestampQueue.push(m_rtReorderBuffer);
        m_rtReorderBuffer = rtStart;
        m_bReorderBufferValid = TRUE;
      } else {
        m_timestampQueue.push(rtStart);
      }
    }

    if (m_bNeedKeyFrame) {
      if (pictype != AV_PICTURE_TYPE_I) {
        if (m_bManualReorder)
          m_timestampQueue.pop();
        return S_OK;
      } else {
        m_bNeedKeyFrame = FALSE;
        bSyncPoint = TRUE;
      }
    }
  }

  IMFSample *pSample = nullptr;
  hr = MF.CreateSample(&pSample);
  if (FAILED(hr)) {
    DbgLog((LOG_ERROR, 10, L"Unable to allocate MF Sample, hr: 0x%x", hr));
    return E_FAIL;
  }
  
  IMFMediaBuffer *pMFBuffer = CreateMediaBuffer(buffer, buflen);
  if (!pMFBuffer) {
    DbgLog((LOG_TRACE, 10, L"Unable to allocate media buffer"));
    SafeRelease(&pSample);
    return E_FAIL;
  }

  pSample->AddBuffer(pMFBuffer);

  if (rtStart != AV_NOPTS_VALUE) {
    pSample->SetSampleTime(rtStart);
    if (rtStop != AV_NOPTS_VALUE && rtStop > (rtStart - 1))
      pSample->SetSampleDuration(rtStop - rtStart);
  }

  pSample->SetUINT32(MFSampleExtension_CleanPoint,    bSyncPoint);
  pSample->SetUINT32(MFSampleExtension_Discontinuity, bDiscontinuity);

  hr = m_pMFT->ProcessInput(0, pSample, 0);

  if (hr == MF_E_NOTACCEPTING) {
    // Not accepting data right now, try to process output and try again
    ProcessOutput();
    hr = m_pMFT->ProcessInput(0, pSample, 0);
  }

  SafeRelease(&pMFBuffer);
  SafeRelease(&pSample);

  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> ProcessInput failed with hr: 0x%x", hr));
    return E_FAIL;
  }

  return ProcessOutput();
}

static inline void memcpy_plane(BYTE *dst, const BYTE *src, ptrdiff_t width, ptrdiff_t stride, int height)
{
  for (int i = 0; i < height; i++) {
    memcpy(dst, src, width);
    dst += stride;
    src += width;
  }
}

IMFMediaBuffer * CDecWMV9MFT::GetBuffer(DWORD dwRequiredSize)
{
  CAutoLock lock(&m_BufferCritSec);
  HRESULT hr;

  Buffer *buffer = nullptr;
  for (auto it = m_BufferQueue.begin(); it != m_BufferQueue.end(); it++) {
    if (!(*it)->used) {
      buffer = *it;
      break;
    }
  }
  if (buffer) {
    // Validate Size
    if (buffer->size < dwRequiredSize || !buffer->pBuffer) {
      SafeRelease(&buffer->pBuffer);
      hr = MF.CreateAlignedMemoryBuffer(dwRequiredSize, MF_32_BYTE_ALIGNMENT, &buffer->pBuffer);
      if (FAILED(hr)) return nullptr;
      buffer->size = dwRequiredSize;
    }
  } else {
    // Create a new buffer
    DbgLog((LOG_TRACE, 10, L"Allocating new buffer for WMV9 MFT"));
    buffer = new Buffer();
    hr = MF.CreateAlignedMemoryBuffer(dwRequiredSize, MF_32_BYTE_ALIGNMENT, &buffer->pBuffer);
    if (FAILED(hr)) { delete buffer; return nullptr; }
    buffer->size = dwRequiredSize;
    m_BufferQueue.push_back(buffer);
  }
  buffer->used = 1;
  buffer->pBuffer->AddRef();
  buffer->pBuffer->SetCurrentLength(0);
  return buffer->pBuffer;
}

void CDecWMV9MFT::ReleaseBuffer(IMFMediaBuffer *pBuffer)
{
  CAutoLock lock(&m_BufferCritSec);
  Buffer *buffer = nullptr;
  for (auto it = m_BufferQueue.begin(); it != m_BufferQueue.end(); it++) {
    if ((*it)->pBuffer == pBuffer) {
      (*it)->used = 0;
      break;
    }
  }
  pBuffer->Release();
}

void CDecWMV9MFT::wmv9_buffer_destruct(LAVFrame *pFrame)
{
  CDecWMV9MFT *pDec = (CDecWMV9MFT *)pFrame->priv_data;
  IMFMediaBuffer * pMFBuffer = (IMFMediaBuffer *)pFrame->data[3];
  pMFBuffer->Unlock();
  pDec->ReleaseBuffer(pMFBuffer);
}

STDMETHODIMP CDecWMV9MFT::ProcessOutput()
{
  HRESULT hr = S_OK;
  DWORD dwStatus = 0;

  MFT_OUTPUT_STREAM_INFO outputInfo = {0};
  m_pMFT->GetOutputStreamInfo(0, &outputInfo);

  IMFMediaBuffer *pMFBuffer = nullptr;
  ASSERT(!(outputInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES));

  MFT_OUTPUT_DATA_BUFFER OutputBuffer = {0};
  if (!(outputInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)) {
    pMFBuffer = GetBuffer(outputInfo.cbSize);
    if (!pMFBuffer) { DbgLog((LOG_TRACE, 10, L"Unable to allocate media buffere")); return E_FAIL; }
  
    IMFSample *pSampleOut = nullptr;
    hr = MF.CreateSample(&pSampleOut);
    if (FAILED(hr)) { DbgLog((LOG_TRACE, 10, L"Unable to allocate MF sample, hr: 0x%x", hr)); ReleaseBuffer(pMFBuffer); return E_FAIL; }
    
    pSampleOut->AddBuffer(pMFBuffer);
    OutputBuffer.pSample = pSampleOut;
  }
  hr = m_pMFT->ProcessOutput(0, 1, &OutputBuffer, &dwStatus);

  // We don't process events, just release them
  SafeRelease(&OutputBuffer.pEvents);

  // handle stream format changes
  if (hr == MF_E_TRANSFORM_STREAM_CHANGE || OutputBuffer.dwStatus == MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE ) {
    SafeRelease(&OutputBuffer.pSample);
    ReleaseBuffer(pMFBuffer);
    hr = SelectOutputType();
    if (FAILED(hr)) {
      DbgLog((LOG_TRACE, 10, L"-> Failed to handle stream change, hr: %x", hr));
      return E_FAIL;
    }
    // try again with the new type, it should work now!
    return ProcessOutput();
  }
  
  // the MFT generated no output, discard the sample and return
  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT || OutputBuffer.dwStatus == MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE) {
    SafeRelease(&OutputBuffer.pSample);
    ReleaseBuffer(pMFBuffer);
    return S_FALSE;
  }
  
  // unknown error condition
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> ProcessOutput failed with hr: %x", hr));
    SafeRelease(&OutputBuffer.pSample);
    ReleaseBuffer(pMFBuffer);
    return E_FAIL;
  }

  LAVFrame *pFrame = nullptr;
  AllocateFrame(&pFrame);

  IMFMediaType *pMTOut = nullptr;
  m_pMFT->GetOutputCurrentType(0, &pMTOut);

  MFGetAttributeSize(pMTOut, MF_MT_FRAME_SIZE, (UINT32 *)&pFrame->width, (UINT32 *)&pFrame->height);
  pFrame->format = m_OutPixFmt;

  AVRational pixel_aspect_ratio = {1, 1};
  MFGetAttributeRatio(pMTOut, MF_MT_PIXEL_ASPECT_RATIO, (UINT32*)&pixel_aspect_ratio.num, (UINT32*)&pixel_aspect_ratio.den);

  AVRational display_aspect_ratio = {0, 0};
  av_reduce(&display_aspect_ratio.num, &display_aspect_ratio.den, (int64_t)pixel_aspect_ratio.num * pFrame->width, (int64_t)pixel_aspect_ratio.den * pFrame->height, INT_MAX);
  pFrame->aspect_ratio = display_aspect_ratio;

  pFrame->interlaced = MFGetAttributeUINT32(OutputBuffer.pSample, MFSampleExtension_Interlaced,       FALSE);
  pFrame->repeat     = MFGetAttributeUINT32(OutputBuffer.pSample, MFSampleExtension_RepeatFirstField, FALSE);

  LAVDeintFieldOrder fo = m_pSettings->GetDeintFieldOrder();
  pFrame->tff = (fo == DeintFieldOrder_Auto) ? !MFGetAttributeUINT32(OutputBuffer.pSample, MFSampleExtension_BottomFieldFirst, FALSE) : (fo == DeintFieldOrder_TopFieldFirst);

  if (pFrame->interlaced && !m_bInterlaced)
    m_bInterlaced = TRUE;

  pFrame->interlaced = (pFrame->interlaced || (m_bInterlaced && m_pSettings->GetDeinterlacingMode() == DeintMode_Aggressive) || m_pSettings->GetDeinterlacingMode() == DeintMode_Force) && !(m_pSettings->GetDeinterlacingMode() == DeintMode_Disable);

  pFrame->ext_format.VideoPrimaries         = MFGetAttributeUINT32(pMTOut, MF_MT_VIDEO_PRIMARIES,     MFVideoPrimaries_Unknown);
  pFrame->ext_format.VideoTransferFunction  = MFGetAttributeUINT32(pMTOut, MF_MT_TRANSFER_FUNCTION,   MFVideoTransFunc_Unknown);
  pFrame->ext_format.VideoTransferMatrix    = MFGetAttributeUINT32(pMTOut, MF_MT_YUV_MATRIX,          MFVideoTransferMatrix_Unknown);
  pFrame->ext_format.VideoChromaSubsampling = MFGetAttributeUINT32(pMTOut, MF_MT_VIDEO_CHROMA_SITING, MFVideoChromaSubsampling_Unknown);
  pFrame->ext_format.NominalRange           = MFGetAttributeUINT32(pMTOut, MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_Unknown);

  // HACK: don't flag range=limited if its the only value set, since its also the implied default, this helps to avoid a reconnect
  // The MFT always sets this value, even if the bitstream says nothing about it, causing a reconnect on every vc1/wmv3 file
  if (pFrame->ext_format.value == 0x2000)
    pFrame->ext_format.value = 0;

  // Timestamps
  if (m_bManualReorder) {
    if (!m_timestampQueue.empty()) {
      pFrame->rtStart = m_timestampQueue.front();
      m_timestampQueue.pop();
      
      LONGLONG llDuration = 0;
      hr = OutputBuffer.pSample->GetSampleDuration(&llDuration);
      if (SUCCEEDED(hr) && llDuration > 0) {
        pFrame->rtStop = pFrame->rtStart + llDuration;
      }
    }
  } else {
    LONGLONG llTimestamp = 0;
    hr = OutputBuffer.pSample->GetSampleTime(&llTimestamp);
    if (SUCCEEDED(hr)) {
      pFrame->rtStart = llTimestamp;
      
      LONGLONG llDuration = 0;
      hr = OutputBuffer.pSample->GetSampleDuration(&llDuration);
      if (SUCCEEDED(hr) && llDuration > 0) {
        pFrame->rtStop = pFrame->rtStart + llDuration;
      }
    }
  }

  SafeRelease(&pMTOut);

  // Lock memory in the buffer
  BYTE *pBuffer = nullptr;
  pMFBuffer->Lock(&pBuffer, NULL, NULL);

  // Check alignment
  // If not properly aligned, we need to make the data aligned.
  int alignment = (m_OutPixFmt == LAVPixFmt_NV12) ? 16 : 32;
  if ((pFrame->width % alignment) != 0) {
    hr = AllocLAVFrameBuffers(pFrame);
    if (FAILED(hr)) {
      pMFBuffer->Unlock();
      ReleaseBuffer(pMFBuffer);
      SafeRelease(&OutputBuffer.pSample);
      return hr;
    }
    size_t ySize = pFrame->width * pFrame->height;
    
    memcpy_plane(pFrame->data[0], pBuffer, pFrame->width, pFrame->stride[0], pFrame->height);
    if (m_OutPixFmt == LAVPixFmt_NV12) {
      memcpy_plane(pFrame->data[1], pBuffer + ySize, pFrame->width, pFrame->stride[1], pFrame->height / 2);
    } else if (m_OutPixFmt == LAVPixFmt_YUV420) {
      size_t uvSize = ySize / 4;
      memcpy_plane(pFrame->data[2], pBuffer + ySize, pFrame->width / 2, pFrame->stride[2], pFrame->height / 2);
      memcpy_plane(pFrame->data[1], pBuffer + ySize + uvSize, pFrame->width / 2, pFrame->stride[1], pFrame->height / 2);
    }
    pMFBuffer->Unlock();
    ReleaseBuffer(pMFBuffer);
  } else {
    if (m_OutPixFmt == LAVPixFmt_NV12) {
      pFrame->data[0] = pBuffer;
      pFrame->data[1] = pBuffer + pFrame->width * pFrame->height;
      pFrame->stride[0] = pFrame->stride[1] = pFrame->width;
    } else if (m_OutPixFmt == LAVPixFmt_YUV420) {
      pFrame->data[0] = pBuffer;
      pFrame->data[2] = pBuffer + pFrame->width * pFrame->height;
      pFrame->data[1] = pFrame->data[2] + (pFrame->width / 2) * (pFrame->height / 2);
      pFrame->stride[0] = pFrame->width;
      pFrame->stride[1] = pFrame->stride[2] = pFrame->width / 2;
    }
    pFrame->data[3] = (BYTE *)pMFBuffer;
    pFrame->destruct = wmv9_buffer_destruct;
    pFrame->priv_data = this;
  }
  pFrame->flags |= LAV_FRAME_FLAG_BUFFER_MODIFY;
  Deliver(pFrame);

  SafeRelease(&OutputBuffer.pSample);

  if (OutputBuffer.dwStatus == MFT_OUTPUT_DATA_BUFFER_INCOMPLETE)
    return ProcessOutput();
  return hr;
}

STDMETHODIMP CDecWMV9MFT::Flush()
{
  DbgLog((LOG_TRACE, 10, L"CDecWMV9MFT::Flush(): Flushing WMV9 decoder"));
  m_pMFT->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);

  std::queue<REFERENCE_TIME>().swap(m_timestampQueue);
  m_rtReorderBuffer = AV_NOPTS_VALUE;
  m_bReorderBufferValid = FALSE;
  m_bNeedKeyFrame = TRUE;

  return __super::Flush();
}

STDMETHODIMP CDecWMV9MFT::EndOfStream()
{
  if (m_bReorderBufferValid)
    m_timestampQueue.push(m_rtReorderBuffer);
  m_bReorderBufferValid = FALSE;
  m_rtReorderBuffer = AV_NOPTS_VALUE;

  m_pMFT->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
  ProcessOutput();
  return S_OK;
}

STDMETHODIMP CDecWMV9MFT::GetPixelFormat(LAVPixelFormat *pPix, int *pBpp)
{
  if (pPix)
    *pPix = (m_OutPixFmt != LAVPixFmt_None) ? m_OutPixFmt : LAVPixFmt_NV12;
  if (pBpp)
    *pBpp = 8;
  return S_OK;
}

STDMETHODIMP_(BOOL) CDecWMV9MFT::IsInterlaced(BOOL bAllowGuess)
{
  return (m_bInterlaced || m_pSettings->GetDeinterlacingMode() == DeintMode_Force);
}
