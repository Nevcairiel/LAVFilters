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
#include "LAVVideo.h"
#include "VideoSettingsProp.h"
#include "Media.h"
#include <dvdmedia.h>

#include "moreuuids.h"
#include "registry.h"

#include <Shlwapi.h>

#include "parsers/VC1HeaderParser.h"
#include "parsers/MPEG2HeaderParser.h"

static int ff_lockmgr(void **mutex, enum AVLockOp op)
{
  DbgLog((LOG_TRACE, 10, L"ff_lockmgr: mutex: %p, op: %d", *mutex, op));
  CRITICAL_SECTION **critSec = (CRITICAL_SECTION **)mutex;
  switch (op) {
  case AV_LOCK_CREATE:
    *critSec = new CRITICAL_SECTION();
    InitializeCriticalSection(*critSec);
    break;
  case AV_LOCK_OBTAIN:
    EnterCriticalSection(*critSec);
    break;
  case AV_LOCK_RELEASE:
    LeaveCriticalSection(*critSec);
    break;
  case AV_LOCK_DESTROY:
    DeleteCriticalSection(*critSec);
    SAFE_DELETE(*critSec);
    break;
  }
  return 0;
}

CLAVVideo::CLAVVideo(LPUNKNOWN pUnk, HRESULT* phr)
  : CTransformFilter(NAME("LAV Video Decoder"), 0, __uuidof(CLAVVideo))
  , m_pDecoder(NULL)
  , m_rtPrevStart(0)
  , m_rtPrevStop(0)
  , m_bRuntimeConfig(FALSE)
  , m_bForceInputAR(FALSE)
  , m_bSendMediaType(FALSE)
  , m_bHWDecoder(FALSE), m_bHWDecoderFailed(FALSE)
  , m_bDXVAExtFormatSupport(-1)
{
  avcodec_register_all();
  if (av_lockmgr_addref() == 1)
    av_lockmgr_register(ff_lockmgr);

  LoadSettings();

  m_PixFmtConverter.SetSettings(this);

#ifdef DEBUG
  DbgSetModuleLevel (LOG_TRACE, DWORD_MAX);
  DbgSetModuleLevel (LOG_ERROR, DWORD_MAX);
  DbgSetModuleLevel (LOG_CUSTOM1, DWORD_MAX); // FFMPEG messages use custom1
#if ENABLE_DEBUG_LOGFILE
  DbgSetLogFileDesktop(LAVC_VIDEO_LOG_FILE);
#endif
#endif
}

CLAVVideo::~CLAVVideo()
{
  SAFE_DELETE(m_pDecoder);

  // If its 0, that means we're the last one using/owning the object, and can free it
  if(av_lockmgr_release() == 0)
    av_lockmgr_register(NULL);
}

HRESULT CLAVVideo::LoadDefaults()
{
  // Query OS version info
  OSVERSIONINFO os;
  ZeroMemory(&os, sizeof(os));
  os.dwOSVersionInfoSize = sizeof(os);
  GetVersionEx(&os);

  // Set Defaults
  m_settings.StreamAR = TRUE;
  m_settings.InterlacedFlags = TRUE;
  m_settings.NumThreads = 0;
  m_settings.HighQualityPixConv = FALSE;
  m_settings.RGBRange = 2; // Full range default

  for (int i = 0; i < Codec_NB; ++i)
    m_settings.bFormats[i] = TRUE;

  m_settings.bFormats[Codec_VC1]      = FALSE;
  m_settings.bFormats[Codec_RV12]     = FALSE;
  m_settings.bFormats[Codec_Lagarith] = FALSE;
  m_settings.bFormats[Codec_Cinepak]  = FALSE;
  m_settings.bFormats[Codec_QPEG]     = FALSE;
  m_settings.bFormats[Codec_MSRLE]    = FALSE;

  for (int i = 0; i < LAVOutPixFmt_NB; ++i)
    m_settings.bPixFmts[i] = TRUE;

  m_settings.HWAccel = HWAccel_None;
  for (int i = 0; i < HWCodec_NB; ++i)
    m_settings.bHWFormats[i] = TRUE;

  m_settings.HWDeintMode = HWDeintMode_Hardware;
  m_settings.HWDeintOutput = HWDeintOutput_FramePerField;
  m_settings.HWDeintHQ = (os.dwMajorVersion >= 6); // Activate by default on Vista and above, on XP it causes issues
  m_settings.HWDeintFieldOrder = HWDeintFieldOrder_Auto;
  m_settings.HWDeintForce = FALSE;

  return S_OK;
}

static const WCHAR* pixFmtSettingsMap[LAVOutPixFmt_NB] = {
  L"yv12", L"nv12", L"yuy2", L"uyvy", L"ayuv", L"p010", L"p210", L"y410", L"p016", L"p216", L"y416", L"rgb32", L"rgb24"
};

