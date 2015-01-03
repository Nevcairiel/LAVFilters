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
#include "HEVCSequenceParser.h"

#include "H264Nalu.h"

#define HEVC_NAL_VPS 32
#define HEVC_NAL_SPS 33
#define HEVC_NAL_PPS 34

CHEVCSequenceParser::CHEVCSequenceParser()
{
  ZeroMemory(&sps, sizeof(sps));
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
      ParseSPS(data, len);
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

  parser.BitRead(4); // sps_video_parameter_set_id
  int max_sub_layers = parser.BitRead(3); // sps_max_sub_layers_minus1
  parser.BitRead(1); // sps_temporal_id_nesting_flag

  // profile tier level
  parser.BitRead(2); // general_profile_space
  parser.BitRead(1); // general_tier_flag
  sps.profile = parser.BitRead(5); // general_profile_idc

  parser.BitRead(32); // general_profile_compatibility_flag[32]
  parser.BitRead(1); // general_progressive_source_flag
  parser.BitRead(1); // general_interlaced_source_flag
  parser.BitRead(1); // general_non_packed_constraint_flag
  parser.BitRead(1); // general_frame_only_constraint_flag
  parser.BitRead(22); // general_reserved_zero_44bits
  parser.BitRead(22); // general_reserved_zero_44bits
  sps.level = parser.BitRead(8); // general_level_idc

  for (i = 0; i < max_sub_layers; i++) {
    parser.BitRead(1); // sub_layer_profile_present_flag
    parser.BitRead(1); // sub_layer_level_present_flag
  }

  if (max_sub_layers > 0) {
    for (i = max_sub_layers; i < 8; i++)
      parser.BitRead(2); // reserved_zero_2bits
  }

  return S_OK;
}
