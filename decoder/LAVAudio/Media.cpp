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

extern "C" {
#include "libavutil/intreadwrite.h"
};

typedef struct {
  const CLSID*         clsMinorType;
  const enum AVCodecID nFFCodec;
} FFMPEG_SUBTYPE_MAP;

// Map Media Subtype <> FFMPEG Codec Id
static const FFMPEG_SUBTYPE_MAP lavc_audio_codecs[] = {
  // AAC
  { &MEDIASUBTYPE_AAC,          AV_CODEC_ID_AAC      },
  { &MEDIASUBTYPE_LATM_AAC,     AV_CODEC_ID_AAC_LATM },
  { &MEDIASUBTYPE_MPEG_LOAS,    AV_CODEC_ID_AAC_LATM },
  { &MEDIASUBTYPE_MP4A,         AV_CODEC_ID_AAC      },
  { &MEDIASUBTYPE_mp4a,         AV_CODEC_ID_AAC      },
  { &MEDIASUBTYPE_AAC_ADTS,     AV_CODEC_ID_AAC      },
  { &MEDIASUBTYPE_MPEG_ADTS_AAC,AV_CODEC_ID_AAC      },
  { &MEDIASUBTYPE_MPEG_RAW_AAC, AV_CODEC_ID_AAC      },
  { &MEDIASUBTYPE_MPEG_HEAAC,   AV_CODEC_ID_AAC      },
  { &MEDIASUBTYPE_ALS,          AV_CODEC_ID_MP4ALS   },

  // Dolby
  { &MEDIASUBTYPE_DOLBY_AC3,    AV_CODEC_ID_AC3      },
  { &MEDIASUBTYPE_DOLBY_DDPLUS, AV_CODEC_ID_EAC3     },
  { &MEDIASUBTYPE_DOLBY_DDPLUS_ARCSOFT, AV_CODEC_ID_EAC3 },
  { &MEDIASUBTYPE_DOLBY_TRUEHD, AV_CODEC_ID_TRUEHD   },
  { &MEDIASUBTYPE_WAVE_DOLBY_AC3, AV_CODEC_ID_AC3    },

  // DTS
  { &MEDIASUBTYPE_DTS,          AV_CODEC_ID_DTS      },
  { &MEDIASUBTYPE_DTS_HD,       AV_CODEC_ID_DTS      },
  { &MEDIASUBTYPE_WAVE_DTS,     AV_CODEC_ID_DTS      },

  // MPEG Audio
  { &MEDIASUBTYPE_MPEG1Packet,  AV_CODEC_ID_MP1      },
  { &MEDIASUBTYPE_MPEG1Payload, AV_CODEC_ID_MP1      },
  { &MEDIASUBTYPE_MPEG1AudioPayload, AV_CODEC_ID_MP1 },
  { &MEDIASUBTYPE_MPEG2_AUDIO,  AV_CODEC_ID_MP2      },
  { &MEDIASUBTYPE_MP3,          AV_CODEC_ID_MP3      },

  // FLAC
  { &MEDIASUBTYPE_FLAC,         AV_CODEC_ID_FLAC     },
  { &MEDIASUBTYPE_FLAC_FRAMED,  AV_CODEC_ID_FLAC     },

  // Ogg Vorbis
  { &MEDIASUBTYPE_Vorbis2,      AV_CODEC_ID_VORBIS   },

  // Other Lossless formats
  { &MEDIASUBTYPE_TTA1,         AV_CODEC_ID_TTA      },
  { &MEDIASUBTYPE_WAVPACK4,     AV_CODEC_ID_WAVPACK  },
  { &MEDIASUBTYPE_MLP,          AV_CODEC_ID_MLP      },
  { &MEDIASUBTYPE_ALAC,         AV_CODEC_ID_ALAC     },
  { &MEDIASUBTYPE_TAK,          AV_CODEC_ID_TAK      },
  { &MEDIASUBTYPE_AES3,         AV_CODEC_ID_S302M    },

  // BluRay LPCM
  { &MEDIASUBTYPE_DVD_LPCM_AUDIO, AV_CODEC_ID_PCM_DVD },
  { &MEDIASUBTYPE_BD_LPCM_AUDIO,   AV_CODEC_ID_PCM_BLURAY },
  { &MEDIASUBTYPE_HDMV_LPCM_AUDIO, AV_CODEC_ID_PCM_BLURAY }, // MPC-HC MPEG Splitter type with header stripped off

  // QT PCM
  { &MEDIASUBTYPE_PCM_NONE,     AV_CODEC_ID_PCM_QTRAW},
  { &MEDIASUBTYPE_PCM_RAW,      AV_CODEC_ID_PCM_QTRAW},
  { &MEDIASUBTYPE_PCM_TWOS,     AV_CODEC_ID_PCM_SxxBE},
  { &MEDIASUBTYPE_PCM_SOWT,     AV_CODEC_ID_PCM_SxxLE},
  { &MEDIASUBTYPE_PCM_IN24,     AV_CODEC_ID_PCM_S24BE},
  { &MEDIASUBTYPE_PCM_IN32,     AV_CODEC_ID_PCM_S32BE},
  { &MEDIASUBTYPE_PCM_FL32,     AV_CODEC_ID_PCM_F32BE},
  { &MEDIASUBTYPE_PCM_FL64,     AV_CODEC_ID_PCM_F64BE},
  { &MEDIASUBTYPE_PCM_IN24_le,  AV_CODEC_ID_PCM_S24LE},
  { &MEDIASUBTYPE_PCM_IN32_le,  AV_CODEC_ID_PCM_S32LE},
  { &MEDIASUBTYPE_PCM_FL32_le,  AV_CODEC_ID_PCM_F32LE},
  { &MEDIASUBTYPE_PCM_FL64_le,  AV_CODEC_ID_PCM_F64LE},

  // WMV
  { &MEDIASUBTYPE_MSAUDIO1,     AV_CODEC_ID_WMAV1    },
  { &MEDIASUBTYPE_WMAUDIO2,     AV_CODEC_ID_WMAV2    },
  { &MEDIASUBTYPE_WMAUDIO3,     AV_CODEC_ID_WMAPRO   },
  { &MEDIASUBTYPE_WMAUDIO_LOSSLESS, AV_CODEC_ID_WMALOSSLESS },

  // RealMedia Audio
  { &MEDIASUBTYPE_COOK,         AV_CODEC_ID_COOK     },
  { &MEDIASUBTYPE_RAAC,         AV_CODEC_ID_AAC      },
  { &MEDIASUBTYPE_RACP,         AV_CODEC_ID_AAC      },
  { &MEDIASUBTYPE_SIPR,         AV_CODEC_ID_SIPR     },
  { &MEDIASUBTYPE_SIPR_WAVE,    AV_CODEC_ID_SIPR     },
  { &MEDIASUBTYPE_DNET,         AV_CODEC_ID_AC3      },
  { &MEDIASUBTYPE_28_8,         AV_CODEC_ID_RA_288   },
  { &MEDIASUBTYPE_14_4,         AV_CODEC_ID_RA_144   },
  { &MEDIASUBTYPE_RALF,         AV_CODEC_ID_RALF     },

  // Misc
  { &MEDIASUBTYPE_SPEEX,        AV_CODEC_ID_SPEEX    },
  { &MEDIASUBTYPE_OPUS,         AV_CODEC_ID_OPUS     },
  { &MEDIASUBTYPE_SAMR,         AV_CODEC_ID_AMR_NB   },
  { &MEDIASUBTYPE_NELLYMOSER,   AV_CODEC_ID_NELLYMOSER },
  { &MEDIASUBTYPE_ALAW,         AV_CODEC_ID_PCM_ALAW },
  { &MEDIASUBTYPE_MULAW,        AV_CODEC_ID_PCM_MULAW },
  { &MEDIASUBTYPE_MSGSM610,     AV_CODEC_ID_GSM_MS   },
  { &MEDIASUBTYPE_ADPCM_MS,     AV_CODEC_ID_ADPCM_MS },
  { &MEDIASUBTYPE_TRUESPEECH,   AV_CODEC_ID_TRUESPEECH },
  { &MEDIASUBTYPE_QDM2,         AV_CODEC_ID_QDM2     },
  { &MEDIASUBTYPE_VOXWARE_RT29, AV_CODEC_ID_METASOUND },
  { &MEDIASUBTYPE_ATRAC3,       AV_CODEC_ID_ATRAC3   },
  { &MEDIASUBTYPE_ATRC,         AV_CODEC_ID_ATRAC3   },
  { &MEDIASUBTYPE_ATRAC3P,      AV_CODEC_ID_ATRAC3P  },

  // Special LAVFSplitter interface
  { &MEDIASUBTYPE_FFMPEG_AUDIO, AV_CODEC_ID_NONE     },
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
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_MPEG_LOAS    },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_MP4A         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_mp4a         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_AAC_ADTS     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_MPEG_ADTS_AAC},
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_MPEG_RAW_AAC },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_MPEG_HEAAC   },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_ALS          },

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
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_ALAC         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_TAK          },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_AES3         },

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
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_MSAUDIO1     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_WMAUDIO2     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_WMAUDIO3     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_WMAUDIO_LOSSLESS },

  // RealMedia Audio
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_COOK         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_RAAC         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_RACP         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_SIPR         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_SIPR_WAVE    },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_DNET         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_28_8         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_14_4         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_RALF         },

  // Misc
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_SPEEX        },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_OPUS         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_SAMR         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_NELLYMOSER   },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_ALAW         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_MULAW        },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_MSGSM610     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_ADPCM_MS     },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_TRUESPEECH   },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_QDM2,        },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_VOXWARE_RT29 },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_ATRAC3       },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_ATRC         },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_ATRAC3P      },

  // Special LAVFSplitter interface
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_FFMPEG_AUDIO },
};
const UINT CLAVAudio::sudPinTypesInCount = countof(CLAVAudio::sudPinTypesIn);

