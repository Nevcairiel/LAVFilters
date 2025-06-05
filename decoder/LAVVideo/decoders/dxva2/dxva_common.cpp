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

#include "stdafx.h"
#include "dxva_common.h"
#include "moreuuids.h"

extern "C"
{
#include "libavcodec/h265_rext_profiles.h"
};

#define DXVA_SURFACE_BASE_ALIGN 16

DWORD dxva_align_dimensions(AVCodecID codec, DWORD dim)
{
  int align = DXVA_SURFACE_BASE_ALIGN;

  // MPEG-2 needs higher alignment on Intel cards, and it doesn't seem to harm anything to do it for all cards.
  if (codec == AV_CODEC_ID_MPEG2VIDEO)
    align <<= 1;
  else if (codec == AV_CODEC_ID_HEVC || codec == AV_CODEC_ID_AV1)
    align = 128;

  return FFALIGN(dim, align);
}

////////////////////////////////////////////////////////////////////////////////
// Codec Maps
////////////////////////////////////////////////////////////////////////////////

/*
DXVA2 Codec Mappings, as defined by VLC
*/

static const int prof_mpeg2_main[] = { AV_PROFILE_MPEG2_SIMPLE, AV_PROFILE_MPEG2_MAIN, AV_PROFILE_UNKNOWN };
static const int prof_h264_high[] = { AV_PROFILE_H264_CONSTRAINED_BASELINE, AV_PROFILE_H264_MAIN, AV_PROFILE_H264_HIGH, AV_PROFILE_UNKNOWN };
static const int prof_hevc_main[] = { AV_PROFILE_HEVC_MAIN, AV_PROFILE_UNKNOWN };
static const int prof_hevc_main10[] = { AV_PROFILE_HEVC_MAIN_10, AV_PROFILE_UNKNOWN };
static const int prof_hevc_rext[] = {AV_PROFILE_HEVC_REXT, AV_PROFILE_UNKNOWN};
static const int prof_vp9_0[] = { AV_PROFILE_VP9_0, AV_PROFILE_UNKNOWN };
static const int prof_vp9_2_10bit[] = { AV_PROFILE_VP9_2, AV_PROFILE_UNKNOWN };
static const int prof_av1_0[] = {AV_PROFILE_AV1_MAIN, AV_PROFILE_UNKNOWN};
static const int prof_av1_1[] = {AV_PROFILE_AV1_HIGH, AV_PROFILE_UNKNOWN};
static const int prof_av1_2[] = {AV_PROFILE_AV1_PROFESSIONAL, AV_PROFILE_UNKNOWN};

