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
#include "LAVAudio.h"
#include "PostProcessor.h"

#include <MMReg.h>
#include <assert.h>

#include "moreuuids.h"
#include "DShowUtil.h"

#include "AudioSettingsProp.h"

#include "registry.h"

#include "DeCSS/DeCSSInputPin.h"

extern "C" {
#include "libavcodec/dca.h"
#include "libavutil/intreadwrite.h"
#include "libavformat/spdif.h"
};

extern HINSTANCE g_hInst;

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

// Constructor
CLAVAudio::CLAVAudio(LPUNKNOWN pUnk, HRESULT* phr)
  : CTransformFilter(NAME("lavc audio decoder"), 0, __uuidof(CLAVAudio))
  , m_nCodecId(CODEC_ID_NONE)
  , m_pAVCodec(NULL)
  , m_pAVCtx(NULL)
  , m_pPCMData(NULL)
  , m_bDiscontinuity(FALSE)
  , m_rtStart(0)
  , m_dStartOffset(0.0)
  , m_DecodeFormat(SampleFormat_16)
  , m_pFFBuffer(NULL)
  , m_nFFBufferSize(0)
  , m_bRuntimeConfig(FALSE)
  , m_bVolumeStats(FALSE)
  , m_pParser(NULL)
  , m_bQueueResync(FALSE)
  , m_avioBitstream(NULL)
  , m_avBSContext(NULL)
  , m_bDTSHD(FALSE)
  , m_bUpdateTimeCache(TRUE)
  , m_rtStartInputCache(AV_NOPTS_VALUE)
  , m_rtStopInputCache(AV_NOPTS_VALUE)
  , m_rtStartCacheLT(AV_NOPTS_VALUE)
  , m_faJitter(100)
  , m_hDllExtraDecoder(NULL)
  , m_pDTSDecoderContext(NULL)
  , m_DecodeLayout(0)
  , m_DecodeLayoutSanified(0)
  , m_bChannelMappingRequired(FALSE)
  , m_bFindDTSInPCM(FALSE)
{
  av_register_all();
  if (av_lockmgr_addref() == 1)
    av_lockmgr_register(ff_lockmgr);

  m_pInput = new CDeCSSInputPin(TEXT("CDeCSSInputPin"), this, phr, L"Input");
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

  m_bSampleSupport[SampleFormat_U8] = TRUE;
  m_bSampleSupport[SampleFormat_16] = TRUE;
  m_bSampleSupport[SampleFormat_24] = TRUE;
  m_bSampleSupport[SampleFormat_32] = TRUE;
  m_bSampleSupport[SampleFormat_FP32] = TRUE;

  LoadSettings();

  InitBitstreaming();

#ifdef DEBUG
  DbgSetModuleLevel (LOG_ERROR, DWORD_MAX);
  DbgSetModuleLevel (LOG_TRACE, DWORD_MAX);
  //DbgSetModuleLevel (LOG_CUSTOM5, DWORD_MAX); // Extensive timing options

#if ENABLE_DEBUG_LOGFILE
  DbgSetLogFileDesktop(LAVC_AUDIO_LOG_FILE);
#endif
#endif
}

CLAVAudio::~CLAVAudio()
{
  ffmpeg_shutdown();
  av_freep(&m_pFFBuffer);

  ShutdownBitstreaming();

  if (m_hDllExtraDecoder) {
    FreeLibrary(m_hDllExtraDecoder);
    m_hDllExtraDecoder = NULL;
  }

  // If its 0, that means we're the last one using/owning the object, and can free it
  if(av_lockmgr_release() == 0)
    av_lockmgr_register(NULL);

#if defined(DEBUG) && ENABLE_DEBUG_LOGFILE
  DbgCloseLogFile();
#endif
}

HRESULT CLAVAudio::LoadDefaults()
{
  m_settings.DRCEnabled = FALSE;
  m_settings.DRCLevel   = 100;

  // Default all Codecs to enabled
  for(int i = 0; i < Codec_NB; ++i)
    m_settings.bFormats[i] = true;

  // Disable WMA codecs by default
  m_settings.bFormats[Codec_WMA2] = false;
  m_settings.bFormats[Codec_WMAPRO] = false;

  // Default bitstreaming to disabled
  memset(m_settings.bBitstream, 0, sizeof(m_settings.bBitstream));

  m_settings.DTSHDFraming         = FALSE;
  m_settings.AutoAVSync           = TRUE;
  m_settings.ExpandMono           = FALSE;
  m_settings.Expand61             = FALSE;
  m_settings.OutputStandardLayout = TRUE;
  m_settings.AllowRawSPDIF        = FALSE;

  // Default all Sample Formats to enabled
  for(int i = 0; i < SampleFormat_NB; ++i)
    m_settings.bSampleFormats[i] = true;

  m_settings.AudioDelayEnabled = FALSE;
  m_settings.AudioDelay = 0;

  return S_OK;
}

