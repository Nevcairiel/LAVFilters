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
#include "LAVAudio.h"

#include <MMReg.h>
#include <assert.h>
#include <Shlwapi.h>

#include "moreuuids.h"
#include "DShowUtil.h"

#include "AudioSettingsProp.h"

#include "registry.h"

// Buffer Size for decoded PCM: 1s of 192kHz 32-bit with 8 channels
// 192000 (Samples) * 4 (Bytes per Sample) * 8 (channels)
#define LAV_AUDIO_BUFFER_SIZE 6144000

#define AUTO_RESYNC 0

#define REQUEST_FLOAT 1

// Maximum Durations (in reference time)
// 100ms
#define PCM_BUFFER_MAX_DURATION 1000000
// 16ms
#define PCM_BUFFER_MIN_DURATION  160000

// Maximum desync that we attribute to jitter before re-syncing (50ms)
#define MAX_JITTER_DESYNC 500000i64

extern HINSTANCE g_hInst;

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
  , m_pExtraDecoderContext(NULL)
{
  avcodec_init();
  av_register_all();

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
#endif
}

CLAVAudio::~CLAVAudio()
{
  ffmpeg_shutdown();
  free(m_pFFBuffer);

  ShutdownBitstreaming();

  if (m_hDllExtraDecoder) {
    FreeLibrary(m_hDllExtraDecoder);
    m_hDllExtraDecoder = NULL;
  }
}

#pragma region DTSDecoder Initialization

typedef void* (*DtsOpen)();
typedef int (*DtsClose)(void *context);
typedef int (*DtsReset)();
typedef int (*DtsSetParam)(void *context, int channels, int bitdepth, int unk1, int unk2, int unk3);
typedef int (*DtsDecode)(void *context, BYTE *pInput, int len, BYTE *pOutput, int unk1, int unk2, int *pBitdepth, int *pChannels, int *pCoreSampleRate, int *pUnk4, int *pHDSampleRate, int *pUnk5, int *pProfile);

struct DTSDecoder {
  DtsOpen pDtsOpen;
  DtsClose pDtsClose;
  DtsReset pDtsReset;
  DtsSetParam pDtsSetParam;
  DtsDecode pDtsDecode;

  void *dtsContext;

  DTSDecoder() : pDtsOpen(NULL), pDtsClose(NULL), pDtsReset(NULL), pDtsSetParam(NULL), pDtsDecode(NULL), dtsContext(NULL) {}
  ~DTSDecoder() {
    if (pDtsClose && dtsContext) {
      pDtsClose(dtsContext);
    }
  }
};

HRESULT CLAVAudio::InitDTSDecoder()
{
  if (!m_hDllExtraDecoder) {
    // Add path of LAVAudio.ax into the Dll search path
    WCHAR wModuleFile[1024];
    GetModuleFileName(g_hInst, wModuleFile, 1024);
    PathRemoveFileSpecW(wModuleFile);
    SetDllDirectory(wModuleFile);

    HMODULE hDll = LoadLibrary(TEXT("dtsdecoderdll.dll"));
    CheckPointer(hDll, E_FAIL);

    m_hDllExtraDecoder = hDll;
  }

  DTSDecoder *context = new DTSDecoder();

  context->pDtsOpen = (DtsOpen)GetProcAddress(m_hDllExtraDecoder, "DtsApiDecOpen");
  if(!context->pDtsOpen) goto fail;

  context->pDtsClose = (DtsClose)GetProcAddress(m_hDllExtraDecoder, "DtsApiDecClose");
  if(!context->pDtsClose) goto fail;

  context->pDtsReset = (DtsReset)GetProcAddress(m_hDllExtraDecoder, "DtsApiDecReset");
  if(!context->pDtsReset) goto fail;

  context->pDtsSetParam = (DtsSetParam)GetProcAddress(m_hDllExtraDecoder, "DtsApiDecSetParam");
  if(!context->pDtsSetParam) goto fail;

  context->pDtsDecode = (DtsDecode)GetProcAddress(m_hDllExtraDecoder, "DtsApiDecodeData");
  if(!context->pDtsDecode) goto fail;

  context->dtsContext = context->pDtsOpen();
  if(!context->dtsContext) goto fail;

  context->pDtsSetParam(context->dtsContext, 8, 24, 0, 0, 0);
  m_iDTSBitDepth = 24;

  m_pExtraDecoderContext = context;

  return S_OK;
fail:
  SAFE_DELETE(context);
  FreeLibrary(m_hDllExtraDecoder);
  m_hDllExtraDecoder = NULL;
  return E_FAIL;
}

