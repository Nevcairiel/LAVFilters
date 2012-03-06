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
#include "MPEG2HeaderParser.h"

#pragma warning( push )
#pragma warning( disable : 4018 )
#pragma warning( disable : 4244 )
#define AVCODEC_X86_MATHOPS_H
#include "libavcodec/get_bits.h"
#pragma warning( pop )

#define SEQ_START_CODE          0x000001b3
#define EXT_START_CODE          0x000001b5

static inline const uint8_t* find_next_marker(const uint8_t *src, const uint8_t *end)
{
  uint32_t mrk = 0xFFFFFFFF;

  if(end-src < 4) return end;
  while(src < end){
    mrk = (mrk << 8) | *src++;
    if((mrk & ~0xFF) == 0x00000100)
      return src-4;
  }
  return end;
}

CMPEG2HeaderParser::CMPEG2HeaderParser(const BYTE *pData, size_t length)
{
  memset(&hdr, 0, sizeof(hdr));
  ParseMPEG2Header(pData, length);
}

CMPEG2HeaderParser::~CMPEG2HeaderParser(void)
{
}

void CMPEG2HeaderParser::ParseMPEG2Header(const BYTE *pData, size_t length)
{
  if (length < 16)
    return;

  GetBitContext gb;

  const uint8_t *start = pData;
  const uint8_t *end = start + length;
  const uint8_t *next = NULL;

  int size;

  start = find_next_marker(start, end);
  next = start;

  for(; next < end; start = next) {
    next = find_next_marker(start + 4, end);
    size = (int)(next - start - 4);
    if(size <= 0) continue;

    init_get_bits(&gb, start + 4, (size - 4) * 8);

    switch(AV_RB32(start)) {
      case SEQ_START_CODE:
        MPEG2ParseSequenceHeader(&gb);
        break;
      case EXT_START_CODE:
        MPEG2ParseExtHeader(&gb);
        break;
    }
  }
}

void CMPEG2HeaderParser::MPEG2ParseSequenceHeader(GetBitContext *gb)
{
}

void CMPEG2HeaderParser::MPEG2ParseExtHeader(GetBitContext *gb)
{
  int startcode = get_bits(gb, 4); // Start Code
  if (startcode == 1) {
    hdr.valid = 1;

    skip_bits(gb, 1); // profile and level esc
    hdr.profile = get_bits(gb, 3);
    hdr.level = get_bits(gb, 4);

    hdr.interlaced = !get_bits1(gb);
    hdr.chroma = get_bits(gb, 2);

    // TODO: Fill in other fields, if needed
  }
}
