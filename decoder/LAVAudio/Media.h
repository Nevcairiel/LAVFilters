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

#pragma once

struct scmap_t {
  WORD nChannels;
  BYTE ch[8];
  DWORD dwChannelMask;
};

#define MAX_NUM_CC_CODECS 2

struct codec_config_t {
  int nCodecs;
  CodecID codecs[MAX_NUM_CC_CODECS];
  const wchar_t *name;
  const wchar_t *description;
};

typedef enum ConfigCodecs {
  CC_AAC,
  CC_AC3,
  CC_EAC3,
  CC_DTS,
  CC_MP2,
  CC_MP3,
  CC_TRUEHD,
  CC_FLAC,
  CC_VORBIS,
  CC_LPCM,
  CC_PCM,

  CC_NB        // Number of entrys
};

const codec_config_t *get_codec_config(ConfigCodecs codec);

CodecID FindCodecId(const CMediaType *mt);

const scmap_t* get_channel_map(AVCodecContext *avctx);

const char *get_sample_format_desc(LAVAudioSampleFormat sfFormat);
const char *get_sample_format_desc(CMediaType &mt);

BYTE get_byte_per_sample(LAVAudioSampleFormat sfFormat);

WORD get_channel_from_flag(DWORD dwMask, DWORD dwFlag);
DWORD get_flag_from_channel(DWORD dwMask, WORD wChannel);
const char *get_channel_desc(DWORD dwFlag);

// Gets a sample from the buffer for processing
// The sample is returned as a floating point, with either single or double precision, depending on the template type
// DO NOT USE WITH AN INTEGER TYPE - only double and float are allowed
template <class T>
T get_sample_from_buffer(const BYTE *pBuffer, LAVAudioSampleFormat sfFormat);
