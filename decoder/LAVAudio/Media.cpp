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

#include <MMReg.h>

#include "moreuuids.h"

typedef struct {
  const CLSID*        clsMinorType;
  const enum CodecID  nFFCodec;
} FFMPEG_SUBTYPE_MAP;

// Map Media Subtype <> FFMPEG Codec Id
static const FFMPEG_SUBTYPE_MAP lavc_audio_codecs[] = {
  // AAC
  { &MEDIASUBTYPE_AAC,          CODEC_ID_AAC      },
  { &MEDIASUBTYPE_LATM_AAC,     CODEC_ID_AAC_LATM },
  { &MEDIASUBTYPE_MP4A,         CODEC_ID_AAC      },
  { &MEDIASUBTYPE_mp4a,         CODEC_ID_AAC      },
  { &MEDIASUBTYPE_AAC_ADTS,     CODEC_ID_AAC      },

  // Dolby
  { &MEDIASUBTYPE_DOLBY_AC3,    CODEC_ID_AC3      },
  { &MEDIASUBTYPE_DOLBY_DDPLUS, CODEC_ID_EAC3     },
  { &MEDIASUBTYPE_DOLBY_DDPLUS_ARCSOFT, CODEC_ID_EAC3 },
  { &MEDIASUBTYPE_DOLBY_TRUEHD, CODEC_ID_TRUEHD   },
  { &MEDIASUBTYPE_WAVE_DOLBY_AC3, CODEC_ID_AC3    },

  // DTS
  { &MEDIASUBTYPE_DTS,          CODEC_ID_DTS      },
  { &MEDIASUBTYPE_DTS_HD,       CODEC_ID_DTS      },
  { &MEDIASUBTYPE_WAVE_DTS,     CODEC_ID_DTS      },

  // MPEG Audio
  { &MEDIASUBTYPE_MPEG1Packet,  CODEC_ID_MP1      },
  { &MEDIASUBTYPE_MPEG1Payload, CODEC_ID_MP1      },
  { &MEDIASUBTYPE_MPEG1AudioPayload, CODEC_ID_MP1 },
  { &MEDIASUBTYPE_MPEG2_AUDIO,  CODEC_ID_MP2      },
  { &MEDIASUBTYPE_MP3,          CODEC_ID_MP3      },

  // FLAC
  { &MEDIASUBTYPE_FLAC,         CODEC_ID_FLAC     },
  { &MEDIASUBTYPE_FLAC_FRAMED,  CODEC_ID_FLAC     },

  // Ogg Vorbis
  { &MEDIASUBTYPE_Vorbis2,      CODEC_ID_VORBIS   },

  // Other Lossless formats
  { &MEDIASUBTYPE_TTA1,         CODEC_ID_TTA      },
  { &MEDIASUBTYPE_WAVPACK4,     CODEC_ID_WAVPACK  },
  { &MEDIASUBTYPE_MLP,          CODEC_ID_MLP      },

  // BluRay LPCM
  { &MEDIASUBTYPE_DVD_LPCM_AUDIO, CODEC_ID_PCM_DVD },
  { &MEDIASUBTYPE_BD_LPCM_AUDIO,   CODEC_ID_PCM_BLURAY },
  { &MEDIASUBTYPE_HDMV_LPCM_AUDIO, CODEC_ID_PCM_BLURAY }, // MPC-HC MPEG Splitter type with header stripped off

  // QT PCM
  { &MEDIASUBTYPE_PCM_NONE,     CODEC_ID_PCM_QTRAW},
  { &MEDIASUBTYPE_PCM_RAW,      CODEC_ID_PCM_QTRAW},
  { &MEDIASUBTYPE_PCM_TWOS,     CODEC_ID_PCM_SxxBE},
  { &MEDIASUBTYPE_PCM_SOWT,     CODEC_ID_PCM_SxxLE},
  { &MEDIASUBTYPE_PCM_IN24,     CODEC_ID_PCM_S24BE},
  { &MEDIASUBTYPE_PCM_IN32,     CODEC_ID_PCM_S32BE},
  { &MEDIASUBTYPE_PCM_FL32,     CODEC_ID_PCM_F32BE},
  { &MEDIASUBTYPE_PCM_FL64,     CODEC_ID_PCM_F64BE},
  { &MEDIASUBTYPE_PCM_IN24_le,  CODEC_ID_PCM_S24LE},
  { &MEDIASUBTYPE_PCM_IN32_le,  CODEC_ID_PCM_S32LE},
  { &MEDIASUBTYPE_PCM_FL32_le,  CODEC_ID_PCM_F32LE},
  { &MEDIASUBTYPE_PCM_FL64_le,  CODEC_ID_PCM_F64LE},

  // WMV
  { &MEDIASUBTYPE_WMAUDIO1,     CODEC_ID_WMAV1    },
  { &MEDIASUBTYPE_WMAUDIO2,     CODEC_ID_WMAV2    },
  { &MEDIASUBTYPE_WMAUDIO3,     CODEC_ID_WMAPRO   },

  // Special LAVFSplitter interface
  { &MEDIASUBTYPE_FFMPEG_AUDIO, CODEC_ID_NONE     },
};

