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
#include "pixconv_internal.h"

extern "C" {
#include "libavutil/intreadwrite.h"
};

#define ALIGN(x,a) (((x)+(a)-1UL)&~((a)-1UL))

DECLARE_CONV_FUNC_IMPL(convert_generic)
{
  HRESULT hr = S_OK;

  PixelFormat inputFmt = GetFFInput();

  switch (m_OutputPixFmt) {
  case LAVOutPixFmt_YV12:
    hr = swscale_scale(inputFmt, PIX_FMT_YUV420P, src, srcStride, dst, width, height, dstStride, lav_pixfmt_desc[m_OutputPixFmt], true);
    break;
  case LAVOutPixFmt_NV12:
    hr = swscale_scale(inputFmt, PIX_FMT_NV12, src, srcStride, dst, width, height, dstStride, lav_pixfmt_desc[m_OutputPixFmt]);
    break;
  case LAVOutPixFmt_YUY2:
    hr = ConvertTo422Packed(src, srcStride, dst, width, height, dstStride);
    break;
  case LAVOutPixFmt_UYVY:
    hr = ConvertTo422Packed(src, srcStride, dst, width, height, dstStride);
    break;
  case LAVOutPixFmt_AYUV:
    hr = ConvertToAYUV(src, srcStride, dst, width, height, dstStride);
    break;
  case LAVOutPixFmt_P010:
    hr = ConvertToPX1X(src, srcStride, dst, width, height, dstStride, 2);
    break;
  case LAVOutPixFmt_P016:
    hr = ConvertToPX1X(src, srcStride, dst, width, height, dstStride, 2);
    break;
  case LAVOutPixFmt_P210:
    hr = ConvertToPX1X(src, srcStride, dst, width, height, dstStride, 1);
    break;
  case LAVOutPixFmt_P216:
    hr = ConvertToPX1X(src, srcStride, dst, width, height, dstStride, 1);
    break;
  case LAVOutPixFmt_Y410:
    hr = ConvertToY410(src, srcStride, dst, width, height, dstStride);
    break;
  case LAVOutPixFmt_Y416:
    hr = ConvertToY416(src, srcStride, dst, width, height, dstStride);
    break;
  case LAVOutPixFmt_RGB32:
    hr = swscale_scale(inputFmt, PIX_FMT_BGRA, src, srcStride, dst, width, height, dstStride * 4, lav_pixfmt_desc[m_OutputPixFmt]);
    break;
  case LAVOutPixFmt_RGB24:
    hr = swscale_scale(inputFmt, PIX_FMT_BGR24, src, srcStride, dst, width, height, dstStride * 3, lav_pixfmt_desc[m_OutputPixFmt]);
    break;
  case LAVOutPixFmt_v210:
    hr = ConvertTov210(src, srcStride, dst, width, height, dstStride);
    break;
  case LAVOutPixFmt_v410:
    hr = ConvertTov410(src, srcStride, dst, width, height, dstStride);
    break;
  case LAVOutPixFmt_YV16:
    hr = swscale_scale(inputFmt, PIX_FMT_YUV422P, src, srcStride, dst, width, height, dstStride, lav_pixfmt_desc[m_OutputPixFmt], true);
    break;
  case LAVOutPixFmt_YV24:
    hr = swscale_scale(inputFmt, PIX_FMT_YUV444P, src, srcStride, dst, width, height, dstStride, lav_pixfmt_desc[m_OutputPixFmt], true);
    break;
  default:
    ASSERT(0);
    hr = E_FAIL;
    break;
  }

  return S_OK;
}

