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
#include "LAVAudio.h"
#include "PostProcessor.h"

#include <MMReg.h>
#include <assert.h>

#include "moreuuids.h"
#include "DShowUtil.h"
#include "IMediaSideData.h"
#include "IMediaSideDataFFmpeg.h"

#include "AudioSettingsProp.h"

#include "registry.h"
#include "resource.h"

#include "DeCSS/DeCSSInputPin.h"

#pragma warning( push )
#pragma warning( disable : 4018 )
#pragma warning( disable : 4101 )
#pragma warning( disable : 4244 )
extern "C" {
#define AVCODEC_X86_MATHOPS_H
#include "libavformat/spdif.h"
#include "libavcodec/flac.h"
#include "libavcodec/mpegaudiodecheader.h"

extern int ff_vorbis_comment(AVFormatContext *ms, AVDictionary **m, const uint8_t *buf, int size, int parse_picture);
extern void ff_rm_reorder_sipr_data(uint8_t *buf, int sub_packet_h, int framesize);
__declspec(dllimport) extern const unsigned char ff_sipr_subpk_size[4];
};
#pragma warning( pop )

#ifdef DEBUG
#include "lavf_log.h"
#endif

extern HINSTANCE g_hInst;

// Constructor
CLAVAudio::CLAVAudio(LPUNKNOWN pUnk, HRESULT* phr)
  : CTransformFilter(NAME("lavc audio decoder"), 0, __uuidof(CLAVAudio))
{
  m_pInput = new CDeCSSTransformInputPin(TEXT("CDeCSSTransformInputPin"), this, phr, L"Input");
  if(!m_pInput) {
    *phr = E_OUTOFMEMORY;
  }
  if (FAILED(*phr)) {
    return;
  }

  m_pOutput = new CTransformOutputPin(NAME("CTransformOutputPin"), this, phr, L"Output");
  if(!m_pOutput) {
    *phr = E_OUTOFMEMORY;
  }
  if(FAILED(*phr))  {
    SAFE_DELETE(m_pInput);
    return;
  }

  LoadSettings();

  InitBitstreaming();

#ifdef DEBUG
  DbgSetModuleLevel (LOG_CUSTOM1, DWORD_MAX); // FFMPEG messages use custom1
  av_log_set_callback(lavf_log_callback);

  DbgSetModuleLevel (LOG_ERROR, DWORD_MAX);
  DbgSetModuleLevel (LOG_TRACE, DWORD_MAX);
  //DbgSetModuleLevel (LOG_CUSTOM2, DWORD_MAX); // Jitter statistics
  //DbgSetModuleLevel (LOG_CUSTOM5, DWORD_MAX); // Extensive timing options

#ifdef LAV_DEBUG_RELEASE
  DbgSetLogFileDesktop(LAVC_AUDIO_LOG_FILE);
#endif
#else
  av_log_set_callback(nullptr);
#endif
}

CLAVAudio::~CLAVAudio()
{
  SAFE_DELETE(m_pTrayIcon);
  ffmpeg_shutdown();

  ShutdownBitstreaming();

  if (m_hDllExtraDecoder) {
    FreeLibrary(m_hDllExtraDecoder);
    m_hDllExtraDecoder = nullptr;
  }

#if defined(DEBUG) && defined(LAV_DEBUG_RELEASE)
  DbgCloseLogFile();
#endif
}

STDMETHODIMP CLAVAudio::CreateTrayIcon()
{
  CAutoLock cObjectLock(m_pLock);
  if (m_pTrayIcon)
    return E_UNEXPECTED;
  if (CBaseTrayIcon::ProcessBlackList())
    return S_FALSE;
  m_pTrayIcon = new CBaseTrayIcon(this, TEXT(LAV_AUDIO), IDI_ICON1);
  return S_OK;
}

STDMETHODIMP CLAVAudio::JoinFilterGraph(IFilterGraph * pGraph, LPCWSTR pName)
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

HRESULT CLAVAudio::LoadDefaults()
{
  m_settings.TrayIcon   = FALSE;

  m_settings.DRCEnabled = FALSE;
  m_settings.DRCLevel   = 100;

  // Default all Codecs to enabled
  for(int i = 0; i < Codec_AudioNB; ++i)
    m_settings.bFormats[i] = TRUE;

  // Disable WMA codecs by default
  m_settings.bFormats[Codec_WMA2]   = FALSE;
  m_settings.bFormats[Codec_WMAPRO] = FALSE;
  m_settings.bFormats[Codec_WMALL]  = FALSE;

  // Default bitstreaming to disabled
  memset(m_settings.bBitstream, 0, sizeof(m_settings.bBitstream));

  m_settings.DTSHDFraming         = FALSE;
  m_settings.bBitstreamingFallback= TRUE;
  m_settings.AutoAVSync           = TRUE;
  m_settings.ExpandMono           = FALSE;
  m_settings.Expand61             = FALSE;
  m_settings.OutputStandardLayout = TRUE;
  m_settings.Output51Legacy       = FALSE;
  m_settings.AllowRawSPDIF        = FALSE;

  // Default all Sample Formats to enabled
  for(int i = 0; i < SampleFormat_NB; ++i)
    m_settings.bSampleFormats[i] = TRUE;
  m_settings.SampleConvertDither = TRUE;

  m_settings.AudioDelayEnabled = FALSE;
  m_settings.AudioDelay = 0;

  m_settings.MixingEnabled = FALSE;
  m_settings.MixingLayout  = AV_CH_LAYOUT_STEREO;
  m_settings.MixingFlags   = LAV_MIXING_FLAG_CLIP_PROTECTION;
  m_settings.MixingMode    = MatrixEncoding_None;
  m_settings.MixingCenterLevel   = 7071;
  m_settings.MixingSurroundLevel = 7071;
  m_settings.MixingLFELevel      = 0;

  m_settings.SuppressFormatChanges = FALSE;

  return S_OK;
}

static const WCHAR* bitstreamingCodecs[Bitstream_NB] = {
  L"ac3", L"eac3", L"truehd", L"dts", L"dtshd"
};

static const WCHAR* sampleFormats[SampleFormat_Bitstream] = {
  L"s16", L"s24", L"s32", L"u8", L"fp32"
};

HRESULT CLAVAudio::LoadSettings()
{
  LoadDefaults();
  if (m_bRuntimeConfig)
    return S_FALSE;

  ReadSettings(HKEY_LOCAL_MACHINE);
  return ReadSettings(HKEY_CURRENT_USER);
}

HRESULT CLAVAudio::ReadSettings(HKEY rootKey)
{
  HRESULT hr;
  DWORD dwVal;
  BOOL bFlag;
  BYTE *pBuf = nullptr;

  CRegistry reg = CRegistry(rootKey, LAVC_AUDIO_REGISTRY_KEY, hr, TRUE);
  if (SUCCEEDED(hr)) {
    bFlag = reg.ReadDWORD(L"TrayIcon", hr);
    if (SUCCEEDED(hr)) m_settings.TrayIcon = bFlag;

    bFlag = reg.ReadDWORD(L"DRCEnabled", hr);
    if (SUCCEEDED(hr)) m_settings.DRCEnabled = bFlag;

    dwVal = reg.ReadDWORD(L"DRCLevel", hr);
    if (SUCCEEDED(hr)) m_settings.DRCLevel = (int)dwVal;

    // Deprecated format storage
    pBuf = reg.ReadBinary(L"Formats", dwVal, hr);
    if (SUCCEEDED(hr)) {
      memcpy(&m_settings.bFormats, pBuf, min(dwVal, sizeof(m_settings.bFormats)));
      SAFE_CO_FREE(pBuf);
    }

    // Deprecated bitstreaming storage
    pBuf = reg.ReadBinary(L"Bitstreaming", dwVal, hr);
    if (SUCCEEDED(hr)) {
      memcpy(&m_settings.bBitstream, pBuf, min(dwVal, sizeof(m_settings.bBitstream)));
      SAFE_CO_FREE(pBuf);
    }

    bFlag = reg.ReadBOOL(L"DTSHDFraming", hr);
    if (SUCCEEDED(hr)) m_settings.DTSHDFraming = bFlag;

    bFlag = reg.ReadBOOL(L"BitstreamingFallback", hr);
    if (SUCCEEDED(hr)) m_settings.bBitstreamingFallback = bFlag;

    bFlag = reg.ReadBOOL(L"AutoAVSync", hr);
    if (SUCCEEDED(hr)) m_settings.AutoAVSync = bFlag;

    bFlag = reg.ReadBOOL(L"ExpandMono", hr);
    if (SUCCEEDED(hr)) m_settings.ExpandMono = bFlag;

    bFlag = reg.ReadBOOL(L"Expand61", hr);
    if (SUCCEEDED(hr)) m_settings.Expand61 = bFlag;

    bFlag = reg.ReadBOOL(L"OutputStandardLayout", hr);
    if (SUCCEEDED(hr)) m_settings.OutputStandardLayout = bFlag;

    bFlag = reg.ReadBOOL(L"Output51Legacy", hr);
    if (SUCCEEDED(hr)) m_settings.Output51Legacy = bFlag;

    bFlag = reg.ReadBOOL(L"Mixing", hr);
    if (SUCCEEDED(hr)) m_settings.MixingEnabled = bFlag;

    dwVal = reg.ReadDWORD(L"MixingLayout", hr);
    if (SUCCEEDED(hr)) m_settings.MixingLayout = dwVal;

    if (m_settings.MixingLayout == AV_CH_LAYOUT_5POINT1_BACK)
      m_settings.MixingLayout = AV_CH_LAYOUT_5POINT1;

    dwVal = reg.ReadDWORD(L"MixingFlags", hr);
    if (SUCCEEDED(hr)) m_settings.MixingFlags = dwVal;

    dwVal = reg.ReadDWORD(L"MixingMode", hr);
    if (SUCCEEDED(hr)) m_settings.MixingMode = dwVal;

    dwVal = reg.ReadDWORD(L"MixingCenterLevel", hr);
    if (SUCCEEDED(hr)) m_settings.MixingCenterLevel = dwVal;

    dwVal = reg.ReadDWORD(L"MixingSurroundLevel", hr);
    if (SUCCEEDED(hr)) m_settings.MixingSurroundLevel = dwVal;

    dwVal = reg.ReadDWORD(L"MixingLFELevel", hr);
    if (SUCCEEDED(hr)) m_settings.MixingLFELevel = dwVal;

    // Deprecated sample format storage
    pBuf = reg.ReadBinary(L"SampleFormats", dwVal, hr);
    if (SUCCEEDED(hr)) {
      memcpy(&m_settings.bSampleFormats, pBuf, min(dwVal, sizeof(m_settings.bSampleFormats)));
      SAFE_CO_FREE(pBuf);
    }

    bFlag = reg.ReadBOOL(L"AudioDelayEnabled", hr);
    if (SUCCEEDED(hr)) m_settings.AudioDelayEnabled = bFlag;

    dwVal = reg.ReadDWORD(L"AudioDelay", hr);
    if (SUCCEEDED(hr)) m_settings.AudioDelay = (int)dwVal;

    for (int i = 0; i < Bitstream_NB; ++i) {
      std::wstring key = std::wstring(L"Bitstreaming_") + std::wstring(bitstreamingCodecs[i]);
      bFlag = reg.ReadBOOL(key.c_str(), hr);
      if (SUCCEEDED(hr)) m_settings.bBitstream[i] = bFlag;
    }

    for (int i = 0; i < SampleFormat_Bitstream; ++i) {
      std::wstring key = std::wstring(L"SampleFormat_") + std::wstring(sampleFormats[i]);
      bFlag = reg.ReadBOOL(key.c_str(), hr);
      if (SUCCEEDED(hr)) m_settings.bSampleFormats[i] = bFlag;
    }

    bFlag = reg.ReadBOOL(L"SampleConvertDither", hr);
    if (SUCCEEDED(hr)) m_settings.SampleConvertDither = bFlag;
  }

  CRegistry regF = CRegistry(rootKey, LAVC_AUDIO_REGISTRY_KEY_FORMATS, hr, TRUE);
  if (SUCCEEDED(hr)) {
    for (int i = 0; i < Codec_AudioNB; ++i) {
      const codec_config_t *info = get_codec_config((LAVAudioCodec)i);
      ATL::CA2W name(info->name);
      bFlag = regF.ReadBOOL(name, hr);
      if (SUCCEEDED(hr)) m_settings.bFormats[i] = bFlag;
    }
  }

  return S_OK;
}