HRESULT CLAVVideo::LoadSettings()
{
  LoadDefaults();
  if (m_bRuntimeConfig)
    return S_FALSE;

  HRESULT hr;
  BOOL bFlag;
  DWORD dwVal;

  CreateRegistryKey(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY);
  CRegistry reg = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY, hr);
  // We don't check if opening succeeded, because the read functions will set their hr accordingly anyway,
  // and we need to fill the settings with defaults.
  // ReadString returns an empty string in case of failure, so thats fine!

  bFlag = reg.ReadDWORD(L"StreamAR", hr);
  if (SUCCEEDED(hr)) m_settings.StreamAR = bFlag;

  bFlag = reg.ReadDWORD(L"InterlacedFlags", hr);
  if (SUCCEEDED(hr)) m_settings.InterlacedFlags = bFlag;

  dwVal = reg.ReadDWORD(L"NumThreads", hr);
  if (SUCCEEDED(hr)) m_settings.NumThreads = dwVal;

  bFlag = reg.ReadDWORD(L"HighQualityPixConv", hr);
  if (SUCCEEDED(hr)) m_settings.HighQualityPixConv = bFlag;

  dwVal = reg.ReadDWORD(L"RGBRange", hr);
  if (SUCCEEDED(hr)) m_settings.RGBRange = dwVal;

  CreateRegistryKey(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_FORMATS);
  CRegistry regF = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_FORMATS, hr);

  for (int i = 0; i < Codec_NB; ++i) {
    const codec_config_t *info = get_codec_config((LAVVideoCodec)i);
    bFlag = regF.ReadBOOL(info->name, hr);
    if (SUCCEEDED(hr)) m_settings.bFormats[i] = bFlag;
  }

  CreateRegistryKey(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_OUTPUT);
  CRegistry regP = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_OUTPUT, hr);

  for (int i = 0; i < LAVOutPixFmt_NB; ++i) {
    bFlag = regP.ReadBOOL(pixFmtSettingsMap[i], hr);
    if (SUCCEEDED(hr)) m_settings.bPixFmts[i] = bFlag;
  }

  CreateRegistryKey(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_HWACCEL);
  CRegistry regHW = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_HWACCEL, hr);

  dwVal = regHW.ReadDWORD(L"HWAccel", hr);
  if (SUCCEEDED(hr)) m_settings.HWAccel = dwVal;

  bFlag = regHW.ReadBOOL(L"h264", hr);
  if (SUCCEEDED(hr)) m_settings.bHWFormats[HWCodec_H264] = bFlag;

  bFlag = regHW.ReadBOOL(L"vc1", hr);
  if (SUCCEEDED(hr)) m_settings.bHWFormats[HWCodec_VC1] = bFlag;

  bFlag = regHW.ReadBOOL(L"mpeg2", hr);
  if (SUCCEEDED(hr)) m_settings.bHWFormats[HWCodec_MPEG2] = bFlag;

  dwVal = regHW.ReadDWORD(L"HWDeintMode", hr);
  if (SUCCEEDED(hr)) m_settings.HWDeintMode = dwVal;

  dwVal = regHW.ReadDWORD(L"HWDeintOutput", hr);
  if (SUCCEEDED(hr)) m_settings.HWDeintOutput = dwVal;

  bFlag = regHW.ReadBOOL(L"HWDeintHQ", hr);
  if (SUCCEEDED(hr)) m_settings.HWDeintHQ = bFlag;

  dwVal = regHW.ReadDWORD(L"HWDeintFieldOrder", hr);
  if (SUCCEEDED(hr)) m_settings.HWDeintFieldOrder = dwVal;

  bFlag = regHW.ReadBOOL(L"HWDeintForce", hr);
  if (SUCCEEDED(hr)) m_settings.HWDeintForce = bFlag;

  return S_OK;
}

HRESULT CLAVVideo::SaveSettings()
{
  if (m_bRuntimeConfig)
    return S_FALSE;

  HRESULT hr;
  CRegistry reg = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY, hr);
  if (SUCCEEDED(hr)) {
    reg.WriteBOOL(L"StreamAR", m_settings.StreamAR);
    reg.WriteBOOL(L"InterlacedFlags", m_settings.InterlacedFlags);
    reg.WriteDWORD(L"NumThreads", m_settings.NumThreads);
    reg.WriteBOOL(L"HighQualityPixConv", m_settings.HighQualityPixConv);
    reg.WriteDWORD(L"RGBRange", m_settings.RGBRange);

    CRegistry regF = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_FORMATS, hr);
    for (int i = 0; i < Codec_NB; ++i) {
      const codec_config_t *info = get_codec_config((LAVVideoCodec)i);
      regF.WriteBOOL(info->name, m_settings.bFormats[i]);
    }

    CRegistry regP = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_OUTPUT, hr);
    for (int i = 0; i < LAVOutPixFmt_NB; ++i) {
      regP.WriteBOOL(pixFmtSettingsMap[i], m_settings.bPixFmts[i]);
    }

    CRegistry regHW = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_HWACCEL, hr);
    regHW.WriteDWORD(L"HWAccel", m_settings.HWAccel);
    regHW.WriteBOOL(L"h264", m_settings.bHWFormats[HWCodec_H264]);
    regHW.WriteBOOL(L"vc1", m_settings.bHWFormats[HWCodec_VC1]);
    regHW.WriteBOOL(L"mpeg2",m_settings.bHWFormats[HWCodec_MPEG2]);

    regHW.WriteDWORD(L"HWDeintMode", m_settings.HWDeintMode);
    regHW.WriteDWORD(L"HWDeintOutput", m_settings.HWDeintOutput);
    regHW.WriteBOOL(L"HWDeintHQ", m_settings.HWDeintHQ);
    regHW.WriteDWORD(L"HWDeintFieldOrder", m_settings.HWDeintFieldOrder);
    regHW.WriteBOOL(L"HWDeintForce", m_settings.HWDeintForce);
  }
  return S_OK;
}

// IUnknown
STDMETHODIMP CLAVVideo::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = NULL;

  return
    QI2(ISpecifyPropertyPages)
    QI2(ILAVVideoSettings)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

// ISpecifyPropertyPages
STDMETHODIMP CLAVVideo::GetPages(CAUUID *pPages)
{
  CheckPointer(pPages, E_POINTER);
  pPages->cElems = 2;
  pPages->pElems = (GUID *)CoTaskMemAlloc(sizeof(GUID) * pPages->cElems);
  if (pPages->pElems == NULL) {
    return E_OUTOFMEMORY;
  }
  pPages->pElems[0] = CLSID_LAVVideoSettingsProp;
  pPages->pElems[1] = CLSID_LAVVideoFormatsProp;
  return S_OK;
}

STDMETHODIMP_(LPWSTR) CLAVVideo::GetFileExtension()
{
  if (m_strExtension.empty()) {
    m_strExtension = L"";

    IFileSourceFilter *pSource = NULL;
    if (SUCCEEDED(FindIntefaceInGraph(m_pInput, IID_IFileSourceFilter, (void **)&pSource))) {
      LPOLESTR pwszFile = NULL;
      if (SUCCEEDED(pSource->GetCurFile(&pwszFile, NULL)) && pwszFile) {
        LPWSTR pwszExtension = PathFindExtensionW(pwszFile);
        m_strExtension = std::wstring(pwszExtension);
        CoTaskMemFree(pwszFile);
      }
      SafeRelease(&pSource);
    }
  }

  size_t len = m_strExtension.size() + 1;
  LPWSTR pszExtension = (LPWSTR)CoTaskMemAlloc(sizeof(WCHAR) * len);
  if (!pszExtension)
    return NULL;

  wcscpy_s(pszExtension, len, m_strExtension.c_str());
  return pszExtension;
}

