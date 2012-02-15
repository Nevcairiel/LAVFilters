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

#pragma once

// Load the dithering coefficients for this line
// reg   - register to load coefficients into
// line  - index of line to process (0 based)
// bits  - number of bits to dither (for 10 -> 8, set to 2)
#define PIXCONV_LOAD_DITHER_COEFFS(reg,line,bits,name)  \
  const uint16_t *name = dither_8x8_256[(line) % 8];    \
  reg = _mm_load_si128((const __m128i *)name);          \
  reg = _mm_srli_epi16(reg, 8-bits); /* shift to the required dithering strength */

// Load 8 16-bit pixels into a register, and dither them to 8 bit
// The 8-bit pixels will be in the high-bytes of the 8 16-bit parts
// NOTE: the low-bytes are clobbered, and not empty.
// reg   - register to store pixels in
// dreg  - register with dithering coefficients
// src   - memory pointer of the source
// shift - shift offset to 8-bit (ie. 2 for 10bit)
#define PIXCONV_LOAD_PIXEL16_DITHER_HIGH(reg,dreg,src,shift)          \
  reg = _mm_load_si128((const __m128i *)(src)); /* load (aligned) */  \
  reg = _mm_slli_epi16(reg, 8-shift);           /* shift to 16-bit */ \
  reg = _mm_adds_epu16(reg, dreg);              /* dither */

// Load 8 16-bit pixels into a register, and dither them to 8 bit
// The 8-bit pixels will be in the low-bytes of the 8 16-bit parts
// reg   - register to store pixels in
// dreg  - register with dithering coefficients
// src   - memory pointer of the source
// shift - shift offset to 8-bit (ie. 2 for 10bit)
#define PIXCONV_LOAD_PIXEL16_DITHER(reg,dreg,src,shift)          \
  PIXCONV_LOAD_PIXEL16_DITHER_HIGH(reg,dreg,src,shift)           \
  reg = _mm_srli_epi16(reg, 8);                 /* shift to 8-bit */

// Load 8 16-bit pixels into a register, and dither them to 8 bit
// The 8-bit pixels will be in the 8 low-bytes in the register
// reg   - register to store pixels in
// dreg  - register with dithering coefficients
// src   - memory pointer of the source
// shift - shift offset to 16-bit (ie. 6 for 10bit)
#define PIXCONV_LOAD_PIXEL16_DITHER_PACKED(reg,dreg,zero,src,shift)   \
  PIXCONV_LOAD_PIXEL16_DITHER(reg,dreg,src,shift) /* load unpacked */ \
  reg = _mm_packus_epi16(reg, zero);              /* pack */

// Load 16 8-bit pixels into a register
// reg   - register to store pixels in
// src   - memory pointer of the source
#define PIXCONV_LOAD_PIXEL8(reg,src) \
  reg = _mm_loadu_si128((const __m128i *)(src));     /* load (unaligned) */

// Load 16 8-bit pixels into a register, using aligned memory access
// reg   - register to store pixels in
// src   - memory pointer of the source
#define PIXCONV_LOAD_PIXEL8_ALIGNED(reg,src) \
  reg = _mm_load_si128((const __m128i *)(src));      /* load (aligned) */

// Load 8 16-bit pixels into a register, using aligned memory access
// reg   - register to store pixels in
// src   - memory pointer of the source
#define PIXCONV_LOAD_PIXEL16(reg,src,shift)                             \
  reg = _mm_load_si128((const __m128i *)(src));  /* load (aligned) */     \
  reg = _mm_slli_epi16(reg, shift);            /* shift to 16-bit */

// Load 4 8-bit pixels into the register
// reg     - register to store pixels in
// src     - source memory
#define PIXCONV_LOAD_4PIXEL8(reg,src) \
   reg = _mm_cvtsi32_si128(*(const int*)(src)); /* load 32-bit (4 pixel) */

// Load 4 16-bit pixels into the register
// reg     - register to store pixels in
// src     - source memory
#define PIXCONV_LOAD_4PIXEL16(reg,src) \
   reg = _mm_loadl_epi64((const __m128i *)(src)); /* load 64-bit (4 pixel) */

// SSE2 Aligned memcpy
// dst - memory destination
// src - memory source
// len - size in bytes
#define PIXCONV_MEMCPY_ALIGNED(dst,src,len)     \
  {                                             \
    __m128i reg;                                \
    __m128i *dst128 =  (__m128i *)(dst);        \
    for (int i = 0; i < len; i+=16) {           \
      PIXCONV_LOAD_PIXEL8_ALIGNED(reg,(src)+i); \
      _mm_stream_si128(dst128++, reg);          \
    }                                           \
  }

// SSE2 Aligned memcpy (for 32-bit aligned data)
// dst - memory destination
// src - memory source
// len - size in bytes
#define PIXCONV_MEMCPY_ALIGNED32(dst,src,len)    \
  {                                              \
    __m128i reg1,reg2;                           \
    __m128i *dst128 =  (__m128i *)(dst);         \
    for (int i = 0; i < len; i+=32) {            \
      PIXCONV_LOAD_PIXEL8_ALIGNED(reg1,(src)+i); \
      PIXCONV_LOAD_PIXEL8_ALIGNED(reg2,(src)+i+16); \
      _mm_stream_si128(dst128++, reg1);          \
      _mm_stream_si128(dst128++, reg2);          \
    }                                            \
  }

// SSE2 Aligned memcpy
// Copys the same size from two source into two destinations at the same time
// Can be useful to copy U/V planes in one go
// dst1 - memory destination
// src1 - memory source
// dst2 - memory destination
// src2 - memory source
// len  - size in bytes
#define PIXCONV_MEMCPY_ALIGNED_TWO(dst1,src1,dst2,src2,len)     \
  {                                               \
    __m128i reg1,reg2;                            \
    __m128i *dst128_1 =  (__m128i *)(dst1);       \
    __m128i *dst128_2 =  (__m128i *)(dst2);       \
    for (int i = 0; i < len; i+=16) {             \
      PIXCONV_LOAD_PIXEL8_ALIGNED(reg1,(src1)+i); \
      PIXCONV_LOAD_PIXEL8_ALIGNED(reg2,(src2)+i); \
      _mm_stream_si128(dst128_1++, reg1);         \
      _mm_stream_si128(dst128_2++, reg2);         \
    }                                             \
  }
