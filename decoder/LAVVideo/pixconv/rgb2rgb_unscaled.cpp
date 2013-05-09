/*
 *      Copyright (C) 2010-2013 Hendrik Leppkes
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

DECLARE_CONV_FUNC_IMPL(convert_rgb48_rgb32_ssse3)
{
  const uint16_t *rgb = (const uint16_t *)src[0];
  const ptrdiff_t inStride = srcStride[0] >> 1;
  const ptrdiff_t outStride = dstStride * 4;
  ptrdiff_t line, i;

  int processWidth = width * 3;

  LAVDitherMode ditherMode = m_pSettings->GetDitherMode();
  const uint16_t *dithers = GetRandomDitherCoeffs(height, 4, 8, 0);
  if (dithers == NULL)
    ditherMode = LAVDither_Ordered;

  __m128i xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7;
  __m128i mask = _mm_setr_epi8(0,1,2,3,4,5,-1,-1,6,7,8,9,10,11,-1,-1);

  _mm_sfence();
  for (line = 0; line < height; line++) {
    __m128i *dst128 = (__m128i *)(dst + line * outStride);

    // Load dithering coefficients for this line
    if (ditherMode == LAVDither_Random) {
      xmm5 = _mm_load_si128((const __m128i *)(dithers + (line << 5) + 0));
      xmm6 = _mm_load_si128((const __m128i *)(dithers + (line << 5) + 8));
      xmm7 = _mm_load_si128((const __m128i *)(dithers + (line << 5) + 16));
    } else {
      PIXCONV_LOAD_DITHER_COEFFS(xmm7,line,8,dithers);
      xmm5 = xmm6 = xmm7;
    }
    for (i = 0; i < processWidth; i += 24) {
      PIXCONV_LOAD_ALIGNED(xmm0, (rgb + i));      /* load */
      _mm_adds_epu16(xmm0, xmm5);                 /* apply dithering coefficients */
      xmm0 = _mm_srli_epi16(xmm0, 8);             /* shift to 8-bit */
      PIXCONV_LOAD_ALIGNED(xmm1, (rgb + i + 8));  /* load */
      _mm_adds_epu16(xmm1, xmm6);                 /* apply dithering coefficients */
      xmm1 = _mm_srli_epi16(xmm1, 8);             /* shift to 8-bit */
      PIXCONV_LOAD_ALIGNED(xmm2, (rgb + i + 16)); /* load */
      _mm_adds_epu16(xmm2, xmm7);                 /* apply dithering coefficients */
      xmm2 = _mm_srli_epi16(xmm2, 8);             /* shift to 8-bit */

      xmm3 = _mm_shuffle_epi8(xmm0, mask);
      xmm4 = _mm_shuffle_epi8(_mm_alignr_epi8(xmm1, xmm0, 12), mask);
      xmm0 = _mm_shuffle_epi8(_mm_alignr_epi8(xmm2, xmm1, 8),  mask);
      xmm1 = _mm_shuffle_epi8(_mm_alignr_epi8(xmm2, xmm2, 4),  mask);
      
      xmm3 = _mm_packus_epi16(xmm3, xmm4);
      xmm0 = _mm_packus_epi16(xmm0, xmm1);

      _mm_stream_si128(dst128++, xmm3);
      _mm_stream_si128(dst128++, xmm0);
    }

    rgb += inStride;
  }

  return S_OK;
}

DECLARE_CONV_FUNC_IMPL(convert_rgb48_rgb24_ssse3)
{
  const uint16_t *rgb = (const uint16_t *)src[0];
  const ptrdiff_t inStride = srcStride[0] >> 1;
  const ptrdiff_t outStride = dstStride * 3;
  ptrdiff_t line, i;

  int processWidth = width * 3;

  LAVDitherMode ditherMode = m_pSettings->GetDitherMode();
  const uint16_t *dithers = GetRandomDitherCoeffs(height, 2, 8, 0);
  if (dithers == NULL)
    ditherMode = LAVDither_Ordered;

  __m128i xmm0,xmm1,xmm6,xmm7;

  _mm_sfence();
  for (line = 0; line < height; line++) {
    __m128i *dst128 = (__m128i *)(dst + line * outStride);

    // Load dithering coefficients for this line
    if (ditherMode == LAVDither_Random) {
      xmm6 = _mm_load_si128((const __m128i *)(dithers + (line << 4) + 0));
      xmm7 = _mm_load_si128((const __m128i *)(dithers + (line << 4) + 8));
    } else {
      PIXCONV_LOAD_DITHER_COEFFS(xmm7,line,8,dithers);
      xmm6 = xmm7;
    }
    for (i = 0; i < processWidth; i += 16) {
      PIXCONV_LOAD_ALIGNED(xmm0, (rgb + i));      /* load */
      _mm_adds_epu16(xmm0, xmm6);                 /* apply dithering coefficients */
      xmm0 = _mm_srli_epi16(xmm0, 8);             /* shift to 8-bit */
      PIXCONV_LOAD_ALIGNED(xmm1, (rgb + i + 8));  /* load */
      _mm_adds_epu16(xmm1, xmm7);                 /* apply dithering coefficients */
      xmm1 = _mm_srli_epi16(xmm1, 8);             /* shift to 8-bit */
      
      xmm0 = _mm_packus_epi16(xmm0, xmm1);
      _mm_stream_si128(dst128++, xmm0);
    }

    rgb += inStride;
  }

  return S_OK;
}