// CTransformFilter
HRESULT CLAVVideo::CheckInputType(const CMediaType *mtIn)
{
  for(int i = 0; i < sudPinTypesInCount; i++) {
    if(*sudPinTypesIn[i].clsMajorType == mtIn->majortype
      && *sudPinTypesIn[i].clsMinorType == mtIn->subtype && (mtIn->formattype == FORMAT_VideoInfo || mtIn->formattype == FORMAT_VideoInfo2 || mtIn->formattype == FORMAT_MPEGVideo || mtIn->formattype == FORMAT_MPEG2Video)) {
        return S_OK;
    }
  }
  return VFW_E_TYPE_NOT_ACCEPTED;
}

// Check if the types are compatible
HRESULT CLAVVideo::CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut)
{
  DbgLog((LOG_TRACE, 10, L"::CheckTransform()"));
  if (SUCCEEDED(CheckInputType(mtIn)) && mtOut->majortype == MEDIATYPE_Video) {
    if (m_PixFmtConverter.IsAllowedSubtype(&mtOut->subtype)) {
      return S_OK;
    }
  }
  return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CLAVVideo::DecideBufferSize(IMemAllocator* pAllocator, ALLOCATOR_PROPERTIES* pProperties)
{
  DbgLog((LOG_TRACE, 10, L"::DecideBufferSize()"));
  if(m_pInput->IsConnected() == FALSE) {
    return E_UNEXPECTED;
  }

  BITMAPINFOHEADER *pBIH = NULL;
  CMediaType mtOut = m_pOutput->CurrentMediaType();
  videoFormatTypeHandler(mtOut.Format(), mtOut.FormatType(), &pBIH, NULL);

  pProperties->cBuffers = 4;
  pProperties->cbBuffer = pBIH->biSizeImage;
  pProperties->cbAlign  = 1;
  pProperties->cbPrefix = 0;

  HRESULT hr;
  ALLOCATOR_PROPERTIES Actual;
  if(FAILED(hr = pAllocator->SetProperties(pProperties, &Actual))) {
    return hr;
  }

  return pProperties->cBuffers > Actual.cBuffers || pProperties->cbBuffer > Actual.cbBuffer
    ? E_FAIL : S_OK;
}

HRESULT CLAVVideo::GetMediaType(int iPosition, CMediaType *pMediaType)
{
  DbgLog((LOG_TRACE, 10, L"::GetMediaType(): position: %d", iPosition));
  if(m_pInput->IsConnected() == FALSE) {
    return E_UNEXPECTED;
  }
  if(iPosition < 0) {
    return E_INVALIDARG;
  }

  if(iPosition >= (m_PixFmtConverter.GetNumMediaTypes() * 2)) {
    return VFW_S_NO_MORE_ITEMS;
  }

  int index = iPosition / 2;
  BOOL bVIH1 = iPosition % 2;

  CMediaType &mtIn = m_pInput->CurrentMediaType();

  BITMAPINFOHEADER *pBIH = NULL;
  REFERENCE_TIME rtAvgTime;
  DWORD dwAspectX = 0, dwAspectY = 0;
  videoFormatTypeHandler(mtIn.Format(), mtIn.FormatType(), &pBIH, &rtAvgTime, &dwAspectX, &dwAspectY);

  m_PixFmtConverter.GetMediaType(pMediaType, index, pBIH->biWidth, pBIH->biHeight, dwAspectX, dwAspectY, rtAvgTime, bVIH1, IsHWDeintActive());

  return S_OK;
}

HRESULT CLAVVideo::CreateDecoder(const CMediaType *pmt)
{
  DbgLog((LOG_TRACE, 10, L"::CreateDecoder(): Creating new decoder..."));
  HRESULT hr = S_OK;

  SAFE_DELETE(m_pDecoder);

  CodecID codec = FindCodecId(pmt);
  if (codec == CODEC_ID_NONE) {
    return VFW_E_TYPE_NOT_ACCEPTED;
  }

  // Check if codec is activated
  for(int i = 0; i < Codec_NB; ++i) {
    const codec_config_t *config = get_codec_config((LAVVideoCodec)i);
    bool bMatched = false;
    for (int k = 0; k < config->nCodecs; ++k) {
      if (config->codecs[k] == codec) {
        bMatched = true;
        break;
      }
    }
    if (bMatched && !m_settings.bFormats[i]) {
      DbgLog((LOG_TRACE, 10, L"-> Codec is disabled", codec));
      return VFW_E_TYPE_NOT_ACCEPTED;
    }
  }

  if (m_settings.HWAccel != HWAccel_None && !m_bHWDecoderFailed &&
    (  (codec == CODEC_ID_H264 && m_settings.bHWFormats[HWCodec_H264])
    || (codec == CODEC_ID_VC1 && m_settings.bHWFormats[HWCodec_VC1])
    || (codec == CODEC_ID_MPEG2VIDEO && m_settings.bHWFormats[HWCodec_MPEG2])))
  {
    if (m_settings.HWAccel == HWAccel_CUDA)
      m_pDecoder = CreateDecoderCUVID();
    m_bHWDecoder = TRUE;
  }

  // Fallback for software
  if (!m_pDecoder) {
    m_bHWDecoder = FALSE;
    m_pDecoder = CreateDecoderAVCodec();
  }

  hr = m_pDecoder->InitInterfaces(static_cast<ILAVVideoSettings *>(this), static_cast<ILAVVideoCallback *>(this));
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Init Interfaces failed (hr: 0x%x)", hr));
    goto done;
  }

  hr = m_pDecoder->InitDecoder(codec, pmt);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Init Decoder failed (hr: 0x%x)", hr));
    goto done;
  }

  LAVPixelFormat pix;
  int bpp;
  m_pDecoder->GetPixelFormat(&pix, &bpp);
  m_PixFmtConverter.SetInputFmt(pix, bpp);

