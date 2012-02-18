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

#define PIXCONV_INTERLEAVE_AYUV(regY, regU, regV, regA, regOut1, regOut2) \
  regY    = _mm_unpacklo_epi8(regY, regA);     /* YAYAYAYA */             \
  regV    = _mm_unpacklo_epi8(regV, regU);     /* VUVUVUVU */             \
  regOut1 = _mm_unpacklo_epi16(regV, regY);    /* VUYAVUYA */             \
  regOut2 = _mm_unpackhi_epi16(regV, regY);    /* VUYAVUYA */

#define YUV444_PACK_AYUV(dst) *idst++ = v[i] | (u[i] << 8) | (y[i] << 16) | (0xff << 24);

DECLARE_CONV_FUNC_IMPL(convert_yuv444_ayuv)
{
  const uint8_t *y = (const uint8_t *)src[0];
  const uint8_t *u = (const uint8_t *)src[1];
  const uint8_t *v = (const uint8_t *)src[2];

  const int inStride = srcStride[0];
  const int outStride = dstStride << 2;

  int line, i;

  __m128i xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6;

  xmm6 = _mm_set1_epi32(-1);

  _mm_sfence();

  for (line = 0; line < height; ++line) {
    __m128i *dst128 = (__m128i *)(dst + line * outStride);

    for (i = 0; i < width; i+=16) {
      // Load pixels into registers
      PIXCONV_LOAD_PIXEL8_ALIGNED(xmm0, (y+i)); /* YYYYYYYY */
      PIXCONV_LOAD_PIXEL8_ALIGNED(xmm1, (u+i)); /* UUUUUUUU */
      PIXCONV_LOAD_PIXEL8_ALIGNED(xmm2, (v+i)); /* VVVVVVVV */

      // Interlave into AYUV
      xmm4 = xmm0;
      xmm0 = _mm_unpacklo_epi8(xmm0, xmm6);     /* YAYAYAYA */
      xmm4 = _mm_unpackhi_epi8(xmm4, xmm6);     /* YAYAYAYA */

      xmm5 = xmm2;
      xmm2 = _mm_unpacklo_epi8(xmm2, xmm1);     /* VUVUVUVU */
      xmm5 = _mm_unpackhi_epi8(xmm5, xmm1);     /* VUVUVUVU */

      xmm1 = _mm_unpacklo_epi16(xmm2, xmm0);    /* VUYAVUYA */
      xmm2 = _mm_unpackhi_epi16(xmm2, xmm0);    /* VUYAVUYA */

      xmm0 = _mm_unpacklo_epi16(xmm5, xmm4);    /* VUYAVUYA */
      xmm3 = _mm_unpackhi_epi16(xmm5, xmm4);    /* VUYAVUYA */

      // Write data back
      _mm_stream_si128(dst128++, xmm1);
      _mm_stream_si128(dst128++, xmm2);
      _mm_stream_si128(dst128++, xmm0);
      _mm_stream_si128(dst128++, xmm3);
    }

    y += inStride;
    u += inStride;
    v += inStride;
  }

  return S_OK;
}

DECLARE_CONV_FUNC_IMPL(convert_yuv444_ayuv_dither_le)
{
  const uint16_t *y = (const uint16_t *)src[0];
  const uint16_t *u = (const uint16_t *)src[1];
  const uint16_t *v = (const uint16_t *)src[2];

  const int inStride = srcStride[0] >> 1;
  const int outStride = dstStride << 2;

  LAVDitherMode ditherMode = m_pSettings->GetDitherMode();
  const uint16_t *dithers = GetRandomDitherCoeffs(height, 3, 8, 0);
  if (dithers == NULL)
    ditherMode = LAVDither_Ordered;

  // Number of bits to shift to reach 8
  int shift = bpp - 8;

  int line, i;

  __m128i xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7;

  xmm7 = _mm_set1_epi16(-256); /* 0xFF00 - 0A0A0A0A */

  _mm_sfence();

  for (line = 0; line < height; ++line) {
    // Load dithering coefficients for this line
    if (ditherMode == LAVDither_Random) {
      xmm4 = _mm_load_si128((const __m128i *)(dithers + (line * 24) +  0));
      xmm5 = _mm_load_si128((const __m128i *)(dithers + (line * 24) +  8));
      xmm6 = _mm_load_si128((const __m128i *)(dithers + (line * 24) + 16));
    } else {
      PIXCONV_LOAD_DITHER_COEFFS(xmm6,line,8,dithers);
      xmm4 = xmm5 = xmm6;
    }

    __m128i *dst128 = (__m128i *)(dst + line * outStride);

    for (i = 0; i < width; i+=8) {
      // Load pixels into registers, and apply dithering
      PIXCONV_LOAD_PIXEL16_DITHER(xmm0, xmm4, (y+i), shift); /* Y0Y0Y0Y0 */
      PIXCONV_LOAD_PIXEL16_DITHER_HIGH(xmm1, xmm5, (u+i), shift); /* U0U0U0U0 */
      PIXCONV_LOAD_PIXEL16_DITHER(xmm2, xmm6, (v+i), shift); /* V0V0V0V0 */

      // Interlave into AYUV
      xmm0 = _mm_or_si128(xmm0, xmm7);          /* YAYAYAYA */
      xmm1 = _mm_and_si128(xmm1, xmm7);         /* clear out clobbered low-bytes */
      xmm2 = _mm_or_si128(xmm2, xmm1);          /* VUVUVUVU */

      xmm3 = xmm2;
      xmm2 = _mm_unpacklo_epi16(xmm2, xmm0);    /* VUYAVUYA */
      xmm3 = _mm_unpackhi_epi16(xmm3, xmm0);    /* VUYAVUYA */

      // Write data back
      _mm_stream_si128(dst128++, xmm2);
      _mm_stream_si128(dst128++, xmm3);
    }

    y += inStride;
    u += inStride;
    v += inStride;
  }

  return S_OK;
}
