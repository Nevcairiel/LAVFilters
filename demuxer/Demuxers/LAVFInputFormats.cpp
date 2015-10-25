/*
 *      Copyright (C) 2010-2015 Hendrik Leppkes
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

static const char *RAW_VIDEO = "rawvideo";
static const char *RAW_VIDEO_DESC = "raw video files";

static const char *RAW_AUDIO = "rawaudio";
static const char *RAW_AUDIO_DESC = "raw audio files";

struct lavf_iformat_map
{
  const char *orig_format;
  const char *new_format;
  const char *new_description;
} lavf_input_formats[] = {
  // Shorten these formats
  { "matroska,webm",           "matroska", nullptr },
  { "mov,mp4,m4a,3gp,3g2,mj2", "mp4",      "MPEG-4/QuickTime format" },
  { "yuv4mpegpipe",            "y4m",      "YUV4MPEG" },
  { "asf_o",                   "asf",      nullptr },

  // Raw Video formats (grouped into "rawvideo")
  { "dirac", RAW_VIDEO, RAW_VIDEO_DESC },
  { "dnxhd", RAW_VIDEO, RAW_VIDEO_DESC },
  { "h261",  RAW_VIDEO, RAW_VIDEO_DESC },
  { "h263",  RAW_VIDEO, RAW_VIDEO_DESC },
  { "h264",  RAW_VIDEO, RAW_VIDEO_DESC },
  { "hevc",  RAW_VIDEO, RAW_VIDEO_DESC },
  { "ingenient", RAW_VIDEO, RAW_VIDEO_DESC },
  { "mjpeg", RAW_VIDEO, RAW_VIDEO_DESC },
  { "vc1",   RAW_VIDEO, RAW_VIDEO_DESC },

  // Raw Audio Formats (grouped into "rawaudio")
  { "f32be", RAW_AUDIO, RAW_AUDIO_DESC },
  { "f32le", RAW_AUDIO, RAW_AUDIO_DESC },
  { "f64be", RAW_AUDIO, RAW_AUDIO_DESC },
  { "f64le", RAW_AUDIO, RAW_AUDIO_DESC },
  { "g722",  RAW_AUDIO, RAW_AUDIO_DESC },
  { "gsm",   RAW_AUDIO, RAW_AUDIO_DESC },
  { "s16be", RAW_AUDIO, RAW_AUDIO_DESC },
  { "s16le", RAW_AUDIO, RAW_AUDIO_DESC },
  { "s24be", RAW_AUDIO, RAW_AUDIO_DESC },
  { "s24le", RAW_AUDIO, RAW_AUDIO_DESC },
  { "s32be", RAW_AUDIO, RAW_AUDIO_DESC },
  { "s32le", RAW_AUDIO, RAW_AUDIO_DESC },
  { "s8",    RAW_AUDIO, RAW_AUDIO_DESC },
  { "u16be", RAW_AUDIO, RAW_AUDIO_DESC },
  { "u16le", RAW_AUDIO, RAW_AUDIO_DESC },
  { "u24be", RAW_AUDIO, RAW_AUDIO_DESC },
  { "u24le", RAW_AUDIO, RAW_AUDIO_DESC },
  { "u32be", RAW_AUDIO, RAW_AUDIO_DESC },
  { "u32le", RAW_AUDIO, RAW_AUDIO_DESC },
  { "u8",    RAW_AUDIO, RAW_AUDIO_DESC },
  { "image2", "image2", "Image Files" },
  { "image2pipe", "image2", "Image Files" },

  // Disabled Formats
  { "ffm", nullptr, nullptr },
  { "ffmetadata", nullptr, nullptr },
  { "mpegtsraw", nullptr, nullptr },
  { "spdif", nullptr, nullptr },
  { "tty", nullptr, nullptr },
  { "vc1test", nullptr, nullptr },
};

void lavf_get_iformat_infos(AVInputFormat *pFormat, const char **pszName, const char **pszDescription)
{
  const char *name = pFormat->name;
  const char *desc = pFormat->long_name;
  for (int i=0; i<countof(lavf_input_formats); ++i) {
    if (strcmp(lavf_input_formats[i].orig_format, name) == 0) {
      name = lavf_input_formats[i].new_format;
      if (lavf_input_formats[i].new_description)
        desc = lavf_input_formats[i].new_description;
      break;
    }
  }

  if (pszName)
    *pszName = name;
  if (pszDescription)
    *pszDescription = desc;
}
