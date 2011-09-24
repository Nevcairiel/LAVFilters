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

#pragma once

typedef struct sFormatMapping {
  CodecID codec;
  const GUID *subtype;
  unsigned codecTag;
  const GUID *format;
} FormatMapping;

const char *get_stream_language(const AVStream *pStream);
std::string get_codec_name(AVCodecContext *pCodecCtx);
HRESULT lavf_describe_stream(AVStream *pStream, WCHAR **ppszName);

#define LAVF_DISPOSITION_SUB_STREAM      0x10000
#define LAVF_DISPOSITION_SECONDARY_AUDIO 0x20000

inline int get_bits_per_sample(AVCodecContext *ctx)
{
  int bits = av_get_bits_per_sample(ctx->codec_id);
  if (!bits) {
    if (ctx->codec_id == CODEC_ID_PCM_DVD || ctx->codec_id == CODEC_ID_PCM_BLURAY) {
      bits = ctx->bits_per_coded_sample;
    }
    if(!bits) {
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
extern "C" {
const char *lavf_get_parsing_string(enum AVStreamParseType parsing);
void lavf_log_callback(void* ptr, int level, const char* fmt, va_list vl);
}
#endif
