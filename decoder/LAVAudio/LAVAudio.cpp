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

#include "moreuuids.h"
#include "DShowUtil.h"

#include "AudioSettingsProp.h"

#include "registry.h"

// static constructor
CUnknown* WINAPI CLAVAudio::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr)
{
  return new CLAVAudio(pUnk, phr);
}

// Constructor
CLAVAudio::CLAVAudio(LPUNKNOWN pUnk, HRESULT* phr)
  : CTransformFilter(NAME("lavc audio decoder"), 0, __uuidof(CLAVAudio)), m_nCodecId(CODEC_ID_NONE), m_pAVCodec(NULL), m_pAVCtx(NULL), m_pPCMData(NULL), m_fDiscontinuity(FALSE), m_rtStart(0), m_DecodeFormat(SampleFormat_16)
  , m_pFFBuffer(NULL), m_nFFBufferSize(0), m_bVolumeStats(FALSE), m_pParser(NULL)
{
  avcodec_init();
  avcodec_register_all();

  m_bSampleSupport[SampleFormat_U8] = TRUE;
  m_bSampleSupport[SampleFormat_16] = TRUE;
  m_bSampleSupport[SampleFormat_24] = TRUE;
  m_bSampleSupport[SampleFormat_32] = TRUE;
  m_bSampleSupport[SampleFormat_FP32] = TRUE;

  LoadSettings();

#ifdef DEBUG
  DbgSetModuleLevel (LOG_TRACE, DWORD_MAX);
#endif
}

CLAVAudio::~CLAVAudio()
{
  ffmpeg_shutdown();
  free(m_pFFBuffer);
}

HRESULT CLAVAudio::LoadSettings()
{
  HRESULT hr;
  DWORD dwVal;
  BOOL bFlag;

  CreateRegistryKey(HKEY_CURRENT_USER, LAVC_AUDIO_REGISTRY_KEY);
  CRegistry reg = CRegistry(HKEY_CURRENT_USER, LAVC_AUDIO_REGISTRY_KEY, hr);
  // We don't check if opening succeeded, because the read functions will set their hr accordingly anyway,
  // and we need to fill the settings with defaults.
  // ReadString returns an empty string in case of failure, so thats fine!
  bFlag = reg.ReadDWORD(L"DRCEnabled", hr);
  m_settings.DRCEnabled = SUCCEEDED(hr) ? bFlag : FALSE;

  dwVal = reg.ReadDWORD(L"DRCLevel", hr);
  m_settings.DRCLevel = SUCCEEDED(hr) ? (int)dwVal : 100;

  return S_OK;
}

