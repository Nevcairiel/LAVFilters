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
#include "LAVCAudio.h"

#include <MMReg.h>
#include <assert.h>

#include "DShowUtil.h"

// static constructor
CUnknown* WINAPI CLAVCAudio::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr)
{
  return new CLAVCAudio(pUnk, phr);
}

// Constructor
CLAVCAudio::CLAVCAudio(LPUNKNOWN pUnk, HRESULT* phr)
  : CTransformFilter(NAME("lavc audio decoder"), 0, __uuidof(CLAVCAudio)), m_nCodecId(CODEC_ID_NONE), m_pAVCodec(NULL), m_pAVCtx(NULL), m_pPCMData(NULL), m_fDiscontinuity(FALSE), m_rtStart(0), m_SampleFormat(SampleFormat_16)
  , m_pFFBuffer(NULL), m_nFFBufferSize(0), m_pParser(NULL)
{
  avcodec_init();
  avcodec_register_all();
}

CLAVCAudio::~CLAVCAudio()
{
  ffmpeg_shutdown();
  free(m_pFFBuffer);
}

void CLAVCAudio::ffmpeg_shutdown()
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

// CTransformFilter
HRESULT CLAVCAudio::CheckInputType(const CMediaType *mtIn)
{
  for(int i = 0; i < sudPinTypesInCount; i++) {
    if(*sudPinTypesIn[i].clsMajorType == mtIn->majortype
      && *sudPinTypesIn[i].clsMinorType == mtIn->subtype && mtIn->formattype == FORMAT_WaveFormatEx) {
        return S_OK;
    }
  }

  return VFW_E_TYPE_NOT_ACCEPTED;
}

// Get the output media types
HRESULT CLAVCAudio::GetMediaType(int iPosition, CMediaType *pMediaType)
{
  if(m_pInput->IsConnected() == FALSE) {
    return E_UNEXPECTED;
  }

  if(iPosition < 0) {
    return E_INVALIDARG;
  }
  if(iPosition > 0) {
    return VFW_S_NO_MORE_ITEMS;
  }
  WAVEFORMATEX* wfein = (WAVEFORMATEX*)m_pInput->CurrentMediaType().Format();
  AVSampleFormat sample_fmt = m_pAVCodec->sample_fmts ? m_pAVCodec->sample_fmts[0] : AV_SAMPLE_FMT_S16;
  *pMediaType = CreateMediaType(sample_fmt, wfein->nSamplesPerSec, wfein->nChannels);
  return S_OK;
}

HRESULT CLAVCAudio::ReconnectOutput(int nSamples, CMediaType& mt)
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