HRESULT CLAVAudio::SaveSettings()
{
  if (m_bRuntimeConfig)
    return S_FALSE;

  HRESULT hr;
  CreateRegistryKey(HKEY_CURRENT_USER, LAVC_AUDIO_REGISTRY_KEY);
  CRegistry reg = CRegistry(HKEY_CURRENT_USER, LAVC_AUDIO_REGISTRY_KEY, hr);
  if (SUCCEEDED(hr)) {
    reg.WriteBOOL(L"TrayIcon", m_settings.TrayIcon);
    reg.WriteBOOL(L"DRCEnabled", m_settings.DRCEnabled);
    reg.WriteDWORD(L"DRCLevel", m_settings.DRCLevel);
    reg.WriteBOOL(L"DTSHDFraming", m_settings.DTSHDFraming);
    reg.WriteBOOL(L"BitstreamingFallback", m_settings.bBitstreamingFallback);
    reg.WriteBOOL(L"AutoAVSync", m_settings.AutoAVSync);
    reg.WriteBOOL(L"ExpandMono", m_settings.ExpandMono);
    reg.WriteBOOL(L"Expand61", m_settings.Expand61);
    reg.WriteBOOL(L"OutputStandardLayout", m_settings.OutputStandardLayout);
    reg.WriteBOOL(L"Output51Legacy", m_settings.Output51Legacy);
    reg.WriteBOOL(L"AudioDelayEnabled", m_settings.AudioDelayEnabled);
    reg.WriteDWORD(L"AudioDelay", m_settings.AudioDelay);

    reg.WriteBOOL(L"Mixing", m_settings.MixingEnabled);
    reg.WriteDWORD(L"MixingLayout", m_settings.MixingLayout);
    reg.WriteDWORD(L"MixingFlags", m_settings.MixingFlags);
    reg.WriteDWORD(L"MixingMode", m_settings.MixingMode);
    reg.WriteDWORD(L"MixingCenterLevel", m_settings.MixingCenterLevel);
    reg.WriteDWORD(L"MixingSurroundLevel", m_settings.MixingSurroundLevel);
    reg.WriteDWORD(L"MixingLFELevel", m_settings.MixingLFELevel);

    reg.DeleteKey(L"Formats");
    CreateRegistryKey(HKEY_CURRENT_USER, LAVC_AUDIO_REGISTRY_KEY_FORMATS);
    CRegistry regF = CRegistry(HKEY_CURRENT_USER, LAVC_AUDIO_REGISTRY_KEY_FORMATS, hr);
    for (int i = 0; i < Codec_AudioNB; ++i) {
      const codec_config_t *info = get_codec_config((LAVAudioCodec)i);
      ATL::CA2W name(info->name);
      regF.WriteBOOL(name, m_settings.bFormats[i]);
    }

    reg.DeleteKey(L"Bitstreaming");
    for (int i = 0; i < Bitstream_NB; ++i) {
      std::wstring key = std::wstring(L"Bitstreaming_") + std::wstring(bitstreamingCodecs[i]);
      reg.WriteBOOL(key.c_str(), m_settings.bBitstream[i]);
    }

    reg.DeleteKey(L"SampleFormats");
    for (int i = 0; i < SampleFormat_Bitstream; ++i) {
      std::wstring key = std::wstring(L"SampleFormat_") + std::wstring(sampleFormats[i]);
      reg.WriteBOOL(key.c_str(), m_settings.bSampleFormats[i]);
    }

    reg.WriteBOOL(L"SampleConvertDither", m_settings.SampleConvertDither);
  }
  return S_OK;
}

void CLAVAudio::ffmpeg_shutdown()
{
  m_pAVCodec	= nullptr;
  if (m_pAVCtx) {
    avcodec_close(m_pAVCtx);
    av_freep(&m_pAVCtx->extradata);
    av_freep(&m_pAVCtx);
  }
  av_frame_free(&m_pFrame);

  if (m_pParser) {
    av_parser_close(m_pParser);
    m_pParser = nullptr;
  }

  if (m_avrContext) {
    avresample_close(m_avrContext);
    avresample_free(&m_avrContext);
  }

  FreeBitstreamContext();

  FreeDTSDecoder();

  m_nCodecId = AV_CODEC_ID_NONE;
}

// IUnknown
STDMETHODIMP CLAVAudio::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = nullptr;

  return 
    QI(ISpecifyPropertyPages)
    QI(ISpecifyPropertyPages2)
    QI2(ILAVAudioSettings)
    QI2(ILAVAudioStatus)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

// ISpecifyPropertyPages2
STDMETHODIMP CLAVAudio::GetPages(CAUUID *pPages)
{
  CheckPointer(pPages, E_POINTER);
  BOOL bShowStatusPage = m_pInput && m_pInput->IsConnected();
  pPages->cElems = bShowStatusPage ? 4 : 3;
  pPages->pElems = (GUID *)CoTaskMemAlloc(sizeof(GUID) * pPages->cElems);
  if (pPages->pElems == nullptr) {
    return E_OUTOFMEMORY;
  }
  pPages->pElems[0] = CLSID_LAVAudioSettingsProp;
  pPages->pElems[1] = CLSID_LAVAudioMixingProp;
  pPages->pElems[2] = CLSID_LAVAudioFormatsProp;
  if (bShowStatusPage)
    pPages->pElems[3] = CLSID_LAVAudioStatusProp;
  return S_OK;
}

STDMETHODIMP CLAVAudio::CreatePage(const GUID& guid, IPropertyPage** ppPage)
{
  CheckPointer(ppPage, E_POINTER);
  HRESULT hr = S_OK;

  if (*ppPage != nullptr)
    return E_INVALIDARG;

  if (guid == CLSID_LAVAudioSettingsProp)
    *ppPage = new CLAVAudioSettingsProp(nullptr, &hr);
  else if (guid == CLSID_LAVAudioMixingProp)
    *ppPage = new CLAVAudioMixingProp(nullptr, &hr);
  else if (guid == CLSID_LAVAudioFormatsProp)
    *ppPage = new CLAVAudioFormatsProp(nullptr, &hr);
  else if (guid == CLSID_LAVAudioStatusProp)
    *ppPage = new CLAVAudioStatusProp(nullptr, &hr);

  if (SUCCEEDED(hr) && *ppPage) {
    (*ppPage)->AddRef();
    return S_OK;
  } else {
    SAFE_DELETE(*ppPage);
    return E_FAIL;
  }
}

// ILAVAudioSettings
HRESULT CLAVAudio::GetDRC(BOOL *pbDRCEnabled, int *piDRCLevel)
{
  if (pbDRCEnabled) {
    *pbDRCEnabled = m_settings.DRCEnabled;
  }
  if (piDRCLevel) {
    *piDRCLevel = m_settings.DRCLevel;
  }
  return S_OK;
}

// ILAVAudioSettings
HRESULT CLAVAudio::SetRuntimeConfig(BOOL bRuntimeConfig)
{
  m_bRuntimeConfig = bRuntimeConfig;
  LoadSettings();

  // Tray Icon is disabled by default
  SAFE_DELETE(m_pTrayIcon);

  return S_OK;
}

HRESULT CLAVAudio::SetDRC(BOOL bDRCEnabled, int fDRCLevel)
{
  m_settings.DRCEnabled = bDRCEnabled;
  m_settings.DRCLevel = fDRCLevel;

  if (m_pAVCtx) {
    float fDRC = bDRCEnabled ? (float)fDRCLevel / 100.0f : 0.0f;
    av_opt_set_double(m_pAVCtx, "drc_scale", fDRC, AV_OPT_SEARCH_CHILDREN);
  }

  SaveSettings();
  return S_OK;
}

BOOL CLAVAudio::GetFormatConfiguration(LAVAudioCodec aCodec)
{
  if (aCodec < 0 || aCodec >= Codec_AudioNB)
    return FALSE;
  return m_settings.bFormats[aCodec];
}

HRESULT CLAVAudio::SetFormatConfiguration(LAVAudioCodec aCodec, BOOL bEnabled)
{
  if (aCodec < 0 || aCodec >= Codec_AudioNB)
    return E_FAIL;

  m_settings.bFormats[aCodec] = (bEnabled != 0);

  SaveSettings();
  return S_OK;
}

BOOL CLAVAudio::GetBitstreamConfig(LAVBitstreamCodec bsCodec)
{
  if (bsCodec < 0 || bsCodec >= Bitstream_NB)
    return FALSE;
  
  return m_settings.bBitstream[bsCodec];
}

HRESULT CLAVAudio::SetBitstreamConfig(LAVBitstreamCodec bsCodec, BOOL bEnabled)
{
  if (bsCodec >= Bitstream_NB)
    return E_FAIL;

  m_settings.bBitstream[bsCodec] = (bEnabled != 0);

  SaveSettings();

  m_bBitStreamingSettingsChanged = TRUE;

  return S_OK;
}

STDMETHODIMP_(BOOL) CLAVAudio::GetDTSHDFraming()
{
  return m_settings.DTSHDFraming;
}

STDMETHODIMP_(BOOL) CLAVAudio::GetBitstreamingFallback()
{
	return m_settings.bBitstreamingFallback;
}

STDMETHODIMP CLAVAudio::SetDTSHDFraming(BOOL bHDFraming)
{
  m_settings.DTSHDFraming = bHDFraming;
  SaveSettings();

  if (m_nCodecId == AV_CODEC_ID_DTS)
    m_bBitStreamingSettingsChanged = TRUE;

  return S_OK;
}

STDMETHODIMP CLAVAudio::SetBitstreamingFallback(BOOL bBitstreamingFallback)
{
	m_settings.bBitstreamingFallback = bBitstreamingFallback;
	SaveSettings();

	return S_OK;
}

STDMETHODIMP_(BOOL) CLAVAudio::GetAutoAVSync()
{
  return m_settings.AutoAVSync;
}

STDMETHODIMP CLAVAudio::SetAutoAVSync(BOOL bAutoSync)
{
  m_settings.AutoAVSync = bAutoSync;
  SaveSettings();

  return S_OK;
}

STDMETHODIMP_(BOOL) CLAVAudio::GetOutputStandardLayout()
{
  return m_settings.OutputStandardLayout;
}

STDMETHODIMP CLAVAudio::SetOutputStandardLayout(BOOL bStdLayout)
{
  m_settings.OutputStandardLayout = bStdLayout;
  SaveSettings();

  return S_OK;
}

STDMETHODIMP_(BOOL) CLAVAudio::GetExpandMono()
{
  return m_settings.ExpandMono;
}

STDMETHODIMP CLAVAudio::SetExpandMono(BOOL bExpandMono)
{
  m_settings.ExpandMono = bExpandMono;
  SaveSettings();

  return S_OK;
}

STDMETHODIMP_(BOOL) CLAVAudio::GetExpand61()
{
  return m_settings.Expand61;
}

STDMETHODIMP CLAVAudio::SetExpand61(BOOL bExpand61)
{
  m_settings.Expand61 = bExpand61;
  SaveSettings();

  return S_OK;
}

STDMETHODIMP_(BOOL) CLAVAudio::GetAllowRawSPDIFInput()
{
  return m_settings.AllowRawSPDIF;
}

STDMETHODIMP CLAVAudio::SetAllowRawSPDIFInput(BOOL bAllow)
{
  m_settings.AllowRawSPDIF = bAllow;
  SaveSettings();

  return S_OK;
}

STDMETHODIMP_(BOOL) CLAVAudio::GetSampleFormat(LAVAudioSampleFormat format)
{
  if (format < 0 || format >= SampleFormat_NB)
    return FALSE;

  return m_settings.bSampleFormats[format];
}

STDMETHODIMP CLAVAudio::SetSampleFormat(LAVAudioSampleFormat format, BOOL bEnabled)
{
  if (format < 0 || format >= SampleFormat_NB)
    return E_FAIL;

  m_settings.bSampleFormats[format] = (bEnabled != 0);
  SaveSettings();

  return S_OK;
}

STDMETHODIMP CLAVAudio::GetAudioDelay(BOOL *pbEnabled, int *pDelay)
{
  if (pbEnabled)
    *pbEnabled = m_settings.AudioDelayEnabled;
  if (pDelay)
    *pDelay = m_settings.AudioDelay;
  return S_OK;
}

STDMETHODIMP CLAVAudio::SetAudioDelay(BOOL bEnabled, int delay)
{
  m_settings.AudioDelayEnabled = bEnabled;
  m_settings.AudioDelay = delay;
  SaveSettings();

  return S_OK;
}

STDMETHODIMP CLAVAudio::SetMixingEnabled(BOOL bEnabled)
{
  m_settings.MixingEnabled = bEnabled;
  SaveSettings();

  m_bMixingSettingsChanged = TRUE;

  return S_OK;
}

STDMETHODIMP_(BOOL) CLAVAudio::GetMixingEnabled()
{
  return m_settings.MixingEnabled;
}

STDMETHODIMP CLAVAudio::SetMixingLayout(DWORD dwLayout)
{
  m_settings.MixingLayout = dwLayout;
  SaveSettings();

  m_bMixingSettingsChanged = TRUE;

  return S_OK;
}

STDMETHODIMP_(DWORD) CLAVAudio::GetMixingLayout()
{
  return m_settings.MixingLayout;
}