done:
  if (FAILED(hr) && m_bHWDecoder) {
    DbgLog((LOG_TRACE, 10, L"-> Hardware decoder failed to initialize, re-trying with software..."));
    m_bHWDecoderFailed = TRUE;
    return CreateDecoder(pmt);
  }
  return SUCCEEDED(hr) ? S_OK : VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CLAVVideo::SetMediaType(PIN_DIRECTION dir, const CMediaType *pmt)
{
  HRESULT hr = S_OK;
  DbgLog((LOG_TRACE, 5, L"SetMediaType -- %S", dir == PINDIR_INPUT ? "in" : "out"));
  if (dir == PINDIR_INPUT) {
    hr = CreateDecoder(pmt);
    if (FAILED(hr)) {
      return hr;
    }
    m_bForceInputAR = TRUE;
  } else if (dir == PINDIR_OUTPUT) {
    m_PixFmtConverter.SetOutputPixFmt(m_PixFmtConverter.GetOutputBySubtype(pmt->Subtype()));
  }
  return __super::SetMediaType(dir, pmt);
}

HRESULT CLAVVideo::EndOfStream()
{
  DbgLog((LOG_TRACE, 1, L"EndOfStream"));
  CAutoLock cAutoLock(&m_csReceive);

  m_pDecoder->EndOfStream();

  return __super::EndOfStream();
}

HRESULT CLAVVideo::BeginFlush()
{
  DbgLog((LOG_TRACE, 1, L"::BeginFlush"));
  return __super::BeginFlush();
}

HRESULT CLAVVideo::EndFlush()
{
  DbgLog((LOG_TRACE, 1, L"::EndFlush"));
  CAutoLock cAutoLock(&m_csReceive);
  return __super::EndFlush();
}

HRESULT CLAVVideo::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
  DbgLog((LOG_TRACE, 1, L"::NewSegment - %d / %d", tStart, tStop));
  CAutoLock cAutoLock(&m_csReceive);

  m_pDecoder->Flush();
  m_rtPrevStart = m_rtPrevStop = 0;

  return __super::NewSegment(tStart, tStop, dRate);
}

HRESULT CLAVVideo::BreakConnect(PIN_DIRECTION dir)
{
  DbgLog((LOG_TRACE, 10, L"::BreakConnect"));
  if (dir == PINDIR_INPUT) {
    m_bHWDecoderFailed = FALSE;
    SAFE_DELETE(m_pDecoder);
  }
  return __super::BreakConnect(dir);
}

HRESULT CLAVVideo::GetDeliveryBuffer(IMediaSample** ppOut, int width, int height, AVRational ar, DXVA2_ExtendedFormat dxvaExtFlags, REFERENCE_TIME avgFrameDuration)
{
  CheckPointer(ppOut, E_POINTER);

  HRESULT hr;

  if(FAILED(hr = ReconnectOutput(width, height, ar, dxvaExtFlags, avgFrameDuration))) {
    return hr;
  }

  if(FAILED(hr = m_pOutput->GetDeliveryBuffer(ppOut, NULL, NULL, 0))) {
    return hr;
  }

  AM_MEDIA_TYPE* pmt = NULL;
  if(SUCCEEDED((*ppOut)->GetMediaType(&pmt)) && pmt) {
#ifdef DEBUG
    BITMAPINFOHEADER *pBMINew = NULL, *pBMIOld = NULL;
    videoFormatTypeHandler(pmt->pbFormat, &pmt->formattype, &pBMINew);
    CMediaType &outMt = m_pOutput->CurrentMediaType();
    videoFormatTypeHandler(outMt.pbFormat, &outMt.formattype, &pBMIOld);
    DbgLog((LOG_TRACE, 10, L"::GetDeliveryBuffer(): Sample contains new media type from downstream filter.."));
    DbgLog((LOG_TRACE, 10, L"-> Width changed from %d to %d", pBMIOld->biWidth, pBMINew->biWidth));
#endif

    CMediaType mt = *pmt;
    m_pOutput->SetMediaType(&mt);
    DeleteMediaType(pmt);
    pmt = NULL;
  }

  (*ppOut)->SetDiscontinuity(FALSE);
  (*ppOut)->SetSyncPoint(TRUE);

  return S_OK;
}

