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
  { 3, 1, { 1 },       { 1 }       },       ///< LAVPixFmt_RGB24
  { 4, 1, { 1 },       { 1 }       },       ///< LAVPixFmt_RGB32
  { 4, 1, { 1 },       { 1 }       },       ///< LAVPixFmt_ARGB32
};

LAVPixFmtDesc getPixelFormatDesc(LAVPixelFormat pixFmt)
{
  return lav_pixfmt_desc[pixFmt];
}

static struct {
  LAVPixelFormat pixfmt;
  PixelFormat ffpixfmt;
} lav_ff_pixfmt_map[] = {
  { LAVPixFmt_YUV420, PIX_FMT_YUV420P },
  { LAVPixFmt_YUV422, PIX_FMT_YUV422P },
  { LAVPixFmt_YUV444, PIX_FMT_YUV444P },
  { LAVPixFmt_NV12,   PIX_FMT_NV12    },
  { LAVPixFmt_YUY2,   PIX_FMT_YUYV422 },
  { LAVPixFmt_RGB24,  PIX_FMT_BGR24   },
  { LAVPixFmt_RGB32,  PIX_FMT_BGRA    },
  { LAVPixFmt_ARGB32, PIX_FMT_BGRA    },
};

PixelFormat getFFPixelFormatFromLAV(LAVPixelFormat pixFmt, int bpp)
{
  PixelFormat fmt = PIX_FMT_NONE;
  for(int i = 0; i < countof(lav_ff_pixfmt_map); i++) {
    if (lav_ff_pixfmt_map[i].pixfmt == pixFmt) {
      fmt = lav_ff_pixfmt_map[i].ffpixfmt;
      break;
    }
  }
  if (fmt == PIX_FMT_NONE) {
    switch(pixFmt) {
    case LAVPixFmt_YUV420bX:
      fmt = (bpp == 9) ? PIX_FMT_YUV420P9LE : ((bpp == 10) ? PIX_FMT_YUV420P10LE : PIX_FMT_YUV420P16LE);
      break;
    case LAVPixFmt_YUV422bX:
      fmt = (bpp == 10) ? PIX_FMT_YUV422P10LE : PIX_FMT_YUV422P16LE;
      break;
    case LAVPixFmt_YUV444bX:
      fmt = (bpp == 9) ? PIX_FMT_YUV444P9LE : ((bpp == 10) ? PIX_FMT_YUV444P10LE : PIX_FMT_YUV444P16LE);
      break;
    default:
      ASSERT(0);
    }
  }
  return fmt;
}

HRESULT AllocLAVFrameBuffers(LAVFrame *pFrame, int stride)
{
  LAVPixFmtDesc desc = getPixelFormatDesc(pFrame->format);

  if (stride < pFrame->width) {
    stride = FFALIGN(pFrame->width, 64);
  }

  stride *= desc.codedbytes;

  size_t basesize = stride * pFrame->height;
  for (int plane = 0; plane < desc.planes; plane++) {
    size_t size = (basesize  / desc.planeHeight[plane]) / desc.planeWidth[plane];
    pFrame->data[plane]   = (BYTE *)av_malloc(size);
    pFrame->stride[plane] =  stride / desc.planeWidth[plane];
  }

  return S_OK;
}
