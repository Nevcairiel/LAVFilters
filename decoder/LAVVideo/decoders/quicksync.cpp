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
#include "quicksync.h"

#include "moreuuids.h"

#include "parsers/H264SequenceParser.h"
#include "parsers/MPEG2HeaderParser.h"
#include "parsers/VC1HeaderParser.h"

#include <Shlwapi.h>

////////////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////////////

ILAVDecoder *CreateDecoderQuickSync() {
  return new CDecQuickSync();
}

////////////////////////////////////////////////////////////////////////////////
// Codec FourCC map
////////////////////////////////////////////////////////////////////////////////

static FOURCC FourCC_MPG1 = mmioFOURCC('M','P','G','1');
static FOURCC FourCC_MPG2 = mmioFOURCC('M','P','G','2');
static FOURCC FourCC_VC1  = mmioFOURCC('W','V','C','1');
static FOURCC FourCC_WMV3 = mmioFOURCC('W','M','V','3');
static FOURCC FourCC_H264 = mmioFOURCC('H','2','6','4');
static FOURCC FourCC_AVC1 = mmioFOURCC('A','V','C','1');

static struct {
  CodecID ffcodec;
  FOURCC fourCC;
} quicksync_codecs[] = {
  { CODEC_ID_MPEG1VIDEO, FourCC_MPG1 },
  { CODEC_ID_MPEG2VIDEO, FourCC_MPG2 },
  { CODEC_ID_VC1,        FourCC_VC1  },
  { CODEC_ID_WMV3,       FourCC_WMV3 },
  { CODEC_ID_H264,       FourCC_H264 },
};


////////////////////////////////////////////////////////////////////////////////
// QuickSync decoder implementation
////////////////////////////////////////////////////////////////////////////////

CDecQuickSync::CDecQuickSync(void)
  : CDecBase()
  , m_bInterlaced(TRUE)
  , m_pDecoder(NULL)
  , m_bAVC1(FALSE)
  , m_nAVCNalSize(0)
{
  ZeroMemory(&qs, sizeof(qs));
}

CDecQuickSync::~CDecQuickSync(void)
{
  DestroyDecoder(true);
}

STDMETHODIMP CDecQuickSync::DestroyDecoder(bool bFull)
{
  if (bFull) {
    if (m_pDecoder) {
      qs.destroy(m_pDecoder);
      m_pDecoder = NULL;
    }

    FreeLibrary(qs.quickSyncLib);
  }

  return S_OK;
}

// ILAVDecoder
STDMETHODIMP CDecQuickSync::Init()
{
  DbgLog((LOG_TRACE, 10, L"CDecQuickSync::Init(): Trying to open QuickSync decoder"));
  HRESULT hr = S_OK;

  int flags = av_get_cpu_flags();
  if (!(flags & AV_CPU_FLAG_SSE4)) {
    DbgLog((LOG_TRACE, 10, L"-> CPU is not SSE 4.1 capable, this is not even worth a try...."));
    return E_FAIL;
  }

  WCHAR wModuleFile[1024];
  GetModuleFileName(g_hInst, wModuleFile, 1024);
  PathRemoveFileSpecW(wModuleFile);

  wcscat_s(wModuleFile, L"\\");
  wcscat_s(wModuleFile, TEXT(QS_DEC_DLL_NAME));

  qs.quickSyncLib = LoadLibrary(wModuleFile);
  if (qs.quickSyncLib == NULL) {
    DWORD dwError = GetLastError();
    DbgLog((LOG_ERROR, 10, L"-> Loading of " TEXT(QS_DEC_DLL_NAME) L" failed (%d)", dwError));
    return E_FAIL;
  }

  qs.create = (pcreateQuickSync *)GetProcAddress(qs.quickSyncLib, "createQuickSync");
  if (qs.create == NULL) {
    DbgLog((LOG_ERROR, 10, L"-> Failed to load function \"createQuickSync\""));
    return E_FAIL;
  }
  qs.destroy = (pdestroyQuickSync *)GetProcAddress(qs.quickSyncLib, "destroyQuickSync");
  if (qs.destroy == NULL) {
    DbgLog((LOG_ERROR, 10, L"-> Failed to load function \"destroyQuickSync\""));
    return E_FAIL;
  }

  m_pDecoder = qs.create();
  if (m_pDecoder == NULL || !m_pDecoder->getOK()) {
    DbgLog((LOG_ERROR, 10, L"-> Creation of decoder failed"));
    return E_FAIL;
  }

  m_pDecoder->SetDeliverSurfaceCallback(this, &QS_DeliverSurfaceCallback);

  return S_OK;
}