HRESULT CLAVVideo::ReconnectOutput(int width, int height, AVRational ar, DXVA2_ExtendedFormat dxvaExtFlags, REFERENCE_TIME avgFrameDuration)
{
  CMediaType& mt = m_pOutput->CurrentMediaType();


  HRESULT hr = S_FALSE;
  BOOL bNeedReconnect = FALSE;

  DWORD dwAspectX, dwAspectY;

  if ((dxvaExtFlags.value & ~0xff) != 0)
    dxvaExtFlags.SampleFormat = AMCONTROL_USED | AMCONTROL_COLORINFO_PRESENT;
  else
    dxvaExtFlags.value = 0;

  // Haali is incompatible with DXVA_ExtendedFormat
  if (m_bDXVAExtFormatSupport == -1)
    m_bDXVAExtFormatSupport = !(FilterInGraph(CLSID_DXR) || FilterInGraph(CLSID_OverlayMixer));

  if (mt.formattype  == FORMAT_VideoInfo) {
    VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)mt.Format();
    if (avgFrameDuration == AV_NOPTS_VALUE)
      avgFrameDuration = vih->AvgTimePerFrame;

    bNeedReconnect = (vih->rcTarget.right != width || vih->rcTarget.bottom != height || vih->AvgTimePerFrame != avgFrameDuration);
  } else if (mt.formattype  == FORMAT_VideoInfo2) {
    VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)mt.Format();
    if (avgFrameDuration == AV_NOPTS_VALUE)
      avgFrameDuration = vih2->AvgTimePerFrame;

    int num = ar.num, den = ar.den;
    if (!m_settings.StreamAR || num == 0 || den == 0) {
      if (m_bForceInputAR) {
        DWORD dwARX, dwARY;
        videoFormatTypeHandler(m_pInput->CurrentMediaType().Format(), m_pInput->CurrentMediaType().FormatType(), NULL, NULL, &dwARX, &dwARY);
        num = dwARX;
        den = dwARY;
      }
      if (!m_bForceInputAR || num == 0 || den == 0) {
        num = vih2->dwPictAspectRatioX;
        den = vih2->dwPictAspectRatioY;
      }
      m_bForceInputAR = FALSE;
    }
    dwAspectX = num;
    dwAspectY = den;

    bNeedReconnect = (vih2->rcTarget.right != width || vih2->rcTarget.bottom != height || vih2->dwPictAspectRatioX != num || vih2->dwPictAspectRatioY != den || vih2->AvgTimePerFrame != avgFrameDuration || (m_bDXVAExtFormatSupport && vih2->dwControlFlags != dxvaExtFlags.value));
  }



  if (bNeedReconnect) {
    DbgLog((LOG_TRACE, 10, L"::ReconnectOutput(): Performing reconnect"));
    BITMAPINFOHEADER *pBIH = NULL;
    if (mt.formattype == FORMAT_VideoInfo) {
      VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)mt.Format();

      DbgLog((LOG_TRACE, 10, L"Using VIH, Format dump:"));
      DbgLog((LOG_TRACE, 10, L"-> width: %d -> %d", vih->rcTarget.right, width));
      DbgLog((LOG_TRACE, 10, L"-> height: %d -> %d", vih->rcTarget.bottom, height));
      DbgLog((LOG_TRACE, 10, L"-> FPS: %I64d -> %I64d", vih->AvgTimePerFrame, avgFrameDuration));

      SetRect(&vih->rcSource, 0, 0, width, height);
      SetRect(&vih->rcTarget, 0, 0, width, height);

      vih->AvgTimePerFrame = avgFrameDuration;

      pBIH = &vih->bmiHeader;
    } else if (mt.formattype == FORMAT_VideoInfo2) {
      VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)mt.Format();

      DbgLog((LOG_TRACE, 10, L"Using VIH2, Format dump:"));
      DbgLog((LOG_TRACE, 10, L"-> width: %d -> %d", vih2->rcTarget.right, width));
      DbgLog((LOG_TRACE, 10, L"-> height: %d -> %d", vih2->rcTarget.bottom, height));
      DbgLog((LOG_TRACE, 10, L"-> AR: %d:%d -> %d:%d", vih2->dwPictAspectRatioX, vih2->dwPictAspectRatioY, dwAspectX, dwAspectY));
      DbgLog((LOG_TRACE, 10, L"-> FPS: %I64d -> %I64d", vih2->AvgTimePerFrame, avgFrameDuration));
      DbgLog((LOG_TRACE, 10, L"-> flags: %d -> %d", vih2->dwControlFlags, dxvaExtFlags.value));

      vih2->dwPictAspectRatioX = dwAspectX;
      vih2->dwPictAspectRatioY = dwAspectY;

      SetRect(&vih2->rcSource, 0, 0, width, height);
      SetRect(&vih2->rcTarget, 0, 0, width, height);

      vih2->AvgTimePerFrame = avgFrameDuration;
      if (m_bDXVAExtFormatSupport)
        vih2->dwControlFlags = dxvaExtFlags.value;

      pBIH = &vih2->bmiHeader;
    }

    DWORD oldSizeImage = pBIH->biSizeImage;

    pBIH->biWidth = width;
    pBIH->biHeight = height;
    pBIH->biSizeImage = width * height * pBIH->biBitCount >> 3;

    if (mt.subtype == MEDIASUBTYPE_RGB32 || mt.subtype == MEDIASUBTYPE_RGB24) {
       pBIH->biHeight = -pBIH->biHeight;
    }

    HRESULT hrQA = m_pOutput->GetConnected()->QueryAccept(&mt);
    if(SUCCEEDED(hr = m_pOutput->GetConnected()->ReceiveConnection(m_pOutput, &mt))) {
      IMediaSample *pOut = NULL;
      if (SUCCEEDED(hr = m_pOutput->GetDeliveryBuffer(&pOut, NULL, NULL, 0))) {
        AM_MEDIA_TYPE *pmt = NULL;
        if(SUCCEEDED(pOut->GetMediaType(&pmt)) && pmt) {
          CMediaType newmt = *pmt;
          m_pOutput->SetMediaType(&newmt);
#ifdef DEBUG
          videoFormatTypeHandler(newmt.Format(), newmt.FormatType(), &pBIH);
          DbgLog((LOG_TRACE, 10, L"-> New MediaType negotiated; actual width: %d - renderer requests: %ld", width, pBIH->biWidth));
#endif
          DeleteMediaType(pmt);
        } else { // No Stride Request? We're ok with that, too!
          long size = pOut->GetSize();
          pBIH->biWidth = size / abs(pBIH->biHeight) * 8 / pBIH->biBitCount;
          m_bSendMediaType = TRUE;
          DbgLog((LOG_TRACE, 10, L"-> We did not get a stride request, calculated stride: %d", pBIH->biWidth));
        }
        pOut->Release();
      }
    } else if (hrQA == S_OK && hr == VFW_E_ALREADY_CONNECTED && (pBIH->biSizeImage > oldSizeImage)) {
      DbgLog((LOG_TRACE, 10, L"-> Downstream filter refuses new format, but more space required, updating allocator manually..."));
      IMemInputPin *pMemPin = NULL;
      if (SUCCEEDED(hr = m_pOutput->GetConnected()->QueryInterface<IMemInputPin>(&pMemPin)) && pMemPin) {
        IMemAllocator *pMemAllocator = NULL;
        if (SUCCEEDED(hr = pMemPin->GetAllocator(&pMemAllocator)) && pMemAllocator) {
          ALLOCATOR_PROPERTIES props, actual;
          hr = pMemAllocator->GetProperties(&props);
          hr = pMemAllocator->Decommit();
          props.cbBuffer = pBIH->biSizeImage;
          hr = pMemAllocator->SetProperties(&props, &actual);
          hr = pMemAllocator->Commit();
          SafeRelease(&pMemAllocator);
        }
        SafeRelease(&pMemPin);
        m_pOutput->SetMediaType(&mt);
        m_bSendMediaType = TRUE;
      }
    } else {
      DbgLog((LOG_TRACE, 10, L"-> Receive Connection failed (hr: %x)", hr));
    }
    if (bNeedReconnect)
      NotifyEvent(EC_VIDEO_SIZE_CHANGED, MAKELPARAM(width, height), 0);

    hr = S_OK;
  }

  return hr;
}

