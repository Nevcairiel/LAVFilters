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

#include "stdafx.h"
#include "lavfutils.h"

#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>

extern "C" {
#include "libavcodec/audioconvert.h"
#include "libavutil/avstring.h"
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

const char *get_stream_language(AVStream *pStream)
{
  char *lang = NULL;
  if (av_metadata_get(pStream->metadata, "language", NULL, 0)) {
    lang = av_metadata_get(pStream->metadata, "language", NULL, 0)->value;
  } else if(pStream->language && strlen(pStream->language) > 0) {
    lang = pStream->language;
  }
  return lang;
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

  const char *lang = get_stream_language(pStream);
  std::string sLanguage;

  if(lang) {
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
    av_get_channel_layout_string(channel, 32, enc->channels, enc->channel_layout);
    buf << ", " << channel;
    // Sample Format
    if (get_bits_per_sample(enc)) {
      if (enc->sample_fmt == AV_SAMPLE_FMT_FLT || enc->sample_fmt == AV_SAMPLE_FMT_DBL) {
        buf << ", fp";
      } else {
        buf << ", s";
      }
      buf << get_bits_per_sample(enc);
    }
    // Bitrate
    if (bitrate > 0) {
      buf << ", " << (bitrate / 1000) << " kb/s";
    }
    // Closing tag
    if (title || lang) {
      buf << ")";
    }
    // Default tag
    if (pStream->disposition & AV_DISPOSITION_DEFAULT)
      buf << " [default]";
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
    // Subtitle flags
    // TODO: This is rather ugly, make it nicer!
    if (pStream->disposition & AV_DISPOSITION_FORCED || pStream->disposition & AV_DISPOSITION_HEARING_IMPAIRED) {
      buf << " [";
      if (pStream->disposition & AV_DISPOSITION_FORCED)
        buf << "forced";
      if (pStream->disposition & AV_DISPOSITION_HEARING_IMPAIRED) {
        if (pStream->disposition & AV_DISPOSITION_FORCED)
          buf << ", ";
        buf << "hearing impaired";
      }
      buf << "]";
    }
    break;
  default:
    buf << "Unknown: Stream #" << pStream->index;
    break;
  }

  std::string info = buf.str();
  size_t len = info.size() + 1;
  *ppszName = (WCHAR *)CoTaskMemAlloc(len * sizeof(WCHAR));
  MultiByteToWideChar(CP_UTF8, 0, info.c_str(), -1, *ppszName, (int)len);

  return S_OK;
}

extern "C" {
static int ufile_open(URLContext *h, const char *filename, int flags)
{
    int access;
    int fd;

    av_strstart(filename, "ufile:", &filename);

    /// 4096 should be enough for a path name
    wchar_t wfilename[4096];
    int nChars = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        filename,
        -1,    // string is NULL terminated
        wfilename,
        sizeof(wfilename) / sizeof(*wfilename)
        );

    if(nChars <= 0) {
        return AVERROR(ENOENT);
    }

    if (flags & URL_RDWR) {
        access = _O_CREAT | _O_TRUNC | _O_RDWR;
    } else if (flags & URL_WRONLY) {
        access = _O_CREAT | _O_TRUNC | _O_WRONLY;
    } else {
        access = _O_RDONLY;
    }
#ifdef O_BINARY
    access |= O_BINARY;
#endif
    _wsopen_s(&fd, wfilename, access, _SH_DENYNO, _S_IREAD | _S_IWRITE);
    if (fd == -1)
        return AVERROR(errno);
    h->priv_data = (void *) (intptr_t) fd;
    return 0;
}

static int ufile_read(URLContext *h, unsigned char *buf, int size)
{
    int fd = (int) h->priv_data;
    return _read(fd, buf, size);
}

static int ufile_write(URLContext *h, const unsigned char *buf, int size)
{
    int fd = (int) h->priv_data;
    return _write(fd, buf, size);
}

static int ufile_get_handle(URLContext *h)
{
    return (int) h->priv_data;
}

/* XXX: use llseek */
static int64_t ufile_seek(URLContext *h, int64_t pos, int whence)
{
    int fd = (int) h->priv_data;
    if (whence == AVSEEK_SIZE) {
        struct stat st;
        int ret = fstat(fd, &st);
        return ret < 0 ? AVERROR(errno) : st.st_size;
    }
    return _lseeki64(fd, pos, whence);
}

static int ufile_close(URLContext *h)
{
    int fd = (int) h->priv_data;
    return _close(fd);
}

URLProtocol ufile_protocol = {
    "ufile",
    ufile_open,
    ufile_read,
    ufile_write,
    ufile_seek,
    ufile_close,
    NULL,
    NULL,
    NULL,
    ufile_get_handle,
};

#ifdef DEBUG
#define LOG_BUF_LEN 1024
void lavf_log_callback(void* ptr, int level, const char* fmt, va_list vl)
{
  static int print_prefix=1;
  static int count;
  static char line[LOG_BUF_LEN], prev[LOG_BUF_LEN];

  AVClass* avc= ptr ? *(AVClass**)ptr : NULL;
  line[0]=0;

  if(print_prefix && avc) {
    if(avc->version >= (50<<16 | 15<<8 | 3) && avc->parent_log_context_offset){
      AVClass** parent= *(AVClass***)(((uint8_t*)ptr) + avc->parent_log_context_offset);
      if(parent && *parent){
        sprintf_s(line, LOG_BUF_LEN, "[%s @ %p] ", (*parent)->item_name(parent), parent);
      }
    }
    sprintf_s(line + strlen(line), LOG_BUF_LEN - strlen(line), "[%s @ %p] ", avc->item_name(ptr), ptr);
  }

  vsnprintf(line + strlen(line), LOG_BUF_LEN - strlen(line), fmt, vl);

  print_prefix= line[strlen(line)-1] == '\n';

  if(print_prefix && !strcmp(line, prev)){
    count++;
    return;
  }
  if(count>0){
    fprintf(stderr, "    Last message repeated %d times\n", count);
    count=0;
  }
  if (line[strlen(line)-1] == '\n') {
    line[strlen(line)-1] = 0;
  }

  DbgLog((LOG_CUSTOM1, level, L"%S", line));
  strcpy_s(prev, LOG_BUF_LEN, line);
}
#endif
}
