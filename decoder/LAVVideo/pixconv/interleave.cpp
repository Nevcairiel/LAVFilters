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

#include <emmintrin.h>

#include "pixconv_internal.h"
#include "pixconv_sse2_templates.h"

DECLARE_CONV_FUNC_IMPL(convert_yuv444_y410)
{
  const uint16_t *y = (const uint16_t *)src[0];
  const uint16_t *u = (const uint16_t *)src[1];
  const uint16_t *v = (const uint16_t *)src[2];

  int inStride = srcStride[0] >> 1;
  int outStride = dstStride << 2;
  int shift = 10 - bpp;

  int line, i;

  __m128i xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7;

  xmm7 = _mm_set1_epi32(0xC0000000);
  xmm6 = _mm_setzero_si128();

  _mm_sfence();

  for (line = 0; line < height; ++line) {
    __m128i *dst128 = (__m128i *)(dst + line * outStride);

    for (i = 0; i < width; i+=8) {
      PIXCONV_LOAD_PIXEL16(xmm0, (y+i), shift);
      PIXCONV_LOAD_PIXEL16(xmm1, (u+i), shift);
      PIXCONV_LOAD_PIXEL16(xmm2, (v+i), shift+4); // +4 so its directly aligned properly (data from bit 14 to bit 4)

      xmm3 = _mm_unpacklo_epi16(xmm1, xmm2); // 0VVVVV00000UUUUU
      xmm4 = _mm_unpackhi_epi16(xmm1, xmm2); // 0VVVVV00000UUUUU
      xmm3 = _mm_or_si128(xmm3, xmm7);       // AVVVVV00000UUUUU
      xmm4 = _mm_or_si128(xmm4, xmm7);       // AVVVVV00000UUUUU

      xmm5 = _mm_unpacklo_epi16(xmm0, xmm6); // 00000000000YYYYY
      xmm2 = _mm_unpackhi_epi16(xmm0, xmm6); // 00000000000YYYYY
      xmm5 = _mm_slli_epi32(xmm5, 10);       // 000000YYYYY00000
      xmm2 = _mm_slli_epi32(xmm2, 10);       // 000000YYYYY00000

      xmm3 = _mm_or_si128(xmm3, xmm5);       // AVVVVVYYYYYUUUUU
      xmm4 = _mm_or_si128(xmm4, xmm2);       // AVVVVVYYYYYUUUUU

      // Write data back
      _mm_stream_si128(dst128++, xmm3);
      _mm_stream_si128(dst128++, xmm4);
    }

    y += inStride;
    u += inStride;
    v += inStride;
  }
  return S_OK;
}