inline SwsContext *CLAVPixFmtConverter::GetSWSContext(int width, int height, enum PixelFormat srcPix, enum PixelFormat dstPix, int flags)
{
  if (!m_pSwsContext || swsWidth != width || swsHeight != height) {
    // Get context
    m_pSwsContext = sws_getCachedContext(m_pSwsContext,
                                 width, height, srcPix,
                                 width, height, dstPix,
                                 flags|SWS_PRINT_INFO, NULL, NULL, NULL);

    int *inv_tbl = NULL, *tbl = NULL;
    int srcRange, dstRange, brightness, contrast, saturation;
    int ret = sws_getColorspaceDetails(m_pSwsContext, &inv_tbl, &srcRange, &tbl, &dstRange, &brightness, &contrast, &saturation);
    if (ret >= 0) {
      const int *rgbTbl = NULL;
      if (m_ColorProps.VideoTransferMatrix != DXVA2_VideoTransferMatrix_Unknown) {
        int colorspace = SWS_CS_ITU709;
        switch (m_ColorProps.VideoTransferMatrix) {
        case DXVA2_VideoTransferMatrix_BT709:
          colorspace = SWS_CS_ITU709;
          break;
        case DXVA2_VideoTransferMatrix_BT601:
          colorspace = SWS_CS_ITU601;
          break;
        case DXVA2_VideoTransferMatrix_SMPTE240M:
          colorspace = SWS_CS_SMPTE240M;
          break;
        }
        rgbTbl = sws_getCoefficients(colorspace);
      } else {
        BOOL isHD = (height >= 720 || width >= 1280);
        rgbTbl = sws_getCoefficients(isHD ? SWS_CS_ITU709 : SWS_CS_ITU601);
      }
      srcRange = dstRange = (m_ColorProps.NominalRange == DXVA2_NominalRange_0_255);
      sws_setColorspaceDetails(m_pSwsContext, rgbTbl, srcRange, tbl, dstRange, brightness, contrast, saturation);
    }
    swsWidth = width;
    swsHeight = height;
  }
  return m_pSwsContext;
}

