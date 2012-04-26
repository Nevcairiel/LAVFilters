/*
 *      Copyright (C) 2010-2012 Hendrik Leppkes
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
#include "wmv9.h"

#include "moreuuids.h"
#include <wmsdk.h>
#include <wmcodecdsp.h>

#include "parsers/VC1HeaderParser.h"
#include "registry.h"

#define VC1_CODE_RES0 0x00000100
#define IS_MARKER(x) (((x) & ~0xFF) == VC1_CODE_RES0)

EXTERN_GUID(CLSID_CWMVDecMediaObject,
    0x82d353df, 0x90bd, 0x4382, 0x8b, 0xc2, 0x3f, 0x61, 0x92, 0xb7, 0x6e, 0x34);

////////////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////////////

ILAVDecoder *CreateDecoderWMV9() {
  return new CDecWMV9();
}

////////////////////////////////////////////////////////////////////////////////
// WMV9 IMediaBuffer implementation
////////////////////////////////////////////////////////////////////////////////

class CMediaBuffer : public IMediaBuffer, public INSSBuffer3
{
public:
  CMediaBuffer(BYTE *pData, DWORD dwLength, bool bNSSBuffer) : m_pData(pData), m_dwLength(dwLength), m_dwMaxLength(dwLength), m_cRef(1), m_bNSSBuffer(bNSSBuffer), m_ContentType(0) {}

  // IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if (riid == IID_IUnknown) {
      AddRef();
      *ppvObject = this;
    } else if (riid == IID_IMediaBuffer) {
      AddRef();
      *ppvObject = (IMediaBuffer *)this;
    } else if (riid == IID_INSSBuffer3 && m_bNSSBuffer) {
      AddRef();
      *ppvObject = (INSSBuffer3 *)this;
    } else {
      return E_NOINTERFACE;
    }
    return S_OK;
  }
  ULONG STDMETHODCALLTYPE AddRef() { InterlockedIncrement( &m_cRef ); return m_cRef; }
  ULONG STDMETHODCALLTYPE Release() { InterlockedDecrement( &m_cRef ); if (m_cRef == 0) { m_cRef++; delete this; return 0; } return m_cRef; }

  // IMediaBuffer
  HRESULT STDMETHODCALLTYPE SetLength(DWORD cbLength) { if (cbLength <= m_dwMaxLength) { m_dwLength = cbLength; return S_OK; } return E_INVALIDARG; }
  HRESULT STDMETHODCALLTYPE GetMaxLength(DWORD *pcbMaxLength) { CheckPointer(pcbMaxLength, E_POINTER); *pcbMaxLength = m_dwMaxLength; return S_OK; }
  HRESULT STDMETHODCALLTYPE GetBufferAndLength(BYTE **ppBuffer, DWORD *pcbLength) { if (ppBuffer) *ppBuffer = m_pData; if (pcbLength) *pcbLength = m_dwLength; return S_OK; }

  // INSSBuffer3
  HRESULT STDMETHODCALLTYPE GetLength(DWORD *pdwLength) { CheckPointer(pdwLength, E_POINTER); *pdwLength = m_dwLength; return S_OK; }
  HRESULT STDMETHODCALLTYPE GetBuffer(BYTE **ppdwBuffer) { CheckPointer(ppdwBuffer, E_POINTER); *ppdwBuffer = m_pData; return S_OK; }
  HRESULT STDMETHODCALLTYPE GetSampleProperties(DWORD cbProperties, BYTE *pbProperties) { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE SetSampleProperties(DWORD cbProperties, BYTE *pbProperties) { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE SetProperty(GUID guidBufferProperty, void *pvBufferProperty, DWORD dwBufferPropertySize)
  {
    CheckPointer(pvBufferProperty, E_INVALIDARG);
    if (dwBufferPropertySize == 0)
      return E_INVALIDARG;

    if (guidBufferProperty == WM_SampleExtensionGUID_ContentType) {
      m_ContentType = *(uint8_t *)pvBufferProperty;
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetProperty(GUID guidBufferProperty, void *pvBufferProperty, DWORD *pdwBufferPropertySize)
  {
    CheckPointer(pdwBufferPropertySize, E_POINTER);
    if (guidBufferProperty == WM_SampleExtensionGUID_ContentType) {
      DWORD dwReqSize = WM_SampleExtension_ContentType_Size;
      if (pvBufferProperty != NULL && *pdwBufferPropertySize >= dwReqSize)
        *((uint8_t*)pvBufferProperty) = m_ContentType;

      *pdwBufferPropertySize = dwReqSize;
      return S_OK;
    }
    return NS_E_UNSUPPORTED_PROPERTY;
  }

private:
  ULONG m_cRef;
  bool  m_bNSSBuffer;
  BYTE *m_pData;
  DWORD m_dwLength;
  DWORD m_dwMaxLength;

  struct _prop {
    void *pPropValue;
    DWORD dwPropSize;
  };

  uint8_t m_ContentType;
};

////////////////////////////////////////////////////////////////////////////////
// WMV9 decoder implementation
////////////////////////////////////////////////////////////////////////////////

CDecWMV9::CDecWMV9(void)
  : CDecBase()
  , m_pDMO(NULL)
  , m_pRawBufferSize(0)
  , m_bInterlaced(TRUE)
  , m_OutPixFmt(LAVPixFmt_None)
{
  memset(&m_StreamAR, 0, sizeof(m_StreamAR));
}

CDecWMV9::~CDecWMV9(void)
{
  DestroyDecoder(true);
}

STDMETHODIMP CDecWMV9::DestroyDecoder(bool bFull)
{
  {
    CAutoLock lock(&m_BufferCritSec);
    for (auto it = m_BufferQueue.begin(); it != m_BufferQueue.end(); it++) {
      av_free((*it)->buffer);
      delete (*it);
    }
    m_BufferQueue.clear();
  }
  m_pRawBufferSize = 0;
  if (bFull) {
    SafeRelease(&m_pDMO);
  }
  return S_OK;
}

BYTE *CDecWMV9::GetBuffer(size_t size)
{
  CAutoLock lock(&m_BufferCritSec);

  Buffer *buffer = NULL;
  for (auto it = m_BufferQueue.begin(); it != m_BufferQueue.end(); it++) {
    if (!(*it)->used) {
      buffer = *it;
      break;
    }
  }
  if (buffer) {
    // Validate Size
    if (buffer->size < size) {
      av_freep(&buffer->buffer);
      buffer->buffer = (BYTE *)av_malloc(size + FF_INPUT_BUFFER_PADDING_SIZE);
      buffer->size = size;
    }
  } else {
    // Create a new buffer
    DbgLog((LOG_TRACE, 10, L"Allocating new buffer for WMV9"));
    buffer = new Buffer();
    buffer->buffer = (BYTE *)av_malloc(size + FF_INPUT_BUFFER_PADDING_SIZE);
    buffer->size = size;
    m_BufferQueue.push_back(buffer);
  }
  buffer->used = 1;
  return buffer->buffer;
}

void CDecWMV9::ReleaseBuffer(BYTE *buffer)
{
  CAutoLock lock(&m_BufferCritSec);
  Buffer *b = NULL;
  for (auto it = m_BufferQueue.begin(); it != m_BufferQueue.end(); it++) {
    if ((*it)->buffer == buffer) {
      b = *it;
      break;
    }
  }
  ASSERT(b);
  b->used = 0;
}

// ILAVDecoder
STDMETHODIMP CDecWMV9::Init()
{
  DbgLog((LOG_TRACE, 10, L"CDecWMV9::Init(): Trying to open WMV9 DMO decoder"));
  HRESULT hr = S_OK;

  // Disable deinterlacing setting in the registry
  // Apparently required on XP
  CRegistry reg;
  reg.Open(HKEY_CURRENT_USER, L"Software\\Microsoft\\Scrunch");
  reg.ReadDWORD(L"Deinterlace.old", hr);
  if (FAILED(hr)) {
    DWORD dwValue = reg.ReadDWORD(L"Deinterlace", hr);
    if (FAILED(hr))
      dwValue = 0;
    reg.WriteDWORD(L"Deinterlace.old", dwValue);
    reg.WriteDWORD(L"Deinterlace", 0);
  }

  hr = CoCreateInstance(CLSID_CWMVDecMediaObject, NULL, CLSCTX_INPROC_SERVER, IID_IMediaObject, (void **)&m_pDMO);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Failed to create DMO object"));
    return hr;
  }

  // Force decoder deinterlacing to off
  IPropertyBag *pProp = NULL;
  hr = m_pDMO->QueryInterface(&pProp);
  if (SUCCEEDED(hr)) {
    VARIANT var = {0};
    var.vt = VT_BOOL;
    var.boolVal = FALSE;
    pProp->Write(g_wszWMVCDecoderDeinterlacing, &var);
    SafeRelease(&pProp);
  }

  return S_OK;
}

STDMETHODIMP CDecWMV9::InitDecoder(CodecID codec, const CMediaType *pmt)
{
  HRESULT hr = S_OK;
  DbgLog((LOG_TRACE, 10, L"CDecWMV9::InitDecoder(): Initializing WMV9 DMO decoder"));

  DestroyDecoder(false);

  BITMAPINFOHEADER *pBMI = NULL;
  REFERENCE_TIME rtAvg = 0;
  DWORD dwARX = 0, dwARY = 0;
  videoFormatTypeHandler(*pmt, &pBMI, &rtAvg, &dwARX, &dwARY);
  
  size_t extralen = 0;
  BYTE *extra = NULL;
  getExtraData(*pmt, NULL, &extralen);
  if (extralen > 0) {
    extra = (BYTE *)av_mallocz(extralen + FF_INPUT_BUFFER_PADDING_SIZE);
    getExtraData(*pmt, extra, &extralen);
  }

  if (codec == CODEC_ID_VC1 && extralen) {
    size_t i = 0;
    for (i = 0; i < (extralen - 4); i++) {
      uint32_t code = AV_RB32(extra+i);
      if (IS_MARKER(code))
        break;
    }
    if (i == 0) {
      memmove(extra+1, extra, extralen);
      *extra = 0;
      extralen++;
    } else if (i > 1) {
      DbgLog((LOG_TRACE, 10, L"-> VC-1 Header at position %u (should be 0 or 1)", i));
    }
  }

  /* Create input type */

  GUID subtype = codec == CODEC_ID_VC1 ? MEDIASUBTYPE_WVC1 : MEDIASUBTYPE_WMV3;

  mtIn.SetType(&MEDIATYPE_Video);
  mtIn.SetSubtype(&subtype);
  mtIn.SetFormatType(&FORMAT_VideoInfo);
  mtIn.SetTemporalCompression(TRUE);
  mtIn.SetSampleSize(0);
  mtIn.SetVariableSize();
  
  VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)mtIn.AllocFormatBuffer((ULONG)(sizeof(VIDEOINFOHEADER) + extralen));
  memset(vih, 0, sizeof(VIDEOINFOHEADER));
  vih->bmiHeader.biWidth       = pBMI->biWidth;
  vih->bmiHeader.biHeight      = pBMI->biHeight;
  vih->bmiHeader.biPlanes      = 1;
  vih->bmiHeader.biBitCount    = 24;
  vih->bmiHeader.biSizeImage   = pBMI->biWidth * pBMI->biHeight * 3 / 2;
  vih->bmiHeader.biSize        = (DWORD)(sizeof(BITMAPINFOHEADER) + extralen);
  vih->bmiHeader.biCompression = subtype.Data1;
  vih->AvgTimePerFrame = rtAvg;
  SetRect(&vih->rcSource, 0, 0, pBMI->biWidth, pBMI->biHeight);
  vih->rcTarget = vih->rcSource;
  
  if (extralen > 0) {
    memcpy((BYTE *)vih + sizeof(VIDEOINFOHEADER), extra, extralen);
    av_freep(&extra);
    extra = (BYTE *)vih + sizeof(VIDEOINFOHEADER);
  }

  hr = m_pDMO->SetInputType(0, &mtIn, 0);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Failed to set input type on DMO"));
    return hr;
  }

  /* Create output type */
  int idx = 0;
  while(SUCCEEDED(hr = m_pDMO->GetOutputType(0, idx++, &mtOut))) {
    if (mtOut.subtype == MEDIASUBTYPE_NV12) {
      hr = m_pDMO->SetOutputType(0, &mtOut, 0);
      m_OutPixFmt = LAVPixFmt_NV12;
      break;
    } else if (mtOut.subtype == MEDIASUBTYPE_YV12) {
      hr = m_pDMO->SetOutputType(0, &mtOut, 0);
      m_OutPixFmt = LAVPixFmt_YUV420;
      break;
    }
  }

  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Failed to set output type on DMO"));
    return hr;
  }

  videoFormatTypeHandler(mtOut, &pBMI);
  m_pRawBufferSize = pBMI->biSizeImage + FF_INPUT_BUFFER_PADDING_SIZE;

  m_bInterlaced = FALSE;
  memset(&m_StreamAR, 0, sizeof(m_StreamAR));
  if (codec == CODEC_ID_VC1 && extralen > 0) {
    CVC1HeaderParser vc1hdr(extra, extralen);
    if (vc1hdr.hdr.valid) {
      m_bInterlaced = vc1hdr.hdr.interlaced;
      m_StreamAR = vc1hdr.hdr.ar;
    }
  }

  return S_OK;
}