CMediaType CLAVCAudio::CreateMediaType(AVSampleFormat outputFormat, DWORD nSamplesPerSec, WORD nChannels, DWORD dwChannelMask)
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
  if (outputFormat == AV_SAMPLE_FMT_S32 && m_pAVCtx->bits_per_raw_sample > 0) {
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
    if (outputFormat == AV_SAMPLE_FMT_S32 && m_pAVCtx->bits_per_raw_sample > 0) {
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
HRESULT CLAVCAudio::CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut)
{
  return SUCCEEDED(CheckInputType(mtIn))
    && mtOut->majortype == MEDIATYPE_Audio
    && (mtOut->subtype == MEDIASUBTYPE_PCM || mtOut->subtype == MEDIASUBTYPE_IEEE_FLOAT)
    ? S_OK
    : VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CLAVCAudio::DecideBufferSize(IMemAllocator* pAllocator, ALLOCATOR_PROPERTIES* pProperties)
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

HRESULT CLAVCAudio::SetMediaType(PIN_DIRECTION dir, const CMediaType *pmt)
{
  if (dir == PINDIR_INPUT) {
    CodecID codec = FindCodecId(pmt);
    if (codec == CODEC_ID_NONE) {
      return VFW_E_TYPE_NOT_ACCEPTED;
    }

    ffmpeg_shutdown();

    if (codec == CODEC_ID_MP3) {
      m_pAVCodec    = avcodec_find_decoder_by_name("mp3float");
    } else {
      m_pAVCodec    = avcodec_find_decoder(codec);
    }
    CheckPointer(m_pAVCodec, VFW_E_UNSUPPORTED_AUDIO);

    m_pAVCtx = avcodec_alloc_context();
    CheckPointer(m_pAVCtx, E_POINTER);

    m_pParser = av_parser_init(codec);

    WAVEFORMATEX*	wfein	= (WAVEFORMATEX*)pmt->Format();

    m_pAVCtx->codec_type            = CODEC_TYPE_AUDIO;
    m_pAVCtx->codec_id              = (CodecID)codec;
    m_pAVCtx->sample_rate           = wfein->nSamplesPerSec;
    m_pAVCtx->channels              = wfein->nChannels;
    m_pAVCtx->bit_rate              = wfein->nAvgBytesPerSec * 8;
    m_pAVCtx->bits_per_coded_sample = wfein->wBitsPerSample;
    m_pAVCtx->block_align           = wfein->nBlockAlign;
    m_pAVCtx->flags                |= CODEC_FLAG_TRUNCATED;

    m_nCodecId                      = codec;

    if (wfein->cbSize) {
      m_pAVCtx->extradata_size      = wfein->cbSize;
      m_pAVCtx->extradata           = (uint8_t *)av_mallocz(m_pAVCtx->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);
      memcpy(m_pAVCtx->extradata, (BYTE *)wfein + sizeof(WAVEFORMATEX), m_pAVCtx->extradata_size);
    }

    int ret = avcodec_open(m_pAVCtx, m_pAVCodec);
    if (ret >= 0) {
      m_pPCMData	= (BYTE*)av_mallocz(AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);
    } else {
      return VFW_E_UNSUPPORTED_AUDIO;
    }
  }
  return __super::SetMediaType(dir, pmt);
}

HRESULT CLAVCAudio::CheckConnect(PIN_DIRECTION dir, IPin *pPin)
{
  if (dir == PINDIR_INPUT) {
    // TODO: Check if the upstream source filter is LAVFSplitter, and store that somewhere
    // Validate that this is called before any media type negotiation
  }
  return __super::CheckConnect(dir, pPin);
}

HRESULT CLAVCAudio::EndOfStream()
{
  CAutoLock cAutoLock(&m_csReceive);
  return __super::EndOfStream();
}

HRESULT CLAVCAudio::BeginFlush()
{
  return __super::BeginFlush();
}

HRESULT CLAVCAudio::EndFlush()
{
  CAutoLock cAutoLock(&m_csReceive);
  m_buff.SetSize(0);
  return __super::EndFlush();
}

HRESULT CLAVCAudio::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
  CAutoLock cAutoLock(&m_csReceive);
  m_buff.SetSize(0);
  if (m_pAVCtx) {
    avcodec_flush_buffers (m_pAVCtx);
  }
  return __super::NewSegment(tStart, tStop, dRate);
}

HRESULT CLAVCAudio::Receive(IMediaSample *pIn)
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
  m_buff.SetSize(bufflen + len);
  memcpy(m_buff.Ptr() + bufflen, pDataIn, len);
  len += bufflen;

  hr = ProcessBuffer();

  return hr;
}

HRESULT CLAVCAudio::ProcessBuffer()
{
  HRESULT hr = S_OK;

  BYTE *p = m_buff.Ptr();
  BYTE *base = p;
  BYTE *end = p + m_buff.GetCount();

  int consumed = 0;

  // Consume the buffer data
  Decode(p, end-p, consumed);

  if (consumed <= 0) {
    return S_OK;
  }
  // Remove the consumed data from the buffer
  p += consumed;
  memmove(base, p, end - p);
  end = base + (end - p);
  p = base;
  m_buff.SetSize(end - p);

  return hr;
}

