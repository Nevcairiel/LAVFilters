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
#include <ppl.h>

#include "pixconv_internal.h"
#include "pixconv_sse2_templates.h"

#define DITHER_STEPS 2

// This function converts 8x2 pixels from the source into 8x2 YUY2 pixels in the destination
template <LAVPixelFormat inputFormat, int shift, int uyvy, int dithertype> __forceinline
static int yuv420yuy2_convert_pixels(const uint8_t* &srcY, const uint8_t* &srcU, const uint8_t* &srcV, uint8_t* &dst, int srcStrideY, int srcStrideUV, int dstStride, int line, const uint16_t* &dithers, int pos)
{
  __m128i xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7;
  xmm7 = _mm_setzero_si128 ();

  // Shift > 0 is for 9/10 bit formats
  if (shift > 0) {
    // Load 4 U/V values from line 0/1 into registers
    PIXCONV_LOAD_4PIXEL16(xmm1, srcU);
    PIXCONV_LOAD_4PIXEL16(xmm3, srcU+srcStrideUV);
    PIXCONV_LOAD_4PIXEL16(xmm0, srcV);
    PIXCONV_LOAD_4PIXEL16(xmm2, srcV+srcStrideUV);

    // Interleave U and V
    xmm0 = _mm_unpacklo_epi16(xmm1, xmm0);                       /* 0V0U0V0U */
    xmm2 = _mm_unpacklo_epi16(xmm3, xmm2);                       /* 0V0U0V0U */
  } else if (inputFormat == LAVPixFmt_NV12) {
    // Load 4 16-bit macro pixels, which contain 4 UV samples
    PIXCONV_LOAD_4PIXEL16(xmm0, srcU);
    PIXCONV_LOAD_4PIXEL16(xmm2, srcU+srcStrideUV);

    // Expand to 16-bit
    xmm0 = _mm_unpacklo_epi8(xmm0, xmm7);                       /* 0V0U0V0U */
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm7);                       /* 0V0U0V0U */
  } else {
    PIXCONV_LOAD_4PIXEL8(xmm1, srcU);
    PIXCONV_LOAD_4PIXEL8(xmm3, srcU+srcStrideUV);
    PIXCONV_LOAD_4PIXEL8(xmm0, srcV);
    PIXCONV_LOAD_4PIXEL8(xmm2, srcV+srcStrideUV);

    // Interleave U and V
    xmm0 = _mm_unpacklo_epi8(xmm1, xmm0);                       /* VUVU0000 */
    xmm2 = _mm_unpacklo_epi8(xmm3, xmm2);                       /* VUVU0000 */

    // Expand to 16-bit
    xmm0 = _mm_unpacklo_epi8(xmm0, xmm7);                       /* 0V0U0V0U */
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm7);                       /* 0V0U0V0U */
  }

  // xmm0/xmm2 contain 4 interleaved U/V samples from two lines each in the 16bit parts, still in their native bitdepth

  // Chroma upsampling
  if (shift > 0 || inputFormat == LAVPixFmt_NV12) {
    srcU += 8;
    srcV += 8;
  } else {
    srcU += 4;
    srcV += 4;
  }

  xmm1 = xmm0;
  xmm1 = _mm_add_epi16(xmm1, xmm0);                         /* 2x line 0 */
  xmm1 = _mm_add_epi16(xmm1, xmm0);                         /* 3x line 0 */
  xmm1 = _mm_add_epi16(xmm1, xmm2);                         /* 3x line 0 + line 1 (10bit) */

  xmm3 = xmm2;
  xmm3 = _mm_add_epi16(xmm3, xmm2);                         /* 2x line 1 */
  xmm3 = _mm_add_epi16(xmm3, xmm2);                         /* 3x line 1 */
  xmm3 = _mm_add_epi16(xmm3, xmm0);                         /* 3x line 1 + line 0 (10bit) */
  
  // After this step, xmm1 and xmm3 contain 8 16-bit values, V and U interleaved. For 4:2:0, filling input+2 bits (10, 11, 12).
  // Load Y
  if (shift > 0) {
    // Load 8 Y values from line 0/1 into registers
    PIXCONV_LOAD_PIXEL8_ALIGNED(xmm0, srcY);
    PIXCONV_LOAD_PIXEL8_ALIGNED(xmm5, srcY+srcStrideY);

    srcY += 16;
  } else {
    PIXCONV_LOAD_4PIXEL16(xmm0, srcY);
    PIXCONV_LOAD_4PIXEL16(xmm5, srcY+srcStrideY);
    srcY += 8;

    xmm0 = _mm_unpacklo_epi8(xmm0, xmm7);                     /* YYYYYYYY (16-bit fields)*/
    xmm5 = _mm_unpacklo_epi8(xmm5, xmm7);                     /* YYYYYYYY (16-bit fields) */
  }

  // Dither everything to 8-bit

  // Dithering
  if (dithertype == LAVDither_Random) {
    /* Load random dithering coeffs from the dithers buffer */
    int offset = (pos % (DITHER_STEPS * 8 * 2)) * 2;
    xmm6 = _mm_load_si128((const __m128i *)(dithers +  0 + offset));
    xmm7 = _mm_load_si128((const __m128i *)(dithers +  8 + offset));
  } else {
    PIXCONV_LOAD_DITHER_COEFFS(xmm6, line+0, shift+2, odithers);
    PIXCONV_LOAD_DITHER_COEFFS(xmm7, line+1, shift+2, odithers2);
  }

  // Dither UV
  xmm1 = _mm_adds_epu16(xmm1, xmm6);
  xmm3 = _mm_adds_epu16(xmm3, xmm7);
  xmm1 = _mm_srai_epi16(xmm1, shift+2);
  xmm3 = _mm_srai_epi16(xmm3, shift+2);

  if (shift) {                                                /* Y only needs to be dithered if it was > 8 bit */
    xmm6 = _mm_srli_epi16(xmm6, 2);                           /* Shift dithering coeffs to proper strength */
    xmm7 = _mm_srli_epi16(xmm6, 2);

    xmm0 = _mm_adds_epu16(xmm0, xmm6);                        /* Apply dithering coeffs */
    xmm0 = _mm_srai_epi16(xmm0, shift);                       /* Shift to 8 bit */

    xmm5 = _mm_adds_epu16(xmm5, xmm7);                        /* Apply dithering coeffs */
    xmm5 = _mm_srai_epi16(xmm5, shift);                       /* Shift to 8 bit */
  }

  // Pack into 8-bit containers
  xmm0 = _mm_packus_epi16(xmm0, xmm5);
  xmm1 = _mm_packus_epi16(xmm1, xmm3);

  // Interleave U/V with Y
  if (uyvy) {
    xmm3 = xmm1;
    xmm3 = _mm_unpacklo_epi8(xmm3, xmm0);
    xmm4 = _mm_unpackhi_epi8(xmm1, xmm0);
  } else {
    xmm3 = xmm0;
    xmm3 = _mm_unpacklo_epi8(xmm3, xmm1);
    xmm4 = _mm_unpackhi_epi8(xmm0, xmm1);
  }

  // Write back into the target memory
  _mm_stream_si128((__m128i *)(dst), xmm3);
  _mm_stream_si128((__m128i *)(dst + dstStride), xmm4);

  dst += 16;

  return 0;
}

