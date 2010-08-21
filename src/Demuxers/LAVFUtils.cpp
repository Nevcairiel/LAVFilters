/*
 *      Copyright (C) 2010 Hendrik Leppkes
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
    bit_rate = bits_per_sample ? ctx->sample_rate * ctx->channels * bits_per_sample : ctx->bit_rate;
    break;
  default:
    bit_rate = 0;
    break;
  }
  return bit_rate;
}

HRESULT lavf_describe_stream(AVStream *pStream, WCHAR **ppszName)
{
  CheckPointer(pStream, E_POINTER);
  CheckPointer(ppszName, E_POINTER);

  AVCodecContext *enc = pStream->codec;

  char tmpbuf1[32];

  // Grab the codec
  const char *codec_name;
  AVCodec *p = avcodec_find_decoder(enc->codec_id);

  if (p) {
    codec_name = p->name;
  } else if (enc->codec_name[0] != '\0') {
    codec_name = enc->codec_name;
  } else {
    /* output avi tags */
    char tag_buf[32];
    av_get_codec_tag_string(tag_buf, sizeof(tag_buf), enc->codec_tag);
    sprintf_s(tmpbuf1, "%s / 0x%04X", tag_buf, enc->codec_tag);
    codec_name = tmpbuf1;
  }

  char *lang = NULL;
  std::string sLanguage;
  if (av_metadata_get(pStream->metadata, "language", NULL, 0)) {
    lang = av_metadata_get(pStream->metadata, "language", NULL, 0)->value;
    sLanguage = ProbeLangForLanguage(lang);
    if (sLanguage.empty()) {
      sLanguage = lang;
    }
  }

  char *title = NULL;
  if (av_metadata_get(pStream->metadata, "title", NULL, 0)) {
    title = av_metadata_get(pStream->metadata, "title", NULL, 0)->value;
  }

  int bitrate = get_bit_rate(enc);


  std::ostringstream buf;
  switch(enc->codec_type) {
  case AVMEDIA_TYPE_VIDEO:
    buf << "V: ";
    // Title/Language
    if (title && lang) {
      buf << title << " [" << lang << "] (";
    } else if (title || lang) {
      // Print either title or lang
      buf << (title ? title : sLanguage) << " (";
    }
    // Codec
    buf << codec_name;
    // Pixel Format
    if (enc->pix_fmt != PIX_FMT_NONE) {
      buf << ", " << avcodec_get_pix_fmt_name(enc->pix_fmt);
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
    break;
  case AVMEDIA_TYPE_AUDIO:
    buf << "A: ";
    // Title/Language
    if (title && lang) {
      buf << title << " [" << lang << "] (";
    } else if (title || lang) {
      // Print either title or lang
      buf << (title ? title : sLanguage) << " (";
    }
    // Codec
    buf << codec_name;
    // Sample Rate
    if (enc->sample_rate) {
      buf << ", " << enc->sample_rate << " Hz";
    }
    // Get channel layout
    char channel[32];
    avcodec_get_channel_layout_string(channel, 32, enc->channels, enc->channel_layout);
    buf << ", " << channel;
    // Sample Format
    if (enc->sample_fmt != SAMPLE_FMT_NONE) {
      buf << ", " << avcodec_get_sample_fmt_name(enc->sample_fmt);
    }
    // Bitrate
    if (bitrate > 0) {
      buf << ", " << (bitrate / 1000) << " kb/s";
    }
    // Closing tag
    if (title || lang) {
      buf << ")";
    }
    break;
  case AVMEDIA_TYPE_SUBTITLE:
    buf << "S: ";
    // Title/Language
    if (title && lang) {
      buf << title << " [" << lang << "]";
    } else if (title || lang) {
      // Print either title or lang
      buf << (title ? title : sLanguage);
    } else {
      buf << "Stream #" << pStream->index;
    }
    break;
  default:
    buf << "Unknown: Stream " << pStream->index;
    break;
  }

  std::string info = buf.str();
  size_t len = info.size() + 1;
  *ppszName = (WCHAR *)CoTaskMemAlloc(len * sizeof(WCHAR));
  MultiByteToWideChar(CP_UTF8, 0, info.c_str(), -1, *ppszName, len);

  return S_OK;
}