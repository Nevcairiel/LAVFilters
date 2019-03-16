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
#include "LAVVideo.h"
#include "VideoSettingsProp.h"
#include "Media.h"
#include <dvdmedia.h>

#include "VideoInputPin.h"
#include "VideoOutputPin.h"

#include "moreuuids.h"
#include "registry.h"
#include "resource.h"

#include "IMediaSample3D.h"
#include "IMediaSideDataFFmpeg.h"

#include <Shlwapi.h>

#include "parsers/VC1HeaderParser.h"
#include "parsers/MPEG2HeaderParser.h"

#include <Mfidl.h>
#include <evr.h>
#include <d3d9.h>

#pragma warning(disable: 4355)

CLAVVideo::CLAVVideo(LPUNKNOWN pUnk, HRESULT* phr)
  : CTransformFilter(NAME("LAV Video Decoder"), 0, __uuidof(CLAVVideo))
  , m_Decoder(this)
{
  *phr = S_OK;
  m_pInput = new CVideoInputPin(TEXT("CVideoInputPin"), this, phr, L"Input");
  ASSERT(SUCCEEDED(*phr));

  m_pOutput = new CVideoOutputPin(TEXT("CVideoOutputPin"), this, phr, L"Output");
  ASSERT(SUCCEEDED(*phr));

  memset(&m_LAVPinInfo, 0, sizeof(m_LAVPinInfo));
  memset(&m_FilterPrevFrame, 0, sizeof(m_FilterPrevFrame));
  memset(&m_SideData, 0, sizeof(m_SideData));

  LoadSettings();

  m_PixFmtConverter.SetSettings(this);

#ifdef DEBUG
  DbgSetModuleLevel (LOG_TRACE, DWORD_MAX);
  DbgSetModuleLevel (LOG_ERROR, DWORD_MAX);
  DbgSetModuleLevel (LOG_CUSTOM1, DWORD_MAX); // FFMPEG messages use custom1
#ifdef LAV_DEBUG_RELEASE
  DbgSetLogFileDesktop(LAVC_VIDEO_LOG_FILE);
#endif
#endif
}

CLAVVideo::~CLAVVideo()
{
  SAFE_DELETE(m_pTrayIcon);

  ReleaseLastSequenceFrame();
  m_Decoder.Close();

  if (m_pFilterGraph)
    avfilter_graph_free(&m_pFilterGraph);
  m_pFilterBufferSrc = nullptr;
  m_pFilterBufferSink = nullptr;

  if (m_SubtitleConsumer)
    m_SubtitleConsumer->DisconnectProvider();
  SafeRelease(&m_SubtitleConsumer);

  SAFE_DELETE(m_pSubtitleInput);
  SAFE_DELETE(m_pCCOutputPin);

#if defined(DEBUG) && defined(LAV_DEBUG_RELEASE)
  DbgCloseLogFile();
#endif
}

HRESULT CLAVVideo::CreateTrayIcon()
{
  CAutoLock cObjectLock(m_pLock);
  if (m_pTrayIcon)
    return E_UNEXPECTED;
  if (CBaseTrayIcon::ProcessBlackList())
    return S_FALSE;
  m_pTrayIcon = new CBaseTrayIcon(this, TEXT(LAV_VIDEO), IDI_ICON1);
  return S_OK;
}

STDMETHODIMP CLAVVideo::JoinFilterGraph(IFilterGraph * pGraph, LPCWSTR pName)
{
  CAutoLock cObjectLock(m_pLock);
  HRESULT hr = __super::JoinFilterGraph(pGraph, pName);
  if (pGraph && !m_pTrayIcon && m_settings.TrayIcon) {
    CreateTrayIcon();
  } else if (!pGraph && m_pTrayIcon) {
    SAFE_DELETE(m_pTrayIcon);
  }
  return hr;
}

HRESULT CLAVVideo::LoadDefaults()
{
  m_settings.TrayIcon = FALSE;

  // Set Defaults
  m_settings.StreamAR = 2;
  m_settings.NumThreads = 0;
  m_settings.DeintFieldOrder = DeintFieldOrder_Auto;
  m_settings.DeintMode = DeintMode_Auto;
  m_settings.RGBRange = 2; // Full range default

  for (int i = 0; i < Codec_VideoNB; ++i)
    m_settings.bFormats[i] = TRUE;

  m_settings.bFormats[Codec_RV12]     = FALSE;
  m_settings.bFormats[Codec_QPEG]     = FALSE;
  m_settings.bFormats[Codec_MSRLE]    = FALSE;

  m_settings.bDVDVideo  = TRUE;
  m_settings.bMSWMV9DMO = TRUE;

  // Raw formats, off by default
  m_settings.bFormats[Codec_v210]     = FALSE;

  for (int i = 0; i < LAVOutPixFmt_NB; ++i)
    m_settings.bPixFmts[i] = TRUE;

  m_settings.bPixFmts[LAVOutPixFmt_YV16] = FALSE;
  m_settings.bPixFmts[LAVOutPixFmt_AYUV] = FALSE;

  m_settings.HWAccel = HWAccel_None;
  for (int i = 0; i < HWCodec_NB; ++i)
    m_settings.bHWFormats[i] = TRUE;

  m_settings.bHWFormats[HWCodec_MPEG4] = FALSE;
  m_settings.bHWFormats[HWCodec_H264MVC] = FALSE;

  m_settings.HWAccelResFlags = LAVHWResFlag_SD|LAVHWResFlag_HD|LAVHWResFlag_UHD;

  m_settings.HWAccelCUVIDXVA = TRUE;

  m_settings.HWDeintMode = HWDeintMode_Weave;
  m_settings.HWDeintOutput = DeintOutput_FramePerField;

  m_settings.SWDeintMode = SWDeintMode_None;
  m_settings.SWDeintOutput = DeintOutput_FramePerField;

  m_settings.DitherMode = LAVDither_Random;

  m_settings.HWAccelDeviceDXVA2 = LAVHWACCEL_DEVICE_DEFAULT;
  m_settings.HWAccelDeviceDXVA2Desc = 0;

  m_settings.HWAccelDeviceD3D11 = LAVHWACCEL_DEVICE_DEFAULT;
  m_settings.HWAccelDeviceD3D11Desc = 0;

  m_settings.bH264MVCOverride = TRUE;
  m_settings.bCCOutputPinEnabled = FALSE;

  return S_OK;
}

static const WCHAR* pixFmtSettingsMap[LAVOutPixFmt_NB] = {
  L"yv12", L"nv12", L"yuy2", L"uyvy", L"ayuv", L"p010", L"p210", L"y410", L"p016", L"p216", L"y416", L"rgb32", L"rgb24", L"v210", L"v410", L"yv16", L"yv24", L"rgb48"
};

HRESULT CLAVVideo::LoadSettings()
{
  LoadDefaults();
  if (m_bRuntimeConfig)
    return S_FALSE;

  ReadSettings(HKEY_LOCAL_MACHINE);
  return ReadSettings(HKEY_CURRENT_USER);
}

HRESULT CLAVVideo::ReadSettings(HKEY rootKey)
{
  HRESULT hr;
  BOOL bFlag;
  DWORD dwVal;

  CRegistry reg = CRegistry(rootKey, LAVC_VIDEO_REGISTRY_KEY, hr, TRUE);
  if (SUCCEEDED(hr)) {
    bFlag = reg.ReadBOOL(L"TrayIcon", hr);
    if (SUCCEEDED(hr)) m_settings.TrayIcon = bFlag;

    dwVal = reg.ReadDWORD(L"StreamAR", hr);
    if (SUCCEEDED(hr)) m_settings.StreamAR = dwVal;

    dwVal = reg.ReadDWORD(L"NumThreads", hr);
    if (SUCCEEDED(hr)) m_settings.NumThreads = dwVal;

    dwVal = reg.ReadDWORD(L"DeintFieldOrder", hr);
    if (SUCCEEDED(hr)) m_settings.DeintFieldOrder = dwVal;

    // Load deprecated deint flags and convert them
    bFlag = reg.ReadBOOL(L"DeintAggressive", hr);
    if (SUCCEEDED(hr) && bFlag) m_settings.DeintMode = DeintMode_Aggressive;

    bFlag = reg.ReadBOOL(L"DeintForce", hr);
    if (SUCCEEDED(hr) && bFlag) m_settings.DeintMode = DeintMode_Force;

    bFlag = reg.ReadBOOL(L"DeintTreatAsProgressive", hr);
    if (SUCCEEDED(hr) && bFlag) m_settings.DeintMode = DeintMode_Disable;

    dwVal = reg.ReadDWORD(L"DeintMode", hr);
    if (SUCCEEDED(hr)) m_settings.DeintMode = (LAVDeintMode)dwVal;

    dwVal = reg.ReadDWORD(L"RGBRange", hr);
    if (SUCCEEDED(hr)) m_settings.RGBRange = dwVal;

    dwVal = reg.ReadDWORD(L"SWDeintMode", hr);
    if (SUCCEEDED(hr)) m_settings.SWDeintMode = dwVal;

    dwVal = reg.ReadDWORD(L"SWDeintOutput", hr);
    if (SUCCEEDED(hr)) m_settings.SWDeintOutput = dwVal;

    dwVal = reg.ReadDWORD(L"DitherMode", hr);
    if (SUCCEEDED(hr)) m_settings.DitherMode = dwVal;

    bFlag = reg.ReadBOOL(L"DVDVideo", hr);
    if (SUCCEEDED(hr)) m_settings.bDVDVideo = bFlag;

    bFlag = reg.ReadBOOL(L"MSWMV9DMO", hr);
    if (SUCCEEDED(hr)) m_settings.bMSWMV9DMO = bFlag;
  }

  CRegistry regF = CRegistry(rootKey, LAVC_VIDEO_REGISTRY_KEY_FORMATS, hr, TRUE);
  if (SUCCEEDED(hr)) {
    for (int i = 0; i < Codec_VideoNB; ++i) {
      const codec_config_t *info = get_codec_config((LAVVideoCodec)i);
      ATL::CA2W name(info->name);
      bFlag = regF.ReadBOOL(name, hr);
      if (SUCCEEDED(hr)) m_settings.bFormats[i] = bFlag;
    }
  }

  CRegistry regP = CRegistry(rootKey, LAVC_VIDEO_REGISTRY_KEY_OUTPUT, hr, TRUE);
  if (SUCCEEDED(hr)) {
    for (int i = 0; i < LAVOutPixFmt_NB; ++i) {
      bFlag = regP.ReadBOOL(pixFmtSettingsMap[i], hr);
      if (SUCCEEDED(hr)) m_settings.bPixFmts[i] = bFlag;
    }
    // Force disable, for future use
    m_settings.bPixFmts[LAVOutPixFmt_YV16] = FALSE;
  }

  CRegistry regHW = CRegistry(rootKey, LAVC_VIDEO_REGISTRY_KEY_HWACCEL, hr, TRUE);
  if (SUCCEEDED(hr)) {
    dwVal = regHW.ReadDWORD(L"HWAccel", hr);
    if (SUCCEEDED(hr)) m_settings.HWAccel = dwVal;

    bFlag = regHW.ReadBOOL(L"h264", hr);
    if (SUCCEEDED(hr)) m_settings.bHWFormats[HWCodec_H264] = bFlag;

    bFlag = regHW.ReadBOOL(L"vc1", hr);
    if (SUCCEEDED(hr)) m_settings.bHWFormats[HWCodec_VC1] = bFlag;

    bFlag = regHW.ReadBOOL(L"mpeg2", hr);
    if (SUCCEEDED(hr)) m_settings.bHWFormats[HWCodec_MPEG2] = bFlag;

    bFlag = regHW.ReadBOOL(L"mpeg4", hr);
    if (SUCCEEDED(hr)) m_settings.bHWFormats[HWCodec_MPEG4] = bFlag;

    bFlag = regHW.ReadBOOL(L"dvd", hr);
    if (SUCCEEDED(hr)) m_settings.bHWFormats[HWCodec_MPEG2DVD] = bFlag;

    bFlag = regHW.ReadBOOL(L"hevc", hr);
    if (SUCCEEDED(hr)) m_settings.bHWFormats[HWCodec_HEVC] = bFlag;

    bFlag = regHW.ReadBOOL(L"vp9", hr);
    if (SUCCEEDED(hr)) m_settings.bHWFormats[HWCodec_VP9] = bFlag;

    bFlag = regHW.ReadBOOL(L"h264mvc", hr);
    if (SUCCEEDED(hr)) m_settings.bHWFormats[HWCodec_H264MVC] = bFlag;

    dwVal = regHW.ReadDWORD(L"HWResFlags", hr);
    if (SUCCEEDED(hr)) m_settings.HWAccelResFlags = dwVal;

    dwVal = regHW.ReadDWORD(L"HWDeintMode", hr);
    if (SUCCEEDED(hr)) m_settings.HWDeintMode = dwVal;

    dwVal = regHW.ReadDWORD(L"HWDeintOutput", hr);
    if (SUCCEEDED(hr)) m_settings.HWDeintOutput = dwVal;

    dwVal = regHW.ReadDWORD(L"HWAccelDeviceDXVA2", hr);
    if (SUCCEEDED(hr)) m_settings.HWAccelDeviceDXVA2 = dwVal;

    dwVal = regHW.ReadDWORD(L"HWAccelDeviceDXVA2Desc", hr);
    if (SUCCEEDED(hr)) m_settings.HWAccelDeviceDXVA2Desc = dwVal;

    dwVal = regHW.ReadDWORD(L"HWAccelDeviceD3D11", hr);
    if (SUCCEEDED(hr)) m_settings.HWAccelDeviceD3D11 = dwVal;

    dwVal = regHW.ReadDWORD(L"HWAccelDeviceD3D11Desc", hr);
    if (SUCCEEDED(hr)) m_settings.HWAccelDeviceD3D11Desc = dwVal;

    bFlag = regHW.ReadBOOL(L"HWAccelCUVIDXVA", hr);
    if (SUCCEEDED(hr)) m_settings.HWAccelCUVIDXVA = bFlag;
  }

  return S_OK;
}