template <LAVPixelFormat inputFormat, int shift, int uyvy, int dithertype>
static int __stdcall yuv420yuy2_process_lines(const uint8_t *srcY, const uint8_t *srcU, const uint8_t *srcV, uint8_t *dst, int width, int height, int srcStrideY, int srcStrideUV, int dstStride, const uint16_t *dithers)
{
  const uint8_t *y = srcY;
  const uint8_t *u = srcU;
  const uint8_t *v = srcV;
  uint8_t *yuy2 = dst;

  dstStride *= 2;

  // Processing starts at line 1, and ends at height - 1. The first and last line have special handling
  int line = 1;
  const int lastLine = height - 1;

  const uint16_t *lineDither = dithers;

  _mm_sfence();

  // Process first line
  // This needs special handling because of the chroma offset of YUV420
  for (int i = 0; i < width; i += 8) {
    yuv420yuy2_convert_pixels<inputFormat, shift, uyvy, dithertype>(y, u, v, yuy2, 0, 0, 0, 0, lineDither, i);
  }

  for (; line < lastLine; line += 2) {
    if (dithertype == LAVDither_Random)
      lineDither = dithers + (line * 16 * DITHER_STEPS);

    y = srcY + line * srcStrideY;

    u = srcU + (line >> 1) * srcStrideUV;
    v = srcV + (line >> 1) * srcStrideUV;

    yuy2 = dst + line * dstStride;

    for (int i = 0; i < width; i += 8) {
      yuv420yuy2_convert_pixels<inputFormat, shift, uyvy, dithertype>(y, u, v, yuy2, srcStrideY, srcStrideUV, dstStride, line, lineDither, i);
    }
  }

  // Process last line
  // This needs special handling because of the chroma offset of YUV420
  if (dithertype == LAVDither_Random)
    lineDither = dithers + ((height - 2) * 16 * DITHER_STEPS);

  y = srcY + (height - 1) * srcStrideY;
  u = srcU + ((height >> 1) - 1)  * srcStrideUV;
  v = srcV + ((height >> 1) - 1)  * srcStrideUV;
  yuy2 = dst + (height - 1) * dstStride;

  for (int i = 0; i < width; i += 8) {
    yuv420yuy2_convert_pixels<inputFormat, shift, uyvy, dithertype>(y, u, v, yuy2, 0, 0, 0, line, lineDither, i);
  }
  return 0;
}

