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
#include "PostProcessor.h"
#include "LAVAudio.h"
#include "Media.h"

extern "C" {
#include "libavutil/intreadwrite.h"
};

// PCM Volume Adjustment Factors, both for integer and float math
// entries start at 2 channel mixing, half volume
static int pcm_volume_adjust_integer[7] = {
  362, 443, 512, 572, 627, 677, 724
};

static float pcm_volume_adjust_float[7] = {
  1.41421356f, 1.73205081f, 2.00000000f, 2.23606798f, 2.44948974f, 2.64575131f, 2.82842712f
};

// SCALE_CA helper macro for SampleCopyAdjust
#define SCALE_CA(sample, iFactor, factor) { \
  if (iFactor > 0) { \
    sample *= factor; \
    sample >>= 8; \
  } else { \
    sample <<= 8; \
    sample /= factor; \
  } \
}

//
// Helper Function that reads one sample from pIn, applys the scale specified by iFactor, and writes it to pOut
//
static inline void SampleCopyAdjust(BYTE *pOut, const BYTE *pIn, int iFactor, LAVAudioSampleFormat sfSampleFormat)
{
  ASSERT(abs(iFactor) > 1 && abs(iFactor) <= 8);
  const int factorIndex = abs(iFactor) - 2;

  switch(sfSampleFormat) {
  case SampleFormat_U8:
    {
      uint8_t *pOutSample = pOut;
      int32_t sample = *pIn + INT8_MIN;
      SCALE_CA(sample, iFactor, pcm_volume_adjust_integer[factorIndex]);
      *pOutSample = av_clip_uint8(sample - INT8_MIN) ;
    }
    break;
  case SampleFormat_16:
    {
      int16_t *pOutSample = (int16_t *)pOut;
      int32_t sample = *((int16_t *)pIn);
      SCALE_CA(sample, iFactor, pcm_volume_adjust_integer[factorIndex]);
      *pOutSample = av_clip_int16(sample);
    }
    break;
  case SampleFormat_24:
    {
      int32_t sample = (pIn[0] << 8) + (pIn[1] << 16) + (pIn[2] << 24);
      sample >>= 8;
      SCALE_CA(sample, iFactor, pcm_volume_adjust_integer[factorIndex]);
      sample = av_clip(sample, INT24_MIN, INT24_MAX);
      pOut[0] = sample & 0xff;
      pOut[1] = (sample >> 8) & 0xff;
      pOut[2] = (sample >> 16) & 0xff;
    }
    break;
  case SampleFormat_32:
    {
      int32_t *pOutSample = (int32_t *)pOut;
      int64_t sample = *((int32_t *)pIn);
      SCALE_CA(sample, iFactor, pcm_volume_adjust_integer[factorIndex]);
      *pOutSample = av_clipl_int32(sample);
    }
    break;
  case SampleFormat_FP32:
    {
      float *pOutSample = (float *)pOut;
      float sample = *((float *)pIn);
      if (iFactor > 0) {
        sample *= pcm_volume_adjust_float[factorIndex];
      } else {
        sample /= pcm_volume_adjust_float[factorIndex];
      }
      *pOutSample = av_clipf(sample, -1.0f, 1.0f);
    }
    break;
  default:
    ASSERT(0);
    break;
  }
}

//
// Writes one sample of silence into the buffer
//
static inline void Silence(BYTE *pBuffer, LAVAudioSampleFormat sfSampleFormat)
{
  switch (sfSampleFormat) {
  case SampleFormat_16:
  case SampleFormat_24:
  case SampleFormat_32:
  case SampleFormat_FP32:
    memset(pBuffer, 0, get_byte_per_sample(sfSampleFormat));
    break;
  case SampleFormat_U8:
    *pBuffer = 128U;
    break;
  default:
    ASSERT(0);
  }
}