// Define Input Media Types
const AMOVIESETUP_MEDIATYPE CLAVAudio::sudPinTypesIn[] = {
  // DVD Types
  { &MEDIATYPE_DVD_ENCRYPTED_PACK, &MEDIASUBTYPE_MPEG2_AUDIO },
  { &MEDIATYPE_MPEG2_PACK,         &MEDIASUBTYPE_MPEG2_AUDIO },
  { &MEDIATYPE_MPEG2_PES,          &MEDIASUBTYPE_MPEG2_AUDIO },
  { &MEDIATYPE_DVD_ENCRYPTED_PACK, &MEDIASUBTYPE_DOLBY_AC3 },
  { &MEDIATYPE_MPEG2_PACK,         &MEDIASUBTYPE_DOLBY_AC3 },
  { &MEDIATYPE_MPEG2_PES,          &MEDIASUBTYPE_DOLBY_AC3 },
  { &MEDIATYPE_DVD_ENCRYPTED_PACK, &MEDIASUBTYPE_DTS },
  { &MEDIATYPE_MPEG2_PACK,         &MEDIASUBTYPE_DTS },
  { &MEDIATYPE_MPEG2_PES,          &MEDIASUBTYPE_DTS },
  { &MEDIATYPE_DVD_ENCRYPTED_PACK, &MEDIASUBTYPE_DVD_LPCM_AUDIO },
  { &MEDIATYPE_MPEG2_PACK,         &MEDIASUBTYPE_DVD_LPCM_AUDIO },
  { &MEDIATYPE_MPEG2_PES,          &MEDIASUBTYPE_DVD_LPCM_AUDIO },

  // AAC
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_AAC          },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_LATM_AAC     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_MP4A         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_mp4a         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_AAC_ADTS     },

  // Dolby
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_DOLBY_AC3    },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_DOLBY_DDPLUS },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_DOLBY_DDPLUS_ARCSOFT },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_DOLBY_TRUEHD },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_WAVE_DOLBY_AC3 },

  // DTS
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_DTS          },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_WAVE_DTS     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_DTS_HD       },

  // MPEG Audio
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_MPEG1Packet  },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_MPEG1Payload },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_MPEG1AudioPayload },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_MPEG2_AUDIO  },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_MP3          },

  // FLAC
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_FLAC,        },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_FLAC_FRAMED  },

  // Ogg Vorbis
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_Vorbis2      },

  // Other Lossless formats
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_TTA1         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_WAVPACK4     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_MLP          },

  // BluRay LPCM
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_DVD_LPCM_AUDIO  },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_BD_LPCM_AUDIO   },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_HDMV_LPCM_AUDIO }, // MPC-HC MPEG Splitter type with header stripped off

  // QT PCM
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM_NONE     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM_RAW      },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM_TWOS     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM_SOWT     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM_IN24     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM_IN32     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM_FL32     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM_FL64     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM_IN24_le  },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM_IN32_le  },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM_FL32_le  },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM_FL64_le  },

  // WMV
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_WMAUDIO1     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_WMAUDIO2     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_WMAUDIO3     },

  // Special LAVFSplitter interface
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_FFMPEG_AUDIO },
};
const int CLAVAudio::sudPinTypesInCount = countof(CLAVAudio::sudPinTypesIn);

// Define Output Media Types
const AMOVIESETUP_MEDIATYPE CLAVAudio::sudPinTypesOut[] = {
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM        },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_IEEE_FLOAT },
};
const int CLAVAudio::sudPinTypesOutCount = countof(CLAVAudio::sudPinTypesOut);

// Crawl the lavc_audio_codecs array for the proper codec
CodecID FindCodecId(const CMediaType *mt)
{
  for (int i=0; i<countof(lavc_audio_codecs); ++i) {
    if (mt->subtype == *lavc_audio_codecs[i].clsMinorType) {
      return lavc_audio_codecs[i].nFFCodec;
    }
  }
  return CODEC_ID_NONE;
}

