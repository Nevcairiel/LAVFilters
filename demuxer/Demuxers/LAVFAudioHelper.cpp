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
 *
 *  Contributions by Ti-BEN from the XBMC DSPlayer Project, also under GPLv2
 */

#include "stdafx.h"
#include "moreuuids.h"
#include "BaseDemuxer.h"
#include "LAVFAudioHelper.h"
#include "LAVFUtils.h"

#include "ExtradataParser.h"

#include <vector>

CLAVFAudioHelper g_AudioHelper;

// Map codec ids to media subtypes
static FormatMapping audio_map[] = {
  { AV_CODEC_ID_AC3,        &MEDIASUBTYPE_DOLBY_AC3,         WAVE_FORMAT_DOLBY_AC3,  nullptr },
  { AV_CODEC_ID_AAC,        &MEDIASUBTYPE_AAC,               WAVE_FORMAT_AAC,        nullptr },
  { AV_CODEC_ID_AAC_LATM,   &MEDIASUBTYPE_LATM_AAC,          WAVE_FORMAT_LATM_AAC,   nullptr },
  { AV_CODEC_ID_DTS,        &MEDIASUBTYPE_WAVE_DTS,          0,                      nullptr },
  { AV_CODEC_ID_EAC3,       &MEDIASUBTYPE_DOLBY_DDPLUS,      0,                      nullptr },
  { AV_CODEC_ID_TRUEHD,     &MEDIASUBTYPE_DOLBY_TRUEHD,      0,                      nullptr },
  { AV_CODEC_ID_MLP,        &MEDIASUBTYPE_MLP,               WAVE_FORMAT_MLP,        nullptr },
  { AV_CODEC_ID_VORBIS,     &MEDIASUBTYPE_Vorbis2,           0,                      &FORMAT_VorbisFormat2 },
  { AV_CODEC_ID_MP1,        &MEDIASUBTYPE_MPEG1AudioPayload, WAVE_FORMAT_MPEG,       nullptr },
  { AV_CODEC_ID_MP2,        &MEDIASUBTYPE_MPEG2_AUDIO,       WAVE_FORMAT_MPEG,       nullptr },
  { AV_CODEC_ID_MP3,        &MEDIASUBTYPE_MP3,               WAVE_FORMAT_MPEGLAYER3, nullptr },
  { AV_CODEC_ID_PCM_BLURAY, &MEDIASUBTYPE_BD_LPCM_AUDIO,     0,                      nullptr },
  { AV_CODEC_ID_PCM_DVD,    &MEDIASUBTYPE_DVD_LPCM_AUDIO,    0,                      nullptr },
  { AV_CODEC_ID_PCM_S16LE,  &MEDIASUBTYPE_PCM,               WAVE_FORMAT_PCM,        nullptr },
  { AV_CODEC_ID_PCM_S24LE,  &MEDIASUBTYPE_PCM,               WAVE_FORMAT_PCM,        nullptr },
  { AV_CODEC_ID_PCM_S32LE,  &MEDIASUBTYPE_PCM,               WAVE_FORMAT_PCM,        nullptr },
  { AV_CODEC_ID_PCM_F32LE,  &MEDIASUBTYPE_IEEE_FLOAT,        WAVE_FORMAT_IEEE_FLOAT, nullptr },
  { AV_CODEC_ID_PCM_S16LE_PLANAR, &MEDIASUBTYPE_PCM,         WAVE_FORMAT_PCM,        nullptr },
  { AV_CODEC_ID_PCM_S24LE_PLANAR, &MEDIASUBTYPE_PCM,         WAVE_FORMAT_PCM,        nullptr },
  { AV_CODEC_ID_PCM_S32LE_PLANAR, &MEDIASUBTYPE_PCM,         WAVE_FORMAT_PCM,        nullptr },
  { AV_CODEC_ID_WMAV1,      &MEDIASUBTYPE_MSAUDIO1,          WAVE_FORMAT_MSAUDIO1,   nullptr },
  { AV_CODEC_ID_WMAV2,      &MEDIASUBTYPE_WMAUDIO2,          WAVE_FORMAT_WMAUDIO2,   nullptr },
  { AV_CODEC_ID_WMAPRO,     &MEDIASUBTYPE_WMAUDIO3,          WAVE_FORMAT_WMAUDIO3,   nullptr },
  { AV_CODEC_ID_ADPCM_IMA_AMV, &MEDIASUBTYPE_IMA_AMV,        0,                      nullptr },
  { AV_CODEC_ID_FLAC,       &MEDIASUBTYPE_FLAC_FRAMED,       WAVE_FORMAT_FLAC,       nullptr },
  { AV_CODEC_ID_COOK,       &MEDIASUBTYPE_COOK,              WAVE_FORMAT_COOK,       nullptr },
  { AV_CODEC_ID_SIPR,       &MEDIASUBTYPE_SIPR,              WAVE_FORMAT_SIPR,       nullptr },
  { AV_CODEC_ID_RA_288,     &MEDIASUBTYPE_28_8,              WAVE_FORMAT_28_8,       nullptr },
  { AV_CODEC_ID_RA_144,     &MEDIASUBTYPE_14_4,              WAVE_FORMAT_14_4,       nullptr },
  { AV_CODEC_ID_RALF,       &MEDIASUBTYPE_RALF,              WAVE_FORMAT_RALF,       nullptr },
  { AV_CODEC_ID_ALAC,       &MEDIASUBTYPE_ALAC,              0,                      nullptr },
  { AV_CODEC_ID_MP4ALS,     &MEDIASUBTYPE_ALS,               0,                      nullptr },
  { AV_CODEC_ID_OPUS,       &MEDIASUBTYPE_OPUS,              0,                      nullptr },
  { AV_CODEC_ID_NELLYMOSER, &MEDIASUBTYPE_NELLYMOSER,        0,                      nullptr },
  { AV_CODEC_ID_PCM_ALAW,   &MEDIASUBTYPE_ALAW,              WAVE_FORMAT_ALAW,       nullptr },
  { AV_CODEC_ID_PCM_MULAW,  &MEDIASUBTYPE_MULAW,             WAVE_FORMAT_MULAW,      nullptr },
  { AV_CODEC_ID_GSM_MS,     &MEDIASUBTYPE_MSGSM610,          WAVE_FORMAT_GSM610,     nullptr },
  { AV_CODEC_ID_ADPCM_MS,   &MEDIASUBTYPE_ADPCM_MS,          WAVE_FORMAT_ADPCM,      nullptr },
  { AV_CODEC_ID_TRUESPEECH, &MEDIASUBTYPE_TRUESPEECH,        WAVE_FORMAT_DSPGROUP_TRUESPEECH, nullptr },
  { AV_CODEC_ID_TAK,        &MEDIASUBTYPE_TAK,               0,                      nullptr },
  { AV_CODEC_ID_S302M,      &MEDIASUBTYPE_AES3,              0,                      nullptr },
  { AV_CODEC_ID_PCM_S16BE,  &MEDIASUBTYPE_PCM_TWOS,          0,                      nullptr },
  { AV_CODEC_ID_PCM_S16BE_PLANAR, &MEDIASUBTYPE_PCM_TWOS,    0,                      nullptr },
  { AV_CODEC_ID_PCM_S24BE,  &MEDIASUBTYPE_PCM_IN24,          0,                      nullptr },
  { AV_CODEC_ID_PCM_S32BE,  &MEDIASUBTYPE_PCM_IN32,          0,                      nullptr },
  { AV_CODEC_ID_QDM2,       &MEDIASUBTYPE_QDM2,              WAVE_FORMAT_QDESIGN_MUSIC, nullptr },
  { AV_CODEC_ID_ATRAC1,     &MEDIASUBTYPE_ATRC,              WAVE_FORMAT_ATRC,       nullptr },
  { AV_CODEC_ID_ATRAC3,     &MEDIASUBTYPE_ATRC,              WAVE_FORMAT_ATRC,       nullptr },
  { AV_CODEC_ID_ATRAC3P,    &MEDIASUBTYPE_ATRAC3P,           WAVE_FORMAT_ATRAC3P,    nullptr },
  { AV_CODEC_ID_DSD_LSBF,   &MEDIASUBTYPE_DSDL,              0,                      nullptr },
  { AV_CODEC_ID_DSD_MSBF,   &MEDIASUBTYPE_DSDM,              0,                      nullptr },
  { AV_CODEC_ID_DSD_LSBF_PLANAR, &MEDIASUBTYPE_DSD1,         0,                      nullptr },
  { AV_CODEC_ID_DSD_MSBF_PLANAR, &MEDIASUBTYPE_DSD8,         0,                      nullptr },
};