template<int uyvy, int dithertype>
static int __stdcall yuv420yuy2_dispatch(LAVPixelFormat inputFormat, int bpp, const uint8_t *srcY, const uint8_t *srcU, const uint8_t *srcV, uint8_t *dst, int width, int height, int srcStrideY, int srcStrideUV, int dstStride, const uint16_t *dithers)
{
    // Wrap the input format into template args
  switch (inputFormat) {
  case LAVPixFmt_YUV420:
    return yuv420yuy2_process_lines<LAVPixFmt_YUV420, 0, uyvy, dithertype>(srcY, srcU, srcV, dst, width, height, srcStrideY, srcStrideUV, dstStride, dithers);
  case LAVPixFmt_NV12:
    return yuv420yuy2_process_lines<LAVPixFmt_NV12, 0, uyvy, dithertype>(srcY, srcU, srcV, dst, width, height, srcStrideY, srcStrideUV, dstStride, dithers);
  case LAVPixFmt_YUV420bX:
    if (bpp == 10)
      return yuv420yuy2_process_lines<LAVPixFmt_YUV420, 2, uyvy, dithertype>(srcY, srcU, srcV, dst, width, height, srcStrideY, srcStrideUV, dstStride, dithers);
    else if (bpp == 9)
      return yuv420yuy2_process_lines<LAVPixFmt_YUV420, 1, uyvy, dithertype>(srcY, srcU, srcV, dst, width, height, srcStrideY, srcStrideUV, dstStride, dithers);
    else
      ASSERT(0);
    break;
  default:
    ASSERT(0);
  }
  return 0;
}

template<int uyvy>
DECLARE_CONV_FUNC_IMPL(convert_yuv420_yuy2)
{
  LAVDitherMode ditherMode = m_pSettings->GetDitherMode();
  const uint16_t *dithers = (ditherMode == LAVDither_Random) ? GetRandomDitherCoeffs(height, DITHER_STEPS * 2, bpp - 8 + 2, 0) : NULL;
  if (ditherMode == LAVDither_Random && dithers != NULL) {
    yuv420yuy2_dispatch<uyvy, 1>(inputFormat, bpp, src[0], src[1], src[2], dst, width, height, srcStride[0], srcStride[1], dstStride, dithers);
  } else {
    yuv420yuy2_dispatch<uyvy, 0>(inputFormat, bpp, src[0], src[1], src[2], dst, width, height, srcStride[0], srcStride[1], dstStride, NULL);
  }

  return S_OK;
}

// Force creation of these two variants
template HRESULT CLAVPixFmtConverter::convert_yuv420_yuy2<0>CONV_FUNC_PARAMS;
template HRESULT CLAVPixFmtConverter::convert_yuv420_yuy2<1>CONV_FUNC_PARAMS;