HRESULT CLAVVideo::SaveSettings()
{
  if (m_bRuntimeConfig)
    return S_FALSE;

  HRESULT hr;
  CreateRegistryKey(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY);
  CRegistry reg = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY, hr);
  if (SUCCEEDED(hr)) {
    reg.WriteBOOL(L"TrayIcon", m_settings.TrayIcon);
    reg.WriteDWORD(L"StreamAR", m_settings.StreamAR);
    reg.WriteDWORD(L"NumThreads", m_settings.NumThreads);
    reg.WriteDWORD(L"DeintFieldOrder", m_settings.DeintFieldOrder);
    reg.WriteDWORD(L"DeintMode", m_settings.DeintMode);
    reg.WriteDWORD(L"RGBRange", m_settings.RGBRange);

    CreateRegistryKey(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_FORMATS);
    CRegistry regF = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_FORMATS, hr);
    for (int i = 0; i < Codec_VideoNB; ++i) {
      const codec_config_t *info = get_codec_config((LAVVideoCodec)i);
      ATL::CA2W name(info->name);
      regF.WriteBOOL(name, m_settings.bFormats[i]);
    }

    reg.WriteBOOL(L"DVDVideo", m_settings.bDVDVideo);
    reg.WriteBOOL(L"MSWMV9DMO", m_settings.bMSWMV9DMO);

    CreateRegistryKey(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_OUTPUT);
    CRegistry regP = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_OUTPUT, hr);
    for (int i = 0; i < LAVOutPixFmt_NB; ++i) {
      regP.WriteBOOL(pixFmtSettingsMap[i], m_settings.bPixFmts[i]);
    }

    CreateRegistryKey(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_HWACCEL);
    CRegistry regHW = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_HWACCEL, hr);
    regHW.WriteDWORD(L"HWAccel", m_settings.HWAccel);
    regHW.WriteBOOL(L"h264", m_settings.bHWFormats[HWCodec_H264]);
    regHW.WriteBOOL(L"vc1", m_settings.bHWFormats[HWCodec_VC1]);
    regHW.WriteBOOL(L"mpeg2",m_settings.bHWFormats[HWCodec_MPEG2]);
    regHW.WriteBOOL(L"mpeg4",m_settings.bHWFormats[HWCodec_MPEG4]);
    regHW.WriteBOOL(L"dvd",m_settings.bHWFormats[HWCodec_MPEG2DVD]);
    regHW.WriteBOOL(L"hevc",m_settings.bHWFormats[HWCodec_HEVC]);
    regHW.WriteBOOL(L"vp9", m_settings.bHWFormats[HWCodec_VP9]);
    regHW.WriteBOOL(L"h264mvc", m_settings.bHWFormats[HWCodec_H264MVC]);

    regHW.WriteDWORD(L"HWResFlags", m_settings.HWAccelResFlags);

    regHW.WriteDWORD(L"HWDeintMode", m_settings.HWDeintMode);
    regHW.WriteDWORD(L"HWDeintOutput", m_settings.HWDeintOutput);

    regHW.WriteDWORD(L"HWAccelDeviceDXVA2", m_settings.HWAccelDeviceDXVA2);
    regHW.WriteDWORD(L"HWAccelDeviceDXVA2Desc", m_settings.HWAccelDeviceDXVA2Desc);

    regHW.WriteDWORD(L"HWAccelDeviceD3D11", m_settings.HWAccelDeviceD3D11);
    regHW.WriteDWORD(L"HWAccelDeviceD3D11Desc", m_settings.HWAccelDeviceD3D11Desc);

    regHW.WriteBOOL(L"HWAccelCUVIDXVA", m_settings.HWAccelCUVIDXVA);

    reg.WriteDWORD(L"SWDeintMode", m_settings.SWDeintMode);
    reg.WriteDWORD(L"SWDeintOutput", m_settings.SWDeintOutput);
    reg.WriteDWORD(L"DitherMode", m_settings.DitherMode);

    reg.DeleteKey(L"DeintAggressive");
    reg.DeleteKey(L"DeintForce");
    reg.DeleteKey(L"DeintTreatAsProgressive");
  }
  return S_OK;
}

// IUnknown
STDMETHODIMP CLAVVideo::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = nullptr;

  return
    QI(ISpecifyPropertyPages)
    QI(ISpecifyPropertyPages2)
    QI(IPropertyBag)
    QI2(ILAVVideoSettings)
    QI2(ILAVVideoStatus)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

// ISpecifyPropertyPages2
STDMETHODIMP CLAVVideo::GetPages(CAUUID *pPages)
{
  CheckPointer(pPages, E_POINTER);
  pPages->cElems = 2;
  pPages->pElems = (GUID *)CoTaskMemAlloc(sizeof(GUID) * pPages->cElems);
  if (pPages->pElems == nullptr) {
    return E_OUTOFMEMORY;
  }
  pPages->pElems[0] = CLSID_LAVVideoSettingsProp;
  pPages->pElems[1] = CLSID_LAVVideoFormatsProp;
  return S_OK;
}

STDMETHODIMP CLAVVideo::CreatePage(const GUID& guid, IPropertyPage** ppPage)
{
  CheckPointer(ppPage, E_POINTER);
  HRESULT hr = S_OK;

  if (*ppPage != nullptr)
    return E_INVALIDARG;

  if (guid == CLSID_LAVVideoSettingsProp)
    *ppPage = new CLAVVideoSettingsProp(nullptr, &hr);
  else if (guid == CLSID_LAVVideoFormatsProp)
    *ppPage = new CLAVVideoFormatsProp(nullptr, &hr);

  if (SUCCEEDED(hr) && *ppPage) {
    (*ppPage)->AddRef();
    return S_OK;
  } else {
    SAFE_DELETE(*ppPage);
    return E_FAIL;
  }
}