//
// Channel Remapping Processor
// This function can process a PCM buffer of any sample format, and remap the channels
// into any arbitrary layout and channel count.
//
// The samples are copied byte-by-byte, without any conversion or loss.
//
// The ChannelMap is assumed to always have at least uOutChannels valid entries.
// Its layout is in output format:
//      Map[0] is the first output channel, and should contain the index in the source stream (or -1 for silence)
//      Map[1] is the second output channel
//
// Source channels can be applied multiple times to the Destination, however Volume Adjustments are not performed.
// Furthermore, multiple Source channels cannot be merged into one channel.
// Note that when copying one source channel into multiple destinations, you always want to reduce its volume.
// You can either do this in a second step, or use ExtendedChannelMapping
//
// Examples:
// 5.1 Input Buffer, following map will extract the Center channel, and return it as Mono:
// uOutChannels == 1; map = {2}
//
// Mono Input Buffer, Convert to Stereo
// uOutChannels == 2; map = {0, 0}
//
HRESULT ChannelMapping(BufferDetails *pcm, const unsigned uOutChannels, const ChannelMap map)
{
  ExtendedChannelMap extMap;
  for (unsigned ch = 0; ch < uOutChannels; ++ch) {
    ASSERT(map[ch] >= -1 && map[ch] < pcm->wChannels);
    extMap[ch].idx = map[ch];
    extMap[ch].factor = 0;
  }

  return ExtendedChannelMapping(pcm, uOutChannels, extMap);
}

//
// Extended Channel Remapping Processor
// Same functionality as ChannelMapping, except that a factor can be applied to all PCM samples.
//
// For optimization, the factor cannot be freely specified.
// Factors -1, 0, 1 are ignored.
// A Factor of 2 doubles the volume, 3 trippled, etc.
// A Factor of -2 will produce half volume, -3 one third, etc.
// The limit is a factor of 8/-8
//
// Otherwise, see ChannelMapping
HRESULT ExtendedChannelMapping(BufferDetails *pcm, const unsigned uOutChannels, const ExtendedChannelMap extMap)
{
#ifdef DEBUG
  ASSERT(pcm && pcm->bBuffer);
  ASSERT(uOutChannels > 0 && uOutChannels <= 8);
  for(unsigned idx = 0; idx < uOutChannels; ++idx) {
    ASSERT(extMap[idx].idx >= -1 && extMap[idx].idx < pcm->wChannels);
  }
#endif
  // Sample Size
  const unsigned uSampleSize = get_byte_per_sample(pcm->sfFormat);

  // New Output Buffer
  GrowableArray<BYTE> *out = new GrowableArray<BYTE>();
  out->SetSize(uOutChannels * pcm->nSamples * uSampleSize);

  const BYTE *pIn = pcm->bBuffer->Ptr();
  BYTE *pOut = out->Ptr();

  for(unsigned i = 0; i < pcm->nSamples; ++i) {
    for (unsigned ch = 0; ch < uOutChannels; ++ch) {
      if (extMap[ch].idx >= 0) {
        if (!extMap[ch].factor || abs(extMap[ch].factor) == 1)
          memcpy(pOut, pIn + (extMap[ch].idx * uSampleSize), uSampleSize);
        else
          SampleCopyAdjust(pOut, pIn + (extMap[ch].idx * uSampleSize), extMap[ch].factor, pcm->sfFormat);
      } else
        Silence(pOut, pcm->sfFormat);
      pOut += uSampleSize;
    }
    pIn += uSampleSize * pcm->wChannels;
  }

  // Apply changes to buffer
  delete pcm->bBuffer;
  pcm->bBuffer       = out;
  pcm->wChannels     = uOutChannels;

  return S_OK;
}

#define CHL_CONTAINS_ALL(l, m) (((l) & (m)) == (m))
#define CHL_ALL_OR_NONE(l, m) (((l) & (m)) == (m) || ((l) & (m)) == 0)