/* XXX Preferred modes must come first */
// clang-format off
const dxva_mode_t dxva_modes[] = {
  /* MPEG-1/2 */
  { "MPEG-2 variable-length decoder",                                               &DXVA2_ModeMPEG2_VLD,                   AV_CODEC_ID_MPEG2VIDEO, prof_mpeg2_main },
  { "MPEG-2 & MPEG-1 variable-length decoder",                                      &DXVA2_ModeMPEG2and1_VLD,               AV_CODEC_ID_MPEG2VIDEO, prof_mpeg2_main },
  { "MPEG-2 motion compensation",                                                   &DXVA2_ModeMPEG2_MoComp,                0 },
  { "MPEG-2 inverse discrete cosine transform",                                     &DXVA2_ModeMPEG2_IDCT,                  0 },

  { "MPEG-1 variable-length decoder",                                               &DXVA2_ModeMPEG1_VLD,                   0 },

  /* H.264 */
  { "H.264 variable-length decoder, film grain technology",                         &DXVA2_ModeH264_F,                      AV_CODEC_ID_H264, prof_h264_high },
  { "H.264 variable-length decoder, no film grain technology",                      &DXVA2_ModeH264_E,                      AV_CODEC_ID_H264, prof_h264_high },
  { "H.264 variable-length decoder, no film grain technology, FMO/ASO",             &DXVA_ModeH264_VLD_WithFMOASO_NoFGT,    AV_CODEC_ID_H264, prof_h264_high },
  { "H.264 variable-length decoder, no film grain technology, Flash",               &DXVA_ModeH264_VLD_NoFGT_Flash,         AV_CODEC_ID_H264, prof_h264_high },

  { "H.264 inverse discrete cosine transform, film grain technology",               &DXVA2_ModeH264_D,                      0 },
  { "H.264 inverse discrete cosine transform, no film grain technology",            &DXVA2_ModeH264_C,                      0 },

  { "H.264 motion compensation, film grain technology",                             &DXVA2_ModeH264_B,                      0 },
  { "H.264 motion compensation, no film grain technology",                          &DXVA2_ModeH264_A,                      0 },

  /* WMV */
  { "Windows Media Video 8 motion compensation",                                    &DXVA2_ModeWMV8_B,                      0 },
  { "Windows Media Video 8 post processing",                                        &DXVA2_ModeWMV8_A,                      0 },

  { "Windows Media Video 9 IDCT",                                                   &DXVA2_ModeWMV9_C,                      0 },
  { "Windows Media Video 9 motion compensation",                                    &DXVA2_ModeWMV9_B,                      0 },
  { "Windows Media Video 9 post processing",                                        &DXVA2_ModeWMV9_A,                      0 },

  /* VC-1 */
  { "VC-1 variable-length decoder (2010)",                                          &DXVA2_ModeVC1_D2010,                   AV_CODEC_ID_VC1 },
  { "VC-1 variable-length decoder (2010)",                                          &DXVA2_ModeVC1_D2010,                   AV_CODEC_ID_WMV3 },
  { "VC-1 variable-length decoder",                                                 &DXVA2_ModeVC1_D,                       AV_CODEC_ID_VC1 },
  { "VC-1 variable-length decoder",                                                 &DXVA2_ModeVC1_D,                       AV_CODEC_ID_WMV3 },

  { "VC-1 inverse discrete cosine transform",                                       &DXVA2_ModeVC1_C,                       0 },
  { "VC-1 motion compensation",                                                     &DXVA2_ModeVC1_B,                       0 },
  { "VC-1 post processing",                                                         &DXVA2_ModeVC1_A,                       0 },

  /* MPEG4-ASP */
  { "MPEG-4 Part 2 nVidia bitstream decoder",                                       &DXVA_nVidia_MPEG4_ASP,                 0 },
  { "MPEG-4 Part 2 variable-length decoder, Simple Profile",                        &DXVA_ModeMPEG4pt2_VLD_Simple,          0 },
  { "MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, no GMC",       &DXVA_ModeMPEG4pt2_VLD_AdvSimple_NoGMC, 0 },
  { "MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, GMC",          &DXVA_ModeMPEG4pt2_VLD_AdvSimple_GMC,   0 },
  { "MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, Avivo",        &DXVA_ModeMPEG4pt2_VLD_AdvSimple_Avivo, 0 },

  /* H.264 MVC */
  { "H.264 MVC variable-length decoder, stereo, progressive",                       &DXVA_ModeH264_VLD_Stereo_Progressive_NoFGT, 0 },
  { "H.264 MVC variable-length decoder, stereo",                                    &DXVA_ModeH264_VLD_Stereo_NoFGT,             0 },
  { "H.264 MVC variable-length decoder, multiview",                                 &DXVA_ModeH264_VLD_Multiview_NoFGT,          0 },

  /* H.264 SVC */
  { "H.264 SVC variable-length decoder, baseline",                                  &DXVA_ModeH264_VLD_SVC_Scalable_Baseline,                    0 },
  { "H.264 SVC variable-length decoder, constrained baseline",                      &DXVA_ModeH264_VLD_SVC_Restricted_Scalable_Baseline,         0 },
  { "H.264 SVC variable-length decoder, high",                                      &DXVA_ModeH264_VLD_SVC_Scalable_High,                        0 },
  { "H.264 SVC variable-length decoder, constrained high progressive",              &DXVA_ModeH264_VLD_SVC_Restricted_Scalable_High_Progressive, 0 },

  /* HEVC / H.265 */
  { "HEVC / H.265 variable-length decoder, main",                                   &DXVA_ModeHEVC_VLD_Main,                AV_CODEC_ID_HEVC, prof_hevc_main },
  { "HEVC / H.265 variable-length decoder, main10",                                 &DXVA_ModeHEVC_VLD_Main10,              AV_CODEC_ID_HEVC, prof_hevc_main10 },
  { "HEVC / H.265 variable-length decoder, main12",                                 &DXVA_ModeHEVC_VLD_Main12,              AV_CODEC_ID_HEVC, prof_hevc_rext, FF_HEVC_REXT_PROFILE_MAIN_12 },
  { "HEVC / H.265 variable-length decoder, main10 422",                             &DXVA_ModeHEVC_VLD_Main10_422,          AV_CODEC_ID_HEVC, prof_hevc_rext, FF_HEVC_REXT_PROFILE_MAIN422_10 },
  { "HEVC / H.265 variable-length decoder, main12 422",                             &DXVA_ModeHEVC_VLD_Main12_422,          AV_CODEC_ID_HEVC, prof_hevc_rext, FF_HEVC_REXT_PROFILE_MAIN422_12 },
  { "HEVC / H.265 variable-length decoder, main 444",                               &DXVA_ModeHEVC_VLD_Main_444,            AV_CODEC_ID_HEVC, prof_hevc_rext, FF_HEVC_REXT_PROFILE_MAIN444 },
  { "HEVC / H.265 variable-length decoder, main10 extended",                        &DXVA_ModeHEVC_VLD_Main10_Ext,          0 },
  { "HEVC / H.265 variable-length decoder, main10 444",                             &DXVA_ModeHEVC_VLD_Main10_444,          AV_CODEC_ID_HEVC, prof_hevc_rext, FF_HEVC_REXT_PROFILE_MAIN444_10 },
  { "HEVC / H.265 variable-length decoder, main12 444",                             &DXVA_ModeHEVC_VLD_Main12_444,          AV_CODEC_ID_HEVC, prof_hevc_rext, FF_HEVC_REXT_PROFILE_MAIN444_12 },
  { "HEVC / H.265 variable-length decoder, main16",                                 &DXVA_ModeHEVC_VLD_Main16,              AV_CODEC_ID_HEVC, prof_hevc_rext, FF_HEVC_REXT_PROFILE_MAIN444_16 },
  { "HEVC / H.265 variable-length decoder, monochrome",                             &DXVA_ModeHEVC_VLD_Monochrome,          0 },
  { "HEVC / H.265 variable-length decoder, monochrome10",                           &DXVA_ModeHEVC_VLD_Monochrome10,        0 },

  /* VP8/9 */
  { "VP9 variable-length decoder, profile 0",                                       &DXVA_ModeVP9_VLD_Profile0,             AV_CODEC_ID_VP9, prof_vp9_0 },
  { "VP9 variable-length decoder, 10bit, profile 2",                                &DXVA_ModeVP9_VLD_10bit_Profile2,       AV_CODEC_ID_VP9, prof_vp9_2_10bit },
  { "VP8 variable-length decoder",                                                  &DXVA_ModeVP8_VLD,                      0 },

  /* AV1 */
  { "AV1 variable-length decoder, profile 0",                                       &DXVA_ModeAV1_VLD_Profile0,             AV_CODEC_ID_AV1, prof_av1_0 },
  { "AV1 variable-length decoder, profile 1",                                       &DXVA_ModeAV1_VLD_Profile1,             0 },
  { "AV1 variable-length decoder, profile 2",                                       &DXVA_ModeAV1_VLD_Profile2,             0 },
  { "AV1 variable-length decoder, profile 2 12-bit",                                &DXVA_ModeAV1_VLD_12bit_Profile2,       0 },
  { "AV1 variable-length decoder, profile 2 12-bit 4:2:0",                          &DXVA_ModeAV1_VLD_12bit_Profile2_420,   0 },

  /* Intel specific modes (only useful on older GPUs) */
  { "H.264 variable-length decoder, no film grain technology (Intel ClearVideo)",   &DXVADDI_Intel_ModeH264_E,              AV_CODEC_ID_H264, prof_h264_high },
  { "H.264 inverse discrete cosine transform, no film grain technology (Intel)",    &DXVADDI_Intel_ModeH264_C,              0 },
  { "H.264 motion compensation, no film grain technology (Intel)",                  &DXVADDI_Intel_ModeH264_A,              0 },
  { "VC-1 variable-length decoder 2 (Intel)",                                       &DXVA_Intel_VC1_ClearVideo_2,           0 },
  { "VC-1 variable-length decoder (Intel)",                                         &DXVA_Intel_VC1_ClearVideo,             0 },

  { nullptr, nullptr, 0 }
};
// clang-format on