STDMETHODIMP CDecQuickSync::InitDecoder(CodecID codec, const CMediaType *pmt)
{
  HRESULT hr = S_OK;
  DbgLog((LOG_TRACE, 10, L"CDecQuickSync::InitDecoder(): Initializing QuickSync decoder"));

  FOURCC fourCC = (FOURCC)0;
  for (int i = 0; i < countof(quicksync_codecs); i++) {
    if (quicksync_codecs[i].ffcodec == codec) {
      fourCC = quicksync_codecs[i].fourCC;
      break;
    }
  }

  if (fourCC == 0) {
    DbgLog((LOG_TRACE, 10, L"-> Codec id %d does not map to a QuickSync FourCC codec", codec));
    return E_FAIL;
  }

  if (pmt->subtype == MEDIASUBTYPE_AVC1 || pmt->subtype == MEDIASUBTYPE_avc1 || pmt->subtype == MEDIASUBTYPE_CCV1) {
    if (pmt->formattype == FORMAT_MPEG2Video) {
      MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)pmt->pbFormat;
      fourCC = FourCC_AVC1;
      m_bAVC1 = TRUE;
      m_nAVCNalSize = mp2vi->dwFlags;
    } else {
      DbgLog((LOG_TRACE, 10, L"-> AVC1 without MPEG2VIDEOINFO not supported"));
      return E_FAIL;
    }
  }

  CQsConfig qsConfig;
  m_pDecoder->GetConfig(&qsConfig);
  qsConfig.bTimeStampCorrection = false;
  qsConfig.bMod16Width = false;
  qsConfig.nOutputQueueLength = 0;
  m_pDecoder->SetConfig(&qsConfig);

  hr = m_pDecoder->TestMediaType(pmt, fourCC);
  if (hr != S_OK) {
    DbgLog((LOG_TRACE, 10, L"-> TestMediaType failed"));
    return E_FAIL;
  }

  hr = m_pDecoder->InitDecoder(pmt, fourCC);
  if (hr != S_OK) {
    DbgLog((LOG_TRACE, 10, L"-> InitDecoder failed"));
    return E_FAIL;
  }

  m_Codec = fourCC;

  return S_OK;
}

STDMETHODIMP CDecQuickSync::Decode(IMediaSample *pSample)
{
  HRESULT hr;

  hr = m_pDecoder->Decode(pSample);

  return hr;
}

HRESULT CDecQuickSync::QS_DeliverSurfaceCallback(void* obj, QsFrameData* data)
{
  CDecQuickSync *filter = (CDecQuickSync *)obj;
  filter->HandleFrame(data);

  return S_OK;
}

STDMETHODIMP CDecQuickSync::HandleFrame(QsFrameData *data)
{
  if (data->rtStart != AV_NOPTS_VALUE && data->rtStart < 0)
    return S_OK;

  // Setup the LAVFrame
  LAVFrame *pFrame = NULL;
  AllocateFrame(&pFrame);

  pFrame->format = LAVPixFmt_NV12;
  pFrame->width  = data->rcClip.right - data->rcClip.left + 1;
  pFrame->height = data->rcClip.bottom - data->rcClip.top + 1;
  pFrame->rtStart = data->rtStart;
  pFrame->rtStop = data->rtStop;
  pFrame->repeat = !!(data->dwInterlaceFlags & AM_VIDEO_FLAG_REPEAT_FIELD);
  pFrame->aspect_ratio.num = data->dwPictAspectRatioX;
  pFrame->aspect_ratio.den = data->dwPictAspectRatioY;
  //pFrame->ext_format = (DXVA2_ExtendedFormat)0;
  pFrame->interlaced = !(data->dwInterlaceFlags & AM_VIDEO_FLAG_WEAVE);
  pFrame->tff = !!(data->dwInterlaceFlags & AM_VIDEO_FLAG_FIELD1FIRST);

  // Assign the buffer to the LAV Frame bufers
  pFrame->data[0] = data->y;
  pFrame->data[1] = data->u;
  pFrame->stride[0] = pFrame->stride[1] = data->dwStride;

  if (!m_bInterlaced && pFrame->interlaced)
    m_bInterlaced = TRUE;

  m_pCallback->Deliver(pFrame);

  return S_OK;
}

STDMETHODIMP CDecQuickSync::Flush()
{
  DbgLog((LOG_TRACE, 10, L"CDecQuickSync::Flush(): Flushing QuickSync decoder"));

  m_pDecoder->BeginFlush();
  m_pDecoder->OnSeek(0);
  m_pDecoder->EndFlush();

  return S_OK;
}

STDMETHODIMP CDecQuickSync::EndOfStream()
{
  m_pDecoder->Flush(true);
  return S_OK;
}

STDMETHODIMP CDecQuickSync::GetPixelFormat(LAVPixelFormat *pPix, int *pBpp)
{
  // Output is always NV12
  if (pPix)
    *pPix = LAVPixFmt_NV12;
  if (pBpp)
    *pBpp = 8;
  return S_OK;
}

STDMETHODIMP_(REFERENCE_TIME) CDecQuickSync::GetFrameDuration()
{
  return 0;
}

STDMETHODIMP_(BOOL) CDecQuickSync::IsInterlaced()
{
  return !m_pSettings->GetDeintTreatAsProgressive() && (m_bInterlaced || m_pSettings->GetDeintForce());
}