static const struct s_ffmpeg_codec_overrides {
  CodecID codec;
  const char *override;
} ffmpeg_codec_overrides[] = {
  { CODEC_ID_MP1, "mp1float" },
  { CODEC_ID_MP2, "mp2float" },
  { CODEC_ID_MP3, "mp3float" },
};

const char *find_codec_override(CodecID codec)
{
  for (int i=0; i<countof(ffmpeg_codec_overrides); ++i) {
    if (ffmpeg_codec_overrides[i].codec == codec)
      return ffmpeg_codec_overrides[i].override;
  }
  return NULL;
}

// Default Channel to Speaker Map
static const scmap_t m_scmap_default[] = {
  //    FL  FR  FC  LFe BL  BR  FLC FRC
  {1, 0},		// Mono			M1, 0
  {2, 0},		// Stereo		FL, FR
  {3, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER},															// 3/0			FL, FR, FC
  {4, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY},										// 3/1			FL, FR, FC, Surround
  {5, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT},						// 3/2			FL, FR, FC, BL, BR
  {6, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT},// 3/2+LFe		FL, FR, FC, BL, BR, LFe
  {7, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT|SPEAKER_BACK_CENTER},	// 3/4			FL, FR, FC, BL, Bls, Brs, BR
  {8, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_SIDE_LEFT|SPEAKER_SIDE_RIGHT|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT},// 3/4+LFe		FL, FR, FC, BL, Bls, Brs, BR, LFe
};

DWORD get_channel_mask(int num_channels)
{
  if (num_channels < 1 && num_channels > 8)
    return 0;
  return m_scmap_default[num_channels - 1].dwChannelMask;
}


static const char *sample_format_strings[] = {
  "16bit Integer",
  "24bit Integer",
  "32bit Integer",
  "8bit Integer",
  "32bit Float",
  "Bitstreaming"
};

const char *get_sample_format_desc(LAVAudioSampleFormat sfFormat)
{
  return sample_format_strings[sfFormat];
}

const char *get_sample_format_desc(CMediaType &mt)
{
  LAVAudioSampleFormat format;
  if(mt.subtype == MEDIASUBTYPE_IEEE_FLOAT) {
    format = SampleFormat_FP32;
  } else {
    WAVEFORMATEX *wfout = (WAVEFORMATEX *)mt.Format();

    switch(wfout->wBitsPerSample) {
    case 8:
      format = SampleFormat_U8;
      break;
    case 16:
      format = SampleFormat_16;
      break;
    case 24:
      format = SampleFormat_24;
      break;
    case 32:
      format = SampleFormat_32;
      break;
    default:
      ASSERT(false);
    }
  }
  return get_sample_format_desc(format);
}

BYTE get_byte_per_sample(LAVAudioSampleFormat sfFormat)
{
  switch(sfFormat) {
  case SampleFormat_U8:
    return 1;
  case SampleFormat_16:
    return 2;
  case SampleFormat_24:
    return 3;
  case SampleFormat_32:
  case SampleFormat_FP32:
    return 4;
  }
  return 0;
}

LAVAudioSampleFormat get_lav_sample_fmt(AVSampleFormat sample_fmt, int bits)
{
  LAVAudioSampleFormat lav_sample_fmt;
  switch(sample_fmt) {
  case AV_SAMPLE_FMT_S16:
  case AV_SAMPLE_FMT_S32:
    if (bits > 24 || (!bits && sample_fmt == AV_SAMPLE_FMT_S32))
      lav_sample_fmt = SampleFormat_32;
    else if (bits > 16)
      lav_sample_fmt = SampleFormat_24;
    else
      lav_sample_fmt = SampleFormat_16;
    break;
  case AV_SAMPLE_FMT_DBL:
  case AV_SAMPLE_FMT_FLT:
    lav_sample_fmt = SampleFormat_FP32;
    break;
  case AV_SAMPLE_FMT_U8:
    lav_sample_fmt = SampleFormat_U8;
    break;
  default:
    lav_sample_fmt = SampleFormat_16;
    break;
  }
  return lav_sample_fmt;
}

static BYTE get_lpcm_sample_rate_index(int sample_rate)
{
  switch(sample_rate) {
  case 48000:
    return 1;
  case 96000:
    return 4;
  case 192000:
    return 5;
  }
  return 0;
}

