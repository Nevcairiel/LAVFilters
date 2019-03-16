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

#include <MMReg.h>
#include "moreuuids.h"

#define LAV_BITSTREAM_BUFFER_SIZE 4096
#define LAV_BITSTREAM_DTS_HD_HR_RATE 192000
#define LAV_BITSTREAM_DTS_HD_MA_RATE 768000

static struct {
  AVCodecID codec;
  LAVBitstreamCodec config;
} lavf_bitstream_config[] = {
  { AV_CODEC_ID_AC3,    Bitstream_AC3 },
  { AV_CODEC_ID_EAC3,   Bitstream_EAC3 },
  { AV_CODEC_ID_TRUEHD, Bitstream_TRUEHD },
  { AV_CODEC_ID_DTS,    Bitstream_DTS } // DTS-HD is still DTS, and handled special below
};

// Check whether a codec is bitstreaming eligible and enabled
BOOL CLAVAudio::IsBitstreaming(AVCodecID codec)
{
  for(int i = 0; i < countof(lavf_bitstream_config); ++i) {
    if (lavf_bitstream_config[i].codec == codec) {
      return m_bBitstreamOverride[lavf_bitstream_config[i].config] ? FALSE : m_settings.bBitstream[lavf_bitstream_config[i].config];
    }
  }
  return FALSE;
}