HRESULT CLAVPixFmtConverter::swscale_scale(enum PixelFormat srcPix, enum PixelFormat dstPix, const uint8_t* const src[], const int srcStride[], BYTE *pOut, int width, int height, int stride, LAVOutPixFmtDesc pixFmtDesc, bool swapPlanes12)
{
  uint8_t *dst[4];
  int     dstStride[4];
  int     i, ret;

  SwsContext *ctx = GetSWSContext(width, height, srcPix, dstPix, SWS_BILINEAR);
  CheckPointer(m_pSwsContext, E_POINTER);

  memset(dst, 0, sizeof(dst));
  memset(dstStride, 0, sizeof(dstStride));

  dst[0] = pOut;
  dstStride[0] = stride;
  for (i = 1; i < pixFmtDesc.planes; ++i) {
    dst[i] = dst[i-1] + (stride / pixFmtDesc.planeWidth[i-1]) * (height / pixFmtDesc.planeHeight[i-1]);
    dstStride[i] = stride / pixFmtDesc.planeWidth[i];
  }

  if (swapPlanes12) {
    BYTE *tmp = dst[1];
    dst[1] = dst[2];
    dst[2] = tmp;
  }
  ret = sws_scale(ctx, src, srcStride, 0, height, dst, dstStride);

  return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertTo422Packed(const uint8_t* const src[4], const int srcStride[4], BYTE *pOut, int width, int height, int dstStride)
{
  const BYTE *y = NULL;
  const BYTE *u = NULL;
  const BYTE *v = NULL;
  int line, i;
  int sourceStride = 0;
  BYTE *pTmpBuffer = NULL;

  if (m_InputPixFmt != LAVPixFmt_YUV422) {
    uint8_t *dst[4] = {NULL};
    int     dstStride[4] = {0};
    int scaleStride = FFALIGN(width, 32);

    pTmpBuffer = (BYTE *)av_malloc(height * scaleStride * 2);

    dst[0] = pTmpBuffer;
    dst[1] = dst[0] + (height * scaleStride);
    dst[2] = dst[1] + (height * scaleStride / 2);
    dst[3] = NULL;

    dstStride[0] = scaleStride;
    dstStride[1] = scaleStride / 2;
    dstStride[2] = scaleStride / 2;
    dstStride[3] = 0;

    SwsContext *ctx = GetSWSContext(width, height, GetFFInput(), PIX_FMT_YUV422P, SWS_FAST_BILINEAR);
    sws_scale(ctx, src, srcStride, 0, height, dst, dstStride);

    y = dst[0];
    u = dst[1];
    v = dst[2];
    sourceStride = scaleStride;
  }  else {
    y = src[0];
    u = src[1];
    v = src[2];
    sourceStride = srcStride[0];
  }

  dstStride <<= 1;

#define YUV422_PACK_YUY2(offset) *idst++ = y[(i+offset) * 2] | (u[i+offset] << 8) | (y[(i+offset) * 2 + 1] << 16) | (v[i+offset] << 24);
#define YUV422_PACK_UYVY(offset) *idst++ = u[i+offset] | (y[(i+offset) * 2] << 8) | (v[i+offset] << 16) | (y[(i+offset) * 2 + 1] << 24);

  BYTE *out = pOut;
  int halfwidth = width >> 1;
  int halfstride = sourceStride >> 1;

  if (m_OutputPixFmt == LAVOutPixFmt_YUY2) {
    for (line = 0; line < height; ++line) {
      uint32_t *idst = (uint32_t *)out;
      for (i = 0; i < (halfwidth - 7); i+=8) {
        YUV422_PACK_YUY2(0)
        YUV422_PACK_YUY2(1)
        YUV422_PACK_YUY2(2)
        YUV422_PACK_YUY2(3)
        YUV422_PACK_YUY2(4)
        YUV422_PACK_YUY2(5)
        YUV422_PACK_YUY2(6)
        YUV422_PACK_YUY2(7)
      }
      for(; i < halfwidth; ++i) {
        YUV422_PACK_YUY2(0)
      }
      y += sourceStride;
      u += halfstride;
      v += halfstride;
      out += dstStride;
    }
  } else {
    for (line = 0; line < height; ++line) {
      uint32_t *idst = (uint32_t *)out;
      for (i = 0; i < (halfwidth - 7); i+=8) {
        YUV422_PACK_UYVY(0)
        YUV422_PACK_UYVY(1)
        YUV422_PACK_UYVY(2)
        YUV422_PACK_UYVY(3)
        YUV422_PACK_UYVY(4)
        YUV422_PACK_UYVY(5)
        YUV422_PACK_UYVY(6)
        YUV422_PACK_UYVY(7)
      }
      for(; i < halfwidth; ++i) {
        YUV422_PACK_UYVY(0)
      }
      y += sourceStride;
      u += halfstride;
      v += halfstride;
      out += dstStride;
    }
  }

  av_freep(&pTmpBuffer);

  return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertToAYUV(const uint8_t* const src[4], const int srcStride[4], BYTE *pOut, int width, int height, int dstStride)
{
  const BYTE *y = NULL;
  const BYTE *u = NULL;
  const BYTE *v = NULL;
  int line, i = 0;
  int sourceStride = 0;
  BYTE *pTmpBuffer = NULL;

  if (m_InputPixFmt != LAVPixFmt_YUV444) {
    uint8_t *dst[4] = {NULL};
    int     swStride[4] = {0};
    int scaleStride = FFALIGN(dstStride, 32);

    pTmpBuffer = (BYTE *)av_malloc(height * scaleStride * 3);

    dst[0] = pTmpBuffer;
    dst[1] = dst[0] + (height * scaleStride);
    dst[2] = dst[1] + (height * scaleStride);
    dst[3] = NULL;
    swStride[0] = scaleStride;
    swStride[1] = scaleStride;
    swStride[2] = scaleStride;
    swStride[3] = 0;

    SwsContext *ctx = GetSWSContext(width, height, GetFFInput(), PIX_FMT_YUV444P, SWS_POINT);
    sws_scale(ctx, src, srcStride, 0, height, dst, swStride);

    y = dst[0];
    u = dst[1];
    v = dst[2];
    sourceStride = scaleStride;
  } else {
    y = src[0];
    u = src[1];
    v = src[2];
    sourceStride = srcStride[0];
  }

#define YUV444_PACK_AYUV(offset) *idst++ = v[i+offset] | (u[i+offset] << 8) | (y[i+offset] << 16) | (0xff << 24);

  BYTE *out = pOut;
  for (line = 0; line < height; ++line) {
    int32_t *idst = (int32_t *)out;
    for (i = 0; i < (width-7); i+=8) {
      YUV444_PACK_AYUV(0)
      YUV444_PACK_AYUV(1)
      YUV444_PACK_AYUV(2)
      YUV444_PACK_AYUV(3)
      YUV444_PACK_AYUV(4)
      YUV444_PACK_AYUV(5)
      YUV444_PACK_AYUV(6)
      YUV444_PACK_AYUV(7)
    }
    for (; i < width; ++i) {
      YUV444_PACK_AYUV(0)
    }
    y += sourceStride;
    u += sourceStride;
    v += sourceStride;
    out += dstStride << 2;
  }

  av_freep(&pTmpBuffer);

  return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertToPX1X(const uint8_t* const src[4], const int srcStride[4], BYTE *pOut, int width, int height, int dstStride, int chromaVertical)
{
  const BYTE *y = NULL;
  const BYTE *u = NULL;
  const BYTE *v = NULL;
  int line, i = 0;
  int sourceStride = 0;

  int shift = 0;

  // Stride needs to be doubled for 16-bit per pixel
  dstStride <<= 1;

  BYTE *pTmpBuffer = NULL;

  if ((chromaVertical == 1 && m_InputPixFmt != LAVPixFmt_YUV422bX) || (chromaVertical == 2 && m_InputPixFmt != LAVPixFmt_YUV420bX)) {
    uint8_t *dst[4] = {NULL};
    int     dstStride[4] = {0};
    int scaleStride = FFALIGN(width, 32) * 2;

    pTmpBuffer = (BYTE *)av_malloc(height * scaleStride * 2);

    dst[0] = pTmpBuffer;
    dst[1] = dst[0] + (height * scaleStride);
    dst[2] = dst[1] + ((height / chromaVertical) * (scaleStride / 2));
    dst[3] = NULL;
    dstStride[0] = scaleStride;
    dstStride[1] = scaleStride / 2;
    dstStride[2] = scaleStride / 2;
    dstStride[3] = 0;

    SwsContext *ctx = GetSWSContext(width, height, GetFFInput(), chromaVertical == 1 ? PIX_FMT_YUV422P16LE : PIX_FMT_YUV420P16LE, SWS_POINT);
    sws_scale(ctx, src, srcStride, 0, height, dst, dstStride);

    y = dst[0];
    u = dst[1];
    v = dst[2];
    sourceStride = scaleStride;
  } else {
    y = src[0];
    u = src[1];
    v = src[2];
    sourceStride = srcStride[0];

    shift = (16 - m_InBpp);
  }

  // copy Y
  BYTE *pLineOut = pOut;
  const BYTE *pLineIn = y;
  for (line = 0; line < height; ++line) {
    if (shift == 0) {
      memcpy(pLineOut, pLineIn, width * 2);
    } else {
      const int16_t *yc = (int16_t *)pLineIn;
      int16_t *idst = (int16_t *)pLineOut;
      for (i = 0; i < width; ++i) {
        int32_t yv = AV_RL16(yc+i);
        if (shift) yv <<= shift;
        *idst++ = yv;
      }
    }
    pLineOut += dstStride;
    pLineIn += sourceStride;
  }

  sourceStride >>= 2;

  // Merge U/V
  BYTE *out = pLineOut;
  const int16_t *uc = (int16_t *)u;
  const int16_t *vc = (int16_t *)v;
  for (line = 0; line < height/chromaVertical; ++line) {
    int32_t *idst = (int32_t *)out;
    for (i = 0; i < width/2; ++i) {
      int32_t uv = AV_RL16(uc+i);
      int32_t vv = AV_RL16(vc+i);
      if (shift) {
        uv <<= shift;
        vv <<= shift;
      }
      *idst++ = uv | (vv << 16);
    }
    uc += sourceStride;
    vc += sourceStride;
    out += dstStride;
  }

  av_freep(&pTmpBuffer);

  return S_OK;
}

#define YUV444_PACKED_LOOP_HEAD(width, height, y, u, v, out) \
  for (int line = 0; line < height; ++line) { \
    int32_t *idst = (int32_t *)out; \
    for(int i = 0; i < width; ++i) { \
      int32_t yv, uv, vv;

#define YUV444_PACKED_LOOP_HEAD_LE(width, height, y, u, v, out) \
  YUV444_PACKED_LOOP_HEAD(width, height, y, u, v, out) \
    yv = AV_RL16(y+i); uv = AV_RL16(u+i); vv = AV_RL16(v+i);

#define YUV444_PACKED_LOOP_END(y, u, v, out, srcStride, dstStride) \
    } \
    y += srcStride; \
    u += srcStride; \
    v += srcStride; \
    out += dstStride; \
  }

HRESULT CLAVPixFmtConverter::ConvertToY410(const uint8_t* const src[4], const int srcStride[4], BYTE *pOut, int width, int height, int dstStride)
{
  const int16_t *y = NULL;
  const int16_t *u = NULL;
  const int16_t *v = NULL;
  int sourceStride = 0;
  bool b9Bit = false;

  BYTE *pTmpBuffer = NULL;

  if (m_InputPixFmt != LAVPixFmt_YUV444bX || m_InBpp > 10) {
    uint8_t *dst[4] = {NULL};
    int     dstStride[4] = {0};
    int scaleStride = FFALIGN(width, 32);

    pTmpBuffer = (BYTE *)av_malloc(height * scaleStride * 6);

    dst[0] = pTmpBuffer;
    dst[1] = dst[0] + (height * scaleStride * 2);
    dst[2] = dst[1] + (height * scaleStride * 2);
    dst[3] = NULL;
    dstStride[0] = scaleStride * 2;
    dstStride[1] = scaleStride * 2;
    dstStride[2] = scaleStride * 2;
    dstStride[3] = 0;

    SwsContext *ctx = GetSWSContext(width, height, GetFFInput(), PIX_FMT_YUV444P10LE, SWS_POINT);
    sws_scale(ctx, src, srcStride, 0, height, dst, dstStride);

    y = (int16_t *)dst[0];
    u = (int16_t *)dst[1];
    v = (int16_t *)dst[2];
    sourceStride = scaleStride;
  } else {
    y = (int16_t *)src[0];
    u = (int16_t *)src[1];
    v = (int16_t *)src[2];
    sourceStride = srcStride[0] / 2;

    b9Bit = (m_InBpp == 9);
  }

  // 32-bit per pixel
  dstStride *= 4;

#define YUV444_Y410_PACK \
  *idst++ = (uv & 0x3FF) | ((yv & 0x3FF) << 10) | ((vv & 0x3FF) << 20) | (3 << 30);

  BYTE *out = pOut;
  YUV444_PACKED_LOOP_HEAD_LE(width, height, y, u, v, out)
    if (b9Bit) {
      yv <<= 1;
      uv <<= 1;
      vv <<= 1;
    }
    YUV444_Y410_PACK
  YUV444_PACKED_LOOP_END(y, u, v, out, sourceStride, dstStride)

  av_freep(&pTmpBuffer);

  return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertToY416(const uint8_t* const src[4], const int srcStride[4], BYTE *pOut, int width, int height, int dstStride)
{
  const int16_t *y = NULL;
  const int16_t *u = NULL;
  const int16_t *v = NULL;
  int sourceStride = 0;

  BYTE *pTmpBuffer = NULL;

  if (m_InputPixFmt != LAVPixFmt_YUV444bX || m_InBpp != 16) {
    uint8_t *dst[4] = {NULL};
    int     dstStride[4] = {0};
    int scaleStride = FFALIGN(width, 32);

    pTmpBuffer = (BYTE *)av_malloc(height * scaleStride * 6);

    dst[0] = pTmpBuffer;
    dst[1] = dst[0] + (height * scaleStride * 2);
    dst[2] = dst[1] + (height * scaleStride * 2);
    dst[3] = NULL;
    dstStride[0] = scaleStride * 2;
    dstStride[1] = scaleStride * 2;
    dstStride[2] = scaleStride * 2;
    dstStride[3] = 0;

    SwsContext *ctx = GetSWSContext(width, height, GetFFInput(), PIX_FMT_YUV444P16LE, SWS_POINT);
    sws_scale(ctx, src, srcStride, 0, height, dst, dstStride);

    y = (int16_t *)dst[0];
    u = (int16_t *)dst[1];
    v = (int16_t *)dst[2];
    sourceStride = scaleStride;
  } else {
    y = (int16_t *)src[0];
    u = (int16_t *)src[1];
    v = (int16_t *)src[2];
    sourceStride = srcStride[0] / 2;
  }

  // 64-bit per pixel
  dstStride <<= 3;

#define YUV444_Y416_PACK \
  *idst++ = 0xFFFF | (vv << 16); \
  *idst++ = yv | (uv << 16);

  BYTE *out = pOut;
  YUV444_PACKED_LOOP_HEAD_LE(width, height, y, u, v, out)
    YUV444_Y416_PACK
  YUV444_PACKED_LOOP_END(y, u, v, out, sourceStride, dstStride)

  av_freep(&pTmpBuffer);

  return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertTov210(const uint8_t* const src[4], const int srcStride[4], BYTE *pOut, int width, int height, int dstStride)
{
  const int16_t *y = NULL;
  const int16_t *u = NULL;
  const int16_t *v = NULL;
  int sourceStride = 0;

  BYTE *pTmpBuffer = NULL;

  if (m_InputPixFmt != LAVPixFmt_YUV422bX || m_InBpp != 10) {
    uint8_t *dst[4] = {NULL};
    int     dstStride[4] = {0};
    int scaleStride = FFALIGN(width, 32);

    pTmpBuffer = (BYTE *)av_malloc(height * scaleStride * 6);

    dst[0] = pTmpBuffer;
    dst[1] = dst[0] + (height * scaleStride * 2);
    dst[2] = dst[1] + (height * scaleStride * 2);
    dst[3] = NULL;
    dstStride[0] = scaleStride * 2;
    dstStride[1] = scaleStride * 2;
    dstStride[2] = scaleStride * 2;
    dstStride[3] = 0;

    SwsContext *ctx = GetSWSContext(width, height, GetFFInput(), PIX_FMT_YUV422P10LE, SWS_POINT);
    sws_scale(ctx, src, srcStride, 0, height, dst, dstStride);

    y = (int16_t *)dst[0];
    u = (int16_t *)dst[1];
    v = (int16_t *)dst[2];
    sourceStride = scaleStride;
  } else {
    y = (int16_t *)src[0];
    u = (int16_t *)src[1];
    v = (int16_t *)src[2];
    sourceStride = srcStride[0] / 2;
  }

  // 32-bit per pixel
  dstStride = (FFALIGN(dstStride, 48) / 48) * 128;

  BYTE *pdst = pOut;
  int32_t *p = (int32_t *)pdst;
  int w;

//#define CLIP(v) av_clip(v, 4, 1019)
#define CLIP(v) (v & 0x03FF)
#define WRITE_PIXELS(a, b, c)       \
  do {                              \
    val =   CLIP(*a++);             \
    val |= (CLIP(*b++) << 10) |     \
           (CLIP(*c++) << 20);      \
    *p++ = val;                     \
  } while (0)

  for (int h = 0; h < height; h++) {
    uint32_t val;
    for (w = 0; w < width - 5; w += 6) {
      WRITE_PIXELS(u, y, v);
      WRITE_PIXELS(y, u, y);
      WRITE_PIXELS(v, y, u);
      WRITE_PIXELS(y, v, y);
    }
    if (w < width - 1) {
      WRITE_PIXELS(u, y, v);

      val = CLIP(*y++);
      if (w == width - 2)
        *p++ = val;
      if (w < width - 3) {
        val |= (CLIP(*u++) << 10) | (CLIP(*y++) << 20);
        *p++ = val;

        val = CLIP(*v++) | (CLIP(*y++) << 10);
        *p++ = val;
      }
    }

    pdst += dstStride;
    memset(p, 0, pdst - (BYTE *)p);
    p = (int32_t *)pdst;
    y += sourceStride - width;
    u += (sourceStride - width) >> 1;
    v += (sourceStride - width) >> 1;
  }
  av_freep(&pTmpBuffer);

  return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertTov410(const uint8_t* const src[4], const int srcStride[4], BYTE *pOut, int width, int height, int dstStride)
{
  const int16_t *y = NULL;
  const int16_t *u = NULL;
  const int16_t *v = NULL;
  int sourceStride = 0;
  bool b9Bit = false;

  BYTE *pTmpBuffer = NULL;

  if (m_InputPixFmt != LAVPixFmt_YUV444bX || m_InBpp > 10) {
    uint8_t *dst[4] = {NULL};
    int     dstStride[4] = {0};
    int scaleStride = FFALIGN(width, 32);

    pTmpBuffer = (BYTE *)av_malloc(height * scaleStride * 6);

    dst[0] = pTmpBuffer;
    dst[1] = dst[0] + (height * scaleStride * 2);
    dst[2] = dst[1] + (height * scaleStride * 2);
    dst[3] = NULL;
    dstStride[0] = scaleStride * 2;
    dstStride[1] = scaleStride * 2;
    dstStride[2] = scaleStride * 2;
    dstStride[3] = 0;

    SwsContext *ctx = GetSWSContext(width, height, GetFFInput(), PIX_FMT_YUV444P10LE, SWS_POINT);
    sws_scale(ctx, src, srcStride, 0, height, dst, dstStride);

    y = (int16_t *)dst[0];
    u = (int16_t *)dst[1];
    v = (int16_t *)dst[2];
    sourceStride = scaleStride;
  } else {
    y = (int16_t *)src[0];
    u = (int16_t *)src[1];
    v = (int16_t *)src[2];
    sourceStride = srcStride[0] / 2;

    b9Bit = (m_InBpp == 9);
  }

  // 32-bit per pixel
  dstStride *= 4;

#define YUV444_v410_PACK \
  *idst++ = ((uv & 0x3FF) << 2) | ((yv & 0x3FF) << 12) | ((vv & 0x3FF) << 22);

  BYTE *out = pOut;
  YUV444_PACKED_LOOP_HEAD_LE(width, height, y, u, v, out)
    if (b9Bit) {
      yv <<= 1;
      uv <<= 1;
      vv <<= 1;
    }
    YUV444_v410_PACK
  YUV444_PACKED_LOOP_END(y, u, v, out, sourceStride, dstStride)

  av_freep(&pTmpBuffer);

  return S_OK;
}