CMediaType CLAVFAudioHelper::initAudioType(AVCodecParameters *codecpar, unsigned int &codecTag, std::string container)
{
  CMediaType mediaType;
  mediaType.InitMediaType();
  mediaType.majortype = MEDIATYPE_Audio;
  mediaType.subtype = FOURCCMap(codecTag);
  mediaType.formattype = FORMAT_WaveFormatEx; //default value
  mediaType.SetSampleSize(256000);

  // Check against values from the map above
  for(unsigned i = 0; i < countof(audio_map); ++i) {
    if (audio_map[i].codec == codecpar->codec_id) {
      if (audio_map[i].subtype)
        mediaType.subtype = *audio_map[i].subtype;
      if (audio_map[i].codecTag)
        codecTag = audio_map[i].codecTag;
      if (audio_map[i].format)
        mediaType.formattype = *audio_map[i].format;
      break;
    }
  }

  // special cases
  switch(codecpar->codec_id)
  {
  case AV_CODEC_ID_PCM_F64LE:
    // Qt PCM
    if (codecTag == MKTAG('f', 'l', '6', '4')) mediaType.subtype = MEDIASUBTYPE_PCM_FL64_le;
    break;
  }
  return mediaType;
}

WAVEFORMATEX *CLAVFAudioHelper::CreateWVFMTEX(const AVStream *avstream, ULONG *size) {
  WAVEFORMATEX *wvfmt = (WAVEFORMATEX *)CoTaskMemAlloc(sizeof(WAVEFORMATEX) + avstream->codecpar->extradata_size);
  if (!wvfmt)
    return nullptr;
  memset(wvfmt, 0, sizeof(WAVEFORMATEX));

  wvfmt->wFormatTag = avstream->codecpar->codec_tag;

  wvfmt->nChannels = avstream->codecpar->channels ? avstream->codecpar->channels : 2;
  wvfmt->nSamplesPerSec = avstream->codecpar->sample_rate ? avstream->codecpar->sample_rate : 48000;
  wvfmt->nAvgBytesPerSec = (DWORD)(avstream->codecpar->bit_rate / 8);

  if(avstream->codecpar->codec_id == AV_CODEC_ID_AAC || avstream->codecpar->codec_id == AV_CODEC_ID_AAC_LATM) {
    wvfmt->wBitsPerSample = 0;
    wvfmt->nBlockAlign = 1;
  } else {
    wvfmt->wBitsPerSample = get_bits_per_sample(avstream->codecpar);

    if ( avstream->codecpar->block_align > 0 ) {
      wvfmt->nBlockAlign = avstream->codecpar->block_align;
    } else {
      if ( wvfmt->wBitsPerSample == 0 ) {
        DbgOutString(L"BitsPerSample is 0, no good!");
      }
      wvfmt->nBlockAlign = (WORD)(wvfmt->nChannels * av_get_bytes_per_sample((AVSampleFormat)avstream->codecpar->format));
    }
  }
  if (!wvfmt->nAvgBytesPerSec)
    wvfmt->nAvgBytesPerSec = (wvfmt->nSamplesPerSec * wvfmt->nChannels * wvfmt->wBitsPerSample) >> 3;

  if (avstream->codecpar->extradata_size > 0) {
    wvfmt->cbSize = avstream->codecpar->extradata_size;
    memcpy((BYTE *)wvfmt + sizeof(WAVEFORMATEX), avstream->codecpar->extradata, avstream->codecpar->extradata_size);
  }

  *size = sizeof(WAVEFORMATEX) + avstream->codecpar->extradata_size;
  return wvfmt;
}