HRESULT CLAVAudio::InitBitstreaming()
{
  // Alloc buffer for the AVIO context
  BYTE *buffer = (BYTE *)CoTaskMemAlloc(LAV_BITSTREAM_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
  if(!buffer)
    return E_OUTOFMEMORY;
  
  // Create AVIO context
  m_avioBitstream = avio_alloc_context(buffer, LAV_BITSTREAM_BUFFER_SIZE, 1, this, nullptr, BSWriteBuffer, nullptr);
  if(!m_avioBitstream) {
    SAFE_CO_FREE(buffer);
    return E_FAIL;
  }

  return S_OK;
}

HRESULT CLAVAudio::ShutdownBitstreaming()
{
  if (m_avioBitstream) {
    SAFE_CO_FREE(m_avioBitstream->buffer);
    av_freep(&m_avioBitstream);
  }
  return S_OK;
}

// Static function for the AVIO context that writes the buffer into our own output buffer
int CLAVAudio::BSWriteBuffer(void *opaque, uint8_t *buf, int buf_size)
{
  CLAVAudio *filter = (CLAVAudio *)opaque;
  filter->m_bsOutput.Append(buf, buf_size);
  return buf_size;
}

HRESULT CLAVAudio::CreateBitstreamContext(AVCodecID codec, WAVEFORMATEX *wfe)
{
  int ret = 0;

  if (m_avBSContext)
    FreeBitstreamContext();
  m_bsParser.Reset();

  DbgLog((LOG_TRACE, 20, "Creating Bistreaming Context..."));

  ret = avformat_alloc_output_context2(&m_avBSContext, nullptr, "spdif", nullptr);
  if (ret < 0 || !m_avBSContext) {
    DbgLog((LOG_ERROR, 10, L"::CreateBitstreamContext() -- alloc of avformat spdif muxer failed (ret: %d)", ret));
    goto fail;
  }

  m_avBSContext->pb = m_avioBitstream;
  m_avBSContext->oformat->flags |= AVFMT_NOFILE;

  // flush IO after every packet, so we can send it to the audio renderer immediately
  m_avBSContext->flags |= AVFMT_FLAG_FLUSH_PACKETS;

  // DTS-HD is by default off, unless explicitly asked for
  if (m_settings.DTSHDFraming && m_settings.bBitstream[Bitstream_DTSHD] && !m_bForceDTSCore) {
    m_DTSBitstreamMode = DTS_HDMA;
    av_opt_set_int(m_avBSContext->priv_data, "dtshd_rate", LAV_BITSTREAM_DTS_HD_MA_RATE, 0);
  } else {
    m_DTSBitstreamMode = DTS_Core;
    av_opt_set_int(m_avBSContext->priv_data, "dtshd_rate", 0, 0);
  }
  av_opt_set_int(m_avBSContext->priv_data, "dtshd_fallback_time", -1, 0);

  AVStream *st = avformat_new_stream(m_avBSContext, 0);
  if (!st) {
    DbgLog((LOG_ERROR, 10, L"::CreateBitstreamContext() -- alloc of output stream failed"));
    goto fail;
  }
  st->codecpar->codec_id    = codec;
  st->codecpar->codec_type  = AVMEDIA_TYPE_AUDIO;
  st->codecpar->channels    = wfe->nChannels;
  st->codecpar->sample_rate = wfe->nSamplesPerSec;

  ret = avformat_write_header(m_avBSContext, nullptr);
  if (ret < 0) {
    DbgLog((LOG_ERROR, 10, L"::CreateBitstreamContext() -- av_write_header returned an error code (%d)", -ret));
    goto fail;
  }

  return S_OK;
fail:
  FreeBitstreamContext();
  return E_FAIL;
}

HRESULT CLAVAudio::UpdateBitstreamContext()
{
  if (!m_pInput || !m_pInput->IsConnected())
    return E_UNEXPECTED;

  BOOL bBitstream = IsBitstreaming(m_nCodecId);
  if ((bBitstream && !m_avBSContext) || (!bBitstream && m_avBSContext)) {
    CMediaType mt = m_pInput->CurrentMediaType();

    const void *format = mt.Format();
    GUID format_type = mt.formattype;
    DWORD formatlen = mt.cbFormat;

    // Override the format type
    if (mt.subtype == MEDIASUBTYPE_FFMPEG_AUDIO && format_type == FORMAT_WaveFormatExFFMPEG) {
      WAVEFORMATEXFFMPEG *wfexff = (WAVEFORMATEXFFMPEG *)mt.Format();
      format = &wfexff->wfex;
      format_type = FORMAT_WaveFormatEx;
      formatlen -= sizeof(WAVEFORMATEXFFMPEG) - sizeof(WAVEFORMATEX);
    }

    ffmpeg_init(m_nCodecId, format, format_type, formatlen);
    m_bQueueResync = TRUE;
  }

  // Configure DTS-HD setting
  if(m_avBSContext) {
    if (m_settings.bBitstream[Bitstream_DTSHD] && m_settings.DTSHDFraming && !m_bForceDTSCore) {
      m_DTSBitstreamMode = DTS_HDMA;
      av_opt_set_int(m_avBSContext->priv_data, "dtshd_rate", LAV_BITSTREAM_DTS_HD_MA_RATE, 0);
    } else {
      m_DTSBitstreamMode = DTS_Core; // Force auto-detection
      av_opt_set_int(m_avBSContext->priv_data, "dtshd_rate", 0, 0);
    }
  }

  return S_OK;
}

HRESULT CLAVAudio::FreeBitstreamContext()
{
  if (m_avBSContext) {
    av_write_trailer(m_avBSContext); // For the SPDIF muxer that frees the buffers
    avformat_free_context(m_avBSContext);
  }
  m_avBSContext = nullptr;

  // Dump any remaining data
  m_bsOutput.SetSize(0);

  // reset TrueHD MAT state
  memset(&m_TrueHDMATState, 0, sizeof(m_TrueHDMATState));

  return S_OK;
}

CMediaType CLAVAudio::CreateBitstreamMediaType(AVCodecID codec, DWORD dwSampleRate, BOOL bDTSHDOverride)
{
   CMediaType mt;

   mt.majortype  = MEDIATYPE_Audio;
   mt.subtype    = MEDIASUBTYPE_PCM;
   mt.formattype = FORMAT_WaveFormatEx;

   WAVEFORMATEXTENSIBLE wfex;
   memset(&wfex, 0, sizeof(wfex));

   WAVEFORMATEX* wfe = &wfex.Format;

   wfe->nChannels = 2;
   wfe->wBitsPerSample = 16;

   GUID subtype = GUID_NULL;

   switch(codec) {
   case AV_CODEC_ID_AC3:
     wfe->wFormatTag     = WAVE_FORMAT_DOLBY_AC3_SPDIF;
     wfe->nSamplesPerSec = min(dwSampleRate, 48000);
     break;
   case AV_CODEC_ID_EAC3:
     wfe->nSamplesPerSec = 192000;
     wfe->nChannels      = 2;
     subtype = KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL_PLUS;
     break;
   case AV_CODEC_ID_TRUEHD:
     wfe->nSamplesPerSec = 192000;
     wfe->nChannels      = 8;
     subtype = KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_MLP;
     break;
   case AV_CODEC_ID_DTS:
     if (m_settings.bBitstream[Bitstream_DTSHD] && !bDTSHDOverride && m_DTSBitstreamMode != DTS_Core) {
       wfe->nSamplesPerSec = 192000;
       wfe->nChannels      = (m_DTSBitstreamMode == DTS_HDHR) ? 2 : 8;
       subtype = KSDATAFORMAT_SUBTYPE_IEC61937_DTS_HD;
     } else {
       wfe->wFormatTag     = WAVE_FORMAT_DOLBY_AC3_SPDIF; // huh? but it works.
       wfe->nSamplesPerSec = min(dwSampleRate, 48000);
     }
     break;
   default:
     ASSERT(0);
     break;
   }

   wfe->nBlockAlign = wfe->nChannels * wfe->wBitsPerSample / 8;
   wfe->nAvgBytesPerSec = wfe->nSamplesPerSec * wfe->nBlockAlign;

   if (subtype != GUID_NULL) {
      wfex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
      wfex.Format.cbSize = sizeof(wfex) - sizeof(wfex.Format);
      wfex.dwChannelMask = get_channel_mask(wfe->nChannels);
      wfex.Samples.wValidBitsPerSample = wfex.Format.wBitsPerSample;
      wfex.SubFormat = subtype;
   }

   mt.SetSampleSize(1);
   mt.SetFormat((BYTE*)&wfex, sizeof(wfex.Format) + wfex.Format.cbSize);

   return mt;
}

CLAVAudio::DTSBitstreamMode CLAVAudio::GetDTSHDBitstreamMode()
{
  if (m_bForceDTSCore || !m_settings.bBitstream[Bitstream_DTSHD])
    return DTS_Core;

  if (m_settings.DTSHDFraming)
    return DTS_HDMA;

  if (!m_bsParser.m_bDTSHD)
    return DTS_Core;

  if (m_pAVCtx->profile == FF_PROFILE_DTS_HD_HRA)
    return DTS_HDHR;

  return DTS_HDMA;
}

void CLAVAudio::ActivateDTSHDMuxing()
{
  DbgLog((LOG_TRACE, 20, L"::ActivateDTSHDMuxing(): Found DTS-HD marker - switching to DTS-HD muxing mode"));
  m_DTSBitstreamMode = GetDTSHDBitstreamMode();

  // Check if downstream actually accepts it..
  const CMediaType &mt = CreateBitstreamMediaType(m_nCodecId, m_bsParser.m_dwSampleRate);
  HRESULT hr = m_pOutput->GetConnected()->QueryAccept(&mt);
  if (hr != S_OK) {
    DbgLog((LOG_TRACE, 20, L"-> But downstream doesn't want DTS-HD, sticking to DTS core"));
    m_DTSBitstreamMode = DTS_Core;
    m_bForceDTSCore = TRUE;
  } else {
    av_opt_set_int(m_avBSContext->priv_data, "dtshd_rate", (m_DTSBitstreamMode == DTS_HDHR) ? LAV_BITSTREAM_DTS_HD_HR_RATE : LAV_BITSTREAM_DTS_HD_MA_RATE, 0);
  }
}

HRESULT CLAVAudio::Bitstream(const BYTE *pDataBuffer, int buffsize, int &consumed, HRESULT *hrDeliver)
{
  HRESULT hr = S_OK;
  int ret = 0;
  BOOL bFlush = (pDataBuffer == nullptr);

  AVPacket avpkt;
  av_init_packet(&avpkt);
  avpkt.duration = 1;

  consumed = 0;
  while (buffsize > 0) {
    if (bFlush) buffsize = 0;

    BYTE *pOut = nullptr;
    int pOut_size = 0;
    int used_bytes = av_parser_parse2(m_pParser, m_pAVCtx, &pOut, &pOut_size, pDataBuffer, buffsize, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    if (used_bytes < 0) {
      DbgLog((LOG_TRACE, 50, L"::Bitstream() - audio parsing failed (ret: %d)", -used_bytes));
      return E_FAIL;
    } else if(used_bytes == 0 && pOut_size == 0) {
      DbgLog((LOG_TRACE, 50, L"::Bitstream() - could not process buffer, starving?"));
      break;
    }

    // Timestamp cache to compensate for one frame delay the parser might introduce, in case the frames were already perfectly sliced apart
    // If we used more (or equal) bytes then was output again, we encountered a new frame, update timestamps
    if (used_bytes >= pOut_size && m_bUpdateTimeCache) {
      m_rtStartInputCache = m_rtStartInput;
      m_rtStopInputCache = m_rtStopInput;
      m_bUpdateTimeCache = FALSE;
    }

    if (!bFlush && used_bytes > 0) {
      buffsize -= used_bytes;
      pDataBuffer += used_bytes;
      consumed += used_bytes;
    }

    if (pOut_size > 0) {
      hr = m_bsParser.Parse(m_nCodecId, pOut, pOut_size, m_pParser->priv_data);
      if (FAILED(hr)) {
        continue;
      }

      if (m_nCodecId == AV_CODEC_ID_TRUEHD)
      {
        m_bUpdateTimeCache = TRUE;

        // Set long-time cache to the first timestamp encountered, used by TrueHD and E-AC3 because the S/PDIF muxer caches data internally
        // If the current timestamp is not valid, use the last delivery timestamp in m_rtStart
        if (m_rtBitstreamCache == AV_NOPTS_VALUE)
          m_rtBitstreamCache = m_rtStartInputCache != AV_NOPTS_VALUE ? m_rtStartInputCache : m_rtStart;

        BitstreamTrueHD(pOut, pOut_size, hrDeliver);
      }
      else
      {
        if (m_nCodecId == AV_CODEC_ID_DTS)
        {
          DTSBitstreamMode mode = GetDTSHDBitstreamMode();
          if (mode != DTS_Core && mode != m_DTSBitstreamMode)
            ActivateDTSHDMuxing();
        }

        avpkt.data = pOut;
        avpkt.size = pOut_size;

        // Write SPDIF muxed frame
        ret = av_write_frame(m_avBSContext, &avpkt);
        if (ret < 0) {
          DbgLog((LOG_ERROR, 20, "::Bitstream(): av_write_frame returned error code (%d)", -ret));
          m_bsOutput.SetSize(0);
          continue;
        }

        m_bUpdateTimeCache = TRUE;

        // Set long-time cache to the first timestamp encountered, used by TrueHD and E-AC3 because the S/PDIF muxer caches data internally
        // If the current timestamp is not valid, use the last delivery timestamp in m_rtStart
        if (m_rtBitstreamCache == AV_NOPTS_VALUE)
          m_rtBitstreamCache = m_rtStartInputCache != AV_NOPTS_VALUE ? m_rtStartInputCache : m_rtStart;

        // Deliver frame
        if (m_bsOutput.GetCount() > 0) {
          *hrDeliver = DeliverBitstream(m_nCodecId, m_bsOutput.Ptr(), m_bsOutput.GetCount(), m_rtStartInputCache, m_rtStopInputCache);
          m_bsOutput.SetSize(0);
        }
      }

      /* if the bitstreaming context is lost at this point, then the deliver function caused a fallback to PCM */
      if (m_avBSContext == nullptr)
        return S_FALSE;

    }
  }

  return S_OK;
}

HRESULT CLAVAudio::DeliverBitstream(AVCodecID codec, const BYTE *buffer, DWORD dwSize, REFERENCE_TIME rtStartInput, REFERENCE_TIME rtStopInput, BOOL bSwap)
{
  HRESULT hr = S_OK;

  if (m_bFlushing)
    return S_FALSE;

  CMediaType mt = CreateBitstreamMediaType(codec, m_bsParser.m_dwSampleRate);

  if(FAILED(hr = ReconnectOutput(dwSize, mt))) {
    return hr;
  }

  IMediaSample *pOut;
  BYTE *pDataOut = nullptr;
  if(FAILED(GetDeliveryBuffer(&pOut, &pDataOut))) {
    return E_FAIL;
  }

  if (m_bResyncTimestamp && (rtStartInput != AV_NOPTS_VALUE || m_rtBitstreamCache != AV_NOPTS_VALUE)) {
    if (m_rtBitstreamCache != AV_NOPTS_VALUE)
      m_rtStart = m_rtBitstreamCache;
    else
      m_rtStart = rtStartInput;
    m_bResyncTimestamp = FALSE;
  }

  REFERENCE_TIME rtStart = m_rtStart, rtStop = AV_NOPTS_VALUE;
  double dDuration = DBL_SECOND_MULT * (double)m_bsParser.m_dwSamples / m_bsParser.m_dwSampleRate / m_dRate;
  m_dStartOffset += fmod(dDuration, 1.0);

  // Add rounded duration to rtStop
  rtStop = rtStart + (REFERENCE_TIME)(dDuration + 0.5);
  // and unrounded to m_rtStart..
  m_rtStart += (REFERENCE_TIME)dDuration;
  // and accumulate error..
  if (m_dStartOffset > 0.5) {
    m_rtStart++;
    m_dStartOffset -= 1.0;
  }

  REFERENCE_TIME rtJitter = 0;
  if (m_rtBitstreamCache != AV_NOPTS_VALUE)
    rtJitter = rtStart - m_rtBitstreamCache;
  m_faJitter.Sample(rtJitter);

  REFERENCE_TIME rtJitterMin = m_faJitter.AbsMinimum();
  if (m_settings.AutoAVSync && abs(rtJitterMin) > m_JitterLimit && m_bHasVideo) {
    DbgLog((LOG_TRACE, 10, L"::Deliver(): corrected A/V sync by %I64d", rtJitterMin));
    m_rtStart -= rtJitterMin;
    m_faJitter.OffsetValues(-rtJitterMin);
    m_bDiscontinuity = TRUE;
  }

#ifdef DEBUG
  DbgLog((LOG_CUSTOM5, 20, L"Bitstream Delivery, rtStart(calc): %I64d, rtStart(input): %I64d, duration: %I64d, diff: %I64d", rtStart, m_rtBitstreamCache, rtStop-rtStart, rtJitter));

  if (m_faJitter.CurrentSample() == 0) {
    DbgLog((LOG_CUSTOM2, 20, L"Jitter Stats: min: %I64d - max: %I64d - avg: %I64d", rtJitterMin, m_faJitter.AbsMaximum(), m_faJitter.Average()));
  }
#endif
  m_rtBitstreamCache = AV_NOPTS_VALUE;

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

  pOut->SetActualDataLength(dwSize);

  // byte-swap if needed
  if (bSwap)
  {
    lav_spdif_bswap_buf16((uint16_t *)pDataOut, (uint16_t *)buffer, dwSize >> 1);
  }
  else
  {
    memcpy(pDataOut, buffer, dwSize);
  }

  if(hr == S_OK) {
    hr = m_pOutput->GetConnected()->QueryAccept(&mt);
    if (hr == S_FALSE && m_nCodecId == AV_CODEC_ID_DTS && m_DTSBitstreamMode != DTS_Core) {
      DbgLog((LOG_TRACE, 1, L"DTS-HD Media Type failed with %0#.8x, trying fallback to DTS core", hr));
      m_bForceDTSCore = TRUE;
      UpdateBitstreamContext();
      goto done;
    }
    else if (hr == S_FALSE && m_settings.bBitstreamingFallback)
    {
      BitstreamFallbackToPCM();
      goto done;
    }
    DbgLog((LOG_TRACE, 1, L"Sending new Media Type (QueryAccept: %0#.8x)", hr));
    m_pOutput->SetMediaType(&mt);
    pOut->SetMediaType(&mt);
  }

  hr = m_pOutput->Deliver(pOut);
  if (FAILED(hr)) {
    DbgLog((LOG_ERROR, 10, L"::DeliverBitstream failed with code: %0#.8x", hr));
  }

done:
  SafeRelease(&pOut);
  return hr;
}

HRESULT CLAVAudio::BitstreamFallbackToPCM()
{
  // fallback to decoding
  FreeBitstreamContext();

  for (int i = 0; i < countof(lavf_bitstream_config); ++i) {
    if (lavf_bitstream_config[i].codec == m_nCodecId) {
      m_bBitstreamOverride[lavf_bitstream_config[i].config] = TRUE;
    }
  }

  m_bQueueResync = TRUE;

  return S_OK;
}