static BYTE get_lpcm_bit_per_sample_index(int bit_per_sample)
{
  switch(bit_per_sample) {
  case 16:
    return 1;
  case 20:
    return 2;
  case 24:
    return 3;
  }
  return 0;
}

void CLAVAudio::CreateBDLPCMHeader(BYTE * const pBuf, const WAVEFORMATEX_HDMV_LPCM * const wfex_lpcm) const
{
  const BYTE channel_conf = (wfex_lpcm->cbSize >= 1) ? wfex_lpcm->channel_conf : 0;
  pBuf[0] = 0;
  pBuf[1] = 0;
  pBuf[2] = ((channel_conf) << 4) | (get_lpcm_sample_rate_index(wfex_lpcm->nSamplesPerSec) & 0x0f);
  pBuf[3] = get_lpcm_bit_per_sample_index(wfex_lpcm->wBitsPerSample) << 6;
}

// Gets a sample from the buffer for processing
// The sample is returned as a floating point, with either single or double precision, depending on the template type
// DO NOT USE WITH AN INTEGER TYPE - only double and float are allowed
template <class T>
T get_sample_from_buffer(const BYTE * const pBuffer, LAVAudioSampleFormat sfFormat)
{
  T fSample = 0.0;
  switch(sfFormat) {
  case SampleFormat_U8:
    fSample = (T)(*(uint8_t *)pBuffer);
    fSample = (fSample + INT8_MIN) / INT8_MAX;
    break;
  case SampleFormat_16:
    fSample = (T)(*(int16_t *)pBuffer);
    fSample /= INT16_MAX;
    break;
  case SampleFormat_24:
    fSample += pBuffer[0] << 8;
    fSample += pBuffer[1] << 16;
    fSample += pBuffer[2] << 24;
    fSample /= INT32_MAX;
    break;
  case SampleFormat_32:
    fSample = (T)(*(int32_t *)pBuffer);
    fSample /= INT32_MAX;
    break;
  case SampleFormat_FP32:
    fSample = (T)*(float *)pBuffer;
    break;
  }
  return fSample;
}

// This function calculates the Root mean square (RMS) of all samples in the buffer,
// converts the result into a reference dB value, and adds it to the volume floating average
void CLAVAudio::UpdateVolumeStats(const BufferDetails &buffer)
{
  const BYTE bSampleSize = get_byte_per_sample(buffer.sfFormat);
  const DWORD dwSamplesPerChannel = buffer.nSamples;
  const BYTE *pBuffer = buffer.bBuffer->Ptr();
  float * const fChAvg = (float *)calloc(buffer.wChannels, sizeof(float));
  for (DWORD i = 0; i < dwSamplesPerChannel; ++i) {
    for (WORD ch = 0; ch < buffer.wChannels; ++ch) {
      const float fSample = get_sample_from_buffer<float>(pBuffer, buffer.sfFormat);
      fChAvg[ch] += fSample * fSample;
      pBuffer += bSampleSize;
    }
  }

  for (int ch = 0; ch < buffer.wChannels; ++ch) {
    if (fChAvg[ch] > FLT_EPSILON) {
      const float fAvgSqrt =  sqrt(fChAvg[ch] / dwSamplesPerChannel);
      const float fDb = 20.0f * log10(fAvgSqrt);
      m_faVolume[ch].Sample(fDb);
    } else {
      m_faVolume[ch].Sample(-100.0f);
    }
  }
  free(fChAvg);
}

#define MAX_SPEAKER_LAYOUT 18

// Get the channel index for the given speaker flag
// First channel starts at index 0
WORD get_channel_from_flag(DWORD dwMask, DWORD dwFlag)
{
  WORD nChannel = 0;
  for(int i = 0; i < MAX_SPEAKER_LAYOUT; i++) {
    DWORD flag = 1 << i;
    if (dwMask & flag) {
      if (dwFlag == flag)
        break;
      else
        nChannel++;
    }
  }
  return nChannel;
}

// Get the speaker flag for the given channel in the mask
// First channel starts at index 0
DWORD get_flag_from_channel(DWORD dwMask, WORD wChannel)
{
  WORD nChannel = 0;
  for(int i = 0; i < MAX_SPEAKER_LAYOUT; i++) {
    DWORD flag = 1 << i;
    if (dwMask & flag) {
      if (nChannel == wChannel)
        return flag;
      else
        nChannel++;
    }
  }
  return 0;
}

static const char *channel_short_descs[] = {
  "L", "R", "C", "LFE", "BL", "BR", "SL", "SR",
  "BC", "SL", "SR", "TC", "TFL", "TFC", "TFR",
  "TBL", "TBC", "TBR"
};

