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
#include <ppl.h>

#include "pixconv_internal.h"
#include "pixconv_sse2_templates.h"

#pragma warning(push)
#pragma warning(disable: 4556)

#define DITHER_STEPS 3

// This function converts 4x2 pixels from the source into 4x2 RGB pixels in the destination
template <LAVPixelFormat inputFormat, int shift, int outFmt, int right_edge, int dithertype, int ycgco> __forceinline
static int yuv2rgb_convert_pixels(const uint8_t* &srcY, const uint8_t* &srcU, const uint8_t* &srcV, uint8_t* &dst, ptrdiff_t srcStrideY, ptrdiff_t srcStrideUV, ptrdiff_t dstStride, ptrdiff_t line, const RGBCoeffs *coeffs, const uint16_t* &dithers, ptrdiff_t pos)
{
  __m128i xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7;
  xmm7 = _mm_setzero_si128 ();

  // Shift > 0 is for 9/10 bit formats
  if (inputFormat == LAVPixFmt_P016) {
    // Load 2 32-bit macro pixels from each line, which contain 4 UV at 16-bit each samples
    PIXCONV_LOAD_PIXEL8(xmm0, srcU);
    PIXCONV_LOAD_PIXEL8(xmm2, srcU+srcStrideUV);
  } else if (shift > 0) {
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

  // Chroma upsampling required
  if (inputFormat == LAVPixFmt_YUV420 || inputFormat == LAVPixFmt_NV12 || inputFormat == LAVPixFmt_YUV422 || inputFormat == LAVPixFmt_P016) {
    if (inputFormat == LAVPixFmt_P016) {
      srcU += 8;
      srcV += 8;
    } else if (shift > 0 || inputFormat == LAVPixFmt_NV12) {
      srcU += 4;
      srcV += 4;
    } else {
      srcU += 2;
      srcV += 2;
    }

    // Cut off the over-read into the stride and replace it with the last valid pixel
    if (right_edge) {
      xmm6 = _mm_set_epi32(0, 0xffffffff, 0, 0);

      // First line
      xmm1 = xmm0;
      xmm1 = _mm_slli_si128(xmm1, 4);
      xmm1 = _mm_and_si128(xmm1, xmm6);
      xmm0 = _mm_andnot_si128(xmm6, xmm0);
      xmm0 = _mm_or_si128(xmm0, xmm1);

      // Second line
      xmm3 = xmm2;
      xmm3 = _mm_slli_si128(xmm3, 4);
      xmm3 = _mm_and_si128(xmm3, xmm6);
      xmm2 = _mm_andnot_si128(xmm6, xmm2);
      xmm2 = _mm_or_si128(xmm2, xmm3);
    }

    // 4:2:0 - upsample to 4:2:2 using 75:25
    if (inputFormat == LAVPixFmt_YUV420 || inputFormat == LAVPixFmt_NV12 || inputFormat == LAVPixFmt_P016) {
      // Too high bitdepth, shift down to 14-bit
      if (shift >= 7) {
        xmm0 = _mm_srli_epi16(xmm0, shift-6);
        xmm2 = _mm_srli_epi16(xmm2, shift-6);
      }
      xmm1 = xmm0;
      xmm1 = _mm_add_epi16(xmm1, xmm0);                         /* 2x line 0 */
      xmm1 = _mm_add_epi16(xmm1, xmm0);                         /* 3x line 0 */
      xmm1 = _mm_add_epi16(xmm1, xmm2);                         /* 3x line 0 + line 1 (10bit) */

      xmm3 = xmm2;
      xmm3 = _mm_add_epi16(xmm3, xmm2);                         /* 2x line 1 */
      xmm3 = _mm_add_epi16(xmm3, xmm2);                         /* 3x line 1 */
      xmm3 = _mm_add_epi16(xmm3, xmm0);                         /* 3x line 1 + line 0 (10bit) */

      // If the bit depth is too high, we need to reduce it here (max 15bit)
      // 14-16 bits need the reduction, because they all result in a 16-bit result
      if (shift >= 6) {
        xmm1 = _mm_srli_epi16(xmm1, 1);
        xmm3 = _mm_srli_epi16(xmm3, 1);
      }
    } else {
      xmm1 = xmm0;
      xmm3 = xmm2;

      // Shift to maximum of 15-bit, if required
      if (shift >= 8) {
        xmm1 = _mm_srli_epi16(xmm1, 1);
        xmm3 = _mm_srli_epi16(xmm3, 1);
      }
    }
    // After this step, xmm1 and xmm3 contain 8 16-bit values, V and U interleaved. For 4:2:2, filling 8 to 15 bits (original bit depth). For 4:2:0, filling input+2 bits (10 to 15).

    // Upsample to 4:4:4 using 100:0, 50:50, 0:100 scheme (MPEG2 chroma siting)
    // TODO: MPEG1 chroma siting, use 75:25

    xmm0 = xmm1;                                               /* UV UV UV UV */
    xmm0 = _mm_unpacklo_epi32(xmm0, xmm7);                     /* UV 00 UV 00 */
    xmm1 = _mm_srli_si128(xmm1, 4);                            /* UV UV UV 00 */
    xmm1 = _mm_unpacklo_epi32(xmm7, xmm1);                     /* 00 UV 00 UV */

    xmm1 = _mm_add_epi16(xmm1, xmm0);                         /*  UV  UV  UV  UV */
    xmm1 = _mm_add_epi16(xmm1, xmm0);                         /* 2UV  UV 2UV  UV */

    xmm0 = _mm_slli_si128(xmm0, 4);                            /*  00  UV  00  UV */
    xmm1 = _mm_add_epi16(xmm1, xmm0);                         /* 2UV 2UV 2UV 2UV */

    // Same for the second row
    xmm2 = xmm3;                                               /* UV UV UV UV */
    xmm2 = _mm_unpacklo_epi32(xmm2, xmm7);                     /* UV 00 UV 00 */
    xmm3 = _mm_srli_si128(xmm3, 4);                            /* UV UV UV 00 */
    xmm3 = _mm_unpacklo_epi32(xmm7, xmm3);                     /* 00 UV 00 UV */

    xmm3 = _mm_add_epi16(xmm3, xmm2);                         /*  UV  UV  UV  UV */
    xmm3 = _mm_add_epi16(xmm3, xmm2);                         /* 2UV  UV 2UV  UV */

    xmm2 = _mm_slli_si128(xmm2, 4);                            /*  00  UV  00  UV */
    xmm3 = _mm_add_epi16(xmm3, xmm2);                         /* 2UV 2UV 2UV 2UV */

    // Shift the result to 12 bit
    // For 10-bit input, we need to shift one bit off, or we exceed the allowed processing depth
    // For 8-bit, we need to add one bit
    if ((inputFormat == LAVPixFmt_YUV420 && shift > 1) || inputFormat == LAVPixFmt_P016) {
      if (shift >= 5) {
        xmm1 = _mm_srli_epi16(xmm1, 4);
        xmm3 = _mm_srli_epi16(xmm3, 4);
      } else {
        xmm1 = _mm_srli_epi16(xmm1, shift-1);
        xmm3 = _mm_srli_epi16(xmm3, shift-1);
      }
    } else if (inputFormat == LAVPixFmt_YUV422) {
      if (shift >= 7) {
        xmm1 = _mm_srli_epi16(xmm1, 4);
        xmm3 = _mm_srli_epi16(xmm3, 4);
      } else if (shift > 3) {
        xmm1 = _mm_srli_epi16(xmm1, shift-3);
        xmm3 = _mm_srli_epi16(xmm3, shift-3);
      } else if (shift < 3) {
        xmm1 = _mm_slli_epi16(xmm1, 3-shift);
        xmm3 = _mm_slli_epi16(xmm3, 3-shift);
      }
    } else if ((inputFormat == LAVPixFmt_YUV420 && shift == 0) || inputFormat == LAVPixFmt_NV12) {
      xmm1 = _mm_slli_epi16(xmm1, 1);
      xmm3 = _mm_slli_epi16(xmm3, 1);
    }

    // 12-bit result, xmm1 & xmm3 with 4 UV combinations each
  } else if (inputFormat == LAVPixFmt_YUV444) {
    if (shift > 0) {
      srcU += 8;
      srcV += 8;
    } else {
      srcU += 4;
      srcV += 4;
    }
    // Shift to 12 bit
    if (shift > 4) {
      xmm1 = _mm_srli_epi16(xmm0, shift-4);
      xmm3 = _mm_srli_epi16(xmm2, shift-4);
    } else if (shift < 4) {
      xmm1 = _mm_slli_epi16(xmm0, 4-shift);
      xmm3 = _mm_slli_epi16(xmm2, 4-shift);
    } else {
      xmm1 = xmm0;
      xmm3 = xmm2;
    }
  }

  // Load Y
  if (shift > 0) {
    // Load 4 Y values from line 0/1 into registers
    PIXCONV_LOAD_4PIXEL16(xmm5, srcY);
    PIXCONV_LOAD_4PIXEL16(xmm0, srcY+srcStrideY);

    srcY += 8;
  } else {
    PIXCONV_LOAD_4PIXEL8(xmm5, srcY);
    PIXCONV_LOAD_4PIXEL8(xmm0, srcY+srcStrideY);
    srcY += 4;

    xmm5 = _mm_unpacklo_epi8(xmm5, xmm7);                       /* YYYY0000 (16-bit fields) */
    xmm0 = _mm_unpacklo_epi8(xmm0, xmm7);                       /* YYYY0000 (16-bit fields)*/
  }

  xmm0 = _mm_unpacklo_epi64(xmm0, xmm5);                        /* YYYYYYYY */

  // After this step, xmm1 & xmm3 contain 4 UV pairs, each in a 16-bit value, filling 12-bit.
  if (!ycgco) {
    // YCbCr conversion
    // Shift Y to 14 bits
    if (shift < 6) {
      xmm0 = _mm_slli_epi16(xmm0, 6-shift);
    } else if (shift > 6) {
      xmm0 = _mm_srli_epi16(xmm0, shift-6);
    }
    xmm0 = _mm_subs_epu16(xmm0, coeffs->Ysub);                  /* Y-16 (in case of range expansion) */
    xmm0 = _mm_mulhi_epi16(xmm0, coeffs->cy);                   /* Y*cy (result is 28 bits, with 12 high-bits packed into the result) */
    xmm0 = _mm_add_epi16(xmm0, coeffs->rgb_add);                /* Y*cy + 16 (in case of range compression) */

    xmm1 = _mm_subs_epi16(xmm1, coeffs->CbCr_center);           /* move CbCr to proper range */
    xmm3 = _mm_subs_epi16(xmm3, coeffs->CbCr_center);

    xmm6 = xmm1;
    xmm4 = xmm3;
    xmm6 = _mm_madd_epi16(xmm6, coeffs->cR_Cr);                 /* Result is 25 bits (12 from chroma, 13 from coeff) */
    xmm4 = _mm_madd_epi16(xmm4, coeffs->cR_Cr);
    xmm6 = _mm_srai_epi32(xmm6, 13);                            /* Reduce to 12 bit */
    xmm4 = _mm_srai_epi32(xmm4, 13);
    xmm6 = _mm_packs_epi32(xmm6, xmm7);                         /* Pack back into 16 bit cells */
    xmm4 = _mm_packs_epi32(xmm4, xmm7);
    xmm6 = _mm_unpacklo_epi64(xmm4, xmm6);                      /* Interleave both parts */
    xmm6 = _mm_add_epi16(xmm6, xmm0);                           /* R (12bit) */

    xmm5 = xmm1;
    xmm4 = xmm3;
    xmm5 = _mm_madd_epi16(xmm5, coeffs->cG_Cb_cG_Cr);           /* Result is 25 bits (12 from chroma, 13 from coeff) */
    xmm4 = _mm_madd_epi16(xmm4, coeffs->cG_Cb_cG_Cr);
    xmm5 = _mm_srai_epi32(xmm5, 13);                            /* Reduce to 12 bit */
    xmm4 = _mm_srai_epi32(xmm4, 13);
    xmm5 = _mm_packs_epi32(xmm5, xmm7);                         /* Pack back into 16 bit cells */
    xmm4 = _mm_packs_epi32(xmm4, xmm7);
    xmm5 = _mm_unpacklo_epi64(xmm4, xmm5);                      /* Interleave both parts */
    xmm5 = _mm_add_epi16(xmm5, xmm0);                           /* G (12bit) */

    xmm1 = _mm_madd_epi16(xmm1, coeffs->cB_Cb);                 /* Result is 25 bits (12 from chroma, 13 from coeff) */
    xmm3 = _mm_madd_epi16(xmm3, coeffs->cB_Cb);
    xmm1 = _mm_srai_epi32(xmm1, 13);                            /* Reduce to 12 bit */
    xmm3 = _mm_srai_epi32(xmm3, 13);
    xmm1 = _mm_packs_epi32(xmm1, xmm7);                         /* Pack back into 16 bit cells */
    xmm3 = _mm_packs_epi32(xmm3, xmm7);
    xmm1 = _mm_unpacklo_epi64(xmm3, xmm1);                      /* Interleave both parts */
    xmm1 = _mm_add_epi16(xmm1, xmm0);                           /* B (12bit) */
  } else {
    // YCgCo conversion
    // Shift Y to 12 bits
    if (shift < 4) {
      xmm0 = _mm_slli_epi16(xmm0, 4-shift);
    } else if (shift > 4) {
      xmm0 = _mm_srli_epi16(xmm0, shift-4);
    }

    xmm7 = _mm_set1_epi32(0x0000FFFF);
    xmm2 = xmm1;
    xmm4 = xmm3;

    xmm1 = _mm_and_si128(xmm1, xmm7);                          /* null out the high-order bytes to get the Cg values */
    xmm4 = _mm_and_si128(xmm4, xmm7);

    xmm3 = _mm_srli_epi32(xmm3, 16);                           /* right shift the Co values */
    xmm2 = _mm_srli_epi32(xmm2, 16);

    xmm1 = _mm_packs_epi32(xmm4, xmm1);                       /* Pack Cg into xmm1 */
    xmm3 = _mm_packs_epi32(xmm3, xmm2);                       /* Pack Co into xmm3 */

    xmm2 = coeffs->CbCr_center;                               /* move CgCo to proper range */
    xmm1 = _mm_subs_epi16(xmm1, xmm2);
    xmm3 = _mm_subs_epi16(xmm3, xmm2);

    xmm2 = xmm0;
    xmm2 = _mm_subs_epi16(xmm2, xmm1);                         /* tmp = Y - Cg */
    xmm6 = _mm_adds_epi16(xmm2, xmm3);                         /* R = tmp + Co */
    xmm5 = _mm_adds_epi16(xmm0, xmm1);                         /* G = Y + Cg */
    xmm1 = _mm_subs_epi16(xmm2, xmm3);                         /* B = tmp - Co */
  }

  // Dithering
  if (dithertype == LAVDither_Random) {
    /* Load random dithering coeffs from the dithers buffer */
    int offset = (pos % (DITHER_STEPS * 4 * 2)) * 6;
    xmm2 = _mm_load_si128((const __m128i *)(dithers +  0 + offset));
    xmm3 = _mm_load_si128((const __m128i *)(dithers +  8 + offset));
    xmm4 = _mm_load_si128((const __m128i *)(dithers + 16 + offset));
  } else {
    /* Load dithering coeffs and combine them for two lines */
    const uint16_t *d1 = dither_8x8_256[line % 8];
    xmm2 = _mm_load_si128((const __m128i *)d1);
    const uint16_t *d2 = dither_8x8_256[(line+1) % 8];
    xmm3 = _mm_load_si128((const __m128i *)d2);

    xmm4 = xmm2;
    xmm2 = _mm_unpacklo_epi64(xmm2, xmm3);
    xmm4 = _mm_unpackhi_epi64(xmm4, xmm3);
    xmm2 = _mm_srli_epi16(xmm2, 4);
    xmm4 = _mm_srli_epi16(xmm4, 4);

    xmm3 = xmm4;
  }

  xmm6 = _mm_adds_epu16(xmm6, xmm2);                          /* Apply coefficients to the RGB values */
  xmm5 = _mm_adds_epu16(xmm5, xmm3);
  xmm1 = _mm_adds_epu16(xmm1, xmm4);

  xmm6 = _mm_srai_epi16(xmm6, 4);                             /* Shift to 8 bit */
  xmm5 = _mm_srai_epi16(xmm5, 4);
  xmm1 = _mm_srai_epi16(xmm1, 4);

  xmm2 = _mm_cmpeq_epi8(xmm2, xmm2);                          /* 0xffffffff,0xffffffff,0xffffffff,0xffffffff */
  xmm6 = _mm_packus_epi16(xmm6, xmm7);                        /* R (lower 8bytes,8bit) * 8 */
  xmm5 = _mm_packus_epi16(xmm5, xmm7);                        /* G (lower 8bytes,8bit) * 8 */
  xmm1 = _mm_packus_epi16(xmm1, xmm7);                        /* B (lower 8bytes,8bit) * 8 */

  xmm6 = _mm_unpacklo_epi8(xmm6,xmm2); // 0xff,R
  xmm1 = _mm_unpacklo_epi8(xmm1,xmm5); // G,B
  xmm2 = xmm1;

  xmm1 = _mm_unpackhi_epi16(xmm1, xmm6); // 0xff,RGB * 4 (line 0)
  xmm2 = _mm_unpacklo_epi16(xmm2, xmm6); // 0xff,RGB * 4 (line 1)

  // TODO: RGB limiting

  if (outFmt == 1) {
    _mm_stream_si128((__m128i *)(dst), xmm1);
    _mm_stream_si128((__m128i *)(dst + dstStride), xmm2);
    dst += 16;
  } else {
    // RGB 24 output is terribly inefficient due to the un-aligned size of 3 bytes per pixel
    uint32_t eax;
    DECLARE_ALIGNED(16, uint8_t, rgbbuf)[32];
    *(uint32_t *)rgbbuf = _mm_cvtsi128_si32(xmm1);
    xmm1 = _mm_srli_si128(xmm1, 4);
    *(uint32_t *)(rgbbuf+3) = _mm_cvtsi128_si32 (xmm1);
    xmm1 = _mm_srli_si128(xmm1, 4);
    *(uint32_t *)(rgbbuf+6) = _mm_cvtsi128_si32 (xmm1);
    xmm1 = _mm_srli_si128(xmm1, 4);
    *(uint32_t *)(rgbbuf+9) = _mm_cvtsi128_si32 (xmm1);

    *(uint32_t *)(rgbbuf+16) = _mm_cvtsi128_si32 (xmm2);
    xmm2 = _mm_srli_si128(xmm2, 4);
    *(uint32_t *)(rgbbuf+19) = _mm_cvtsi128_si32 (xmm2);
    xmm2 = _mm_srli_si128(xmm2, 4);
    *(uint32_t *)(rgbbuf+22) = _mm_cvtsi128_si32 (xmm2);
    xmm2 = _mm_srli_si128(xmm2, 4);
    *(uint32_t *)(rgbbuf+25) = _mm_cvtsi128_si32 (xmm2);

    xmm1 = _mm_loadl_epi64((const __m128i *)(rgbbuf));
    xmm2 = _mm_loadl_epi64((const __m128i *)(rgbbuf+16));

    _mm_storel_epi64((__m128i *)(dst), xmm1);
    eax = *(uint32_t *)(rgbbuf + 8);
    *(uint32_t *)(dst + 8) = eax;

    _mm_storel_epi64((__m128i *)(dst + dstStride), xmm2);
    eax = *(uint32_t *)(rgbbuf + 24);
    *(uint32_t *)(dst + dstStride + 8) = eax;

    dst += 12;
  }

  return 0;
}

template <LAVPixelFormat inputFormat, int shift, int outFmt, int dithertype, int ycgco>
static int __stdcall yuv2rgb_convert(const uint8_t *srcY, const uint8_t *srcU, const uint8_t *srcV, uint8_t *dst, int width, int height, ptrdiff_t srcStrideY, ptrdiff_t srcStrideUV, ptrdiff_t dstStride, ptrdiff_t sliceYStart, ptrdiff_t sliceYEnd, const RGBCoeffs *coeffs, const uint16_t *dithers)
{
  const uint8_t *y = srcY;
  const uint8_t *u = srcU;
  const uint8_t *v = srcV;
  uint8_t *rgb = dst;

  ptrdiff_t line = sliceYStart;
  ptrdiff_t lastLine = sliceYEnd;
  bool lastLineInOddHeight = false;

  const ptrdiff_t endx = width - 4;

  const uint16_t *lineDither = dithers;

  _mm_sfence();

  // 4:2:0 needs special handling for the first and the last line
  if (inputFormat == LAVPixFmt_YUV420 || inputFormat == LAVPixFmt_NV12 || inputFormat == LAVPixFmt_P016) {
    if (line == 0) {
      for (ptrdiff_t i = 0; i < endx; i += 4) {
        yuv2rgb_convert_pixels<inputFormat, shift, outFmt, 0, dithertype, ycgco>(y, u, v, rgb, 0, 0, 0, line, coeffs, lineDither, i);
      }
      yuv2rgb_convert_pixels<inputFormat, shift, outFmt, 1, dithertype, ycgco>(y, u, v, rgb, 0, 0, 0, line, coeffs, lineDither, 0);

      line = 1;
    }
    if (lastLine == height)
      lastLine--;
  } else if (lastLine == height && (lastLine & 1)) {
    lastLine--;
    lastLineInOddHeight = true;
  }

  for (; line < lastLine; line += 2) {
    if (dithertype == LAVDither_Random)
      lineDither = dithers + (line * 24 * DITHER_STEPS);
    y = srcY + line * srcStrideY;

    if (inputFormat == LAVPixFmt_YUV420 || inputFormat == LAVPixFmt_NV12 || inputFormat == LAVPixFmt_P016) {
      u = srcU + (line >> 1) * srcStrideUV;
      v = srcV + (line >> 1) * srcStrideUV;
    } else {
      u = srcU + line * srcStrideUV;
      v = srcV + line * srcStrideUV;
    }

    rgb = dst + line * dstStride;

    for (ptrdiff_t i = 0; i < endx; i += 4) {
      yuv2rgb_convert_pixels<inputFormat, shift, outFmt, 0, dithertype, ycgco>(y, u, v, rgb, srcStrideY, srcStrideUV, dstStride, line, coeffs, lineDither, i);
    }
    yuv2rgb_convert_pixels<inputFormat, shift, outFmt, 1, dithertype, ycgco>(y, u, v, rgb, srcStrideY, srcStrideUV, dstStride, line, coeffs, lineDither, 0);
  }

  if (inputFormat == LAVPixFmt_YUV420 || inputFormat == LAVPixFmt_NV12 || inputFormat == LAVPixFmt_P016 || lastLineInOddHeight) {
    if (sliceYEnd == height) {
      if (dithertype == LAVDither_Random)
        lineDither = dithers + ((height - 2) * 24 * DITHER_STEPS);
      y = srcY + (height - 1) * srcStrideY;
      if (inputFormat == LAVPixFmt_YUV420 || inputFormat == LAVPixFmt_NV12 || inputFormat == LAVPixFmt_P016) {
        u = srcU + ((height >> 1) - 1)  * srcStrideUV;
        v = srcV + ((height >> 1) - 1)  * srcStrideUV;
      } else {
        u = srcU + (height - 1)  * srcStrideUV;
        v = srcV + (height - 1)  * srcStrideUV;
      }
      rgb = dst + (height - 1) * dstStride;

      for (ptrdiff_t i = 0; i < endx; i += 4) {
        yuv2rgb_convert_pixels<inputFormat, shift, outFmt, 0, dithertype, ycgco>(y, u, v, rgb, 0, 0, 0, line, coeffs, lineDither, i);
      }
      yuv2rgb_convert_pixels<inputFormat, shift, outFmt, 1, dithertype, ycgco>(y, u, v, rgb, 0, 0, 0, line, coeffs, lineDither, 0);
    }
  }
  return 0;
}

DECLARE_CONV_FUNC_IMPL(convert_yuv_rgb)
{
  const RGBCoeffs *coeffs = getRGBCoeffs(width, height);
  if (coeffs == nullptr)
    return E_OUTOFMEMORY;

  if (!m_bRGBConvInit) {
    m_bRGBConvInit = TRUE;
    InitRGBConvDispatcher();
  }

  BOOL bYCgCo = (m_ColorProps.VideoTransferMatrix == 7);
  int shift = max(bpp - 8, 0);
  ASSERT(shift >= 0 && shift <= 8);

  int outFmt = -1;
  switch (outputFormat) {
  case LAVOutPixFmt_RGB24:
    outFmt = 0;
    break;
  case LAVOutPixFmt_RGB32:
    outFmt = 1;
    break;
  default:
    ASSERT(0);
  }

  LAVDitherMode ditherMode = m_pSettings->GetDitherMode();
  const uint16_t *dithers = (ditherMode == LAVDither_Random) ? GetRandomDitherCoeffs(height, DITHER_STEPS * 3, 4, 0) : nullptr;
  if (ditherMode == LAVDither_Random && dithers == nullptr) {
    ditherMode = LAVDither_Ordered;
  }

  // Map the bX formats to their normal counter part, the shift parameter controls this now
  if (inputFormat == LAVPixFmt_YUV420bX || inputFormat == LAVPixFmt_YUV422bX || inputFormat == LAVPixFmt_YUV444bX)
    inputFormat = (LAVPixelFormat)(inputFormat - 1);

  // P010 has the data in the high bits, so set shift appropriately
  if (inputFormat == LAVPixFmt_P016)
    shift = 8;

  YUVRGBConversionFunc convFn = m_RGBConvFuncs[outFmt][ditherMode][bYCgCo][inputFormat][shift];
  if (convFn == nullptr) {
    ASSERT(0);
    return E_FAIL;
  }

  // run conversion, threaded
  if (m_NumThreads <= 1) {
    convFn(src[0], src[1], src[2], dst[0], width, height, srcStride[0], srcStride[1], dstStride[0], 0, height, coeffs, dithers);
  } else {
    const int is_odd = (inputFormat == LAVPixFmt_YUV420 || inputFormat == LAVPixFmt_NV12 || inputFormat == LAVPixFmt_P016);
    const ptrdiff_t lines_per_thread = (height / m_NumThreads)&~1;

    Concurrency::parallel_for(0, m_NumThreads, [&](int i) {
      const ptrdiff_t starty = (i * lines_per_thread);
      const ptrdiff_t endy = (i == (m_NumThreads - 1)) ? height : starty + lines_per_thread + is_odd;
      convFn(src[0], src[1], src[2], dst[0], width, height, srcStride[0], srcStride[1], dstStride[0], starty + (i ? is_odd : 0), endy, coeffs, dithers);
    });
  }

  return S_OK;
}

#define CONV_FUNC_INT2(out32, dither, ycgco, format, shift) \
  m_RGBConvFuncs[out32][dither][ycgco][format][shift] = yuv2rgb_convert<format, shift, out32, dither, ycgco>;

#define CONV_FUNC_INT(dither, ycgco, format, shift)  \
  CONV_FUNC_INT2(0, dither, ycgco, format, shift)    \
  CONV_FUNC_INT2(1, dither, ycgco, format, shift)    \

#define CONV_FUNC(format, shift)                     \
  CONV_FUNC_INT(LAVDither_Ordered, 0, format, shift) \
  CONV_FUNC_INT(LAVDither_Random,  0, format, shift) \
  CONV_FUNC_INT(LAVDither_Ordered, 1, format, shift) \
  CONV_FUNC_INT(LAVDither_Random,  1, format, shift) \

#define CONV_FUNCX(format)   \
  CONV_FUNC(format, 0)       \
  CONV_FUNC(format, 1)       \
  CONV_FUNC(format, 2)       \
  /* CONV_FUNC(format, 3) */ \
  CONV_FUNC(format, 4)       \
  /* CONV_FUNC(format, 5) */ \
  CONV_FUNC(format, 6)       \
  /* CONV_FUNC(format, 7) */ \
  CONV_FUNC(format, 8)

void CLAVPixFmtConverter::InitRGBConvDispatcher()
{
  ZeroMemory(&m_RGBConvFuncs, sizeof(m_RGBConvFuncs));

  CONV_FUNC(LAVPixFmt_NV12,   0);
  CONV_FUNC(LAVPixFmt_P016,   8);

  CONV_FUNCX(LAVPixFmt_YUV420);
  CONV_FUNCX(LAVPixFmt_YUV422);
  CONV_FUNCX(LAVPixFmt_YUV444);
}

const RGBCoeffs* CLAVPixFmtConverter::getRGBCoeffs(int width, int height)
{
  if (!m_rgbCoeffs || width != swsWidth || height != swsHeight) {
    swsWidth = width;
    swsHeight = height;

    if (!m_rgbCoeffs) {
      m_rgbCoeffs = (RGBCoeffs *)_aligned_malloc(sizeof(RGBCoeffs), 16);
      if (m_rgbCoeffs == nullptr)
        return nullptr;
    }

    DXVA2_VideoTransferMatrix matrix = (DXVA2_VideoTransferMatrix)m_ColorProps.VideoTransferMatrix;
    if (matrix == DXVA2_VideoTransferMatrix_Unknown) {
      matrix = (swsHeight > 576 || swsWidth > 1024) ? DXVA2_VideoTransferMatrix_BT709 : DXVA2_VideoTransferMatrix_BT601;
    }

    BOOL inFullRange = (m_ColorProps.NominalRange == DXVA2_NominalRange_0_255);
    BOOL outFullRange = (swsOutputRange == 0) ? inFullRange : (swsOutputRange == 2);

    int inputWhite, inputBlack, inputChroma, outputWhite, outputBlack;
    if (inFullRange) {
      inputWhite = 255;
      inputBlack = 0;
      inputChroma = 1;
    } else {
      inputWhite = 235;
      inputBlack = 16;
      inputChroma = 16;
    }

    if (outFullRange) {
      outputWhite = 255;
      outputBlack = 0;
    } else {
      outputWhite = 235;
      outputBlack = 16;
    }

    double Kr, Kg, Kb;
    switch (matrix) {
    case DXVA2_VideoTransferMatrix_BT601:
      Kr = 0.299;
      Kg = 0.587;
      Kb = 0.114;
      break;
    case DXVA2_VideoTransferMatrix_SMPTE240M:
      Kr = 0.2120;
      Kg = 0.7010;
      Kb = 0.0870;
      break;
    case 6: // FCC
      Kr = 0.300;
      Kg = 0.590;
      Kb = 0.110;
      break;
    case 4: // BT.2020
      Kr = 0.2627;
      Kg = 0.6780;
      Kb = 0.0593;
      break;
    default:
      DbgLog((LOG_TRACE, 10, L"::getRGBCoeffs(): Unknown color space: %d - defaulting to BT709", matrix));
    case DXVA2_VideoTransferMatrix_BT709:
      Kr = 0.2126;
      Kg = 0.7152;
      Kb = 0.0722;
      break;
    }

    double in_y_range = inputWhite - inputBlack;
    double chr_range = 128 - inputChroma;

    double cspOptionsRGBrange = outputWhite - outputBlack;

    double y_mul, vr_mul, ug_mul, vg_mul, ub_mul;
    y_mul  = cspOptionsRGBrange / in_y_range;
    vr_mul = (cspOptionsRGBrange / chr_range) * (1.0 - Kr);
    ug_mul = (cspOptionsRGBrange / chr_range) * (1.0 - Kb) * Kb / Kg;
    vg_mul = (cspOptionsRGBrange / chr_range) * (1.0 - Kr) * Kr / Kg;
    ub_mul = (cspOptionsRGBrange / chr_range) * (1.0 - Kb);
    short sub = min(outputBlack, inputBlack);
    short Ysub = inputBlack - sub;
    short RGB_add1 = outputBlack - sub;

    short cy  = short(y_mul * 16384 + 0.5);
    short crv = short(vr_mul * 8192 + 0.5);
    short cgu = short(-ug_mul * 8192 - 0.5);
    short cgv = short(-vg_mul * 8192 - 0.5);
    short cbu = short(ub_mul * 8192 + 0.5);

    m_rgbCoeffs->Ysub        = _mm_set1_epi16(Ysub << 6);
    m_rgbCoeffs->cy          = _mm_set1_epi16(cy);
    m_rgbCoeffs->CbCr_center = _mm_set1_epi16(128 << 4);

    m_rgbCoeffs->cR_Cr       = _mm_set1_epi32(crv << 16);         // R
    m_rgbCoeffs->cG_Cb_cG_Cr = _mm_set1_epi32((cgv << 16) + cgu); // G
    m_rgbCoeffs->cB_Cb       = _mm_set1_epi32(cbu);               // B

    m_rgbCoeffs->rgb_add     = _mm_set1_epi16(RGB_add1 << 4);

    // YCgCo
    if (matrix == 7) {
      m_rgbCoeffs->CbCr_center = _mm_set1_epi16(0x0800);
      // Other Coeffs are not used in YCgCo
    }

  }
  return m_rgbCoeffs;
}

#pragma warning(pop)
