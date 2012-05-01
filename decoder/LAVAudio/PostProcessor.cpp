/*
 *      Copyright (C) 2010-2012 Hendrik Leppkes
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
  m_ChannelMapOutputLayout = AV_CH_LAYOUT_5POINT1_BACK;
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

static DWORD sanitize_mask(DWORD mask, CodecID codec)
{
  DWORD newmask = mask;
  // Alot of codecs set 6.1/6.0 wrong..
  // Only these codecs we can trust to properly set BL/BR + BC layouts
  if (codec != CODEC_ID_DTS) {
    // 6.1
    if (mask == (AV_CH_LAYOUT_5POINT1_BACK|AV_CH_BACK_CENTER))
      newmask = AV_CH_LAYOUT_5POINT1|AV_CH_BACK_CENTER;
    // 6.0
    if (mask == (AV_CH_LAYOUT_5POINT0_BACK|AV_CH_BACK_CENTER))
      newmask = AV_CH_LAYOUT_5POINT0|AV_CH_BACK_CENTER;
  }
  // The reverse, TrueHD sets SL/SR when it actually should be BL/BR
  if (codec == CODEC_ID_TRUEHD) {
    // 6.1
    if(mask == (AV_CH_LAYOUT_5POINT1|AV_CH_BACK_CENTER))
      newmask = AV_CH_LAYOUT_5POINT1_BACK|AV_CH_BACK_CENTER;
    // 6.0
    if(mask == (AV_CH_LAYOUT_5POINT0|AV_CH_BACK_CENTER))
      newmask = AV_CH_LAYOUT_5POINT0_BACK|AV_CH_BACK_CENTER;
  }

  // Prefer the 5.1 BACK mask
  if (mask == AV_CH_LAYOUT_5POINT1)
    newmask = AV_CH_LAYOUT_5POINT1_BACK;

  return newmask;
}

template <LAVAudioSampleFormat in, LAVAudioSampleFormat out>
static void SampleConvert(const BYTE *pPCMIn, BYTE *pPCMOut, unsigned samples, unsigned channels)
{
  unsigned i, ch;

  const int bppIn  = get_byte_per_sample(in);
  const int bppOut = get_byte_per_sample(out);

  for (i = 0; i < samples; i++) {
    for (ch = 0; ch < channels; ch++) {
      if (in == SampleFormat_16) {
        int32_t value = *(int16_t *)pPCMIn;
        switch (out) {
        case SampleFormat_24: AV_WL24(pPCMOut, ((int32_t)value << 8));        break;
        case SampleFormat_32: *(int32_t *)pPCMOut = ((int32_t)value << 16);   break;
        case SampleFormat_U8: *pPCMOut = (value >> 8) + 0x80;                 break;
        case SampleFormat_FP32: *(float *)pPCMOut = value * (1.0f / (1U<<15)); break;
        }
      } else if (in == SampleFormat_24 || in == SampleFormat_32) {
        int32_t value = 0;
        if (in == SampleFormat_24)
          value = (pPCMIn[0] << 8) + (pPCMIn[1] << 16) + (pPCMIn[2] << 24);
        else
          value = *(int32_t *)pPCMIn;
        switch (out) {
        case SampleFormat_16: *(int16_t *)pPCMOut = value >> 16;               break;
        case SampleFormat_24: AV_WL24(pPCMOut, value >> 8);                    break;
        case SampleFormat_32: *(int32_t *)pPCMOut = value;                     break;
        case SampleFormat_U8: *pPCMOut = (value >> 24) + 0x80;                 break;
        case SampleFormat_FP32: *(float *)pPCMOut = value * (1.0f / (1U<<31)); break;
        }
      } else if (in == SampleFormat_U8) {
        int32_t value = *pPCMIn - 0x80;
        switch (out) {
        case SampleFormat_16: *(int16_t *)pPCMOut = value << 8;                break;
        case SampleFormat_24: AV_WL24(pPCMOut, value << 16);                   break;
        case SampleFormat_32: *(int32_t *)pPCMOut = value << 24;               break;
        case SampleFormat_FP32: *(float *)pPCMOut = value * (1.0f / (1U<<7));   break;
        }
      } else if (in == SampleFormat_FP32) {
        float value = *(float *)pPCMIn;
        switch (out) {
        case SampleFormat_16: *(int16_t *)pPCMOut = av_clip_int16(int(value * (1U<<15)));             break;
        case SampleFormat_24: AV_WL24(pPCMOut, av_clip(int(value * (1U<<23)), INT24_MIN, INT24_MAX)); break;
        case SampleFormat_32: *(int32_t *)pPCMOut = av_clipl_int32(int64_t(value * (1U<<31)));        break;
        case SampleFormat_U8: *pPCMOut = av_clip_uint8(int(value * (1U<<7)) + 0x80);                  break;
        }
      }
      pPCMIn += bppIn;
      pPCMOut += bppOut;
    }
  }
}

#define CONV_MAP(in, out)  \
  if (pcm->sfFormat == in  && outputFormat == out) { \
    SampleConvert<in, out>(pcm->bBuffer->Ptr(), pcmOut->Ptr(), pcm->nSamples, pcm->wChannels); \
  }

HRESULT CLAVAudio::ConvertSampleFormat(BufferDetails *pcm, LAVAudioSampleFormat outputFormat)
{
  const unsigned uSampleSize = get_byte_per_sample(outputFormat);

  // New Output Buffer
  GrowableArray<BYTE> *pcmOut = new GrowableArray<BYTE>();
  pcmOut->SetSize(pcm->wChannels * pcm->nSamples * uSampleSize);

       CONV_MAP(SampleFormat_16, SampleFormat_24)
  else CONV_MAP(SampleFormat_16, SampleFormat_32)
  else CONV_MAP(SampleFormat_16, SampleFormat_U8)
  else CONV_MAP(SampleFormat_16, SampleFormat_FP32)
  else CONV_MAP(SampleFormat_24, SampleFormat_16)
  else CONV_MAP(SampleFormat_24, SampleFormat_32)
  else CONV_MAP(SampleFormat_24, SampleFormat_U8)
  else CONV_MAP(SampleFormat_24, SampleFormat_FP32)
  else CONV_MAP(SampleFormat_32, SampleFormat_16)
  else CONV_MAP(SampleFormat_32, SampleFormat_24)
  else CONV_MAP(SampleFormat_32, SampleFormat_U8)
  else CONV_MAP(SampleFormat_32, SampleFormat_FP32)
  else CONV_MAP(SampleFormat_FP32, SampleFormat_16)
  else CONV_MAP(SampleFormat_FP32, SampleFormat_24)
  else CONV_MAP(SampleFormat_FP32, SampleFormat_32)
  else CONV_MAP(SampleFormat_FP32, SampleFormat_U8)
  else CONV_MAP(SampleFormat_U8, SampleFormat_16)
  else CONV_MAP(SampleFormat_U8, SampleFormat_24)
  else CONV_MAP(SampleFormat_U8, SampleFormat_32)
  else CONV_MAP(SampleFormat_U8, SampleFormat_FP32)
  else ASSERT(0);

  // Apply changes to buffer
  delete pcm->bBuffer;
  pcm->bBuffer        = pcmOut;
  pcm->sfFormat       = outputFormat;
  pcm->wBitsPerSample = get_byte_per_sample(outputFormat) << 3;

  return S_OK;
}

HRESULT CLAVAudio::Truncate32Buffer(BufferDetails *buffer)
{
  ASSERT(buffer->sfFormat == SampleFormat_32 && buffer->wBitsPerSample <= 24);
  GrowableArray<BYTE> *pcmOut = new GrowableArray<BYTE>();

  const int bytes_per_sample = buffer->wBitsPerSample > 16 ? 3 : 2;
  const int skip = 4 - bytes_per_sample;

  const DWORD size = (buffer->nSamples * buffer->wChannels) * bytes_per_sample;
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

HRESULT CLAVAudio::PostProcess(BufferDetails *buffer)
{
  LAVAudioSampleFormat outputFormat = GetBestAvailableSampleFormat(buffer->sfFormat);
  if (outputFormat != buffer->sfFormat) {
    ConvertSampleFormat(buffer, outputFormat);
  }

  buffer->dwChannelMask = sanitize_mask(buffer->dwChannelMask, m_nCodecId);

  int layout_channels = av_get_channel_layout_nb_channels(buffer->dwChannelMask);

  // Validate channel mask
  if (!buffer->dwChannelMask || layout_channels != buffer->wChannels) {
    buffer->dwChannelMask = get_channel_mask(buffer->wChannels);
  }

  // Remap to standard configurations, if requested
  if (m_settings.OutputStandardLayout) {
    if (buffer->dwChannelMask != m_DecodeLayoutSanified) {
      m_DecodeLayoutSanified = buffer->dwChannelMask;
      CheckChannelLayoutConformity(buffer->dwChannelMask);
    }
    if (m_bChannelMappingRequired) {
      ExtendedChannelMapping(buffer, m_ChannelMapOutputChannels, m_ChannelMap);
      buffer->dwChannelMask = m_ChannelMapOutputLayout;
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

  if (buffer->sfFormat == SampleFormat_32 && buffer->wBitsPerSample && buffer->wBitsPerSample <= 24) {
    Truncate32Buffer(buffer);
  }
  return S_OK;
}
