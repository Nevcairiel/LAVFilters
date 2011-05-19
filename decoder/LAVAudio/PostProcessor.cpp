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
#include "PostProcessor.h"
#include "LAVAudio.h"
#include "Media.h"

// PCM Volume Adjustment Factors, both for integer and float math
// Entrys start at 2 channel mixing, half volume
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

HRESULT CLAVAudio::CheckChannelLayoutConformity()
{
  int channels = av_get_channel_layout_nb_channels(m_DecodeLayout);

  // Every multi-channel layout needs to be stereo
  if (!CHL_CONTAINS_ALL(m_DecodeLayout, AV_CH_LAYOUT_STEREO))
    goto noprocessing;

  // Layouts with one side channel are not supported, how odd would that be
  if ((!(m_DecodeLayout & AV_CH_SIDE_LEFT) != !(m_DecodeLayout & AV_CH_SIDE_RIGHT)) || (!(m_DecodeLayout & AV_CH_BACK_LEFT) != !(m_DecodeLayout & AV_CH_BACK_RIGHT)) || (!(m_DecodeLayout & AV_CH_FRONT_LEFT_OF_CENTER) != !(m_DecodeLayout & AV_CH_FRONT_RIGHT_OF_CENTER)))
    goto noprocessing;

  // We do not know what to do with "top" channels
  if (m_DecodeLayout & (AV_CH_TOP_CENTER|AV_CH_TOP_FRONT_LEFT|AV_CH_TOP_FRONT_CENTER|AV_CH_TOP_FRONT_RIGHT|AV_CH_TOP_BACK_LEFT|AV_CH_TOP_BACK_CENTER|AV_CH_TOP_BACK_RIGHT))
    goto noprocessing;

  if (channels > 2 || channels < 6) {
    // If the layout contains side or back channels and a back center, we need to expand it to 6.1 at least
    if ((CHL_CONTAINS_ALL(m_DecodeLayout, AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER) || CHL_CONTAINS_ALL(m_DecodeLayout, AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT) || CHL_CONTAINS_ALL(m_DecodeLayout, AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)) && (m_DecodeLayout & AV_CH_BACK_CENTER))
      return Create61Conformity();

    return Create51Conformity();
  } else if ((channels == 6 && (m_DecodeLayout == AV_CH_LAYOUT_5POINT1 || m_DecodeLayout == AV_CH_LAYOUT_5POINT1_BACK))
          || (channels == 7 && (m_DecodeLayout == (AV_CH_LAYOUT_5POINT1_BACK|AV_CH_BACK_CENTER)))) {
    goto noprocessing;
  } else if (channels == 6) {
    if (m_DecodeLayout & AV_CH_BACK_CENTER)
      return Create61Conformity();
    return Create71Conformity();
  } else if (channels == 7) {
    return Create71Conformity();
  }

noprocessing:
  m_bChannelMappingRequired = FALSE;
  return S_FALSE;
}

HRESULT CLAVAudio::Create51Conformity()
{
  int ch = 0;
  ExtChMapClear(&m_ChannelMap);
  // All layouts we support have to contain L/R
  ExtChMapSet(&m_ChannelMap, 0, ch++, 0);
  ExtChMapSet(&m_ChannelMap, 1, ch++, 0);
  // Center channel
  if (m_DecodeLayout & AV_CH_FRONT_CENTER)
    ExtChMapSet(&m_ChannelMap, 2, ch++, 0);
  // LFE
  if (m_DecodeLayout & AV_CH_LOW_FREQUENCY)
    ExtChMapSet(&m_ChannelMap, 3, ch++, 0);
  // Back/Side
  if (m_DecodeLayout & (AV_CH_SIDE_LEFT|AV_CH_BACK_LEFT|AV_CH_FRONT_LEFT_OF_CENTER)) {
    ExtChMapSet(&m_ChannelMap, 4, ch++, 0);
    ExtChMapSet(&m_ChannelMap, 5, ch++, 0);
  // Back Center
  } else if (m_DecodeLayout & AV_CH_BACK_CENTER) {
    ExtChMapSet(&m_ChannelMap, 4, ch, -2);
    ExtChMapSet(&m_ChannelMap, 5, ch++, -2);
  }
  m_bChannelMappingRequired = TRUE;
  m_ChannelMapOutputChannels = 6;
  m_ChannelMapOutputLayout = AV_CH_LAYOUT_5POINT1_BACK;
  return S_OK;
}

HRESULT CLAVAudio::Create61Conformity()
{
  if (m_settings.Expand61)
    return Create71Conformity();

  int ch = 0;
  ExtChMapClear(&m_ChannelMap);
  // All layouts we support have to contain L/R
  ExtChMapSet(&m_ChannelMap, 0, ch++, 0);
  ExtChMapSet(&m_ChannelMap, 1, ch++, 0);
  // Center channel
  if (m_DecodeLayout & AV_CH_FRONT_CENTER)
    ExtChMapSet(&m_ChannelMap, 2, ch++, 0);
  // LFE
  if (m_DecodeLayout & AV_CH_LOW_FREQUENCY)
    ExtChMapSet(&m_ChannelMap, 3, ch++, 0);
  // Back channels
  if (m_DecodeLayout & (AV_CH_BACK_LEFT|AV_CH_FRONT_LEFT_OF_CENTER)) {
    ExtChMapSet(&m_ChannelMap, 4, ch++, 0);
    ExtChMapSet(&m_ChannelMap, 5, ch++, 0);
  }
  // Back Center
  if (m_DecodeLayout & AV_CH_BACK_CENTER)
    ExtChMapSet(&m_ChannelMap, 6, ch++, 0);
  // Side channels, they appear *after* the back center in the original buffer
  if (m_DecodeLayout & (AV_CH_SIDE_LEFT)) {
    ExtChMapSet(&m_ChannelMap, 4, ch++, 0);
    ExtChMapSet(&m_ChannelMap, 5, ch++, 0);
  }

  m_bChannelMappingRequired = TRUE;
  m_ChannelMapOutputChannels = 7;
  m_ChannelMapOutputLayout = AV_CH_LAYOUT_5POINT1_BACK | AV_CH_BACK_CENTER;
  return S_OK;
}

