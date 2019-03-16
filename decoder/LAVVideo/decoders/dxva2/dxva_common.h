/*
*      Copyright (C) 2011-2019 Hendrik Leppkes
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

/* Align dimensions for hardware and codec requirements */
DWORD dxva_align_dimensions(AVCodecID codec, DWORD dim);

/* hardware mode description */
typedef struct {
  const char   *name;
  const GUID   *guid;
  int          codec;
  const int    *profiles;
  int          high_bit_depth;
} dxva_mode_t;

extern const dxva_mode_t dxva_modes[];

const dxva_mode_t *get_dxva_mode_from_guid(const GUID *guid);
int check_dxva_mode_compatibility(const dxva_mode_t *mode, int codec, int profile);

int check_dxva_codec_profile(AVCodecID codec, AVPixelFormat pix_fmt, int profile, int hwpixfmt);

#define H264_CHECK_PROFILE(profile) \
  (((profile) & ~FF_PROFILE_H264_CONSTRAINED) <= FF_PROFILE_H264_HIGH)

#define HEVC_CHECK_PROFILE(profile) \
  ((profile) <= FF_PROFILE_HEVC_MAIN_10)

#define VP9_CHECK_PROFILE(profile) \
  ((profile) == FF_PROFILE_VP9_0 || (profile) == FF_PROFILE_VP9_2)
