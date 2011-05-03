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
#include "moreuuids.h"

#define LAV_BITSTREAM_BUFFER_SIZE 4096

static struct {
  CodecID codec;
  BitstreamCodecs config;
} lavf_bitstream_config[] = {
  { CODEC_ID_AC3,    BS_AC3 },
  { CODEC_ID_EAC3,   BS_EAC3 },
  { CODEC_ID_TRUEHD, BS_TRUEHD },
  { CODEC_ID_DTS,    BS_DTS } // DTS-HD is still DTS, and handled special below
};

// Check wether a codec is bitstreaming eligible and enabled
BOOL CLAVAudio::IsBitstreaming(CodecID codec)
{
  for(int i = 0; i < countof(lavf_bitstream_config); ++i) {
    if (lavf_bitstream_config[i].codec == codec) {
      return m_settings.bBitstream[lavf_bitstream_config[i].config];
    }
  }
  return FALSE;
}

HRESULT CLAVAudio::InitBitstreaming()
{
  // Alloc buffer for the AVIO context
  BYTE *buffer = (BYTE *)CoTaskMemAlloc(LAV_BITSTREAM_BUFFER_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);
  if(!buffer)
    return E_FAIL;
  
  // Create AVIO context
  m_avioBitstream = avio_alloc_context(buffer, LAV_BITSTREAM_BUFFER_SIZE, 1, this, NULL, BSWriteBuffer, NULL);
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

HRESULT CLAVAudio::CreateBitstreamContext(CodecID codec, WAVEFORMATEX *wfe)
{
  int ret = 0;

  if (m_avBSContext)
    FreeBitstreamContext();

  m_avBSContext = avformat_alloc_output_context("spdif", NULL, NULL);
  if (!m_avBSContext) {
    DbgLog((LOG_ERROR, 10, L"::CreateBitstreamContext() -- alloc of avformat spdif muxer failed"));
    goto fail;
  }

  m_avBSContext->pb = m_avioBitstream;
  m_avBSContext->oformat->flags |= AVFMT_NOFILE;

  if (codec == CODEC_ID_DTS && m_settings.bBitstream[BS_DTSHD]) {
    av_set_string3(m_avBSContext->priv_data, "dtshd_rate", "768000", 0, NULL);
    av_set_string3(m_avBSContext->priv_data, "dtshd_fallback_time", "-1", 0, NULL);
  }

  AVStream *st = av_new_stream(m_avBSContext, 0);
  if (!st) {
    DbgLog((LOG_ERROR, 10, L"::CreateBitstreamContext() -- alloc of output stream failed"));
    goto fail;
  }
  st->codec->codec_id = codec;
  st->codec->codec_type = AVMEDIA_TYPE_AUDIO;
  st->codec->channels = wfe->nChannels;
  st->codec->sample_rate = wfe->nSamplesPerSec;

  ret = av_write_header(m_avBSContext);
  if (ret < 0) {
    DbgLog((LOG_ERROR, 10, L"::CreateBitstreamContext() -- av_write_header returned an error code (%d)", -ret));
    goto fail;
  }

  m_nCodecId = codec;

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

    // Override the format type
    if (mt.subtype == MEDIASUBTYPE_FFMPEG_AUDIO && format_type == FORMAT_WaveFormatExFFMPEG) {
      WAVEFORMATEXFFMPEG *wfexff = (WAVEFORMATEXFFMPEG *)mt.Format();
      format = &wfexff->wfex;
      format_type = FORMAT_WaveFormatEx;
    }

    ffmpeg_init(m_nCodecId, format, format_type);
    m_bQueueResync = TRUE;
  }


  // Configure DTS-HD setting
  if(m_avBSContext && m_nCodecId == CODEC_ID_DTS) {
    av_set_string3(m_avBSContext->priv_data, "dtshd_fallback_time", "-1", 0, NULL);
    if (m_settings.bBitstream[BS_DTSHD]) {
      av_set_string3(m_avBSContext->priv_data, "dtshd_rate", "768000", 0, NULL);
    } else {
      av_set_string3(m_avBSContext->priv_data, "dtshd_rate", "0", 0, NULL);
    }
  }

  return S_OK;
}

