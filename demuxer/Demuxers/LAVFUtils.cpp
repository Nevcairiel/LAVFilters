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
#include "lavfutils.h"

extern "C" {
#include "libavcodec/audioconvert.h"
}

#include <sstream>

static int get_bit_rate(AVCodecContext *ctx)
{
  int bit_rate;
  int bits_per_sample;

  switch(ctx->codec_type) {
  case AVMEDIA_TYPE_VIDEO:
  case AVMEDIA_TYPE_DATA:
  case AVMEDIA_TYPE_SUBTITLE:
  case AVMEDIA_TYPE_ATTACHMENT:
    bit_rate = ctx->bit_rate;
    break;
  case AVMEDIA_TYPE_AUDIO:
    bits_per_sample = av_get_bits_per_sample(ctx->codec_id);
    bit_rate = ctx->bit_rate ? ctx->bit_rate : ctx->sample_rate * ctx->channels * bits_per_sample;
    break;
  default:
    bit_rate = 0;
    break;
  }
  return bit_rate;
}

const char *get_stream_language(const AVStream *pStream)
{
  char *lang = NULL;
  if (av_dict_get(pStream->metadata, "language", NULL, 0)) {
    lang = av_dict_get(pStream->metadata, "language", NULL, 0)->value;
  }
  // Don't bother with undetermined languages (fallback value in some containers)
  if(lang && strncmp(lang, "und", 3))
    return lang;
  return NULL;
}

struct s_id_map {
  int id;
  const char *name;
};

struct s_id_map nice_codec_names[] = {
  // Video
  { CODEC_ID_H264, "h264" }, // XXX: Do not remove, required for custom profile/level formatting
  { CODEC_ID_VC1, "vc-1" },  // XXX: Do not remove, required for custom profile/level formatting
  { CODEC_ID_MPEG2VIDEO, "mpeg2" },
  // Audio
  { CODEC_ID_DTS, "dts" },
  { CODEC_ID_AAC_LATM, "aac (latm)" },
  // Subs
  { CODEC_ID_TEXT, "txt" },
  { CODEC_ID_MOV_TEXT, "tx3g" },
  { CODEC_ID_SRT, "srt" },
  { CODEC_ID_HDMV_PGS_SUBTITLE, "pgs" },
  { CODEC_ID_DVD_SUBTITLE, "vobsub" },
  { CODEC_ID_DVB_SUBTITLE, "dvbsub" },
  { CODEC_ID_SSA, "ssa/ass" },
  { CODEC_ID_XSUB, "xsub" },
};

// Uppercase the given string
static std::string tolower(const char *str) {
  size_t len = strlen(str);
  std::string ret(len, char());
  for (size_t i = 0; i < len; ++i) {
    ret[i] = (str[i] <= 'Z' && str[i] >= 'A') ? str[i]+('a'-'A') : str[i];
  }
  return ret;
}