HRESULT CLAVCAudio::Decode(BYTE *p, int buffsize, int &consumed)
{
  HRESULT hr = S_OK;
  int nPCMLength	= 0;
  BYTE *pDataInBuff = p;
  GrowableArray<BYTE> pBuffOut;
  scmap_t *scmap = NULL;

  AVPacket avpkt;
  av_init_packet(&avpkt);

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

    int used_bytes = 0;
    if (m_pParser) {
      BYTE *pOut = NULL;
      int pOut_size = 0;
      used_bytes = av_parser_parse2(m_pParser, m_pAVCtx, &pOut, &pOut_size, m_pFFBuffer, buffsize, AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);
      if (used_bytes < 0 || (used_bytes == 0 && pOut_size == 0)) {
        return S_OK;
      }

      buffsize -= used_bytes;
      pDataInBuff += used_bytes;

      if (pOut_size > 0) {
        avpkt.data = (uint8_t *)pOut;
        avpkt.size = pOut_size;

        int ret2 = avcodec_decode_audio3(m_pAVCtx, (int16_t*)m_pPCMData, &nPCMLength, &avpkt);
        if (ret2 < 0) {
          consumed = used_bytes;
          return S_OK;
        }
      } else {
        continue;
      }
    } else {
      avpkt.data = (uint8_t *)m_pFFBuffer;
      avpkt.size = buffsize;

      used_bytes = avcodec_decode_audio3(m_pAVCtx, (int16_t*)m_pPCMData, &nPCMLength, &avpkt);

      if(used_bytes < 0 || used_bytes == 0 && nPCMLength <= 0 ) {
        consumed = used_bytes;
        return S_OK;
      }
      buffsize -= used_bytes;
      pDataInBuff += used_bytes;
    }
    consumed += used_bytes;

    // Channel re-mapping and sample format conversion
    if (nPCMLength > 0) {
      DWORD idx_start = pBuffOut.GetCount();
      scmap = &m_scmap_default[m_pAVCtx->channels-1];

      switch (m_pAVCtx->sample_fmt) {
      case AV_SAMPLE_FMT_S16:
        {
          pBuffOut.SetSize(idx_start + nPCMLength);
          int16_t *pDataOut = (int16_t *)(pBuffOut.Ptr() + idx_start);

          size_t num_elements = nPCMLength / sizeof(int16_t) / m_pAVCtx->channels;
          for (size_t i = 0; i < num_elements; ++i) {
            for(int ch = 0; ch < m_pAVCtx->channels; ++ch) {
              *pDataOut = ((int16_t *)m_pPCMData) [scmap->ch[ch]+i*m_pAVCtx->channels];
              pDataOut++;
            }
          }
        }
        m_SampleFormat = SampleFormat_16;
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

          DWORD size = (nPCMLength >> 2) * bytes_per_sample;
          pBuffOut.SetSize(idx_start + size);
          // We use BYTE instead of int32_t because we don't know if its actually a 32-bit value we want to write
          BYTE *pDataOut = (BYTE *)(pBuffOut.Ptr() + idx_start);

          // The source is always in 32-bit values
          size_t num_elements = nPCMLength / sizeof(int32_t) / m_pAVCtx->channels;
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

          m_SampleFormat = bits_per_sample == 32 ? SampleFormat_32 : SampleFormat_24;
        }
        break;
      case AV_SAMPLE_FMT_FLT:
        {
          pBuffOut.SetSize(idx_start + nPCMLength);
          float *pDataOut = (float *)(pBuffOut.Ptr() + idx_start);

          size_t num_elements = nPCMLength / sizeof(float) / m_pAVCtx->channels;
          for (size_t i = 0; i < num_elements; ++i) {
            for(int ch = 0; ch < m_pAVCtx->channels; ++ch) {
              *pDataOut = ((float *)m_pPCMData) [scmap->ch[ch]+i*m_pAVCtx->channels];
              pDataOut++;
            }
          }
        }
        m_SampleFormat = SampleFormat_FP32;
        break;
      default:
        assert(FALSE);
        break;
      }
    }
  }

  if(pBuffOut.GetCount() > 0) {
    hr = Deliver(pBuffOut, m_pAVCtx->sample_rate, scmap->nChannels, scmap->dwChannelMask);
  }

  return hr;
}

HRESULT CLAVCAudio::GetDeliveryBuffer(IMediaSample** pSample, BYTE** pData)
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

HRESULT CLAVCAudio::Deliver(GrowableArray<BYTE> &pBuff, DWORD nSamplesPerSec, WORD nChannels, DWORD dwChannelMask)
{
  HRESULT hr = S_OK;

  CMediaType mt = CreateMediaType(m_pAVCtx->sample_fmt, nSamplesPerSec, nChannels, dwChannelMask);
  WAVEFORMATEX* wfe = (WAVEFORMATEX*)mt.Format();

  int nSamples = pBuff.GetCount() / wfe->nChannels;

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

  pOut->SetActualDataLength(pBuff.GetCount());

  // TODO: sample format stuff
  // m_SampleFormat contains the format the last packet was encoded in
  memcpy(pDataOut, pBuff.Ptr(), pBuff.GetCount());

  hr = m_pOutput->Deliver(pOut);
done:
  SafeRelease(&pOut);
  return hr;
}

HRESULT CLAVCAudio::StartStreaming()
{
  HRESULT hr = __super::StartStreaming();
  if(FAILED(hr)) {
    return hr;
  }

  m_fDiscontinuity = false;

  return S_OK;
}

HRESULT CLAVCAudio::StopStreaming()
{
  return __super::StopStreaming();
}

HRESULT CLAVCAudio::BreakConnect(PIN_DIRECTION dir)
{
  if(dir == PINDIR_INPUT) {
    ffmpeg_shutdown();
  }
  return __super::BreakConnect(dir);
}