// Define Output Media Types
const AMOVIESETUP_MEDIATYPE CLAVAudio::sudPinTypesOut[] = {
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM        },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_IEEE_FLOAT },
};
const UINT CLAVAudio::sudPinTypesOutCount = countof(CLAVAudio::sudPinTypesOut);

// Crawl the lavc_audio_codecs array for the proper codec
AVCodecID FindCodecId(const CMediaType *mt)
{
  for (int i=0; i<countof(lavc_audio_codecs); ++i) {
    if (mt->subtype == *lavc_audio_codecs[i].clsMinorType) {
      return lavc_audio_codecs[i].nFFCodec;
    }
  }
  return AV_CODEC_ID_NONE;
}

static const struct s_ffmpeg_codec_overrides {
  AVCodecID codec;
  const char *override;
} ffmpeg_codec_overrides[] = {
  { AV_CODEC_ID_MP1, "mp1float" },
  { AV_CODEC_ID_MP2, "mp2float" },
  { AV_CODEC_ID_MP3, "mp3float" },
  { AV_CODEC_ID_AMR_NB, "libopencore_amrnb" },
  { AV_CODEC_ID_AMR_WB, "libopencore_amrwb" },
};

const char *find_codec_override(AVCodecID codec)
{
  for (int i=0; i<countof(ffmpeg_codec_overrides); ++i) {
    if (ffmpeg_codec_overrides[i].codec == codec)
      return ffmpeg_codec_overrides[i].override;
  }
  return nullptr;
}