HRESULT CLAVVideo::NegotiatePixelFormat(CMediaType &outMt, int width, int height)
{
  DbgLog((LOG_TRACE, 10, L"::NegotiatePixelFormat()"));

  HRESULT hr = S_OK;
  int i = 0;

  DWORD dwAspectX, dwAspectY;
  REFERENCE_TIME rtAvg;
  BOOL bVIH1 = (outMt.formattype == FORMAT_VideoInfo);
  videoFormatTypeHandler(outMt.Format(), outMt.FormatType(), NULL, &rtAvg, &dwAspectX, &dwAspectY);

  CMediaType mt;
  for (i = 0; i < m_PixFmtConverter.GetNumMediaTypes(); ++i) {
    m_PixFmtConverter.GetMediaType(&mt, i, width, height, dwAspectX, dwAspectY, rtAvg, bVIH1, IsHWDeintActive());
    //hr = m_pOutput->GetConnected()->QueryAccept(&mt);
    hr = m_pOutput->GetConnected()->ReceiveConnection(m_pOutput, &mt);
    if (hr == S_OK) {
      DbgLog((LOG_TRACE, 10, L"::NegotiatePixelFormat(): Filter accepted format with index %d", i));
      m_pOutput->SetMediaType(&mt);
      hr = S_OK;
      goto done;
    }
  }

  DbgLog((LOG_ERROR, 10, L"::NegotiatePixelFormat(): Unable to agree on a pixel format", i));
  hr = E_FAIL;

done:
  FreeMediaType(mt);
  return hr;
}

HRESULT CLAVVideo::Receive(IMediaSample *pIn)
{
  CAutoLock cAutoLock(&m_csReceive);
  HRESULT        hr = S_OK;
  BYTE           *pDataIn = NULL;
  int            nSize;
  REFERENCE_TIME rtStart = AV_NOPTS_VALUE;
  REFERENCE_TIME rtStop = AV_NOPTS_VALUE;

  AM_SAMPLE2_PROPERTIES const *pProps = m_pInput->SampleProps();
  if(pProps->dwStreamId != AM_STREAM_MEDIA) {
    return m_pOutput->Deliver(pIn);
  }

  AM_MEDIA_TYPE *pmt = NULL;
  if (SUCCEEDED(pIn->GetMediaType(&pmt)) && pmt) {
    DbgLog((LOG_TRACE, 10, L"::Receive(): Input sample contained media type, dynamic format change..."));
    CMediaType mt = *pmt;
    m_pInput->SetMediaType(&mt);
    DeleteMediaType(pmt);
  }

  if (FAILED(hr = pIn->GetPointer(&pDataIn))) {
    return hr;
  }

  nSize = pIn->GetActualDataLength();
  hr = pIn->GetTime(&rtStart, &rtStop);

  if (FAILED(hr)) {
    rtStart = rtStop = AV_NOPTS_VALUE;
  }

  if (hr == VFW_S_NO_STOP_TIME || rtStop-1 <= rtStart) {
    rtStop = AV_NOPTS_VALUE;
  }

  hr = m_pDecoder->Decode(pDataIn, nSize, rtStart, rtStop, pIn->IsSyncPoint() == S_OK, pIn->IsDiscontinuity() == S_OK);
  // If a hardware decoder indicates a hard failure, we switch back to software
  // This is used to indicate incompatible media
  if (FAILED(hr) && m_bHWDecoder) {
    DbgLog((LOG_TRACE, 10, L"::Receive(): Hardware decoder indicates failure, switching back to software"));
    m_bHWDecoderFailed = TRUE;

    CMediaType &mt = m_pInput->CurrentMediaType();
    hr = CreateDecoder(&mt);
    if (FAILED(hr)) {
      DbgLog((LOG_ERROR, 10, L"-> Creating software decoder failed, this is bad."));
      return E_FAIL;
    }

    DbgLog((LOG_TRACE, 10, L"-> Software decoder created, decoding frame again..."));
    hr = m_pDecoder->Decode(pDataIn, nSize, rtStart, rtStop, pIn->IsSyncPoint(), pIn->IsDiscontinuity());
  }

  return S_OK;
}

// ILAVVideoCallback
STDMETHODIMP CLAVVideo::AllocateFrame(LAVFrame **ppFrame)
{
  CheckPointer(ppFrame, E_POINTER);

  *ppFrame = (LAVFrame *)CoTaskMemAlloc(sizeof(LAVFrame));
  if (!*ppFrame) {
    return E_OUTOFMEMORY;
  }

  // Initialize with zero
  ZeroMemory(*ppFrame, sizeof(LAVFrame));

  // Set some defaults
  (*ppFrame)->bpp = 8;
  (*ppFrame)->rtStart = AV_NOPTS_VALUE;
  (*ppFrame)->rtStop  = AV_NOPTS_VALUE;

  return S_OK;
}

STDMETHODIMP CLAVVideo::ReleaseFrame(LAVFrame **ppFrame)
{
  CheckPointer(ppFrame, E_POINTER);

  // Allow *ppFrame to be NULL already
  if (*ppFrame) {
    if ((*ppFrame)->destruct) {
      (*ppFrame)->destruct(*ppFrame);
    }
    SAFE_CO_FREE(*ppFrame);
  }
  return S_OK;
}

STDMETHODIMP_(BOOL) CLAVVideo::VC1IsDTS()
{
  return !(FilterInGraph(CLSID_MPCHCMPEGSplitter) || FilterInGraph(CLSID_MPCHCMPEGSplitterSource) || FilterInGraph(CLSID_MPBDReader));
}

