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

template <int nv12>
DECLARE_CONV_FUNC_IMPL(convert_yuv_yv_nv12_dither_le)
{
  const ptrdiff_t inYStride   = srcStride[0];
  const ptrdiff_t inUVStride  = srcStride[1];

  const ptrdiff_t outYStride  = dstStride[0];
  const ptrdiff_t outUVStride = dstStride[1];

  ptrdiff_t chromaWidth       = width;
  ptrdiff_t chromaHeight      = height;

  LAVDitherMode ditherMode = m_pSettings->GetDitherMode();
  const uint16_t *dithers = GetRandomDitherCoeffs(height, 4, 8, 0);
  if (dithers == nullptr)
    ditherMode = LAVDither_Ordered;

  if (inputFormat == LAVPixFmt_YUV420bX)
    chromaHeight = chromaHeight >> 1;
  if (inputFormat == LAVPixFmt_YUV420bX || inputFormat == LAVPixFmt_YUV422bX)
    chromaWidth = (chromaWidth + 1) >> 1;

  ptrdiff_t line, i;

  __m128i xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7;

  _mm_sfence();

  // Process Y
  for (line = 0; line < height; ++line) {
    // Load dithering coefficients for this line
    if (ditherMode == LAVDither_Random) {
      xmm4 = _mm_load_si128((const __m128i *)(dithers + (line << 5) + 0));
      xmm5 = _mm_load_si128((const __m128i *)(dithers + (line << 5) + 8));
      xmm6 = _mm_load_si128((const __m128i *)(dithers + (line << 5) + 16));
      xmm7 = _mm_load_si128((const __m128i *)(dithers + (line << 5) + 24));
    } else {
      PIXCONV_LOAD_DITHER_COEFFS(xmm7,line,8,dithers);
      xmm4 = xmm5 = xmm6 = xmm7;
    }

    const uint16_t * const y  = (const uint16_t *)(src[0] + line * inYStride);
          uint16_t * const dy = (      uint16_t *)(dst[0] + line * outYStride);

    for (i = 0; i < width; i+=32) {
      // Load pixels into registers, and apply dithering
      PIXCONV_LOAD_PIXEL16_DITHER(xmm0, xmm4, (y+i+ 0), bpp);  /* Y0Y0Y0Y0 */
      PIXCONV_LOAD_PIXEL16_DITHER(xmm1, xmm5, (y+i+ 8), bpp);  /* Y0Y0Y0Y0 */
      PIXCONV_LOAD_PIXEL16_DITHER(xmm2, xmm6, (y+i+16), bpp);  /* Y0Y0Y0Y0 */
      PIXCONV_LOAD_PIXEL16_DITHER(xmm3, xmm7, (y+i+24), bpp);  /* Y0Y0Y0Y0 */
      xmm0 = _mm_packus_epi16(xmm0, xmm1);                     /* YYYYYYYY */
      xmm2 = _mm_packus_epi16(xmm2, xmm3);                     /* YYYYYYYY */

      // Write data back
      PIXCONV_PUT_STREAM(dy + (i >> 1) + 0, xmm0);
      PIXCONV_PUT_STREAM(dy + (i >> 1) + 8, xmm2);
    }

    // Process U/V for chromaHeight lines
    if (line < chromaHeight) {
      const uint16_t * const u = (const uint16_t *)(src[1] + line * inUVStride);
      const uint16_t * const v = (const uint16_t *)(src[2] + line * inUVStride);

      uint8_t * const duv = (uint8_t *)(dst[1] + line * outUVStride);
      uint8_t * const du  = (uint8_t *)(dst[2] + line * outUVStride);
      uint8_t * const dv  = (uint8_t *)(dst[1] + line * outUVStride);

       for (i = 0; i < chromaWidth; i+=16) {
        PIXCONV_LOAD_PIXEL16_DITHER(xmm0, xmm4, (u+i+0), bpp);  /* U0U0U0U0 */
        PIXCONV_LOAD_PIXEL16_DITHER(xmm1, xmm5, (u+i+8), bpp);  /* U0U0U0U0 */
        PIXCONV_LOAD_PIXEL16_DITHER(xmm2, xmm6, (v+i+0), bpp);  /* V0V0V0V0 */
        PIXCONV_LOAD_PIXEL16_DITHER(xmm3, xmm7, (v+i+8), bpp);  /* V0V0V0V0 */

        xmm0 = _mm_packus_epi16(xmm0, xmm1);                    /* UUUUUUUU */
        xmm2 = _mm_packus_epi16(xmm2, xmm3);                    /* VVVVVVVV */
        if (nv12) {
          xmm1 = xmm0;
          xmm0 = _mm_unpacklo_epi8(xmm0, xmm2);
          xmm1 = _mm_unpackhi_epi8(xmm1, xmm2);

          PIXCONV_PUT_STREAM(duv + (i << 1) + 0, xmm0);
          PIXCONV_PUT_STREAM(duv + (i << 1) + 16, xmm1);
        } else {
          PIXCONV_PUT_STREAM(du + i, xmm0);
          PIXCONV_PUT_STREAM(dv + i, xmm2);
        }
      }
    }
  }

  return S_OK;
}