const char *get_channel_desc(DWORD dwFlag)
{
  for(int i = 0; i < MAX_SPEAKER_LAYOUT; i++) {
    DWORD flag = 1 << i;
    if (flag & dwFlag) {
      return channel_short_descs[i];
    }
  }
  return "-";
}

// Strings will be filled in eventually.
// CODEC_ID_NONE means there is some special handling going on.
// Order is Important, has to be the same as the CC Enum
// Also, the order is used for storage in the Registry
static codec_config_t m_codec_config[] = {
  { 2, { CODEC_ID_AAC, CODEC_ID_AAC_LATM }},       // CC_AAC
  { 1, { CODEC_ID_AC3 }},                          // CC_AC3
  { 1, { CODEC_ID_EAC3 }},                         // CC_EAC3
  { 1, { CODEC_ID_DTS }},                          // CC_DTS
  { 2, { CODEC_ID_MP2, CODEC_ID_MP1 }},            // CC_MP2
  { 1, { CODEC_ID_MP3 }},                          // CC_MP3
  { 2, { CODEC_ID_TRUEHD, CODEC_ID_MLP }},         // CC_TRUEHD
  { 1, { CODEC_ID_FLAC }},                         // CC_FLAC
  { 1, { CODEC_ID_VORBIS }},                       // CC_VORBIS
  { 2, { CODEC_ID_PCM_BLURAY, CODEC_ID_PCM_DVD }, L"lpcm", L"Linear PCM (BluRay & DVD)"}, // CC_LPCM
  { 1, { CODEC_ID_NONE }, L"pcm", L"Raw PCM Types (including QT PCM)" }, // CC_LPCM
  { 1, { CODEC_ID_WAVPACK }},                      // CC_WAVPACK
  { 1, { CODEC_ID_TTA }},                          // CC_TTA
  { 2, { CODEC_ID_WMAV2, CODEC_ID_WMAV1 }, L"wma", L"Windows Media Audio 1/2"}, // CC_WMA2
  { 1, { CODEC_ID_WMAPRO }},                       // CC_WMAPRO
};

const codec_config_t *get_codec_config(LAVAudioCodec codec)
{
  codec_config_t *config = &m_codec_config[codec];
  if (!config->name) {
    AVCodec *codec = avcodec_find_decoder(config->codecs[0]);
    if (codec) {
      size_t name_len = strlen(codec->name) + 1;
      wchar_t *name = (wchar_t *)calloc(name_len, sizeof(wchar_t));
      MultiByteToWideChar(CP_UTF8, 0, codec->name, -1, name, (int)name_len);

      size_t desc_len = strlen(codec->long_name) + 1;
      wchar_t *desc = (wchar_t *)calloc(desc_len, sizeof(wchar_t));
      MultiByteToWideChar(CP_UTF8, 0, codec->long_name, -1, desc, (int)desc_len);

      config->name = name;
      config->description = desc;
    }
  }

  return &m_codec_config[codec];
}

static LAVAudioSampleFormat sampleFormatMapping[5][5] = {
  { SampleFormat_16, SampleFormat_24, SampleFormat_FP32, SampleFormat_32, SampleFormat_U8 },  // SampleFormat_16
  { SampleFormat_24, SampleFormat_FP32, SampleFormat_32, SampleFormat_16, SampleFormat_U8 },  // SampleFormat_24
  { SampleFormat_32, SampleFormat_FP32, SampleFormat_24, SampleFormat_16, SampleFormat_U8 },  // SampleFormat_32
  { SampleFormat_U8, SampleFormat_16, SampleFormat_24, SampleFormat_FP32, SampleFormat_32 },  // SampleFormat_U8
  { SampleFormat_FP32, SampleFormat_24, SampleFormat_32, SampleFormat_16, SampleFormat_U8 },  // SampleFormat_FP32
};

LAVAudioSampleFormat CLAVAudio::GetBestAvailableSampleFormat(LAVAudioSampleFormat inFormat)
{
  ASSERT(inFormat >= 0 && inFormat < SampleFormat_Bitstream);

  LAVAudioSampleFormat outFormat = sampleFormatMapping[inFormat][0];
  for(int i = 0; i < 5; i++) {
    if (GetSampleFormat(sampleFormatMapping[inFormat][i])) {
      outFormat = sampleFormatMapping[inFormat][i];
      break;
    }
  }

  return outFormat;
}