const dxva_mode_t *get_dxva_mode_from_guid(const GUID *guid)
{
  for (unsigned i = 0; dxva_modes[i].name; i++) {
    if (IsEqualGUID(*dxva_modes[i].guid, *guid))
      return &dxva_modes[i];
  }
  return nullptr;
}

int check_dxva_mode_compatibility(const dxva_mode_t *mode, int codec, int profile, int level, bool b8Bit)
{
  if (mode->codec != codec)
    return 0;

  if (mode->profiles && profile != AV_PROFILE_UNKNOWN)
  {
    for (int i = 0; mode->profiles[i] != AV_PROFILE_UNKNOWN; i++)
    {
        if (mode->profiles[i] == profile && (mode->level == 0 || mode->level == level))
            return 1;
    }

    /* hevc main and main10 are very similar, and in some cases streams can be flagged as main10, but actually contain 8-bit content */
    if (codec == AV_CODEC_ID_HEVC && mode->profiles[0] == AV_PROFILE_HEVC_MAIN && profile == AV_PROFILE_HEVC_MAIN_10 && b8Bit)
        return 1;

    return 0;
  }

  return 1;
}

#define H264_MAX_REF_DPB(ctx, dpbsize, dpblimit, mbcount) (min(dpblimit, (dpbsize) / (mbcount)))

