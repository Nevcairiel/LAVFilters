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
#include "HEVCSequenceParser.h"

#include "H264Nalu.h"

#define HEVC_NAL_VPS 32
#define HEVC_NAL_SPS 33
#define HEVC_NAL_PPS 34

CHEVCSequenceParser::CHEVCSequenceParser()
{
  ZeroMemory(&sps, sizeof(sps));
  sps.bitdepth = 8;
}

CHEVCSequenceParser::~CHEVCSequenceParser()
{
}


HRESULT CHEVCSequenceParser::ParseNALs(const BYTE *buffer, size_t buflen, int nal_size)
{
  CH265Nalu nalu;
  nalu.SetBuffer(buffer, buflen, nal_size);

  while (nalu.ReadNext())  {
    const BYTE *data = nalu.GetDataBuffer() + 2;
    const size_t len = nalu.GetDataLength() - 2;
    if (nalu.GetType() == HEVC_NAL_SPS) {
      CH264NALUnescape unescapedNAL(data, len);
      ParseSPS(unescapedNAL.GetBuffer(), unescapedNAL.GetSize());
      break;
    }
  }

  return S_OK;
}

HRESULT CHEVCSequenceParser::ParseSPS(const BYTE *buffer, size_t buflen)
{
  CByteParser parser(buffer, buflen);
  int i;

  ZeroMemory(&sps, sizeof(sps));
  sps.valid = 1;

  struct {
    int profile_present;
    int level_present;
  } Sublayers[7];

  parser.BitSkip(4); // sps_video_parameter_set_id
  int max_sub_layers = parser.BitRead(3); // sps_max_sub_layers_minus1
  parser.BitSkip(1); // sps_temporal_id_nesting_flag

  // profile tier level
  parser.BitSkip(2); // general_profile_space
  parser.BitSkip(1); // general_tier_flag
  sps.profile = parser.BitRead(5); // general_profile_idc

  for (i = 0; i < 32; i++)
  {
    int flag = parser.BitRead(1);
    if (sps.profile == 0 && i > 0 && flag)
      sps.profile = i;
  }
  parser.BitSkip(1); // general_progressive_source_flag
  parser.BitSkip(1); // general_interlaced_source_flag
  parser.BitSkip(1); // general_non_packed_constraint_flag
  parser.BitSkip(1); // general_frame_only_constraint_flag
  if (sps.profile == 4) {
    sps.rext_profile = parser.BitRead(8); // 8 constraint flags

    parser.BitSkip(1);  // general_lower_bit_rate_constraint_flag
    parser.BitSkip(17); // general_reserved_zero_34bits
    parser.BitSkip(17); // general_reserved_zero_34bits
  }
  else {
    parser.BitSkip(21); // general_reserved_zero_43bits
    parser.BitSkip(22); // general_reserved_zero_43bits
  }
  parser.BitSkip(1); // general_reserved_zero_bit
  sps.level = parser.BitRead(8); // general_level_idc

  if (max_sub_layers > 7)
    return E_FAIL;

  for (i = 0; i < max_sub_layers; i++) {
    Sublayers[i].profile_present = parser.BitRead(1); // sub_layer_profile_present_flag
    Sublayers[i].level_present = parser.BitRead(1); // sub_layer_level_present_flag
  }

  if (max_sub_layers > 0) {
    for (i = max_sub_layers; i < 8; i++)
      parser.BitSkip(2); // reserved_zero_2bits
  }

  for (i = 0; i < max_sub_layers; i++) {
    if (Sublayers[i].profile_present) {
      parser.BitSkip(2); // general_profile_space
      parser.BitSkip(1); // general_tier_flag
      parser.BitSkip(5); // general_profile_idc

      parser.BitSkip(32); // profile_compatibility_flag[32]
      parser.BitSkip(1); // progressive_source_flag
      parser.BitSkip(1); // interlaced_source_flag
      parser.BitSkip(1); // non_packed_constraint_flag
      parser.BitSkip(1); // frame_only_constraint_flag
      parser.BitSkip(22); // reserved_zero_44bits
      parser.BitSkip(22); // reserved_zero_44bits
    }
    if (Sublayers[i].level_present) {
      parser.BitSkip(8); // level_idc
    }
  }

  parser.UExpGolombRead(); // sps_id
  sps.chroma = parser.UExpGolombRead(); // chroma_format_idc
  if (sps.chroma == 3)
    parser.BitRead(1); // separate_color_plane_flag

  parser.UExpGolombRead(); // width
  parser.UExpGolombRead(); // height

  if (parser.BitRead(1)) { // conformance_window_flag
    parser.UExpGolombRead(); // left offset
    parser.UExpGolombRead(); // right offset
    parser.UExpGolombRead(); // top offset
    parser.UExpGolombRead(); // bottom offset
  }

  sps.bitdepth = parser.UExpGolombRead() + 8; // bit_depth_luma_minus8
  parser.UExpGolombRead(); // bit_depth_chroma_minus8

  /*int log2_max_pic_order_count = parser.UExpGolombRead() + 4; // log2_max_pic_order_cnt_lsb_minus4

  for (i = (parser.BitRead(1) ? 0 : max_sub_layers); i <= max_sub_layers; i++) {
    parser.UExpGolombRead(); // sps_max_dec_pic_buffering_minus1
    parser.UExpGolombRead(); // sps_max_num_reorder_pics
    parser.UExpGolombRead(); // sps_max_latency_increase_plus1
  }

  parser.UExpGolombRead(); // log2_min_luma_coding_block_size_minus3
  parser.UExpGolombRead(); // log2_diff_max_min_luma_coding_block_size
  parser.UExpGolombRead(); // log2_min_luma_transform_block_size_minus2
  parser.UExpGolombRead(); // log2_diff_max_min_luma_transform_block_size
  parser.UExpGolombRead(); // max_transform_hierarchy_depth_inter
  parser.UExpGolombRead(); // max_transform_hierarchy_depth_intra

  // scaling list
  if (parser.BitRead(1)) { // scaling_list_enabled_flag
    if (parser.BitRead(1)) { // sps_scaling_list_data_present_flag
      for (int sizeId = 0; sizeId < 4; sizeId++) {
        for (int matrixId = 0; matrixId < 6; matrixId += ((sizeId == 3) ? 3 : 1)) {
          if (!parser.BitRead(1)) { // scaling_list_pred_mode_flag
            parser.UExpGolombRead(); // scaling_list_pred_matrix_id_delta
          }
          else {
            if (sizeId > 1)
              parser.SExpGolombRead();

            int coefNum = min(64, 1 << (4 + (sizeId << 1)));
            for (i = 0; i < coefNum; i++)
              parser.SExpGolombRead(); // scaling_list_delta_coef
          }
        }
      }
    }
  }

  parser.BitSkip(1); // amp_enabled_flag
  parser.BitSkip(1); // sao_enabled_flag
  if (parser.BitRead(1)) { // pcm_enabled_flag
    parser.BitSkip(4); // pcm_sample_bit_depth_luma_minus1
    parser.BitSkip(4); // pcm_sample_bit_depth_chroma_minus1
    parser.UExpGolombRead(); // log2_min_pcm_luma_coding_block_size_minus3
    parser.UExpGolombRead(); // log2_diff_max_min_pcm_luma_coding_block_size
    parser.BitSkip(1); // pcm_loop_filter_disabled_flag
  }

  int num_short_term_ref_pic_set = parser.UExpGolombRead();
  if (num_short_term_ref_pic_set > 64)
    return E_FAIL;

  // st_ref_pic_set
  for (i = 0; i < num_short_term_ref_pic_set; i++) {
    if (i > 0 && parser.BitRead(1)) { // inter_ref_pic_set_prediction_flag
      if (i == num_short_term_ref_pic_set) // only in slice header?
        parser.UExpGolombRead(); // delta_idx_minus1

      parser.BitSkip(1); // delta_rps_sign
      parser.UExpGolombRead(); // abs_delta_rps_minus1
      for (j = 0; j <= ShortTermRPS[i - 1].NumDeltaPocs; j++) {
        if (!parser.BitRead(1)) // used_by_curr_pic_flag
          parser.BitRead(1); // use_delta_flag
      }
    }
    else {
      int num_negative = parser.UExpGolombRead();
      int num_positive = parser.UExpGolombRead();
      ShortTermRPS[i].NumDeltaPocs = num_negative + num_positive;

      for (j = 0; j < num_negative; j++) {
        parser.UExpGolombRead(); // delta_poc_s0_minus1
        parser.BitSkip(1); // used_by_curr_pic_s0_flag
      }

      for (j = 0; j < num_positive; j++) {
        parser.UExpGolombRead(); // delta_poc_s0_minus1
        parser.BitSkip(1); // used_by_curr_pic_s0_flag
      }
    }
  }

  if (parser.BitRead(1)) { // long_term_ref_pics_present_flag
    int num_long_term = parser.UExpGolombRead();
    for (i = 0; i < num_long_term; i++) {
      parser.BitSkip(log2_max_pic_order_count);
      parser.BitSkip(1);
    }
  }

  parser.BitSkip(1); // sps_temporal_mvp_enabled_flag
  parser.BitSkip(1); // strong_intra_smoothing_enabled_flag

  // VUI
  if (parser.BitRead(1)) { // vui_parameters_present_flag
    if (parser.BitRead(1)) { // aspect_ratio_info_present_flag
      if (parser.BitRead(8) == 255) { // aspect_ratio_idc
        parser.BitSkip(16); // sar_width
        parser.BitSkip(16); // sar_height
      }
    }

    if (parser.BitRead(1)) // overscan_info_present_flag
      parser.BitSkip(1); // overscan_appropriate_flag

    if (parser.BitRead(1)) { // video_signal_type_present_flag
      parser.BitSkip(3); // video_format
      parser.BitSkip(1); // video_full_range_flag

      if (parser.BitRead(1)) { // colour_description_present_flag
        parser.BitSkip(8); // colour_primaries
        parser.BitSkip(8); // transfer_characteristics
        parser.BitSkip(8); // matrix_coeffs
      }
    }

    if (parser.BitRead(1)) { // chroma_loc_info_present_flag
      parser.UExpGolombRead(); // chroma_sample_loc_type_top_field
      parser.UExpGolombRead(); // chroma_sample_loc_type_bottom_field
    }

    parser.BitSkip(1); // neutral_chroma_indication_flag
    parser.BitSkip(1); // field_seq_flag
    parser.BitSkip(1); // frame_field_info_present_flag

    if (parser.BitRead(1)) { // default_display_window_flag
      parser.UExpGolombRead(); // def_disp_win_left_offset
      parser.UExpGolombRead(); // def_disp_win_right_offset
      parser.UExpGolombRead(); // def_disp_win_top_offset
      parser.UExpGolombRead(); // def_disp_win_bottom_offset
    }

    if (parser.BitRead(1)) { // vui_timing_info_present_flag
      parser.BitSkip(32); // vui_num_units_in_tick
      parser.BitSkip(32); // vui_time_scale

      if (parser.BitRead(1)) // vui_poc_proportional_to_timing_flag
        parser.UExpGolombRead(); // vui_num_ticks_poc_diff_one_minus1

      // hrd parameters
      if (parser.BitRead(1)) { // vui_hrd_parameters_present_flag
        int nal_hrd_parameters_present = parser.BitRead(1);
        int vcl_hrd_parameters_present = parser.BitRead(1);
        int sub_hrd_parameters_present = 0;

        if (nal_hrd_parameters_present || vcl_hrd_parameters_present) {
          sub_hrd_parameters_present = parser.BitRead(1);
          if (sub_hrd_parameters_present) {
            parser.BitSkip(8); // tick_divisor_minus2
            parser.BitSkip(5); // du_cpb_removal_delay_increment_length_minus1
            parser.BitSkip(1); // sub_pic_cpb_params_in_pic_timing_sei_flag
            parser.BitSkip(5); // dpb_output_delay_du_length_minus1
          }
          parser.BitSkip(4); // bit_rate_scale
          parser.BitSkip(4); // cbp_size_scale
          if (sub_hrd_parameters_present)
            parser.BitSkip(4); // cbp_size_du_scale

          parser.BitSkip(5); // initial_cpb_removal_delay_length_minus1
          parser.BitSkip(5); // au_cpb_removal_delay_length_minus1
          parser.BitSkip(5); // dpb_output_delay_length_minus1
        }

        for (i = 0; i <= max_sub_layers; i++) {
          int fixed_pic_rate_flag = parser.BitRead(1);
          int fixed_pic_rate_in_cvs_flag = 1, low_delay_hrd = 0;

          if (!fixed_pic_rate_flag)
            fixed_pic_rate_in_cvs_flag = parser.BitRead(1);

          if (fixed_pic_rate_in_cvs_flag)
            parser.UExpGolombRead();
          else
            low_delay_hrd = parser.BitRead(1);

          int cpb_count = 0;
          if (!low_delay_hrd)
            cpb_count = parser.UExpGolombRead();

          if (nal_hrd_parameters_present) {
            for (j = 0; j <= cpb_count; j++) {
              parser.UExpGolombRead(); // bit_rate_value_minus1
              parser.UExpGolombRead(); // cpb_size_value_minus1

              if (sub_hrd_parameters_present) {
                parser.UExpGolombRead(); // bit_rate_du_value_minus1
                parser.UExpGolombRead(); // cpb_size_du_value_minus1
              }

              parser.BitSkip(1);
            }
          }
          if (vcl_hrd_parameters_present) {
            for (j = 0; j <= cpb_count; j++) {
              parser.UExpGolombRead(); // bit_rate_value_minus1
              parser.UExpGolombRead(); // cpb_size_value_minus1

              if (sub_hrd_parameters_present) {
                parser.UExpGolombRead(); // bit_rate_du_value_minus1
                parser.UExpGolombRead(); // cpb_size_du_value_minus1
              }

              parser.BitSkip(1);
            }
          }
        }
      }


    }
  }

  int sps_range_extension_present = 0;
  if (parser.BitRead(1)) { // sps_extension_present_flag
    sps_range_extension_present = parser.BitRead(1);
    parser.BitSkip(1); // sps_multilayer_extension_flag
    parser.BitSkip(1); // sps_3d_extension_flag
    parser.BitSkip(5); // sps_extension_5bits
  }

  if (sps_range_extension_present) {
    sps.range_extension_flags = parser.BitRead(9);
  }*/

  return S_OK;
}