#pragma endregion

HRESULT CLAVAudio::LoadSettings()
{
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
  m_settings.DRCEnabled = SUCCEEDED(hr) ? bFlag : FALSE;

  dwVal = reg.ReadDWORD(L"DRCLevel", hr);
  m_settings.DRCLevel = SUCCEEDED(hr) ? (int)dwVal : 100;

  // Default all Codecs to enabled
  for(int i = 0; i < CC_NB; ++i)
    m_settings.bFormats[i] = true;

  pBuf = reg.ReadBinary(L"Formats", dwVal, hr);
  if (SUCCEEDED(hr)) {
    memcpy(&m_settings.bFormats, pBuf, min(dwVal, sizeof(m_settings.bFormats)));
    SAFE_CO_FREE(pBuf);
  }

  // Default bitstreaming to disabled
  memset(m_settings.bBitstream, 0, sizeof(m_settings.bBitstream));

  pBuf = reg.ReadBinary(L"Bitstreaming", dwVal, hr);
  if (SUCCEEDED(hr)) {
    memcpy(&m_settings.bBitstream, pBuf, min(dwVal, sizeof(m_settings.bBitstream)));
    SAFE_CO_FREE(pBuf);
  }

  bFlag = reg.ReadBOOL(L"DTSHDFraming", hr);
  m_settings.DTSHDFraming = SUCCEEDED(hr) ? bFlag : FALSE;

  bFlag = reg.ReadBOOL(L"AutoAVSync", hr);
  m_settings.AutoAVSync = SUCCEEDED(hr) ? bFlag : TRUE;

  return S_OK;
}

HRESULT CLAVAudio::SaveSettings()
{
  HRESULT hr;
  CRegistry reg = CRegistry(HKEY_CURRENT_USER, LAVC_AUDIO_REGISTRY_KEY, hr);
  if (SUCCEEDED(hr)) {
    reg.WriteBOOL(L"DRCEnabled", m_settings.DRCEnabled);
    reg.WriteDWORD(L"DRCLevel", m_settings.DRCLevel);
    reg.WriteBinary(L"Formats", (BYTE *)m_settings.bFormats, sizeof(m_settings.bFormats));
    reg.WriteBinary(L"Bitstreaming", (BYTE *)m_settings.bBitstream, sizeof(m_settings.bBitstream));
    reg.WriteBOOL(L"DTSHDFraming", m_settings.DTSHDFraming);
    reg.WriteBOOL(L"AutoAVSync", m_settings.AutoAVSync);
  }
  return S_OK;
}

void CLAVAudio::ffmpeg_shutdown()
{
  m_pAVCodec	= NULL;
  if (m_pAVCtx) {
    avcodec_close(m_pAVCtx);
    av_free(m_pAVCtx->extradata);
    av_free(m_pAVCtx);
    m_pAVCtx = NULL;
  }

  if (m_pParser) {
    av_parser_close(m_pParser);
    m_pParser = NULL;
  }

  if (m_pPCMData) {
    av_free(m_pPCMData);
    m_pPCMData = NULL;
  }

  FreeBitstreamContext();

  if(m_pExtraDecoderContext) {
    DTSDecoder *dec = (DTSDecoder *)m_pExtraDecoderContext;
    delete dec;
    m_pExtraDecoderContext = NULL;
  }

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

HRESULT CLAVAudio::GetFormatConfiguration(bool *bFormat)
{
  CheckPointer(bFormat, E_POINTER);

  memcpy(bFormat, &m_settings.bFormats, sizeof(m_settings.bFormats));
  return S_OK;
}

HRESULT CLAVAudio::SetFormatConfiguration(bool *bFormat)
{
  CheckPointer(bFormat, E_POINTER);

  memcpy(&m_settings.bFormats, bFormat, sizeof(m_settings.bFormats));
  SaveSettings();
  return S_OK;
}

HRESULT CLAVAudio::GetBitstreamConfig(bool *bBitstreaming)
{
  CheckPointer(bBitstreaming, E_POINTER);

  memcpy(bBitstreaming, &m_settings.bBitstream, sizeof(m_settings.bBitstream));
  return S_OK;
}

HRESULT CLAVAudio::SetBitstreamConfig(bool *bBitstreaming)
{
  CheckPointer(bBitstreaming, E_POINTER);

  memcpy(&m_settings.bBitstream, bBitstreaming, sizeof(m_settings.bBitstream));
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

// ILAVAudioStatus
BOOL CLAVAudio::IsSampleFormatSupported(LAVAudioSampleFormat sfCheck)
{
  if(!m_pOutput || m_pOutput->IsConnected() == FALSE) {
    return FALSE;
  }
  return m_bSampleSupport[sfCheck];
}

HRESULT CLAVAudio::GetInputDetails(const char **pCodec, int *pnChannels, int *pSampleRate)
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
  } else {
    if (pCodec) {
      if (m_nCodecId == CODEC_ID_DTS && m_pExtraDecoderContext) {
        static const char *DTSProfiles[] = {
          "dts", NULL, "dts-es", "dts 96/24", NULL, "dts-hd hra", "dts-hd ma"
        };

        int index = 0, profile = m_pAVCtx->profile;
        while(profile >>= 1) index++;
        if (index > 6) index = 0;

        *pCodec = DTSProfiles[index] ? DTSProfiles[index] : "dts";
      } else {
        *pCodec = m_pAVCodec->name;
      }
    }
    if (pnChannels) {
      *pnChannels = m_pAVCtx->channels;
    }
    if (pSampleRate) {
      *pSampleRate = m_pAVCtx->sample_rate;
    }
  }
  return S_OK;
}