std::string get_codec_name(AVCodecContext *pCodecCtx)
{
  CodecID id = pCodecCtx->codec_id;

  // Grab the codec
  AVCodec *p = avcodec_find_decoder(id);
  const char *profile = p ? av_get_profile_name(p, pCodecCtx->profile) : NULL;

  std::ostringstream codec_name;

  const char *nice_name = NULL;
  for (int i = 0; i < countof(nice_codec_names); ++i)
  {
    if (nice_codec_names[i].id == id) {
      nice_name = nice_codec_names[i].name;
      break;
    }
  }

  if (id == CODEC_ID_DTS && pCodecCtx->codec_tag == 0xA2) {
    profile = "DTS Express";
  }

  if (id == CODEC_ID_H264 && profile) {
    codec_name << nice_name << " " << tolower(profile);
    if (pCodecCtx->level && pCodecCtx->level != FF_LEVEL_UNKNOWN && pCodecCtx->level < 1000) {
      char l_buf[5];
      sprintf_s(l_buf, "%.1f", pCodecCtx->level / 10.0);
      codec_name << " L" << l_buf;
    }
  } else if (id == CODEC_ID_VC1 && profile) {
    codec_name << nice_name << " " << tolower(profile);
    if (pCodecCtx->level != FF_LEVEL_UNKNOWN) {
      codec_name << " L" << pCodecCtx->level;
    }
  } else if (id == CODEC_ID_DTS && profile) {
    codec_name << tolower(profile);
  } else if (nice_name) {
    codec_name << nice_name;
    if (profile)
      codec_name << " " << tolower(profile);
  } else if (p && p->name) {
    codec_name << p->name;
    if (profile)
      codec_name << " " << tolower(profile);
  } else if (pCodecCtx->codec_name[0] != '\0') {
    codec_name << pCodecCtx->codec_name;
  } else {
    /* output avi tags */
    char buf[32];
    av_get_codec_tag_string(buf, sizeof(buf), pCodecCtx->codec_tag);
    codec_name << buf;
    sprintf_s(buf, "0x%04X", pCodecCtx->codec_tag);
    codec_name  << " / " << buf;
  }
  return codec_name.str();
}

#define SUPPORTED_FLAGS (AV_DISPOSITION_FORCED|AV_DISPOSITION_DEFAULT|AV_DISPOSITION_HEARING_IMPAIRED|AV_DISPOSITION_VISUAL_IMPAIRED|LAVF_DISPOSITION_SUB_STREAM|LAVF_DISPOSITION_SECONDARY_AUDIO)

static std::string format_flags(int flags){
  std::ostringstream out;

#define FLAG_TAG(f, s) if(flags & f) { if(!first) out << ","; out << s; first = false; }

  if (flags & SUPPORTED_FLAGS) {
    bool first = true;
    out << " [";
    FLAG_TAG(AV_DISPOSITION_DEFAULT, "default");
    FLAG_TAG(AV_DISPOSITION_FORCED, "forced");
    FLAG_TAG(AV_DISPOSITION_HEARING_IMPAIRED, "hearing impaired");
    FLAG_TAG(AV_DISPOSITION_VISUAL_IMPAIRED, "visual impaired");
    FLAG_TAG(LAVF_DISPOSITION_SUB_STREAM, "sub");
    FLAG_TAG(LAVF_DISPOSITION_SECONDARY_AUDIO, "secondary");
    out << "]";
  }
  return out.str();
}

static bool show_sample_fmt(CodecID codec_id) {
  // PCM Codecs
  if (codec_id >= 0x10000 && codec_id < 0x12000) {
    return true;
  }
  // Lossless Codecs
  if (codec_id == CODEC_ID_MLP
   || codec_id == CODEC_ID_TRUEHD
   || codec_id == CODEC_ID_FLAC
   || codec_id == CODEC_ID_WMALOSSLESS
   || codec_id == CODEC_ID_WAVPACK
   || codec_id == CODEC_ID_MP4ALS
   || codec_id == CODEC_ID_ALAC) {
     return true;
  }
  return false;
}