HRESULT CLAVAudio::SaveSettings()
{
  HRESULT hr;
  CRegistry reg = CRegistry(HKEY_CURRENT_USER, LAVC_AUDIO_REGISTRY_KEY, hr);
  if (SUCCEEDED(hr)) {
    reg.WriteBOOL(L"DRCEnabled", m_settings.DRCEnabled);
    reg.WriteDWORD(L"DRCLevel", m_settings.DRCLevel);
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
  pPages->cElems = 2;
  pPages->pElems = (GUID *)CoTaskMemAlloc(sizeof(GUID) * pPages->cElems);
  if (pPages->pElems == NULL) {
    return E_OUTOFMEMORY;
  }
  pPages->pElems[0] = CLSID_LAVAudioSettingsProp;
  pPages->pElems[1] = CLSID_LAVAudioStatusProp;
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
  if (pCodec) {
    *pCodec = m_pAVCodec->name;
  }
  if (pnChannels) {
    *pnChannels = m_pAVCtx->channels;
  }
  if (pSampleRate) {
    *pSampleRate = m_pAVCtx->sample_rate;
  }
  return S_OK;
}

HRESULT CLAVAudio::GetOutputDetails(const char **pDecodeFormat, const char **pOutputFormat, DWORD *pChannelMask)
{
  if(!m_pOutput || m_pOutput->IsConnected() == FALSE) {
    return E_UNEXPECTED;
  }
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
  if (!m_pOutput || m_pOutput->IsConnected() == FALSE || !m_bVolumeStats) {
    return E_UNEXPECTED;
  }
  if (nChannel >= m_pAVCtx->channels) {
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
  if(m_pInput->IsConnected() == FALSE || !m_pAVCtx || !m_pAVCodec) {
    return E_UNEXPECTED;
  }

  if(iPosition < 0) {
    return E_INVALIDARG;
  }
  if(iPosition > 0) {
    return VFW_S_NO_MORE_ITEMS;
  }

  const int nChannels = m_pAVCtx->channels;
  const int nSamplesPerSec = m_pAVCtx->sample_rate;

  const AVSampleFormat sample_fmt = (m_pAVCtx->sample_fmt != AV_SAMPLE_FMT_NONE) ? m_pAVCtx->sample_fmt : (m_pAVCodec->sample_fmts ? m_pAVCodec->sample_fmts[0] : AV_SAMPLE_FMT_S16);
  const DWORD dwChannelMask = get_channel_map(m_pAVCtx)->dwChannelMask;

  *pMediaType = CreateMediaType(sample_fmt, nSamplesPerSec, nChannels, dwChannelMask);
  return S_OK;
}

HRESULT CLAVAudio::ReconnectOutput(int nSamples, CMediaType& mt)
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

  WAVEFORMATEX* wfe = (WAVEFORMATEX*)mt.Format();
  long cbBuffer = nSamples * wfe->nBlockAlign;

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

CMediaType CLAVAudio::CreateMediaType(AVSampleFormat outputFormat, DWORD nSamplesPerSec, WORD nChannels, DWORD dwChannelMask, WORD wBitsPerSampleOverride) const
{
  CMediaType mt;

  mt.majortype = MEDIATYPE_Audio;
  mt.subtype = (outputFormat == AV_SAMPLE_FMT_FLT) ? MEDIASUBTYPE_IEEE_FLOAT : MEDIASUBTYPE_PCM;
  mt.formattype = FORMAT_WaveFormatEx;

  WAVEFORMATEXTENSIBLE wfex;
  memset(&wfex, 0, sizeof(wfex));

  WAVEFORMATEX* wfe = &wfex.Format;
  wfe->wFormatTag = (WORD)mt.subtype.Data1;
  wfe->nChannels = nChannels;
  wfe->nSamplesPerSec = nSamplesPerSec;
  if (wBitsPerSampleOverride) {
    wfe->wBitsPerSample = wBitsPerSampleOverride;
  } else if (outputFormat == AV_SAMPLE_FMT_S32 && m_pAVCtx->bits_per_raw_sample > 0) {
    wfe->wBitsPerSample = m_pAVCtx->bits_per_raw_sample > 24 ? 32 : (m_pAVCtx->bits_per_raw_sample > 16 ? 24 : 16);
  } else {
    wfe->wBitsPerSample = av_get_bits_per_sample_fmt(outputFormat);
  }
  wfe->nBlockAlign = wfe->nChannels * wfe->wBitsPerSample / 8;
  wfe->nAvgBytesPerSec = wfe->nSamplesPerSec * wfe->nBlockAlign;

  if(dwChannelMask == 0 && wfe->wBitsPerSample > 16) {
    dwChannelMask = nChannels == 2 ? (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT) : SPEAKER_FRONT_CENTER;
  }

  if(dwChannelMask) {
    wfex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfex.Format.cbSize = sizeof(wfex) - sizeof(wfex.Format);
    wfex.dwChannelMask = dwChannelMask;
    if (!wBitsPerSampleOverride && (outputFormat == AV_SAMPLE_FMT_S32 && m_pAVCtx->bits_per_raw_sample > 0)) {
      wfex.Samples.wValidBitsPerSample = m_pAVCtx->bits_per_raw_sample;
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
  pProperties->cbBuffer = 48000*6*(32/8)/10; // 48KHz 6ch 32bps 100ms
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
  ffmpeg_shutdown();

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

  if (codec == CODEC_ID_MP3) {
    m_pAVCodec    = avcodec_find_decoder_by_name("mp3float");
  } else {
    m_pAVCodec    = avcodec_find_decoder(codec);
  }
  CheckPointer(m_pAVCodec, VFW_E_UNSUPPORTED_AUDIO);

  m_pAVCtx = avcodec_alloc_context();
  CheckPointer(m_pAVCtx, E_POINTER);

  if (codec != CODEC_ID_AAC && codec != CODEC_ID_AAC_LATM)
    m_pParser = av_parser_init(codec);

  m_pAVCtx->codec_type            = AVMEDIA_TYPE_AUDIO;
  m_pAVCtx->codec_id              = (CodecID)codec;
  m_pAVCtx->flags                |= CODEC_FLAG_TRUNCATED;
  m_pAVCtx->drc_scale             = m_settings.DRCEnabled ? (float)m_settings.DRCLevel / 100.0f : 0.0f;

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
    m_pPCMData = (BYTE*)av_mallocz(AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);
  } else {
    return VFW_E_UNSUPPORTED_AUDIO;
  }

  // This could probably be a bit smarter..
  if (codec == CODEC_ID_PCM_BLURAY || codec == CODEC_ID_PCM_DVD) {
    m_pAVCtx->bits_per_raw_sample = m_pAVCtx->bits_per_coded_sample;
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
    const DWORD dwChannelMask = get_channel_map(m_pAVCtx)->dwChannelMask;

    check_mt = CreateMediaType(AV_SAMPLE_FMT_FLT, nSamplesPerSec, nChannels, dwChannelMask);
    m_bSampleSupport[SampleFormat_FP32] = pPin->QueryAccept(&check_mt) == S_OK;

    check_mt = CreateMediaType(AV_SAMPLE_FMT_S32, nSamplesPerSec, nChannels, dwChannelMask, 32);
    m_bSampleSupport[SampleFormat_32] = pPin->QueryAccept(&check_mt) == S_OK;

    check_mt = CreateMediaType(AV_SAMPLE_FMT_S32, nSamplesPerSec, nChannels, dwChannelMask, 24);
    m_bSampleSupport[SampleFormat_24] = pPin->QueryAccept(&check_mt) == S_OK;

    check_mt = CreateMediaType(AV_SAMPLE_FMT_S16, nSamplesPerSec, nChannels, dwChannelMask);
    m_bSampleSupport[SampleFormat_16] = pPin->QueryAccept(&check_mt) == S_OK;

    check_mt = CreateMediaType(AV_SAMPLE_FMT_U8, nSamplesPerSec, nChannels, dwChannelMask);
    m_bSampleSupport[SampleFormat_U8] = pPin->QueryAccept(&check_mt) == S_OK;
  }
  return __super::CheckConnect(dir, pPin);
}

HRESULT CLAVAudio::EndOfStream()
{
  CAutoLock cAutoLock(&m_csReceive);
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
  return __super::EndFlush();
}

HRESULT CLAVAudio::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
  CAutoLock cAutoLock(&m_csReceive);
  m_buff.SetSize(0);
  if (m_pAVCtx) {
    avcodec_flush_buffers (m_pAVCtx);
  }
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
    m_fDiscontinuity = true;
    m_buff.SetSize(0);
    if(FAILED(hr)) {
      return S_OK;
    }
    m_rtStart = rtStart;
  }

  if(SUCCEEDED(hr) && _abs64((m_rtStart - rtStart)) > 1000000i64) { // +-100ms jitter is allowed for now
    m_buff.SetSize(0);
    m_rtStart = rtStart;
  }

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

  return S_OK;
}

HRESULT CLAVAudio::ProcessBuffer()
{
  HRESULT hr = S_OK;

  int buffer_size = m_buff.GetCount();

  BYTE *p = m_buff.Ptr();
  BYTE *base = p;
  BYTE *end = p + buffer_size;

  int consumed = 0;

  // Consume the buffer data
  BufferDetails output_buffer;
  if (SUCCEEDED(Decode(p, buffer_size, consumed, &output_buffer))) {
    if (SUCCEEDED(PostProcess(&output_buffer))) {
      hr = Deliver(output_buffer);
    }
  }

  if (consumed <= 0) {
    return S_OK;
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

  AVPacket avpkt;
  av_init_packet(&avpkt);

  consumed = 0;
  while (buffsize > 0) {
    nPCMLength = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    if (buffsize+FF_INPUT_BUFFER_PADDING_SIZE > m_nFFBufferSize) {
      m_nFFBufferSize = buffsize + FF_INPUT_BUFFER_PADDING_SIZE;
      m_pFFBuffer = (BYTE*)realloc(m_pFFBuffer, m_nFFBufferSize);
    }


    // Required number of additionally allocated bytes at the end of the input bitstream for decoding.
    // This is mainly needed because some optimized bitstream readers read
    // 32 or 64 bit at once and could read over the end.<br>
    // Note: If the first 23 bits of the additional bytes are not 0, then damaged
    // MPEG bitstreams could cause overread and segfault.
    memcpy(m_pFFBuffer, pDataInBuff, buffsize);
    memset(m_pFFBuffer+buffsize, 0, FF_INPUT_BUFFER_PADDING_SIZE);

    if (m_pParser) {
      BYTE *pOut = NULL;
      int pOut_size = 0;
      int used_bytes = av_parser_parse2(m_pParser, m_pAVCtx, &pOut, &pOut_size, m_pFFBuffer, buffsize, AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);
      if (used_bytes < 0 || (used_bytes == 0 && pOut_size == 0)) {
        return E_FAIL;
      }

      buffsize -= used_bytes;
      pDataInBuff += used_bytes;
      consumed += used_bytes;

      if (pOut_size > 0) {
        avpkt.data = (uint8_t *)pOut;
        avpkt.size = pOut_size;

        int ret2 = avcodec_decode_audio3(m_pAVCtx, (int16_t*)m_pPCMData, &nPCMLength, &avpkt);
        if (ret2 < 0) {
          return E_FAIL;
        }
      } else {
        continue;
      }
    } else {
      avpkt.data = (uint8_t *)m_pFFBuffer;
      avpkt.size = buffsize;

      int used_bytes = avcodec_decode_audio3(m_pAVCtx, (int16_t*)m_pPCMData, &nPCMLength, &avpkt);

      if(used_bytes < 0 || used_bytes == 0 && nPCMLength <= 0 ) {
        return E_FAIL;
      }
      buffsize -= used_bytes;
      pDataInBuff += used_bytes;
      consumed += used_bytes;
    }

    // Channel re-mapping and sample format conversion
    if (nPCMLength > 0) {
      const DWORD idx_start = out->bBuffer->GetCount();
      scmap = get_channel_map(m_pAVCtx);

      out->wChannels = m_pAVCtx->channels;
      out->dwSamplesPerSec = m_pAVCtx->sample_rate;
      out->dwChannelMask = scmap->dwChannelMask;

      switch (m_pAVCtx->sample_fmt) {
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
      default:
        assert(FALSE);
        break;
      }
    }
  }

  if(out->bBuffer->GetCount() <= 0) {
    return E_FAIL;
  }
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

HRESULT CLAVAudio::Deliver(const BufferDetails &buffer)
{
  HRESULT hr = S_OK;

  CMediaType mt = CreateMediaType(m_pAVCtx->sample_fmt, buffer.dwSamplesPerSec, buffer.wChannels, buffer.dwChannelMask);
  WAVEFORMATEX* wfe = (WAVEFORMATEX*)mt.Format();

  int nSamples = buffer.bBuffer->GetCount() / wfe->nChannels;

  if(FAILED(hr = ReconnectOutput(nSamples, mt))) {
    return hr;
  }

  IMediaSample *pOut;
  BYTE *pDataOut = NULL;
  if(FAILED(GetDeliveryBuffer(&pOut, &pDataOut))) {
    return E_FAIL;
  }

  REFERENCE_TIME rtDur = 10000000i64 * nSamples / wfe->nSamplesPerSec;
  REFERENCE_TIME rtStart = m_rtStart, rtStop = m_rtStart + rtDur;
  m_rtStart += rtDur;

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
  pOut->SetDiscontinuity(m_fDiscontinuity);
  m_fDiscontinuity = false;
  pOut->SetSyncPoint(TRUE);

  pOut->SetActualDataLength(buffer.bBuffer->GetCount());

  // TODO: sample format stuff
  // m_SampleFormat contains the format the last packet was encoded in
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

  m_fDiscontinuity = false;

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
