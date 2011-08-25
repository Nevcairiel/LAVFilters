/*
 *      Copyright (C) 2011 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 */

#include "stdafx.h"

#include <emmintrin.h>

#include "pixconv_internal.h"
#include "pixconv_sse2_templates.h"

template <int nv12>
DECLARE_CONV_FUNC_IMPL(convert_yuv420_yv12_nv12_dither_le)
{
  const uint16_t *y = (const uint16_t *)src[0];
  const uint16_t *u = (const uint16_t *)src[1];
  const uint16_t *v = (const uint16_t *)src[2];

  int inYStride = srcStride[0] >> 1;
  int inUVStride = srcStride[1] >> 1;
  int outYStride = dstStride;
  int outUVStride = dstStride >> 1;
  int shift = (inputFormat == PIX_FMT_YUV420P10LE ? 2 : (inputFormat == PIX_FMT_YUV420P9LE) ? 1 : 8);

  int line, i;

  __m128i xmm0,xmm1,xmm2,xmm3,xmm4;

  uint8_t *dstY = dst;
  uint8_t *dstV = dst + outYStride * height;
  uint8_t *dstU = dstV + outUVStride * (height >> 1);

  // Process Y
  for (line = 0; line < height; ++line) {
    // Load dithering coefficients for this line
    PIXCONV_LOAD_DITHER_COEFFS(xmm4,line,shift,dithers);

    __m128i *dst128Y = (__m128i *)(dst + line * outYStride);

    for (i = 0; i < width; i+=16) {
      // Load pixels into registers, and apply dithering
      PIXCONV_LOAD_PIXEL16_DITHER(xmm0, xmm4, (y+i), shift);   /* Y0Y0Y0Y0 */
      PIXCONV_LOAD_PIXEL16_DITHER(xmm1, xmm4, (y+i+8), shift); /* Y0Y0Y0Y0 */
      xmm0 = _mm_packus_epi16(xmm0, xmm1);                     /* YYYYYYYY */

      // Write data back
      _mm_stream_si128(dst128Y++, xmm0);
    }

    y += inYStride;
  }

  // Process U & V

  for (line = 0; line < (height >> 1); ++line) {
    // Load dithering coefficients for this line
    PIXCONV_LOAD_DITHER_COEFFS(xmm4,line,shift,dithers);

    __m128i *dst128UV = (__m128i *)(dstV + line * outYStride);
    __m128i *dst128U = (__m128i *)(dstU + line * outUVStride);
    __m128i *dst128V = (__m128i *)(dstV + line * outUVStride);

    for (i = 0; i < (width >> 1); i+=16) {
      PIXCONV_LOAD_PIXEL16_DITHER(xmm0, xmm4, (u+i), shift);    /* U0U0U0U0 */
      PIXCONV_LOAD_PIXEL16_DITHER(xmm1, xmm4, (u+i+8), shift);  /* U0U0U0U0 */
      PIXCONV_LOAD_PIXEL16_DITHER(xmm2, xmm4, (v+i), shift);    /* V0V0V0V0 */
      PIXCONV_LOAD_PIXEL16_DITHER(xmm3, xmm4, (v+i+8), shift);  /* V0V0V0V0 */

      if (nv12) {
        xmm2 = _mm_slli_epi16(xmm2, 8);                         /* 0V0V0V0V */
        xmm3 = _mm_slli_epi16(xmm3, 8);                         /* 0V0V0V0V */
        xmm0 = _mm_or_si128(xmm0, xmm2);                        /* UVUVUVUV */
        xmm1 = _mm_or_si128(xmm1, xmm3);                        /* UVUVUVUV */
        _mm_stream_si128(dst128UV++, xmm0);
        _mm_stream_si128(dst128UV++, xmm1);
      } else {
        xmm0 = _mm_packus_epi16(xmm0, xmm1);                    /* UUUUUUUU */
        _mm_stream_si128(dst128U++, xmm0);

        xmm2 = _mm_packus_epi16(xmm2, xmm3);                    /* VVVVVVVV */
        _mm_stream_si128(dst128V++, xmm2);
      }
    }

    u += inUVStride;
    v += inUVStride;
  }

  return S_OK;
}

// Force creation of these two variants
template HRESULT CLAVPixFmtConverter::convert_yuv420_yv12_nv12_dither_le<0>CONV_FUNC_PARAMS;
template HRESULT CLAVPixFmtConverter::convert_yuv420_yv12_nv12_dither_le<1>CONV_FUNC_PARAMS;

DECLARE_CONV_FUNC_IMPL(convert_yuv420_px1x_le)
{
  const uint16_t *y = (const uint16_t *)src[0];
  const uint16_t *u = (const uint16_t *)src[1];
  const uint16_t *v = (const uint16_t *)src[2];

  int inYStride = srcStride[0] >> 1;
  int inUVStride = srcStride[1] >> 1;
  int outStride = dstStride << 1;
  int shift = ((inputFormat == PIX_FMT_YUV420P10LE || inputFormat == PIX_FMT_YUV422P10LE) ? 6 : (inputFormat == PIX_FMT_YUV420P9LE) ? 7 : 0);
  int uvHeight = (outputFormat == LAVPixFmt_P010 || outputFormat == LAVPixFmt_P016) ? (height >> 1) : height;

  int line, i;
  __m128i xmm0,xmm1,xmm2,xmm3;

  // Process Y
  for (line = 0; line < height; ++line) {
    __m128i *dst128Y = (__m128i *)(dst + line * outStride);

    for (i = 0; i < width; i+=8) {
      // Load 8 pixels into register
      PIXCONV_LOAD_PIXEL16(xmm0, (y+i), shift); /* YYYY */
      // and write them out
      _mm_stream_si128(dst128Y++, xmm0);
    }

    y += inYStride;
  }

  BYTE *dstUV = dst + (height * outStride);

  // Process UV
  for (line = 0; line < uvHeight; ++line) {
    __m128i *dst128UV = (__m128i *)(dstUV + line * outStride);

    for (i = 0; i < (width >> 1); i+=8) {
      // Load 8 pixels into register
      PIXCONV_LOAD_PIXEL16(xmm0, (v+i), shift); /* VVVV */
      PIXCONV_LOAD_PIXEL16(xmm1, (u+i), shift); /* UUUU */

      xmm2 = _mm_unpacklo_epi16(xmm1, xmm0);    /* UVUV */
      xmm3 = _mm_unpackhi_epi16(xmm1, xmm0);    /* UVUV */

      _mm_stream_si128(dst128UV++, xmm2);
      _mm_stream_si128(dst128UV++, xmm3);
    }

    u += inUVStride;
    v += inUVStride;
  }

  return S_OK;
}