// Default Channel to Speaker Map
static const scmap_t m_scmap_default[] = {
  //    FL  FR  FC  LFe BL  BR  FLC FRC
  {1, 0},		// Mono			M1, 0
  {2, 0},		// Stereo		FL, FR
  {3, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER},															// 3/0			FL, FR, FC
  {4, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY},										// 3/1			FL, FR, FC, Surround
  {5, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT},						// 3/2			FL, FR, FC, BL, BR
  {6, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_SIDE_LEFT|SPEAKER_SIDE_RIGHT},// 3/2+LFe		FL, FR, FC, BL, BR, LFe
  {7, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT|SPEAKER_BACK_CENTER},	// 3/4			FL, FR, FC, BL, Bls, Brs, BR
  {8, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_SIDE_LEFT|SPEAKER_SIDE_RIGHT|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT},// 3/4+LFe		FL, FR, FC, BL, Bls, Brs, BR, LFe
};

DWORD get_channel_mask(int num_channels)
{
  if (num_channels < 1 || num_channels > 8)
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
  case AV_SAMPLE_FMT_S16P:
  case AV_SAMPLE_FMT_S32:
  case AV_SAMPLE_FMT_S32P:
    if (bits > 24 || (!bits && (sample_fmt == AV_SAMPLE_FMT_S32 || sample_fmt == AV_SAMPLE_FMT_S32P)))
      lav_sample_fmt = SampleFormat_32;
    else if (bits > 16)
      lav_sample_fmt = SampleFormat_24;
    else
      lav_sample_fmt = SampleFormat_16;
    break;
  case AV_SAMPLE_FMT_DBL:
  case AV_SAMPLE_FMT_DBLP:
  case AV_SAMPLE_FMT_FLT:
  case AV_SAMPLE_FMT_FLTP:
    lav_sample_fmt = SampleFormat_FP32;
    break;
  case AV_SAMPLE_FMT_U8:
  case AV_SAMPLE_FMT_U8P:
    lav_sample_fmt = SampleFormat_U8;
    break;
  default:
    lav_sample_fmt = SampleFormat_16;
    break;
  }
  return lav_sample_fmt;
}