WAVEFORMATEXFFMPEG *CLAVFAudioHelper::CreateWVFMTEX_FF(const AVStream *avstream, ULONG *size) {
  WAVEFORMATEX *wvfmt = CreateWVFMTEX(avstream, size);
  if (!wvfmt)
    return nullptr;

  const size_t diff_size = sizeof(WAVEFORMATEXFFMPEG) - sizeof(WAVEFORMATEX);
  WAVEFORMATEXFFMPEG *wfex_ff = (WAVEFORMATEXFFMPEG *)CoTaskMemAlloc(diff_size + *size);
  if (!wfex_ff)
    return nullptr;
  memset(wfex_ff, 0, sizeof(WAVEFORMATEXFFMPEG));
  memcpy(&wfex_ff->wfex, wvfmt, *size);

  wfex_ff->nCodecId = avstream->codecpar->codec_id;
  CoTaskMemFree(wvfmt);

  *size = sizeof(WAVEFORMATEXFFMPEG) + wfex_ff->wfex.cbSize;
  return wfex_ff;
}

WAVEFORMATEX_HDMV_LPCM *CLAVFAudioHelper::CreateWVFMTEX_LPCM(const AVStream *avstream, ULONG *size) {
  WAVEFORMATEX *wvfmt = CreateWVFMTEX(avstream, size);

  WAVEFORMATEX_HDMV_LPCM *lpcm = (WAVEFORMATEX_HDMV_LPCM *)CoTaskMemAlloc(sizeof(WAVEFORMATEX_HDMV_LPCM));
  memset(lpcm, 0, sizeof(WAVEFORMATEX_HDMV_LPCM));
  memcpy(lpcm, wvfmt, sizeof(WAVEFORMATEX));

  lpcm->cbSize = sizeof(WAVEFORMATEX_HDMV_LPCM) - sizeof(WAVEFORMATEX);
  BYTE channel_conf = 0;
  switch (avstream->codecpar->channel_layout) {
  case AV_CH_LAYOUT_MONO:
    channel_conf = 1;
    break;
  case AV_CH_LAYOUT_STEREO:
    channel_conf = 3;
    break;
  case AV_CH_LAYOUT_SURROUND:
    channel_conf = 4;
    break;
  case AV_CH_LAYOUT_2_1:
    channel_conf = 5;
    break;
  case AV_CH_LAYOUT_4POINT0:
    channel_conf = 6;
    break;
  case AV_CH_LAYOUT_2_2:
    channel_conf = 7;
    break;
  case AV_CH_LAYOUT_5POINT0:
    channel_conf = 8;
    break;
  case AV_CH_LAYOUT_5POINT1:
    channel_conf = 9;
    break;
  case AV_CH_LAYOUT_7POINT0:
    channel_conf = 10;
    break;
  case AV_CH_LAYOUT_7POINT1:
    channel_conf = 11;
    break;
  default:
    channel_conf = 0;
  }
  lpcm->channel_conf = channel_conf;

  CoTaskMemFree(wvfmt);

  *size = sizeof(WAVEFORMATEX_HDMV_LPCM);
  return lpcm;
}