STDMETHODIMP CLAVAudio::SetMixingFlags(DWORD dwFlags)
{
  m_settings.MixingFlags = dwFlags;
  SaveSettings();

  m_bMixingSettingsChanged = TRUE;

  return S_OK;
}

STDMETHODIMP_(DWORD) CLAVAudio::GetMixingFlags()
{
  return m_settings.MixingFlags;
}

STDMETHODIMP CLAVAudio::SetMixingMode(LAVAudioMixingMode mixingMode)
{
  m_settings.MixingMode = mixingMode;
  SaveSettings();

  m_bMixingSettingsChanged = TRUE;

  return S_OK;
}

STDMETHODIMP_(LAVAudioMixingMode) CLAVAudio::GetMixingMode()
{
  return (LAVAudioMixingMode)m_settings.MixingMode;
}

STDMETHODIMP CLAVAudio::SetMixingLevels(DWORD dwCenterLevel, DWORD dwSurroundLevel, DWORD dwLFELevel)
{
  m_settings.MixingCenterLevel = dwCenterLevel;
  m_settings.MixingSurroundLevel = dwSurroundLevel;
  m_settings.MixingLFELevel = dwLFELevel;
  SaveSettings();

  m_bMixingSettingsChanged = TRUE;

  return S_OK;
}

STDMETHODIMP CLAVAudio::GetMixingLevels(DWORD *dwCenterLevel, DWORD *dwSurroundLevel, DWORD *dwLFELevel)
{
  if (dwCenterLevel)
    *dwCenterLevel = m_settings.MixingCenterLevel;
  if (dwSurroundLevel)
    *dwSurroundLevel = m_settings.MixingSurroundLevel;
  if (dwLFELevel)
    *dwLFELevel = m_settings.MixingLFELevel;

  return S_OK;
}

