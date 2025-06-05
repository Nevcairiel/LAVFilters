/*
 *      Copyright (C) 2011-2021 Hendrik Leppkes
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

#define VEND_ID_ATI 0x1002
#define VEND_ID_NVIDIA 0x10DE
#define VEND_ID_INTEL 0x8086

/* hardware mode description */
typedef struct
{
    const char *name;
    const GUID *guid;
    int codec;
    const int *profiles;
    const int level;
} dxva_mode_t;

extern const dxva_mode_t dxva_modes[];

const dxva_mode_t *get_dxva_mode_from_guid(const GUID *guid);
int check_dxva_mode_compatibility(const dxva_mode_t *mode, int codec, int profile, int level, bool b8Bit);

int check_dxva_codec_profile(const AVCodecContext *ctx, int hwpixfmt);

#define H264_CHECK_PROFILE(profile) (((profile) & ~AV_PROFILE_H264_CONSTRAINED) <= AV_PROFILE_H264_HIGH)
#define HEVC_CHECK_PROFILE(profile) ((profile) <= AV_PROFILE_HEVC_MAIN_10)
#define VP9_CHECK_PROFILE(profile) ((profile) == AV_PROFILE_VP9_0 || (profile) == AV_PROFILE_VP9_2)