WAVEFORMATEXTENSIBLE *CLAVFAudioHelper::CreateWFMTEX_RAW_PCM(const AVStream *avstream, ULONG *size, const GUID subtype, ULONG *samplesize)
{
  WAVEFORMATEXTENSIBLE *wfex = (WAVEFORMATEXTENSIBLE *)CoTaskMemAlloc(sizeof(WAVEFORMATEXTENSIBLE));
  if (!wfex)
    return nullptr;
  memset(wfex, 0, sizeof(*wfex));

  WAVEFORMATEX *wfe = &wfex->Format;
  wfe->wFormatTag = (WORD)subtype.Data1;
  wfe->nChannels = avstream->codecpar->channels;
  wfe->nSamplesPerSec = avstream->codecpar->sample_rate;
  if (avstream->codecpar->format == AV_SAMPLE_FMT_S32 && avstream->codecpar->bits_per_raw_sample > 0) {
    wfe->wBitsPerSample = avstream->codecpar->bits_per_raw_sample > 24 ? 32 : (avstream->codecpar->bits_per_raw_sample > 16 ? 24 : 16);
  } else {
    wfe->wBitsPerSample = av_get_bytes_per_sample((AVSampleFormat)avstream->codecpar->format) << 3;
  }
  wfe->nBlockAlign = wfe->nChannels * wfe->wBitsPerSample / 8;
  wfe->nAvgBytesPerSec = wfe->nSamplesPerSec * wfe->nBlockAlign;

  DWORD dwChannelMask = 0;
  if((wfe->wBitsPerSample > 16 || wfe->nSamplesPerSec > 48000) && wfe->nChannels <= 2) {
    dwChannelMask = wfe->nChannels == 2 ? (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT) : SPEAKER_FRONT_CENTER;
  } else if (wfe->nChannels > 2) {
    dwChannelMask = (DWORD)avstream->codecpar->channel_layout;
    if (!dwChannelMask) {
      dwChannelMask = (DWORD)av_get_default_channel_layout(wfe->nChannels);
    }
  }

  if(dwChannelMask) {
    wfex->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfex->Format.cbSize = sizeof(*wfex) - sizeof(wfex->Format);
    wfex->dwChannelMask = dwChannelMask;
    if (avstream->codecpar->format == AV_SAMPLE_FMT_S32 && avstream->codecpar->bits_per_raw_sample > 0) {
      wfex->Samples.wValidBitsPerSample = avstream->codecpar->bits_per_raw_sample;
    } else {
      wfex->Samples.wValidBitsPerSample = wfex->Format.wBitsPerSample;
    }
    wfex->SubFormat = subtype;
    *size = sizeof(WAVEFORMATEXTENSIBLE);
  } else {
    *size = sizeof(WAVEFORMATEX);
  }

  *samplesize = wfe->wBitsPerSample * wfe->nChannels / 8;

  return wfex;
}

