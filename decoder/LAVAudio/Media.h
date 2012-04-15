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

#pragma once

#define DBL_SECOND_MULT 10000000.0
#define RT_SECOND_MULT  10000000LL

#define INT24_MAX 8388607i32
#define INT24_MIN (-8388607i32 - 1)

#define LAV_CH_LAYOUT_5POINT1_BC (AV_CH_LAYOUT_SURROUND|AV_CH_LOW_FREQUENCY|AV_CH_BACK_CENTER)
#define LAV_CH_LAYOUT_5POINT1_WIDE (AV_CH_LAYOUT_SURROUND|AV_CH_LOW_FREQUENCY|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define LAV_CH_LAYOUT_7POINT1_EXTRAWIDE (LAV_CH_LAYOUT_5POINT1_WIDE|AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT)

struct scmap_t {
  WORD nChannels;
  DWORD dwChannelMask;
};

#define MAX_NUM_CC_CODECS 5

struct codec_config_t {
  int nCodecs;
  CodecID codecs[MAX_NUM_CC_CODECS];
  const char *name;
  const char *description;
};

const codec_config_t *get_codec_config(LAVAudioCodec codec);

CodecID FindCodecId(const CMediaType *mt);

const char *find_codec_override(CodecID codec);

DWORD get_channel_mask(int num_channels);

const char *get_sample_format_desc(LAVAudioSampleFormat sfFormat);
const char *get_sample_format_desc(CMediaType &mt);

BYTE get_byte_per_sample(LAVAudioSampleFormat sfFormat);

LAVAudioSampleFormat get_lav_sample_fmt(AVSampleFormat sample_fmt, int bits = 0);

WORD get_channel_from_flag(DWORD dwMask, DWORD dwFlag);
DWORD get_flag_from_channel(DWORD dwMask, WORD wChannel);
const char *get_channel_desc(DWORD dwFlag);

// Gets a sample from the buffer for processing
// The sample is returned as a floating point, with either single or double precision, depending on the template type
// DO NOT USE WITH AN INTEGER TYPE - only double and float are allowed
template <class T>
T get_sample_from_buffer(const BYTE *pBuffer, LAVAudioSampleFormat sfFormat);