STDMETHODIMP_(LPWSTR) CLAVVideo::GetFileExtension()
{
  if (m_strExtension.empty()) {
    m_strExtension = L"";

    IFileSourceFilter *pSource = nullptr;
    if (SUCCEEDED(FindIntefaceInGraph(m_pInput, IID_IFileSourceFilter, (void **)&pSource))) {
      LPOLESTR pwszFile = nullptr;
      if (SUCCEEDED(pSource->GetCurFile(&pwszFile, nullptr)) && pwszFile) {
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
    return nullptr;

  wcscpy_s(pszExtension, len, m_strExtension.c_str());
  return pszExtension;
}

// CTransformFilter

int CLAVVideo::GetPinCount()
{
  int nCount = 2;

  if (m_pSubtitleInput)
    nCount++;

  if (m_pCCOutputPin)
    nCount++;

  return nCount;
}

CBasePin* CLAVVideo::GetPin(int n)
{
  if (n >= 2)
  {
    if (m_pSubtitleInput && n == 2)
      return m_pSubtitleInput;
    else if ((m_pSubtitleInput && n == 3) || (!m_pSubtitleInput && n == 2))
      return m_pCCOutputPin;
  }
  return __super::GetPin(n);
}

HRESULT CLAVVideo::CheckInputType(const CMediaType *mtIn)
{
  for(UINT i = 0; i < sudPinTypesInCount; i++) {
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

  BITMAPINFOHEADER *pBIH = nullptr;
  CMediaType &mtOut = m_pOutput->CurrentMediaType();
  videoFormatTypeHandler(mtOut, &pBIH);

  // try to honor the requested number of downstream buffers, but cap at the decoders maximum
  long decoderBuffersMax = LONG_MAX;
  long decoderBuffs = m_Decoder.GetBufferCount(&decoderBuffersMax);
  long downstreamBuffers = pProperties->cBuffers;
  pProperties->cBuffers = min(max(pProperties->cBuffers, 2) + decoderBuffs, decoderBuffersMax);
  pProperties->cbBuffer = pBIH ? pBIH->biSizeImage : 3110400;
  pProperties->cbAlign  = 1;
  pProperties->cbPrefix = 0;

  DbgLog((LOG_TRACE, 10, L" -> Downstream wants %d buffers, decoder wants %d, for a total of: %d", downstreamBuffers, decoderBuffs, pProperties->cBuffers));

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

  BITMAPINFOHEADER *pBIH = nullptr;
  REFERENCE_TIME rtAvgTime = 0;
  DWORD dwAspectX = 0, dwAspectY = 0;
  videoFormatTypeHandler(mtIn.Format(), mtIn.FormatType(), &pBIH, &rtAvgTime, &dwAspectX, &dwAspectY);

  // Adjust for deinterlacing
  if (m_Decoder.IsInterlaced(FALSE) && m_settings.DeintMode != DeintMode_Disable) {
    BOOL bFramePerField = (m_settings.SWDeintMode == SWDeintMode_YADIF && m_settings.SWDeintOutput == DeintOutput_FramePerField)
                        || m_settings.SWDeintMode == SWDeintMode_W3FDIF_Simple || m_settings.SWDeintMode == SWDeintMode_W3FDIF_Complex;
    if (bFramePerField)
      rtAvgTime /= 2;
  }

  m_PixFmtConverter.GetMediaType(pMediaType, index, pBIH->biWidth, pBIH->biHeight, dwAspectX, dwAspectY, rtAvgTime, IsInterlacedOutput(), bVIH1);

  return S_OK;
}

BOOL CLAVVideo::IsInterlacedOutput()
{
  return (m_settings.SWDeintMode == SWDeintMode_None || m_filterPixFmt == LAVPixFmt_None) && m_Decoder.IsInterlaced(TRUE) && !(m_settings.DeintMode == DeintMode_Disable);
}

static const LPWSTR stream_ar_blacklist[] = {
  L".mkv", L".webm",
  L".mp4", L".mov", L".m4v",
};

HRESULT CLAVVideo::CreateDecoder(const CMediaType *pmt)
{
  DbgLog((LOG_TRACE, 10, L"::CreateDecoder(): Creating new decoder..."));
  HRESULT hr = S_OK;

  AVCodecID codec = FindCodecId(pmt);
  if (codec == AV_CODEC_ID_NONE) {
    return VFW_E_TYPE_NOT_ACCEPTED;
  }

  // Check if codec is activated
  for(int i = 0; i < Codec_VideoNB; ++i) {
    const codec_config_t *config = get_codec_config((LAVVideoCodec)i);
    bool bMatched = false;
    for (int k = 0; k < config->nCodecs; ++k) {
      if (config->codecs[k] == codec) {
        bMatched = true;
        break;
      }
    }
    if (bMatched && !m_settings.bFormats[i]) {
      DbgLog((LOG_TRACE, 10, L"-> Codec is disabled"));
      return VFW_E_TYPE_NOT_ACCEPTED;
    }
  }

  if (codec == AV_CODEC_ID_H264_MVC && !m_settings.bH264MVCOverride) {
    DbgLog((LOG_TRACE, 10, L"-> H.264 MVC override, refusing"));
    return VFW_E_TYPE_NOT_ACCEPTED;
  }

  ILAVPinInfo *pPinInfo = nullptr;
  hr = FindPinIntefaceInGraph(m_pInput, IID_ILAVPinInfo, (void **)&pPinInfo);
  if (SUCCEEDED(hr)) {
    memset(&m_LAVPinInfo, 0, sizeof(m_LAVPinInfo));

    m_LAVPinInfoValid = TRUE;
    m_LAVPinInfo.flags = pPinInfo->GetStreamFlags();
    m_LAVPinInfo.pix_fmt = (AVPixelFormat)pPinInfo->GetPixelFormat();
    m_LAVPinInfo.has_b_frames = pPinInfo->GetHasBFrames();

    SafeRelease(&pPinInfo);
  } else {
    m_LAVPinInfoValid = FALSE;
  }

  // Clear old sidedata
  memset(&m_SideData, 0, sizeof(m_SideData));

  // Read and store stream-level sidedata
  IMediaSideData *pPinSideData = nullptr;
  hr = FindPinIntefaceInGraph(m_pInput, __uuidof(IMediaSideData), (void **)&pPinSideData);
  if (SUCCEEDED(hr)) {
    MediaSideDataFFMpeg *pSideData = nullptr;
    size_t size = 0;
    hr = pPinSideData->GetSideData(IID_MediaSideDataFFMpeg, (const BYTE **)&pSideData, &size);
    if (SUCCEEDED(hr) && size == sizeof(MediaSideDataFFMpeg)) {
      for (int i = 0; i < pSideData->side_data_elems; i++) {
        AVPacketSideData *sd = &pSideData->side_data[i];

        // Display Mastering metadata, including color info
        if (sd->type == AV_PKT_DATA_MASTERING_DISPLAY_METADATA && sd->size == sizeof(AVMasteringDisplayMetadata))
        {
          m_SideData.Mastering = *(AVMasteringDisplayMetadata *)sd->data;
        }
        else if (sd->type == AV_PKT_DATA_CONTENT_LIGHT_LEVEL && sd->size == sizeof(AVContentLightMetadata))
        {
          m_SideData.ContentLight = *(AVContentLightMetadata *)sd->data;
        }
      }
    }

    SafeRelease(&pPinSideData);
  }

  m_dwDecodeFlags = 0;

  LPWSTR pszExtension = GetFileExtension();
  if (pszExtension) {
    DbgLog((LOG_TRACE, 10, L"-> File extension: %s", pszExtension));

    for (int i = 0; i < countof(stream_ar_blacklist); i++) {
      if (_wcsicmp(stream_ar_blacklist[i], pszExtension) == 0) {
        m_dwDecodeFlags |= LAV_VIDEO_DEC_FLAG_STREAMAR_BLACKLIST;
        break;
      }
    }
    if (m_dwDecodeFlags & LAV_VIDEO_DEC_FLAG_STREAMAR_BLACKLIST) {
      // MPC-HC MP4 Splitter fails at Container AR
      if (FilterInGraph(PINDIR_INPUT, CLSID_MPCHCMP4Splitter) || FilterInGraph(PINDIR_INPUT, CLSID_MPCHCMP4SplitterSource)) {
        m_dwDecodeFlags &= ~LAV_VIDEO_DEC_FLAG_STREAMAR_BLACKLIST;
      }
    }
  }

  BOOL bSageVC1Hack = (codec == AV_CODEC_ID_VC1) && FilterInGraph(PINDIR_INPUT, CLSID_SageTVMpegDeMux);

  // Certain filters send VC-1 in PTS instead of DTS, so list them here
  // Usually, these are MPEG systems.
  BOOL bVC1DTS =  (codec == AV_CODEC_ID_VC1) &&
                 !(FilterInGraph(PINDIR_INPUT, CLSID_MPCHCMPEGSplitter)
                || FilterInGraph(PINDIR_INPUT, CLSID_MPCHCMPEGSplitterSource)
                || FilterInGraph(PINDIR_INPUT, CLSID_MPBDReader)
                || bSageVC1Hack);

  BOOL bH2645IsAVI  = ((codec == AV_CODEC_ID_H264 || codec == AV_CODEC_ID_HEVC) && (!m_LAVPinInfoValid && pszExtension && _wcsicmp(pszExtension, L".avi") == 0));
  BOOL bLAVSplitter = FilterInGraph(PINDIR_INPUT, CLSID_LAVSplitterSource) || FilterInGraph(PINDIR_INPUT, CLSID_LAVSplitter);
  BOOL bDVDPlayback = (pmt->majortype == MEDIATYPE_DVD_ENCRYPTED_PACK || pmt->majortype == MEDIATYPE_MPEG2_PACK || pmt->majortype == MEDIATYPE_MPEG2_PES);
  BOOL bTheoraMPCHCOgg = (codec == AV_CODEC_ID_THEORA || codec == AV_CODEC_ID_VP3) && (FilterInGraph(PINDIR_INPUT, CLSID_MPCHCOggSplitter) || FilterInGraph(PINDIR_INPUT, CLSID_MPCHCOggSource));

  if (bDVDPlayback && !m_settings.bDVDVideo) {
    DbgLog((LOG_TRACE, 10, L"-> DVD Video support is disabled"));
    return VFW_E_TYPE_NOT_ACCEPTED;
  }

  if (m_LAVPinInfoValid && (m_LAVPinInfo.flags & LAV_STREAM_FLAG_ONLY_DTS))
    m_dwDecodeFlags |= LAV_VIDEO_DEC_FLAG_ONLY_DTS;
  if (bVC1DTS)
    m_dwDecodeFlags |= LAV_VIDEO_DEC_FLAG_ONLY_DTS;
  if (bH2645IsAVI)
    m_dwDecodeFlags |= LAV_VIDEO_DEC_FLAG_ONLY_DTS;
  if (bLAVSplitter)
    m_dwDecodeFlags |= LAV_VIDEO_DEC_FLAG_LAVSPLITTER;
  if (bDVDPlayback)
    m_dwDecodeFlags |= LAV_VIDEO_DEC_FLAG_DVD;
  if (bTheoraMPCHCOgg)
    m_dwDecodeFlags |= LAV_VIDEO_DEC_FLAG_NO_MT;
  if (bSageVC1Hack)
    m_dwDecodeFlags |= LAV_VIDEO_DEC_FLAG_SAGE_HACK;
  if (m_LAVPinInfoValid && (m_LAVPinInfo.flags & LAV_STREAM_FLAG_LIVE))
    m_dwDecodeFlags |= LAV_VIDEO_DEC_FLAG_LIVE;

  SAFE_CO_FREE(pszExtension);

  hr = m_Decoder.CreateDecoder(pmt, codec);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Decoder creation failed"));
    goto done;
  }

  // Get avg time per frame
  videoFormatTypeHandler(pmt->Format(), pmt->FormatType(), nullptr, &m_rtAvgTimePerFrame);

  LAVPixelFormat pix;
  int bpp;
  m_Decoder.GetPixelFormat(&pix, &bpp);

  // Set input on the converter, and force negotiation if needed
  if (m_PixFmtConverter.SetInputFmt(pix, bpp) && m_pOutput->IsConnected())
    m_bForceFormatNegotiation = TRUE;

  if (pix == LAVPixFmt_YUV420 || pix == LAVPixFmt_YUV422 || pix == LAVPixFmt_NV12)
    m_filterPixFmt = pix;

  if (m_settings.bCCOutputPinEnabled && !bDVDPlayback && (codec == AV_CODEC_ID_MPEG2VIDEO || codec == AV_CODEC_ID_H264))
  {
    if (m_pCCOutputPin == nullptr && CBaseFilter::IsStopped())
      m_pCCOutputPin = new CCCOutputPin(TEXT("CCCOutputPin"), this, &m_csFilter, &hr, L"~CC Output");
  }

done:
  return SUCCEEDED(hr) ? S_OK : VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CLAVVideo::CheckDirectMode()
{
  LAVPixelFormat pix;
  int bpp;
  m_Decoder.GetPixelFormat(&pix, &bpp);

  BOOL bDirect = (pix == LAVPixFmt_NV12 || pix == LAVPixFmt_P016);
  if (pix == LAVPixFmt_NV12 && m_Decoder.IsInterlaced(FALSE) && m_settings.SWDeintMode != SWDeintMode_None)
    bDirect = FALSE;
  else if (pix == LAVPixFmt_NV12 && m_pOutput->CurrentMediaType().subtype != MEDIASUBTYPE_NV12 && m_pOutput->CurrentMediaType().subtype != MEDIASUBTYPE_YV12)
    bDirect = FALSE;
  else if (pix == LAVPixFmt_P016 && m_pOutput->CurrentMediaType().subtype != MEDIASUBTYPE_P010 && m_pOutput->CurrentMediaType().subtype != MEDIASUBTYPE_P016 && m_pOutput->CurrentMediaType().subtype != MEDIASUBTYPE_NV12)
    bDirect = FALSE;
  else if (m_SubtitleConsumer && m_SubtitleConsumer->HasProvider())
    bDirect = FALSE;

  m_Decoder.SetDirectOutput(bDirect);

  return S_OK;
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
  DbgLog((LOG_TRACE, 1, L"EndOfStream, flushing decoder"));
  CAutoLock cAutoLock(&m_csReceive);

  m_Decoder.EndOfStream();
  Filter(GetFlushFrame());

  if (m_pCCOutputPin)
    m_pCCOutputPin->DeliverEndOfStream();

  DbgLog((LOG_TRACE, 1, L"EndOfStream finished, decoder flushed"));
  return __super::EndOfStream();
}

HRESULT CLAVVideo::EndOfSegment()
{
  DbgLog((LOG_TRACE, 1, L"EndOfSegment, flushing decoder"));
  CAutoLock cAutoLock(&m_csReceive);

  m_Decoder.EndOfStream();
  Filter(GetFlushFrame());

  // Forward the EndOfSegment call downstream
  if (m_pOutput != NULL && m_pOutput->IsConnected())
  {
    IPin *pConnected = m_pOutput->GetConnected();

    IPinSegmentEx *pPinSegmentEx = NULL;
    if (pConnected->QueryInterface(&pPinSegmentEx) == S_OK)
    {
      pPinSegmentEx->EndOfSegment();
    }
    SafeRelease(&pPinSegmentEx);
  }

  return S_OK;
}

HRESULT CLAVVideo::BeginFlush()
{
  DbgLog((LOG_TRACE, 1, L"::BeginFlush"));
  m_bFlushing = TRUE;

  if (m_pCCOutputPin)
    m_pCCOutputPin->DeliverBeginFlush();

  return __super::BeginFlush();
}

HRESULT CLAVVideo::EndFlush()
{
  DbgLog((LOG_TRACE, 1, L"::EndFlush"));
  CAutoLock cAutoLock(&m_csReceive);

  ReleaseLastSequenceFrame();

  if (m_dwDecodeFlags & LAV_VIDEO_DEC_FLAG_DVD) {
    PerformFlush();
  }

  HRESULT hr = __super::EndFlush();

  if (m_pCCOutputPin)
    m_pCCOutputPin->DeliverEndFlush();
  m_bFlushing = FALSE;

  return hr;
}

HRESULT CLAVVideo::ReleaseLastSequenceFrame()
{
  // Release DXVA2 frames hold in the last sequence frame
  if (m_pLastSequenceFrame && m_pLastSequenceFrame->format == LAVPixFmt_DXVA2) {
    IMediaSample *pSample = (IMediaSample *)m_pLastSequenceFrame->data[0];
    IDirect3DSurface9 *pSurface = (IDirect3DSurface9 *)m_pLastSequenceFrame->data[3];
    SafeRelease(&pSample);
    SafeRelease(&pSurface);
  }
  else if (m_pLastSequenceFrame && m_pLastSequenceFrame->format == LAVPixFmt_D3D11)
  {
    // TODO D3D11
  }
  ReleaseFrame(&m_pLastSequenceFrame);

  return S_OK;
}

HRESULT CLAVVideo::PerformFlush()
{
  CAutoLock cAutoLock(&m_csReceive);

  ReleaseLastSequenceFrame();
  m_Decoder.Flush();

  m_bInDVDMenu = FALSE;

  if (m_pFilterGraph)
    avfilter_graph_free(&m_pFilterGraph);

  m_rtPrevStart = m_rtPrevStop = 0;
  memset(&m_FilterPrevFrame, 0, sizeof(m_FilterPrevFrame));

  return S_OK;
}

HRESULT CLAVVideo::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
  DbgLog((LOG_TRACE, 1, L"::NewSegment - %I64d / %I64d", tStart, tStop));

  PerformFlush();

  if (m_pCCOutputPin)
    m_pCCOutputPin->DeliverNewSegment(tStart, tStop, dRate);

  return __super::NewSegment(tStart, tStop, dRate);
}

HRESULT CLAVVideo::CheckConnect(PIN_DIRECTION dir, IPin *pPin)
{
  if (dir == PINDIR_INPUT) {
    if (!m_bRuntimeConfig && CheckApplicationBlackList(LAVC_VIDEO_REGISTRY_KEY L"\\Blacklist"))
      return E_FAIL;

    if (FilterInGraphSafe(pPin, CLSID_LAVVideo, TRUE)) {
      DbgLog((LOG_TRACE, 10, L"CLAVVideo::CheckConnect(): LAVVideo is already in this graph branch, aborting."));
      return E_FAIL;
    }
  }
  else if (dir == PINDIR_OUTPUT) {
    // Get the filter we're connecting to
    IBaseFilter *pFilter = GetFilterFromPin(pPin);
    CLSID guidFilter = GUID_NULL;
    if (pFilter != nullptr) {
      if (FAILED(pFilter->GetClassID(&guidFilter)))
        guidFilter = GUID_NULL;

      SafeRelease(&pFilter);
    }

    // Don't allow connection to the AVI Decompressor, it doesn't support a variety of our output formats properly
    if (guidFilter == CLSID_AVIDec)
      return VFW_E_TYPE_NOT_ACCEPTED;
  }
  return __super::CheckConnect(dir, pPin);
}

HRESULT CLAVVideo::BreakConnect(PIN_DIRECTION dir)
{
  DbgLog((LOG_TRACE, 10, L"::BreakConnect"));
  if (dir == PINDIR_INPUT) {
    if (m_pFilterGraph)
      avfilter_graph_free(&m_pFilterGraph);

    m_Decoder.Close();
    m_X264Build = -1;
  }
  else if (dir == PINDIR_OUTPUT)
  {
    m_Decoder.BreakConnect();
  }
  return __super::BreakConnect(dir);
}

HRESULT CLAVVideo::CompleteConnect(PIN_DIRECTION dir, IPin *pReceivePin)
{
  DbgLog((LOG_TRACE, 10, L"::CompleteConnect"));
  HRESULT hr = S_OK;
  if (dir == PINDIR_OUTPUT) {
    BOOL bFailNonDXVA = false;
    // Fail P010 software connections before Windows 10 Creators Update (presumably it was fixed before Creators already, but this is definitely a safe known condition)
    if (!IsWindows10BuildOrNewer(15063) && (m_pOutput->CurrentMediaType().subtype == MEDIASUBTYPE_P010 || m_pOutput->CurrentMediaType().subtype == MEDIASUBTYPE_P016)) {
      // Check if we're connecting to EVR
      IBaseFilter *pFilter = GetFilterFromPin(pReceivePin);
      if (pFilter != nullptr) {
        CLSID guid;
        if (SUCCEEDED(pFilter->GetClassID(&guid))) {
          DbgLog((LOG_TRACE, 10, L"-> Connecting P010/P016 to %s", WStringFromGUID(guid).c_str()));
          bFailNonDXVA = (guid == CLSID_EnhancedVideoRenderer || guid == CLSID_VideoMixingRenderer9 || guid == CLSID_VideoMixingRenderer);
        }
        SafeRelease(&pFilter);
      }
    }

    hr = m_Decoder.PostConnect(pReceivePin);
    if (SUCCEEDED(hr)) {
      if (bFailNonDXVA) {
        LAVPixelFormat fmt;
        m_Decoder.GetPixelFormat(&fmt, nullptr);

        if (fmt != LAVPixFmt_DXVA2) {
          DbgLog((LOG_TRACE, 10, L"-> Non-DXVA2 Connection rejected on this renderer and subtype"));
          return VFW_E_TYPE_NOT_ACCEPTED;
        }
      }

      CheckDirectMode();
    }
  } else if (dir == PINDIR_INPUT) {
    if (m_pInput->CurrentMediaType().subtype == MEDIASUBTYPE_MPEG2_VIDEO && !m_pSubtitleInput && (m_dwDecodeFlags & LAV_VIDEO_DEC_FLAG_DVD)) {
      m_pSubtitleInput = new CLAVVideoSubtitleInputPin(TEXT("CLAVVideoSubtitleInputPin"), this, &m_csFilter, &hr, L"Subtitle Input");
      ASSERT(SUCCEEDED(hr));
      m_SubtitleConsumer = new CLAVSubtitleConsumer(this);
      m_SubtitleConsumer->AddRef();
      m_pSubtitleInput->SetSubtitleConsumer(m_SubtitleConsumer);

      BITMAPINFOHEADER *pBIH = nullptr;
      videoFormatTypeHandler(m_pInput->CurrentMediaType(), &pBIH);
      if (pBIH) {
        m_SubtitleConsumer->SetVideoSize(pBIH->biWidth, pBIH->biHeight);
      }
    }
  }
  return hr;
}

HRESULT CLAVVideo::StartStreaming()
{
  CAutoLock cAutoLock(&m_csReceive);

  if (m_pSubtitleInput) {
    bool bFoundConsumer = false;
    ISubRenderConsumer *pSubConsumer = nullptr;
    if (SUCCEEDED(FindIntefaceInGraph(m_pOutput, __uuidof(ISubRenderConsumer), (void **)&pSubConsumer))) {
      int nc = 0;
      LPWSTR strName = nullptr, strVersion = nullptr;
      pSubConsumer->GetString("name", &strName, &nc);
      pSubConsumer->GetString("version", &strVersion, &nc);
      DbgLog((LOG_TRACE, 10, L"CLAVVideo::StartStreaming():: Found ISubRenderConsumer (%s %s)", strName, strVersion));

      // only support madVR for now, and only recent versions (anything before 0.89.10 crashes)
      if (strName && strVersion && wcscmp(strName, L"madVR") == 0) {
        int major, minor, build, rev;
        if (swscanf_s(strVersion, L"%d.%d.%d.%d", &major, &minor, &build, &rev) == 4) {
          if (major > 0 || (major == 0 && (minor > 89 || (minor == 89 && build >= 10))))
            bFoundConsumer = true;
        }
      }

      if (bFoundConsumer) {
        m_pSubtitleInput->SetSubtitleConsumer(pSubConsumer);
        SafeRelease(&m_SubtitleConsumer);
      }

      SafeRelease(&pSubConsumer);
    }

    if (bFoundConsumer == false) {
      if (m_SubtitleConsumer == nullptr) {
        m_SubtitleConsumer = new CLAVSubtitleConsumer(this);
        m_SubtitleConsumer->AddRef();

        BITMAPINFOHEADER *pBIH = nullptr;
        videoFormatTypeHandler(m_pInput->CurrentMediaType(), &pBIH);
        if (pBIH) {
          m_SubtitleConsumer->SetVideoSize(pBIH->biWidth, pBIH->biHeight);
        }
      }

      m_pSubtitleInput->SetSubtitleConsumer(m_SubtitleConsumer);
    }
  }

  return S_OK;
}

STDMETHODIMP CLAVVideo::Stop()
{
  // Get the receiver lock and prevent frame delivery
  {
    CAutoLock lck3(&m_csReceive);
    m_bFlushing = TRUE;
  }

  // actually perform the stop
  HRESULT hr = __super::Stop();

  // unblock delivery again, if we continue receiving frames
  m_bFlushing = FALSE;
  return hr;
}

HRESULT CLAVVideo::GetDeliveryBuffer(IMediaSample** ppOut, int width, int height, AVRational ar, DXVA2_ExtendedFormat dxvaExtFlags, REFERENCE_TIME avgFrameDuration)
{
  CheckPointer(ppOut, E_POINTER);

  HRESULT hr;

  if(FAILED(hr = ReconnectOutput(width, height, ar, dxvaExtFlags, avgFrameDuration))) {
    return hr;
  }

  if(FAILED(hr = m_pOutput->GetDeliveryBuffer(ppOut, nullptr, nullptr, 0))) {
    return hr;
  }

  CheckPointer(*ppOut, E_UNEXPECTED);

  AM_MEDIA_TYPE* pmt = nullptr;
  if(SUCCEEDED((*ppOut)->GetMediaType(&pmt)) && pmt) {
    CMediaType &outMt = m_pOutput->CurrentMediaType();
    BITMAPINFOHEADER *pBMINew = nullptr;
    videoFormatTypeHandler(pmt->pbFormat, &pmt->formattype, &pBMINew);
#ifdef DEBUG
    BITMAPINFOHEADER *pBMIOld = nullptr;
    videoFormatTypeHandler(outMt.pbFormat, &outMt.formattype, &pBMIOld);

    RECT rcTarget = {0};
    if (pmt->formattype == FORMAT_VideoInfo2) {
      rcTarget = ((VIDEOINFOHEADER2 *)pmt->pbFormat)->rcTarget;
    } else if (pmt->formattype == FORMAT_VideoInfo) {
      rcTarget = ((VIDEOINFOHEADER *)pmt->pbFormat)->rcTarget;
    }
    DbgLog((LOG_TRACE, 10, L"::GetDeliveryBuffer(): Sample contains new media type from downstream filter.."));
    DbgLog((LOG_TRACE, 10, L"-> Width changed from %d to %d (target: %d)", pBMIOld->biWidth, pBMINew->biWidth, rcTarget.right));
#endif

    if (pBMINew->biWidth < width) {
      DbgLog((LOG_TRACE, 10, L" -> Renderer is trying to shrink the output window, failing!"));
      (*ppOut)->Release();
      (*ppOut) = nullptr;
      DeleteMediaType(pmt);
      return E_FAIL;
    }


    if (pmt->formattype == FORMAT_VideoInfo2 && outMt.formattype == FORMAT_VideoInfo2 && m_bDXVAExtFormatSupport) {
      VIDEOINFOHEADER2 *vih2Current = (VIDEOINFOHEADER2 *)outMt.pbFormat;
      VIDEOINFOHEADER2 *vih2New = (VIDEOINFOHEADER2 *)pmt->pbFormat;
      if (vih2Current->dwControlFlags && vih2Current->dwControlFlags != vih2New->dwControlFlags) {
        m_bDXVAExtFormatSupport = FALSE;
        DbgLog((LOG_TRACE, 10, L"-> Disabled DXVA2ExtFormat Support, because renderer changed our value."));
      }
    }

    // Check image size
    DWORD lSampleSize = m_PixFmtConverter.GetImageSize(pBMINew->biWidth, abs(pBMINew->biHeight));
    if (lSampleSize != pBMINew->biSizeImage) {
      DbgLog((LOG_TRACE, 10, L"-> biSizeImage does not match our calculated sample size, correcting"));
      pBMINew->biSizeImage = lSampleSize;
      pmt->lSampleSize = lSampleSize;
      m_bSendMediaType = TRUE;
    }

    CMediaType mt = *pmt;
    m_pOutput->SetMediaType(&mt);
    DeleteMediaType(pmt);
    pmt = nullptr;
  }

  (*ppOut)->SetDiscontinuity(FALSE);
  (*ppOut)->SetSyncPoint(TRUE);

  return S_OK;
}

HRESULT CLAVVideo::ReconnectOutput(int width, int height, AVRational ar, DXVA2_ExtendedFormat dxvaExtFlags, REFERENCE_TIME avgFrameDuration, BOOL bDXVA)
{
  CMediaType mt = m_pOutput->CurrentMediaType();

  HRESULT hr = S_FALSE;
  BOOL bNeedReconnect = FALSE;
  int timeout = 100;

  DWORD dwAspectX = 0, dwAspectY = 0;
  RECT rcTargetOld = {0};
  LONG biWidthOld = 0;

  // HACK: 1280 is the value when only chroma location is set to MPEG2, do not bother to send this information, as its the same for basically every clip.
  if ((dxvaExtFlags.value & ~0xff) != 0 && (dxvaExtFlags.value & ~0xff) != 1280)
    dxvaExtFlags.SampleFormat = AMCONTROL_USED | AMCONTROL_COLORINFO_PRESENT;
  else
    dxvaExtFlags.value = 0;

  if (m_bMadVR == -1)
    m_bMadVR = FilterInGraph(PINDIR_OUTPUT, CLSID_madVR);

  if (m_bOverlayMixer == -1) {
    m_bOverlayMixer = !m_bMadVR && FilterInGraph(PINDIR_OUTPUT, CLSID_OverlayMixer);
    if (m_bOverlayMixer)
      m_bDXVAExtFormatSupport = 0;
  }

  // Determine Interlaced flags
  // - madVR handles the flags properly, so properly indicate forced deint, adaptive deint and progressive
  // - The OverlayMixer fails at interlaced support, so completely disable interlaced flags for it
  BOOL bInterlaced = IsInterlacedOutput();
  DWORD dwInterlacedFlags = 0;
  if (m_bMadVR) {
    if (bInterlaced && (m_settings.DeintMode == DeintMode_Force || m_settings.DeintMode == DeintMode_Aggressive)) {
      dwInterlacedFlags = AMINTERLACE_IsInterlaced | AMINTERLACE_DisplayModeBobOnly;
    } else if (bInterlaced) {
      dwInterlacedFlags = AMINTERLACE_IsInterlaced | AMINTERLACE_DisplayModeBobOrWeave;
    } else {
      dwInterlacedFlags = 0;
    }
  } else if (!m_bOverlayMixer) {
    dwInterlacedFlags = bInterlaced ? AMINTERLACE_IsInterlaced | AMINTERLACE_DisplayModeBobOrWeave : 0;
  }

  // Remove custom matrix settings, which are not understood upstream
  if (dxvaExtFlags.VideoTransferMatrix == 6) {
    dxvaExtFlags.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT601;
  } else if (dxvaExtFlags.VideoTransferMatrix > 5 && !m_bMadVR) {
    dxvaExtFlags.VideoTransferMatrix = DXVA2_VideoTransferMatrix_Unknown;
  }

  // madVR doesn't understand HLG yet, fallback to BT.709, which makes use of the backwards-compatible nature of HLG
  // Value 16 was used for ST2084 by madVR, which results in a terrible experience with HLG videos otherwise
  // TODO: remove once madVR learns HLG
  if (dxvaExtFlags.VideoTransferFunction == 16 && m_bMadVR) {
    dxvaExtFlags.VideoTransferFunction = DXVA2_VideoTransFunc_709;
  }

  if (mt.formattype  == FORMAT_VideoInfo) {
    VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)mt.Format();
    if (avgFrameDuration == AV_NOPTS_VALUE)
      avgFrameDuration = vih->AvgTimePerFrame;

    bNeedReconnect = (vih->rcTarget.right != width || vih->rcTarget.bottom != height || abs(vih->AvgTimePerFrame - avgFrameDuration) > 10);
  } else if (mt.formattype  == FORMAT_VideoInfo2) {
    VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)mt.Format();
    if (avgFrameDuration == AV_NOPTS_VALUE)
      avgFrameDuration = vih2->AvgTimePerFrame;

    DWORD dwARX, dwARY;
    videoFormatTypeHandler(m_pInput->CurrentMediaType().Format(), m_pInput->CurrentMediaType().FormatType(), nullptr, nullptr, &dwARX, &dwARY);

    int num = ar.num, den = ar.den;
    BOOL bStreamAR = (m_settings.StreamAR == 1) || (m_settings.StreamAR == 2 && (!(m_dwDecodeFlags & LAV_VIDEO_DEC_FLAG_STREAMAR_BLACKLIST) || !(dwARX && dwARY)));
    if (!bStreamAR || num == 0 || den == 0) {
      if (m_bForceInputAR && dwARX && dwARY) {
        num = dwARX;
        den = dwARY;
      } else {
        num = vih2->dwPictAspectRatioX;
        den = vih2->dwPictAspectRatioY;
      }
      m_bForceInputAR = FALSE;
    }
    dwAspectX = num;
    dwAspectY = den;

    // Disallow switching to non-interlaced mediatype for most renderers, as it causes dropped frames without real advantages
    if (!m_bMadVR && vih2->dwInterlaceFlags)
      dwInterlacedFlags = vih2->dwInterlaceFlags;

    bNeedReconnect = (vih2->rcTarget.right != width || vih2->rcTarget.bottom != height || vih2->dwPictAspectRatioX != num || vih2->dwPictAspectRatioY != den || abs(vih2->AvgTimePerFrame - avgFrameDuration) > 10 || (m_bDXVAExtFormatSupport && vih2->dwControlFlags != dxvaExtFlags.value) || (vih2->dwInterlaceFlags != dwInterlacedFlags));
  }

  if (!bNeedReconnect && bDXVA) {
    BITMAPINFOHEADER *pBMI = nullptr;
    videoFormatTypeHandler(mt.Format(), mt.FormatType(), &pBMI);
    bNeedReconnect = (pBMI->biCompression != mmioFOURCC('d','x','v','a'));
  }

  if (bNeedReconnect) {
    DbgLog((LOG_TRACE, 10, L"::ReconnectOutput(): Performing reconnect"));
    BITMAPINFOHEADER *pBIH = nullptr;
    if (mt.formattype == FORMAT_VideoInfo) {
      VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)mt.Format();

      rcTargetOld = vih->rcTarget;

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

      rcTargetOld = vih2->rcTarget;

      DbgLog((LOG_TRACE, 10, L"Using VIH2, Format dump:"));
      DbgLog((LOG_TRACE, 10, L"-> width: %d -> %d", vih2->rcTarget.right, width));
      DbgLog((LOG_TRACE, 10, L"-> height: %d -> %d", vih2->rcTarget.bottom, height));
      DbgLog((LOG_TRACE, 10, L"-> AR: %d:%d -> %d:%d", vih2->dwPictAspectRatioX, vih2->dwPictAspectRatioY, dwAspectX, dwAspectY));
      DbgLog((LOG_TRACE, 10, L"-> FPS: %I64d -> %I64d", vih2->AvgTimePerFrame, avgFrameDuration));
      DbgLog((LOG_TRACE, 10, L"-> interlaced: 0x%0.8x -> 0x%0.8x", vih2->dwInterlaceFlags, dwInterlacedFlags));
      DbgLog((LOG_TRACE, 10, L"-> flags: 0x%0.8x -> 0x%0.8x", vih2->dwControlFlags, m_bDXVAExtFormatSupport ? dxvaExtFlags.value : vih2->dwControlFlags));

      vih2->dwPictAspectRatioX = dwAspectX;
      vih2->dwPictAspectRatioY = dwAspectY;

      SetRect(&vih2->rcSource, 0, 0, width, height);
      SetRect(&vih2->rcTarget, 0, 0, width, height);

      vih2->AvgTimePerFrame = avgFrameDuration;
      vih2->dwInterlaceFlags = dwInterlacedFlags;
      if (m_bDXVAExtFormatSupport)
        vih2->dwControlFlags = dxvaExtFlags.value;

      pBIH = &vih2->bmiHeader;
    }

    DWORD oldSizeImage = pBIH->biSizeImage;

    biWidthOld = pBIH->biWidth;
    pBIH->biWidth = width;
    pBIH->biHeight = pBIH->biHeight < 0 ? -height : height;
    pBIH->biSizeImage = m_PixFmtConverter.GetImageSize(width, height);

    if (bDXVA)
      pBIH->biCompression = mmioFOURCC('d','x','v','a');

    if (mt.subtype == FOURCCMap('012v')) {
      pBIH->biWidth = FFALIGN(width, 48);
    }

    HRESULT hrQA = m_pOutput->GetConnected()->QueryAccept(&mt);
    if (bDXVA) {
      m_bSendMediaType = TRUE;
      m_pOutput->SetMediaType(&mt);
    } else {
receiveconnection:
      hr = m_pOutput->GetConnected()->ReceiveConnection(m_pOutput, &mt);
      if(SUCCEEDED(hr)) {
        IMediaSample *pOut = nullptr;
        if (SUCCEEDED(hr = m_pOutput->GetDeliveryBuffer(&pOut, nullptr, nullptr, 0)) && pOut) {
          AM_MEDIA_TYPE *pmt = nullptr;
          if(SUCCEEDED(pOut->GetMediaType(&pmt)) && pmt) {
            CMediaType newmt = *pmt;
            videoFormatTypeHandler(newmt.Format(), newmt.FormatType(), &pBIH);
            DbgLog((LOG_TRACE, 10, L"-> New MediaType negotiated; actual width: %d - renderer requests: %ld", width, pBIH->biWidth));
            if (pBIH->biWidth < width) {
              DbgLog((LOG_ERROR, 10, L"-> Renderer requests width smaller than image width, failing.."));
              DeleteMediaType(pmt);
              pOut->Release();
              return E_FAIL;
            }
            // Check image size
            DWORD lSampleSize = m_PixFmtConverter.GetImageSize(pBIH->biWidth, abs(pBIH->biHeight));
            if (lSampleSize != pBIH->biSizeImage) {
              DbgLog((LOG_TRACE, 10, L"-> biSizeImage does not match our calculated sample size, correcting"));
              pBIH->biSizeImage = lSampleSize;
              newmt.SetSampleSize(lSampleSize);
              m_bSendMediaType = TRUE;
            }
            // Store media type
            m_pOutput->SetMediaType(&newmt);
            m_bSendMediaType = TRUE;
            if (newmt.formattype == FORMAT_VideoInfo2 && mt.formattype == FORMAT_VideoInfo2 && m_bDXVAExtFormatSupport) {
              VIDEOINFOHEADER2 *vih2New = (VIDEOINFOHEADER2 *)newmt.pbFormat;
              if (dxvaExtFlags.value && dxvaExtFlags.value != vih2New->dwControlFlags) {
                m_bDXVAExtFormatSupport = FALSE;
                DbgLog((LOG_TRACE, 10, L"-> Disabled DXVA2ExtFormat Support, because renderer changed our value."));
              }
            }
            DeleteMediaType(pmt);
          } else { // No Stride Request? We're ok with that, too!
            // The overlay mixer doesn't ask for a stride, but it needs one anyway
            // It'll provide a buffer just in the right size, so we can calculate this here.
            if (m_bOverlayMixer) {
              long size = pOut->GetSize();
              pBIH->biWidth = size / abs(pBIH->biHeight) * 8 / pBIH->biBitCount;
            }
            DbgLog((LOG_TRACE, 10, L"-> We did not get a stride request, using width %d for stride", pBIH->biWidth));
            m_bSendMediaType = TRUE;
            m_pOutput->SetMediaType(&mt);
          }
          pOut->Release();
        }
      } else if (hr == VFW_E_BUFFERS_OUTSTANDING && timeout != -1) {
        if (timeout > 0) {
          DbgLog((LOG_TRACE, 10, L"-> Buffers outstanding, retrying in 10ms.."));
          Sleep(10);
          timeout -= 10;
        } else {
          DbgLog((LOG_TRACE, 10, L"-> Buffers outstanding, timeout reached, flushing.."));
          m_pOutput->DeliverBeginFlush();
          m_pOutput->DeliverEndFlush();
          timeout = -1;
        }
        goto receiveconnection;
      } else if (hrQA == S_OK) {
        DbgLog((LOG_TRACE, 10, L"-> Downstream accepts new format, but cannot reconnect dynamically..."));
        if (pBIH->biSizeImage > oldSizeImage) {
          DbgLog((LOG_TRACE, 10, L"-> But, we need a bigger buffer, try to adapt allocator manually"));
          IMemInputPin *pMemPin = nullptr;
          if (SUCCEEDED(hr = m_pOutput->GetConnected()->QueryInterface<IMemInputPin>(&pMemPin)) && pMemPin) {
            IMemAllocator *pMemAllocator = nullptr;
            if (SUCCEEDED(hr = pMemPin->GetAllocator(&pMemAllocator)) && pMemAllocator) {
              ALLOCATOR_PROPERTIES props, actual;
              hr = pMemAllocator->GetProperties(&props);
              hr = pMemAllocator->Decommit();
              props.cbBuffer = pBIH->biSizeImage;
              hr = pMemAllocator->SetProperties(&props, &actual);
              hr = pMemAllocator->Commit();
              SafeRelease(&pMemAllocator);
            }
          }
          SafeRelease(&pMemPin);
        } else {
          // Check if there was a stride before..
          if (rcTargetOld.right && biWidthOld > rcTargetOld.right && biWidthOld > pBIH->biWidth) {
            // If we had a stride before, the filter is apparently stride aware
            // Try to make it easier by keeping the old stride around
            pBIH->biWidth = biWidthOld;
          }
        }
        m_pOutput->SetMediaType(&mt);
        m_bSendMediaType = TRUE;
      } else {
        DbgLog((LOG_TRACE, 10, L"-> Receive Connection failed (hr: %x); QueryAccept: %x", hr, hrQA));
      }
    }
    if (bNeedReconnect && !bDXVA)
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
  int timeout = 100;

  DWORD dwAspectX, dwAspectY;
  REFERENCE_TIME rtAvg;
  BOOL bVIH1 = (outMt.formattype == FORMAT_VideoInfo);
  videoFormatTypeHandler(outMt.Format(), outMt.FormatType(), nullptr, &rtAvg, &dwAspectX, &dwAspectY);

  CMediaType mt;
  for (i = 0; i < m_PixFmtConverter.GetNumMediaTypes(); ++i) {
    m_PixFmtConverter.GetMediaType(&mt, i, width, height, dwAspectX, dwAspectY, rtAvg, IsInterlacedOutput(), bVIH1);
    //hr = m_pOutput->GetConnected()->QueryAccept(&mt);
receiveconnection:
    hr = m_pOutput->GetConnected()->ReceiveConnection(m_pOutput, &mt);
    if (hr == S_OK) {
      DbgLog((LOG_TRACE, 10, L"::NegotiatePixelFormat(): Filter accepted format with index %d", i));
      m_pOutput->SetMediaType(&mt);
      hr = S_OK;
      goto done;
    } else if (hr == VFW_E_BUFFERS_OUTSTANDING && timeout != -1) {
      if (timeout > 0) {
        DbgLog((LOG_TRACE, 10, L"-> Buffers outstanding, retrying in 10ms.."));
        Sleep(10);
        timeout -= 10;
      } else {
        DbgLog((LOG_TRACE, 10, L"-> Buffers outstanding, timeout reached, flushing.."));
        m_pOutput->DeliverBeginFlush();
        m_pOutput->DeliverEndFlush();
        timeout = -1;
      }
      goto receiveconnection;
    }
  }

  DbgLog((LOG_ERROR, 10, L"::NegotiatePixelFormat(): Unable to agree on a pixel format"));
  hr = E_FAIL;

done:
  FreeMediaType(mt);
  return hr;
}

HRESULT CLAVVideo::Receive(IMediaSample *pIn)
{
  CAutoLock cAutoLock(&m_csReceive);
  HRESULT        hr = S_OK;

  AM_SAMPLE2_PROPERTIES const *pProps = m_pInput->SampleProps();
  if(pProps->dwStreamId != AM_STREAM_MEDIA) {
    return m_pOutput->Deliver(pIn);
  }

  AM_MEDIA_TYPE *pmt = nullptr;
  if (SUCCEEDED(pIn->GetMediaType(&pmt)) && pmt) {
    CMediaType mt = *pmt;
    DeleteMediaType(pmt);
    if (mt != m_pInput->CurrentMediaType() || !(m_dwDecodeFlags & LAV_VIDEO_DEC_FLAG_DVD)) {
      DbgLog((LOG_TRACE, 10, L"::Receive(): Input sample contained media type, dynamic format change..."));
      m_Decoder.EndOfStream();
      hr = m_pInput->SetMediaType(&mt);
      if (FAILED(hr)) {
        DbgLog((LOG_ERROR, 10, L"::Receive(): Setting new media type failed..."));
        return hr;
      }
    }
  }

  m_hrDeliver = S_OK;

  // Skip over empty packets
  if (pIn->GetActualDataLength() == 0) {
    return S_OK;
  }

  hr = m_Decoder.Decode(pIn);
  if (FAILED(hr))
    return hr;

  if (FAILED(m_hrDeliver))
    return m_hrDeliver;

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
  (*ppFrame)->aspect_ratio = { 0, 1 };

  (*ppFrame)->frame_type = '?';

  return S_OK;
}

STDMETHODIMP CLAVVideo::ReleaseFrame(LAVFrame **ppFrame)
{
  CheckPointer(ppFrame, E_POINTER);

  // Allow *ppFrame to be NULL already
  if (*ppFrame) {
    FreeLAVFrameBuffers(*ppFrame);
    SAFE_CO_FREE(*ppFrame);
  }
  return S_OK;
}

HRESULT CLAVVideo::DeDirectFrame(LAVFrame *pFrame, bool bDisableDirectMode)
{
  if (!pFrame->direct)
    return S_FALSE;

  ASSERT(pFrame->direct_lock && pFrame->direct_unlock);

  LAVPixFmtDesc desc = getPixelFormatDesc(pFrame->format);

  LAVFrame tmpFrame     = *pFrame;
  pFrame->destruct      = nullptr;
  pFrame->priv_data     = nullptr;
  pFrame->direct        = false;
  pFrame->direct_lock   = nullptr;
  pFrame->direct_unlock = nullptr;
  memset(pFrame->data, 0, sizeof(pFrame->data));

  // sidedata remains on the main frame
  tmpFrame.side_data = nullptr;
  tmpFrame.side_data_count = 0;

  LAVDirectBuffer buffer;
  if (tmpFrame.direct_lock(&tmpFrame, &buffer)) {
    HRESULT hr = AllocLAVFrameBuffers(pFrame, buffer.stride[0] / desc.codedbytes);
    if (FAILED(hr)) {
      tmpFrame.direct_unlock(&tmpFrame);
      FreeLAVFrameBuffers(&tmpFrame);
      return hr;
    }

    // use slow copy, this should only be used extremely rarely
    memcpy(pFrame->data[0], buffer.data[0], pFrame->height * buffer.stride[0]);
    for (int i = 1; i < desc.planes; i++)
      memcpy(pFrame->data[i], buffer.data[i], (pFrame->height / desc.planeHeight[i]) * buffer.stride[i]);

    tmpFrame.direct_unlock(&tmpFrame);
  } else {
    // fallack, alloc anyway so nothing blows up
    HRESULT hr = AllocLAVFrameBuffers(pFrame);
    if (FAILED(hr)) {
      FreeLAVFrameBuffers(&tmpFrame);
      return hr;
    }
  }

  FreeLAVFrameBuffers(&tmpFrame);

  if (bDisableDirectMode)
    m_Decoder.SetDirectOutput(false);

  return S_OK;
}

STDMETHODIMP_(LAVFrame*) CLAVVideo::GetFlushFrame()
{
  LAVFrame *pFlushFrame = nullptr;
  AllocateFrame(&pFlushFrame);
  pFlushFrame->flags |= LAV_FRAME_FLAG_FLUSH;
  pFlushFrame->rtStart = INT64_MAX;
  pFlushFrame->rtStop = INT64_MAX;
  return pFlushFrame;
}

STDMETHODIMP CLAVVideo::Deliver(LAVFrame *pFrame)
{
  // Out-of-sequence flush event to get all frames delivered,
  // only triggered by decoders when they are already "empty"
  // so no need to flush the decoder here
  if (pFrame->flags & LAV_FRAME_FLAG_FLUSH) {
    DbgLog((LOG_TRACE, 10, L"Decoder triggered a flush..."));
    Filter(GetFlushFrame());

    ReleaseFrame(&pFrame);
    return S_FALSE;
  }

  if (m_bFlushing) {
    ReleaseFrame(&pFrame);
    return S_FALSE;
  }

  if (pFrame->rtStart == AV_NOPTS_VALUE) {
    pFrame->rtStart = m_rtPrevStop;
    pFrame->rtStop = AV_NOPTS_VALUE;
  }

  if (pFrame->rtStop == AV_NOPTS_VALUE) {
    REFERENCE_TIME duration = 0;

    CMediaType &mt = m_pOutput->CurrentMediaType();
    videoFormatTypeHandler(mt.Format(), mt.FormatType(), nullptr, &duration, nullptr, nullptr);

    REFERENCE_TIME decoderDuration = m_Decoder.GetFrameDuration();
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
  DbgLog((LOG_TRACE, 10, L"Frame, rtStart: %I64d, dur: %I64d, diff: %I64d, key: %d, type: %C, repeat: %d, interlaced: %d, tff: %d", pFrame->rtStart, pFrame->rtStop-pFrame->rtStart, pFrame->rtStart-m_rtPrevStart, pFrame->key_frame, pFrame->frame_type, pFrame->repeat, pFrame->interlaced, pFrame->tff));
#endif

  m_rtPrevStart = pFrame->rtStart;
  m_rtPrevStop  = pFrame->rtStop;

  if (!pFrame->avgFrameDuration || pFrame->avgFrameDuration == AV_NOPTS_VALUE)
    pFrame->avgFrameDuration = m_rtAvgTimePerFrame;
  else
    m_rtAvgTimePerFrame = pFrame->avgFrameDuration;

  if (pFrame->rtStart < 0) {
    ReleaseFrame(&pFrame);
    return S_OK;
  }

  // Only perform filtering if we have to.
  // DXVA Native generally can't be filtered, and the only filtering we currently support is software deinterlacing
  if ( pFrame->format == LAVPixFmt_DXVA2 || pFrame->format == LAVPixFmt_D3D11
    || !(m_Decoder.IsInterlaced(FALSE) && m_settings.SWDeintMode != SWDeintMode_None)
    || pFrame->flags & LAV_FRAME_FLAG_REDRAW) {
    return DeliverToRenderer(pFrame);
  } else {
    Filter(pFrame);
  }
  return S_OK;
}

HRESULT CLAVVideo::DeliverToRenderer(LAVFrame *pFrame)
{
  HRESULT hr = S_OK;

  // This should never get here, but better check
  if (pFrame->flags & LAV_FRAME_FLAG_FLUSH) {
    ReleaseFrame(&pFrame);
    return S_FALSE;
  }

  if (!(pFrame->flags & LAV_FRAME_FLAG_REDRAW)) {
    // Release the old End-of-Sequence frame, this ensures any "normal" frame will clear the stored EOS frame
    if (pFrame->format != LAVPixFmt_DXVA2 && pFrame->format != LAVPixFmt_D3D11) {
      ReleaseFrame(&m_pLastSequenceFrame);
      if ((pFrame->flags & LAV_FRAME_FLAG_END_OF_SEQUENCE || m_bInDVDMenu)) {
        if (pFrame->direct) {
          hr = DeDirectFrame(pFrame, false);
          if (FAILED(hr)) {
            ReleaseFrame(&pFrame);
            return hr;
          }
        }
        CopyLAVFrame(pFrame, &m_pLastSequenceFrame);
      }
    } else if (pFrame->format == LAVPixFmt_DXVA2) {
      if ((pFrame->flags & LAV_FRAME_FLAG_END_OF_SEQUENCE || m_bInDVDMenu)) {
        if (!m_pLastSequenceFrame) {
          hr = AllocateFrame(&m_pLastSequenceFrame);
          if (SUCCEEDED(hr)) {
            m_pLastSequenceFrame->format = LAVPixFmt_DXVA2;

            hr = GetD3DBuffer(m_pLastSequenceFrame);
            if (FAILED(hr)) {
              ReleaseFrame(&m_pLastSequenceFrame);
            }
          }
        }
        if (m_pLastSequenceFrame && m_pLastSequenceFrame->data[0] && m_pLastSequenceFrame->data[3]) {
          BYTE *data0 = m_pLastSequenceFrame->data[0], *data3 = m_pLastSequenceFrame->data[3];
          *m_pLastSequenceFrame = *pFrame;
          m_pLastSequenceFrame->data[0] = data0;
          m_pLastSequenceFrame->data[3] = data3;

          // Be careful not to accidentally copy the destructor of the original frame, that would end up being bad
          m_pLastSequenceFrame->destruct = nullptr;
          m_pLastSequenceFrame->priv_data = nullptr;

          // don't copy side data
          m_pLastSequenceFrame->side_data = nullptr;
          m_pLastSequenceFrame->side_data_count = 0;

          IDirect3DSurface9 *pSurface = (IDirect3DSurface9 *)m_pLastSequenceFrame->data[3];

          IDirect3DDevice9 *pDevice = nullptr;
          hr = pSurface->GetDevice(&pDevice);
          if (SUCCEEDED(hr)) {
            hr = pDevice->StretchRect((IDirect3DSurface9 *)pFrame->data[3], nullptr, pSurface, nullptr, D3DTEXF_NONE);
            if (FAILED(hr)) {
              DbgLog((LOG_TRACE, 10, L"::Decode(): Copying DXVA2 surface failed"));
            }
            SafeRelease(&pDevice);
          }
        }
      }
    }
    else if (pFrame->format == LAVPixFmt_D3D11)
    {
      // TODO D3D11
    }
  }

  if (m_bFlushing) {
    ReleaseFrame(&pFrame);
    return S_FALSE;
  }

  // Process stream-level sidedata and attach it to the frame if necessary
  if (m_SideData.Mastering.has_colorspace) {
    fillDXVAExtFormat(pFrame->ext_format, m_SideData.Mastering.color_range - 1, m_SideData.Mastering.color_primaries, m_SideData.Mastering.colorspace, m_SideData.Mastering.color_trc, m_SideData.Mastering.chroma_location, false);
  }
  if (m_SideData.Mastering.has_luminance || m_SideData.Mastering.has_primaries) {
    MediaSideDataHDR *hdr = nullptr;

    // Check if HDR data already exists
    if (pFrame->side_data && pFrame->side_data_count) {
      for (int i = 0; i < pFrame->side_data_count; i++) {
        if (pFrame->side_data[i].guidType == IID_MediaSideDataHDR) {
          hdr = (MediaSideDataHDR *)pFrame->side_data[i].data;
          break;
        }
      }
    }

    if (hdr == nullptr)
      hdr = (MediaSideDataHDR *)AddLAVFrameSideData(pFrame, IID_MediaSideDataHDR, sizeof(MediaSideDataHDR));

    processFFHDRData(hdr, &m_SideData.Mastering);
  }

  if (m_SideData.ContentLight.MaxCLL && m_SideData.ContentLight.MaxFALL) {
    MediaSideDataHDRContentLightLevel *hdr = nullptr;

    // Check if data already exists
    if (pFrame->side_data && pFrame->side_data_count) {
      for (int i = 0; i < pFrame->side_data_count; i++) {
        if (pFrame->side_data[i].guidType == IID_MediaSideDataHDRContentLightLevel) {
          hdr = (MediaSideDataHDRContentLightLevel *)pFrame->side_data[i].data;
          break;
        }
      }
    }

    if (hdr == nullptr)
      hdr = (MediaSideDataHDRContentLightLevel *)AddLAVFrameSideData(pFrame, IID_MediaSideDataHDRContentLightLevel, sizeof(MediaSideDataHDRContentLightLevel));

    hdr->MaxCLL = m_SideData.ContentLight.MaxCLL;
    hdr->MaxFALL = m_SideData.ContentLight.MaxFALL;
  }

  // Collect width/height
  int width  = pFrame->width;
  int height = pFrame->height;

  if (width == 1920 && height == 1088) {
    height = 1080;
  }

  if (m_PixFmtConverter.SetInputFmt(pFrame->format, pFrame->bpp) || m_bForceFormatNegotiation) {
    DbgLog((LOG_TRACE, 10, L"::Decode(): Changed input pixel format to %d (%d bpp)", pFrame->format, pFrame->bpp));

    CMediaType& mt = m_pOutput->CurrentMediaType();

    if (m_PixFmtConverter.GetOutputBySubtype(mt.Subtype()) != m_PixFmtConverter.GetPreferredOutput()) {
      NegotiatePixelFormat(mt, width, height);
    }
    m_bForceFormatNegotiation = FALSE;
  }
  m_PixFmtConverter.SetColorProps(pFrame->ext_format, m_settings.RGBRange);

  // Update flags for cases where the converter can change the nominal range
  if (m_PixFmtConverter.IsRGBConverterActive()) {
    if (m_settings.RGBRange != 0)
      pFrame->ext_format.NominalRange = m_settings.RGBRange == 1 ? DXVA2_NominalRange_16_235 : DXVA2_NominalRange_0_255;
    else if (pFrame->ext_format.NominalRange == DXVA2_NominalRange_Unknown)
      pFrame->ext_format.NominalRange = DXVA2_NominalRange_16_235;
  } else if (m_PixFmtConverter.GetOutputPixFmt() == LAVOutPixFmt_RGB32 || m_PixFmtConverter.GetOutputPixFmt() == LAVOutPixFmt_RGB24 || m_PixFmtConverter.GetOutputPixFmt() == LAVOutPixFmt_RGB48) {
    pFrame->ext_format.NominalRange = DXVA2_NominalRange_0_255;
  }

  // Check if we are doing RGB output
  BOOL bRGBOut = (m_PixFmtConverter.GetOutputPixFmt() == LAVOutPixFmt_RGB24 || m_PixFmtConverter.GetOutputPixFmt() == LAVOutPixFmt_RGB32);
  // And blend subtitles if we're on YUV output before blending (because the output YUV formats are more complicated to handle)
  if (m_SubtitleConsumer && m_SubtitleConsumer->HasProvider()) {
    m_SubtitleConsumer->SetVideoSize(width, height);
    m_SubtitleConsumer->RequestFrame(pFrame->rtStart, pFrame->rtStop);
    if (!bRGBOut) {
      if (pFrame->direct) {
        hr = DeDirectFrame(pFrame, true);
        if (FAILED(hr)) {
          ReleaseFrame(&pFrame);
          return hr;
        }
      }
      m_SubtitleConsumer->ProcessFrame(pFrame);
    }
  }

  // Grab a media sample, and start assembling the data for it.
  IMediaSample *pSampleOut = nullptr;
  BYTE         *pDataOut   = nullptr;

  REFERENCE_TIME avgDuration = pFrame->avgFrameDuration;
  if (avgDuration == 0)
    avgDuration = AV_NOPTS_VALUE;

  if (pFrame->format == LAVPixFmt_DXVA2 || pFrame->format == LAVPixFmt_D3D11) {
    pSampleOut = (IMediaSample *)pFrame->data[0];
    // Addref the sample if we need to. If its coming from the decoder, it should be addref'ed,
    // but if its a copy from the subtitle engine, then it should not be addref'ed.
    if (!(pFrame->flags & LAV_FRAME_FLAG_DXVA_NOADDREF))
      pSampleOut->AddRef();

    ReconnectOutput(width, height, pFrame->aspect_ratio, pFrame->ext_format, avgDuration, TRUE);
  } else {
    if(FAILED(hr = GetDeliveryBuffer(&pSampleOut, width, height, pFrame->aspect_ratio, pFrame->ext_format, avgDuration)) || FAILED(hr = pSampleOut->GetPointer(&pDataOut)) || pDataOut == nullptr) {
      SafeRelease(&pSampleOut);
      ReleaseFrame(&pFrame);
      return hr;
    }
  }

  CMediaType& mt = m_pOutput->CurrentMediaType();
  BITMAPINFOHEADER *pBIH = nullptr;
  videoFormatTypeHandler(mt.Format(), mt.FormatType(), &pBIH);

  // Set side data on the media sample
  if (pFrame->side_data_count) {
    IMediaSideData *pMediaSideData = nullptr;
    if (SUCCEEDED(hr = pSampleOut->QueryInterface(&pMediaSideData))) {
      for (int i = 0; i < pFrame->side_data_count; i++)
      {
        if (pFrame->side_data[i].guidType != IID_MediaSideDataEIA608CC)
          pMediaSideData->SetSideData(pFrame->side_data[i].guidType, pFrame->side_data[i].data, pFrame->side_data[i].size);
      }

      SafeRelease(&pMediaSideData);
    }

    if (m_pCCOutputPin && m_pCCOutputPin->IsConnected()) {
      size_t sCC = 0;
      BYTE *CC = GetLAVFrameSideData(pFrame, IID_MediaSideDataEIA608CC, &sCC);
      if (CC && sCC) {
        m_pCCOutputPin->DeliverCCData(CC, sCC, pFrame->rtStart);
      }
    }
  }

  if (pFrame->format != LAVPixFmt_DXVA2 && pFrame->format != LAVPixFmt_D3D11) {
    long required = m_PixFmtConverter.GetImageSize(pBIH->biWidth, abs(pBIH->biHeight));

    long lSampleSize = pSampleOut->GetSize();
    if (lSampleSize < required) {
      DbgLog((LOG_ERROR, 10, L"::Decode(): Buffer is too small! Actual: %d, Required: %d", lSampleSize, required));
      SafeRelease(&pSampleOut);
      ReleaseFrame(&pFrame);
      return E_FAIL;
    }
    pSampleOut->SetActualDataLength(required);

  #if defined(DEBUG) && DEBUG_PIXELCONV_TIMINGS
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
  #endif

    if (pFrame->direct && !m_PixFmtConverter.IsDirectModeSupported((uintptr_t)pDataOut, pBIH->biWidth)) {
      DeDirectFrame(pFrame, true);
    }

    if (pFrame->direct)
      m_PixFmtConverter.ConvertDirect(pFrame, pDataOut, width, height, pBIH->biWidth, abs(pBIH->biHeight));
    else
      m_PixFmtConverter.Convert(pFrame->data, pFrame->stride, pDataOut, width, height, pBIH->biWidth, abs(pBIH->biHeight));

  #if defined(DEBUG) && DEBUG_PIXELCONV_TIMINGS
    QueryPerformanceCounter(&end);
    double diff = (end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;
    m_pixFmtTimingAvg.Sample(diff);

    DbgLog((LOG_TRACE, 10, L"Pixel Mapping took %2.3fms in avg", m_pixFmtTimingAvg.Average()));
  #endif

    // Write the second view into IMediaSample3D, if available
    if (pFrame->flags & LAV_FRAME_FLAG_MVC) {
      IMediaSample3D *pSample3D = nullptr;
      if (SUCCEEDED(hr = pSampleOut->QueryInterface(&pSample3D))) {
        BYTE *pDataOut3D = nullptr;
        if (SUCCEEDED(pSample3D->Enable3D()) && SUCCEEDED(pSample3D->GetPointer3D(&pDataOut3D))) {
          m_PixFmtConverter.Convert(pFrame->stereo, pFrame->stride, pDataOut3D, width, height, pBIH->biWidth, abs(pBIH->biHeight));
        }
        SafeRelease(&pSample3D);
      }
    }

    // Once we're done with the old frame, release its buffers
    // This does not release the frame yet, just free its buffers
    FreeLAVFrameBuffers(pFrame);

    // .. and if we do RGB conversion, blend after the conversion, for improved quality
    if (bRGBOut && m_SubtitleConsumer && m_SubtitleConsumer->HasProvider()) {
      int strideBytes = pBIH->biWidth;
      LAVPixelFormat pixFmt;
      if (m_PixFmtConverter.GetOutputPixFmt() == LAVOutPixFmt_RGB32) {
        pixFmt = LAVPixFmt_RGB32;
        strideBytes *= 4;
      } else {
        pixFmt = LAVPixFmt_RGB24;
        strideBytes *= 3;
      }

      // We need to supply a LAV Frame to the subtitle API
      // So update it with the appropriate settings
      pFrame->data[0]   = pDataOut;
      pFrame->stride[0] = strideBytes;
      pFrame->format    = pixFmt;
      pFrame->bpp       = 8;
      pFrame->flags    |= LAV_FRAME_FLAG_BUFFER_MODIFY;
      m_SubtitleConsumer->ProcessFrame(pFrame);
    }

    if ((mt.subtype == MEDIASUBTYPE_RGB32 || mt.subtype == MEDIASUBTYPE_RGB24) && pBIH->biHeight > 0) {
      int bpp = (mt.subtype == MEDIASUBTYPE_RGB32) ? 4 : 3;
      flip_plane(pDataOut, pBIH->biWidth * bpp, height);
    }
  }

  BOOL bSizeChanged = FALSE;
  if (m_bSendMediaType) {
    AM_MEDIA_TYPE *sendmt = CreateMediaType(&mt);
    pSampleOut->SetMediaType(sendmt);
    DeleteMediaType(sendmt);
    m_bSendMediaType = FALSE;
    if (pFrame->format == LAVPixFmt_DXVA2 || pFrame->format == LAVPixFmt_D3D11)
      bSizeChanged = TRUE;
  }

  // Handle DVD playback rate..
  if (GetDecodeFlags() & LAV_VIDEO_DEC_FLAG_DVD) {
    CVideoInputPin* pPin = dynamic_cast<CVideoInputPin*>(m_pInput);
    AM_SimpleRateChange pinRate = pPin->GetDVDRateChange();
    if (abs(pinRate.Rate) != m_DVDRate.Rate) {
      DbgLog((LOG_TRACE, 10, L"DVD Rate changed to: %d", pinRate.Rate));
      m_DVDRate.Rate = abs(pinRate.Rate);
      m_DVDRate.StartTime = pFrame->rtStart;
    }

    if (m_DVDRate.StartTime != AV_NOPTS_VALUE && m_DVDRate.Rate != 10000) {
       pFrame->rtStart = m_DVDRate.StartTime + (pFrame->rtStart - m_DVDRate.StartTime) * m_DVDRate.Rate / 10000;
       pFrame->rtStop = m_DVDRate.StartTime + (pFrame->rtStop - m_DVDRate.StartTime) * m_DVDRate.Rate / 10000;
    }
  }

  // Set frame timings..
  pSampleOut->SetTime(&pFrame->rtStart, &pFrame->rtStop);
  pSampleOut->SetMediaTime(nullptr, nullptr);

  // And frame flags..
  SetFrameFlags(pSampleOut, pFrame);

  // Release frame before delivery, so it can be re-used by the decoder (if required)
  ReleaseFrame(&pFrame);

  hr = m_pOutput->Deliver(pSampleOut);
  if (FAILED(hr)) {
    DbgLog((LOG_ERROR, 10, L"::Decode(): Deliver failed with hr: %x", hr));
    m_hrDeliver = hr;
  }

  if (bSizeChanged)
    NotifyEvent(EC_VIDEO_SIZE_CHANGED, MAKELPARAM(pBIH->biWidth, abs(pBIH->biHeight)), 0);

  SafeRelease(&pSampleOut);

  return hr;
}

HRESULT CLAVVideo::GetD3DBuffer(LAVFrame *pFrame)
{
  CheckPointer(pFrame, E_POINTER);

  IMediaSample *pSample = nullptr;
  HRESULT hr = m_pOutput->GetDeliveryBuffer(&pSample, nullptr, nullptr, 0);
  if (SUCCEEDED(hr)) {
    IMFGetService *pService = nullptr;
    hr = pSample->QueryInterface(&pService);
    if (SUCCEEDED(hr)) {
      IDirect3DSurface9 *pSurface = nullptr;
      hr = pService->GetService(MR_BUFFER_SERVICE, __uuidof(IDirect3DSurface9), (LPVOID *)&pSurface);
      if (SUCCEEDED(hr)) {
        pFrame->data[0] = (BYTE *)pSample;
        pFrame->data[3] = (BYTE *)pSurface;
      }
      SafeRelease(&pService);
    }
  }
  return hr;
}

HRESULT CLAVVideo::RedrawStillImage()
{
  IMediaControl *pMC = nullptr;
  if (m_pGraph->QueryInterface(&pMC) == S_OK && pMC) {
    OAFilterState state = State_Stopped;
    pMC->GetState(0, &state);
    SafeRelease(&pMC);
    if (state != State_Running) {
      DbgLog((LOG_TRACE, 10, L"CLAVVideo::RedrawStillImage(): Redraw ignored, graph is not running."));
      return S_FALSE;
    }
  }

  if (m_pLastSequenceFrame) {
    CAutoLock lock(&m_csReceive);
    // Since a delivery call can clear the stored sequence frame, we need a second check here
    // Because only after we obtained the receive lock, we are in charge..
    if (!m_pLastSequenceFrame)
      return S_FALSE;

    DbgLog((LOG_TRACE, 10, L"CLAVVideo::RedrawStillImage(): Redrawing still image"));

    LAVFrame *pFrame = nullptr;

    if (m_pLastSequenceFrame->format == LAVPixFmt_DXVA2) {
      HRESULT hr = AllocateFrame(&pFrame);
      if (FAILED(hr))
        return hr;

      *pFrame = *m_pLastSequenceFrame;
      hr = GetD3DBuffer(pFrame);
      if (SUCCEEDED(hr)) {
        IMediaSample *pSample = (IMediaSample *)pFrame->data[0];
        IDirect3DSurface9 *pSurface = (IDirect3DSurface9 *)pFrame->data[3];

        pSample->SetTime(&m_pLastSequenceFrame->rtStart, &m_pLastSequenceFrame->rtStop);

        IDirect3DDevice9 *pDevice = nullptr;
        if (SUCCEEDED(pSurface->GetDevice(&pDevice))) {
          hr = pDevice->StretchRect((IDirect3DSurface9 *)m_pLastSequenceFrame->data[3], nullptr, pSurface, nullptr, D3DTEXF_NONE);
          if (SUCCEEDED(hr)) {
            pFrame->flags |= LAV_FRAME_FLAG_REDRAW|LAV_FRAME_FLAG_BUFFER_MODIFY;
            hr = Deliver(pFrame);
          }
          SafeRelease(&pDevice);
        }

        SafeRelease(&pSurface);
        SafeRelease(&pSample);
      }
      return hr;
    } else if (m_pLastSequenceFrame->format == LAVPixFmt_D3D11) {
      // TODO D3D11
    } else {
      LAVFrame *pFrame = nullptr;
      HRESULT hr = CopyLAVFrame(m_pLastSequenceFrame, &pFrame);
      if (FAILED(hr))
        return hr;

      pFrame->flags |= LAV_FRAME_FLAG_REDRAW;
      return Deliver(pFrame);
    }
    return S_FALSE;
  }

  return S_FALSE;
}

HRESULT CLAVVideo::SetFrameFlags(IMediaSample* pMS, LAVFrame *pFrame)
{
  HRESULT hr = S_OK;
  IMediaSample2 *pMS2 = nullptr;
  if (SUCCEEDED(hr = pMS->QueryInterface(&pMS2))) {
    AM_SAMPLE2_PROPERTIES props;
    if(SUCCEEDED(pMS2->GetProperties(sizeof(props), (BYTE*)&props))) {
      props.dwTypeSpecificFlags &= ~0x7f;

      if(!pFrame->interlaced)
        props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_WEAVE;

      if (pFrame->tff)
        props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_FIELD1FIRST;

      if (pFrame->repeat)
        props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_REPEAT_FIELD;

      pMS2->SetProperties(sizeof(props), (BYTE*)&props);
    }
  }
  SafeRelease(&pMS2);
  return hr;
}

STDMETHODIMP CLAVVideo::Read(LPCOLESTR pszPropName, VARIANT *pVar, IErrorLog *pErrorLog)
{
  CheckPointer(pszPropName, E_INVALIDARG);
  CheckPointer(pVar, E_INVALIDARG);

  if (_wcsicmp(pszPropName, L"STEREOSCOPIC3D") == 0) {
    const WCHAR * pszDecoder = GetActiveDecoderName();
    bool bMVC = pszDecoder && wcscmp(pszDecoder, L"msdk mvc") == 0;

    VariantClear(pVar);
    pVar->vt = VT_BSTR;
    pVar->bstrVal = SysAllocString(bMVC ? L"1" : L"0");
    return S_OK;
  }

  return E_INVALIDARG;
}

STDMETHODIMP_(BOOL) CLAVVideo::HasDynamicInputAllocator()
{
  return dynamic_cast<CVideoInputPin*>(m_pInput)->HasDynamicAllocator();
}

// ILAVVideoSettings
STDMETHODIMP CLAVVideo::SetRuntimeConfig(BOOL bRuntimeConfig)
{
  m_bRuntimeConfig = bRuntimeConfig;
  LoadSettings();

  // Tray Icon is disabled by default
  SAFE_DELETE(m_pTrayIcon);

  return S_OK;
}

STDMETHODIMP CLAVVideo::SetFormatConfiguration(LAVVideoCodec vCodec, BOOL bEnabled)
{
  if (vCodec < 0 || vCodec >= Codec_VideoNB)
    return E_FAIL;

  m_settings.bFormats[vCodec] = bEnabled;

  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetFormatConfiguration(LAVVideoCodec vCodec)
{
  if (vCodec < 0 || vCodec >= Codec_VideoNB)
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

STDMETHODIMP CLAVVideo::SetStreamAR(DWORD bStreamAR)
{
  m_settings.StreamAR = bStreamAR;
  return SaveSettings();
}

STDMETHODIMP_(DWORD) CLAVVideo::GetStreamAR()
{
  return m_settings.StreamAR;
}

STDMETHODIMP CLAVVideo::SetDeintFieldOrder(LAVDeintFieldOrder order)
{
  m_settings.DeintFieldOrder = order;
  return SaveSettings();
}

STDMETHODIMP_(LAVDeintFieldOrder) CLAVVideo::GetDeintFieldOrder()
{
  return (LAVDeintFieldOrder)m_settings.DeintFieldOrder;
}

STDMETHODIMP CLAVVideo::SetDeintAggressive(BOOL bAggressive)
{
  if (m_settings.DeintMode == DeintMode_Auto && bAggressive)
    m_settings.DeintMode = DeintMode_Aggressive;
  else if (m_settings.DeintMode == DeintMode_Aggressive && !bAggressive)
    m_settings.DeintMode = DeintMode_Auto;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetDeintAggressive()
{
  return (m_settings.DeintMode == DeintMode_Aggressive);
}

STDMETHODIMP CLAVVideo::SetDeintForce(BOOL bForce)
{
  if ((m_settings.DeintMode == DeintMode_Auto || m_settings.DeintMode == DeintMode_Aggressive) && bForce)
    m_settings.DeintMode = DeintMode_Force;
  else if (m_settings.DeintMode == DeintMode_Force && !bForce)
    m_settings.DeintMode = DeintMode_Auto;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetDeintForce()
{
  return (m_settings.DeintMode == DeintMode_Force);
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
  if (hwAccel == m_settings.HWAccel && m_Decoder.IsHWDecoderActive())
    return 2;

  HRESULT hr = E_FAIL;
  ILAVDecoder *pDecoder = CDecodeManager::CreateHWAccelDecoder(hwAccel);

  if (pDecoder) {
    hr = pDecoder->InitInterfaces(this, this);
    if (SUCCEEDED(hr)) {
      hr = pDecoder->Check();
    }
    SAFE_DELETE(pDecoder);
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

STDMETHODIMP CLAVVideo::SetHWAccelDeintOutput(LAVDeintOutput deintOutput)
{
  m_settings.HWDeintOutput = deintOutput;
  return SaveSettings();
}

STDMETHODIMP_(LAVDeintOutput) CLAVVideo::GetHWAccelDeintOutput()
{
  return (LAVDeintOutput)m_settings.HWDeintOutput;
}

STDMETHODIMP CLAVVideo::SetHWAccelDeintHQ(BOOL bHQ)
{
  m_settings.HWAccelCUVIDXVA = bHQ;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetHWAccelDeintHQ()
{
  return m_settings.HWAccelCUVIDXVA;
}

STDMETHODIMP CLAVVideo::SetSWDeintMode(LAVSWDeintModes deintMode)
{
  m_settings.SWDeintMode = deintMode;
  return SaveSettings();
}

STDMETHODIMP_(LAVSWDeintModes) CLAVVideo::GetSWDeintMode()
{
  return (LAVSWDeintModes)m_settings.SWDeintMode;
}

STDMETHODIMP CLAVVideo::SetSWDeintOutput(LAVDeintOutput deintOutput)
{
  m_settings.SWDeintOutput = deintOutput;
  return SaveSettings();
}

STDMETHODIMP_(LAVDeintOutput) CLAVVideo::GetSWDeintOutput()
{
  return (LAVDeintOutput)m_settings.SWDeintOutput;
}

STDMETHODIMP CLAVVideo::SetDeintTreatAsProgressive(BOOL bEnabled)
{
  m_settings.DeintMode = bEnabled ? DeintMode_Disable : DeintMode_Auto;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetDeintTreatAsProgressive()
{
  return (m_settings.DeintMode == DeintMode_Disable);
}

STDMETHODIMP CLAVVideo::SetDitherMode(LAVDitherMode ditherMode)
{
  m_settings.DitherMode = ditherMode;
  return SaveSettings();
}

STDMETHODIMP_(LAVDitherMode) CLAVVideo::GetDitherMode()
{
  return (LAVDitherMode)m_settings.DitherMode;
}

STDMETHODIMP CLAVVideo::SetUseMSWMV9Decoder(BOOL bEnabled)
{
  m_settings.bMSWMV9DMO = bEnabled;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetUseMSWMV9Decoder()
{
  return m_settings.bMSWMV9DMO;
}

STDMETHODIMP CLAVVideo::SetDVDVideoSupport(BOOL bEnabled)
{
  m_settings.bDVDVideo = bEnabled;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetDVDVideoSupport()
{
  return m_settings.bDVDVideo;
}

STDMETHODIMP CLAVVideo::SetHWAccelResolutionFlags(DWORD dwResFlags)
{
  m_settings.HWAccelResFlags = dwResFlags;
  return SaveSettings();
}

STDMETHODIMP_(DWORD) CLAVVideo::GetHWAccelResolutionFlags()
{
  return m_settings.HWAccelResFlags;
}

STDMETHODIMP CLAVVideo::SetTrayIcon(BOOL bEnabled)
{
  m_settings.TrayIcon = bEnabled;
  // The tray icon is created if not yet done so, however its not removed on the fly
  // Removing the icon on the fly can cause deadlocks if the config is changed from the icons thread
  if (bEnabled && m_pGraph && !m_pTrayIcon) {
    CreateTrayIcon();
  }
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetTrayIcon()
{
  return m_settings.TrayIcon;
}

STDMETHODIMP CLAVVideo::SetDeinterlacingMode(LAVDeintMode deintMode)
{
  m_settings.DeintMode = deintMode;
  return SaveSettings();
}

STDMETHODIMP_(LAVDeintMode) CLAVVideo::GetDeinterlacingMode()
{
  return m_settings.DeintMode;
}

STDMETHODIMP CLAVVideo::SetGPUDeviceIndex(DWORD dwDevice)
{
  m_dwGPUDeviceIndex = dwDevice;
  return S_OK;
}

STDMETHODIMP_(DWORD) CLAVVideo::GetHWAccelNumDevices(LAVHWAccel hwAccel)
{
  HRESULT hr = S_OK;
  ILAVDecoder *pDecoder = CDecodeManager::CreateHWAccelDecoder(hwAccel);
  DWORD dwDevices = 0;
  if (pDecoder) {
    hr = pDecoder->InitInterfaces(this, this);
    if (SUCCEEDED(hr)) {
      dwDevices = pDecoder->GetHWAccelNumDevices();
    }
    SAFE_DELETE(pDecoder);
  }
  return dwDevices;
}

STDMETHODIMP CLAVVideo::GetHWAccelDeviceInfo(LAVHWAccel hwAccel, DWORD dwIndex, BSTR *pstrDeviceName, DWORD *pdwDeviceIdentifier)
{
  HRESULT hr = E_NOINTERFACE;
  ILAVDecoder *pDecoder = CDecodeManager::CreateHWAccelDecoder(hwAccel);
  if (pDecoder) {
    hr = pDecoder->InitInterfaces(this, this);
    if (SUCCEEDED(hr)) {
      hr = pDecoder->GetHWAccelDeviceInfo(dwIndex, pstrDeviceName, pdwDeviceIdentifier);
    }
    SAFE_DELETE(pDecoder);
  }
  return hr;
}

STDMETHODIMP_(DWORD) CLAVVideo::GetHWAccelDeviceIndex(LAVHWAccel hwAccel, DWORD *pdwDeviceIdentifier)
{
  HRESULT hr = S_OK;
  if (hwAccel == HWAccel_DXVA2CopyBack) {
    DWORD dwDeviceIndex = m_settings.HWAccelDeviceDXVA2;
    DWORD dwDeviceId = m_settings.HWAccelDeviceDXVA2Desc;

    // verify the values and re-match them to adapters appropriately
    if (dwDeviceIndex != LAVHWACCEL_DEVICE_DEFAULT && dwDeviceId != 0) {
      hr = VerifyD3D9Device(dwDeviceIndex, dwDeviceId);
      if (FAILED(hr)) {
        dwDeviceIndex = LAVHWACCEL_DEVICE_DEFAULT;
        dwDeviceId = 0;
      }
    }

    if (pdwDeviceIdentifier)
      *pdwDeviceIdentifier = dwDeviceId;

    return dwDeviceIndex;
  } else if (hwAccel == HWAccel_D3D11) {
      DWORD dwDeviceIndex = m_settings.HWAccelDeviceD3D11;
      DWORD dwDeviceId = m_settings.HWAccelDeviceD3D11Desc;

      // verify the values and re-match them to adapters appropriately
      if (dwDeviceIndex != LAVHWACCEL_DEVICE_DEFAULT && dwDeviceId != 0) {
        hr = VerifyD3D11Device(dwDeviceIndex, dwDeviceId);
        if (FAILED(hr)) {
          dwDeviceIndex = LAVHWACCEL_DEVICE_DEFAULT;
          dwDeviceId = 0;
        }
      }

      if (pdwDeviceIdentifier)
        *pdwDeviceIdentifier = dwDeviceId;

      return dwDeviceIndex;
    }

  if (pdwDeviceIdentifier)
    *pdwDeviceIdentifier = 0;
  return LAVHWACCEL_DEVICE_DEFAULT;
}

STDMETHODIMP CLAVVideo::SetHWAccelDeviceIndex(LAVHWAccel hwAccel, DWORD dwIndex, DWORD dwDeviceIdentifier)
{
  HRESULT hr = S_OK;

  if (dwIndex != LAVHWACCEL_DEVICE_DEFAULT && dwDeviceIdentifier == 0)
    hr = GetHWAccelDeviceInfo(hwAccel, dwIndex, nullptr, &dwDeviceIdentifier);

  if (SUCCEEDED(hr)) {
    if (hwAccel == HWAccel_DXVA2CopyBack) {
      m_settings.HWAccelDeviceDXVA2 = dwIndex;
      m_settings.HWAccelDeviceDXVA2Desc = dwDeviceIdentifier;
    }
    else if (hwAccel == HWAccel_D3D11) {
      m_settings.HWAccelDeviceD3D11 = dwIndex;
      m_settings.HWAccelDeviceD3D11Desc = dwDeviceIdentifier;
    }
    return SaveSettings();
  }
  else {
    return E_INVALIDARG;
  }
}

STDMETHODIMP CLAVVideo::SetH264MVCDecodingOverride(BOOL bEnabled)
{
  m_settings.bH264MVCOverride = bEnabled;
  return S_OK;
}

STDMETHODIMP CLAVVideo::SetEnableCCOutputPin(BOOL bEnabled)
{
  m_settings.bCCOutputPinEnabled = bEnabled;
  return S_OK;
}

STDMETHODIMP CLAVVideo::GetHWAccelActiveDevice(BSTR *pstrDeviceName)
{
  return m_Decoder.GetHWAccelActiveDevice(pstrDeviceName);
}