HRESULT CLAVAudio::CheckChannelLayoutConformity(DWORD dwLayout)
{
  int channels = av_get_channel_layout_nb_channels(dwLayout);

  // We require multi-channel and at least containing stereo
  if (!CHL_CONTAINS_ALL(dwLayout, AV_CH_LAYOUT_STEREO) || channels == 2)
    goto noprocessing;

  // We do not know what to do with "top" channels
  if (dwLayout & (AV_CH_TOP_CENTER|AV_CH_TOP_FRONT_LEFT|AV_CH_TOP_FRONT_CENTER|AV_CH_TOP_FRONT_RIGHT|AV_CH_TOP_BACK_LEFT|AV_CH_TOP_BACK_CENTER|AV_CH_TOP_BACK_RIGHT)) {
    DbgLog((LOG_ERROR, 10, L"::CheckChannelLayoutConformity(): Layout with top channels is not supported (mask: 0x%x)", dwLayout));
    goto noprocessing;
  }

  // We need either both surround channels, or none. One of a type is not supported
  if (!CHL_ALL_OR_NONE(dwLayout, AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
   || !CHL_ALL_OR_NONE(dwLayout, AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT)
   || !CHL_ALL_OR_NONE(dwLayout, AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)) {
    DbgLog((LOG_ERROR, 10, L"::CheckChannelLayoutConformity(): Layout with only one surround channel is not supported (mask: 0x%x)", dwLayout));
    goto noprocessing;
  }

  // No need to process full 5.1/6.1 layouts, or any 8 channel layouts
  if (dwLayout == AV_CH_LAYOUT_5POINT1
   || dwLayout == AV_CH_LAYOUT_5POINT1_BACK
   || dwLayout == AV_CH_LAYOUT_6POINT1_BACK
   || channels == 8) {
    DbgLog((LOG_TRACE, 10, L"::CheckChannelLayoutConformity(): Layout is already a default layout (mask: 0x%x)", dwLayout));
    goto noprocessing;
  }

  // Check 5.1 channels
  if (CHL_CONTAINS_ALL(AV_CH_LAYOUT_5POINT1, dwLayout)        /* 5.1 with side channels */
   || CHL_CONTAINS_ALL(AV_CH_LAYOUT_5POINT1_BACK, dwLayout)   /* 5.1 with back channels */
   || CHL_CONTAINS_ALL(LAV_CH_LAYOUT_5POINT1_WIDE, dwLayout)  /* 5.1 with side-front channels */
   || CHL_CONTAINS_ALL(LAV_CH_LAYOUT_5POINT1_BC, dwLayout))   /* 3/1/x layouts, front channels with a back center */
   return Create51Conformity(dwLayout);

  // Check 6.1 channels (5.1 layouts + Back Center)
  if (CHL_CONTAINS_ALL(AV_CH_LAYOUT_6POINT1, dwLayout)        /* 6.1 with side channels */
   || CHL_CONTAINS_ALL(AV_CH_LAYOUT_6POINT1_BACK, dwLayout)   /* 6.1 with back channels */
   || CHL_CONTAINS_ALL(LAV_CH_LAYOUT_5POINT1_WIDE|AV_CH_BACK_CENTER, dwLayout)) /* 6.1 with side-front channels */
   return Create61Conformity(dwLayout);

  // Check 7.1 channels
  if (CHL_CONTAINS_ALL(AV_CH_LAYOUT_7POINT1, dwLayout)              /* 7.1 with side+back channels */
   || CHL_CONTAINS_ALL(AV_CH_LAYOUT_7POINT1_WIDE, dwLayout)         /* 7.1 with front-side+back channels */
   || CHL_CONTAINS_ALL(LAV_CH_LAYOUT_7POINT1_EXTRAWIDE, dwLayout))  /* 7.1 with front-side+side channels */
   return Create71Conformity(dwLayout);

noprocessing:
  m_bChannelMappingRequired = FALSE;
  return S_FALSE;
}

HRESULT CLAVAudio::Create51Conformity(DWORD dwLayout)
{
  DbgLog((LOG_TRACE, 10, L"::Create51Conformity(): Creating 5.1 default layout (mask: 0x%x)", dwLayout));
  int ch = 0;
  ExtChMapClear(&m_ChannelMap);
  // All layouts we support have to contain L/R
  ExtChMapSet(&m_ChannelMap, 0, ch++, 0);
  ExtChMapSet(&m_ChannelMap, 1, ch++, 0);
  // Center channel
  if (dwLayout & AV_CH_FRONT_CENTER)
    ExtChMapSet(&m_ChannelMap, 2, ch++, 0);
  // LFE
  if (dwLayout & AV_CH_LOW_FREQUENCY)
    ExtChMapSet(&m_ChannelMap, 3, ch++, 0);
  // Back/Side
  if (dwLayout & (AV_CH_SIDE_LEFT|AV_CH_BACK_LEFT|AV_CH_FRONT_LEFT_OF_CENTER)) {
    ExtChMapSet(&m_ChannelMap, 4, ch++, 0);
    ExtChMapSet(&m_ChannelMap, 5, ch++, 0);
  // Back Center
  } else if (dwLayout & AV_CH_BACK_CENTER) {
    ExtChMapSet(&m_ChannelMap, 4, ch, -2);
    ExtChMapSet(&m_ChannelMap, 5, ch++, -2);
  }
  m_bChannelMappingRequired = TRUE;
  m_ChannelMapOutputChannels = 6;
  m_ChannelMapOutputLayout = AV_CH_LAYOUT_5POINT1;
  return S_OK;
}

HRESULT CLAVAudio::Create61Conformity(DWORD dwLayout)
{
  if (m_settings.Expand61) {
    DbgLog((LOG_TRACE, 10, L"::Create61Conformity(): Expanding to 7.1"));
    return Create71Conformity(dwLayout);
  }

  DbgLog((LOG_TRACE, 10, L"::Create61Conformity(): Creating 6.1 default layout (mask: 0x%x)", dwLayout));
  int ch = 0;
  ExtChMapClear(&m_ChannelMap);
  // All layouts we support have to contain L/R
  ExtChMapSet(&m_ChannelMap, 0, ch++, 0);
  ExtChMapSet(&m_ChannelMap, 1, ch++, 0);
  // Center channel
  if (dwLayout & AV_CH_FRONT_CENTER)
    ExtChMapSet(&m_ChannelMap, 2, ch++, 0);
  // LFE
  if (dwLayout & AV_CH_LOW_FREQUENCY)
    ExtChMapSet(&m_ChannelMap, 3, ch++, 0);
  // Back channels, if before BC
  if (dwLayout & (AV_CH_BACK_LEFT|AV_CH_FRONT_LEFT_OF_CENTER)) {
    DbgLog((LOG_TRACE, 10, L"::Create61Conformity(): Using surround channels *before* BC"));
    ExtChMapSet(&m_ChannelMap, 4, ch++, 0);
    ExtChMapSet(&m_ChannelMap, 5, ch++, 0);
  }
  // Back Center
  if (dwLayout & AV_CH_BACK_CENTER)
    ExtChMapSet(&m_ChannelMap, 6, ch++, 0);
  // Back channels, if after BC
  if (dwLayout & AV_CH_SIDE_LEFT) {
    DbgLog((LOG_TRACE, 10, L"::Create61Conformity(): Using surround channels *after* BC"));
    ExtChMapSet(&m_ChannelMap, 4, ch++, 0);
    ExtChMapSet(&m_ChannelMap, 5, ch++, 0);
  }

  m_bChannelMappingRequired = TRUE;
  m_ChannelMapOutputChannels = 7;
  m_ChannelMapOutputLayout = AV_CH_LAYOUT_6POINT1_BACK;
  return S_OK;
}

HRESULT CLAVAudio::Create71Conformity(DWORD dwLayout)
{
  DbgLog((LOG_TRACE, 10, L"::Create71Conformity(): Creating 7.1 default layout (mask: 0x%x)", dwLayout));
  int ch = 0;
  ExtChMapClear(&m_ChannelMap);
  // All layouts we support have to contain L/R
  ExtChMapSet(&m_ChannelMap, 0, ch++, 0);
  ExtChMapSet(&m_ChannelMap, 1, ch++, 0);
  // Center channel
  if (dwLayout & AV_CH_FRONT_CENTER)
    ExtChMapSet(&m_ChannelMap, 2, ch++, 0);
  // LFE
  if (dwLayout & AV_CH_LOW_FREQUENCY)
    ExtChMapSet(&m_ChannelMap, 3, ch++, 0);
  // Back channels
  if (dwLayout & AV_CH_BACK_CENTER) {
    DbgLog((LOG_TRACE, 10, L"::Create71Conformity(): Usign BC to fill back channels"));
    if (dwLayout & AV_CH_SIDE_LEFT) {
      DbgLog((LOG_TRACE, 10, L"::Create71Conformity(): Using BC before side-surround channels"));
      ExtChMapSet(&m_ChannelMap, 4, ch,   -2);
      ExtChMapSet(&m_ChannelMap, 5, ch++, -2);
      ExtChMapSet(&m_ChannelMap, 6, ch++, 0);
      ExtChMapSet(&m_ChannelMap, 7, ch++, 0);
    } else {
      DbgLog((LOG_TRACE, 10, L"::Create71Conformity(): Using BC after side-surround channels"));
      ExtChMapSet(&m_ChannelMap, 6, ch++, 0);
      ExtChMapSet(&m_ChannelMap, 7, ch++, 0);
      ExtChMapSet(&m_ChannelMap, 4, ch,   -2);
      ExtChMapSet(&m_ChannelMap, 5, ch++, -2);
    }
  } else {
    DbgLog((LOG_TRACE, 10, L"::Create71Conformity(): Using original 4 surround channels"));
    ExtChMapSet(&m_ChannelMap, 4, ch++, 0);
    ExtChMapSet(&m_ChannelMap, 5, ch++, 0);
    ExtChMapSet(&m_ChannelMap, 6, ch++, 0);
    ExtChMapSet(&m_ChannelMap, 7, ch++, 0);
  }

  m_bChannelMappingRequired = TRUE;
  m_ChannelMapOutputChannels = 8;
  m_ChannelMapOutputLayout = AV_CH_LAYOUT_7POINT1;
  return S_OK;
}

HRESULT CLAVAudio::PadTo32(BufferDetails *buffer)
{
  ASSERT(buffer->sfFormat == SampleFormat_24);

  const DWORD size = (buffer->nSamples * buffer->wChannels) * 4;
  GrowableArray<BYTE> *pcmOut = new GrowableArray<BYTE>();
  pcmOut->SetSize(size);

  const BYTE *pDataIn = buffer->bBuffer->Ptr();
  BYTE *pDataOut = pcmOut->Ptr();

  for (unsigned int i = 0; i < buffer->nSamples; ++i) {
    for(int ch = 0; ch < buffer->wChannels; ++ch) {
      AV_WL32(pDataOut, AV_RL24(pDataIn) << 8);
      pDataOut += 4;
      pDataIn += 3;
    }
  }
  delete buffer->bBuffer;
  buffer->bBuffer = pcmOut;
  buffer->sfFormat = SampleFormat_32;
  buffer->wBitsPerSample = 24;

  return S_OK;
}

HRESULT CLAVAudio::Truncate32Buffer(BufferDetails *buffer)
{
  ASSERT(buffer->sfFormat == SampleFormat_32 && buffer->wBitsPerSample <= 24);

  // Determine the bytes per sample to keep. Cut a 16-bit sample to 24 if 16-bit is disabled for some reason
  const int bytes_per_sample = buffer->wBitsPerSample <= 16 && GetSampleFormat(SampleFormat_16) ? 2 : 3;
  if (bytes_per_sample == 3 && !GetSampleFormat(SampleFormat_24))
    return S_FALSE;

  const int skip = 4 - bytes_per_sample;
  const DWORD size = (buffer->nSamples * buffer->wChannels) * bytes_per_sample;
  GrowableArray<BYTE> *pcmOut = new GrowableArray<BYTE>();
  pcmOut->SetSize(size);

  const BYTE *pDataIn = buffer->bBuffer->Ptr();
  BYTE *pDataOut = pcmOut->Ptr();

  pDataIn += skip;
  for (unsigned int i = 0; i < buffer->nSamples; ++i) {
    for(int ch = 0; ch < buffer->wChannels; ++ch) {
      memcpy(pDataOut, pDataIn, bytes_per_sample);
      pDataOut += bytes_per_sample;
      pDataIn += 4;
    }
  }

  delete buffer->bBuffer;
  buffer->bBuffer = pcmOut;
  buffer->sfFormat = bytes_per_sample == 3 ? SampleFormat_24 : SampleFormat_16;

  return S_OK;
}

HRESULT CLAVAudio::PerformAVRProcessing(BufferDetails *buffer)
{
  int ret = 0;

  DWORD dwMixingLayout = m_dwOverrideMixer ? m_dwOverrideMixer : (m_settings.MixingEnabled ? m_settings.MixingLayout : buffer->dwChannelMask);
  // No mixing stereo, if the user doesn't want it
  if (buffer->wChannels <= 2 && (m_settings.MixingFlags & LAV_MIXING_FLAG_UNTOUCHED_STEREO))
    dwMixingLayout = buffer->dwChannelMask;

  LAVAudioSampleFormat outputFormat = (dwMixingLayout != buffer->dwChannelMask) ? GetBestAvailableSampleFormat(SampleFormat_FP32) : GetBestAvailableSampleFormat(buffer->sfFormat);

  // Short Circuit some processing
  if (dwMixingLayout == buffer->dwChannelMask && !buffer->bPlanar) {
    if (buffer->sfFormat == SampleFormat_24 && outputFormat == SampleFormat_32) {
      PadTo32(buffer);
      return S_OK;
    } else if (buffer->sfFormat == SampleFormat_32 && outputFormat == SampleFormat_24) {
      buffer->wBitsPerSample = 24;
      Truncate32Buffer(buffer);
      return S_OK;
    }
  }

  // Sadly, we need to convert this, avresample has no 24-bit mode
  if (buffer->sfFormat == SampleFormat_24) {
    PadTo32(buffer);
  }

  if (buffer->dwChannelMask != m_MixingInputLayout || (!m_avrContext && !m_bAVResampleFailed) || m_bMixingSettingsChanged || m_dwRemixLayout != dwMixingLayout || outputFormat != m_sfRemixFormat || buffer->sfFormat != m_MixingInputFormat) {
    m_bAVResampleFailed = FALSE;
    m_bMixingSettingsChanged = FALSE;
    if (m_avrContext) {
      avresample_close(m_avrContext);
      avresample_free(&m_avrContext);
    }

    m_MixingInputLayout = buffer->dwChannelMask;
    m_MixingInputFormat = buffer->sfFormat;
    m_dwRemixLayout = dwMixingLayout;
    m_sfRemixFormat = outputFormat;

    m_avrContext = avresample_alloc_context();
    av_opt_set_int(m_avrContext, "in_channel_layout", buffer->dwChannelMask, 0);
    av_opt_set_int(m_avrContext, "in_sample_fmt", get_ff_sample_fmt(buffer->sfFormat), 0);

    av_opt_set_int(m_avrContext, "out_channel_layout", dwMixingLayout, 0);
    av_opt_set_int(m_avrContext, "out_sample_fmt", get_ff_sample_fmt(m_sfRemixFormat), 0);

    av_opt_set_int(m_avrContext, "dither_method", m_settings.SampleConvertDither ? AV_RESAMPLE_DITHER_TRIANGULAR_HP : AV_RESAMPLE_DITHER_NONE, 0);

    // Setup mixing properties, if needed
    if (buffer->dwChannelMask != dwMixingLayout) {
      BOOL bNormalize = !!(m_settings.MixingFlags & LAV_MIXING_FLAG_NORMALIZE_MATRIX);
      av_opt_set_int(m_avrContext, "clip_protection", !bNormalize && (m_settings.MixingFlags & LAV_MIXING_FLAG_CLIP_PROTECTION), 0);

      // Create Matrix
      int in_ch = buffer->wChannels;
      int out_ch = av_get_channel_layout_nb_channels(dwMixingLayout);
      double *matrix_dbl = (double *)av_mallocz(in_ch * out_ch * sizeof(*matrix_dbl));

      const double center_mix_level = (double)m_settings.MixingCenterLevel / 10000.0;
      const double surround_mix_level = (double)m_settings.MixingSurroundLevel / 10000.0;
      const double lfe_mix_level = (double)m_settings.MixingLFELevel / 10000.0 / (dwMixingLayout == AV_CH_LAYOUT_MONO ? 1.0 : M_SQRT1_2);
      ret = avresample_build_matrix(buffer->dwChannelMask, dwMixingLayout, center_mix_level, surround_mix_level, lfe_mix_level, bNormalize, matrix_dbl, in_ch, (AVMatrixEncoding)m_settings.MixingMode);
      if (ret < 0) {
        DbgLog((LOG_ERROR, 10, L"avresample_build_matrix failed, layout in: %x, out: %x, sample fmt in: %d, out: %d", buffer->dwChannelMask, dwMixingLayout, buffer->sfFormat, m_sfRemixFormat));
        av_free(matrix_dbl);
        goto setuperr;
      }

      // Set Matrix on the context
      ret = avresample_set_matrix(m_avrContext, matrix_dbl, in_ch);
      av_free(matrix_dbl);
      if (ret < 0) {
        DbgLog((LOG_ERROR, 10, L"avresample_set_matrix failed, layout in: %x, out: %x, sample fmt in: %d, out: %d", buffer->dwChannelMask, dwMixingLayout, buffer->sfFormat, m_sfRemixFormat));
        goto setuperr;
      }
    }

    // Open Resample Context
    ret = avresample_open(m_avrContext);
    if (ret < 0) {
      DbgLog((LOG_ERROR, 10, L"avresample_open failed, layout in: %x, out: %x, sample fmt in: %d, out: %d", buffer->dwChannelMask, dwMixingLayout, buffer->sfFormat, m_sfRemixFormat));
      goto setuperr;
    }
  }

  if (!m_avrContext) {
    DbgLog((LOG_ERROR, 10, L"avresample context missing?"));
    return E_FAIL;
  }

  LAVAudioSampleFormat bufferFormat = (m_sfRemixFormat == SampleFormat_24) ? SampleFormat_32 : m_sfRemixFormat; // avresample always outputs 32-bit

  GrowableArray<BYTE> *pcmOut = new GrowableArray<BYTE>();
  pcmOut->Allocate(FFALIGN(buffer->nSamples, 32) * av_get_channel_layout_nb_channels(m_dwRemixLayout) * get_byte_per_sample(bufferFormat));
  BYTE *pOut = pcmOut->Ptr();

  BYTE *pIn = buffer->bBuffer->Ptr();
  ret = avresample_convert(m_avrContext, &pOut, pcmOut->GetAllocated(), buffer->nSamples, &pIn, buffer->bBuffer->GetAllocated(), buffer->nSamples);
  if (ret < 0) {
    DbgLog((LOG_ERROR, 10, L"avresample_convert failed"));
    delete pcmOut;
    return S_FALSE;
  }

  delete buffer->bBuffer;
  buffer->bBuffer = pcmOut;
  buffer->dwChannelMask = m_dwRemixLayout;
  buffer->sfFormat = bufferFormat;
  buffer->wBitsPerSample = get_byte_per_sample(m_sfRemixFormat) << 3;
  buffer->wChannels = av_get_channel_layout_nb_channels(m_dwRemixLayout);
  buffer->bBuffer->SetSize(buffer->wChannels * buffer->nSamples * get_byte_per_sample(buffer->sfFormat));

  return S_OK;
setuperr:
  avresample_free(&m_avrContext);
  m_bAVResampleFailed = TRUE;
  return E_FAIL;
}

HRESULT CLAVAudio::PostProcess(BufferDetails *buffer)
{
  int layout_channels = av_get_channel_layout_nb_channels(buffer->dwChannelMask);

  // Validate channel mask
  if (!buffer->dwChannelMask || layout_channels != buffer->wChannels) {
    buffer->dwChannelMask = get_channel_mask(buffer->wChannels);
    if (!buffer->dwChannelMask && buffer->wChannels <= 2) {
      buffer->dwChannelMask = buffer->wChannels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
    }
  }

  if (m_settings.SuppressFormatChanges) {
    if (!m_SuppressLayout) {
      m_SuppressLayout = buffer->dwChannelMask;
    } else {
      if (buffer->dwChannelMask != m_SuppressLayout && buffer->wChannels <= av_get_channel_layout_nb_channels(m_SuppressLayout)) {
        // only warn once
        if (m_dwOverrideMixer != m_SuppressLayout) {
          DbgLog((LOG_TRACE, 10, L"Channel Format change suppressed, from 0x%x to 0x%x", m_SuppressLayout, buffer->dwChannelMask));
          m_dwOverrideMixer = m_SuppressLayout;
        }
      } else if (buffer->wChannels > av_get_channel_layout_nb_channels(m_SuppressLayout)) {
        DbgLog((LOG_TRACE, 10, L"Channel count increased, allowing change from 0x%x to 0x%x", m_SuppressLayout, buffer->dwChannelMask));
        m_dwOverrideMixer = 0;
        m_SuppressLayout = buffer->dwChannelMask;
      }
    }
  }

  DWORD dwMixingLayout = m_dwOverrideMixer ? m_dwOverrideMixer : m_settings.MixingLayout;
  BOOL bMixing = (m_settings.MixingEnabled || m_dwOverrideMixer) && buffer->dwChannelMask != dwMixingLayout;
  LAVAudioSampleFormat outputFormat = GetBestAvailableSampleFormat(buffer->sfFormat);
  // Perform conversion to layout and sample format, if required
  if (bMixing || outputFormat != buffer->sfFormat) {
    PerformAVRProcessing(buffer);
  }

  // Remap to standard configurations, if requested (not in combination with mixing)
  if (!bMixing && m_settings.OutputStandardLayout) {
    if (buffer->dwChannelMask != m_DecodeLayoutSanified) {
      m_DecodeLayoutSanified = buffer->dwChannelMask;
      CheckChannelLayoutConformity(buffer->dwChannelMask);
    }
    if (m_bChannelMappingRequired) {
      ExtendedChannelMapping(buffer, m_ChannelMapOutputChannels, m_ChannelMap);
      buffer->dwChannelMask = m_ChannelMapOutputLayout;
    }
  }

  // Map to the requested 5.1 layout
  if (m_settings.Output51Legacy && buffer->dwChannelMask == AV_CH_LAYOUT_5POINT1)
    buffer->dwChannelMask = AV_CH_LAYOUT_5POINT1_BACK;
  else if (!m_settings.Output51Legacy && buffer->dwChannelMask == AV_CH_LAYOUT_5POINT1_BACK)
    buffer->dwChannelMask = AV_CH_LAYOUT_5POINT1;

  // Check if current output uses back layout, and keep it active in that case
  if (buffer->dwChannelMask == AV_CH_LAYOUT_5POINT1) {
    WAVEFORMATEX * wfe = (WAVEFORMATEX *)m_pOutput->CurrentMediaType().Format();
    if (wfe->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
      WAVEFORMATEXTENSIBLE * wfex  = (WAVEFORMATEXTENSIBLE *)wfe;
      if (wfex->dwChannelMask == AV_CH_LAYOUT_5POINT1_BACK)
        buffer->dwChannelMask = AV_CH_LAYOUT_5POINT1_BACK;
    }
  }

  // Mono -> Stereo expansion
  if (buffer->wChannels == 1 && m_settings.ExpandMono) {
    ExtendedChannelMap map = {{0,-2}, {0, -2}};
    ExtendedChannelMapping(buffer, 2, map);
    buffer->dwChannelMask = AV_CH_LAYOUT_STEREO;
  }

  // 6.1 -> 7.1 expansion
  if (m_settings.Expand61) {
    if (buffer->dwChannelMask == AV_CH_LAYOUT_6POINT1_BACK) {
      ExtendedChannelMap map = {{0,0}, {1,0}, {2,0}, {3,0}, {6,-2}, {6,-2}, {4,0}, {5,0}};
      ExtendedChannelMapping(buffer, 8, map);
      buffer->dwChannelMask = AV_CH_LAYOUT_7POINT1;
    } else if (buffer->dwChannelMask == AV_CH_LAYOUT_6POINT1) {
      ExtendedChannelMap map = {{0,0}, {1,0}, {2,0}, {3,0}, {4,-2}, {4,-2}, {5,0}, {6,0}};
      ExtendedChannelMapping(buffer, 8, map);
      buffer->dwChannelMask = AV_CH_LAYOUT_7POINT1;
    }
  }

  if (m_bVolumeStats) {
    UpdateVolumeStats(*buffer);
  }

  // Truncate 24-in-32 to real 24
  if (buffer->sfFormat == SampleFormat_32 && buffer->wBitsPerSample && buffer->wBitsPerSample <= 24) {
    Truncate32Buffer(buffer);
  }

  return S_OK;
}
