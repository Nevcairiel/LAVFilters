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

typedef struct sFormatMapping {
  CodecID codec;
  const GUID *subtype;
  unsigned codecTag;
  const GUID *format;
} FormatMapping;

const char *get_stream_language(const AVStream *pStream);
std::string get_codec_name(AVCodecContext *pCodecCtx);
std::string lavf_get_stream_description(AVStream *pStream);

#define LAVF_DISPOSITION_SUB_STREAM      0x10000
#define LAVF_DISPOSITION_SECONDARY_AUDIO 0x20000

inline int get_bits_per_sample(AVCodecContext *ctx, bool bRaw = false)
{
  int bits = av_get_bits_per_sample(ctx->codec_id);
  if (!bits || bRaw) {
    bits = ctx->bits_per_coded_sample;
    if(!bits || bRaw) {
      if (ctx->sample_fmt == AV_SAMPLE_FMT_S32 && ctx->bits_per_raw_sample) {
        bits = ctx->bits_per_raw_sample;
      } else {
        bits = av_get_bits_per_sample_fmt(ctx->sample_fmt);
      }
    }
  }
  return bits;
}

#ifdef DEBUG
const char *lavf_get_parsing_string(enum AVStreamParseType parsing);
#endif
