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

#include <emmintrin.h>

#include "pixconv_internal.h"
#include "pixconv_sse2_templates.h"

DECLARE_CONV_FUNC_IMPL(convert_rgb48_rgb32_ssse3)
{
  const uint16_t *rgb = (const uint16_t *)src[0];
  const ptrdiff_t inStride = srcStride[0] >> 1;
  const ptrdiff_t outStride = dstStride[0];
  ptrdiff_t line, i;

  int processWidth = width * 3;

  LAVDitherMode ditherMode = m_pSettings->GetDitherMode();
  const uint16_t *dithers = GetRandomDitherCoeffs(height, 4, 8, 0);
  if (dithers == nullptr)
    ditherMode = LAVDither_Ordered;

  __m128i xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7;
  __m128i mask = _mm_setr_epi8(4,5,2,3,0,1,-1,-1,10,11,8,9,6,7,-1,-1);

  _mm_sfence();
  for (line = 0; line < height; line++) {
    __m128i *dst128 = (__m128i *)(dst[0] + line * outStride);

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
      PIXCONV_LOAD_ALIGNED(xmm1, (rgb + i + 8));
      PIXCONV_LOAD_ALIGNED(xmm2, (rgb + i + 16));
      xmm0 = _mm_adds_epu16(xmm0, xmm5);          /* apply dithering coefficients */
      xmm1 = _mm_adds_epu16(xmm1, xmm6);
      xmm2 = _mm_adds_epu16(xmm2, xmm7);
      xmm0 = _mm_srli_epi16(xmm0, 8);             /* shift to 8-bit */
      xmm1 = _mm_srli_epi16(xmm1, 8);
      xmm2 = _mm_srli_epi16(xmm2, 8);

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

template <int out32>
DECLARE_CONV_FUNC_IMPL(convert_rgb48_rgb)
{
  // Byte Swap to BGR layout
  uint8_t *dstBS[4]    = {nullptr};
  dstBS[0] = (BYTE *)av_malloc(height * srcStride[0]);
  if (dstBS[0] == nullptr)
    return E_OUTOFMEMORY;

  SwsContext *ctx = GetSWSContext(width, height, GetFFInput(), AV_PIX_FMT_BGR48LE, SWS_POINT);
  sws_scale2(ctx, src, srcStride, 0, height, dstBS, srcStride);

  // Dither to RGB24/32 with SSE2
  const uint16_t *rgb = (const uint16_t *)dstBS[0];
  const ptrdiff_t inStride = srcStride[0] >> 1;
  const ptrdiff_t outStride = dstStride[0];
  ptrdiff_t line, i;
  int processWidth = width * 3;

  LAVDitherMode ditherMode = m_pSettings->GetDitherMode();
  const uint16_t *dithers = GetRandomDitherCoeffs(height, 2, 8, 0);
  if (dithers == nullptr)
    ditherMode = LAVDither_Ordered;

  __m128i xmm0,xmm1,xmm6,xmm7;

  uint8_t *rgb24buffer = nullptr;
  if (out32) {
    rgb24buffer = (uint8_t *)av_malloc(outStride + AV_INPUT_BUFFER_PADDING_SIZE);
    if (rgb24buffer == nullptr) {
      av_freep(&dstBS[0]);
      return E_OUTOFMEMORY;
    }
  }

  _mm_sfence();
  for (line = 0; line < height; line++) {
    __m128i *dst128 = nullptr;
    if (out32) {
      dst128 = (__m128i *)rgb24buffer;
    } else {
      dst128 = (__m128i *)(dst[0] + line * outStride);
    }

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
      PIXCONV_LOAD_ALIGNED(xmm1, (rgb + i + 8));
      xmm0 = _mm_adds_epu16(xmm0, xmm6);          /* apply dithering coefficients */
      xmm1 = _mm_adds_epu16(xmm1, xmm7);
      xmm0 = _mm_srli_epi16(xmm0, 8);             /* shift to 8-bit */
      xmm1 = _mm_srli_epi16(xmm1, 8);

      xmm0 = _mm_packus_epi16(xmm0, xmm1);
      _mm_stream_si128(dst128++, xmm0);
    }

    rgb += inStride;
    if (out32) {
      uint32_t *src24 = (uint32_t *)rgb24buffer;
      uint32_t *dst32 = (uint32_t *)(dst[0] + line * outStride);
      for (i = 0; i < width; i += 4) {
        uint32_t sa = src24[0];
        uint32_t sb = src24[1];
        uint32_t sc = src24[2];

        dst32[i+0] = sa;
        dst32[i+1] = (sa>>24) | (sb<<8);
        dst32[i+2] = (sb>>16) | (sc<<16);
        dst32[i+3] = sc>>8;

        src24 += 3;
      }
    }
  }

  if (out32)
    av_freep(&rgb24buffer);
  av_freep(&dstBS[0]);

  return S_OK;
}

template HRESULT CLAVPixFmtConverter::convert_rgb48_rgb<0>CONV_FUNC_PARAMS;
template HRESULT CLAVPixFmtConverter::convert_rgb48_rgb<1>CONV_FUNC_PARAMS;
