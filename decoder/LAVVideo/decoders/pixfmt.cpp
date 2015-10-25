/*
 *      Copyright (C) 2010-2015 Hendrik Leppkes
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
#include "ILAVDecoder.h"

static LAVPixFmtDesc lav_pixfmt_desc[] = {
  { 1, 3, { 1, 2, 2 }, { 1, 2, 2 } },       ///< LAVPixFmt_YUV420
  { 2, 3, { 1, 2, 2 }, { 1, 2, 2 } },       ///< LAVPixFmt_YUV420bX
  { 1, 3, { 1, 2, 2 }, { 1, 1, 1 } },       ///< LAVPixFmt_YUV422
  { 2, 3, { 1, 2, 2 }, { 1, 1, 1 } },       ///< LAVPixFmt_YUV422bX
  { 1, 3, { 1, 1, 1 }, { 1, 1, 1 } },       ///< LAVPixFmt_YUV444
  { 2, 3, { 1, 1, 1 }, { 1, 1, 1 } },       ///< LAVPixFmt_YUV444bX
  { 1, 2, { 1, 1 },    { 1, 2 }    },       ///< LAVPixFmt_NV12
  { 2, 1, { 1 },       { 1 }       },       ///< LAVPixFmt_YUY2
  { 2, 2, { 1, 1 },    { 1, 2 }    },       ///< LAVPixFmt_P010
  { 3, 1, { 1 },       { 1 }       },       ///< LAVPixFmt_RGB24
  { 4, 1, { 1 },       { 1 }       },       ///< LAVPixFmt_RGB32
  { 4, 1, { 1 },       { 1 }       },       ///< LAVPixFmt_ARGB32
  { 6, 1, { 1 },       { 1 }       },       ///< LAVPixFmt_RGB48
};

LAVPixFmtDesc getPixelFormatDesc(LAVPixelFormat pixFmt)
{
  return lav_pixfmt_desc[pixFmt];
}

static struct {
  LAVPixelFormat pixfmt;
  AVPixelFormat  ffpixfmt;
} lav_ff_pixfmt_map[] = {
  { LAVPixFmt_YUV420, AV_PIX_FMT_YUV420P },
  { LAVPixFmt_YUV422, AV_PIX_FMT_YUV422P },
  { LAVPixFmt_YUV444, AV_PIX_FMT_YUV444P },
  { LAVPixFmt_NV12,   AV_PIX_FMT_NV12    },
  { LAVPixFmt_YUY2,   AV_PIX_FMT_YUYV422 },
  { LAVPixFmt_P010,   AV_PIX_FMT_P010    },
  { LAVPixFmt_RGB24,  AV_PIX_FMT_BGR24   },
  { LAVPixFmt_RGB32,  AV_PIX_FMT_BGRA    },
  { LAVPixFmt_ARGB32, AV_PIX_FMT_BGRA    },
  { LAVPixFmt_RGB48,  AV_PIX_FMT_RGB48LE },
};

AVPixelFormat getFFPixelFormatFromLAV(LAVPixelFormat pixFmt, int bpp)
{
  AVPixelFormat fmt = AV_PIX_FMT_NONE;
  for(int i = 0; i < countof(lav_ff_pixfmt_map); i++) {
    if (lav_ff_pixfmt_map[i].pixfmt == pixFmt) {
      fmt = lav_ff_pixfmt_map[i].ffpixfmt;
      break;
    }
  }
  if (fmt == AV_PIX_FMT_NONE) {
    switch(pixFmt) {
    case LAVPixFmt_YUV420bX:
      fmt = (bpp == 9) ? AV_PIX_FMT_YUV420P9LE : ((bpp == 10) ? AV_PIX_FMT_YUV420P10LE : ((bpp == 12) ? AV_PIX_FMT_YUV420P12LE : ((bpp == 14) ? AV_PIX_FMT_YUV420P14LE : AV_PIX_FMT_YUV420P16LE)));
      break;
    case LAVPixFmt_YUV422bX:
      fmt = (bpp == 9) ? AV_PIX_FMT_YUV422P9LE : ((bpp == 10) ? AV_PIX_FMT_YUV422P10LE : ((bpp == 12) ? AV_PIX_FMT_YUV422P12LE : ((bpp == 14) ? AV_PIX_FMT_YUV422P14LE : AV_PIX_FMT_YUV422P16LE)));
      break;
    case LAVPixFmt_YUV444bX:
      fmt = (bpp == 9) ? AV_PIX_FMT_YUV444P9LE : ((bpp == 10) ? AV_PIX_FMT_YUV444P10LE : ((bpp == 12) ? AV_PIX_FMT_YUV444P12LE : ((bpp == 14) ? AV_PIX_FMT_YUV444P14LE : AV_PIX_FMT_YUV444P16LE)));
      break;
    default:
      ASSERT(0);
    }
  }
  return fmt;
}

static void free_buffers(struct LAVFrame *pFrame)
{
  _aligned_free(pFrame->data[0]);
  _aligned_free(pFrame->data[1]);
  _aligned_free(pFrame->data[2]);
  _aligned_free(pFrame->data[3]);
  memset(pFrame->data, 0, sizeof(pFrame->data));
}

HRESULT AllocLAVFrameBuffers(LAVFrame *pFrame, ptrdiff_t stride)
{
  LAVPixFmtDesc desc = getPixelFormatDesc(pFrame->format);

  if (stride < pFrame->width) {
    // Ensure alignment of at least 32 on all planes
    stride = FFALIGN(pFrame->width, 64);
  }

  stride *= desc.codedbytes;

  int alignedHeight = FFALIGN(pFrame->height, 2);

  memset(pFrame->data, 0, sizeof(pFrame->data));
  memset(pFrame->stride, 0, sizeof(pFrame->stride));
  for (int plane = 0; plane < desc.planes; plane++) {
    ptrdiff_t planeStride = stride / desc.planeWidth[plane];
    size_t size = planeStride * (alignedHeight / desc.planeHeight[plane]);
    pFrame->data[plane]   = (BYTE *)_aligned_malloc(size + FF_INPUT_BUFFER_PADDING_SIZE, 64);
    pFrame->stride[plane] = planeStride;
  }

  pFrame->destruct = &free_buffers;
  pFrame->flags   |= LAV_FRAME_FLAG_BUFFER_MODIFY;

  return S_OK;
}

HRESULT FreeLAVFrameBuffers(LAVFrame *pFrame)
{
  CheckPointer(pFrame, E_POINTER);

  if (pFrame->destruct) {
    pFrame->destruct(pFrame);
    pFrame->destruct = nullptr;
    pFrame->priv_data = nullptr;
  }
  memset(pFrame->data, 0, sizeof(pFrame->data));
  memset(pFrame->stride, 0, sizeof(pFrame->stride));
  return S_OK;
}

HRESULT CopyLAVFrame(LAVFrame *pSrc, LAVFrame **ppDst)
{
  ASSERT(pSrc->format != LAVPixFmt_DXVA2);
  *ppDst = (LAVFrame *)CoTaskMemAlloc(sizeof(LAVFrame));
  if (!*ppDst) return E_OUTOFMEMORY;
  **ppDst = *pSrc;

  (*ppDst)->destruct  = nullptr;
  (*ppDst)->priv_data = nullptr;

  AllocLAVFrameBuffers(*ppDst);

  LAVPixFmtDesc desc = getPixelFormatDesc(pSrc->format);
  for (int plane = 0; plane < desc.planes; plane++) {
    size_t linesize = (pSrc->width / desc.planeWidth[plane]) * desc.codedbytes;
    BYTE *dst = (*ppDst)->data[plane];
    BYTE *src = pSrc->data[plane];
    if (!dst || !src)
      return E_FAIL;
    for (int i = 0; i < (pSrc->height / desc.planeHeight[plane]); i++) {
      memcpy(dst, src, linesize);
      dst += (*ppDst)->stride[plane];
      src += pSrc->stride[plane];
    }
  }

  return S_OK;
}

HRESULT CopyLAVFrameInPlace(LAVFrame *pFrame)
{
  LAVFrame *tmpFrame = nullptr;
  CopyLAVFrame(pFrame, &tmpFrame);
  FreeLAVFrameBuffers(pFrame);
  *pFrame = *tmpFrame;
  SAFE_CO_FREE(tmpFrame);
  return S_OK;
}