HRESULT CLAVAudio::LoadSettings()
{
  LoadDefaults();
  if (m_bRuntimeConfig)
    return S_FALSE;

  HRESULT hr;
  DWORD dwVal;
  BOOL bFlag;
  BYTE *pBuf = NULL;

  CreateRegistryKey(HKEY_CURRENT_USER, LAVC_AUDIO_REGISTRY_KEY);
  CRegistry reg = CRegistry(HKEY_CURRENT_USER, LAVC_AUDIO_REGISTRY_KEY, hr);
  // We don't check if opening succeeded, because the read functions will set their hr accordingly anyway,
  // and we need to fill the settings with defaults.
  // ReadString returns an empty string in case of failure, so thats fine!
  bFlag = reg.ReadDWORD(L"DRCEnabled", hr);
  if (SUCCEEDED(hr)) m_settings.DRCEnabled = bFlag;

  dwVal = reg.ReadDWORD(L"DRCLevel", hr);
  if (SUCCEEDED(hr)) m_settings.DRCLevel = (int)dwVal;

  pBuf = reg.ReadBinary(L"Formats", dwVal, hr);
  if (SUCCEEDED(hr)) {
    memcpy(&m_settings.bFormats, pBuf, min(dwVal, sizeof(m_settings.bFormats)));
    SAFE_CO_FREE(pBuf);
  }

  pBuf = reg.ReadBinary(L"Bitstreaming", dwVal, hr);
  if (SUCCEEDED(hr)) {
    memcpy(&m_settings.bBitstream, pBuf, min(dwVal, sizeof(m_settings.bBitstream)));
    SAFE_CO_FREE(pBuf);
  }

  bFlag = reg.ReadBOOL(L"DTSHDFraming", hr);
  if (SUCCEEDED(hr)) m_settings.DTSHDFraming = bFlag;

  bFlag = reg.ReadBOOL(L"AutoAVSync", hr);
  if (SUCCEEDED(hr)) m_settings.AutoAVSync = bFlag;

  bFlag = reg.ReadBOOL(L"ExpandMono", hr);
  if (SUCCEEDED(hr)) m_settings.ExpandMono = bFlag;

  bFlag = reg.ReadBOOL(L"Expand61", hr);
  if (SUCCEEDED(hr)) m_settings.Expand61 = bFlag;

  bFlag = reg.ReadBOOL(L"OutputStandardLayout", hr);
  if (SUCCEEDED(hr)) m_settings.OutputStandardLayout = bFlag;

  pBuf = reg.ReadBinary(L"SampleFormats", dwVal, hr);
  if (SUCCEEDED(hr)) {
    memcpy(&m_settings.bSampleFormats, pBuf, min(dwVal, sizeof(m_settings.bSampleFormats)));
    SAFE_CO_FREE(pBuf);
  }

  bFlag = reg.ReadBOOL(L"AudioDelayEnabled", hr);
  if (SUCCEEDED(hr)) m_settings.AudioDelayEnabled = bFlag;

  dwVal = reg.ReadDWORD(L"AudioDelay", hr);
  if (SUCCEEDED(hr)) m_settings.AudioDelay = (int)dwVal;

  return S_OK;
}

HRESULT CLAVAudio::SaveSettings()
{
  if (m_bRuntimeConfig)
    return S_FALSE;

  HRESULT hr;
  CRegistry reg = CRegistry(HKEY_CURRENT_USER, LAVC_AUDIO_REGISTRY_KEY, hr);
  if (SUCCEEDED(hr)) {
    reg.WriteBOOL(L"DRCEnabled", m_settings.DRCEnabled);
    reg.WriteDWORD(L"DRCLevel", m_settings.DRCLevel);
    reg.WriteBinary(L"Formats", (BYTE *)m_settings.bFormats, sizeof(m_settings.bFormats));
    reg.WriteBinary(L"Bitstreaming", (BYTE *)m_settings.bBitstream, sizeof(m_settings.bBitstream));
    reg.WriteBOOL(L"DTSHDFraming", m_settings.DTSHDFraming);
    reg.WriteBOOL(L"AutoAVSync", m_settings.AutoAVSync);
    reg.WriteBOOL(L"ExpandMono", m_settings.ExpandMono);
    reg.WriteBOOL(L"Expand61", m_settings.Expand61);
    reg.WriteBOOL(L"OutputStandardLayout", m_settings.OutputStandardLayout);
    reg.WriteBinary(L"SampleFormats", (BYTE *)m_settings.bSampleFormats, sizeof(m_settings.bSampleFormats));
    reg.WriteBOOL(L"AudioDelayEnabled", m_settings.AudioDelayEnabled);
    reg.WriteDWORD(L"AudioDelay", m_settings.AudioDelay);
  }
  return S_OK;
}

void CLAVAudio::ffmpeg_shutdown()
{
  m_pAVCodec	= NULL;
  if (m_pAVCtx) {
    avcodec_close(m_pAVCtx);
    av_freep(&m_pAVCtx->extradata);
    av_freep(&m_pAVCtx);
  }

  if (m_pParser) {
    av_parser_close(m_pParser);
    m_pParser = NULL;
  }

  av_freep(&m_pPCMData);

  FreeBitstreamContext();

  FreeDTSDecoder();

  m_nCodecId = CODEC_ID_NONE;
}

// IUnknown
STDMETHODIMP CLAVAudio::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = NULL;

  return 
    QI2(ISpecifyPropertyPages)
    QI2(ILAVAudioSettings)
    QI2(ILAVAudioStatus)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

// ISpecifyPropertyPages
STDMETHODIMP CLAVAudio::GetPages(CAUUID *pPages)
{
  CheckPointer(pPages, E_POINTER);
  pPages->cElems = 3;
  pPages->pElems = (GUID *)CoTaskMemAlloc(sizeof(GUID) * pPages->cElems);
  if (pPages->pElems == NULL) {
    return E_OUTOFMEMORY;
  }
  pPages->pElems[0] = CLSID_LAVAudioSettingsProp;
  pPages->pElems[1] = CLSID_LAVAudioFormatsProp;
  pPages->pElems[2] = CLSID_LAVAudioStatusProp;
  return S_OK;
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

  return S_OK;
}

HRESULT CLAVAudio::SetDRC(BOOL bDRCEnabled, int fDRCLevel)
{
  m_settings.DRCEnabled = bDRCEnabled;
  m_settings.DRCLevel = fDRCLevel;

  if (m_pAVCtx) {
    float fDRC = bDRCEnabled ? (float)fDRCLevel / 100.0f : 0.0f;
    m_pAVCtx->drc_scale = fDRC;
  }

  SaveSettings();
  return S_OK;
}

BOOL CLAVAudio::GetFormatConfiguration(LAVAudioCodec aCodec)
{
  if (aCodec < 0 || aCodec >= Codec_NB)
    return FALSE;
  return m_settings.bFormats[aCodec];
}

HRESULT CLAVAudio::SetFormatConfiguration(LAVAudioCodec aCodec, BOOL bEnabled)
{
  if (aCodec < 0 || aCodec >= Codec_NB)
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

  UpdateBitstreamContext();

  return S_OK;
}

STDMETHODIMP_(BOOL) CLAVAudio::GetDTSHDFraming()
{
  return m_settings.DTSHDFraming;
}

