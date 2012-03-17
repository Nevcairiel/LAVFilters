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
#include "VC1HeaderParser.h"

#pragma warning( push )
#pragma warning( disable : 4018 )
#pragma warning( disable : 4244 )
extern "C" {
#define AVCODEC_X86_MATHOPS_H
#include "libavcodec/get_bits.h"
extern __declspec(dllimport) const AVRational ff_vc1_pixel_aspect[16];
};
#pragma warning( pop )

/** Markers used in VC-1 AP frame data */
//@{
enum VC1Code{
  VC1_CODE_RES0       = 0x00000100,
  VC1_CODE_ENDOFSEQ   = 0x0000010A,
  VC1_CODE_SLICE,
  VC1_CODE_FIELD,
  VC1_CODE_FRAME,
  VC1_CODE_ENTRYPOINT,
  VC1_CODE_SEQHDR,
};
//@}

/** Available Profiles */
//@{
enum Profile {
  PROFILE_SIMPLE,
  PROFILE_MAIN,
  PROFILE_COMPLEX, ///< TODO: WMV9 specific
  PROFILE_ADVANCED
};
//@}

#define IS_MARKER(x) (((x) & ~0xFF) == VC1_CODE_RES0)

/** Find VC-1 marker in buffer
* @return position where next marker starts or end of buffer if no marker found
*/
static inline const uint8_t* find_next_marker(const uint8_t *src, const uint8_t *end)
{
  uint32_t mrk = 0xFFFFFFFF;

  if(end-src < 4) return end;
  while(src < end){
    mrk = (mrk << 8) | *src++;
    if(IS_MARKER(mrk))
      return src-4;
  }
  return end;
}

static inline int vc1_unescape_buffer(const uint8_t *src, int size, uint8_t *dst)
{
    int dsize = 0, i;

    if(size < 4){
        for(dsize = 0; dsize < size; dsize++) *dst++ = *src++;
        return size;
    }
    for(i = 0; i < size; i++, src++) {
        if(src[0] == 3 && i >= 2 && !src[-1] && !src[-2] && i < size-1 && src[1] < 4) {
            dst[dsize++] = src[1];
            src++;
            i++;
        } else
            dst[dsize++] = *src;
    }
    return dsize;
}

CVC1HeaderParser::CVC1HeaderParser(const BYTE *pData, size_t length)
{
  memset(&hdr, 0, sizeof(hdr));
  ParseVC1Header(pData, length);
}

CVC1HeaderParser::~CVC1HeaderParser(void)
{
}

void CVC1HeaderParser::ParseVC1Header(const BYTE *pData, size_t length)
{
  if (length < 16)
    return;

  GetBitContext gb;

  const uint8_t *start = pData;
  const uint8_t *end = start + length;
  const uint8_t *next = NULL;

  int size, buf2_size;
  uint8_t *buf2;

  buf2 = (uint8_t *)av_mallocz(length + FF_INPUT_BUFFER_PADDING_SIZE);

  start = find_next_marker(start, end);
  next = start;

  for(; next < end; start = next) {
    next = find_next_marker(start + 4, end);
    size = (int)(next - start - 4);
    if(size <= 0) continue;
    buf2_size = vc1_unescape_buffer(start + 4, size, buf2);

    init_get_bits(&gb, buf2, buf2_size * 8);

    switch(AV_RB32(start)) {
      case VC1_CODE_SEQHDR:
        VC1ParseSequenceHeader(&gb);
        break;
    }
  }
  av_freep(&buf2);
}

void CVC1HeaderParser::VC1ParseSequenceHeader(GetBitContext *gb)
{
  hdr.profile = get_bits(gb, 2);

  if (hdr.profile == PROFILE_ADVANCED) {
    hdr.valid = 1;

    hdr.level = get_bits(gb, 3);
    skip_bits(gb, 2); // Chroma Format, only 1 should be set for 4:2:0
    skip_bits(gb, 3); // frmrtq_postproc
    skip_bits(gb, 5); // bitrtq_postproc
    skip_bits1(gb);   // postprocflag

    hdr.width = (get_bits(gb, 12) + 1) << 1;
    hdr.height = (get_bits(gb, 12) + 1) << 1;

    hdr.broadcast = get_bits1(gb);    // broadcast
    hdr.interlaced = get_bits1(gb);   // interlaced

    skip_bits1(gb); // tfcntrflag
    skip_bits1(gb); // finterpflag
    skip_bits1(gb); // reserved
    skip_bits1(gb); // psf

    if (get_bits1(gb)) { // Display Info
      int w, h, ar = 0;
      w = get_bits(gb, 14) + 1;
      h = get_bits(gb, 14) + 1;
      if (get_bits1(gb))
        ar = get_bits(gb, 4);
      if (ar && ar < 14) {
        hdr.ar = ff_vc1_pixel_aspect[ar];
      } else if (ar == 15) {
        w = get_bits(gb, 8) + 1;
        h = get_bits(gb, 8) + 1;
        hdr.ar.num = w;
        hdr.ar.den = h;
      } else {
        av_reduce(&hdr.ar.num, &hdr.ar.den, hdr.height * w, hdr.width * h, 1 << 30);
      }
    }

    // TODO: add other fields
  }
}