STDMETHODIMP CLAVVideo::Deliver(LAVFrame *pFrame)
{
  HRESULT hr = S_OK;

  if (pFrame->rtStart == AV_NOPTS_VALUE) {
    pFrame->rtStart = m_rtPrevStop;
    pFrame->rtStop = AV_NOPTS_VALUE;
  }

  if (pFrame->rtStop == AV_NOPTS_VALUE) {
    REFERENCE_TIME duration = 0;

    CMediaType &mt = m_pOutput->CurrentMediaType();
    videoFormatTypeHandler(mt.Format(), mt.FormatType(), NULL, &duration, NULL, NULL);

    REFERENCE_TIME decoderDuration = m_pDecoder->GetFrameDuration();
    if (pFrame->avgFrameDuration && pFrame->avgFrameDuration != AV_NOPTS_VALUE) {
      duration = pFrame->avgFrameDuration;
    } else if (!duration && decoderDuration) {
      duration = decoderDuration;
    } else if(!duration) {
      duration = 1;
    }

    pFrame->rtStop = pFrame->rtStart + (duration * (pFrame->repeat ? 3 : 2)  / 2);
  }

#if defined(DEBUG) && DEBUG_FRAME_TIMINGS
  DbgLog((LOG_TRACE, 10, L"Frame, rtStart: %I64d, diff: %I64d, key: %d, repeat: %d", pFrame->rtStart, pFrame->rtStart-m_rtPrevStart, pFrame->key_frame, pFrame->repeat));
#endif

  m_rtPrevStart = pFrame->rtStart;
  m_rtPrevStop  = pFrame->rtStop;

  if (pFrame->rtStart < 0) {
    ReleaseFrame(&pFrame);
    return S_OK;
  }

  // Collect width/height
  int width  = pFrame->width;
  int height = pFrame->height;

  if (width == 1920 && height == 1088) {
    height = 1080;
  }

  if (m_PixFmtConverter.SetInputFmt(pFrame->format, pFrame->bpp)) {
    DbgLog((LOG_TRACE, 10, L"::Decode(): Changed input pixel format to %d (%d bpp)", pFrame->format, pFrame->bpp));

    CMediaType& mt = m_pOutput->CurrentMediaType();

    if (m_PixFmtConverter.GetOutputBySubtype(mt.Subtype()) != m_PixFmtConverter.GetPreferredOutput()) {
      NegotiatePixelFormat(mt, width, height);
    }
  }
  m_PixFmtConverter.SetColorProps(pFrame->ext_format, m_settings.RGBRange);

  // Update flags for cases where the converter can change the nominal range
  if (m_PixFmtConverter.IsRGBConverterActive() && m_settings.RGBRange != 0) {
    pFrame->ext_format.NominalRange = m_settings.RGBRange == 1 ? DXVA2_NominalRange_16_235 : DXVA2_NominalRange_0_255;
  } else if (m_PixFmtConverter.GetOutputPixFmt() == LAVOutPixFmt_RGB32 || m_PixFmtConverter.GetOutputPixFmt() == LAVOutPixFmt_RGB24) {
    pFrame->ext_format.NominalRange = DXVA2_NominalRange_0_255;
  }

  IMediaSample *pSampleOut = NULL;
  BYTE         *pDataOut   = NULL;

  REFERENCE_TIME avgDuration = pFrame->avgFrameDuration;
  if (avgDuration == 0)
    avgDuration = AV_NOPTS_VALUE;

  if(FAILED(hr = GetDeliveryBuffer(&pSampleOut, width, height, pFrame->aspect_ratio, pFrame->ext_format, avgDuration)) || FAILED(hr = pSampleOut->GetPointer(&pDataOut))) {
    ReleaseFrame(&pFrame);
    return hr;
  }

  pSampleOut->SetTime(&pFrame->rtStart, &pFrame->rtStop);
  pSampleOut->SetMediaTime(NULL, NULL);

  CMediaType& mt = m_pOutput->CurrentMediaType();
  BITMAPINFOHEADER *pBIH = NULL;
  videoFormatTypeHandler(mt.Format(), mt.FormatType(), &pBIH);

  long required = (pBIH->biWidth * abs(pBIH->biHeight) * pBIH->biBitCount) >> 3;

  if (pSampleOut->GetSize() < required) {
    DbgLog((LOG_ERROR, 10, L"::Decode(): Buffer is too small! Actual: %d, Required: %d", pSampleOut->GetSize(), required));
    SafeRelease(&pSampleOut);
    ReleaseFrame(&pFrame);
    return E_FAIL;
  }


#if defined(DEBUG) && DEBUG_PIXELCONV_TIMINGS
  LARGE_INTEGER frequency, start, end;
  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&start);
#endif
  m_PixFmtConverter.Convert(pFrame, pDataOut, width, height, pBIH->biWidth);
#if defined(DEBUG) && DEBUG_PIXELCONV_TIMINGS
  QueryPerformanceCounter(&end);
  double diff = (end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;
  m_pixFmtTimingAvg.Sample(diff);

  DbgLog((LOG_TRACE, 10, L"Pixel Mapping took %2.3fms in avg", m_pixFmtTimingAvg.Average()));
#endif

  if (m_bSendMediaType) {
    AM_MEDIA_TYPE *sendmt = CreateMediaType(&mt);
    if (sendmt->formattype == FORMAT_VideoInfo) {
      VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)sendmt->pbFormat;
      SetRect(&vih->rcSource, 0, 0, 0, 0);
    } else if (sendmt->formattype == FORMAT_VideoInfo2) {
      VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)sendmt->pbFormat;
      SetRect(&vih2->rcSource, 0, 0, 0, 0);
    }
    pSampleOut->SetMediaType(sendmt);
    DeleteMediaType(sendmt);
    m_bSendMediaType = FALSE;
  }

  if (m_settings.InterlacedFlags && !IsHWDeintActive())
    SetInterlaceFlags (pSampleOut, pFrame);

  hr = m_pOutput->Deliver(pSampleOut);
  SafeRelease(&pSampleOut);
  ReleaseFrame(&pFrame);

  return hr;
}

HRESULT CLAVVideo::SetInterlaceFlags(IMediaSample* pMS, LAVFrame *pFrame)
{
  HRESULT hr = S_OK;
  IMediaSample2 *pMS2 = NULL;
  if (SUCCEEDED(hr = pMS->QueryInterface(&pMS2))) {
    AM_SAMPLE2_PROPERTIES props;
    if(SUCCEEDED(pMS2->GetProperties(sizeof(props), (BYTE*)&props))) {
      props.dwTypeSpecificFlags &= ~0x7f;

      if(!pFrame->ext_format.SampleFormat || pFrame->ext_format.SampleFormat == DXVA2_SampleProgressiveFrame) {
        props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_WEAVE;
      } else {
        if(pFrame->ext_format.SampleFormat == DXVA2_SampleFieldInterleavedEvenFirst) {
          props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_FIELD1FIRST;
        }
      }

      pMS2->SetProperties(sizeof(props), (BYTE*)&props);
    }
  }
  SafeRelease(&pMS2);
  return hr;
}