// Force creation of these two variants
template HRESULT CLAVPixFmtConverter::convert_yuv_yv_nv12_dither_le<0>CONV_FUNC_PARAMS;
template HRESULT CLAVPixFmtConverter::convert_yuv_yv_nv12_dither_le<1>CONV_FUNC_PARAMS;

DECLARE_CONV_FUNC_IMPL(convert_yuv420_px1x_le)
{
  const ptrdiff_t inYStride   = srcStride[0];
  const ptrdiff_t inUVStride  = srcStride[1];
  const ptrdiff_t outYStride  = dstStride[0];
  const ptrdiff_t outUVStride = dstStride[1];
  const ptrdiff_t uvHeight    = (outputFormat == LAVOutPixFmt_P010 || outputFormat == LAVOutPixFmt_P016) ? (height >> 1) : height;
  const ptrdiff_t uvWidth     = (width + 1) >> 1;

  ptrdiff_t line, i;
  __m128i xmm0,xmm1,xmm2;

  _mm_sfence();

  // Process Y
  for (line = 0; line < height; ++line) {
    const uint16_t * const y = (const uint16_t *)(src[0] + line * inYStride);
          uint16_t * const d = (      uint16_t *)(dst[0] + line * outYStride);

    for (i = 0; i < width; i+=16) {
      // Load 2x8 pixels into registers
      PIXCONV_LOAD_PIXEL16X2(xmm0, xmm1, (y+i+0), (y+i+8), bpp);
      // and write them out
      PIXCONV_PUT_STREAM(d+i+0, xmm0);
      PIXCONV_PUT_STREAM(d+i+8, xmm1);
    }
  }

  // Process UV
  for (line = 0; line < uvHeight; ++line) {
    const uint16_t * const u = (const uint16_t *)(src[1] + line * inUVStride);
    const uint16_t * const v = (const uint16_t *)(src[2] + line * inUVStride);
          uint16_t * const d = (      uint16_t *)(dst[1] + line * outUVStride);

    for (i = 0; i < uvWidth; i+=8) {
      // Load 8 pixels into register
      PIXCONV_LOAD_PIXEL16X2(xmm0, xmm1, (v+i), (u+i), bpp); // Load V and U

      xmm2 = xmm0;
      xmm0 = _mm_unpacklo_epi16(xmm1, xmm0);    /* UVUV */
      xmm2 = _mm_unpackhi_epi16(xmm1, xmm2);    /* UVUV */

      PIXCONV_PUT_STREAM(d + (i << 1) + 0, xmm0);
      PIXCONV_PUT_STREAM(d + (i << 1) + 8, xmm2);
    }
  }

  return S_OK;
}