STDMETHODIMP CDecWMV9::Decode(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity)
{
  HRESULT hr = S_OK;
  DWORD dwStatus = 0;

  hr = m_pDMO->GetInputStatus(0, &dwStatus);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> GetInputStatus() failed with hr: 0x%x", hr));
    return S_FALSE;
  }
  if (!(dwStatus & DMO_INPUT_STATUSF_ACCEPT_DATA))
    return S_FALSE;

  CMediaBuffer *pInBuffer = new CMediaBuffer(const_cast<BYTE*>(buffer), buflen, false);
  
  DWORD dwFlags = 0;
  if (bSyncPoint)
    dwFlags |= DMO_INPUT_DATA_BUFFERF_SYNCPOINT;
  if (rtStart != AV_NOPTS_VALUE)
    dwFlags |= DMO_INPUT_DATA_BUFFERF_TIME;
  if (rtStop != AV_NOPTS_VALUE)
    dwFlags |= DMO_INPUT_DATA_BUFFERF_TIMELENGTH;

  hr = m_pDMO->ProcessInput(0, pInBuffer, dwFlags, rtStart, rtStop - rtStart);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> ProcessInput failed with hr: %0x%x", hr));
    return E_FAIL;
  }
  if (S_FALSE == hr)
    return S_FALSE;

  SafeRelease(&pInBuffer);
  return ProcessOutput();
}