HRESULT CLAVAudio::GetOutputDetails(const char **pDecodeFormat, const char **pOutputFormat, DWORD *pChannelMask)
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
  // Normal Decode Mode
  if (pDecodeFormat) {
    if (IsActive()) {
      *pDecodeFormat = get_sample_format_desc(m_DecodeFormat);
    } else {
      *pDecodeFormat = "Not Running";
    }
  }
  if (pOutputFormat) {
    *pOutputFormat = get_sample_format_desc(m_pOutput->CurrentMediaType());
  }
  if (pChannelMask) {
    WAVEFORMATEX *wfout = (WAVEFORMATEX *)m_pOutput->CurrentMediaType().Format();
    if (wfout->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
      WAVEFORMATEXTENSIBLE *wfext = (WAVEFORMATEXTENSIBLE *)wfout;
      *pChannelMask = wfext->dwChannelMask;
    } else {
      *pChannelMask = 0;
    }
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
  if (nChannel >= m_pAVCtx->channels || nChannel >= 8) {
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
      && *sudPinTypesIn[i].clsMinorType == mtIn->subtype && (mtIn->formattype == FORMAT_WaveFormatEx || mtIn->formattype == FORMAT_WaveFormatExFFMPEG)) {
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
    const DWORD dwChannelMask = get_channel_map(m_pAVCtx)->dwChannelMask;

    *pMediaType = CreateMediaType(get_lav_sample_fmt(sample_fmt, m_pAVCtx->bits_per_raw_sample), nSamplesPerSec, nChannels, dwChannelMask, m_pAVCtx->bits_per_raw_sample);
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
    if(cbBuffer > props.cbBuffer) {
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

  if(dwChannelMask == 0 && wfe->wBitsPerSample > 16) {
    dwChannelMask = nChannels == 2 ? (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT) : SPEAKER_FRONT_CENTER;
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

HRESULT CLAVAudio::ffmpeg_init(CodecID codec, const void *format, GUID format_type)
{
  CAutoLock lock(&m_csReceive);
  ffmpeg_shutdown();

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
  if (codec >= 0x10000 && codec < 0x12000 && codec != CODEC_ID_PCM_BLURAY && codec != CODEC_ID_PCM_DVD && !m_settings.bFormats[CC_PCM])
    return VFW_E_UNSUPPORTED_AUDIO;

  for(int i = 0; i < CC_NB; ++i) {
    const codec_config_t *config = get_codec_config((ConfigCodecs)i);
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

  m_pAVCtx = avcodec_alloc_context();
  CheckPointer(m_pAVCtx, E_POINTER);

  if (codec != CODEC_ID_AAC && codec != CODEC_ID_AAC_LATM && codec != CODEC_ID_FLAC)
    m_pParser = av_parser_init(codec);

  if (m_pAVCodec->capabilities & CODEC_CAP_TRUNCATED)
    m_pAVCtx->flags                |= CODEC_FLAG_TRUNCATED;

  // Set Dynamic Range Compression
  m_pAVCtx->drc_scale             = m_settings.DRCEnabled ? (float)m_settings.DRCLevel / 100.0f : 0.0f;

#if REQUEST_FLOAT
  m_pAVCtx->request_sample_fmt = AV_SAMPLE_FMT_FLT;
#endif

  if (format_type == FORMAT_WaveFormatEx) {
    WAVEFORMATEX *wfein             = (WAVEFORMATEX *)format;
    m_pAVCtx->sample_rate           = wfein->nSamplesPerSec;
    m_pAVCtx->channels              = wfein->nChannels;
    m_pAVCtx->bit_rate              = wfein->nAvgBytesPerSec * 8;
    m_pAVCtx->bits_per_coded_sample = wfein->wBitsPerSample;
    m_pAVCtx->block_align           = wfein->nBlockAlign;

    if (wfein->cbSize) {
      m_pAVCtx->extradata_size      = wfein->cbSize;
      m_pAVCtx->extradata           = (uint8_t *)av_mallocz(m_pAVCtx->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);
      memcpy(m_pAVCtx->extradata, (BYTE *)wfein + sizeof(WAVEFORMATEX), m_pAVCtx->extradata_size);
    }
  }

  m_nCodecId                      = codec;

  int ret = avcodec_open(m_pAVCtx, m_pAVCodec);
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

  return S_OK;
}

HRESULT CLAVAudio::SetMediaType(PIN_DIRECTION dir, const CMediaType *pmt)
{
  DbgLog((LOG_TRACE, 5, L"SetMediaType -- %S", dir == PINDIR_INPUT ? "in" : "out"));
  if (dir == PINDIR_INPUT) {
    CodecID codec = CODEC_ID_NONE;
    const void *format = pmt->Format();
    GUID format_type = format_type = pmt->formattype;

    // Override the format type
    if (pmt->subtype == MEDIASUBTYPE_FFMPEG_AUDIO && pmt->formattype == FORMAT_WaveFormatExFFMPEG) {
      WAVEFORMATEXFFMPEG *wfexff = (WAVEFORMATEXFFMPEG *)pmt->Format();
      codec = (CodecID)wfexff->nCodecId;
      format = &wfexff->wfex;
      format_type = FORMAT_WaveFormatEx;
    } else {
      codec = FindCodecId(pmt);
    }

    if (codec == CODEC_ID_NONE) {
      return VFW_E_TYPE_NOT_ACCEPTED;
    }

    HRESULT hr = ffmpeg_init(codec, format, format_type);
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
    const DWORD dwChannelMask = m_pAVCtx ? get_channel_map(m_pAVCtx)->dwChannelMask : 0;

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
  CAutoLock cAutoLock(&m_csReceive);
  // Flush the last data out of the parser
  if (m_pParser)
    ProcessBuffer(TRUE);
  FlushOutput(TRUE);
  return __super::EndOfStream();
}

HRESULT CLAVAudio::BeginFlush()
{
  return __super::BeginFlush();
}

HRESULT CLAVAudio::EndFlush()
{
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

  if (m_nCodecId == CODEC_ID_DTS && m_pExtraDecoderContext) {
    DTSDecoder *context = (DTSDecoder *)m_pExtraDecoderContext;

    context->pDtsReset();
    context->pDtsSetParam(context->dtsContext, 8, m_iDTSBitDepth, 0, 0, 0);
  }

  return __super::EndFlush();
}

HRESULT CLAVAudio::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
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
    CMediaType mt(*pmt);
    m_pInput->SetMediaType(&mt);
    DeleteMediaType(pmt);
    pmt = NULL;
    m_buff.SetSize(0);
  }

  BYTE *pDataIn = NULL;
  if(FAILED(hr = pIn->GetPointer(&pDataIn))) {
    return hr;
  }

  long len = pIn->GetActualDataLength();

  REFERENCE_TIME rtStart = _I64_MIN, rtStop = _I64_MIN;
  hr = pIn->GetTime(&rtStart, &rtStop);

  if(pIn->IsDiscontinuity() == S_OK) {
    m_bDiscontinuity = TRUE;
    m_buff.SetSize(0);
    FlushOutput(FALSE);
    m_bQueueResync = TRUE;
    if(FAILED(hr)) {
      return S_OK;
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
  if(FAILED(hr) || (AUTO_RESYNC && hr != S_OK))
    m_bQueueResync = TRUE;

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

  // If a bitstreaming context exists, we should bitstream
  if (m_avBSContext) {
    hr2 = Bitstream(p, buffer_size, consumed);
    if (FAILED(hr2)) {
      DbgLog((LOG_TRACE, 10, L"Invalid sample when bitstreaming!"));
      m_buff.SetSize(0);
      return E_FAIL;
    } else if (hr2 == S_FALSE) {
      DbgLog((LOG_TRACE, 10, L"::Bitstream returned S_FALSE"));
      hr = S_FALSE;
    }
  } else {
    // Decoding
    // Consume the buffer data
    BufferDetails output_buffer;
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
      return E_FAIL;
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
  const scmap_t *scmap = NULL;
  BOOL bDTSDecoder = FALSE;

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

        if (m_nCodecId == CODEC_ID_DTS && m_hDllExtraDecoder) {
          bDTSDecoder = TRUE;

          DTSDecoder *dtsDec = (DTSDecoder *)m_pExtraDecoderContext;
          int bitdepth, channels, CoreSampleRate, HDSampleRate, profile;
          int unk4 = 0, unk5 = 0;
          nPCMLength = dtsDec->pDtsDecode(dtsDec->dtsContext, pOut, pOut_size, m_pPCMData, 0, 8, &bitdepth, &channels, &CoreSampleRate, &unk4, &HDSampleRate, &unk5, &profile);
          if (nPCMLength > 0 && bitdepth != m_iDTSBitDepth) {
            int decodeBits = bitdepth > 16 ? 24 : 16;

            // If the bit-depth changed, instruct the DTS Decoder to decode to the new bit depth, and decode the previous sample again.
            if (decodeBits != m_iDTSBitDepth && out->bBuffer->GetCount() == 0) {
              DbgLog((LOG_TRACE, 20, L"::Decode(): The DTS decoder indicated that it outputs %d bits, change config to %d bits to compensate", bitdepth, decodeBits));
              m_iDTSBitDepth = decodeBits;

              dtsDec->pDtsSetParam(dtsDec->dtsContext, 8, m_iDTSBitDepth, 0, 0, 0);
              nPCMLength = dtsDec->pDtsDecode(dtsDec->dtsContext, pOut, pOut_size, m_pPCMData, 0, 8, &bitdepth, &channels, &CoreSampleRate, &unk4, &HDSampleRate, &unk5, &profile);
            }
          }

          if (nPCMLength <= 0) {
            DbgLog((LOG_TRACE, 50, L"::Decode() - DTS Decoder returned empty buffer"));
            m_bQueueResync = TRUE;
            continue;
          }

          m_pAVCtx->sample_rate = HDSampleRate;
          m_pAVCtx->channels = channels;
          m_pAVCtx->profile = profile;
        } else {
          int ret2 = avcodec_decode_audio3(m_pAVCtx, (int16_t*)m_pPCMData, &nPCMLength, &avpkt);
          if (ret2 < 0) {
            DbgLog((LOG_TRACE, 50, L"::Decode() - decoding failed despite successfull parsing"));
            m_bQueueResync = TRUE;
            continue;
          }
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
      scmap = get_channel_map(m_pAVCtx);

      out->wChannels = m_pAVCtx->channels;
      out->dwSamplesPerSec = m_pAVCtx->sample_rate;
      out->dwChannelMask = scmap->dwChannelMask;

      int decode_fmt = m_pAVCtx->sample_fmt;

      if (bDTSDecoder) {
        decode_fmt = LAVF_SAMPLE_FMT_DTS;
      }

      switch (decode_fmt) {
      case LAVF_SAMPLE_FMT_DTS:
        {
          out->bBuffer->SetSize(idx_start + nPCMLength);
          uint8_t *pDataOut = (uint8_t *)(out->bBuffer->Ptr() + idx_start);
          memcpy(pDataOut, m_pPCMData, nPCMLength);

          out->sfFormat = m_iDTSBitDepth == 24 ? SampleFormat_24 : SampleFormat_16;
        }
        break;
      case AV_SAMPLE_FMT_U8:
        {
          out->bBuffer->SetSize(idx_start + nPCMLength);
          uint8_t *pDataOut = (uint8_t *)(out->bBuffer->Ptr() + idx_start);

          const size_t num_elements = nPCMLength / sizeof(uint8_t) / m_pAVCtx->channels;
          for (size_t i = 0; i < num_elements; ++i) {
            for(int ch = 0; ch < m_pAVCtx->channels; ++ch) {
              *pDataOut = ((uint8_t *)m_pPCMData) [scmap->ch[ch]+i*m_pAVCtx->channels];
              pDataOut++;
            }
          }
        }
        out->sfFormat = SampleFormat_U8;
        break;
      case AV_SAMPLE_FMT_S16:
        {
          out->bBuffer->SetSize(idx_start + nPCMLength);
          int16_t *pDataOut = (int16_t *)(out->bBuffer->Ptr() + idx_start);

          const size_t num_elements = nPCMLength / sizeof(int16_t) / m_pAVCtx->channels;
          for (size_t i = 0; i < num_elements; ++i) {
            for(int ch = 0; ch < m_pAVCtx->channels; ++ch) {
              *pDataOut = ((int16_t *)m_pPCMData) [scmap->ch[ch]+i*m_pAVCtx->channels];
              pDataOut++;
            }
          }
        }
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
          const short shift = 32 - bits_per_sample;

          const DWORD size = (nPCMLength >> 2) * bytes_per_sample;
          out->bBuffer->SetSize(idx_start + size);
          // We use BYTE instead of int32_t because we don't know if its actually a 32-bit value we want to write
          BYTE *pDataOut = (BYTE *)(out->bBuffer->Ptr() + idx_start);

          // The source is always in 32-bit values
          const size_t num_elements = nPCMLength / sizeof(int32_t) / m_pAVCtx->channels;
          for (size_t i = 0; i < num_elements; ++i) {
            for(int ch = 0; ch < m_pAVCtx->channels; ++ch) {
              // Get the 32-bit sample
              int32_t sample = ((int32_t *)m_pPCMData) [scmap->ch[ch]+i*m_pAVCtx->channels];
              // Create a pointer to the sample for easier access
              BYTE * const b_sample = (BYTE *)&sample;
              // Drop the empty bits
              sample >>= shift;
              // Copy Data into the ouput
              for(short k = 0; k < bytes_per_sample; ++k) {
                pDataOut[k] = b_sample[k];
              }
              pDataOut += bytes_per_sample;
            }
          }

          out->sfFormat = bits_per_sample == 32 ? SampleFormat_32 : (bits_per_sample == 24 ? SampleFormat_24 : SampleFormat_16);
          out->wBitsPerSample = m_pAVCtx->bits_per_raw_sample;
        }
        break;
      case AV_SAMPLE_FMT_FLT:
        {
          out->bBuffer->SetSize(idx_start + nPCMLength);
          float *pDataOut = (float *)(out->bBuffer->Ptr() + idx_start);

          const size_t num_elements = nPCMLength / sizeof(float) / m_pAVCtx->channels;
          for (size_t i = 0; i < num_elements; ++i) {
            for(int ch = 0; ch < m_pAVCtx->channels; ++ch) {
              *pDataOut = ((float *)m_pPCMData) [scmap->ch[ch]+i*m_pAVCtx->channels];
              pDataOut++;
            }
          }
        }
        out->sfFormat = SampleFormat_FP32;
        break;
      case AV_SAMPLE_FMT_DBL:
        {
          out->bBuffer->SetSize(idx_start + (nPCMLength / 2));
          float *pDataOut = (float *)(out->bBuffer->Ptr() + idx_start);

          const size_t num_elements = nPCMLength / sizeof(double) / m_pAVCtx->channels;
          for (size_t i = 0; i < num_elements; ++i) {
            for(int ch = 0; ch < m_pAVCtx->channels; ++ch) {
              *pDataOut = (float)((double *)m_pPCMData) [scmap->ch[ch]+i*m_pAVCtx->channels];
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

  return S_OK;
}

HRESULT CLAVAudio::PostProcess(BufferDetails *buffer)
{
  if (m_bVolumeStats) {
    UpdateVolumeStats(*buffer);
  }
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
    FlushOutput();
  }

  return S_OK;
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

  return S_OK;
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
  DbgLog((LOG_CUSTOM5, 20, L"PCM Delivery, rtStart(calc): %I64d, rtStart(input): %I64d, diff: %I64d", rtStart, m_rtStartCacheLT, rtJitter));

  if (m_faJitter.CurrentSample() == 0) {
    DbgLog((LOG_TRACE, 20, L"Jitter Stats: min: %I64d - max: %I64d - avg: %I64d", rtJitterMin, m_faJitter.AbsMaximum(), m_faJitter.Average()));
  }
#endif
  m_rtStartCacheLT = AV_NOPTS_VALUE;

  if(rtStart < 0) {
    goto done;
  }

  if(hr == S_OK) {
    DbgLog((LOG_CUSTOM1, 1, L"Sending new Media Type"));
    m_pOutput->SetMediaType(&mt);
    pOut->SetMediaType(&mt);
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
