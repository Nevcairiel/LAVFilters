/*
 *      Copyright (C) 2011 Hendrik Leppkes
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

HRESULT CH264SequenceParser::ParseNALs(const BYTE *buffer, int buflen, int nal_size)
{
  CH264Nalu nalu;
  nalu.SetBuffer(buffer, buflen, nal_size);

  while (nalu.ReadNext())  {
    const BYTE *data = nalu.GetDataBuffer() + 1;
    const int len = nalu.GetDataLength() - 1;
    if (nalu.GetType() == NALU_TYPE_SPS) {
      sps.valid = 1;
      ParseSPS(data, len);
    }
  }

  return S_OK;
}

HRESULT CH264SequenceParser::ParseSPS(const BYTE *buffer, int buflen)
{
  CByteParser parser(buffer, buflen);
  
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
  } else {
    sps.chroma = 1;
    sps.luma_bitdepth = 8;
    sps.chroma_bitdepth = 8;
  }

  return S_OK;
}
