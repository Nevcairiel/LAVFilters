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
#include "LAVPixFmtConverter.h"

#include <MMReg.h>
#include "moreuuids.h"

typedef struct {
  enum PixelFormat ff_pix_fmt; // ffmpeg pixel format
  int num_pix_fmt;
  enum LAVVideoPixFmts lav_pix_fmts[LAVPixFmt_NB];
} FF_LAV_PIXFMT_MAP;

static FF_LAV_PIXFMT_MAP lav_pixfmt_map[] = {
  // Default
  { PIX_FMT_NONE, 1, { LAVPixFmt_YV12, LAVPixFmt_NV12 } },

  // 4:2:0
  { PIX_FMT_YUV420P,  2, { LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUVJ420P, 2, { LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_NV12,     2, { LAVPixFmt_NV12, LAVPixFmt_YV12 } },
  { PIX_FMT_NV21,     2, { LAVPixFmt_NV12, LAVPixFmt_YV12 } },

  { PIX_FMT_YUV420P9BE,  3, { LAVPixFmt_P010, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV420P9LE,  3, { LAVPixFmt_P010, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV420P10BE, 3, { LAVPixFmt_P010, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV420P10LE, 3, { LAVPixFmt_P010, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV420P16BE, 4, { LAVPixFmt_P016, LAVPixFmt_P010, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV420P16LE, 4, { LAVPixFmt_P016, LAVPixFmt_P010, LAVPixFmt_YV12, LAVPixFmt_NV12 } },

  // 4:2:2
  { PIX_FMT_YUV422P,  3, { LAVPixFmt_YUY2, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUVJ422P, 3, { LAVPixFmt_YUY2, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUYV422,  3, { LAVPixFmt_YUY2, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_UYVY422,  3, { LAVPixFmt_YUY2, LAVPixFmt_YV12, LAVPixFmt_NV12 } },

  { PIX_FMT_YUV422P10BE, 4, { LAVPixFmt_P210, LAVPixFmt_YUY2, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV422P10LE, 4, { LAVPixFmt_P210, LAVPixFmt_YUY2, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV422P16BE, 4, { LAVPixFmt_P216, LAVPixFmt_YUY2, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV422P16LE, 4, { LAVPixFmt_P216, LAVPixFmt_YUY2, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  
  // 4:4:4
  { PIX_FMT_YUV444P,  4, { LAVPixFmt_AYUV, LAVPixFmt_YUY2, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUVJ444P, 4, { LAVPixFmt_AYUV, LAVPixFmt_YUY2, LAVPixFmt_YV12, LAVPixFmt_NV12 } },

  { PIX_FMT_YUV444P9BE,  5, { LAVPixFmt_Y410, LAVPixFmt_AYUV, LAVPixFmt_YUY2, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV444P9LE,  5, { LAVPixFmt_Y410, LAVPixFmt_AYUV, LAVPixFmt_YUY2, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV444P10BE, 5, { LAVPixFmt_Y410, LAVPixFmt_AYUV, LAVPixFmt_YUY2, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV444P10LE, 5, { LAVPixFmt_Y410, LAVPixFmt_AYUV, LAVPixFmt_YUY2, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV444P16BE, 5, { LAVPixFmt_Y416, LAVPixFmt_AYUV, LAVPixFmt_YUY2, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV444P16LE, 5, { LAVPixFmt_Y416, LAVPixFmt_AYUV, LAVPixFmt_YUY2, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
};

static LAVPixFmtDesc lav_pixfmt_desc[] = {
  { '21VY', 12, 3, { 1, 2, 2 }, { 1, 2, 2 } },        // YV12
  { '21VN', 12, 2, { 1, 2 }, { 1, 1 } },              // NV12
  { '2YUY', 16, 0 },                                  // YUY2 (packed)
  { 'VUYA', 32, 0 },                                  // AYUV (packed)
  { '010P', 15, 2, { 1, 2 }, { 1, 1 } },              // P010
  { '012P', 20, 2, { 1, 1 }, { 1, 1 } },              // P210
  { '014Y', 32, 0 },                                  // Y410 (packed)
  { '610P', 24, 2, { 1, 2 }, { 1, 1 } },              // P016
  { '612P', 32, 2, { 1, 1 }, { 1, 1 } },              // P216
  { '614Y', 64, 0 },                                  // Y416 (packed)
};

CLAVPixFmtConverter::CLAVPixFmtConverter()
  : m_InputPixFmt(PIX_FMT_NONE)
  , m_OutputPixFmt(LAVPixFmt_YV12)
  , m_pSwsContext(NULL)
  , swsWidth(0), swsHeight(0)
{
}


CLAVPixFmtConverter::~CLAVPixFmtConverter()
{
  DestroySWScale();
}

LAVVideoPixFmts CLAVPixFmtConverter::GetOutputBySubtype(const GUID *guid)
{
  DWORD FourCC = guid->Data1;
  for (int i = 0; i < countof(lav_pixfmt_desc); ++i) {
    if (lav_pixfmt_desc[i].fourcc == FourCC) {
      return (LAVVideoPixFmts)i;
    }
  }
  return LAVPixFmt_None;
}

LAVVideoPixFmts CLAVPixFmtConverter::GetPreferredOutput()
{
  int i = 0;
  for (i = 0; i < countof(lav_pixfmt_map); ++i) {
    if (lav_pixfmt_map[i].ff_pix_fmt == m_InputPixFmt)
      return lav_pixfmt_map[i].lav_pix_fmts[0];
  }
  return lav_pixfmt_map[0].lav_pix_fmts[0];
}

int CLAVPixFmtConverter::GetNumMediaTypes()
{
  int i = 0;
  for (i = 0; i < countof(lav_pixfmt_map); ++i) {
    if (lav_pixfmt_map[i].ff_pix_fmt == m_InputPixFmt)
      return lav_pixfmt_map[i].num_pix_fmt;
  }
  
  return lav_pixfmt_map[0].num_pix_fmt;
}

CMediaType CLAVPixFmtConverter::GetMediaType(int index, LONG biWidth, LONG biHeight, DWORD dwAspectX, DWORD dwAspectY, REFERENCE_TIME rtAvgTime)
{
  FF_LAV_PIXFMT_MAP *pixFmtMap = NULL;
  for (int i = 0; i < countof(lav_pixfmt_map); ++i) {
    if (lav_pixfmt_map[i].ff_pix_fmt == m_InputPixFmt) {
      pixFmtMap = &lav_pixfmt_map[i];
      break;
    }
  }
  if (!pixFmtMap)
    pixFmtMap = &lav_pixfmt_map[0];

  if (index >= pixFmtMap->num_pix_fmt)
    index = 0;

  CMediaType mt;
  GUID guid = FOURCCMap(lav_pixfmt_desc[pixFmtMap->lav_pix_fmts[index]].fourcc);

  mt.SetType(&MEDIATYPE_Video);
  mt.SetSubtype(&guid);
  mt.SetFormatType(&FORMAT_VideoInfo2);

  VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)mt.AllocFormatBuffer(sizeof(VIDEOINFOHEADER2));
  memset(vih2, 0, sizeof(VIDEOINFOHEADER2));

  vih2->rcSource.right = vih2->rcTarget.right = biWidth;
  vih2->rcSource.bottom = vih2->rcTarget.bottom = biHeight;
  vih2->AvgTimePerFrame = rtAvgTime;
  vih2->dwPictAspectRatioX = dwAspectX;
  vih2->dwPictAspectRatioY = dwAspectY;
  vih2->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  vih2->bmiHeader.biWidth = biWidth;
  vih2->bmiHeader.biHeight = biHeight;
  vih2->bmiHeader.biBitCount = lav_pixfmt_desc[pixFmtMap->lav_pix_fmts[index]].bpp;
  vih2->bmiHeader.biPlanes = lav_pixfmt_desc[pixFmtMap->lav_pix_fmts[index]].planes;
  vih2->bmiHeader.biSizeImage = (biWidth * biHeight * vih2->bmiHeader.biBitCount) >> 3;
  vih2->bmiHeader.biCompression = lav_pixfmt_desc[pixFmtMap->lav_pix_fmts[index]].fourcc;

  if (!vih2->bmiHeader.biPlanes) {
    vih2->bmiHeader.biPlanes = 1;
  }

  // Always set interlace flags, the samples will be flagged appropriately then.
  vih2->dwInterlaceFlags = AMINTERLACE_IsInterlaced | AMINTERLACE_DisplayModeBobOrWeave;

  mt.SetSampleSize(vih2->bmiHeader.biSizeImage);

  return mt;
}

inline SwsContext *CLAVPixFmtConverter::GetSWSContext(int width, int height, enum PixelFormat srcPix, enum PixelFormat dstPix, int flags)
{
  if (!m_pSwsContext || swsWidth != width || swsHeight != height) {
    m_pSwsContext = sws_getCachedContext(m_pSwsContext,
                                 width, height, srcPix,
                                 width, height, dstPix,
                                 flags|SWS_PRINT_INFO, NULL, NULL, NULL);
    swsWidth = width;
    swsHeight = height;
  }
  return m_pSwsContext;
}

HRESULT CLAVPixFmtConverter::swscale_scale(enum PixelFormat srcPix, enum PixelFormat dstPix, AVFrame *pFrame, BYTE *pOut, int width, int height, int stride, LAVPixFmtDesc pixFmtDesc, bool swapPlanes12)
{
  uint8_t *dst[4];
  int     dstStride[4];
  int     i, ret;

  SwsContext *ctx = GetSWSContext(width, height, srcPix, dstPix, SWS_BICUBIC);
  CheckPointer(m_pSwsContext, E_POINTER);

  memset(dst, 0, sizeof(dst));
  memset(dstStride, 0, sizeof(dstStride));

  dst[0] = pOut;
  dstStride[0] = stride;
  for (i = 1; i < pixFmtDesc.planes; ++i) {
    dst[i] = dst[i-1] + (stride / pixFmtDesc.planeWidth[i-1]) * (height / pixFmtDesc.planeWidth[i-1]);
    dstStride[i] = stride / pixFmtDesc.planeWidth[i];
  }
  
  if (swapPlanes12) {
    BYTE *tmp = dst[1];
    dst[1] = dst[2];
    dst[2] = tmp;
  }
  ret = sws_scale(ctx, pFrame->data, pFrame->linesize, 0, height, dst, dstStride);

  return S_OK;
}

HRESULT CLAVPixFmtConverter::Convert(AVFrame *pFrame, BYTE *pOut, int width, int height, int dstStride)
{
  HRESULT hr = S_OK;
  switch (m_OutputPixFmt) {
  case LAVPixFmt_YV12:
    {
      PixelFormat dst = PIX_FMT_YUV420P;
      if (m_InputPixFmt == PIX_FMT_YUVJ420P || m_InputPixFmt == PIX_FMT_YUVJ422P || m_InputPixFmt == PIX_FMT_YUVJ444P)
        dst = PIX_FMT_YUVJ420P;
      hr = swscale_scale(m_InputPixFmt, dst, pFrame, pOut, width, height, dstStride, lav_pixfmt_desc[m_OutputPixFmt], true);
    }
    break;
  case LAVPixFmt_NV12:
    hr = swscale_scale(m_InputPixFmt, PIX_FMT_NV12, pFrame, pOut, width, height, dstStride, lav_pixfmt_desc[m_OutputPixFmt]);
    break;
  case LAVPixFmt_YUY2:
    hr = swscale_scale(m_InputPixFmt, PIX_FMT_YUYV422, pFrame, pOut, width, height, dstStride * 2, lav_pixfmt_desc[m_OutputPixFmt]);
    break;
  case LAVPixFmt_AYUV:
    hr = ConvertToAYUV(pFrame, pOut, width, height, dstStride);
    break;
  case LAVPixFmt_P010:
    hr = ConvertToPX1X(pFrame, pOut, width, height, dstStride, 2);
    break;
  case LAVPixFmt_P016:
    hr = ConvertToPX1X(pFrame, pOut, width, height, dstStride, 2);
    break;
  case LAVPixFmt_P210:
    hr = ConvertToPX1X(pFrame, pOut, width, height, dstStride, 1);
    break;
  case LAVPixFmt_P216:
    hr = ConvertToPX1X(pFrame, pOut, width, height, dstStride, 1);
    break;
  case LAVPixFmt_Y410:
    hr = ConvertToY410(pFrame, pOut, width, height, dstStride);
    break;
  default:
    ASSERT(0);
    hr = E_FAIL;
    break;
  }
  return hr;
}

HRESULT CLAVPixFmtConverter::ConvertToAYUV(AVFrame *pFrame, BYTE *pOut, int width, int height, int stride)
{
  const BYTE *y = NULL;
  const BYTE *u = NULL;
  const BYTE *v = NULL;
  int line, i = 0;
  int srcStride = 0;
  BYTE *pTmpBuffer = NULL;

  if (m_InputPixFmt != PIX_FMT_YUV444P) {
    uint8_t *dst[4] = {NULL};
    int     dstStride[4] = {0};

    pTmpBuffer = (BYTE *)av_malloc(height * stride * 3);

    dst[0] = pTmpBuffer;
    dst[1] = dst[0] + (height * stride);
    dst[2] = dst[1] + (height * stride);
    dst[3] = NULL;
    dstStride[0] = stride;
    dstStride[1] = stride;
    dstStride[2] = stride;
    dstStride[3] = 0;

    SwsContext *ctx = GetSWSContext(width, height, m_InputPixFmt, PIX_FMT_YUV444P, SWS_POINT);
    sws_scale(ctx, pFrame->data, pFrame->linesize, 0, height, dst, dstStride);

    y = dst[0];
    u = dst[1];
    v = dst[2];
    srcStride = stride;
  } else {
    y = pFrame->data[0];
    u = pFrame->data[1];
    v = pFrame->data[2];
    srcStride = pFrame->linesize[0];
  }

  for (line = 0; line < height; ++line) {
    BYTE *pLine = pOut + line * stride * 4;
    int32_t *idst = (int32_t *)pLine;
    for (i = 0; i < width; ++i) {
      *idst++ = v[i] + (u[i] << 8) + (y[i] << 16) + (0xff << 24);
    }
    y += srcStride;
    u += srcStride;
    v += srcStride;
  }

  av_freep(&pTmpBuffer);

  return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertToPX1X(AVFrame *pFrame, BYTE *pOut, int width, int height, int stride, int chromaVertical)
{
  const BYTE *y = NULL;
  const BYTE *u = NULL;
  const BYTE *v = NULL;
  int line, i = 0;
  int srcStride = 0;

  // Stride needs to be doubled for 16-bit per pixel
  stride *= 2;

  BYTE *pTmpBuffer = NULL;

  PixelFormat pixFmtRequired = (chromaVertical == 2) ? PIX_FMT_YUV420P16LE : PIX_FMT_YUV422P16LE;

  if (m_InputPixFmt != pixFmtRequired) {
    uint8_t *dst[4] = {NULL};
    int     dstStride[4] = {0};

    pTmpBuffer = (BYTE *)av_malloc(height * stride * 2);

    dst[0] = pTmpBuffer;
    dst[1] = dst[0] + (height * stride);
    dst[2] = dst[1] + ((height / chromaVertical) * (stride / 2));
    dst[3] = NULL;
    dstStride[0] = stride;
    dstStride[1] = stride / 2;
    dstStride[2] = stride / 2;
    dstStride[3] = 0;

    SwsContext *ctx = GetSWSContext(width, height, m_InputPixFmt, pixFmtRequired, SWS_POINT);
    sws_scale(ctx, pFrame->data, pFrame->linesize, 0, height, dst, dstStride);

    y = dst[0];
    u = dst[1];
    v = dst[2];
    srcStride = stride;
  } else {
    y = pFrame->data[0];
    u = pFrame->data[1];
    v = pFrame->data[2];
    srcStride = pFrame->linesize[0];
  }

  // copy Y
  BYTE *pLineOut = pOut;
  const BYTE *pLineIn = y;
  for (line = 0; line < height; ++line) {
    memcpy(pLineOut, pLineIn, width * 2);
    pLineOut += stride;
    pLineIn += srcStride;
  }

  // Merge U/V
  BYTE *dstUV = pLineOut;
  int16_t *uc = (int16_t *)u;
  int16_t *vc = (int16_t *)v;
  for (line = 0; line < height/chromaVertical; ++line) {
    int32_t *idst = (int32_t *)(dstUV + line * stride);
    for (i = 0; i < width/2; ++i) {
      *idst++ = uc[i] + (vc[i] << 16);
    }
    uc += srcStride/4;
    vc += srcStride/4;
  }

  av_freep(&pTmpBuffer);

  return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertToY410(AVFrame *pFrame, BYTE *pOut, int width, int height, int stride)
{
  const BYTE *y = NULL;
  const BYTE *u = NULL;
  const BYTE *v = NULL;
  int line, i = 0;
  int srcStride = 0;
  bool bBigEndian = false;
  bool b9Bit = false;

  BYTE *pTmpBuffer = NULL;

  if (m_InputPixFmt != PIX_FMT_YUV444P10BE && m_InputPixFmt != PIX_FMT_YUV444P10LE && m_InputPixFmt != PIX_FMT_YUV444P9BE && m_InputPixFmt != PIX_FMT_YUV444P9LE) {
    uint8_t *dst[4] = {NULL};
    int     dstStride[4] = {0};

    pTmpBuffer = (BYTE *)av_malloc(height * stride * 6);

    dst[0] = pTmpBuffer;
    dst[1] = dst[0] + (height * stride * 2);
    dst[2] = dst[1] + (height * stride * 2);
    dst[3] = NULL;
    dstStride[0] = stride * 2;
    dstStride[1] = stride * 2;
    dstStride[2] = stride * 2;
    dstStride[3] = 0;

    SwsContext *ctx = GetSWSContext(width, height, m_InputPixFmt, PIX_FMT_YUV444P10LE, SWS_POINT);
    sws_scale(ctx, pFrame->data, pFrame->linesize, 0, height, dst, dstStride);

    y = dst[0];
    u = dst[1];
    v = dst[2];
    srcStride = stride * 2;
  } else {
    y = pFrame->data[0];
    u = pFrame->data[1];
    v = pFrame->data[2];
    srcStride = pFrame->linesize[0];

    bBigEndian = (m_InputPixFmt == PIX_FMT_YUV444P10BE || m_InputPixFmt == PIX_FMT_YUV444P9BE);
    b9Bit = (m_InputPixFmt == PIX_FMT_YUV444P9BE || PIX_FMT_YUV444P9LE);
  }

  // 32-bit per pixel
  stride *= 4;

  for (line = 0; line < height; ++line) {
    int16_t *yc = (int16_t *)(y + line * srcStride);
    int16_t *uc = (int16_t *)(u + line * srcStride);
    int16_t *vc = (int16_t *)(v + line * srcStride);
    int32_t *idst = (int32_t *)(pOut + (line * stride));
    for (i = 0; i < width; ++i) {
      int16_t yv = yc[i];
      int16_t uv = uc[i];
      int16_t vv = vc[i];
      *idst++ = (uv & 0x3FF) + ((yv & 0x3FF) << 10) + ((vv & 0x3FF) << 20) + (3 << 30);
    }
  }

  return S_OK;
}