MPEG1WAVEFORMAT *CLAVFAudioHelper::CreateMP1WVFMT(const AVStream *avstream, ULONG *size) {
  WAVEFORMATEX *wvfmt = CreateWVFMTEX(avstream, size);
  if (!wvfmt)
    return nullptr;

  MPEG1WAVEFORMAT *mpwvfmt = (MPEG1WAVEFORMAT *)CoTaskMemAlloc(sizeof(MPEG1WAVEFORMAT));
  if (!mpwvfmt)
    return nullptr;
  memset(mpwvfmt, 0, sizeof(MPEG1WAVEFORMAT));
  memcpy(&mpwvfmt->wfx, wvfmt, sizeof(WAVEFORMATEX));

  mpwvfmt->dwHeadBitrate = (DWORD)avstream->codecpar->bit_rate;
  mpwvfmt->fwHeadMode = avstream->codecpar->channels == 1 ? ACM_MPEG_SINGLECHANNEL : ACM_MPEG_DUALCHANNEL;
  mpwvfmt->fwHeadLayer = (avstream->codecpar->codec_id == AV_CODEC_ID_MP1) ? ACM_MPEG_LAYER1 : ACM_MPEG_LAYER2;

  if (avstream->codecpar->sample_rate == 0) {
    avstream->codecpar->sample_rate = 48000;
  }
  mpwvfmt->wfx.wFormatTag = WAVE_FORMAT_MPEG;
  mpwvfmt->wfx.nBlockAlign = WORD((avstream->codecpar->codec_id == AV_CODEC_ID_MP1)
        ? (12 * avstream->codecpar->bit_rate / avstream->codecpar->sample_rate) * 4
        : 144 * avstream->codecpar->bit_rate / avstream->codecpar->sample_rate);

  mpwvfmt->wfx.cbSize = sizeof(MPEG1WAVEFORMAT) - sizeof(WAVEFORMATEX);

  CoTaskMemFree(wvfmt);

  *size = sizeof(MPEG1WAVEFORMAT);
  return mpwvfmt;
}

VORBISFORMAT *CLAVFAudioHelper::CreateVorbis(const AVStream *avstream, ULONG *size) {
  VORBISFORMAT *vfmt = (VORBISFORMAT *)CoTaskMemAlloc(sizeof(VORBISFORMAT));
  if (!vfmt)
    return nullptr;
  memset(vfmt, 0, sizeof(VORBISFORMAT));

  vfmt->nChannels = avstream->codecpar->channels;
  vfmt->nSamplesPerSec = avstream->codecpar->sample_rate;
  vfmt->nAvgBitsPerSec = (DWORD)avstream->codecpar->bit_rate;
  vfmt->nMinBitsPerSec = vfmt->nMaxBitsPerSec = (DWORD)-1;

  *size = sizeof(VORBISFORMAT);
  return vfmt;
}

VORBISFORMAT2 *CLAVFAudioHelper::CreateVorbis2(const AVStream *avstream, ULONG *size) {
  BYTE *p = avstream->codecpar->extradata;
  std::vector<int> sizes;
  // read the number of blocks, and then the sizes of the individual blocks
  for(BYTE n = *p++; n > 0; n--) {
    int size = 0;
    // Xiph Lacing
    do { size += *p; } while (*p++ == 0xFF);
    sizes.push_back(size);
  }
  
  int totalsize = 0;
  for(unsigned int i = 0; i < sizes.size(); i++)
    totalsize += sizes[i];

  // Get the size of the last block
  sizes.push_back(avstream->codecpar->extradata_size - (int)(p - avstream->codecpar->extradata) - totalsize);
  totalsize += sizes[sizes.size()-1];

  // 3 blocks is the currently valid Vorbis format
  if(sizes.size() == 3) {
    VORBISFORMAT2* pvf2 = (VORBISFORMAT2*)CoTaskMemAlloc(sizeof(VORBISFORMAT2) + totalsize);
    if (!pvf2)
      return nullptr;
    memset(pvf2, 0, sizeof(VORBISFORMAT2));
    
    pvf2->Channels = avstream->codecpar->channels;
    pvf2->SamplesPerSec = avstream->codecpar->sample_rate;
    pvf2->BitsPerSample = get_bits_per_sample(avstream->codecpar);
    
    BYTE *p2 = (BYTE *)pvf2 + sizeof(VORBISFORMAT2);
    for(unsigned int i = 0; i < sizes.size(); p += sizes[i], p2 += sizes[i], i++) {
      memcpy(p2, p, pvf2->HeaderSize[i] = sizes[i]);
    }

    *size = sizeof(VORBISFORMAT2) + totalsize;
    return pvf2;
  }
  *size = 0;
  return nullptr;
}