DECLARE_CONV_FUNC_IMPL(convert_yuv_yv)
{
  const uint8_t *y = src[0];
  const uint8_t *u = src[1];
  const uint8_t *v = src[2];

  const ptrdiff_t inLumaStride    = srcStride[0];
  const ptrdiff_t inChromaStride  = srcStride[1];

  const ptrdiff_t outLumaStride   = dstStride[0];
  const ptrdiff_t outChromaStride = dstStride[1];

  ptrdiff_t line;
  ptrdiff_t chromaWidth       = width;
  ptrdiff_t chromaHeight      = height;

  if (inputFormat == LAVPixFmt_YUV420)
    chromaHeight = chromaHeight >> 1;
  if (inputFormat == LAVPixFmt_YUV420 || inputFormat == LAVPixFmt_YUV422)
    chromaWidth = (chromaWidth + 1) >> 1;

  // Copy planes

  _mm_sfence();

  // Y
  if ((outLumaStride % 16) == 0 && ((intptr_t)dst[0] % 16u) == 0) {
    for (line = 0; line < height; ++line) {
      PIXCONV_MEMCPY_ALIGNED(dst[0] + outLumaStride * line, y + inLumaStride * line, width);
    }
  } else {
    for (line = 0; line < height; ++line) {
      memcpy(dst[0] + outLumaStride * line, y + inLumaStride * line, width);
    }
  }

  // U/V
  if ((outChromaStride % 16) == 0 && ((intptr_t)dst[1] % 16u) == 0) {
    for(line = 0; line < chromaHeight; ++line) {
      PIXCONV_MEMCPY_ALIGNED_TWO(dst[2] + outChromaStride * line, u + inChromaStride * line,
                                 dst[1] + outChromaStride * line, v + inChromaStride * line,
                                 chromaWidth);
    }
  } else {
    for (line = 0; line < chromaHeight; ++line) {
      memcpy(dst[2] + outChromaStride * line, u + inChromaStride * line, chromaWidth);
      memcpy(dst[1] + outChromaStride * line, v + inChromaStride * line, chromaWidth);
    }
  }

  return S_OK;
}

DECLARE_CONV_FUNC_IMPL(convert_yuv420_nv12)
{
  const ptrdiff_t inLumaStride    = srcStride[0];
  const ptrdiff_t inChromaStride  = srcStride[1];

  const ptrdiff_t outLumaStride   = dstStride[0];
  const ptrdiff_t outChromaStride = dstStride[1];

  const ptrdiff_t chromaWidth     = (width + 1) >> 1;
  const ptrdiff_t chromaHeight    = height >> 1;

  ptrdiff_t line,i;
  __m128i xmm0,xmm1,xmm2,xmm3,xmm4;

  _mm_sfence();

  // Y
  for(line = 0; line < height; ++line) {
    PIXCONV_MEMCPY_ALIGNED(dst[0] + outLumaStride * line, src[0] + inLumaStride * line, width);
  }

  // U/V
  for(line = 0; line < chromaHeight; ++line) {
    const uint8_t * const u = src[1] + line * inChromaStride;
    const uint8_t * const v = src[2] + line * inChromaStride;
          uint8_t * const d = dst[1] + line * outChromaStride;

    for (i = 0; i < (chromaWidth - 31); i += 32) {
        PIXCONV_LOAD_PIXEL8_ALIGNED(xmm0, v + i);
        PIXCONV_LOAD_PIXEL8_ALIGNED(xmm1, u + i);
        PIXCONV_LOAD_PIXEL8_ALIGNED(xmm2, v + i + 16);
        PIXCONV_LOAD_PIXEL8_ALIGNED(xmm3, u + i + 16);

        xmm4 = xmm0;
        xmm0 = _mm_unpacklo_epi8(xmm1, xmm0);
        xmm4 = _mm_unpackhi_epi8(xmm1, xmm4);

        xmm1 = xmm2;
        xmm2 = _mm_unpacklo_epi8(xmm3, xmm2);
        xmm1 = _mm_unpackhi_epi8(xmm3, xmm1);

        PIXCONV_PUT_STREAM(d + (i << 1) + 0, xmm0);
        PIXCONV_PUT_STREAM(d + (i << 1) + 16, xmm4);
        PIXCONV_PUT_STREAM(d + (i << 1) + 32, xmm2);
        PIXCONV_PUT_STREAM(d + (i << 1) + 48, xmm1);
    }
    for (; i < chromaWidth; i += 16) {
      PIXCONV_LOAD_PIXEL8_ALIGNED(xmm0, v + i);
      PIXCONV_LOAD_PIXEL8_ALIGNED(xmm1, u + i);

      xmm2 = xmm0;
      xmm0 = _mm_unpacklo_epi8(xmm1, xmm0);
      xmm2 = _mm_unpackhi_epi8(xmm1, xmm2);

      PIXCONV_PUT_STREAM(d + (i << 1) +  0, xmm0);
      PIXCONV_PUT_STREAM(d + (i << 1) + 16, xmm2);
    }
  }

  return S_OK;
}