STDMETHODIMP CLAVAudio::SetDTSHDFraming(BOOL bHDFraming)
{
  m_settings.DTSHDFraming = bHDFraming;
  SaveSettings();

  UpdateBitstreamContext();

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

// ILAVAudioStatus
BOOL CLAVAudio::IsSampleFormatSupported(LAVAudioSampleFormat sfCheck)
{
  if(!m_pOutput || m_pOutput->IsConnected() == FALSE) {
    return FALSE;
  }
  return m_bSampleSupport[sfCheck];
}

HRESULT CLAVAudio::GetDecodeDetails(const char **pCodec, const char **pDecodeFormat, int *pnChannels, int *pSampleRate, DWORD *pChannelMask)
{
  if(!m_pInput || m_pInput->IsConnected() == FALSE) {
    return E_UNEXPECTED;
  }
  if (m_avBSContext) {
    if (pCodec) {
      AVCodec *codec = avcodec_find_decoder(m_nCodecId);
      *pCodec = codec->name;
    }
    if (pnChannels) {
      *pnChannels = m_avBSContext->streams[0]->codec->channels;
    }
    if (pSampleRate) {
      *pSampleRate = m_avBSContext->streams[0]->codec->sample_rate;
    }
    if (pDecodeFormat) {
      *pDecodeFormat = "";
    }
    if (pChannelMask) {
      *pChannelMask = 0;
    }
  } else {
    if (pCodec) {
      if (m_pDTSDecoderContext) {
        static const char *DTSProfiles[] = {
          "dts", NULL, "dts-es", "dts 96/24", NULL, "dts-hd hra", "dts-hd ma", "dts express"
        };

        int index = 0, profile = m_pAVCtx->profile;
        if (profile != FF_PROFILE_UNKNOWN)
          while(profile >>= 1) index++;
        if (index > 7) index = 0;

        *pCodec = DTSProfiles[index] ? DTSProfiles[index] : "dts";
      } else if (m_pAVCodec) {
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
  for(int i = 0; i < sudPinTypesInCount; i++) {
    if(*sudPinTypesIn[i].clsMajorType == mtIn->majortype
      && *sudPinTypesIn[i].clsMinorType == mtIn->subtype && (mtIn->formattype == FORMAT_WaveFormatEx || mtIn->formattype == FORMAT_WaveFormatExFFMPEG || mtIn->formattype == FORMAT_VorbisFormat2)) {
        return S_OK;
    }
  }

  if (m_settings.AllowRawSPDIF) {
    if (mtIn->majortype == MEDIATYPE_Audio && mtIn->formattype == FORMAT_WaveFormatEx &&
       (mtIn->subtype == MEDIASUBTYPE_PCM || mtIn->subtype == MEDIASUBTYPE_DOLBY_AC3_SPDIF)) {
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
  if(iPosition > 0) {
    return VFW_S_NO_MORE_ITEMS;
  }

  if (m_avBSContext) {
    *pMediaType = CreateBitstreamMediaType(m_nCodecId);
  } else {
    const int nChannels = m_pAVCtx->channels;
    const int nSamplesPerSec = m_pAVCtx->sample_rate;

    const AVSampleFormat sample_fmt = (m_pAVCtx->sample_fmt != AV_SAMPLE_FMT_NONE) ? m_pAVCtx->sample_fmt : (m_pAVCodec->sample_fmts ? m_pAVCodec->sample_fmts[0] : AV_SAMPLE_FMT_S16);
    const DWORD dwChannelMask = get_channel_mask(nChannels);

    LAVAudioSampleFormat lav_sample_fmt = m_pDTSDecoderContext ? SampleFormat_24 : get_lav_sample_fmt(sample_fmt, m_pAVCtx->bits_per_raw_sample);
    int bits = m_pDTSDecoderContext ? 0 : m_pAVCtx->bits_per_raw_sample;

    LAVAudioSampleFormat bestFmt = GetBestAvailableSampleFormat(lav_sample_fmt);
    if (bestFmt != lav_sample_fmt) {
      lav_sample_fmt = bestFmt;
      bits = get_byte_per_sample(lav_sample_fmt) << 3;
    }

    *pMediaType = CreateMediaType(lav_sample_fmt, nSamplesPerSec, nChannels, dwChannelMask, bits);
  }
  return S_OK;
}

HRESULT CLAVAudio::ReconnectOutput(long cbBuffer, CMediaType& mt)
{
  HRESULT hr = S_FALSE;

  IMemInputPin *pPin = NULL;
  IMemAllocator *pAllocator = NULL;

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
    if (wBitsPerSample > 0) {
      wfex.Samples.wValidBitsPerSample = wBitsPerSample;
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
  return SUCCEEDED(CheckInputType(mtIn))
    && mtOut->majortype == MEDIATYPE_Audio
    && (mtOut->subtype == MEDIASUBTYPE_PCM || mtOut->subtype == MEDIASUBTYPE_IEEE_FLOAT)
    ? S_OK
    : VFW_E_TYPE_NOT_ACCEPTED;
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

HRESULT CLAVAudio::ffmpeg_init(CodecID codec, const void *format, const GUID format_type, DWORD formatlen)
{
  CAutoLock lock(&m_csReceive);
  ffmpeg_shutdown();
  DbgLog((LOG_TRACE, 10, "::ffmpeg_init(): Initializing decoder for codec %d", codec));

  if (codec == CODEC_ID_DTS || codec == CODEC_ID_TRUEHD)
    m_faJitter.SetNumSamples(200);
  else
    m_faJitter.SetNumSamples(100);

  // Fake codecs that are dependant in input bits per sample, mostly to handle QT PCM tracks
  if (codec == CODEC_ID_PCM_QTRAW || codec == CODEC_ID_PCM_SxxBE || codec == CODEC_ID_PCM_SxxLE || codec == CODEC_ID_PCM_UxxBE || codec == CODEC_ID_PCM_UxxLE) {
    if (format_type == FORMAT_WaveFormatEx) {
      WAVEFORMATEX *wfein = (WAVEFORMATEX *)format;
      ASSERT(wfein->wBitsPerSample == 8 || wfein->wBitsPerSample == 16);
      switch(codec) {
      case CODEC_ID_PCM_QTRAW:
        codec = wfein->wBitsPerSample == 8 ? CODEC_ID_PCM_U8 : CODEC_ID_PCM_S16BE;
        break;
      case CODEC_ID_PCM_SxxBE:
        codec = wfein->wBitsPerSample == 8 ? CODEC_ID_PCM_S8 : CODEC_ID_PCM_S16BE;
        break;
      case CODEC_ID_PCM_SxxLE:
        codec = wfein->wBitsPerSample == 8 ? CODEC_ID_PCM_S8 : CODEC_ID_PCM_S16LE;
        break;
      case CODEC_ID_PCM_UxxBE:
        codec = wfein->wBitsPerSample == 8 ? CODEC_ID_PCM_U8 : CODEC_ID_PCM_U16BE;
        break;
      case CODEC_ID_PCM_UxxLE:
        codec = wfein->wBitsPerSample == 8 ? CODEC_ID_PCM_U8 : CODEC_ID_PCM_U16LE;
        break;
      }
    }
  }

  // Special check for enabled PCM
  if (codec >= 0x10000 && codec < 0x12000 && codec != CODEC_ID_PCM_BLURAY && codec != CODEC_ID_PCM_DVD && !m_settings.bFormats[Codec_PCM])
    return VFW_E_UNSUPPORTED_AUDIO;

  for(int i = 0; i < Codec_NB; ++i) {
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

  if (codec == CODEC_ID_PCM_DVD) {
    if (format_type == FORMAT_WaveFormatEx) {
      WAVEFORMATEX *wfein = (WAVEFORMATEX *)format;
      if (wfein->wBitsPerSample == 16) {
        codec = CODEC_ID_PCM_S16BE;
      }
    }
  }

  // If the codec is bitstreaming, and enabled for it, go there now
  if (IsBitstreaming(codec)) {
    WAVEFORMATEX *wfe = (format_type == FORMAT_WaveFormatEx) ? (WAVEFORMATEX *)format : NULL;
    if(SUCCEEDED(CreateBitstreamContext(codec, wfe))) {
      return S_OK;
    }
  }

  if (codec == CODEC_ID_DTS) {
    InitDTSDecoder();
  }

  const char *codec_override = find_codec_override(codec);
  if (codec_override) {
    m_pAVCodec    = avcodec_find_decoder_by_name(codec_override);
  } else {
    m_pAVCodec    = avcodec_find_decoder(codec);
  }
  CheckPointer(m_pAVCodec, VFW_E_UNSUPPORTED_AUDIO);

  m_pAVCtx = avcodec_alloc_context3(m_pAVCodec);
  CheckPointer(m_pAVCtx, E_POINTER);

  if (codec != CODEC_ID_AAC && codec != CODEC_ID_FLAC)
    m_pParser = av_parser_init(codec);

  if (m_pAVCodec->capabilities & CODEC_CAP_TRUNCATED)
    m_pAVCtx->flags                |= CODEC_FLAG_TRUNCATED;

  // Set Dynamic Range Compression
  m_pAVCtx->drc_scale             = m_settings.DRCEnabled ? (float)m_settings.DRCLevel / 100.0f : 0.0f;

#if REQUEST_FLOAT
  m_pAVCtx->request_sample_fmt = AV_SAMPLE_FMT_FLT;
#endif

  // We can only trust LAV Splitters LATM AAC header...
  BOOL bTrustExtraData = TRUE;
  if (codec == CODEC_ID_AAC_LATM) {
    if (!(FilterInGraph(CLSID_LAVSplitter, m_pGraph) || FilterInGraph(CLSID_LAVSplitterSource, m_pGraph))) {
      bTrustExtraData = FALSE;
    }
  }

  DWORD nSamples, nBytesPerSec;
  WORD nChannels, nBitsPerSample, nBlockAlign;
  audioFormatTypeHandler((BYTE *)format, &format_type, &nSamples, &nChannels, &nBitsPerSample, &nBlockAlign, &nBytesPerSec);

  unsigned extralen = 0;
  getExtraData((BYTE *)format, &format_type, formatlen, NULL, &extralen);

  m_pAVCtx->sample_rate           = nSamples;
  m_pAVCtx->channels              = nChannels;
  m_pAVCtx->bit_rate              = nBytesPerSec << 3;
  m_pAVCtx->bits_per_coded_sample = nBitsPerSample;
  m_pAVCtx->block_align           = nBlockAlign;

  if (bTrustExtraData && extralen) {
    m_pAVCtx->extradata_size      = extralen;
    m_pAVCtx->extradata           = (uint8_t *)av_mallocz(m_pAVCtx->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);
    getExtraData((BYTE *)format, &format_type, formatlen, m_pAVCtx->extradata, NULL);
  }

  m_nCodecId                      = codec;

  int ret = avcodec_open2(m_pAVCtx, m_pAVCodec, NULL);
  if (ret >= 0) {
    m_pPCMData = (BYTE*)av_mallocz(LAV_AUDIO_BUFFER_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);
  } else {
    return VFW_E_UNSUPPORTED_AUDIO;
  }

  // This could probably be a bit smarter..
  if (codec == CODEC_ID_PCM_BLURAY || codec == CODEC_ID_PCM_DVD) {
    m_pAVCtx->bits_per_raw_sample = m_pAVCtx->bits_per_coded_sample;
  }

  // Some sanity checks
  if (m_pAVCtx->channels > 8) {
    return VFW_E_UNSUPPORTED_AUDIO;
  }

  m_bFindDTSInPCM = (codec == CODEC_ID_PCM_S16LE);

  return S_OK;
}

HRESULT CLAVAudio::SetMediaType(PIN_DIRECTION dir, const CMediaType *pmt)
{
  DbgLog((LOG_TRACE, 5, L"SetMediaType -- %S", dir == PINDIR_INPUT ? "in" : "out"));
  if (dir == PINDIR_INPUT) {
    CodecID codec = CODEC_ID_NONE;
    const void *format = pmt->Format();
    GUID format_type = pmt->formattype;
    DWORD formatlen = pmt->cbFormat;

    // Override the format type
    if (pmt->subtype == MEDIASUBTYPE_FFMPEG_AUDIO && pmt->formattype == FORMAT_WaveFormatExFFMPEG) {
      WAVEFORMATEXFFMPEG *wfexff = (WAVEFORMATEXFFMPEG *)pmt->Format();
      codec = (CodecID)wfexff->nCodecId;
      format = &wfexff->wfex;
      format_type = FORMAT_WaveFormatEx;
      formatlen -= sizeof(WAVEFORMATEXFFMPEG) - sizeof(WAVEFORMATEX);
    } else {
      codec = FindCodecId(pmt);
    }

    if (codec == CODEC_ID_NONE) {
      if (m_settings.AllowRawSPDIF) {
        if (pmt->formattype == FORMAT_WaveFormatEx && pmt->subtype == MEDIASUBTYPE_PCM) {
          WAVEFORMATEX *wfex = (WAVEFORMATEX *)pmt->Format();
          switch (wfex->wBitsPerSample) {
          case 8:
            codec = CODEC_ID_PCM_U8;
            break;
          case 16:
            codec = CODEC_ID_PCM_S16LE;
            break;
          case 24:
            codec = CODEC_ID_PCM_S24LE;
            break;
          case 32:
            codec = CODEC_ID_PCM_S32LE;
            break;
          }
        } else if (pmt->subtype == MEDIASUBTYPE_DOLBY_AC3_SPDIF) {
          codec = CODEC_ID_AC3;
        }
      }
      if (codec == CODEC_ID_NONE)
        return VFW_E_TYPE_NOT_ACCEPTED;
    }

    HRESULT hr = ffmpeg_init(codec, format, format_type, formatlen);
    if (FAILED(hr)) {
      return hr;
    }
  }
  return __super::SetMediaType(dir, pmt);
}

HRESULT CLAVAudio::CheckConnect(PIN_DIRECTION dir, IPin *pPin)
{
  DbgLog((LOG_TRACE, 5, L"CheckConnect -- %S", dir == PINDIR_INPUT ? "in" : "out"));
  if (dir == PINDIR_INPUT) {
    // TODO: Check if the upstream source filter is LAVFSplitter, and store that somewhere
    // Validate that this is called before any media type negotiation
  } else if (dir == PINDIR_OUTPUT) {
    CMediaType check_mt;
    const int nChannels = m_pAVCtx ? m_pAVCtx->channels : 2;
    const int nSamplesPerSec = m_pAVCtx ? m_pAVCtx->sample_rate : 48000;
    const DWORD dwChannelMask = get_channel_mask(nChannels);

    check_mt = CreateMediaType(SampleFormat_FP32, nSamplesPerSec, nChannels, dwChannelMask);
    m_bSampleSupport[SampleFormat_FP32] = pPin->QueryAccept(&check_mt) == S_OK;

    check_mt = CreateMediaType(SampleFormat_32, nSamplesPerSec, nChannels, dwChannelMask);
    m_bSampleSupport[SampleFormat_32] = pPin->QueryAccept(&check_mt) == S_OK;

    check_mt = CreateMediaType(SampleFormat_24, nSamplesPerSec, nChannels, dwChannelMask);
    m_bSampleSupport[SampleFormat_24] = pPin->QueryAccept(&check_mt) == S_OK;

    check_mt = CreateMediaType(SampleFormat_16, nSamplesPerSec, nChannels, dwChannelMask);
    m_bSampleSupport[SampleFormat_16] = pPin->QueryAccept(&check_mt) == S_OK;

    check_mt = CreateMediaType(SampleFormat_U8, nSamplesPerSec, nChannels, dwChannelMask);
    m_bSampleSupport[SampleFormat_U8] = pPin->QueryAccept(&check_mt) == S_OK;
  }
  return __super::CheckConnect(dir, pPin);
}

HRESULT CLAVAudio::EndOfStream()
{
  DbgLog((LOG_TRACE, 10, L"CLAVAudio::EndOfStream()"));
  CAutoLock cAutoLock(&m_csReceive);
  // Flush the last data out of the parser
  ProcessBuffer();
  if (m_pParser)
    ProcessBuffer(TRUE);
  FlushOutput(TRUE);
  return __super::EndOfStream();
}

HRESULT CLAVAudio::BeginFlush()
{
  DbgLog((LOG_TRACE, 10, L"CLAVAudio::BeginFlush()"));
  return __super::BeginFlush();
}

HRESULT CLAVAudio::EndFlush()
{
  DbgLog((LOG_TRACE, 10, L"CLAVAudio::EndFlush()"));
  CAutoLock cAutoLock(&m_csReceive);
  m_buff.SetSize(0);
  FlushOutput(FALSE);

  if(m_pParser) {
    av_parser_close(m_pParser);
    m_pParser = av_parser_init(m_nCodecId);
    m_bUpdateTimeCache = TRUE;
  }

  if (m_pAVCtx && m_pAVCtx->codec) {
    avcodec_flush_buffers (m_pAVCtx);
  }

  FlushDTSDecoder();
  m_bsOutput.SetSize(0);

  return __super::EndFlush();
}

HRESULT CLAVAudio::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
  DbgLog((LOG_TRACE, 10, L"CLAVAudio::NewSegment() tStart: %I64d, tStop: %I64d, dRate: %.2f", tStart, tStop, dRate));
  CAutoLock cAutoLock(&m_csReceive);
  m_rtStart = 0;
  m_bQueueResync = TRUE;
  return __super::NewSegment(tStart, tStop, dRate);
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
    pmt = NULL;
    m_buff.SetSize(0);
  }

  if (!m_pAVCtx) {
    return E_FAIL;
  }

  BYTE *pDataIn = NULL;
  if(FAILED(hr = pIn->GetPointer(&pDataIn))) {
    return hr;
  }

  long len = pIn->GetActualDataLength();

  (static_cast<CDeCSSInputPin*>(m_pInput))->StripPacket(pDataIn, len);

  REFERENCE_TIME rtStart = _I64_MIN, rtStop = _I64_MIN;
  hr = pIn->GetTime(&rtStart, &rtStop);

  if(pIn->IsDiscontinuity() == S_OK) {
    m_bDiscontinuity = TRUE;
    m_buff.SetSize(0);
    FlushOutput(FALSE);
    m_bQueueResync = TRUE;
    if(FAILED(hr)) {
      DbgLog((LOG_ERROR, 10, L"::Receive(): Discontinuity without timestamp"));
    }
  }

  if(m_bQueueResync && SUCCEEDED(hr)) {
    DbgLog((LOG_TRACE, 10, L"Resync Request; old: %I64d; new: %I64d; buffer: %d", m_rtStart, rtStart, m_buff.GetCount()));
    FlushOutput();
    m_rtStart = rtStart;
    m_rtStartCacheLT = AV_NOPTS_VALUE;
    m_dStartOffset = 0.0;
    m_bQueueResync = FALSE;
  }

  m_rtStartInput = SUCCEEDED(hr) ? rtStart : AV_NOPTS_VALUE;
  m_rtStopInput = SUCCEEDED(hr) ? rtStop : AV_NOPTS_VALUE;

  int bufflen = m_buff.GetCount();

  // Hack to re-create the BD LPCM header because in the MPC-HC format its stripped off.
  CMediaType inMt(m_pInput->CurrentMediaType());
  if (inMt.subtype == MEDIASUBTYPE_HDMV_LPCM_AUDIO && inMt.formattype == FORMAT_WaveFormatEx) {
    m_buff.SetSize(bufflen + 4);
    BYTE *buf = m_buff.Ptr() + bufflen;
    CreateBDLPCMHeader(buf, (WAVEFORMATEX_HDMV_LPCM *)inMt.pbFormat);
    bufflen = m_buff.GetCount();
  }

  m_buff.SetSize(bufflen + len);
  memcpy(m_buff.Ptr() + bufflen, pDataIn, len);
  len += bufflen;

  hr = ProcessBuffer();

  if (FAILED(hr))
    return hr;

  return S_OK;
}

HRESULT CLAVAudio::ProcessBuffer(BOOL bEOF)
{
  HRESULT hr = S_OK, hr2 = S_OK;

  int buffer_size = m_buff.GetCount();

  BYTE *p = m_buff.Ptr();
  BYTE *base = p;
  BYTE *end = p + buffer_size;

  int consumed = 0;

  if (bEOF) {
    p = NULL;
    buffer_size = -1;
  }

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
      ffmpeg_init(CODEC_ID_DTS, mt.Format(), *mt.FormatType(), mt.FormatLength());
      m_bFindDTSInPCM = FALSE;
    }

    if (buffer_size > (16384 * 4)) {
      m_bFindDTSInPCM = FALSE;
    }

    if (m_bFindDTSInPCM) {
      return S_FALSE;
    }
  }

  if (m_pInput->CurrentMediaType().subtype == MEDIASUBTYPE_DOLBY_AC3_SPDIF) {
    uint16_t word1 = AV_RL16(p);
    uint16_t word2 = AV_RL16(p+2);
    if (word1 == SYNCWORD1 && word2 == SYNCWORD2) {
      uint16_t type = AV_RL16(p+4);
      buffer_size = AV_RL16(p+6) >> 3;

      p += BURST_HEADER_SIZE;

      // SPDIF is apparently big-endian coded
      ff_spdif_bswap_buf16((uint16_t *)p, (uint16_t *)p, buffer_size >> 1);

      end = p + buffer_size;
    }
  }

  // If a bitstreaming context exists, we should bitstream
  if (m_avBSContext) {
    hr2 = Bitstream(p, buffer_size, consumed, &hr);
    if (FAILED(hr2)) {
      DbgLog((LOG_TRACE, 10, L"Invalid sample when bitstreaming!"));
      m_buff.SetSize(0);
      m_bQueueResync = TRUE;
      return S_FALSE;
    } else if (hr2 == S_FALSE) {
      DbgLog((LOG_TRACE, 10, L"::Bitstream returned S_FALSE"));
      hr = S_FALSE;
    }
  } else {
    // Decoding
    // Consume the buffer data
    BufferDetails output_buffer;
    if (m_pDTSDecoderContext)
      hr2 = DecodeDTS(p, buffer_size, consumed, &output_buffer);
    else
      hr2 = Decode(p, buffer_size, consumed, &output_buffer);
    // S_OK means we actually have data to process
    if (hr2 == S_OK) {
      if (SUCCEEDED(PostProcess(&output_buffer))) {
        hr = QueueOutput(output_buffer);
      }
    // FAILED - throw away the data
    } else if (FAILED(hr2)) {
      DbgLog((LOG_TRACE, 10, L"Dropped invalid sample in ProcessBuffer"));
      m_buff.SetSize(0);
      m_bQueueResync = TRUE;
      return S_FALSE;
    } else {
      DbgLog((LOG_TRACE, 10, L"::Decode returned S_FALSE"));
      hr = S_FALSE;
    }
  }

  if (bEOF || consumed <= 0) {
    return hr;
  }

  // This really shouldn't be needed, but apparently it is.
  consumed = min(consumed, buffer_size);

  // Remove the consumed data from the buffer
  p += consumed;
  memmove(base, p, end - p);
  end = base + (end - p);
  p = base;
  m_buff.SetSize((DWORD)(end - p));

  return hr;
}

HRESULT CLAVAudio::Decode(const BYTE * const p, int buffsize, int &consumed, BufferDetails *out)
{
  CheckPointer(out, E_POINTER);
  int nPCMLength	= 0;
  const BYTE *pDataInBuff = p;

  BOOL bEOF = (buffsize == -1);
  if (buffsize == -1) buffsize = 1;

  AVPacket avpkt;
  av_init_packet(&avpkt);

  consumed = 0;
  while (buffsize > 0) {
    nPCMLength = LAV_AUDIO_BUFFER_SIZE;
    if (bEOF) buffsize = 0;
    else {
      COPY_TO_BUFFER(pDataInBuff, buffsize);
    }

    if (m_pParser) {
      BYTE *pOut = NULL;
      int pOut_size = 0;
      int used_bytes = av_parser_parse2(m_pParser, m_pAVCtx, &pOut, &pOut_size, m_pFFBuffer, buffsize, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
      if (used_bytes < 0) {
        DbgLog((LOG_TRACE, 50, L"::Decode() - audio parsing failed (ret: %d)", -used_bytes));
        return E_FAIL;
      } else if(used_bytes == 0 && pOut_size == 0) {
        DbgLog((LOG_TRACE, 50, L"::Decode() - could not process buffer, starving?"));
        break;
      }

      // Timestamp cache to compensate for one frame delay the parser might introduce, in case the frames were already perfectly sliced apart
      // If we used more (or equal) bytes then was output again, we encountered a new frame, update timestamps
      if (used_bytes >= pOut_size && m_bUpdateTimeCache) {
        m_rtStartInputCache = m_rtStartInput;
        m_rtStopInputCache = m_rtStopInput;
        m_bUpdateTimeCache = FALSE;
      }

      if (!bEOF && used_bytes > 0) {
        buffsize -= used_bytes;
        pDataInBuff += used_bytes;
        consumed += used_bytes;
      }

      if (pOut_size > 0) {
        COPY_TO_BUFFER(pOut, pOut_size);
        avpkt.data = m_pFFBuffer;
        avpkt.size = pOut_size;

        int ret2 = avcodec_decode_audio3(m_pAVCtx, (int16_t*)m_pPCMData, &nPCMLength, &avpkt);
        if (ret2 < 0) {
          DbgLog((LOG_TRACE, 50, L"::Decode() - decoding failed despite successfull parsing"));
          m_bQueueResync = TRUE;
          continue;
        }

        m_bUpdateTimeCache = TRUE;

        // Set long-time cache to the first timestamp encountered, used on MPEG-TS containers
        // If the current timestamp is not valid, use the last delivery timestamp in m_rtStart
        if (m_rtStartCacheLT == AV_NOPTS_VALUE) {
          if (m_rtStartInputCache == AV_NOPTS_VALUE) {
            DbgLog((LOG_CUSTOM5, 20, L"WARNING: m_rtStartInputCache is invalid, using calculated rtStart"));
          }
          m_rtStartCacheLT = m_rtStartInputCache != AV_NOPTS_VALUE ? m_rtStartInputCache : m_rtStart;
        }

      } else {
        continue;
      }
    } else if(bEOF) {
      return S_FALSE;
    } else {
      avpkt.data = (uint8_t *)m_pFFBuffer;
      avpkt.size = buffsize;

      int used_bytes = avcodec_decode_audio3(m_pAVCtx, (int16_t*)m_pPCMData, &nPCMLength, &avpkt);

      if(used_bytes < 0) {
        return E_FAIL;
      } else if(used_bytes == 0 && nPCMLength <= 0 ) {
        DbgLog((LOG_TRACE, 50, L"::Decode() - could not process buffer, starving?"));
        break;
      }
      buffsize -= used_bytes;
      pDataInBuff += used_bytes;
      consumed += used_bytes;

      // Set long-time cache to the first timestamp encountered, used on MPEG-TS containers
      // If the current timestamp is not valid, use the last delivery timestamp in m_rtStart
      if (m_rtStartCacheLT == AV_NOPTS_VALUE) {
        if (m_rtStartInput == AV_NOPTS_VALUE) {
          DbgLog((LOG_CUSTOM5, 20, L"WARNING: m_rtStartInput is invalid, using calculated rtStart"));
        }
        m_rtStartCacheLT = m_rtStartInput != AV_NOPTS_VALUE ? m_rtStartInput : m_rtStart;
      }
    }

    // Channel re-mapping and sample format conversion
    if (nPCMLength > 0) {
      const DWORD idx_start = out->bBuffer->GetCount();

      out->wChannels = m_pAVCtx->channels;
      out->dwSamplesPerSec = m_pAVCtx->sample_rate;
      if (m_pAVCtx->channel_layout)
        out->dwChannelMask = (DWORD)m_pAVCtx->channel_layout;
      else
        out->dwChannelMask = get_channel_mask(out->wChannels);

      switch (m_pAVCtx->sample_fmt) {
      case AV_SAMPLE_FMT_U8:
        out->bBuffer->Append(m_pPCMData, nPCMLength);
        out->sfFormat = SampleFormat_U8;
        break;
      case AV_SAMPLE_FMT_S16:
        out->bBuffer->Append(m_pPCMData, nPCMLength);
        out->sfFormat = SampleFormat_16;
        break;
      case AV_SAMPLE_FMT_S32:
        {
          // In FFMPEG, the 32-bit Sample Format is also used for 24-bit samples.
          // So to properly support 24-bit samples, we have to process it here, and store it properly.

          // Figure out the number of bits actually valid in there
          // We only support writing 32, 24 and 16 (16 shouldn't be using AV_SAMPLE_FMT_S32, but who knows)
          short bits_per_sample = 32;
          if (m_pAVCtx->bits_per_raw_sample > 0 && m_pAVCtx->bits_per_raw_sample < 32) {
            bits_per_sample = m_pAVCtx->bits_per_raw_sample > 24 ? 32 : (m_pAVCtx->bits_per_raw_sample > 16 ? 24 : 16);
          }

          const short bytes_per_sample = bits_per_sample >> 3;
          // Number of bits to shift the value to the left
          const short skip = 4 - bytes_per_sample;

          const DWORD size = (nPCMLength >> 2) * bytes_per_sample;
          out->bBuffer->SetSize(idx_start + size);
          // We use BYTE instead of int32_t because we don't know if its actually a 32-bit value we want to write
          BYTE *pDataOut = (BYTE *)(out->bBuffer->Ptr() + idx_start);

          // The source is always in 32-bit values
          const size_t num_elements = nPCMLength / sizeof(int32_t) / m_pAVCtx->channels;
          for (size_t i = 0; i < num_elements; ++i) {
            for(int ch = 0; ch < m_pAVCtx->channels; ++ch) {
              // Get the 32-bit sample
              int32_t sample = ((int32_t *)m_pPCMData) [ch+i*m_pAVCtx->channels];
              // Create a pointer to the sample for easier access
              BYTE * const b_sample = (BYTE *)&sample;
              // Copy the relevant bytes
              memcpy(pDataOut, b_sample + skip, bytes_per_sample);
              pDataOut += bytes_per_sample;
            }
          }

          out->sfFormat = bits_per_sample == 32 ? SampleFormat_32 : (bits_per_sample == 24 ? SampleFormat_24 : SampleFormat_16);
          out->wBitsPerSample = m_pAVCtx->bits_per_raw_sample;
        }
        break;
      case AV_SAMPLE_FMT_FLT:
        out->bBuffer->Append(m_pPCMData, nPCMLength);
        out->sfFormat = SampleFormat_FP32;
        break;
      case AV_SAMPLE_FMT_DBL:
        {
          out->bBuffer->SetSize(idx_start + (nPCMLength / 2));
          float *pDataOut = (float *)(out->bBuffer->Ptr() + idx_start);

          const size_t num_elements = nPCMLength / sizeof(double) / m_pAVCtx->channels;
          for (size_t i = 0; i < num_elements; ++i) {
            for(int ch = 0; ch < m_pAVCtx->channels; ++ch) {
              *pDataOut = (float)((double *)m_pPCMData) [ch+i*m_pAVCtx->channels];
              pDataOut++;
            }
          }
        }
        out->sfFormat = SampleFormat_FP32;
        break;
      default:
        assert(FALSE);
        break;
      }
    }
  }

  if (out->bBuffer->GetCount() <= 0) {
    return S_FALSE;
  }

  out->nSamples = out->bBuffer->GetCount() / get_byte_per_sample(out->sfFormat) / out->wChannels;
  m_DecodeFormat = out->sfFormat;
  m_DecodeLayout = out->dwChannelMask;

  return S_OK;
}

HRESULT CLAVAudio::GetDeliveryBuffer(IMediaSample** pSample, BYTE** pData)
{
  HRESULT hr;

  *pData = NULL;
  if(FAILED(hr = m_pOutput->GetDeliveryBuffer(pSample, NULL, NULL, 0))
    || FAILED(hr = (*pSample)->GetPointer(pData))) {
      return hr;
  }

  AM_MEDIA_TYPE* pmt = NULL;
  if(SUCCEEDED((*pSample)->GetMediaType(&pmt)) && pmt) {
    CMediaType mt = *pmt;
    m_pOutput->SetMediaType(&mt);
    DeleteMediaType(pmt);
    pmt = NULL;
  }

  return S_OK;
}

HRESULT CLAVAudio::QueueOutput(const BufferDetails &buffer)
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

  m_OutputQueue.nSamples += buffer.nSamples;
  m_OutputQueue.bBuffer->Append(buffer.bBuffer);

  // Length of the current sample
  double dDuration = (double)m_OutputQueue.nSamples / m_OutputQueue.dwSamplesPerSec * 10000000.0;
  double dOffset = fmod(dDuration, 1.0);

  // Maximum of 100ms buffer
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

  return hr;
}

HRESULT CLAVAudio::Deliver(const BufferDetails &buffer)
{
  HRESULT hr = S_OK;

  CMediaType mt = CreateMediaType(buffer.sfFormat, buffer.dwSamplesPerSec, buffer.wChannels, buffer.dwChannelMask, buffer.wBitsPerSample);
  WAVEFORMATEX* wfe = (WAVEFORMATEX*)mt.Format();

  long cbBuffer = buffer.nSamples * wfe->nBlockAlign;
  if(FAILED(hr = ReconnectOutput(cbBuffer, mt))) {
    return hr;
  }

  IMediaSample *pOut;
  BYTE *pDataOut = NULL;
  if(FAILED(GetDeliveryBuffer(&pOut, &pDataOut))) {
    return E_FAIL;
  }

  // Length of the current sample
  double dDuration = (double)buffer.nSamples / buffer.dwSamplesPerSec * DBL_SECOND_MULT;
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

  REFERENCE_TIME rtJitter = rtStart - m_rtStartCacheLT;
  m_faJitter.Sample(rtJitter);

  REFERENCE_TIME rtJitterMin = m_faJitter.AbsMinimum();
  if (m_settings.AutoAVSync && abs(rtJitterMin) > MAX_JITTER_DESYNC) {
    DbgLog((LOG_TRACE, 10, L"::Deliver(): corrected A/V sync by %I64d", rtJitterMin));
    m_rtStart -= rtJitterMin;
    m_faJitter.OffsetValues(-rtJitterMin);
  }

#ifdef DEBUG
  DbgLog((LOG_CUSTOM5, 20, L"PCM Delivery, rtStart(calc): %I64d, rtStart(input): %I64d, sample duration: %I64d, diff: %I64d", rtStart, m_rtStartCacheLT, rtStop-rtStart, rtJitter));

  if (m_faJitter.CurrentSample() == 0) {
    DbgLog((LOG_TRACE, 20, L"Jitter Stats: min: %I64d - max: %I64d - avg: %I64d", rtJitterMin, m_faJitter.AbsMaximum(), m_faJitter.Average()));
  }
#endif
  m_rtStartCacheLT = AV_NOPTS_VALUE;

  if(rtStart < 0) {
    goto done;
  }

  if(hr == S_OK) {
    hr = m_pOutput->GetConnected()->QueryAccept(&mt);
    DbgLog((LOG_TRACE, 1, L"Sending new Media Type (QueryAccept: %0#.8x)", hr));
    m_pOutput->SetMediaType(&mt);
    pOut->SetMediaType(&mt);
  }

  if(m_settings.AudioDelayEnabled) {
    REFERENCE_TIME rtDelay = m_settings.AudioDelay * 10000i64;
    rtStart += rtDelay;
    rtStop += rtDelay;
  }

  pOut->SetTime(&rtStart, &rtStop);
  pOut->SetMediaTime(NULL, NULL);

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

HRESULT CLAVAudio::StartStreaming()
{
  HRESULT hr = __super::StartStreaming();
  if(FAILED(hr)) {
    return hr;
  }

  m_bDiscontinuity = FALSE;

  return S_OK;
}

HRESULT CLAVAudio::StopStreaming()
{
  return __super::StopStreaming();
}

HRESULT CLAVAudio::BreakConnect(PIN_DIRECTION dir)
{
  if(dir == PINDIR_INPUT) {
    ffmpeg_shutdown();
  }
  return __super::BreakConnect(dir);
}