std::string lavf_get_stream_description(AVStream *pStream)
{
  AVCodecContext *enc = pStream->codec;

  std::string codec_name = get_codec_name(enc);

  const char *lang = get_stream_language(pStream);
  std::string sLanguage;

  if(lang) {
    sLanguage = ProbeLangForLanguage(lang);
    if (sLanguage.empty()) {
      sLanguage = lang;
    }
  }

  char *title = NULL;
  if (av_dict_get(pStream->metadata, "title", NULL, 0)) {
    title = av_dict_get(pStream->metadata, "title", NULL, 0)->value;
  } else if (av_dict_get(pStream->metadata, "handler_name", NULL, 0)) {
    title = av_dict_get(pStream->metadata, "handler_name", NULL, 0)->value;
    if (strcmp(title, "GPAC ISO Video Handler") == 0 || strcmp(title, "VideoHandler") == 0|| strcmp(title, "GPAC ISO Audio Handler") == 0 || strcmp(title, "GPAC Streaming Text Handler") == 0)
      title = NULL;
  }

  // Empty titles are rather useless
  if (title && strlen(title) == 0)
    title = NULL;

  int bitrate = get_bit_rate(enc);

  std::ostringstream buf;
  switch(enc->codec_type) {
  case AVMEDIA_TYPE_VIDEO:
    buf << "V: ";
    // Title/Language
    if (title && lang) {
      buf << title << " [" << lang << "] (";
    } else if (title) {
      // Print either title or lang
      buf << title << " (";
    } else if (lang) {
      buf << sLanguage << " [" << lang << "] (";
    }
    // Codec
    buf << codec_name;
    // Pixel Format
    if (enc->pix_fmt != PIX_FMT_NONE) {
      buf << ", " << av_get_pix_fmt_name(enc->pix_fmt);
    }
    // Dimensions
    if (enc->width) {
      buf << ", " << enc->width << "x" << enc->height;
    }
    // Bitrate
    if (bitrate > 0) {
      buf << ", " << (bitrate / 1000) << " kb/s";
    }
    // Closing tag
    if (title || lang) {
      buf << ")";
    }
    buf << format_flags(pStream->disposition);
    break;
  case AVMEDIA_TYPE_AUDIO:
    buf << "A: ";
    // Title/Language
    if (title && lang) {
      buf << title << " [" << lang << "] (";
    } else if (title) {
      // Print either title or lang
      buf << title << " (";
    } else if (lang) {
      buf << sLanguage << " [" << lang << "] (";
    }
    // Codec
    buf << codec_name;
    // Sample Rate
    if (enc->sample_rate) {
      buf << ", " << enc->sample_rate << " Hz";
    }
    if (enc->channels) {
      // Get channel layout
      char channel[32];
      av_get_channel_layout_string(channel, 32, enc->channels, enc->channel_layout);
      buf << ", " << channel;
    }
    // Sample Format
    if (show_sample_fmt(enc->codec_id) && get_bits_per_sample(enc, true)) {
      if (enc->sample_fmt == AV_SAMPLE_FMT_FLT || enc->sample_fmt == AV_SAMPLE_FMT_DBL) {
        buf << ", fp";
      } else {
        buf << ", s";
      }
      buf << get_bits_per_sample(enc, true);
    }
    // Bitrate
    if (bitrate > 0) {
      buf << ", " << (bitrate / 1000) << " kb/s";
    }
    // Closing tag
    if (title || lang) {
      buf << ")";
    }
    // Flags
    buf << format_flags(pStream->disposition);
    break;
  case AVMEDIA_TYPE_SUBTITLE:
    buf << "S: ";
    // Title/Language
    if (title && lang) {
      buf << title << " [" << lang << "] (";
    } else if (title) {
      // Print either title or lang
      buf << title << " (";
    } else if (lang) {
      buf << sLanguage << " [" << lang << "] (";
    }
    // Codec
    buf << codec_name;
    if (title || lang) {
      buf << ")";
    }
    // Subtitle flags
    buf << format_flags(pStream->disposition);
    break;
  default:
    buf << "Unknown: Stream #" << pStream->index;
    break;
  }

  return buf.str();
}

#ifdef DEBUG

#define LAVF_PARSE_TYPE(x) case x: return #x;
const char *lavf_get_parsing_string(enum AVStreamParseType parsing)
{
  switch (parsing) {
    LAVF_PARSE_TYPE(AVSTREAM_PARSE_NONE);
    LAVF_PARSE_TYPE(AVSTREAM_PARSE_FULL);
    LAVF_PARSE_TYPE(AVSTREAM_PARSE_HEADERS);
    LAVF_PARSE_TYPE(AVSTREAM_PARSE_TIMESTAMPS);
    LAVF_PARSE_TYPE(AVSTREAM_PARSE_FULL_ONCE);
  };
  return "unknown";
}
#endif