template <int uyvy>
DECLARE_CONV_FUNC_IMPL(convert_yuv422_yuy2_uyvy)
{
  const ptrdiff_t inLumaStride    = srcStride[0];
  const ptrdiff_t inChromaStride  = srcStride[1];

  const ptrdiff_t outStride       = dstStride[0];

  const ptrdiff_t chromaWidth     = (width + 1) >> 1;

  ptrdiff_t line,i;
  __m128i xmm0,xmm1,xmm2,xmm3,xmm4,xmm5;

  _mm_sfence();

  for (line = 0;  line < height; ++line) {
    const uint8_t * const y = src[0] + line * inLumaStride;
    const uint8_t * const u = src[1] + line * inChromaStride;
    const uint8_t * const v = src[2] + line * inChromaStride;
          uint8_t * const d = dst[0] + line * outStride;

    for (i = 0; i < chromaWidth; i+=16) {
      // Load pixels
      PIXCONV_LOAD_PIXEL8_ALIGNED(xmm0, (y+(i<<1)+ 0)); /* YYYY */
      PIXCONV_LOAD_PIXEL8_ALIGNED(xmm1, (y+(i<<1)+16)); /* YYYY */
      PIXCONV_LOAD_PIXEL8_ALIGNED(xmm2, (u+i));         /* UUUU */
      PIXCONV_LOAD_PIXEL8_ALIGNED(xmm3, (v+i));         /* VVVV */

      // Interleave Us and Vs
      xmm4 = xmm2;
      xmm4 = _mm_unpacklo_epi8(xmm4,xmm3);
      xmm2 = _mm_unpackhi_epi8(xmm2,xmm3);

      // Interlave those with the Ys
      if (uyvy) {
        xmm3 = xmm4;
        xmm3 = _mm_unpacklo_epi8(xmm3, xmm0);
        xmm4 = _mm_unpackhi_epi8(xmm4, xmm0);

        xmm5 = xmm2;
        xmm5 = _mm_unpacklo_epi8(xmm5, xmm1);
        xmm2 = _mm_unpackhi_epi8(xmm2, xmm1);
      } else {
        xmm3 = xmm0;
        xmm3 = _mm_unpacklo_epi8(xmm3, xmm4);
        xmm4 = _mm_unpackhi_epi8(xmm0, xmm4);

        xmm5 = xmm1;
        xmm5 = _mm_unpacklo_epi8(xmm5, xmm2);
        xmm2 = _mm_unpackhi_epi8(xmm1, xmm2);
      }

      PIXCONV_PUT_STREAM(d + (i << 2) + 0, xmm3);
      PIXCONV_PUT_STREAM(d + (i << 2) + 16, xmm4);
      PIXCONV_PUT_STREAM(d + (i << 2) + 32, xmm5);
      PIXCONV_PUT_STREAM(d + (i << 2) + 48, xmm2);
    }
  }

  return S_OK;
}

// Force creation of these two variants
template HRESULT CLAVPixFmtConverter::convert_yuv422_yuy2_uyvy<0>CONV_FUNC_PARAMS;
template HRESULT CLAVPixFmtConverter::convert_yuv422_yuy2_uyvy<1>CONV_FUNC_PARAMS;

