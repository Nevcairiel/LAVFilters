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
#include "lavfutils.h"

#include <sstream>

static int64_t get_bit_rate(const AVCodecParameters *par)
{
  int64_t bit_rate;

  switch(par->codec_type) {
  case AVMEDIA_TYPE_VIDEO:
  case AVMEDIA_TYPE_DATA:
  case AVMEDIA_TYPE_SUBTITLE:
  case AVMEDIA_TYPE_ATTACHMENT:
    bit_rate = par->bit_rate;
    break;
  case AVMEDIA_TYPE_AUDIO:
    bit_rate = par->bit_rate ? par->bit_rate : par->sample_rate * par->channels * av_get_bits_per_sample(par->codec_id);
    break;
  default:
    bit_rate = 0;
    break;
  }
  return bit_rate;
}

const char *get_stream_language(const AVStream *pStream)
{
  char *lang = nullptr;
  if (AVDictionaryEntry *dictEntry = av_dict_get(pStream->metadata, "language", nullptr, 0)) {
    lang = dictEntry->value;
  }
  // Don't bother with undetermined languages (fallback value in some containers)
  if(lang && strncmp(lang, "und", 3))
    return lang;
  return nullptr;
}

struct s_id_map {
  int id;
  const char *name;
};

struct s_id_map nice_codec_names[] = {
  // Video
  { AV_CODEC_ID_H264, "h264" }, // XXX: Do not remove, required for custom profile/level formatting
  { AV_CODEC_ID_HEVC, "hevc" }, // XXX: Do not remove, required for custom profile/level formatting
  { AV_CODEC_ID_VC1, "vc-1" },  // XXX: Do not remove, required for custom profile/level formatting
  { AV_CODEC_ID_MPEG2VIDEO, "mpeg2" },
  // Audio
  { AV_CODEC_ID_DTS, "dts" },
  { AV_CODEC_ID_AAC_LATM, "aac (latm)" },
  // Subs
  { AV_CODEC_ID_TEXT, "txt" },
  { AV_CODEC_ID_MOV_TEXT, "tx3g" },
  { AV_CODEC_ID_SRT, "srt" },
  { AV_CODEC_ID_HDMV_PGS_SUBTITLE, "pgs" },
  { AV_CODEC_ID_DVD_SUBTITLE, "vobsub" },
  { AV_CODEC_ID_DVB_SUBTITLE, "dvbsub" },
  { AV_CODEC_ID_SSA, "ssa/ass" },
  { AV_CODEC_ID_XSUB, "xsub" },
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

std::string get_codec_name(const AVCodecParameters *par)
{
  AVCodecID id = par->codec_id;

  // Grab the codec
  const AVCodec *p = avcodec_find_decoder(id);
  const AVCodecDescriptor *desc = avcodec_descriptor_get(id);
  const char *profile = avcodec_profile_name(id, par->profile);

  std::ostringstream codec_name;

  const char *nice_name = nullptr;
  for (int i = 0; i < countof(nice_codec_names); ++i)
  {
    if (nice_codec_names[i].id == id) {
      nice_name = nice_codec_names[i].name;
      break;
    }
  }

  if (id == AV_CODEC_ID_DTS && par->codec_tag == 0xA2) {
    profile = "DTS Express";
  }

  if (id == AV_CODEC_ID_H264 && profile) {
    codec_name << nice_name << " " << tolower(profile);
    if (par->level && par->level != FF_LEVEL_UNKNOWN && par->level < 1000) {
      char l_buf[5];
      sprintf_s(l_buf, "%.1f", par->level / 10.0);
      codec_name << " L" << l_buf;
    }
  } else if (id == AV_CODEC_ID_HEVC && profile) {
    codec_name << nice_name << " " << tolower(profile);
    if (par->level && par->level != FF_LEVEL_UNKNOWN && par->level < 1000) {
      char l_buf[5];
      sprintf_s(l_buf, "%.1f", par->level / 30.0);
      codec_name << " L" << l_buf;
    }
  } else if (id == AV_CODEC_ID_VC1 && profile) {
    codec_name << nice_name << " " << tolower(profile);
    if (par->level != FF_LEVEL_UNKNOWN) {
      codec_name << " L" << par->level;
    }
  } else if (id == AV_CODEC_ID_DTS && profile) {
    codec_name << tolower(profile);
  } else if (id == AV_CODEC_ID_JPEG2000 && profile) {
    codec_name << tolower(profile);
  } else if (nice_name) {
    codec_name << nice_name;
    if (profile)
      codec_name << " " << tolower(profile);
  } else if (desc && desc->name) {
    codec_name << desc->name;
    if (profile)
      codec_name << " " << tolower(profile);
  } else if (p && p->name) {
    codec_name << p->name;
    if (profile)
      codec_name << " " << tolower(profile);
  } else {
    /* output avi tags */
    char buf[AV_FOURCC_MAX_STRING_SIZE] = { 0 };
    codec_name << av_fourcc_make_string(buf, par->codec_tag);
    sprintf_s(buf, "0x%04X", par->codec_tag);
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

static bool show_sample_fmt(const AVCodecParameters *par) {
  AVCodecID codec_id = par->codec_id;

  // PCM Codecs
  if (codec_id >= 0x10000 && codec_id < 0x12000) {
    return true;
  }
  // Lossless Codecs
  if (codec_id == AV_CODEC_ID_MLP
   || codec_id == AV_CODEC_ID_TRUEHD
   || codec_id == AV_CODEC_ID_FLAC
   || codec_id == AV_CODEC_ID_WMALOSSLESS
   || codec_id == AV_CODEC_ID_WAVPACK
   || codec_id == AV_CODEC_ID_MP4ALS
   || codec_id == AV_CODEC_ID_ALAC) {
     return true;
  }

  // Lossless DTS
  if (codec_id == AV_CODEC_ID_DTS && par->profile == FF_PROFILE_DTS_HD_MA)
    return true;

  return false;
}

const char * lavf_get_stream_title(const AVStream * pStream)
{
  char *title = nullptr;
  if (AVDictionaryEntry *dictEntry = av_dict_get(pStream->metadata, "title", nullptr, 0)) {
    title = dictEntry->value;
  } else if (AVDictionaryEntry *dictEntry = av_dict_get(pStream->metadata, "handler_name", nullptr, 0)) {
    title = dictEntry->value;
    if (strcmp(title, "GPAC ISO Video Handler") == 0 || strcmp(title, "VideoHandler") == 0 || strcmp(title, "GPAC ISO Audio Handler") == 0 || strcmp(title, "GPAC Streaming Text Handler") == 0)
      title = nullptr;
  }

  return title;
}

std::string lavf_get_stream_description(const AVStream *pStream)
{
  AVCodecParameters *par = pStream->codecpar;

  std::string codec_name = get_codec_name(par);

  const char *lang = get_stream_language(pStream);
  std::string sLanguage;

  if(lang) {
    sLanguage = ProbeLangForLanguage(lang);
    if (sLanguage.empty()) {
      sLanguage = lang;
    }
  }

  const char * title = lavf_get_stream_title(pStream);

  // Empty titles are rather useless
  if (title && strlen(title) == 0)
    title = nullptr;

  int64_t bitrate = get_bit_rate(par);

  std::ostringstream buf;
  switch(par->codec_type) {
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
    if (const char *pix_fmt = av_get_pix_fmt_name((AVPixelFormat)par->format)) {
      buf << ", " << pix_fmt;
    }
    // Dimensions
    if (par->width) {
      buf << ", " << par->width << "x" << par->height;
    }
    // Bitrate
    if (bitrate > 0) {
      buf << ", " << (bitrate / 1000) << " kb/s";
    }
    if (par->codec_id == AV_CODEC_ID_H264 && par->profile == FF_PROFILE_H264_STEREO_HIGH) {
      AVDictionaryEntry *entry = av_dict_get(pStream->metadata, "stereo_mode", nullptr, 0);
      if (entry && strcmp(entry->value, "mvc_lr") == 0)
        buf << ", lr";
      else if (entry && strcmp(entry->value, "mvc_rl") == 0)
        buf << ", rl";
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
    if (par->sample_rate) {
      buf << ", " << par->sample_rate << " Hz";
    }
    if (par->channels) {
      // Get channel layout
      char channel[32];
      av_get_channel_layout_string(channel, 32, par->channels, par->channel_layout);
      buf << ", " << channel;
    }
    // Sample Format
    if (show_sample_fmt(par) && get_bits_per_sample(par, true)) {
      if (par->format == AV_SAMPLE_FMT_FLT || par->format == AV_SAMPLE_FMT_DBL) {
        buf << ", fp";
      } else {
        buf << ", s";
      }
      buf << get_bits_per_sample(par, true);
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

bool GetH264MVCStreamIndices(AVFormatContext *fmt, int *nBaseIndex, int *nExtensionIndex)
{
  bool bResult = true;
  *nBaseIndex = -1;
  *nExtensionIndex = -1;
  for (unsigned int i = 0; i < fmt->nb_streams; i++) {
    AVStream *st = fmt->streams[i];

    if (st->codecpar->codec_id == AV_CODEC_ID_H264_MVC && st->codecpar->extradata_size > 0) {
      if (*nExtensionIndex == -1)
        *nExtensionIndex = i;
      else {
        DbgLog((LOG_TRACE, 10, L" -> Multiple H264 MVC extension streams, unsupported."));
        bResult = false;
      }
    }
    else if (st->codecpar->codec_id == AV_CODEC_ID_H264) {
      if ((st->codecpar->width == 1920 && st->codecpar->height == 1080) || (st->codecpar->width == 1280 && st->codecpar->height == 720))
      {
        if (*nBaseIndex == -1)
          *nBaseIndex = i;
        else {
          AVProgram *pBaseProgram = av_find_program_from_stream(fmt, nullptr, *nBaseIndex);
          AVProgram *pNewProgram = av_find_program_from_stream(fmt, nullptr, i);
          if (!pBaseProgram && pNewProgram) {
            *nBaseIndex = i;
            continue;
          }
          else if (!pNewProgram && pBaseProgram) {
            continue;
          }

          DbgLog((LOG_TRACE, 10, L" -> Multiple H264 MVC base streams, unsupported."));
          bResult = false;
        }
      }
    }
  }

  bResult = bResult && *nBaseIndex >= 0 && *nExtensionIndex >= 0;
  return bResult;
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