int check_dxva_codec_profile(const AVCodecContext *ctx, int hwpixfmt)
{
    AVCodecID codec = ctx->codec_id;
    AVPixelFormat pix_fmt = ctx->pix_fmt;
    int profile = ctx->profile;
    int level = ctx->level;

    // check mpeg2 pixfmt
    if (codec == AV_CODEC_ID_MPEG2VIDEO && pix_fmt != AV_PIX_FMT_YUV420P && pix_fmt != AV_PIX_FMT_YUVJ420P && pix_fmt != hwpixfmt && pix_fmt != AV_PIX_FMT_NONE)
        return 1;

    // check h264 pixfmt
    if (codec == AV_CODEC_ID_H264 && pix_fmt != AV_PIX_FMT_YUV420P && pix_fmt != AV_PIX_FMT_YUVJ420P && pix_fmt != hwpixfmt && pix_fmt != AV_PIX_FMT_NONE)
        return 1;

    // check h264 profile
    if (codec == AV_CODEC_ID_H264 && profile != AV_PROFILE_UNKNOWN && !H264_CHECK_PROFILE(profile))
        return 1;

    // H.264 Level 5.1 ref frame limits
    const int h264mb_count = (ctx->coded_width / 16) * (ctx->coded_height / 16);
    if (codec == AV_CODEC_ID_H264 && h264mb_count > 0 && ctx->refs > H264_MAX_REF_DPB(ctx, 184320, 16, h264mb_count))
        return 1;

    // check wmv/vc1 profile
    if ((codec == AV_CODEC_ID_WMV3 || codec == AV_CODEC_ID_VC1) && profile == AV_PROFILE_VC1_COMPLEX)
        return 1;

    // check vp9 profile/pixfmt
    if (codec == AV_CODEC_ID_VP9 && (!VP9_CHECK_PROFILE(profile) || (pix_fmt != AV_PIX_FMT_YUV420P && pix_fmt != AV_PIX_FMT_YUV420P10 && pix_fmt != hwpixfmt && pix_fmt != AV_PIX_FMT_NONE)))
        return 1;

    // check av1 profile/pixfmt
    if (codec == AV_CODEC_ID_AV1 && (profile != AV_PROFILE_AV1_MAIN || (pix_fmt != AV_PIX_FMT_YUV420P  && pix_fmt != AV_PIX_FMT_YUV420P10 && pix_fmt != hwpixfmt && pix_fmt != AV_PIX_FMT_NONE)))
        return 1;

    return 0;
}
