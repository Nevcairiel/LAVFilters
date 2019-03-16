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
#include "VC1HeaderParser.h"

#pragma warning( push )
#pragma warning( disable : 4101 )
extern "C" {
#define AVCODEC_X86_MATHOPS_H
#include "libavcodec/get_bits.h"
#include "libavcodec/unary.h"
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

enum FrameCodingMode {
  PROGRESSIVE = 0,    ///<  in the bitstream is reported as 00b
  ILACE_FRAME,        ///<  in the bitstream is reported as 10b
  ILACE_FIELD         ///<  in the bitstream is reported as 11b
};

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

CVC1HeaderParser::CVC1HeaderParser(const BYTE *pData, size_t length, AVCodecID codec)
{
  memset(&hdr, 0, sizeof(hdr));
  ParseVC1Header(pData, length, codec);
}

CVC1HeaderParser::~CVC1HeaderParser(void)
{
}

void CVC1HeaderParser::ParseVC1Header(const BYTE *pData, size_t length, AVCodecID codec)
{
  GetBitContext gb;
  if (codec == AV_CODEC_ID_VC1) {
    if (length < 16)
      return;

    const uint8_t *start = pData;
    const uint8_t *end = start + length;
    const uint8_t *next = nullptr;

    int size, buf2_size;
    uint8_t *buf2;

    buf2 = (uint8_t *)av_mallocz(length + AV_INPUT_BUFFER_PADDING_SIZE);

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
  } else if (codec == AV_CODEC_ID_WMV3) {
    if (length < 4)
      return;
    init_get_bits8(&gb, pData, (int)length);
    VC1ParseSequenceHeader(&gb);
  }
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
        av_reduce(&hdr.ar.num, &hdr.ar.den, (int64_t)hdr.height * w, (int64_t)hdr.width * h, INT_MAX);
      }
    }

    // TODO: add other fields
  } else {
    hdr.valid = 1;
    hdr.old_interlaced = get_bits1(gb); // res_y411
    skip_bits1(gb); // res_sprite

    skip_bits(gb, 3); // frmrtq_postproc
    skip_bits(gb, 5); // bitrtq_postproc
    skip_bits1(gb);   // loop_filter
    skip_bits1(gb);   // res_x8
    skip_bits1(gb);   // multires
    skip_bits1(gb);   // rest_fasttx
    skip_bits1(gb);   // fastuvmc
    skip_bits1(gb);   // extended_mv
    skip_bits(gb, 2); // dquant
    skip_bits1(gb);   // vstransform
    skip_bits1(gb);   // res_transtab
    skip_bits1(gb);   // overlap
    skip_bits1(gb);   // resync marker
    hdr.rangered = get_bits1(gb);
    hdr.bframes = get_bits(gb, 3);
    skip_bits(gb, 2); // quant mode
    hdr.finterp = get_bits1(gb);
  }
}

AVPictureType CVC1HeaderParser::ParseVC1PictureType(const uint8_t *buf, int buflen)
{
  AVPictureType pictype = AV_PICTURE_TYPE_NONE;
  int skipped = 0;
  const BYTE *framestart = buf;
  if (IS_MARKER(AV_RB32(buf))) {
    framestart = nullptr;
    const BYTE *start, *end, *next;
    next = buf;
    for (start = buf, end = buf + buflen; next < end; start = next) {
      if (AV_RB32(start) == VC1_CODE_FRAME) {
        framestart = start + 4;
        break;
      }
      next = find_next_marker(start + 4, end);
    }
  }
  if (framestart) {
    GetBitContext gb;
    init_get_bits8(&gb, framestart, (int)(buflen - (framestart - buf)));
    if (hdr.profile == PROFILE_ADVANCED) {
      int fcm = PROGRESSIVE;
      if (hdr.interlaced)
        fcm = decode012(&gb);
      if (fcm == ILACE_FIELD) {
        int fptype = get_bits(&gb, 3);
        pictype = (fptype & 2) ? AV_PICTURE_TYPE_P : AV_PICTURE_TYPE_I;
        if (fptype & 4) // B-picture
          pictype = (fptype & 2) ? AV_PICTURE_TYPE_BI : AV_PICTURE_TYPE_B;
      } else {
        switch (get_unary(&gb, 0, 4)) {
        case 0:
          pictype = AV_PICTURE_TYPE_P;
          break;
        case 1:
          pictype = AV_PICTURE_TYPE_B;
          break;
        case 2:
          pictype = AV_PICTURE_TYPE_I;
          break;
        case 3:
          pictype = AV_PICTURE_TYPE_BI;
          break;
        case 4:
          pictype = AV_PICTURE_TYPE_P; // skipped pic
          skipped = 1;
          break;
        }
      }
    } else {
      if (hdr.finterp)
        skip_bits1(&gb);
      skip_bits(&gb, 2); // framecnt
      if (hdr.rangered)
        skip_bits1(&gb);
      int pic = get_bits1(&gb);
      if (hdr.bframes) {
        if (!pic) {
          if (get_bits1(&gb)) {
            pictype = AV_PICTURE_TYPE_I;
          } else {
            pictype = AV_PICTURE_TYPE_B;
          }
        } else {
          pictype = AV_PICTURE_TYPE_P;
        }
      } else {
        pictype = pic ? AV_PICTURE_TYPE_P : AV_PICTURE_TYPE_I;
      }
    }
  }
  return pictype;
}