STDMETHODIMP CLAVAudio::SetTrayIcon(BOOL bEnabled)
{
  m_settings.TrayIcon = bEnabled;
  // The tray icon is created if not yet done so, however its not removed on the fly
  // Removing the icon on the fly can cause deadlocks if the config is changed from the icons thread
  if (bEnabled && m_pGraph && !m_pTrayIcon) {
    CreateTrayIcon();
  }
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVAudio::GetTrayIcon()
{
  return m_settings.TrayIcon;
}

STDMETHODIMP CLAVAudio::SetSampleConvertDithering(BOOL bEnabled)
{
  m_settings.SampleConvertDither = bEnabled;
  m_bMixingSettingsChanged = TRUE;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVAudio::GetSampleConvertDithering()
{
  return m_settings.SampleConvertDither;
}

STDMETHODIMP CLAVAudio::SetSuppressFormatChanges(BOOL bEnabled)
{
  m_settings.SuppressFormatChanges = bEnabled;
  return S_OK;
}

STDMETHODIMP_(BOOL) CLAVAudio::GetSuppressFormatChanges()
{
  return m_settings.SuppressFormatChanges;
}

STDMETHODIMP CLAVAudio::SetOutput51LegacyLayout(BOOL b51Legacy)
{
  m_settings.Output51Legacy = b51Legacy;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVAudio::GetOutput51LegacyLayout()
{
  return m_settings.Output51Legacy;
}

// ILAVAudioStatus
BOOL CLAVAudio::IsSampleFormatSupported(LAVAudioSampleFormat sfCheck)
{
  return FALSE;
}

HRESULT CLAVAudio::GetDecodeDetails(const char **pCodec, const char **pDecodeFormat, int *pnChannels, int *pSampleRate, DWORD *pChannelMask)
{
  if(!m_pInput || m_pInput->IsConnected() == FALSE || !m_pAVCtx) {
    return E_UNEXPECTED;
  }
  if (m_avBSContext) {
    if (pCodec) {
      AVCodec *codec = avcodec_find_decoder(m_nCodecId);
      *pCodec = codec->name;
    }
    if (pnChannels) {
      *pnChannels = m_avBSContext->streams[0]->codecpar->channels;
    }
    if (pSampleRate) {
      *pSampleRate = m_avBSContext->streams[0]->codecpar->sample_rate;
    }
    if (pDecodeFormat) {
      *pDecodeFormat = "";
    }
    if (pChannelMask) {
      *pChannelMask = 0;
    }
  } else {
    if (pCodec) {
      if (m_pAVCodec) {
        if (m_nCodecId == AV_CODEC_ID_DTS && m_pAVCtx && m_pAVCtx->profile != FF_PROFILE_UNKNOWN) {
          static const char *DTSProfiles[] = {
            nullptr, nullptr, "dts", "dts-es", "dts 96/24", "dts-hd hra", "dts-hd ma", "dts express"
          };
          int index = m_pAVCtx->profile / 10;
          if (index >= 0 && index < countof(DTSProfiles) && DTSProfiles[index])
            *pCodec = DTSProfiles[index];
          else
            *pCodec = "dts";
        }
        else
          *pCodec = m_pAVCodec->name;
      }
    }
    if (pnChannels) {
      *pnChannels = m_pAVCtx->channels;
    }
    if (pSampleRate) {
      *pSampleRate = m_pAVCtx->sample_rate;
    }
    if (pDecodeFormat) {
      if (IsActive())
        *pDecodeFormat = get_sample_format_desc(m_DecodeFormat);
      else
        *pDecodeFormat = "Not Running";
    }
    if (pChannelMask) {
      *pChannelMask = m_DecodeLayout;
    }
  }
  return S_OK;
}

HRESULT CLAVAudio::GetOutputDetails(const char **pOutputFormat, int *pnChannels, int *pSampleRate, DWORD *pChannelMask)
{
  if(!m_pOutput || m_pOutput->IsConnected() == FALSE) {
    return E_UNEXPECTED;
  }
  if (m_avBSContext) {
    if (pOutputFormat) {
      *pOutputFormat = get_sample_format_desc(SampleFormat_Bitstream);
    }
    return S_FALSE;
  }
  if (pOutputFormat) {
    *pOutputFormat = get_sample_format_desc(m_OutputQueue.sfFormat);
  }
  if (pnChannels) {
    *pnChannels = m_OutputQueue.wChannels;
  }
  if (pSampleRate) {
    *pSampleRate = m_OutputQueue.dwSamplesPerSec;
  }
  if (pChannelMask) {
    *pChannelMask = m_OutputQueue.dwChannelMask;
  }
  return S_OK;
}

HRESULT CLAVAudio::EnableVolumeStats()
{
  DbgLog((LOG_TRACE, 1, L"Volume Statistics Enabled"));
  m_bVolumeStats = TRUE;
  return S_OK;
}

HRESULT CLAVAudio::DisableVolumeStats()
{
  DbgLog((LOG_TRACE, 1, L"Volume Statistics Disabled"));
  m_bVolumeStats = FALSE;
  return S_OK;
}

HRESULT CLAVAudio::GetChannelVolumeAverage(WORD nChannel, float *pfDb)
{
  CheckPointer(pfDb, E_POINTER);
  if (!m_pOutput || m_pOutput->IsConnected() == FALSE || !m_bVolumeStats || m_avBSContext) {
    return E_UNEXPECTED;
  }
  if (nChannel >= m_OutputQueue.wChannels || nChannel >= 8) {
    return E_INVALIDARG;
  }
  *pfDb = m_faVolume[nChannel].Average();
  return S_OK;
}

// CTransformFilter
HRESULT CLAVAudio::CheckInputType(const CMediaType *mtIn)
{
  for(UINT i = 0; i < sudPinTypesInCount; i++) {
    if(*sudPinTypesIn[i].clsMajorType == mtIn->majortype
      && *sudPinTypesIn[i].clsMinorType == mtIn->subtype && (mtIn->formattype == FORMAT_WaveFormatEx || mtIn->formattype == FORMAT_WaveFormatExFFMPEG || mtIn->formattype == FORMAT_VorbisFormat2)) {
        return S_OK;
    }
  }

  if (m_settings.AllowRawSPDIF) {
    if (mtIn->majortype == MEDIATYPE_Audio && mtIn->formattype == FORMAT_WaveFormatEx &&
       (mtIn->subtype == MEDIASUBTYPE_PCM || mtIn->subtype == MEDIASUBTYPE_IEEE_FLOAT || mtIn->subtype == MEDIASUBTYPE_DOLBY_AC3_SPDIF)) {
        return S_OK;
    }
  }

  return VFW_E_TYPE_NOT_ACCEPTED;
}

// Get the output media types
HRESULT CLAVAudio::GetMediaType(int iPosition, CMediaType *pMediaType)
{
  DbgLog((LOG_TRACE, 5, L"GetMediaType"));
  if(m_pInput->IsConnected() == FALSE || !((m_pAVCtx && m_pAVCodec) || m_avBSContext)) {
    return E_UNEXPECTED;
  }

  if(iPosition < 0) {
    return E_INVALIDARG;
  }

  if (m_avBSContext) {
    if (iPosition == 0) {
      *pMediaType = CreateBitstreamMediaType(m_nCodecId, m_pAVCtx->sample_rate, TRUE);
      return S_OK;
    } else {
      if (!m_settings.bBitstreamingFallback)
        return VFW_S_NO_MORE_ITEMS;

      iPosition--;
    }
  }

  const int nSamplesPerSec = m_pAVCtx->sample_rate;
  int nChannels = m_pAVCtx->channels;
  DWORD dwChannelMask = get_channel_mask(nChannels);

  AVSampleFormat sample_fmt = (m_pAVCtx->sample_fmt != AV_SAMPLE_FMT_NONE) ? m_pAVCtx->sample_fmt : (m_pAVCodec->sample_fmts ? m_pAVCodec->sample_fmts[0] : AV_SAMPLE_FMT_NONE);
  if (sample_fmt == AV_SAMPLE_FMT_NONE)
    sample_fmt = AV_SAMPLE_FMT_S32; // this gets mapped to S16/S24/S32 in get_lav_sample_fmt based on the bits per sample

  // Prefer bits_per_raw_sample if set, but if not, try to do a better guess with bits per coded sample
  int bits = m_pAVCtx->bits_per_raw_sample ? m_pAVCtx->bits_per_raw_sample : m_pAVCtx->bits_per_coded_sample;

  LAVAudioSampleFormat lav_sample_fmt;
  if (m_pDTSDecoderContext) {
    bits = m_DTSBitDepth;
    lav_sample_fmt = (m_DTSBitDepth == 24) ? SampleFormat_24 : SampleFormat_16;
  } else
    lav_sample_fmt = get_lav_sample_fmt(sample_fmt, bits);

  if (m_settings.MixingEnabled) {
    if (nChannels != av_get_channel_layout_nb_channels(m_settings.MixingLayout)
      && (nChannels > 2 || !(m_settings.MixingFlags & LAV_MIXING_FLAG_UNTOUCHED_STEREO))) {
      lav_sample_fmt = SampleFormat_FP32;
      bits = 32;
      dwChannelMask = m_settings.MixingLayout;
      nChannels = av_get_channel_layout_nb_channels(dwChannelMask);
    } else if (nChannels == 7 && m_settings.Expand61) {
      nChannels = 8;
      dwChannelMask = get_channel_mask(nChannels);
    } else if (nChannels == 1 && m_settings.ExpandMono) {
      nChannels = 2;
      dwChannelMask = get_channel_mask(nChannels);
    }
  }

  // map to legacy 5.1 if user requested
  if (dwChannelMask == AV_CH_LAYOUT_5POINT1 && m_settings.Output51Legacy)
    dwChannelMask = AV_CH_LAYOUT_5POINT1_BACK;

  if (dwChannelMask == AV_CH_LAYOUT_5POINT1 && iPosition > 1 && iPosition < 4)
    dwChannelMask = AV_CH_LAYOUT_5POINT1_BACK;
  else if (iPosition > 1)
    return VFW_S_NO_MORE_ITEMS;

  if (iPosition % 2) {
    lav_sample_fmt = SampleFormat_16;
    bits = 16;
  } else {
    lav_sample_fmt = GetBestAvailableSampleFormat(lav_sample_fmt, &bits, TRUE);
  }

  *pMediaType = CreateMediaType(lav_sample_fmt, nSamplesPerSec, nChannels, dwChannelMask, bits);
  return S_OK;
}

HRESULT CLAVAudio::ReconnectOutput(long cbBuffer, CMediaType& mt)
{
  HRESULT hr = S_FALSE;

  IMemInputPin *pPin = nullptr;
  IMemAllocator *pAllocator = nullptr;

  CHECK_HR(hr = m_pOutput->GetConnected()->QueryInterface(&pPin));

  if(FAILED(hr = pPin->GetAllocator(&pAllocator)) || !pAllocator) {
    goto done;
  }

  ALLOCATOR_PROPERTIES props, actual;
  CHECK_HR(hr = pAllocator->GetProperties(&props));

  hr = S_FALSE;

  if(mt != m_pOutput->CurrentMediaType() || cbBuffer > props.cbBuffer) {
    DbgLog((LOG_TRACE, 10, L"::ReconnectOutput(): Reconnecting output because media type or buffer size changed..."));
    if(cbBuffer > props.cbBuffer) {
      DbgLog((LOG_TRACE, 10, L"::ReconnectOutput(): -> Increasing buffer size"));
      props.cBuffers = 4;
      props.cbBuffer = cbBuffer*3/2;

      if(FAILED(hr = m_pOutput->DeliverBeginFlush())
        || FAILED(hr = m_pOutput->DeliverEndFlush())
        || FAILED(hr = pAllocator->Decommit())
        || FAILED(hr = pAllocator->SetProperties(&props, &actual))
        || FAILED(hr = pAllocator->Commit())) {
          goto done;
      }

      if(props.cBuffers > actual.cBuffers || props.cbBuffer > actual.cbBuffer) {
        NotifyEvent(EC_ERRORABORT, hr, 0);
        hr = E_FAIL;
        goto done;
      }
    }

    hr = S_OK;
  }

done:
  SafeRelease(&pPin);
  SafeRelease(&pAllocator);
  return hr;
}

CMediaType CLAVAudio::CreateMediaType(LAVAudioSampleFormat outputFormat, DWORD nSamplesPerSec, WORD nChannels, DWORD dwChannelMask, WORD wBitsPerSample) const
{
  CMediaType mt;

  mt.majortype = MEDIATYPE_Audio;
  mt.subtype = (outputFormat == SampleFormat_FP32) ? MEDIASUBTYPE_IEEE_FLOAT : MEDIASUBTYPE_PCM;
  mt.formattype = FORMAT_WaveFormatEx;

  WAVEFORMATEXTENSIBLE wfex;
  memset(&wfex, 0, sizeof(wfex));

  if (wBitsPerSample >> 3 > get_byte_per_sample(outputFormat)) {
    DbgLog((LOG_TRACE, 20, L"Invalid combination of sample format and bits per sample"));
    outputFormat = get_lav_sample_fmt(AV_SAMPLE_FMT_S32, wBitsPerSample);
  }

  WAVEFORMATEX* wfe = &wfex.Format;
  wfe->wFormatTag = (WORD)mt.subtype.Data1;
  wfe->nChannels = nChannels;
  wfe->nSamplesPerSec = nSamplesPerSec;
  wfe->wBitsPerSample = get_byte_per_sample(outputFormat) << 3;
  wfe->nBlockAlign = wfe->nChannels * wfe->wBitsPerSample / 8;
  wfe->nAvgBytesPerSec = wfe->nSamplesPerSec * wfe->nBlockAlign;

  if(dwChannelMask == 0 && (wfe->wBitsPerSample > 16 || wfe->nSamplesPerSec > 48000)) {
    dwChannelMask = nChannels == 2 ? (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT) : SPEAKER_FRONT_CENTER;
  }

  // Dont use a channel mask for "default" mono/stereo sources
  if ((outputFormat == SampleFormat_FP32 || wfe->wBitsPerSample <= 16) && wfe->nSamplesPerSec <= 48000 && ((nChannels == 1 && dwChannelMask == SPEAKER_FRONT_CENTER) || (nChannels == 2 && dwChannelMask == (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT)))) {
    dwChannelMask = 0;
  }

  if(dwChannelMask) {
    wfex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfex.Format.cbSize = sizeof(wfex) - sizeof(wfex.Format);
    wfex.dwChannelMask = dwChannelMask;
    if (wBitsPerSample > 0 && (outputFormat == SampleFormat_24 || outputFormat == SampleFormat_32)) {
      WORD wBpp = wBitsPerSample;
      if ( (outputFormat == SampleFormat_24 && wBpp <= 16)
        || (outputFormat == SampleFormat_32 && wBpp < 24))
        wBpp = 24;
      wfex.Samples.wValidBitsPerSample = wBpp;
    } else {
      wfex.Samples.wValidBitsPerSample = wfex.Format.wBitsPerSample;
    }
    wfex.SubFormat = mt.subtype;
  }

  mt.SetSampleSize(wfe->wBitsPerSample * wfe->nChannels / 8);
  mt.SetFormat((BYTE*)&wfex, sizeof(wfex.Format) + wfex.Format.cbSize);

  return mt;
}

// Check if the types are compatible
HRESULT CLAVAudio::CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut)
{
  // Check major types
  if (FAILED(CheckInputType(mtIn)) || mtOut->majortype != MEDIATYPE_Audio || (mtOut->subtype != MEDIASUBTYPE_PCM && mtOut->subtype != MEDIASUBTYPE_IEEE_FLOAT)
    || mtOut->formattype != FORMAT_WaveFormatEx) {
    return VFW_E_TYPE_NOT_ACCEPTED;
  } else {
    // Check for valid pcm settings, but only when output type is changing
    if (!m_avBSContext && m_pAVCtx && *mtOut != m_pOutput->CurrentMediaType()) {
      WAVEFORMATEX *wfex = (WAVEFORMATEX *)mtOut->pbFormat;
      if (wfex->nSamplesPerSec != m_pAVCtx->sample_rate) {
        return VFW_E_TYPE_NOT_ACCEPTED;
      }
    }
  }
  return S_OK;
}

HRESULT CLAVAudio::DecideBufferSize(IMemAllocator* pAllocator, ALLOCATOR_PROPERTIES* pProperties)
{
  if(m_pInput->IsConnected() == FALSE) {
    return E_UNEXPECTED;
  }

  /*CMediaType& mt = m_pInput->CurrentMediaType();
  WAVEFORMATEX* wfe = (WAVEFORMATEX*)mt.Format();
  UNUSED_ALWAYS(wfe); */

  pProperties->cBuffers = 4;
  // TODO: we should base this on the output media type
  pProperties->cbBuffer = LAV_AUDIO_BUFFER_SIZE; // 48KHz 6ch 32bps 100ms
  pProperties->cbAlign = 1;
  pProperties->cbPrefix = 0;

  HRESULT hr;
  ALLOCATOR_PROPERTIES Actual;
  if(FAILED(hr = pAllocator->SetProperties(pProperties, &Actual))) {
    return hr;
  }

  return pProperties->cBuffers > Actual.cBuffers || pProperties->cbBuffer > Actual.cbBuffer
    ? E_FAIL
    : NOERROR;
}

HRESULT CLAVAudio::ffmpeg_init(AVCodecID codec, const void *format, const GUID format_type, DWORD formatlen)
{
  CAutoLock lock(&m_csReceive);
  ffmpeg_shutdown();
  DbgLog((LOG_TRACE, 10, L"::ffmpeg_init(): Initializing decoder for codec %S", avcodec_get_name(codec)));

  if (codec == AV_CODEC_ID_DTS || codec == AV_CODEC_ID_TRUEHD) {
    m_faJitter.SetNumSamples(200);
    m_JitterLimit = MAX_JITTER_DESYNC * 10;
  } else {
    m_faJitter.SetNumSamples(50);
    m_JitterLimit = MAX_JITTER_DESYNC;
  }

  // Fake codecs that are dependant in input bits per sample, mostly to handle QT PCM tracks
  if (codec == AV_CODEC_ID_PCM_QTRAW || codec == AV_CODEC_ID_PCM_SxxBE || codec == AV_CODEC_ID_PCM_SxxLE || codec == AV_CODEC_ID_PCM_UxxBE || codec == AV_CODEC_ID_PCM_UxxLE) {
    if (format_type == FORMAT_WaveFormatEx) {
      WAVEFORMATEX *wfein = (WAVEFORMATEX *)format;
      ASSERT(wfein->wBitsPerSample == 8 || wfein->wBitsPerSample == 16);
      switch(codec) {
      case AV_CODEC_ID_PCM_QTRAW:
        codec = wfein->wBitsPerSample == 8 ? AV_CODEC_ID_PCM_U8 : AV_CODEC_ID_PCM_S16BE;
        break;
      case AV_CODEC_ID_PCM_SxxBE:
        codec = wfein->wBitsPerSample == 8 ? AV_CODEC_ID_PCM_S8 : AV_CODEC_ID_PCM_S16BE;
        break;
      case AV_CODEC_ID_PCM_SxxLE:
        codec = wfein->wBitsPerSample == 8 ? AV_CODEC_ID_PCM_S8 : AV_CODEC_ID_PCM_S16LE;
        break;
      case AV_CODEC_ID_PCM_UxxBE:
        codec = wfein->wBitsPerSample == 8 ? AV_CODEC_ID_PCM_U8 : AV_CODEC_ID_PCM_U16BE;
        break;
      case AV_CODEC_ID_PCM_UxxLE:
        codec = wfein->wBitsPerSample == 8 ? AV_CODEC_ID_PCM_U8 : AV_CODEC_ID_PCM_U16LE;
        break;
      }
    }
  }

  // Special check for enabled PCM
  if (codec >= 0x10000 && codec < 0x12000 && codec != AV_CODEC_ID_PCM_BLURAY && codec != AV_CODEC_ID_PCM_DVD && !m_settings.bFormats[Codec_PCM])
    return VFW_E_UNSUPPORTED_AUDIO;

  for(int i = 0; i < Codec_AudioNB; ++i) {
    const codec_config_t *config = get_codec_config((LAVAudioCodec)i);
    bool bMatched = false;
    for (int k = 0; k < config->nCodecs; ++k) {
      if (config->codecs[k] == codec) {
        bMatched = true;
        break;
      }
    }
    if (bMatched && !m_settings.bFormats[i]) {
      return VFW_E_UNSUPPORTED_AUDIO;
    }
  }

  WCHAR fileName[1024];
  GetModuleFileName(nullptr, fileName, 1024);
  std::wstring processName = PathFindFileName(fileName);

  if (m_bHasVideo == -1) {
    m_bHasVideo =  _wcsicmp(processName.c_str(), L"dllhost.exe") == 0
                || _wcsicmp(processName.c_str(), L"explorer.exe") == 0
                || _wcsicmp(processName.c_str(), L"ReClockHelper.dll") == 0
                || _wcsicmp(processName.c_str(), L"dvbviewer.exe") == 0
                || HasSourceWithType(m_pInput, MEDIATYPE_Video)
                || m_pInput->CurrentMediaType().majortype != MEDIATYPE_Audio;
  }
  DbgLog((LOG_TRACE, 10, L"-> Do we have video? %d", m_bHasVideo));

  // If the codec is bitstreaming, and enabled for it, go there now
  if (IsBitstreaming(codec)) {
    WAVEFORMATEX *wfe = (format_type == FORMAT_WaveFormatEx) ? (WAVEFORMATEX *)format : nullptr;

    if (wfe)
      CreateBitstreamContext(codec, wfe);
  }

  if (codec == AV_CODEC_ID_DTS) {
    InitDTSDecoder();
  }

  m_pAVCodec = nullptr;

  // Try codec overrides
  const char *codec_override = find_codec_override(codec);
  if (codec_override)
    m_pAVCodec = avcodec_find_decoder_by_name(codec_override);
  
  // Fallback to default
  if (!m_pAVCodec)
    m_pAVCodec = avcodec_find_decoder(codec);
  CheckPointer(m_pAVCodec, VFW_E_UNSUPPORTED_AUDIO);

  m_pAVCtx = avcodec_alloc_context3(m_pAVCodec);
  CheckPointer(m_pAVCtx, E_POINTER);

  if ((codec != AV_CODEC_ID_AAC || m_pInput->CurrentMediaType().subtype == MEDIASUBTYPE_MPEG_ADTS_AAC) && codec != AV_CODEC_ID_FLAC && codec != AV_CODEC_ID_COOK)
    m_pParser = av_parser_init(codec);

  if (codec == AV_CODEC_ID_OPUS) {
    m_pAVCtx->request_sample_fmt = AV_SAMPLE_FMT_FLT;
  } else if (codec == AV_CODEC_ID_ALAC) {
    m_pAVCtx->request_sample_fmt = AV_SAMPLE_FMT_S32P;
  }

  // We can only trust LAV Splitters LATM AAC header...
  BOOL bTrustExtraData = TRUE;
  if (codec == AV_CODEC_ID_AAC_LATM) {
    if (!(FilterInGraphSafe(m_pInput, CLSID_LAVSplitter) || FilterInGraphSafe(m_pInput, CLSID_LAVSplitterSource))) {
      bTrustExtraData = FALSE;
    }
  }

  DWORD nSamples, nBytesPerSec;
  WORD nChannels, nBitsPerSample, nBlockAlign;
  audioFormatTypeHandler((BYTE *)format, &format_type, &nSamples, &nChannels, &nBitsPerSample, &nBlockAlign, &nBytesPerSec);

  size_t extralen = 0;
  getExtraData((BYTE *)format, &format_type, formatlen, nullptr, &extralen);

  m_pAVCtx->thread_count          = 1;
  m_pAVCtx->thread_type           = 0;
  m_pAVCtx->sample_rate           = nSamples;
  m_pAVCtx->channels              = nChannels;
  m_pAVCtx->bit_rate              = nBytesPerSec << 3;
  m_pAVCtx->bits_per_coded_sample = nBitsPerSample;
  m_pAVCtx->block_align           = nBlockAlign;
  m_pAVCtx->err_recognition       = 0;
  m_pAVCtx->refcounted_frames     = 1;
  m_pAVCtx->pkt_timebase.num      = 1;
  m_pAVCtx->pkt_timebase.den      = 10000000;

  memset(&m_raData, 0, sizeof(m_raData));

  if (bTrustExtraData && extralen) {
    if (codec == AV_CODEC_ID_COOK || codec == AV_CODEC_ID_ATRAC3 || codec == AV_CODEC_ID_SIPR) {
      uint8_t *extra = (uint8_t *)av_mallocz(extralen + AV_INPUT_BUFFER_PADDING_SIZE);
      getExtraData((BYTE *)format, &format_type, formatlen, extra, nullptr);

      if (extra[0] == '.' && extra[1] == 'r' && extra[2] == 'a' && extra[3] == 0xfd) {
        HRESULT hr = ParseRealAudioHeader(extra, extralen);
        av_freep(&extra);
        if (FAILED(hr))
          return hr;

        if (codec == AV_CODEC_ID_SIPR) {
          if (m_raData.flavor > 3) {
            DbgLog((LOG_TRACE, 10, L"->  Invalid SIPR flavor (%d)", m_raData.flavor));
            return VFW_E_UNSUPPORTED_AUDIO;
          }
          m_pAVCtx->block_align = ff_sipr_subpk_size[m_raData.flavor];
        } else if (codec == AV_CODEC_ID_COOK || codec == AV_CODEC_ID_ATRAC3) {
          m_pAVCtx->block_align = m_raData.sub_packet_size;
        }

        // Validate some settings
        if (m_raData.deint_id == MKBETAG('g', 'e', 'n', 'r')) {
          if (m_raData.sub_packet_size <= 0 || m_raData.sub_packet_size > m_raData.audio_framesize)
            return VFW_E_UNSUPPORTED_AUDIO;
        }
      } else {
        // Try without any processing?
        m_pAVCtx->extradata_size = (int)extralen;
        m_pAVCtx->extradata      = extra;
      }
    } else {
      m_pAVCtx->extradata_size      = (int)extralen;
      m_pAVCtx->extradata           = (uint8_t *)av_mallocz(m_pAVCtx->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
      getExtraData((BYTE *)format, &format_type, formatlen, m_pAVCtx->extradata, nullptr);
    }
  }

  m_nCodecId = codec;

  int ret = avcodec_open2(m_pAVCtx, m_pAVCodec, nullptr);
  if (ret >= 0) {
    m_pFrame   = av_frame_alloc();
  } else {
    return VFW_E_UNSUPPORTED_AUDIO;
  }

  // Set initial format for DTS based on the best guess
  if (codec == AV_CODEC_ID_DTS) {
    if (m_pInput->CurrentMediaType().subtype == MEDIASUBTYPE_DTS_HD) {
      // HD with 16 or 24 bits is likely HD MA
      // everything else outputs float
      if (nBitsPerSample == 24) {
        m_pAVCtx->sample_fmt = AV_SAMPLE_FMT_S32P;
        m_pAVCtx->bits_per_raw_sample = 24;
      }
      else if (nBitsPerSample == 16) {
        m_pAVCtx->sample_fmt = AV_SAMPLE_FMT_S16P;
        m_pAVCtx->bits_per_raw_sample = 16;
      }
      else {
        m_pAVCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
      }
    }
    // Lossy is by default float output
    else {
      m_pAVCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    }
  }

  // Parse VorbisComment entries from FLAC extradata to find the WAVEFORMATEXTENSIBLE_CHANNEL_MASK tag
  // This tag is used to store non-standard channel layouts in FLAC files, see LAV-8 / Issue 342
  if (codec == AV_CODEC_ID_FLAC) {
    enum FLACExtradataFormat format;
    uint8_t *streaminfo;
    ret = ff_flac_is_extradata_valid(m_pAVCtx, &format, &streaminfo);
    if (ret && format == FLAC_EXTRADATA_FORMAT_FULL_HEADER) {
      AVDictionary *metadata = nullptr;
      int metadata_last = 0, metadata_type, metadata_size;
      uint8_t *header = m_pAVCtx->extradata + 4, *end = m_pAVCtx->extradata + m_pAVCtx->extradata_size;
      while (header + 4 < end && !metadata_last) {
        flac_parse_block_header(header, &metadata_last, &metadata_type, &metadata_size);
        header += 4;
        if (header + metadata_size > end)
          break;
        switch (metadata_type) {
        case FLAC_METADATA_TYPE_VORBIS_COMMENT:
          ff_vorbis_comment(nullptr, &metadata, header, metadata_size, 0);
          break;
        }
        header += metadata_size;
      }
      if (metadata) {
        AVDictionaryEntry *entry = av_dict_get(metadata, "WAVEFORMATEXTENSIBLE_CHANNEL_MASK", nullptr, 0);
        if (entry && entry->value) {
          uint64_t channel_layout = strtol(entry->value, nullptr, 0);
          if (channel_layout && av_get_channel_layout_nb_channels(channel_layout) == m_pAVCtx->channels)
            m_pAVCtx->channel_layout = channel_layout;
        }
        av_dict_free(&metadata);
      }
    }
  }

  // Set Dynamic Range Compression
  float drc_scale = m_settings.DRCEnabled ? (float)m_settings.DRCLevel / 100.0f : 0.0f;
  ret = av_opt_set_double(m_pAVCtx, "drc_scale", drc_scale, AV_OPT_SEARCH_CHILDREN);

  // This could probably be a bit smarter..
  if (codec == AV_CODEC_ID_PCM_BLURAY || codec == AV_CODEC_ID_PCM_DVD) {
    m_pAVCtx->bits_per_raw_sample = m_pAVCtx->bits_per_coded_sample;
  }

  // Some sanity checks
  if (m_pAVCtx->channels > 8) {
    return VFW_E_UNSUPPORTED_AUDIO;
  }

  m_bFindDTSInPCM = (codec == AV_CODEC_ID_PCM_S16LE && m_settings.bFormats[Codec_DTS]);
  m_FallbackFormat = SampleFormat_None;
  m_dwOverrideMixer = 0;
  m_bMixingSettingsChanged = TRUE;
  m_SuppressLayout = 0;
  m_bMPEGAudioResync = (m_pInput->CurrentMediaType().subtype == MEDIASUBTYPE_MPEG1AudioPayload);

  return S_OK;
}

HRESULT CLAVAudio::SetMediaType(PIN_DIRECTION dir, const CMediaType *pmt)
{
  DbgLog((LOG_TRACE, 5, L"SetMediaType -- %S", dir == PINDIR_INPUT ? "in" : "out"));
  if (dir == PINDIR_INPUT) {
    AVCodecID codec = AV_CODEC_ID_NONE;
    const void *format = pmt->Format();
    GUID format_type = pmt->formattype;
    DWORD formatlen = pmt->cbFormat;

    // Override the format type
    if (pmt->subtype == MEDIASUBTYPE_FFMPEG_AUDIO && pmt->formattype == FORMAT_WaveFormatExFFMPEG) {
      WAVEFORMATEXFFMPEG *wfexff = (WAVEFORMATEXFFMPEG *)pmt->Format();
      codec = (AVCodecID)wfexff->nCodecId;
      format = &wfexff->wfex;
      format_type = FORMAT_WaveFormatEx;
      formatlen -= sizeof(WAVEFORMATEXFFMPEG) - sizeof(WAVEFORMATEX);
    } else {
      codec = FindCodecId(pmt);
    }

    if (codec == AV_CODEC_ID_NONE) {
      if (m_settings.AllowRawSPDIF) {
        if (pmt->formattype == FORMAT_WaveFormatEx && pmt->subtype == MEDIASUBTYPE_PCM) {
          WAVEFORMATEX *wfex = (WAVEFORMATEX *)pmt->Format();
          switch (wfex->wBitsPerSample) {
          case 8:
            codec = AV_CODEC_ID_PCM_U8;
            break;
          case 16:
            codec = AV_CODEC_ID_PCM_S16LE;
            break;
          case 24:
            codec = AV_CODEC_ID_PCM_S24LE;
            break;
          case 32:
            codec = AV_CODEC_ID_PCM_S32LE;
            break;
          }
        } else if (pmt->formattype == FORMAT_WaveFormatEx && pmt->subtype == MEDIASUBTYPE_IEEE_FLOAT) {
          codec = AV_CODEC_ID_PCM_F32LE;
        } else if (pmt->subtype == MEDIASUBTYPE_DOLBY_AC3_SPDIF) {
          codec = AV_CODEC_ID_AC3;
        }
      }
      if (codec == AV_CODEC_ID_NONE)
        return VFW_E_TYPE_NOT_ACCEPTED;
    }

    HRESULT hr = ffmpeg_init(codec, format, format_type, formatlen);
    if (FAILED(hr)) {
      return hr;
    }

    m_bDVDPlayback = (pmt->majortype == MEDIATYPE_DVD_ENCRYPTED_PACK || pmt->majortype == MEDIATYPE_MPEG2_PACK || pmt->majortype == MEDIATYPE_MPEG2_PES);

  }
  return __super::SetMediaType(dir, pmt);
}

HRESULT CLAVAudio::CheckConnect(PIN_DIRECTION dir, IPin *pPin)
{
  DbgLog((LOG_TRACE, 5, L"CheckConnect -- %S", dir == PINDIR_INPUT ? "in" : "out"));
  if (dir == PINDIR_INPUT) {
    if (!m_bRuntimeConfig && CheckApplicationBlackList(LAVC_AUDIO_REGISTRY_KEY L"\\Blacklist"))
      return E_FAIL;

    // TODO: Check if the upstream source filter is LAVFSplitter, and store that somewhere
    // Validate that this is called before any media type negotiation
  }
  return __super::CheckConnect(dir, pPin);
}

HRESULT CLAVAudio::CompleteConnect(PIN_DIRECTION dir, IPin *pReceivePin)
{
  DbgLog((LOG_TRACE, 5, L"CompleteConnect -- %S", dir == PINDIR_INPUT ? "in" : "out"));
  if (dir == PINDIR_OUTPUT)
  {
    // check that we connected with a bitstream type, or go back to decoding otherwise
    if (m_avBSContext && m_settings.bBitstreamingFallback) {
      CMediaType &mt = m_pOutput->CurrentMediaType();
      WAVEFORMATEX *wfe = (WAVEFORMATEX *)mt.Format();
      bool bPCM = false;

      // float is always PCM
      if (mt.subtype == MEDIASUBTYPE_IEEE_FLOAT || wfe->wFormatTag == WAVE_FORMAT_PCM)
        bPCM = true;
      else if (wfe->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE *wfex = (WAVEFORMATEXTENSIBLE *)wfe;
        if (wfex->SubFormat == MEDIASUBTYPE_IEEE_FLOAT || wfex->SubFormat == MEDIASUBTYPE_PCM)
          bPCM = true;
      }

      if (bPCM)
        BitstreamFallbackToPCM();
    }
  }
  return __super::CompleteConnect(dir, pReceivePin);
}

HRESULT CLAVAudio::EndOfStream()
{
  DbgLog((LOG_TRACE, 10, L"CLAVAudio::EndOfStream()"));
  CAutoLock cAutoLock(&m_csReceive);

  // Flush the last data out of the parser
  ProcessBuffer(nullptr);
  ProcessBuffer(nullptr, TRUE);

  FlushOutput(TRUE);
  return __super::EndOfStream();
}

HRESULT CLAVAudio::PerformFlush()
{
  CAutoLock cAutoLock(&m_csReceive);

  m_buff.Clear();
  FlushOutput(FALSE);
  FlushDecoder();

  m_bsOutput.SetSize(0);

  m_rtStart = 0;
  m_bQueueResync = TRUE;
  m_bNeedSyncpoint = (m_raData.deint_id != 0);
  m_SuppressLayout = 0;
  m_bMPEGAudioResync = (m_pInput->CurrentMediaType().subtype == MEDIASUBTYPE_MPEG1AudioPayload);

  memset(&m_TrueHDMATState, 0, sizeof(m_TrueHDMATState));
  m_rtBitstreamCache = AV_NOPTS_VALUE;

  return S_OK;
}

HRESULT CLAVAudio::BeginFlush()
{
  DbgLog((LOG_TRACE, 10, L"CLAVAudio::BeginFlush()"));
  m_bFlushing = TRUE;
  return __super::BeginFlush();
}

HRESULT CLAVAudio::EndFlush()
{
  DbgLog((LOG_TRACE, 10, L"CLAVAudio::EndFlush()"));
  CAutoLock cAutoLock(&m_csReceive);

  if (m_bDVDPlayback)
    PerformFlush();

  HRESULT hr = __super::EndFlush();
  m_bFlushing = FALSE;
  return hr;
}

HRESULT CLAVAudio::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
  DbgLog((LOG_TRACE, 10, L"CLAVAudio::NewSegment() tStart: %I64d, tStop: %I64d, dRate: %.2f", tStart, tStop, dRate));
  CAutoLock cAutoLock(&m_csReceive);

  PerformFlush();

  if (dRate > 0.0)
    m_dRate = dRate;
  else
    m_dRate = 1.0;
  return __super::NewSegment(tStart, tStop, dRate);
}

HRESULT CLAVAudio::FlushDecoder()
{
  if (m_bJustFlushed)
    return S_OK;

  if(m_pParser) {
    av_parser_close(m_pParser);
    m_pParser = av_parser_init(m_nCodecId);
    m_bUpdateTimeCache = TRUE;
  }

  if (m_pAVCtx && avcodec_is_open(m_pAVCtx)) {
    avcodec_flush_buffers(m_pAVCtx);
  }

  FlushDTSDecoder();

  m_bJustFlushed = TRUE;

  return S_OK;
}

HRESULT CLAVAudio::Receive(IMediaSample *pIn)
{
  CAutoLock cAutoLock(&m_csReceive);

  HRESULT hr;

  AM_SAMPLE2_PROPERTIES const *pProps = m_pInput->SampleProps();
  if(pProps->dwStreamId != AM_STREAM_MEDIA) {
    return m_pOutput->Deliver(pIn);
  }

  AM_MEDIA_TYPE *pmt;
  if(SUCCEEDED(pIn->GetMediaType(&pmt)) && pmt) {
    DbgLog((LOG_TRACE, 10, L"::Receive(): Input sample contained media type, dynamic format change..."));
    CMediaType mt(*pmt);
    m_pInput->SetMediaType(&mt);
    DeleteMediaType(pmt);
    pmt = nullptr;
    m_buff.Clear();

    m_bQueueResync = TRUE;
  }

  if (m_bBitStreamingSettingsChanged) {
    m_bBitStreamingSettingsChanged = FALSE;
    UpdateBitstreamContext();
  }

  if (!m_pAVCtx) {
    return E_FAIL;
  }

  BYTE *pDataIn = nullptr;
  if(FAILED(hr = pIn->GetPointer(&pDataIn))) {
    return hr;
  }

  long len = pIn->GetActualDataLength();
  if (len < 0) {
    DbgLog((LOG_ERROR, 10, L"Invalid data length, aborting"));
    return E_FAIL;
  }
  else if (len == 0) {
    return S_OK;
  }

  (static_cast<CDeCSSTransformInputPin*>(m_pInput))->StripPacket(pDataIn, len);

  REFERENCE_TIME rtStart = _I64_MIN, rtStop = _I64_MIN;
  hr = pIn->GetTime(&rtStart, &rtStop);

  if((pIn->IsDiscontinuity() == S_OK || (m_bNeedSyncpoint && pIn->IsSyncPoint() == S_OK))) {
    DbgLog((LOG_ERROR, 10, L"::Receive(): Discontinuity, flushing decoder.."));
    m_buff.Clear();
    FlushOutput(FALSE);
    FlushDecoder();
    m_bQueueResync = TRUE;
    m_bDiscontinuity = TRUE;
    if(FAILED(hr)) {
      DbgLog((LOG_ERROR, 10, L" -> Discontinuity without timestamp"));
    }

    if (m_bNeedSyncpoint && pIn->IsSyncPoint() == S_OK) {
      DbgLog((LOG_TRACE, 10, L"::Receive(): Got SyncPoint, resuming decoding...."));
      m_bNeedSyncpoint = FALSE;
    }
  }

  if(m_bQueueResync && SUCCEEDED(hr)) {
    DbgLog((LOG_TRACE, 10, L"Resync Request; old: %I64d; new: %I64d; buffer: %d", m_rtStart, rtStart, m_buff.GetCount()));
    FlushOutput();
    if (m_rtStart != AV_NOPTS_VALUE && rtStart != m_rtStart)
      m_bDiscontinuity = TRUE;

    m_rtStart = rtStart;
    m_rtStartInputCache =  AV_NOPTS_VALUE;
    m_rtBitstreamCache = AV_NOPTS_VALUE;
    m_dStartOffset = 0.0;
    m_bQueueResync = FALSE;
    m_bResyncTimestamp = TRUE;
  }

  m_bJustFlushed = FALSE;

  m_rtStartInput = SUCCEEDED(hr) ? rtStart : AV_NOPTS_VALUE;
  m_rtStopInput = (hr == S_OK) ? rtStop : AV_NOPTS_VALUE;

  DWORD bufflen = m_buff.GetCount();

  // Hack to re-create the BD LPCM header because in the MPC-HC format its stripped off.
  CMediaType inMt(m_pInput->CurrentMediaType());
  if (inMt.subtype == MEDIASUBTYPE_HDMV_LPCM_AUDIO && inMt.formattype == FORMAT_WaveFormatEx) {
    m_buff.SetSize(bufflen + 4);
    BYTE *buf = m_buff.Ptr() + bufflen;
    CreateBDLPCMHeader(buf, (WAVEFORMATEX_HDMV_LPCM *)inMt.pbFormat);
    bufflen = m_buff.GetCount();
  } else if (inMt.subtype == MEDIASUBTYPE_DVD_LPCM_AUDIO && inMt.formattype == FORMAT_WaveFormatEx) {
    m_buff.SetSize(bufflen + 3);
    BYTE *buf = m_buff.Ptr() + bufflen;
    CreateDVDLPCMHeader(buf, (WAVEFORMATEX *)inMt.pbFormat);
    bufflen = m_buff.GetCount();
  }

  // Ensure the size of the buffer doesn't overflow (its used as signed int in various places)
  if (bufflen > (INT_MAX - (DWORD)len)) {
    DbgLog((LOG_TRACE, 10, L"Too much audio buffered, aborting"));
    m_buff.Clear();
    m_bQueueResync = TRUE;
    return E_FAIL;
  }

  m_buff.Allocate(bufflen + len + AV_INPUT_BUFFER_PADDING_SIZE);
  m_buff.Append(pDataIn, len);

  hr = ProcessBuffer(pIn);

  if (FAILED(hr))
    return hr;

  return S_OK;
}

#define SAME_HEADER_MASK \
   (0xffe00000 | (3 << 17) | (3 << 10) | (3 << 19))

static int check_mpegaudio_header(uint8_t *buf, uint32_t *retheader)
{
  MPADecodeHeader sd;
  uint32_t header = AV_RB32(buf);

  if (avpriv_mpegaudio_decode_header(&sd, header) != 0)
    return -1;

  if (retheader)
    *retheader = header;

  return sd.frame_size;
}

HRESULT CLAVAudio::ResyncMPEGAudio()
{
  uint8_t *buf = m_buff.Ptr();
  int size = m_buff.GetCount();

  for (int i = 0; i < (size - 3); i++)
  {
    uint32_t header, header2;
    int frame_size = check_mpegaudio_header(buf + i, &header);
    if (frame_size > 0 && (i + frame_size + 4) < size) {
      int ret = check_mpegaudio_header(buf + i + frame_size, &header2);
      if (ret >= 0 && (header & SAME_HEADER_MASK) == (header2 & SAME_HEADER_MASK))
      {
        if (i > 0) {
          DbgLog((LOG_TRACE, 10, L"CLAVAudio::ResyncMPEGAudio(): Skipping %d bytes of junk", i));
          m_buff.Consume(i);
        }
        return S_OK;
      }
    }
  }

  if (size > 64 * 1024) {
    DbgLog((LOG_TRACE, 10, L"CLAVAudio::ResyncMPEGAudio(): No matching headers found in 64kb of data, aborting search"));
    return S_OK;
  }

  return S_FALSE;
}

HRESULT CLAVAudio::ProcessBuffer(IMediaSample *pMediaSample, BOOL bEOF)
{
  HRESULT hr = S_OK, hr2 = S_OK;

  int buffer_size = m_buff.GetCount();

  BYTE *p = m_buff.Ptr();
  int consumed = 0, consumed_header = 0;

  if (!bEOF) {
    if (m_bFindDTSInPCM) {
      int i = 0, count = 0;
      uint32_t state = -1;
      for (i = 0; i < buffer_size; ++i) {
        state = (state << 8) | p[i];
        if ((state == DCA_MARKER_14B_LE && (i < buffer_size-2) && (p[i+1] & 0xF0) == 0xF0 && p[i+2] == 0x07)
          || (state == DCA_MARKER_14B_BE && (i < buffer_size-2) && p[i+1] == 0x07 && (p[i+2] & 0xF0) == 0xF0)
          || state == DCA_MARKER_RAW_LE || state == DCA_MARKER_RAW_BE) {
            count++;
        }
      }
      if (count >= 4) {
        DbgLog((LOG_TRACE, 10, L"::ProcessBuffer(): Detected %d DTS sync words in %d bytes of data, switching to DTS-in-WAV decoding", count, buffer_size));
        CMediaType mt = m_pInput->CurrentMediaType();
        ffmpeg_init(AV_CODEC_ID_DTS, mt.Format(), *mt.FormatType(), mt.FormatLength());
        m_bFindDTSInPCM = FALSE;
      }

      if (buffer_size > (16384 * (4 + count))) {
        m_bFindDTSInPCM = FALSE;
      }

      if (m_bFindDTSInPCM) {
        if (m_rtStartInputCache == AV_NOPTS_VALUE)
          m_rtStartInputCache = m_rtStartInput;
        return S_FALSE;
      }

      m_rtStartInput = m_rtStartInputCache;
      m_rtStartInputCache = AV_NOPTS_VALUE;
    }

    if (m_pInput->CurrentMediaType().subtype == MEDIASUBTYPE_DOLBY_AC3_SPDIF) {
      if (buffer_size < BURST_HEADER_SIZE)
        return S_FALSE;
      uint16_t word1 = AV_RL16(p);
      uint16_t word2 = AV_RL16(p+2);
      if (word1 == SYNCWORD1 && word2 == SYNCWORD2) {
        uint16_t type = AV_RL16(p+4);
        int spdif_buffer_size = AV_RL16(p+6) >> 3;

        p += BURST_HEADER_SIZE;

        if (spdif_buffer_size+BURST_HEADER_SIZE > buffer_size) {
          DbgLog((LOG_ERROR, 10, L"::ProcessBuffer(): SPDIF sample is too small (%d required, %d present)", spdif_buffer_size+BURST_HEADER_SIZE, buffer_size));
          m_buff.Clear();
          m_bQueueResync = TRUE;
          return S_FALSE;
        }
        buffer_size = spdif_buffer_size;

        // SPDIF is apparently big-endian coded
        lav_spdif_bswap_buf16((uint16_t *)p, (uint16_t *)p, buffer_size >> 1);

        // adjust buffer size to strip off spdif padding
        consumed_header = BURST_HEADER_SIZE;
        m_buff.SetSize(buffer_size + BURST_HEADER_SIZE);
      }
    }

    if (m_bMPEGAudioResync)
    {
      if (ResyncMPEGAudio() != S_OK)
        return S_FALSE;

      m_bMPEGAudioResync = FALSE;
      return ProcessBuffer(pMediaSample, FALSE);
    }
  } else {
    // In DTSinPCm mode, make sure the timestamps are correct, and process the remaining buffer
    if (m_bFindDTSInPCM)
    {
      m_rtStartInput = m_rtStartInputCache;
      m_rtStartInputCache = AV_NOPTS_VALUE;
    }
    // Otherwise, performa proper flush and stop working
    else
    {
      p = nullptr;
      buffer_size = -1;
    }
  }

  // If a bitstreaming context exists, we should bitstream
  if (m_avBSContext) {
    hr2 = Bitstream(p, buffer_size, consumed, &hr);
    if (FAILED(hr2)) {
      DbgLog((LOG_TRACE, 10, L"Invalid sample when bitstreaming!"));
      m_buff.Clear();
      m_bQueueResync = TRUE;
      return S_FALSE;
    } else if (FAILED(hr)) {
      DbgLog((LOG_TRACE, 10, L"::Bitstream indicates delivery failed"));
    } else if (hr2 == S_FALSE) {
      //DbgLog((LOG_TRACE, 10, L"::Bitstream returned S_FALSE"));
      hr = S_FALSE;
    }
  } else {
    // Decoding
    // Consume the buffer data
    if (m_pDTSDecoderContext)
      hr2 = DecodeDTS(p, buffer_size, consumed, &hr);
    else
      hr2 = Decode(p, buffer_size, consumed, &hr, pMediaSample);
    // FAILED - throw away the data
    if (FAILED(hr2)) {
      DbgLog((LOG_TRACE, 10, L"Dropped invalid sample in ProcessBuffer"));
      m_buff.Clear();
      m_bQueueResync = TRUE;
      return S_FALSE;
    } else if (FAILED(hr)) {
      DbgLog((LOG_TRACE, 10, L"::Decode indicates delivery failed"));
    } else if (hr2 == S_FALSE) {
      //DbgLog((LOG_TRACE, 10, L"::Decode returned S_FALSE"));
      hr = S_FALSE;
    }
  }

  if (bEOF || consumed <= 0) {
    return hr;
  }

  // Determine actual buffer consumption
  consumed = consumed_header + min(consumed, buffer_size);

  // Remove the consumed data from the buffer
  m_buff.Consume(consumed);

  return hr;
}

static DWORD get_lav_channel_layout(uint64_t layout)
{
  if (layout > UINT32_MAX) {
    if (layout & AV_CH_WIDE_LEFT)
      layout = (layout & ~AV_CH_WIDE_LEFT) | AV_CH_FRONT_LEFT_OF_CENTER;
    if (layout & AV_CH_WIDE_RIGHT)
      layout = (layout & ~AV_CH_WIDE_RIGHT) | AV_CH_FRONT_RIGHT_OF_CENTER;

    if (layout & AV_CH_SURROUND_DIRECT_LEFT)
      layout = (layout & ~AV_CH_SURROUND_DIRECT_LEFT) | AV_CH_SIDE_LEFT;
    if (layout & AV_CH_SURROUND_DIRECT_RIGHT)
      layout = (layout & ~AV_CH_SURROUND_DIRECT_RIGHT) | AV_CH_SIDE_RIGHT;
  }

  return (DWORD)layout;
}

HRESULT CLAVAudio::Decode(const BYTE * pDataBuffer, int buffsize, int &consumed, HRESULT *hrDeliver, IMediaSample *pMediaSample)
{
  int got_frame	= 0;
  BYTE *tmpProcessBuf = nullptr;
  HRESULT hr = S_FALSE;

  BOOL bFlush = (pDataBuffer == nullptr);

  AVPacket avpkt;
  av_init_packet(&avpkt);

  BufferDetails out;
  const MediaSideDataFFMpeg *pFFSideData = nullptr;

  if (!bFlush && (m_raData.deint_id == MKBETAG('g', 'e', 'n', 'r') || m_raData.deint_id == MKBETAG('s', 'i', 'p', 'r'))) {
    int w = m_raData.audio_framesize;
    int h = m_raData.sub_packet_h;
    int sps = m_raData.sub_packet_size;
    int len = w * h;
    if (buffsize >= len) {
      tmpProcessBuf = (BYTE *)av_mallocz(len + AV_INPUT_BUFFER_PADDING_SIZE);

      // "genr" deinterleaving is used for COOK and ATRAC
      if (m_raData.deint_id == MKBETAG('g', 'e', 'n', 'r')) {
        const BYTE *srcBuf = pDataBuffer;
        for(int y = 0; y < h; y++) {
          for(int x = 0, w2 = w / sps; x < w2; x++) {
            memcpy(tmpProcessBuf + sps*(h*x+((h+1)/2)*(y&1)+(y>>1)), srcBuf, sps);
            srcBuf += sps;
          }
        }
      // "sipr" deinterleaving is used for ... SIPR
      } else if (m_raData.deint_id == MKBETAG('s', 'i', 'p', 'r')) {
        memcpy(tmpProcessBuf, pDataBuffer, len);
        ff_rm_reorder_sipr_data(tmpProcessBuf, h, w);
      }

      pDataBuffer = tmpProcessBuf;
      buffsize = len;

      m_rtStartInput = m_rtStartInputCache;
      m_rtStartInputCache = AV_NOPTS_VALUE;
    } else {
      if (m_rtStartInputCache == AV_NOPTS_VALUE)
        m_rtStartInputCache = m_rtStartInput;
      return S_FALSE;
    }
  }
#ifdef DEBUG
  else if (!bFlush && m_raData.deint_id) {
    const char *deint = (const char *)&m_raData.deint_id;
    DbgLog((LOG_TRACE, 10, L"::Decode(): Unsupported deinterleaving algorithm '%c%c%c%c'", deint[3], deint[2], deint[1], deint[0]));
  }
#endif

  if (pMediaSample) {
    IMediaSideData *pSideData = nullptr;
    if (SUCCEEDED(pMediaSample->QueryInterface(&pSideData))) {
      size_t nFFSideDataSize = 0;
      if (FAILED(pSideData->GetSideData(IID_MediaSideDataFFMpeg, (const BYTE **)&pFFSideData, &nFFSideDataSize)) || nFFSideDataSize != sizeof(MediaSideDataFFMpeg)) {
        pFFSideData = nullptr;
      }

      SafeRelease(&pSideData);
    }
  }

  consumed = 0;
  while (buffsize > 0 || bFlush) {
    got_frame = 0;
    if (bFlush) buffsize = 0;

    if (m_pParser) {
      BYTE *pOut = nullptr;
      int pOut_size = 0;
      int used_bytes = av_parser_parse2(m_pParser, m_pAVCtx, &pOut, &pOut_size, pDataBuffer, buffsize, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
      if (used_bytes < 0) {
        DbgLog((LOG_TRACE, 50, L"::Decode() - audio parsing failed (ret: %d)", -used_bytes));
        goto fail;
      } else if(used_bytes == 0 && pOut_size == 0) {
        DbgLog((LOG_TRACE, 50, L"::Decode() - could not process buffer, starving?"));
        break;
      }

      // Timestamp cache to compensate for one frame delay the parser might introduce, in case the frames were already perfectly sliced apart
      // If we used more (or equal) bytes then was output again, we encountered a new frame, update timestamps
      if (used_bytes >= pOut_size && m_bUpdateTimeCache) {
        m_rtStartInputCache = m_rtStartInput;
        m_rtStopInputCache = m_rtStopInput;
        m_rtStartInput = m_rtStopInput = AV_NOPTS_VALUE;
        m_bUpdateTimeCache = FALSE;
      }

      if (!bFlush && used_bytes > 0) {
        buffsize -= used_bytes;
        pDataBuffer += used_bytes;
        consumed += used_bytes;
      }

      if (pOut_size > 0) {
        avpkt.data = pOut;
        avpkt.size = pOut_size;
        avpkt.dts  = m_rtStartInputCache;

        CopyMediaSideDataFF(&avpkt, &pFFSideData);

        int ret2 = avcodec_decode_audio4(m_pAVCtx, m_pFrame, &got_frame, &avpkt);
        if (ret2 < 0) {
          DbgLog((LOG_TRACE, 50, L"::Decode() - decoding failed despite successfull parsing"));
          m_bQueueResync = TRUE;
          av_packet_unref(&avpkt);
          continue;
        }

        // Send current input time to the delivery function
        out.rtStart = m_pFrame->pkt_dts;
        m_rtStartInputCache = AV_NOPTS_VALUE;
        m_bUpdateTimeCache = TRUE;

      } else {
        continue;
      }
    } else if(bFlush) {
      hr = S_FALSE;
      break;
    } else {
      avpkt.data = (uint8_t *)pDataBuffer;
      avpkt.size = buffsize;
      avpkt.dts  = m_rtStartInput;

      CopyMediaSideDataFF(&avpkt, &pFFSideData);

      int used_bytes = avcodec_decode_audio4(m_pAVCtx, m_pFrame, &got_frame, &avpkt);

      if(used_bytes < 0) {
        av_packet_unref(&avpkt);
        goto fail;
      } else if(used_bytes == 0 && !got_frame) {
        DbgLog((LOG_TRACE, 50, L"::Decode() - could not process buffer, starving?"));
        av_packet_unref(&avpkt);
        break;
      }
      buffsize -= used_bytes;
      pDataBuffer += used_bytes;
      consumed += used_bytes;

      // Send current input time to the delivery function
      out.rtStart = m_pFrame->pkt_dts;
      m_rtStartInput = AV_NOPTS_VALUE;
    }

    av_packet_unref(&avpkt);

    // Channel re-mapping and sample format conversion
    if (got_frame) {
      ASSERT(m_pFrame->nb_samples > 0);
      out.wChannels = m_pAVCtx->channels;
      out.dwSamplesPerSec = m_pAVCtx->sample_rate;
      if (m_pAVCtx->channel_layout)
        out.dwChannelMask = get_lav_channel_layout(m_pAVCtx->channel_layout);
      else
        out.dwChannelMask = get_channel_mask(out.wChannels);

      out.nSamples = m_pFrame->nb_samples;
      DWORD dwPCMSize = out.nSamples * out.wChannels * av_get_bytes_per_sample(m_pAVCtx->sample_fmt);
      DWORD dwPCMSizeAligned = FFALIGN(out.nSamples, 32) * out.wChannels * av_get_bytes_per_sample(m_pAVCtx->sample_fmt);

      if (m_pFrame->decode_error_flags & FF_DECODE_ERROR_INVALID_BITSTREAM) {
        if (m_DecodeLayout != out.dwChannelMask) {
          DbgLog((LOG_TRACE, 50, L"::Decode() - Corrupted audio frame with channel layout change, dropping."));
          av_frame_unref(m_pFrame);
          continue;
        }
      }

      switch (m_pAVCtx->sample_fmt) {
      case AV_SAMPLE_FMT_U8:
        out.bBuffer->Allocate(dwPCMSizeAligned);
        out.bBuffer->Append(m_pFrame->data[0], dwPCMSize);
        out.sfFormat = SampleFormat_U8;
        break;
      case AV_SAMPLE_FMT_S16:
        out.bBuffer->Allocate(dwPCMSizeAligned);
        out.bBuffer->Append(m_pFrame->data[0], dwPCMSize);
        out.sfFormat = SampleFormat_16;
        break;
      case AV_SAMPLE_FMT_S32:
        out.bBuffer->Allocate(dwPCMSizeAligned);
        out.bBuffer->Append(m_pFrame->data[0], dwPCMSize);
        out.sfFormat = SampleFormat_32;
        out.wBitsPerSample = m_pAVCtx->bits_per_raw_sample;
        break;
      case AV_SAMPLE_FMT_FLT:
        out.bBuffer->Allocate(dwPCMSizeAligned);
        out.bBuffer->Append(m_pFrame->data[0], dwPCMSize);
        out.sfFormat = SampleFormat_FP32;
        break;
      case AV_SAMPLE_FMT_DBL:
        {
          out.bBuffer->Allocate(dwPCMSizeAligned / 2);
          out.bBuffer->SetSize(dwPCMSize / 2);
          float *pDataOut = (float *)(out.bBuffer->Ptr());

          for (size_t i = 0; i < out.nSamples; ++i) {
            for(int ch = 0; ch < out.wChannels; ++ch) {
              *pDataOut = (float)((double *)m_pFrame->data[0]) [ch+i*m_pAVCtx->channels];
              pDataOut++;
            }
          }
        }
        out.sfFormat = SampleFormat_FP32;
        break;
      // Planar Formats
      case AV_SAMPLE_FMT_U8P:
        {
          out.bBuffer->Allocate(dwPCMSizeAligned);
          out.bBuffer->SetSize(dwPCMSize);
          uint8_t *pOut = (uint8_t *)(out.bBuffer->Ptr());

          for (size_t i = 0; i < out.nSamples; ++i) {
            for(int ch = 0; ch < out.wChannels; ++ch) {
              *pOut++ = ((uint8_t *)m_pFrame->extended_data[ch])[i];
            }
          }
        }
        out.sfFormat = SampleFormat_U8;
        break;
      case AV_SAMPLE_FMT_S16P:
        {
          out.bBuffer->Allocate(dwPCMSizeAligned);
          out.bBuffer->SetSize(dwPCMSize);
          int16_t *pOut = (int16_t *)(out.bBuffer->Ptr());

          for (size_t i = 0; i < out.nSamples; ++i) {
            for(int ch = 0; ch < out.wChannels; ++ch) {
              *pOut++ = ((int16_t *)m_pFrame->extended_data[ch])[i];
            }
          }
        }
        out.sfFormat = SampleFormat_16;
        break;
      case AV_SAMPLE_FMT_S32P:
        {
          out.bBuffer->Allocate(dwPCMSizeAligned);
          out.bBuffer->SetSize(dwPCMSize);
          int32_t *pOut = (int32_t *)(out.bBuffer->Ptr());

          for (size_t i = 0; i < out.nSamples; ++i) {
            for(int ch = 0; ch < out.wChannels; ++ch) {
              *pOut++ = ((int32_t *)m_pFrame->extended_data[ch])[i];
            }
          }
        }
        out.sfFormat = SampleFormat_32;
        out.wBitsPerSample = m_pAVCtx->bits_per_raw_sample;
        break;
      case AV_SAMPLE_FMT_FLTP:
        {
          out.bBuffer->Allocate(dwPCMSizeAligned);
          out.bBuffer->SetSize(dwPCMSize);
          float *pOut = (float *)(out.bBuffer->Ptr());

          for (size_t i = 0; i < out.nSamples; ++i) {
            for(int ch = 0; ch < out.wChannels; ++ch) {
              *pOut++ = ((float *)m_pFrame->extended_data[ch])[i];
            }
          }
        }
        out.sfFormat = SampleFormat_FP32;
        break;
      case AV_SAMPLE_FMT_DBLP:
        {
          out.bBuffer->Allocate(dwPCMSizeAligned / 2);
          out.bBuffer->SetSize(dwPCMSize / 2);
          float *pOut = (float *)(out.bBuffer->Ptr());

          for (size_t i = 0; i < out.nSamples; ++i) {
            for(int ch = 0; ch < out.wChannels; ++ch) {
              *pOut++ = (float)((double *)m_pFrame->extended_data[ch])[i];
            }
          }
        }
        out.sfFormat = SampleFormat_FP32;
        break;
      default:
        assert(FALSE);
        break;
      }
      av_frame_unref(m_pFrame);
      hr = S_OK;

      m_DecodeFormat = out.sfFormat == SampleFormat_32 && out.wBitsPerSample > 0 && out.wBitsPerSample <= 24 ? (out.wBitsPerSample <= 16 ? SampleFormat_16 : SampleFormat_24) : out.sfFormat;
      m_DecodeLayout = out.dwChannelMask;

      if (SUCCEEDED(PostProcess(&out))) {
        *hrDeliver = QueueOutput(out);
        if (FAILED(*hrDeliver)) {
          hr = S_FALSE;
          break;
        }
      }
    }
  }

  av_free(tmpProcessBuf);
  return hr;
fail:
  av_free(tmpProcessBuf);
  return E_FAIL;
}

HRESULT CLAVAudio::GetDeliveryBuffer(IMediaSample** pSample, BYTE** pData)
{
  HRESULT hr;

  *pData = nullptr;
  if(FAILED(hr = m_pOutput->GetDeliveryBuffer(pSample, nullptr, nullptr, 0))
    || FAILED(hr = (*pSample)->GetPointer(pData))) {
      return hr;
  }

  AM_MEDIA_TYPE* pmt = nullptr;
  if(SUCCEEDED((*pSample)->GetMediaType(&pmt)) && pmt) {
    CMediaType mt = *pmt;
    m_pOutput->SetMediaType(&mt);
    DeleteMediaType(pmt);
    pmt = nullptr;
  }

  return S_OK;
}

HRESULT CLAVAudio::QueueOutput(BufferDetails &buffer)
{
  HRESULT hr = S_OK;
  if (m_OutputQueue.wChannels != buffer.wChannels || m_OutputQueue.sfFormat != buffer.sfFormat || m_OutputQueue.dwSamplesPerSec != buffer.dwSamplesPerSec || m_OutputQueue.dwChannelMask != buffer.dwChannelMask || m_OutputQueue.wBitsPerSample != buffer.wBitsPerSample) {
    if (m_OutputQueue.nSamples > 0)
      FlushOutput();

    m_OutputQueue.sfFormat = buffer.sfFormat;
    m_OutputQueue.wChannels = buffer.wChannels;
    m_OutputQueue.dwChannelMask = buffer.dwChannelMask;
    m_OutputQueue.dwSamplesPerSec = buffer.dwSamplesPerSec;
    m_OutputQueue.wBitsPerSample = buffer.wBitsPerSample;
  }

  if (m_OutputQueue.nSamples == 0)
    m_OutputQueue.rtStart = buffer.rtStart;
  else if (m_OutputQueue.rtStart == AV_NOPTS_VALUE && buffer.rtStart != AV_NOPTS_VALUE)
    m_OutputQueue.rtStart = buffer.rtStart - (REFERENCE_TIME)((double)m_OutputQueue.nSamples / m_OutputQueue.dwSamplesPerSec * 10000000.0);

  // Try to retain the buffer, if possible
  if (m_OutputQueue.nSamples == 0) {
    FFSWAP(GrowableArray<BYTE>*, m_OutputQueue.bBuffer, buffer.bBuffer);
  } else {
    m_OutputQueue.bBuffer->Append(buffer.bBuffer);
  }
  m_OutputQueue.nSamples += buffer.nSamples;

  buffer.bBuffer->SetSize(0);
  buffer.nSamples = 0;

  // Length of the current sample
  double dDuration = (double)m_OutputQueue.nSamples / m_OutputQueue.dwSamplesPerSec * 10000000.0;
  double dOffset = fmod(dDuration, 1.0);

  // Don't exceed the buffer
  if (dDuration >= PCM_BUFFER_MAX_DURATION || (dDuration >= PCM_BUFFER_MIN_DURATION && dOffset <= FLT_EPSILON)) {
    hr = FlushOutput();
  }

  return hr;
}

HRESULT CLAVAudio::FlushOutput(BOOL bDeliver)
{
  CAutoLock cAutoLock(&m_csReceive);

  HRESULT hr = S_OK;
  if (bDeliver && m_OutputQueue.nSamples > 0)
    hr = Deliver(m_OutputQueue);

  // Clear Queue
  m_OutputQueue.nSamples = 0;
  m_OutputQueue.bBuffer->SetSize(0);
  m_OutputQueue.rtStart = AV_NOPTS_VALUE;

  return hr;
}

HRESULT CLAVAudio::Deliver(BufferDetails &buffer)
{
  HRESULT hr = S_OK;

  if (m_bFlushing)
    return S_FALSE;

  CMediaType mt = CreateMediaType(buffer.sfFormat, buffer.dwSamplesPerSec, buffer.wChannels, buffer.dwChannelMask, buffer.wBitsPerSample);
  WAVEFORMATEX* wfe = (WAVEFORMATEX*)mt.Format();

  long cbBuffer = buffer.nSamples * wfe->nBlockAlign;
  if(FAILED(hr = ReconnectOutput(cbBuffer, mt))) {
    return hr;
  }

  IMediaSample *pOut;
  BYTE *pDataOut = nullptr;
  if(FAILED(GetDeliveryBuffer(&pOut, &pDataOut))) {
    return E_FAIL;
  }

  if (m_bResyncTimestamp && buffer.rtStart != AV_NOPTS_VALUE) {
    m_rtStart = buffer.rtStart;
    m_bResyncTimestamp = FALSE;
  }

  // Length of the current sample
  double dDuration = (double)buffer.nSamples / buffer.dwSamplesPerSec * DBL_SECOND_MULT / m_dRate;
  m_dStartOffset += fmod(dDuration, 1.0);

  // Delivery Timestamps
  REFERENCE_TIME rtStart = m_rtStart, rtStop = m_rtStart + (REFERENCE_TIME)(dDuration + 0.5);

  // Compute next start time
  m_rtStart += (REFERENCE_TIME)dDuration;
  // If the offset reaches one (100ns), add it to the next frame
  if (m_dStartOffset > 0.5) {
    m_rtStart++;
    m_dStartOffset -= 1.0;
  }

  if (buffer.rtStart != AV_NOPTS_VALUE) {
    REFERENCE_TIME rtJitter = rtStart - buffer.rtStart;
    m_faJitter.Sample(rtJitter);
    REFERENCE_TIME rtJitterMin = m_faJitter.AbsMinimum();
    if (m_settings.AutoAVSync && abs(rtJitterMin) > m_JitterLimit && m_bHasVideo) {
      DbgLog((LOG_TRACE, 10, L"::Deliver(): corrected A/V sync by %I64d", rtJitterMin));
      m_rtStart -= rtJitterMin;
      rtStart -= rtJitterMin;
      m_faJitter.OffsetValues(-rtJitterMin);
      m_bDiscontinuity = TRUE;
    }

#ifdef DEBUG
    if (m_faJitter.CurrentSample() == 0) {
      DbgLog((LOG_CUSTOM2, 20, L"Jitter Stats: min: %I64d - max: %I64d - avg: %I64d", rtJitterMin, m_faJitter.AbsMaximum(), m_faJitter.Average()));
    }
#endif
    DbgLog((LOG_CUSTOM5, 20, L"PCM Delivery, rtStart(calc): %I64d, rtStart(input): %I64d, sample duration: %I64d, diff: %I64d", rtStart, buffer.rtStart, rtStop-rtStart, rtJitter));
  } else {
    DbgLog((LOG_CUSTOM5, 20, L"PCM Delivery, rtStart(calc): %I64d, rtStart(input): N/A, sample duration: %I64d, diff: N/A", rtStart, rtStop-rtStart));
  }

  if(rtStart < 0) {
    goto done;
  }

  if(hr == S_OK) {
  retry_qa:
    hr = m_pOutput->GetConnected()->QueryAccept(&mt);
    DbgLog((LOG_TRACE, 1, L"Sending new Media Type (QueryAccept: %0#.8x)", hr));
    if (hr != S_OK) {
      if (buffer.sfFormat != SampleFormat_16) {
        mt = CreateMediaType(SampleFormat_16, buffer.dwSamplesPerSec, buffer.wChannels, buffer.dwChannelMask, 16);
        hr = m_pOutput->GetConnected()->QueryAccept(&mt);
        if (hr == S_OK) {
          DbgLog((LOG_TRACE, 1, L"-> 16-bit fallback type accepted"));
          m_FallbackFormat = SampleFormat_16;
          PerformAVRProcessing(&buffer);
        }
      }
      // Try 5.1 back fallback format
      if (buffer.dwChannelMask == AV_CH_LAYOUT_5POINT1) {
        DbgLog((LOG_TRACE, 1, L"-> Trying to fallback to 5.1 back"));
        buffer.dwChannelMask = AV_CH_LAYOUT_5POINT1_BACK;
        mt = CreateMediaType(buffer.sfFormat, buffer.dwSamplesPerSec, buffer.wChannels, buffer.dwChannelMask, buffer.wBitsPerSample);
        goto retry_qa;
      }
      // If a 16-bit fallback isn't enough, try to retain current channel layout as well
      if (hr != S_OK) {
        WAVEFORMATEX* wfeCurrent = (WAVEFORMATEX*)m_pOutput->CurrentMediaType().Format();
        WORD wChannels = wfeCurrent->nChannels;
        DWORD dwChannelMask = 0;
        if (wfeCurrent->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
          WAVEFORMATEXTENSIBLE *wfex = (WAVEFORMATEXTENSIBLE *)wfeCurrent;
          dwChannelMask = wfex->dwChannelMask;
        } else {
          dwChannelMask = wChannels == 2 ? (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT) : SPEAKER_FRONT_CENTER;
        }
        if (buffer.wChannels != wfeCurrent->nChannels || buffer.dwChannelMask != dwChannelMask) {
          mt = CreateMediaType(buffer.sfFormat, buffer.dwSamplesPerSec, wChannels, dwChannelMask, buffer.wBitsPerSample);
          hr = m_pOutput->GetConnected()->QueryAccept(&mt);
          if (hr != S_OK) {
            mt = CreateMediaType(SampleFormat_16, buffer.dwSamplesPerSec, wChannels, dwChannelMask, 16);
            hr = m_pOutput->GetConnected()->QueryAccept(&mt);
            if (hr == S_OK)
              m_FallbackFormat = SampleFormat_16;
          }
          if (hr == S_OK) {
            DbgLog((LOG_TRACE, 1, L"-> Override Mixing to layout 0x%x", dwChannelMask));
            m_dwOverrideMixer = dwChannelMask;
            m_bMixingSettingsChanged = TRUE;
            // Mix to the new layout
            PerformAVRProcessing(&buffer);
          }
        }
      }
    }
    m_pOutput->SetMediaType(&mt);
    pOut->SetMediaType(&mt);
  }

  if(m_settings.AudioDelayEnabled) {
    REFERENCE_TIME rtDelay = (REFERENCE_TIME)((m_settings.AudioDelay * 10000i64) / m_dRate);
    rtStart += rtDelay;
    rtStop += rtDelay;
  }

  pOut->SetTime(&rtStart, &rtStop);
  pOut->SetMediaTime(nullptr, nullptr);

  pOut->SetPreroll(FALSE);
  pOut->SetDiscontinuity(m_bDiscontinuity);
  m_bDiscontinuity = FALSE;
  pOut->SetSyncPoint(TRUE);

  pOut->SetActualDataLength(buffer.bBuffer->GetCount());

  memcpy(pDataOut, buffer.bBuffer->Ptr(), buffer.bBuffer->GetCount());

  hr = m_pOutput->Deliver(pOut);
  if (FAILED(hr)) {
    DbgLog((LOG_ERROR, 10, L"::Deliver failed with code: %0#.8x", hr));
  }
done:
  SafeRelease(&pOut);
  return hr;
}

HRESULT CLAVAudio::BreakConnect(PIN_DIRECTION dir)
{
  if(dir == PINDIR_INPUT) {
    ffmpeg_shutdown();
    m_bHasVideo = -1;
  }
  return __super::BreakConnect(dir);
}