template <int uyvy>
DECLARE_CONV_FUNC_IMPL(convert_yuv422_yuy2_uyvy_dither_le)
{
  const ptrdiff_t inLumaStride    = srcStride[0];
  const ptrdiff_t inChromaStride  = srcStride[1];
  const ptrdiff_t outStride       = dstStride[0];
  const ptrdiff_t chromaWidth     = (width + 1) >> 1;

  LAVDitherMode ditherMode = m_pSettings->GetDitherMode();
  const uint16_t *dithers = GetRandomDitherCoeffs(height, 4, 8, 0);
  if (dithers == nullptr)
    ditherMode = LAVDither_Ordered;

  ptrdiff_t line,i;
  __m128i xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7;

  _mm_sfence();

  for (line = 0;  line < height; ++line) {
    const uint16_t * const y = (const uint16_t *)(src[0] + line * inLumaStride);
    const uint16_t * const u = (const uint16_t *)(src[1] + line * inChromaStride);
    const uint16_t * const v = (const uint16_t *)(src[2] + line * inChromaStride);
          uint16_t * const d = (      uint16_t *)(dst[0] + line * outStride);

    // Load dithering coefficients for this line
    if (ditherMode == LAVDither_Random) {
      xmm4 = _mm_load_si128((const __m128i *)(dithers + (line << 5) +  0));
      xmm5 = _mm_load_si128((const __m128i *)(dithers + (line << 5) +  8));
      xmm6 = _mm_load_si128((const __m128i *)(dithers + (line << 5) + 16));
      xmm7 = _mm_load_si128((const __m128i *)(dithers + (line << 5) + 24));
    } else {
      PIXCONV_LOAD_DITHER_COEFFS(xmm7,line,8,dithers);
      xmm4 = xmm5 = xmm6 = xmm7;
    }

    for (i = 0; i < chromaWidth; i+=8) {
      // Load pixels
      PIXCONV_LOAD_PIXEL16_DITHER(xmm0, xmm4, (y+(i*2)+0), bpp);  /* YYYY */
      PIXCONV_LOAD_PIXEL16_DITHER(xmm1, xmm5, (y+(i*2)+8), bpp);  /* YYYY */
      PIXCONV_LOAD_PIXEL16_DITHER(xmm2, xmm6, (u+i), bpp);        /* UUUU */
      PIXCONV_LOAD_PIXEL16_DITHER(xmm3, xmm7, (v+i), bpp);        /* VVVV */

      // Pack Ys
      xmm0 = _mm_packus_epi16(xmm0, xmm1);

      // Interleave Us and Vs
      xmm2 = _mm_packus_epi16(xmm2, xmm2);
      xmm3 = _mm_packus_epi16(xmm3, xmm3);
      xmm2 = _mm_unpacklo_epi8(xmm2, xmm3);

      // Interlave those with the Ys
      if (uyvy) {
        xmm3 = xmm2;
        xmm3 = _mm_unpacklo_epi8(xmm3, xmm0);
        xmm2 = _mm_unpackhi_epi8(xmm2, xmm0);
      } else {
        xmm3 = xmm0;
        xmm3 = _mm_unpacklo_epi8(xmm3, xmm2);
        xmm2 = _mm_unpackhi_epi8(xmm0, xmm2);
      }

      PIXCONV_PUT_STREAM(d + (i << 1) + 0, xmm3);
      PIXCONV_PUT_STREAM(d + (i << 1) + 8, xmm2);
    }
  }

  return S_OK;
}

// Force creation of these two variants
template HRESULT CLAVPixFmtConverter::convert_yuv422_yuy2_uyvy_dither_le<0>CONV_FUNC_PARAMS;
template HRESULT CLAVPixFmtConverter::convert_yuv422_yuy2_uyvy_dither_le<1>CONV_FUNC_PARAMS;

DECLARE_CONV_FUNC_IMPL(convert_nv12_yv12)
{
  const ptrdiff_t inLumaStride    = srcStride[0];
  const ptrdiff_t inChromaStride  = srcStride[1];
  const ptrdiff_t outLumaStride   = dstStride[0];
  const ptrdiff_t outChromaStride = dstStride[1];
  const ptrdiff_t chromaHeight    = height >> 1;

  ptrdiff_t line, i;
  __m128i xmm0,xmm1,xmm2,xmm3,xmm7;

  xmm7 = _mm_set1_epi16(0x00FF);

  _mm_sfence();

  // Copy the y
  for (line = 0; line < height; line++) {
    PIXCONV_MEMCPY_ALIGNED(dst[0] + outLumaStride * line, src[0] + inLumaStride * line, width);
  }

  for (line = 0; line < chromaHeight; line++) {
    const uint8_t * const uv = src[1] + line * inChromaStride;
          uint8_t * const dv = dst[1] + outChromaStride * line;
          uint8_t * const du = dst[2] + outChromaStride * line;

    for (i = 0; i < width; i+=32) {
      PIXCONV_LOAD_PIXEL8_ALIGNED(xmm0, uv+i+0);
      PIXCONV_LOAD_PIXEL8_ALIGNED(xmm1, uv+i+16);
      xmm2 = xmm0;
      xmm3 = xmm1;

      // null out the high-order bytes to get the U values
      xmm0 = _mm_and_si128(xmm0, xmm7);
      xmm1 = _mm_and_si128(xmm1, xmm7);
      // right shift the V values
      xmm2 = _mm_srli_epi16(xmm2, 8);
      xmm3 = _mm_srli_epi16(xmm3, 8);
      // unpack the values
      xmm0 = _mm_packus_epi16(xmm0, xmm1);
      xmm2 = _mm_packus_epi16(xmm2, xmm3);

      PIXCONV_PUT_STREAM(du + (i>>1), xmm0);
      PIXCONV_PUT_STREAM(dv + (i>>1), xmm2);
    }
  }

  return S_OK;
}

