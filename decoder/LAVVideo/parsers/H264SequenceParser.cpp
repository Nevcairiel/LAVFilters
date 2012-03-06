/*
 *      Copyright (C) 2010-2012 Hendrik Leppkes
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
#include "H264SequenceParser.h"

#include "ByteParser.h"
#include "H264Nalu.h"


CH264SequenceParser::CH264SequenceParser(void)
{
  ZeroMemory(&sps, sizeof(sps));
  ZeroMemory(&pps, sizeof(pps));
}


CH264SequenceParser::~CH264SequenceParser(void)
{
}

HRESULT CH264SequenceParser::ParseNALs(const BYTE *buffer, size_t buflen, int nal_size)
{
  CH264Nalu nalu;
  nalu.SetBuffer(buffer, buflen, nal_size);

  while (nalu.ReadNext())  {
    const BYTE *data = nalu.GetDataBuffer() + 1;
    const size_t len = nalu.GetDataLength() - 1;
    if (nalu.GetType() == NALU_TYPE_SPS) {
      ParseSPS(data, len);
      break;
    }
  }

  return S_OK;
}

static void SPSDecodeScalingList(CByteParser &parser, int size) {
  int i, last = 8, next = 8;
  int matrix = parser.BitRead(1);
  if (matrix) {
    for (i = 0; i < size; i++) {
      if(next)
        next = (last + parser.SExpGolombRead()) & 0xff;
      if(!i && !next){ /* matrix not written */
          break;
      }
      last = next ? next : last;
    }
  }
}

HRESULT CH264SequenceParser::ParseSPS(const BYTE *buffer, size_t buflen)
{
  CByteParser parser(buffer, buflen);
  int i;

  ZeroMemory(&sps, sizeof(sps));
  // Defaults
  sps.valid = 1;
  sps.primaries = AVCOL_PRI_UNSPECIFIED;
  sps.trc = AVCOL_TRC_UNSPECIFIED;
  sps.colorspace = AVCOL_SPC_UNSPECIFIED;
  sps.full_range = -1;

  // Parse
  sps.profile = parser.BitRead(8);
  parser.BitRead(4); // constraint flags
  parser.BitRead(4); // reserved
  sps.level = parser.BitRead(8);
  parser.UExpGolombRead(); // sps id

  if (sps.profile >= 100) {
    sps.chroma = (int)parser.UExpGolombRead();
    if (sps.chroma == 3)
      parser.BitRead(1);
    sps.luma_bitdepth = (int)parser.UExpGolombRead() + 8;
    sps.chroma_bitdepth = (int)parser.UExpGolombRead() + 8;
    parser.BitRead(1); // transform_bypass

    // decode scaling matrices
    int scaling = parser.BitRead(1);
    if (scaling) {
      // Decode scaling lists
      SPSDecodeScalingList(parser, 16); // Intra, Y
      SPSDecodeScalingList(parser, 16); // Intra, Cr
      SPSDecodeScalingList(parser, 16); // Intra, Cb
      SPSDecodeScalingList(parser, 16); // Inter, Y
      SPSDecodeScalingList(parser, 16); // Inter, Cr
      SPSDecodeScalingList(parser, 16); // Inter, Cb

      SPSDecodeScalingList(parser, 64); // Intra, Y
      if (sps.chroma == 3) {
        SPSDecodeScalingList(parser, 64); // Intra, Cr
        SPSDecodeScalingList(parser, 64); // Intra, Cb
      }
      SPSDecodeScalingList(parser, 64); // Inter, Y
      if (sps.chroma == 3) {
        SPSDecodeScalingList(parser, 64); // Inter, Cr
        SPSDecodeScalingList(parser, 64); // Inter, Cb
      }
    }
  } else {
    sps.chroma = 1;
    sps.luma_bitdepth = 8;
    sps.chroma_bitdepth = 8;
  }

  parser.UExpGolombRead();                // log2_max_frame_num
  int poc_type = (int)parser.UExpGolombRead(); // poc_type
  if (poc_type == 0)
    parser.UExpGolombRead();              // log2_max_poc_lsb
  else if (poc_type == 1) {
    parser.BitRead(1);                    // delta_pic_order_always_zero_flag
    parser.SExpGolombRead();              // offset_for_non_ref_pic
    parser.SExpGolombRead();              // offset_for_top_to_bottom_field
    int cyclen = (int)parser.UExpGolombRead(); // poc_cycle_length
    for (i = 0; i < cyclen; i++)
      parser.SExpGolombRead();            // offset_for_ref_frame[i]
  }

  sps.ref_frames = parser.UExpGolombRead(); // ref_frame_count
  parser.BitRead(1);                      // gaps_in_frame_num_allowed_flag
  parser.UExpGolombRead();                // mb_width
  parser.UExpGolombRead();                // mb_height
  sps.interlaced = !parser.BitRead(1);    // frame_mbs_only_flag
  if (sps.interlaced)
    parser.BitRead(1);                    // mb_aff

  parser.BitRead(1);                      // direct_8x8_inference_flag
  int crop = parser.BitRead(1);           // crop
  if (crop) {
    parser.UExpGolombRead();              // crop_left
    parser.UExpGolombRead();              // crop_right
    parser.UExpGolombRead();              // crop_top
    parser.UExpGolombRead();              // crop_bottom
  }

  int vui_present = parser.BitRead(1);    // vui_parameters_present_flag
  if (vui_present) {
    sps.ar_present = parser.BitRead(1);   // aspect_ratio_info_present_flag
    if (sps.ar_present) {
      int ar_idc = parser.BitRead(8);     // aspect_ratio_idc
      if (ar_idc == 255) {
        parser.BitRead(16);               // sar.num
        parser.BitRead(16);               // sar.den
      }
    }

    int overscan = parser.BitRead(1);     // overscan_info_present_flag
    if (overscan)
      parser.BitRead(1);                  // overscan_appropriate_flag

    int vid_sig_type = parser.BitRead(1); // video_signal_type_present_flag
    if (vid_sig_type) {
      parser.BitRead(3);                  // video_format
      sps.full_range = parser.BitRead(1); // video_full_range_flag

      int colorinfo = parser.BitRead(1);  // colour_description_present_flag
      if (colorinfo) {
        sps.primaries = parser.BitRead(8);
        sps.trc = parser.BitRead(8);
        sps.colorspace = parser.BitRead(8);
      }
    }
  }

  return S_OK;
}