// H264 Random Access Helpers

// ILAVVideoSettings
STDMETHODIMP CLAVVideo::SetRuntimeConfig(BOOL bRuntimeConfig)
{
  m_bRuntimeConfig = bRuntimeConfig;
  LoadSettings();

  return S_OK;
}

STDMETHODIMP CLAVVideo::SetFormatConfiguration(LAVVideoCodec vCodec, BOOL bEnabled)
{
  if (vCodec < 0 || vCodec >= Codec_NB)
    return E_FAIL;

  m_settings.bFormats[vCodec] = bEnabled;

  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetFormatConfiguration(LAVVideoCodec vCodec)
{
  if (vCodec < 0 || vCodec >= Codec_NB)
    return FALSE;

  return m_settings.bFormats[vCodec];
}

STDMETHODIMP CLAVVideo::SetNumThreads(DWORD dwNum)
{
  m_settings.NumThreads = dwNum;
  return SaveSettings();
}

STDMETHODIMP_(DWORD) CLAVVideo::GetNumThreads()
{
  return m_settings.NumThreads;
}

STDMETHODIMP CLAVVideo::SetStreamAR(BOOL bStreamAR)
{
  m_settings.StreamAR = bStreamAR;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetStreamAR()
{
  return m_settings.StreamAR;
}

STDMETHODIMP CLAVVideo::SetReportInterlacedFlags(BOOL bEnabled)
{
  m_settings.InterlacedFlags = bEnabled;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetReportInterlacedFlags()
{
  return m_settings.InterlacedFlags;
}

STDMETHODIMP CLAVVideo::SetPixelFormat(LAVOutPixFmts pixFmt, BOOL bEnabled)
{
  if (pixFmt < 0 || pixFmt >= LAVOutPixFmt_NB)
    return E_FAIL;

  m_settings.bPixFmts[pixFmt] = bEnabled;

  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetPixelFormat(LAVOutPixFmts pixFmt)
{
  if (pixFmt < 0 || pixFmt >= LAVOutPixFmt_NB)
    return FALSE;

  return m_settings.bPixFmts[pixFmt];
}

STDMETHODIMP CLAVVideo::SetHighQualityPixelFormatConversion(BOOL bEnabled)
{
  m_settings.HighQualityPixConv = bEnabled;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetHighQualityPixelFormatConversion()
{
  return m_settings.HighQualityPixConv;
}

STDMETHODIMP CLAVVideo::SetRGBOutputRange(DWORD dwRange)
{
  m_settings.RGBRange = dwRange;
  return SaveSettings();
}

STDMETHODIMP_(DWORD) CLAVVideo::GetRGBOutputRange()
{
  return m_settings.RGBRange;
}

STDMETHODIMP_(DWORD) CLAVVideo::CheckHWAccelSupport(LAVHWAccel hwAccel)
{
  if (hwAccel == m_settings.HWAccel && m_bHWDecoder)
    return 2;

  HRESULT hr = E_FAIL;
  switch(hwAccel) {
  case HWAccel_CUDA:
    {
      ILAVDecoder *pDecoder = CreateDecoderCUVID();
      hr = pDecoder->InitInterfaces(this, this);
      SAFE_DELETE(pDecoder);
    }
    break;
  }

  return SUCCEEDED(hr) ? 1 : 0;
}

STDMETHODIMP CLAVVideo::SetHWAccel(LAVHWAccel hwAccel)
{
  m_settings.HWAccel = hwAccel;
  return SaveSettings();
}

STDMETHODIMP_(LAVHWAccel) CLAVVideo::GetHWAccel()
{
  return (LAVHWAccel)m_settings.HWAccel;
}

STDMETHODIMP CLAVVideo::SetHWAccelCodec(LAVVideoHWCodec hwAccelCodec, BOOL bEnabled)
{
  if (hwAccelCodec < 0 || hwAccelCodec >= HWCodec_NB)
    return E_FAIL;

  m_settings.bHWFormats[hwAccelCodec] = bEnabled;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetHWAccelCodec(LAVVideoHWCodec hwAccelCodec)
{
  if (hwAccelCodec < 0 || hwAccelCodec >= HWCodec_NB)
    return FALSE;

  return m_settings.bHWFormats[hwAccelCodec];
}

STDMETHODIMP CLAVVideo::SetHWAccelDeintMode(LAVHWDeintModes deintMode)
{
  m_settings.HWDeintMode = deintMode;
  return SaveSettings();
}

STDMETHODIMP_(LAVHWDeintModes) CLAVVideo::GetHWAccelDeintMode()
{
  return (LAVHWDeintModes)m_settings.HWDeintMode;
}

STDMETHODIMP CLAVVideo::SetHWAccelDeintOutput(LAVHWDeintOutput deintOutput)
{
  m_settings.HWDeintOutput = deintOutput;
  return SaveSettings();
}

STDMETHODIMP_(LAVHWDeintOutput) CLAVVideo::GetHWAccelDeintOutput()
{
  return (LAVHWDeintOutput)m_settings.HWDeintOutput;
}

STDMETHODIMP CLAVVideo::SetHWAccelDeintHQ(BOOL bHQ)
{
  m_settings.HWDeintHQ = bHQ;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetHWAccelDeintHQ()
{
  return m_settings.HWDeintHQ;
}

STDMETHODIMP CLAVVideo::SetHWAccelDeintFieldOrder(LAVHWDeintFieldOrder fieldOrder)
{
  m_settings.HWDeintFieldOrder = fieldOrder;
  return SaveSettings();
}

STDMETHODIMP_(LAVHWDeintFieldOrder) CLAVVideo::GetHWAccelDeintFieldOrder()
{
  return (LAVHWDeintFieldOrder)m_settings.HWDeintFieldOrder;
}

STDMETHODIMP CLAVVideo::SetHWAccelDeintForce(BOOL bForce)
{
  m_settings.HWDeintForce = bForce;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetHWAccelDeintForce()
{
  return m_settings.HWDeintForce;
}