DECLARE_CONV_FUNC_IMPL(convert_p010_nv12_sse2)
{
  const ptrdiff_t inStride = srcStride[0];
  const ptrdiff_t outStride = dstStride[0];
  const ptrdiff_t chromaHeight = (height >> 1);

  const ptrdiff_t byteWidth = width << 1;

  LAVDitherMode ditherMode = m_pSettings->GetDitherMode();
  const uint16_t *dithers = GetRandomDitherCoeffs(height, 2, 8, 0);
  if (dithers == nullptr)
    ditherMode = LAVDither_Ordered;

  __m128i xmm0, xmm1, xmm2, xmm3;

  _mm_sfence();

  ptrdiff_t line, i;

  for (line = 0; line < height; line++) {
    // Load dithering coefficients for this line
    if (ditherMode == LAVDither_Random) {
      xmm2 = _mm_load_si128((const __m128i *)(dithers + (line << 4) + 0));
      xmm3 = _mm_load_si128((const __m128i *)(dithers + (line << 4) + 8));
    } else {
      PIXCONV_LOAD_DITHER_COEFFS(xmm2, line, 8, dithers);
      xmm3 = xmm2;
    }

    const uint8_t *y = (src[0] + line * inStride);
    uint8_t *dy = (dst[0] + line * outStride);

    for (i = 0; i < byteWidth; i += 32) {
      PIXCONV_LOAD_ALIGNED(xmm0, y + i + 0);
      PIXCONV_LOAD_ALIGNED(xmm1, y + i + 16);

      // apply dithering coeffs
      xmm0 = _mm_adds_epu16(xmm0, xmm2);
      xmm1 = _mm_adds_epu16(xmm1, xmm3);

      // shift and pack to 8-bit
      xmm0 = _mm_packus_epi16(_mm_srli_epi16(xmm0, 8), _mm_srli_epi16(xmm1, 8));

      PIXCONV_PUT_STREAM(dy + (i >> 1), xmm0);
    }
  }

  for (line = 0; line < chromaHeight; line++) {
    // Load dithering coefficients for this line
    if (ditherMode == LAVDither_Random) {
      xmm2 = _mm_load_si128((const __m128i *)(dithers + (line << 4) + 0));
      xmm3 = _mm_load_si128((const __m128i *)(dithers + (line << 4) + 8));
    } else {
      PIXCONV_LOAD_DITHER_COEFFS(xmm2, line, 8, dithers);
      xmm3 = xmm2;
    }

    const uint8_t *uv = (src[1] + line * inStride);
    uint8_t *duv = (dst[1] + line * outStride);

    for (i = 0 ; i < byteWidth; i += 32) {
      PIXCONV_LOAD_ALIGNED(xmm0, uv + i + 0);
      PIXCONV_LOAD_ALIGNED(xmm1, uv + i + 16);

      // apply dithering coeffs
      xmm0 = _mm_adds_epu16(xmm0, xmm2);
      xmm1 = _mm_adds_epu16(xmm1, xmm3);

      // shift and pack to 8-bit
      xmm0 = _mm_packus_epi16(_mm_srli_epi16(xmm0, 8), _mm_srli_epi16(xmm1, 8));

      PIXCONV_PUT_STREAM(duv + (i >> 1), xmm0);
    }
  }

  return S_OK;
}
