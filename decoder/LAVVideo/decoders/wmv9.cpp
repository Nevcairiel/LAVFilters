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

#include "parsers/VC1HeaderParser.h"

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
  , m_pNV12Buffer(NULL)
  , m_pNV12BufferSize(0)
  , m_bInterlaced(TRUE)
{
}

CDecWMV9::~CDecWMV9(void)
{
  DestroyDecoder(true);
}

STDMETHODIMP CDecWMV9::DestroyDecoder(bool bFull)
{
  av_freep(&m_pNV12Buffer);
  m_pNV12BufferSize = 0;
  if (bFull) {
    SafeRelease(&m_pDMO);
  }
  return S_OK;
}

// ILAVDecoder
STDMETHODIMP CDecWMV9::Init()
{
  DbgLog((LOG_TRACE, 10, L"CDecWMV9::Init(): Trying to open WMV9 DMO decoder"));
  HRESULT hr = S_OK;

  hr = CoCreateInstance(CLSID_CWMVDecMediaObject, NULL, CLSCTX_INPROC_SERVER, IID_IMediaObject, (void **)&m_pDMO);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Failed to create DMO object"));
    return hr;
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
  
  unsigned int extralen = 0;
  BYTE *extra = NULL;
  getExtraData(*pmt, NULL, &extralen);

  /* Create input type */

  GUID subtype = codec == CODEC_ID_VC1 ? MEDIASUBTYPE_WVC1 : MEDIASUBTYPE_WMV3;

  mtIn.SetType(&MEDIATYPE_Video);
  mtIn.SetSubtype(&subtype);
  mtIn.SetFormatType(&FORMAT_VideoInfo);
  mtIn.SetTemporalCompression(TRUE);
  mtIn.SetSampleSize(0);
  mtIn.SetVariableSize();
  
  VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)mtIn.AllocFormatBuffer(sizeof(VIDEOINFOHEADER) + extralen);
  memset(vih, 0, sizeof(VIDEOINFOHEADER));
  vih->bmiHeader = *pBMI;
  vih->bmiHeader.biPlanes = 1;
  vih->bmiHeader.biBitCount = 24;
  vih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER) + extralen;
  vih->bmiHeader.biCompression = subtype.Data1;
  vih->AvgTimePerFrame = rtAvg;
  SetRect(&vih->rcSource, 0, 0, pBMI->biWidth, pBMI->biHeight);
  vih->rcTarget = vih->rcSource;
  
  if (extralen > 0) {
    extra = (BYTE *)vih + sizeof(VIDEOINFOHEADER);
    getExtraData(*pmt, extra, NULL);
  }

  hr = m_pDMO->SetInputType(0, &mtIn, 0);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Failed to set input type on DMO"));
    return hr;
  }

  /* Create output type */

  mtOut.SetType(&MEDIATYPE_Video);
  mtOut.SetSubtype(&MEDIASUBTYPE_NV12);
  mtOut.SetFormatType(&FORMAT_VideoInfo);
  mtOut.SetTemporalCompression(FALSE);

  vih = (VIDEOINFOHEADER *)mtOut.AllocFormatBuffer(sizeof(VIDEOINFOHEADER));
  memset(vih, 0, sizeof(VIDEOINFOHEADER));
  vih->bmiHeader = *pBMI;
  vih->bmiHeader.biPlanes = 1;
  vih->bmiHeader.biBitCount = 12;
  vih->bmiHeader.biSizeImage = pBMI->biWidth * pBMI->biHeight * 3 / 2;
  vih->bmiHeader.biCompression = FOURCC_NV12;
  vih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  vih->AvgTimePerFrame = rtAvg;
  SetRect(&vih->rcSource, 0, 0, pBMI->biWidth, pBMI->biHeight);
  vih->rcTarget = vih->rcSource;

  mtOut.SetSampleSize(vih->bmiHeader.biSizeImage);

  hr = m_pDMO->SetOutputType(0, &mtOut, 0);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Failed to set output type on DMO"));
    return hr;
  }

  m_pNV12BufferSize = vih->bmiHeader.biSizeImage + FF_INPUT_BUFFER_PADDING_SIZE;
  m_pNV12Buffer = (BYTE* )av_malloc(m_pNV12BufferSize);

  m_bInterlaced = FALSE;
  if (codec == CODEC_ID_VC1 && extralen > 0) {
    CVC1HeaderParser vc1hdr(extra, extralen);
    if (vc1hdr.hdr.valid) {
      m_bInterlaced = vc1hdr.hdr.interlaced;
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

STDMETHODIMP CDecWMV9::ProcessOutput()
{
  HRESULT hr = S_OK;
  DWORD dwStatus = 0;

  CMediaBuffer *pOutBuffer = new CMediaBuffer(m_pNV12Buffer, m_pNV12BufferSize, true);
  pOutBuffer->SetLength(0);

  DMO_OUTPUT_DATA_BUFFER OutputBufferStructs[1];
  memset(&OutputBufferStructs[0], 0, sizeof(DMO_OUTPUT_DATA_BUFFER));
  OutputBufferStructs[0].pBuffer  = pOutBuffer;

  hr = m_pDMO->ProcessOutput(0, 1, OutputBufferStructs, &dwStatus);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> ProcessOutput failed with hr: %x", hr));
    return S_FALSE;
  }
  if (hr == S_FALSE)
    return S_FALSE;

  LAVFrame *pFrame = NULL;
  AllocateFrame(&pFrame);

  BITMAPINFOHEADER *pBMI = NULL;
  videoFormatTypeHandler(mtOut, &pBMI);
  pFrame->width     = pBMI->biWidth;
  pFrame->height    = pBMI->biHeight;
  pFrame->format    = LAVPixFmt_NV12;
  pFrame->key_frame = (OutputBufferStructs[0].dwStatus & DMO_OUTPUT_DATA_BUFFERF_SYNCPOINT);

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
  pFrame->data[0] = m_pNV12Buffer;
  pFrame->data[1] = m_pNV12Buffer + pFrame->width * pFrame->height;
  pFrame->stride[0] = pFrame->stride[1] = pFrame->width;
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
  // Output is always NV12
  if (pPix)
    *pPix = LAVPixFmt_NV12;
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
