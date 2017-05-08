/*
 *      Copyright (C) 2010-2017 Hendrik Leppkes
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

  parser.BitSkip(32); // general_profile_compatibility_flag[32]
  parser.BitSkip(1); // general_progressive_source_flag
  parser.BitSkip(1); // general_interlaced_source_flag
  parser.BitSkip(1); // general_non_packed_constraint_flag
  parser.BitSkip(1); // general_frame_only_constraint_flag
  parser.BitSkip(22); // general_reserved_zero_44bits
  parser.BitSkip(22); // general_reserved_zero_44bits
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

  return S_OK;
}