HRESULT CLAVAudio::Create71Conformity()
{
  int ch = 0;
  ExtChMapClear(&m_ChannelMap);
  // All layouts we support have to contain L/R
  ExtChMapSet(&m_ChannelMap, 0, ch++, 0);
  ExtChMapSet(&m_ChannelMap, 1, ch++, 0);
  // Center channel
  if (m_DecodeLayout & AV_CH_FRONT_CENTER)
    ExtChMapSet(&m_ChannelMap, 2, ch++, 0);
  // LFE
  if (m_DecodeLayout & AV_CH_LOW_FREQUENCY)
    ExtChMapSet(&m_ChannelMap, 3, ch++, 0);
  // Back channels
  int surr_c = 0;
  if (m_DecodeLayout & AV_CH_BACK_LEFT) surr_c++;
  if (m_DecodeLayout & AV_CH_SIDE_LEFT) surr_c++;
  if (m_DecodeLayout & AV_CH_FRONT_LEFT_OF_CENTER) surr_c++;
  // If we have two groups, all is good, just take them
  if (surr_c == 2) {
    ExtChMapSet(&m_ChannelMap, 4, ch++, 0);
    ExtChMapSet(&m_ChannelMap, 5, ch++, 0);
    ExtChMapSet(&m_ChannelMap, 6, ch++, 0);
    ExtChMapSet(&m_ChannelMap, 7, ch++, 0);
  // we have only one surround group
  } else if (surr_c == 1) {
    // Check if we have a back center we can expand into the back channel
    if (m_DecodeLayout & AV_CH_BACK_CENTER) {
      // Side channels are *after* the back center
      if (m_DecodeLayout & AV_CH_SIDE_LEFT) {
        ExtChMapSet(&m_ChannelMap, 4, ch, -2);
        ExtChMapSet(&m_ChannelMap, 5, ch++, -2);
        ExtChMapSet(&m_ChannelMap, 6, ch++, 0);
        ExtChMapSet(&m_ChannelMap, 7, ch++, 0);
      } else {
        ExtChMapSet(&m_ChannelMap, 6, ch++, 0);
        ExtChMapSet(&m_ChannelMap, 7, ch++, 0);
        ExtChMapSet(&m_ChannelMap, 4, ch, -2);
        ExtChMapSet(&m_ChannelMap, 5, ch++, -2);
      }
    // No other back channels, just write our surround channels into the side, and make it effectively 5.1. In practice, we should never get here.
    } else {
      DbgLog((LOG_ERROR, 10, L"::Create71Conformity(): Building 7.1 layout with only 5.1 channels - original mask was: 0x%x", m_DecodeLayout));
      ExtChMapSet(&m_ChannelMap, 6, ch++, 0);
      ExtChMapSet(&m_ChannelMap, 7, ch++, 0);
    }
  } else {
    DbgLog((LOG_ERROR, 10, L"::Create71Conformity(): Building 7.1 layout with %d surround groups - original mask was: 0x%x", surr_c, m_DecodeLayout));
  }

  m_bChannelMappingRequired = TRUE;
  m_ChannelMapOutputChannels = 8;
  m_ChannelMapOutputLayout = AV_CH_LAYOUT_7POINT1;
  return S_OK;
}

HRESULT CLAVAudio::PostProcess(BufferDetails *buffer)
{
  // Validate channel mask and remap channels if necessary
  if (!buffer->dwChannelMask) {
    buffer->dwChannelMask = get_channel_mask(buffer->wChannels);
  } else {
    int layout_channels = av_get_channel_layout_nb_channels(buffer->dwChannelMask);
    if (layout_channels != buffer->wChannels) {
      buffer->dwChannelMask = get_channel_mask(buffer->wChannels);
    } else if (m_settings.OutputStandardLayout) {
      if (buffer->dwChannelMask != m_DecodeLayout) {
        m_DecodeLayout = buffer->dwChannelMask;
        CheckChannelLayoutConformity();
      }
      if (m_bChannelMappingRequired) {
        ExtendedChannelMapping(buffer, m_ChannelMapOutputChannels, m_ChannelMap);
        buffer->dwChannelMask = m_ChannelMapOutputLayout;
      }
    }
  }

  // Mono -> Stereo expansion
  if (buffer->wChannels == 1 && m_settings.ExpandMono) {
    ExtendedChannelMap map = {{0,-2}, {0, -2}};
    ExtendedChannelMapping(buffer, 2, map);
    buffer->dwChannelMask = AV_CH_LAYOUT_STEREO;
  }

  // 6.1 -> 7.1 expansion
  if (m_settings.Expand61 && buffer->dwChannelMask == (AV_CH_LAYOUT_5POINT1_BACK|AV_CH_BACK_CENTER)) {
    ExtendedChannelMap map = {{0,0}, {1,0}, {2,0}, {3,0}, {6,-2}, {6,-2}, {4,0}, {5,0}};
    ExtendedChannelMapping(buffer, 8, map);
    buffer->dwChannelMask = AV_CH_LAYOUT_7POINT1;
  }

  if (m_bVolumeStats) {
    UpdateVolumeStats(*buffer);
  }
  return S_OK;
}