HRESULT CLAVAudio::FreeBitstreamContext()
{
  if (m_avBSContext)
    avformat_free_context(m_avBSContext);
  m_avBSContext = NULL;

  return S_OK;
}

CMediaType CLAVAudio::CreateBitstreamMediaType(CodecID codec)
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
   case CODEC_ID_AC3:
     wfe->wFormatTag     = WAVE_FORMAT_DOLBY_AC3_SPDIF;
     wfe->nSamplesPerSec = 48000;
     break;
   case CODEC_ID_EAC3:
     wfe->nSamplesPerSec = 192000;
     wfe->nChannels      = 2;
     subtype = KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL_PLUS;
     break;
   case CODEC_ID_TRUEHD:
     wfe->nSamplesPerSec = 192000;
     wfe->nChannels      = 8;
     subtype = KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_MLP;
     break;
   case CODEC_ID_DTS:
     if (m_settings.bBitstream[BS_DTSHD]) {
       wfe->nSamplesPerSec = 192000;
       wfe->nChannels      = 8;
       subtype = KSDATAFORMAT_SUBTYPE_IEC61937_DTS_HD;
     } else {
       wfe->wFormatTag     = WAVE_FORMAT_DOLBY_AC3_SPDIF; // huh? but it works.
       wfe->nSamplesPerSec = 48000;
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

HRESULT CLAVAudio::Bitstream(const BYTE *p, int buffsize, int &consumed)
{
  int ret = 0;

  if (buffsize+FF_INPUT_BUFFER_PADDING_SIZE > m_nFFBufferSize) {
    m_nFFBufferSize = buffsize + FF_INPUT_BUFFER_PADDING_SIZE;
    m_pFFBuffer = (BYTE*)realloc(m_pFFBuffer, m_nFFBufferSize);
  }

  memcpy(m_pFFBuffer, p, buffsize);
  memset(m_pFFBuffer+buffsize, 0, FF_INPUT_BUFFER_PADDING_SIZE);

  consumed = buffsize;

  // TODO: use parser
  AVPacket pkt;
  memset(&pkt, 0, sizeof(AVPacket));
  pkt.pts = pkt.dts = AV_NOPTS_VALUE;
  pkt.duration = 1;

  pkt.data = m_pFFBuffer;
  pkt.size = buffsize;

  // Write SPDIF muxed frame
  ret = av_write_frame(m_avBSContext, &pkt);
  if(ret < 0) {
    DbgLog((LOG_ERROR, 20, "::Bitstream(): av_write_frame returned error code (%d)", -ret));
    return E_FAIL;
  }

  HRESULT hr = S_FALSE;

  if (m_bsOutput.GetCount() > 0) {
    hr = DeliverBitstream(m_nCodecId, m_bsOutput.Ptr(), m_bsOutput.GetCount());
    m_bsOutput.SetSize(0);
  }

  return hr;
}

HRESULT CLAVAudio::DeliverBitstream(CodecID codec, const BYTE *buffer, DWORD dwSize)
{
  HRESULT hr = S_OK;

  CMediaType mt = CreateBitstreamMediaType(codec);
  WAVEFORMATEX* wfe = (WAVEFORMATEX*)mt.Format();

  if(FAILED(hr = ReconnectOutput(dwSize, mt))) {
    return hr;
  }

  IMediaSample *pOut;
  BYTE *pDataOut = NULL;
  if(FAILED(GetDeliveryBuffer(&pOut, &pDataOut))) {
    return E_FAIL;
  }

  if(hr == S_OK) {
    DbgLog((LOG_CUSTOM1, 1, L"Sending new Media Type"));
    m_pOutput->SetMediaType(&mt);
    pOut->SetMediaType(&mt);
  }

  pOut->SetTime(&m_rtStartInput, &m_rtStopInput);
  pOut->SetMediaTime(NULL, NULL);

  pOut->SetPreroll(FALSE);
  pOut->SetDiscontinuity(m_bDiscontinuity);
  m_bDiscontinuity = FALSE;
  pOut->SetSyncPoint(TRUE);

  pOut->SetActualDataLength(dwSize);

  memcpy(pDataOut, buffer, dwSize);

  hr = m_pOutput->Deliver(pOut);

  SafeRelease(&pOut);
  return hr;
}