static void memcpy_plane(BYTE *dst, const BYTE *src, int width, int stride, int height)
{
  for (int i = 0; i < height; i++) {
    memcpy(dst, src, width);
    dst += stride;
    src += width;
  }
}

void CDecWMV9::wmv9_buffer_destruct(LAVFrame *pFrame)
{
  CDecWMV9 *pDec = (CDecWMV9 *)pFrame->priv_data;
  pDec->ReleaseBuffer(pFrame->data[0]);
}

STDMETHODIMP CDecWMV9::ProcessOutput()
{
  HRESULT hr = S_OK;
  DWORD dwStatus = 0;

  BYTE *pBuffer = GetBuffer(m_pRawBufferSize);
  CMediaBuffer *pOutBuffer = new CMediaBuffer(pBuffer, m_pRawBufferSize, true);
  pOutBuffer->SetLength(0);

  DMO_OUTPUT_DATA_BUFFER OutputBufferStructs[1];
  memset(&OutputBufferStructs[0], 0, sizeof(DMO_OUTPUT_DATA_BUFFER));
  OutputBufferStructs[0].pBuffer  = pOutBuffer;

  hr = m_pDMO->ProcessOutput(0, 1, OutputBufferStructs, &dwStatus);
  if (FAILED(hr)) {
    ReleaseBuffer(pBuffer);
    DbgLog((LOG_TRACE, 10, L"-> ProcessOutput failed with hr: %x", hr));
    return S_FALSE;
  }
  if (hr == S_FALSE) {
    ReleaseBuffer(pBuffer);
    return S_FALSE;
  }

  LAVFrame *pFrame = NULL;
  AllocateFrame(&pFrame);

  BITMAPINFOHEADER *pBMI = NULL;
  videoFormatTypeHandler(mtOut, &pBMI);
  pFrame->width     = pBMI->biWidth;
  pFrame->height    = pBMI->biHeight;
  pFrame->format    = m_OutPixFmt;
  pFrame->key_frame = (OutputBufferStructs[0].dwStatus & DMO_OUTPUT_DATA_BUFFERF_SYNCPOINT);

  AVRational display_aspect_ratio;
  int64_t num = (int64_t)m_StreamAR.num * pBMI->biWidth;
  int64_t den = (int64_t)m_StreamAR.den * pBMI->biHeight;
  av_reduce(&display_aspect_ratio.num, &display_aspect_ratio.den, num, den, 1 << 30);

  BYTE contentType = 0;
  DWORD dwPropSize = 1;
  pOutBuffer->GetProperty(WM_SampleExtensionGUID_ContentType, &contentType, &dwPropSize);
  pFrame->interlaced = !!(contentType & WM_CT_INTERLACED);
  pFrame->tff        = !!(contentType & WM_CT_TOP_FIELD_FIRST);
  pFrame->repeat     = !!(contentType & WM_CT_REPEAT_FIRST_FIELD);

  if (pFrame->interlaced && !m_bInterlaced)
    m_bInterlaced = TRUE;

  pFrame->interlaced = (pFrame->interlaced || (m_bInterlaced && m_pSettings->GetDeintAggressive()) || m_pSettings->GetDeintForce()) && !m_pSettings->GetDeintTreatAsProgressive();

  if (OutputBufferStructs[0].dwStatus & DMO_OUTPUT_DATA_BUFFERF_TIME) {
    pFrame->rtStart = OutputBufferStructs[0].rtTimestamp;
    if (OutputBufferStructs[0].dwStatus & DMO_OUTPUT_DATA_BUFFERF_TIMELENGTH) {
      pFrame->rtStop = pFrame->rtStart + OutputBufferStructs[0].rtTimelength;
    }
  }

  // Check alignment
  // If not properly aligned, we need to make the data aligned.
  int alignment = (m_OutPixFmt == LAVPixFmt_NV12) ? 16 : 32;
  if ((pFrame->width % alignment) != 0) {
    AllocLAVFrameBuffers(pFrame);
    size_t ySize = pFrame->width * pFrame->height;
    memcpy_plane(pFrame->data[0], pBuffer, pFrame->width, pFrame->stride[0], pFrame->height);
    if (m_OutPixFmt == LAVPixFmt_NV12) {
      memcpy_plane(pFrame->data[1], pBuffer+ySize, pFrame->width, pFrame->stride[1], pFrame->height / 2);
    } else if (m_OutPixFmt == LAVPixFmt_YUV420) {
      size_t uvSize = ySize / 4;
      memcpy_plane(pFrame->data[2], pBuffer+ySize, pFrame->width / 2, pFrame->stride[2], pFrame->height / 2);
      memcpy_plane(pFrame->data[1], pBuffer+ySize+uvSize, pFrame->width / 2, pFrame->stride[1], pFrame->height / 2);
    }
    ReleaseBuffer(pBuffer);
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
    pFrame->destruct = wmv9_buffer_destruct;
    pFrame->priv_data = this;
  }
  Deliver(pFrame);

  SafeRelease(&pOutBuffer);

  if (OutputBufferStructs[0].dwStatus & DMO_OUTPUT_DATA_BUFFERF_INCOMPLETE)
    return ProcessOutput();
  return hr;
}

STDMETHODIMP CDecWMV9::Flush()
{
  DbgLog((LOG_TRACE, 10, L"CDecWMV9::Flush(): Flushing WMV9 decoder"));
  m_pDMO->Flush();
  return S_OK;
}

STDMETHODIMP CDecWMV9::EndOfStream()
{
  m_pDMO->Discontinuity(0);
  ProcessOutput();
  return S_OK;
}

STDMETHODIMP CDecWMV9::GetPixelFormat(LAVPixelFormat *pPix, int *pBpp)
{
  if (pPix)
    *pPix = (m_OutPixFmt != LAVPixFmt_None) ? m_OutPixFmt : LAVPixFmt_NV12;
  if (pBpp)
    *pBpp = 8;
  return S_OK;
}

STDMETHODIMP_(REFERENCE_TIME) CDecWMV9::GetFrameDuration()
{
  return 0;
}

STDMETHODIMP_(BOOL) CDecWMV9::IsInterlaced()
{
  return !m_pSettings->GetDeintTreatAsProgressive() && (m_bInterlaced || m_pSettings->GetDeintForce());
}