AVSampleFormat get_ff_sample_fmt(LAVAudioSampleFormat sample_fmt)
{
  AVSampleFormat ff_sample_fmt;
  switch(sample_fmt) {
  case SampleFormat_16:
    ff_sample_fmt = AV_SAMPLE_FMT_S16;
    break;
  case SampleFormat_24:
  case SampleFormat_32:
    ff_sample_fmt = AV_SAMPLE_FMT_S32;
    break;
  case SampleFormat_FP32:
    ff_sample_fmt = AV_SAMPLE_FMT_FLT;
    break;
  case SampleFormat_U8:
    ff_sample_fmt = AV_SAMPLE_FMT_U8;
    break;
  default:
    assert(0);
    break;
  }
  return ff_sample_fmt;
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

static BYTE get_dvdlpcm_sample_rate_index(int sample_rate)
{
  switch(sample_rate) {
  case 48000:
    return 0;
  case 96000:
    return 1;
  case 44100:
    return 2;
  case 32000:
    return 3;
  }
  return 0;
}

void CLAVAudio::CreateDVDLPCMHeader(BYTE * const pBuf, const WAVEFORMATEX * const wfex) const
{
  pBuf[0] = 0;
  pBuf[1] = (((wfex->wBitsPerSample - 16) / 4) << 6) | (get_dvdlpcm_sample_rate_index(wfex->nSamplesPerSec) << 4) | ((wfex->nChannels - 1) & 7);
  pBuf[2] = 0;
}

HRESULT CLAVAudio::ParseRealAudioHeader(const BYTE *extra, const size_t extralen)
{
  const uint8_t *fmt = extra+4;
  uint16_t version = AV_RB16(fmt);
  fmt += 2;
  if (version == 3) {
    DbgLog((LOG_TRACE, 10, L"RealAudio Header version 3 unsupported"));
    return VFW_E_UNSUPPORTED_AUDIO;
  } else if (version == 4 || version == 5 && extralen > 50) {
    // main format block
    fmt += 2;  // word - unused (always 0)
    fmt += 4;  // byte[4] - .ra4/.ra5 signature
    fmt += 4;  // dword - unknown
    fmt += 2;  // word - Version2
    fmt += 4;  // dword - header size
    m_raData.flavor = AV_RB16(fmt); fmt += 2;  // word - codec flavor
    m_raData.coded_frame_size = AV_RB32(fmt); fmt += 4;  // dword - coded frame size
    fmt += 12; // byte[12] - unknown
    m_raData.sub_packet_h = AV_RB16(fmt); fmt += 2;  // word - sub packet h
    m_raData.audio_framesize = m_pAVCtx->block_align = AV_RB16(fmt); fmt += 2;  // word - frame size
    m_raData.sub_packet_size = AV_RB16(fmt); fmt += 2;  // word - subpacket size
    fmt += 2;  // word - unknown
    // 6 Unknown bytes in ver 5
    if (version == 5)
      fmt += 6;
    // Audio format block
    fmt += 8;
    // Tag info in v4
    if (version == 4) {
      int len = *fmt++;
      m_raData.deint_id = AV_RB32(fmt); fmt += len;
      len = *fmt++;
      fmt += len;
    } else if (version == 5) {
      m_raData.deint_id = AV_RB32(fmt); fmt += 4;
      fmt += 4;
    }
    fmt += 3;
    if (version == 5)
      fmt++;

    size_t ra_extralen = min((size_t)((extra + extralen) - (fmt+4)), AV_RB32(fmt));
    if (ra_extralen > 0)  {
      m_pAVCtx->extradata_size = (int)ra_extralen;
      m_pAVCtx->extradata      = (uint8_t *)av_mallocz(ra_extralen + AV_INPUT_BUFFER_PADDING_SIZE);
      memcpy(m_pAVCtx->extradata, fmt+4, ra_extralen);
    }
  } else {
    DbgLog((LOG_TRACE, 10, L"Unknown RealAudio Header version: %d", version));
    return VFW_E_UNSUPPORTED_AUDIO;
  }

  return S_OK;
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
  "L", "R", "C", "LFE", "BL", "BR", "FLC", "FRC",
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
// AV_CODEC_ID_NONE means there is some special handling going on.
// Order is Important, has to be the same as the CC Enum
// Also, the order is used for storage in the Registry
static codec_config_t m_codec_config[] = {
  { 2, { AV_CODEC_ID_AAC, AV_CODEC_ID_AAC_LATM }},       // CC_AAC
  { 1, { AV_CODEC_ID_AC3 }},                          // CC_AC3
  { 1, { AV_CODEC_ID_EAC3 }},                         // CC_EAC3
  { 1, { AV_CODEC_ID_DTS }, "dts", "DTS Coherent Acoustics (DTS, DTS-HD)"},                          // CC_DTS
  { 2, { AV_CODEC_ID_MP2, AV_CODEC_ID_MP1 }},            // CC_MP2
  { 1, { AV_CODEC_ID_MP3 }},                          // CC_MP3
  { 2, { AV_CODEC_ID_TRUEHD, AV_CODEC_ID_MLP }},         // CC_TRUEHD
  { 1, { AV_CODEC_ID_FLAC }},                         // CC_FLAC
  { 1, { AV_CODEC_ID_VORBIS }},                       // CC_VORBIS
  { 2, { AV_CODEC_ID_PCM_BLURAY, AV_CODEC_ID_PCM_DVD }, "lpcm", "Linear PCM (BluRay & DVD)"}, // CC_LPCM
  { 1, { AV_CODEC_ID_NONE }, "pcm", "Raw PCM Types (including QT PCM)" }, // CC_LPCM
  { 1, { AV_CODEC_ID_WAVPACK }},                      // CC_WAVPACK
  { 1, { AV_CODEC_ID_TTA }},                          // CC_TTA
  { 2, { AV_CODEC_ID_WMAV2, AV_CODEC_ID_WMAV1 }, "wma", "Windows Media Audio 1/2"}, // CC_WMA2
  { 1, { AV_CODEC_ID_WMAPRO }},                       // CC_WMAPRO
  { 1, { AV_CODEC_ID_COOK }},                         // CC_COOK
  { 4, { AV_CODEC_ID_SIPR, AV_CODEC_ID_RA_144, AV_CODEC_ID_RA_288, AV_CODEC_ID_RALF }, "realaudio", "Real Audio (SIPR, RALF, 14.4 28.8)" }, // CC_REAL
  { 1, { AV_CODEC_ID_WMALOSSLESS }},                  // CC_WMALL
  { 1, { AV_CODEC_ID_ALAC }},                         // CC_ALAC
  { 1, { AV_CODEC_ID_OPUS }, "opus", "Opus Audio Codec"}, // CC_OPUS
  { 2, { AV_CODEC_ID_AMR_NB, AV_CODEC_ID_AMR_WB }, "amr", "AMR-NB/WB (Adaptive Multi-Rate NarrowBand/WideBand)" }, // CC_AMR
  { 1, { AV_CODEC_ID_NELLYMOSER }},                   // CC_Nellymoser
  { 4, { AV_CODEC_ID_PCM_ALAW, AV_CODEC_ID_PCM_MULAW, AV_CODEC_ID_GSM_MS, AV_CODEC_ID_ADPCM_MS }, "mspcm", "Microsoft PCM (A-Law, muLaw, MS-GSM, MS ADPCM)" }, // CC_MSPCM
  { 1, { AV_CODEC_ID_TRUESPEECH }},                   // CC_Truespeech
  { 1, { AV_CODEC_ID_TAK }},                          // CC_TAK
  { 3, { AV_CODEC_ID_ATRAC1, AV_CODEC_ID_ATRAC3, AV_CODEC_ID_ATRAC3P }, "atrac", "ATRAC (Adaptive TRansform Acoustic Coding)"}, // CC_ATRAC
};

const codec_config_t *get_codec_config(LAVAudioCodec codec)
{
  codec_config_t *config = &m_codec_config[codec];

  AVCodec *avcodec = avcodec_find_decoder(config->codecs[0]);
  if (avcodec) {
    if (!config->name) {
      config->name = avcodec->name;
    }

    if (!config->description) {
      config->description = avcodec->long_name;
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

LAVAudioSampleFormat CLAVAudio::GetBestAvailableSampleFormat(LAVAudioSampleFormat inFormat, int *bits, BOOL bNoFallback)
{
  ASSERT(inFormat >= 0 && inFormat < SampleFormat_Bitstream);

  if (m_FallbackFormat != SampleFormat_None && !bNoFallback)
    return m_FallbackFormat;

  LAVAudioSampleFormat outFormat = sampleFormatMapping[inFormat][0];
  for(int i = 0; i < 5; i++) {
    if (GetSampleFormat(sampleFormatMapping[inFormat][i])) {
      outFormat = sampleFormatMapping[inFormat][i];
      break;
    }
  }

  if (bits && outFormat != inFormat)
    *bits = get_byte_per_sample(outFormat) << 3;

  return outFormat;
}

// Copy of ff_spdif_bswap_buf16, which sadly is a private symbol
void lav_spdif_bswap_buf16(uint16_t *dst, const uint16_t *src, int w)
{
  int i;

  for (i = 0; i + 8 <= w; i += 8) {
    dst[i + 0] = av_bswap16(src[i + 0]);
    dst[i + 1] = av_bswap16(src[i + 1]);
    dst[i + 2] = av_bswap16(src[i + 2]);
    dst[i + 3] = av_bswap16(src[i + 3]);
    dst[i + 4] = av_bswap16(src[i + 4]);
    dst[i + 5] = av_bswap16(src[i + 5]);
    dst[i + 6] = av_bswap16(src[i + 6]);
    dst[i + 7] = av_bswap16(src[i + 7]);
  }
  for (; i < w; i++)
    dst[i + 0] = av_bswap16(src[i + 0]);
}
