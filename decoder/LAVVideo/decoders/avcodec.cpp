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
#include "avcodec.h"

#include "moreuuids.h"
#include "parsers/MPEG2HeaderParser.h"
#include "parsers/H264SequenceParser.h"
#include "parsers/VC1HeaderParser.h"

#include "Media.h"

#ifdef DEBUG
#include "lavf_log.h"
#endif

extern "C" {
#include "libavutil/pixdesc.h"
};

////////////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////////////

ILAVDecoder *CreateDecoderAVCodec() {
  return new CDecAvcodec();
}

////////////////////////////////////////////////////////////////////////////////
// Create DXVA2 Extended Flags from a AVFrame and AVCodecContext
////////////////////////////////////////////////////////////////////////////////

static DXVA2_ExtendedFormat GetDXVA2ExtendedFlags(AVCodecContext *ctx, AVFrame *frame)
{
  DXVA2_ExtendedFormat fmt;
  ZeroMemory(&fmt, sizeof(fmt));

  fillDXVAExtFormat(fmt, -1, ctx->color_primaries, ctx->colorspace, ctx->color_trc);

  if (frame->format == AV_PIX_FMT_XYZ12LE || frame->format == AV_PIX_FMT_XYZ12BE)
    fmt.VideoPrimaries = DXVA2_VideoPrimaries_BT709;

  // Chroma location
  switch(ctx->chroma_sample_location) {
  case AVCHROMA_LOC_LEFT:
    fmt.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG2;
    break;
  case AVCHROMA_LOC_CENTER:
    fmt.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG1;
    break;
  case AVCHROMA_LOC_TOPLEFT:
    fmt.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_DV_PAL;
    break;
  }

  // Color Range, 0-255 or 16-235
  BOOL ffFullRange = (ctx->color_range == AVCOL_RANGE_JPEG)
                     || frame->format == AV_PIX_FMT_YUVJ420P || frame->format == AV_PIX_FMT_YUVJ422P || frame->format == AV_PIX_FMT_YUVJ444P
                     || frame->format == AV_PIX_FMT_YUVJ440P || frame->format == AV_PIX_FMT_YUVJ411P;
  fmt.NominalRange = ffFullRange ? DXVA2_NominalRange_0_255 : (ctx->color_range == AVCOL_RANGE_MPEG) ? DXVA2_NominalRange_16_235 : DXVA2_NominalRange_Unknown;

  return fmt;
}

////////////////////////////////////////////////////////////////////////////////
// avcodec -> LAV codec mappings
////////////////////////////////////////////////////////////////////////////////

// This mapping table should contain all pixel formats, except hardware formats (VDPAU, XVMC, DXVA, etc)
// A format that is not listed will be converted to YUV420
static struct PixelFormatMapping {
  AVPixelFormat  ffpixfmt;
  LAVPixelFormat lavpixfmt;
  BOOL           conversion;
  int            bpp;
} ff_pix_map[] = {
  { AV_PIX_FMT_YUV420P,   LAVPixFmt_YUV420, FALSE },
  { AV_PIX_FMT_YUYV422,   LAVPixFmt_YUY2,   FALSE },
  { AV_PIX_FMT_RGB24,     LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_BGR24,     LAVPixFmt_RGB24,  FALSE },
  { AV_PIX_FMT_YUV422P,   LAVPixFmt_YUV422, FALSE },
  { AV_PIX_FMT_YUV444P,   LAVPixFmt_YUV444, FALSE },
  { AV_PIX_FMT_YUV410P,   LAVPixFmt_YUV420, TRUE  },
  { AV_PIX_FMT_YUV411P,   LAVPixFmt_YUV422, TRUE  },
  { AV_PIX_FMT_GRAY8,     LAVPixFmt_YUV420, TRUE  },
  { AV_PIX_FMT_MONOWHITE, LAVPixFmt_YUV420, TRUE  },
  { AV_PIX_FMT_MONOBLACK, LAVPixFmt_YUV420, TRUE  },
  { AV_PIX_FMT_PAL8,      LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_YUVJ420P,  LAVPixFmt_YUV420, FALSE },
  { AV_PIX_FMT_YUVJ422P,  LAVPixFmt_YUV422, FALSE },
  { AV_PIX_FMT_YUVJ444P,  LAVPixFmt_YUV444, FALSE },
  { AV_PIX_FMT_UYVY422,   LAVPixFmt_YUV422, TRUE  },
  { AV_PIX_FMT_UYYVYY411, LAVPixFmt_YUV422, TRUE  },
  { AV_PIX_FMT_BGR8,      LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_BGR4,      LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_BGR4_BYTE, LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_RGB8,      LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_RGB4,      LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_RGB4_BYTE, LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_NV12,      LAVPixFmt_NV12,   FALSE },
  { AV_PIX_FMT_NV21,      LAVPixFmt_NV12,   TRUE  },

  { AV_PIX_FMT_ARGB,      LAVPixFmt_ARGB32, TRUE  },
  { AV_PIX_FMT_RGBA,      LAVPixFmt_ARGB32, TRUE  },
  { AV_PIX_FMT_ABGR,      LAVPixFmt_ARGB32, TRUE  },
  { AV_PIX_FMT_BGRA,      LAVPixFmt_ARGB32, FALSE },

  { AV_PIX_FMT_GRAY16BE,  LAVPixFmt_YUV420, TRUE  },
  { AV_PIX_FMT_GRAY16LE,  LAVPixFmt_YUV420, TRUE  },
  { AV_PIX_FMT_YUV440P,   LAVPixFmt_YUV444, TRUE  },
  { AV_PIX_FMT_YUVJ440P,  LAVPixFmt_YUV444, TRUE  },
  { AV_PIX_FMT_YUVA420P,  LAVPixFmt_YUV420, TRUE  },
  { AV_PIX_FMT_RGB48BE,   LAVPixFmt_RGB48,  TRUE  },
  { AV_PIX_FMT_RGB48LE,   LAVPixFmt_RGB48,  FALSE },

  { AV_PIX_FMT_RGB565BE,  LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_RGB565LE,  LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_RGB555BE,  LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_RGB555LE,  LAVPixFmt_RGB32,  TRUE  },

  { AV_PIX_FMT_BGR565BE,  LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_BGR565LE,  LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_BGR555BE,  LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_BGR555LE,  LAVPixFmt_RGB32,  TRUE  },

  { AV_PIX_FMT_YUV420P16LE, LAVPixFmt_YUV420bX, FALSE, 16 },
  { AV_PIX_FMT_YUV420P16BE, LAVPixFmt_YUV420bX, TRUE,  16 },
  { AV_PIX_FMT_YUV422P16LE, LAVPixFmt_YUV422bX, FALSE, 16 },
  { AV_PIX_FMT_YUV422P16BE, LAVPixFmt_YUV422bX, TRUE,  16 },
  { AV_PIX_FMT_YUV444P16LE, LAVPixFmt_YUV444bX, FALSE, 16 },
  { AV_PIX_FMT_YUV444P16BE, LAVPixFmt_YUV444bX, TRUE,  16 },

  { AV_PIX_FMT_RGB444LE,  LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_RGB444BE,  LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_BGR444LE,  LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_BGR444BE,  LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_YA8,       LAVPixFmt_YUV420, TRUE },
  { AV_PIX_FMT_BGR48BE,   LAVPixFmt_RGB48,  TRUE  },
  { AV_PIX_FMT_BGR48LE,   LAVPixFmt_RGB48,  TRUE },

  { AV_PIX_FMT_YUV420P9BE,  LAVPixFmt_YUV420bX, TRUE,  9 },
  { AV_PIX_FMT_YUV420P9LE,  LAVPixFmt_YUV420bX, FALSE, 9 },
  { AV_PIX_FMT_YUV420P10BE, LAVPixFmt_YUV420bX, TRUE,  10 },
  { AV_PIX_FMT_YUV420P10LE, LAVPixFmt_YUV420bX, FALSE, 10 },
  { AV_PIX_FMT_YUV422P10BE, LAVPixFmt_YUV422bX, TRUE,  10 },
  { AV_PIX_FMT_YUV422P10LE, LAVPixFmt_YUV422bX, FALSE, 10 },
  { AV_PIX_FMT_YUV444P9BE,  LAVPixFmt_YUV444bX, TRUE,  9 },
  { AV_PIX_FMT_YUV444P9LE,  LAVPixFmt_YUV444bX, FALSE, 9 },
  { AV_PIX_FMT_YUV444P10BE, LAVPixFmt_YUV444bX, TRUE,  10 },
  { AV_PIX_FMT_YUV444P10LE, LAVPixFmt_YUV444bX, FALSE, 10 },
  { AV_PIX_FMT_YUV422P9BE,  LAVPixFmt_YUV422bX, TRUE,  9 },
  { AV_PIX_FMT_YUV422P9LE,  LAVPixFmt_YUV422bX, FALSE, 9 },

  { AV_PIX_FMT_GBRP,      LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_GBRP9BE,   LAVPixFmt_RGB48,  TRUE  },
  { AV_PIX_FMT_GBRP9LE,   LAVPixFmt_RGB48,  TRUE  },
  { AV_PIX_FMT_GBRP10BE,  LAVPixFmt_RGB48,  TRUE  },
  { AV_PIX_FMT_GBRP10LE,  LAVPixFmt_RGB48,  TRUE  },
  { AV_PIX_FMT_GBRP16BE,  LAVPixFmt_RGB48,  TRUE  },
  { AV_PIX_FMT_GBRP16LE,  LAVPixFmt_RGB48,  TRUE  },

  { AV_PIX_FMT_YUVA422P,     LAVPixFmt_YUV422,   TRUE },
  { AV_PIX_FMT_YUVA444P,     LAVPixFmt_YUV444,   TRUE },
  { AV_PIX_FMT_YUVA420P9BE,  LAVPixFmt_YUV420bX, TRUE,  9 },
  { AV_PIX_FMT_YUVA420P9LE,  LAVPixFmt_YUV420bX, FALSE, 9 },
  { AV_PIX_FMT_YUVA422P9BE,  LAVPixFmt_YUV422bX, TRUE,  9 },
  { AV_PIX_FMT_YUVA422P9LE,  LAVPixFmt_YUV422bX, FALSE, 9 },
  { AV_PIX_FMT_YUVA444P9BE,  LAVPixFmt_YUV444bX, TRUE,  9 },
  { AV_PIX_FMT_YUVA444P9LE,  LAVPixFmt_YUV444bX, FALSE, 9 },
  { AV_PIX_FMT_YUVA420P10BE, LAVPixFmt_YUV420bX, TRUE,  10 },
  { AV_PIX_FMT_YUVA420P10LE, LAVPixFmt_YUV420bX, FALSE, 10 },
  { AV_PIX_FMT_YUVA422P10BE, LAVPixFmt_YUV422bX, TRUE,  10 },
  { AV_PIX_FMT_YUVA422P10LE, LAVPixFmt_YUV422bX, FALSE, 10 },
  { AV_PIX_FMT_YUVA444P10BE, LAVPixFmt_YUV444bX, TRUE,  10 },
  { AV_PIX_FMT_YUVA444P10LE, LAVPixFmt_YUV444bX, FALSE, 10 },
  { AV_PIX_FMT_YUVA420P16BE, LAVPixFmt_YUV420bX, TRUE,  16 },
  { AV_PIX_FMT_YUVA420P16LE, LAVPixFmt_YUV420bX, FALSE, 16 },
  { AV_PIX_FMT_YUVA422P16BE, LAVPixFmt_YUV422bX, TRUE,  16 },
  { AV_PIX_FMT_YUVA422P16LE, LAVPixFmt_YUV422bX, FALSE, 16 },
  { AV_PIX_FMT_YUVA444P16BE, LAVPixFmt_YUV444bX, TRUE,  16 },
  { AV_PIX_FMT_YUVA444P16LE, LAVPixFmt_YUV444bX, FALSE, 16 },

  { AV_PIX_FMT_XYZ12LE, LAVPixFmt_RGB48,  TRUE },
  { AV_PIX_FMT_XYZ12BE, LAVPixFmt_RGB48,  TRUE },
  { AV_PIX_FMT_NV16,    LAVPixFmt_YUV422, TRUE },
  { AV_PIX_FMT_NV20LE,  LAVPixFmt_YUV422bX, TRUE, 10 },
  { AV_PIX_FMT_NV20BE,  LAVPixFmt_YUV422bX, TRUE, 10 },

  { AV_PIX_FMT_RGBA64BE,  LAVPixFmt_RGB48,  TRUE  },
  { AV_PIX_FMT_RGBA64LE,  LAVPixFmt_RGB48,  TRUE  },
  { AV_PIX_FMT_BGRA64BE,  LAVPixFmt_RGB48,  TRUE  },
  { AV_PIX_FMT_BGRA64LE,  LAVPixFmt_RGB48,  TRUE  },

  { AV_PIX_FMT_NV16,      LAVPixFmt_YUV422,   TRUE },
  { AV_PIX_FMT_YA16BE,    LAVPixFmt_YUV420bX, TRUE, 16 },
  { AV_PIX_FMT_YA16LE,    LAVPixFmt_YUV420bX, TRUE, 16 },

  { AV_PIX_FMT_GBRAP,     LAVPixFmt_ARGB32, TRUE },
  { AV_PIX_FMT_GBRAP16BE, LAVPixFmt_RGB48,  TRUE },
  { AV_PIX_FMT_GBRAP16LE, LAVPixFmt_RGB48,  TRUE },

  { AV_PIX_FMT_0RGB,      LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_RGB0,      LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_0BGR,      LAVPixFmt_RGB32,  TRUE  },
  { AV_PIX_FMT_BGR0,      LAVPixFmt_RGB32,  FALSE },

  { AV_PIX_FMT_YUV420P12BE, LAVPixFmt_YUV420bX, TRUE,  12 },
  { AV_PIX_FMT_YUV420P12LE, LAVPixFmt_YUV420bX, FALSE, 12 },
  { AV_PIX_FMT_YUV420P14BE, LAVPixFmt_YUV420bX, TRUE,  14 },
  { AV_PIX_FMT_YUV420P14LE, LAVPixFmt_YUV420bX, FALSE, 14 },
  { AV_PIX_FMT_YUV422P12BE, LAVPixFmt_YUV422bX, TRUE,  12 },
  { AV_PIX_FMT_YUV422P12LE, LAVPixFmt_YUV422bX, FALSE, 12 },
  { AV_PIX_FMT_YUV422P14BE, LAVPixFmt_YUV422bX, TRUE,  14 },
  { AV_PIX_FMT_YUV422P14LE, LAVPixFmt_YUV422bX, FALSE, 14 },
  { AV_PIX_FMT_YUV444P12BE, LAVPixFmt_YUV444bX, TRUE,  12 },
  { AV_PIX_FMT_YUV444P12LE, LAVPixFmt_YUV444bX, FALSE, 12 },
  { AV_PIX_FMT_YUV444P14BE, LAVPixFmt_YUV444bX, TRUE,  14 },
  { AV_PIX_FMT_YUV444P14LE, LAVPixFmt_YUV444bX, FALSE, 14 },
  { AV_PIX_FMT_GBRP12BE,  LAVPixFmt_RGB48,  TRUE },
  { AV_PIX_FMT_GBRP12LE,  LAVPixFmt_RGB48,  TRUE },
  { AV_PIX_FMT_GBRP14BE,  LAVPixFmt_RGB48,  TRUE },
  { AV_PIX_FMT_GBRP14LE,  LAVPixFmt_RGB48,  TRUE },
  { AV_PIX_FMT_YUVJ411P,  LAVPixFmt_YUV422, TRUE },

  { AV_PIX_FMT_YUV440P10LE,  LAVPixFmt_YUV444bX, TRUE, 10 },
  { AV_PIX_FMT_YUV440P10BE,  LAVPixFmt_YUV444bX, TRUE, 10 },
  { AV_PIX_FMT_YUV440P12LE,  LAVPixFmt_YUV444bX, TRUE, 12 },
  { AV_PIX_FMT_YUV440P12BE,  LAVPixFmt_YUV444bX, TRUE, 12 },

  { AV_PIX_FMT_P010LE, LAVPixFmt_P010, FALSE, 10 },

  { AV_PIX_FMT_DXVA2_VLD, LAVPixFmt_DXVA2, FALSE },
};

static AVCodecID ff_interlace_capable[] = {
  AV_CODEC_ID_DNXHD,
  AV_CODEC_ID_DVVIDEO,
  AV_CODEC_ID_FRWU,
  AV_CODEC_ID_MJPEG,
  AV_CODEC_ID_MPEG4,
  AV_CODEC_ID_MPEG2VIDEO,
  AV_CODEC_ID_H264,
  AV_CODEC_ID_VC1,
  AV_CODEC_ID_PNG,
  AV_CODEC_ID_PRORES,
  AV_CODEC_ID_RAWVIDEO,
  AV_CODEC_ID_UTVIDEO
};

static struct PixelFormatMapping getPixFmtMapping(AVPixelFormat pixfmt) {
  const PixelFormatMapping def = { pixfmt, LAVPixFmt_YUV420, TRUE, 8 };
  PixelFormatMapping result = def;
  for (int i = 0; i < countof(ff_pix_map); i++) {
    if (ff_pix_map[i].ffpixfmt == pixfmt) {
      result = ff_pix_map[i];
      break;
    }
  }
  if (result.lavpixfmt != LAVPixFmt_YUV420bX && result.lavpixfmt != LAVPixFmt_YUV422bX && result.lavpixfmt != LAVPixFmt_YUV444bX)
    result.bpp = 8;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
// AVCodec decoder implementation
////////////////////////////////////////////////////////////////////////////////

CDecAvcodec::CDecAvcodec(void)
  : CDecBase()
{
}

CDecAvcodec::~CDecAvcodec(void)
{
  DestroyDecoder();
}

// ILAVDecoder
STDMETHODIMP CDecAvcodec::Init()
{
#ifdef DEBUG
  DbgSetModuleLevel (LOG_CUSTOM1, DWORD_MAX); // FFMPEG messages use custom1
  av_log_set_callback(lavf_log_callback);
#else
  av_log_set_callback(nullptr);
#endif

  avcodec_register_all();
  return S_OK;
}

STDMETHODIMP CDecAvcodec::InitDecoder(AVCodecID codec, const CMediaType *pmt)
{
  DestroyDecoder();
  DbgLog((LOG_TRACE, 10, L"Initializing ffmpeg for codec %S", avcodec_get_name(codec)));

  BITMAPINFOHEADER *pBMI = nullptr;
  videoFormatTypeHandler((const BYTE *)pmt->Format(), pmt->FormatType(), &pBMI);

  m_pAVCodec = avcodec_find_decoder(codec);
  CheckPointer(m_pAVCodec, VFW_E_UNSUPPORTED_VIDEO);

  m_pAVCtx = avcodec_alloc_context3(m_pAVCodec);
  CheckPointer(m_pAVCtx, E_POINTER);

  if(    codec == AV_CODEC_ID_MPEG1VIDEO
      || codec == AV_CODEC_ID_MPEG2VIDEO
      || pmt->subtype == MEDIASUBTYPE_H264
      || pmt->subtype == MEDIASUBTYPE_h264
      || pmt->subtype == MEDIASUBTYPE_X264
      || pmt->subtype == MEDIASUBTYPE_x264
      || pmt->subtype == MEDIASUBTYPE_H264_bis
      || pmt->subtype == MEDIASUBTYPE_HEVC) {
    m_pParser = av_parser_init(codec);
  }

  DWORD dwDecFlags = m_pCallback->GetDecodeFlags();

  LONG biRealWidth = pBMI->biWidth, biRealHeight = pBMI->biHeight;
  if (pmt->formattype == FORMAT_VideoInfo || pmt->formattype == FORMAT_MPEGVideo) {
    VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)pmt->Format();
    if (vih->rcTarget.right != 0 && vih->rcTarget.bottom != 0) {
      biRealWidth  = vih->rcTarget.right;
      biRealHeight = vih->rcTarget.bottom;
    }
  } else if (pmt->formattype == FORMAT_VideoInfo2 || pmt->formattype == FORMAT_MPEG2Video) {
    VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)pmt->Format();
    if (vih2->rcTarget.right != 0 && vih2->rcTarget.bottom != 0) {
      biRealWidth  = vih2->rcTarget.right;
      biRealHeight = vih2->rcTarget.bottom;
    }
  }

  m_pAVCtx->codec_id              = codec;
  m_pAVCtx->codec_tag             = pBMI->biCompression;
  m_pAVCtx->coded_width           = pBMI->biWidth;
  m_pAVCtx->coded_height          = abs(pBMI->biHeight);
  m_pAVCtx->bits_per_coded_sample = pBMI->biBitCount;
  m_pAVCtx->err_recognition       = 0;
  m_pAVCtx->workaround_bugs       = FF_BUG_AUTODETECT | FF_BUG_X264_LOSSLESS;
  m_pAVCtx->refcounted_frames     = 1;

  // Setup threading
  // Thread Count. 0 = auto detect
  int thread_count = m_pSettings->GetNumThreads();
  if (thread_count == 0) {
    thread_count = av_cpu_count() * 3 / 2;
  }
  m_pAVCtx->thread_count = max(1, min(thread_count, AVCODEC_MAX_THREADS));

  if (dwDecFlags & LAV_VIDEO_DEC_FLAG_NO_MT || codec == AV_CODEC_ID_MPEG4) {
    m_pAVCtx->thread_count = 1;
  }

  m_pFrame = av_frame_alloc();
  CheckPointer(m_pFrame, E_POINTER);

  // Process Extradata
  BYTE *extra = nullptr;
  size_t extralen = 0;
  getExtraData(*pmt, nullptr, &extralen);

  BOOL bH264avc = FALSE;
  if (pmt->formattype == FORMAT_MPEG2Video && (m_pAVCtx->codec_tag == MAKEFOURCC('a','v','c','1') || m_pAVCtx->codec_tag == MAKEFOURCC('A','V','C','1') || m_pAVCtx->codec_tag == MAKEFOURCC('C','C','V','1'))) {
    // Reconstruct AVC1 extradata format
    DbgLog((LOG_TRACE, 10, L"-> Processing AVC1 extradata of %d bytes", extralen));
    MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)pmt->Format();
    extralen += 7;
    extra = (uint8_t *)av_mallocz(extralen + FF_INPUT_BUFFER_PADDING_SIZE);
    extra[0] = 1;
    extra[1] = (BYTE)mp2vi->dwProfile;
    extra[2] = 0;
    extra[3] = (BYTE)mp2vi->dwLevel;
    extra[4] = (BYTE)(mp2vi->dwFlags ? mp2vi->dwFlags : 4) - 1;

    // only process extradata if available
    uint8_t ps_count = 0;
    if (extralen > 7) {
      // Actually copy the metadata into our new buffer
      size_t actual_len;
      getExtraData(*pmt, extra + 6, &actual_len);

      // Count the number of SPS/PPS in them and set the length
      // We'll put them all into one block and add a second block with 0 elements afterwards
      // The parsing logic does not care what type they are, it just expects 2 blocks.
      BYTE *p = extra + 6, *end = extra + 6 + actual_len;
      BOOL bSPS = FALSE, bPPS = FALSE;
      while (p + 1 < end) {
        unsigned len = (((unsigned)p[0] << 8) | p[1]) + 2;
        if (p + len > end) {
          break;
        }
        if ((p[2] & 0x1F) == 7)
          bSPS = TRUE;
        if ((p[2] & 0x1F) == 8)
          bPPS = TRUE;
        ps_count++;
        p += len;
      }
    }
    extra[5] = ps_count;
    extra[extralen - 1] = 0;

    bH264avc = TRUE;
    m_pAVCtx->extradata = extra;
    m_pAVCtx->extradata_size = (int)extralen;
  } else if (extralen > 0) {
    DbgLog((LOG_TRACE, 10, L"-> Processing extradata of %d bytes", extralen));
    if (pmt->subtype == MEDIASUBTYPE_LAV_RAWVIDEO) {
      if (extralen < sizeof(m_pAVCtx->pix_fmt)) {
        DbgLog((LOG_TRACE, 10, L"-> LAV RAW Video extradata is missing.."));
      } else {
        extra = (uint8_t *)av_mallocz(extralen + FF_INPUT_BUFFER_PADDING_SIZE);
        getExtraData(*pmt, extra, nullptr);
        m_pAVCtx->pix_fmt = *(AVPixelFormat *)extra;
        extralen -= sizeof(AVPixelFormat);
        memmove(extra, extra+sizeof(AVPixelFormat), extralen);
      }
    } else {
      // Just copy extradata for other formats
      extra = (uint8_t *)av_mallocz(extralen + FF_INPUT_BUFFER_PADDING_SIZE);
      getExtraData(*pmt, extra, nullptr);
    }
    // Hack to discard invalid MP4 metadata with AnnexB style video
    if (codec == AV_CODEC_ID_H264 && !bH264avc && extra && extra[0] == 1) {
      av_freep(&extra);
      extralen = 0;
    }
    m_pAVCtx->extradata = extra;
    m_pAVCtx->extradata_size = (int)extralen;
  } else {
    if (codec == AV_CODEC_ID_VP6 || codec == AV_CODEC_ID_VP6A || codec == AV_CODEC_ID_VP6F) {
      int cropH = pBMI->biWidth - biRealWidth;
      int cropV = pBMI->biHeight - biRealHeight;
      if (cropH >= 0 && cropH <= 0x0f && cropV >= 0 && cropV <= 0x0f) {
        m_pAVCtx->extradata = (uint8_t *)av_mallocz(1 + FF_INPUT_BUFFER_PADDING_SIZE);
        m_pAVCtx->extradata_size = 1;
        m_pAVCtx->extradata[0] = (cropH << 4) | cropV;
      }
    }
  }

  m_CurrentThread = 0;
  m_rtStartCache = AV_NOPTS_VALUE;

  LAVPinInfo lavPinInfo = {0};
  BOOL bLAVInfoValid = SUCCEEDED(m_pCallback->GetLAVPinInfo(lavPinInfo));

  m_bInputPadded = dwDecFlags & LAV_VIDEO_DEC_FLAG_LAVSPLITTER;

  // Setup codec-specific timing logic

  // MPEG-4 with VideoInfo/2 is from AVI, and only DTS
  if (codec == AV_CODEC_ID_MPEG4 && pmt->formattype != FORMAT_MPEG2Video)
    dwDecFlags |= LAV_VIDEO_DEC_FLAG_ONLY_DTS;

  // RealVideo is only DTS
  if (codec == AV_CODEC_ID_RV10 || codec == AV_CODEC_ID_RV20 || codec == AV_CODEC_ID_RV30 || codec == AV_CODEC_ID_RV40)
    dwDecFlags |= LAV_VIDEO_DEC_FLAG_ONLY_DTS;

  // H.264 without B frames should use reordering for proper delay handling
  if (codec == AV_CODEC_ID_H264 && bLAVInfoValid && lavPinInfo.has_b_frames == 0) {
    dwDecFlags &= ~LAV_VIDEO_DEC_FLAG_ONLY_DTS;
  }

  // Use ffmpegs logic to reorder timestamps
  // This is required for H264 content (except AVI), and generally all codecs that use frame threading
  m_bFFReordering        = !(dwDecFlags & LAV_VIDEO_DEC_FLAG_ONLY_DTS) && (
                            (m_pAVCodec->capabilities & CODEC_CAP_FRAME_THREADS) // Covers most modern codecs, others listed above
                           || codec == AV_CODEC_ID_MPEG2VIDEO
                           || codec == AV_CODEC_ID_MPEG1VIDEO
                           || codec == AV_CODEC_ID_DIRAC
                           || codec == AV_CODEC_ID_VC1
                          );

  // Stop time is unreliable, drop it and calculate it
  m_bCalculateStopTime   = (codec == AV_CODEC_ID_H264 || codec == AV_CODEC_ID_DIRAC || (codec == AV_CODEC_ID_MPEG4 && pmt->formattype == FORMAT_MPEG2Video) || (codec == AV_CODEC_ID_VC1 && !(dwDecFlags & LAV_VIDEO_DEC_FLAG_ONLY_DTS)));

  // Real Video content has some odd timestamps
  // LAV Splitter does them allright with RV30/RV40, everything else screws them up
  m_bRVDropBFrameTimings = (codec == AV_CODEC_ID_RV10 || codec == AV_CODEC_ID_RV20 || ((codec == AV_CODEC_ID_RV30 || codec == AV_CODEC_ID_RV40) && (!(dwDecFlags & LAV_VIDEO_DEC_FLAG_LAVSPLITTER) || (bLAVInfoValid && (lavPinInfo.flags & LAV_STREAM_FLAG_RV34_MKV)))));

  // Enable B-Frame delay handling
  m_bBFrameDelay = !m_bFFReordering && !m_bRVDropBFrameTimings;

  m_bWaitingForKeyFrame = TRUE;
  m_bResumeAtKeyFrame =    codec == AV_CODEC_ID_MPEG2VIDEO
                        || codec == AV_CODEC_ID_VC1
                        || codec == AV_CODEC_ID_VC1IMAGE
                        || codec == AV_CODEC_ID_WMV3
                        || codec == AV_CODEC_ID_WMV3IMAGE
                        || codec == AV_CODEC_ID_RV30
                        || codec == AV_CODEC_ID_RV40
                        || codec == AV_CODEC_ID_VP3
                        || codec == AV_CODEC_ID_THEORA
                        || codec == AV_CODEC_ID_MPEG4;

  m_bHasPalette = m_pAVCtx->bits_per_coded_sample <= 8 && m_pAVCtx->extradata_size && !(dwDecFlags & LAV_VIDEO_DEC_FLAG_LAVSPLITTER)
                  &&  (codec == AV_CODEC_ID_MSVIDEO1
                    || codec == AV_CODEC_ID_MSRLE
                    || codec == AV_CODEC_ID_CINEPAK
                    || codec == AV_CODEC_ID_8BPS
                    || codec == AV_CODEC_ID_QPEG
                    || codec == AV_CODEC_ID_QTRLE
                    || codec == AV_CODEC_ID_TSCC);

  if (FAILED(AdditionaDecoderInit())) {
    return E_FAIL;
  }

  if (bLAVInfoValid) {
    // Setting has_b_frames to a proper value will ensure smoother decoding of H264
    if (lavPinInfo.has_b_frames >= 0) {
      DbgLog((LOG_TRACE, 10, L"-> Setting has_b_frames to %d", lavPinInfo.has_b_frames));
      m_pAVCtx->has_b_frames = lavPinInfo.has_b_frames;
    }
  }

  // Open the decoder
  m_bInInit = TRUE;
  int ret = avcodec_open2(m_pAVCtx, m_pAVCodec, nullptr);
  m_bInInit = FALSE;
  if (ret >= 0) {
    DbgLog((LOG_TRACE, 10, L"-> ffmpeg codec opened successfully (ret: %d)", ret));
    m_nCodecId = codec;
  } else {
    DbgLog((LOG_TRACE, 10, L"-> ffmpeg codec failed to open (ret: %d)", ret));
    DestroyDecoder();
    return VFW_E_UNSUPPORTED_VIDEO;
  }

  m_iInterlaced = 0;
  for (int i = 0; i < countof(ff_interlace_capable); i++) {
    if (codec == ff_interlace_capable[i]) {
      m_iInterlaced = -1;
      break;
    }
  }

  // Detect chroma and interlaced
  if (m_pAVCtx->extradata && m_pAVCtx->extradata_size) {
    if (codec == AV_CODEC_ID_MPEG2VIDEO) {
      CMPEG2HeaderParser mpeg2Parser(extra, extralen);
      if (mpeg2Parser.hdr.valid) {
        if (mpeg2Parser.hdr.chroma < 2) {
          m_pAVCtx->pix_fmt = AV_PIX_FMT_YUV420P;
        } else if (mpeg2Parser.hdr.chroma == 2) {
          m_pAVCtx->pix_fmt = AV_PIX_FMT_YUV422P;
        }
        m_iInterlaced = mpeg2Parser.hdr.interlaced;
      }
    } else if (codec == AV_CODEC_ID_H264) {
      CH264SequenceParser h264parser;
      if (bH264avc)
        h264parser.ParseNALs(extra+6, extralen-6, 2);
      else
        h264parser.ParseNALs(extra, extralen, 0);
      if (h264parser.sps.valid)
        m_iInterlaced = h264parser.sps.interlaced;
    } else if (codec == AV_CODEC_ID_VC1) {
      CVC1HeaderParser vc1parser(extra, extralen);
      if (vc1parser.hdr.valid)
        m_iInterlaced = (vc1parser.hdr.interlaced ? -1 : 0);
    }
  }

  if (codec == AV_CODEC_ID_DNXHD)
    m_pAVCtx->pix_fmt = AV_PIX_FMT_YUV422P10;
  else if (codec == AV_CODEC_ID_FRAPS)
    m_pAVCtx->pix_fmt = AV_PIX_FMT_BGR24;

  if (bLAVInfoValid && codec != AV_CODEC_ID_FRAPS && m_pAVCtx->pix_fmt != AV_PIX_FMT_DXVA2_VLD)
    m_pAVCtx->pix_fmt = lavPinInfo.pix_fmt;

  DbgLog((LOG_TRACE, 10, L"AVCodec init successfull. interlaced: %d", m_iInterlaced));

  return S_OK;
}

STDMETHODIMP CDecAvcodec::DestroyDecoder()
{
  DbgLog((LOG_TRACE, 10, L"Shutting down ffmpeg..."));
  m_pAVCodec	= nullptr;

  if (m_pParser) {
    av_parser_close(m_pParser);
    m_pParser = nullptr;
  }

  if (m_pAVCtx) {
    avcodec_close(m_pAVCtx);
    av_freep(&m_pAVCtx->hwaccel_context);
    av_freep(&m_pAVCtx->extradata);
    av_freep(&m_pAVCtx);
  }
  av_frame_free(&m_pFrame);

  av_freep(&m_pFFBuffer);
  m_nFFBufferSize = 0;

  av_freep(&m_pFFBuffer2);
  m_nFFBufferSize2 = 0;

  if (m_pSwsContext) {
    sws_freeContext(m_pSwsContext);
    m_pSwsContext = nullptr;
  }

  m_nCodecId = AV_CODEC_ID_NONE;

  return S_OK;
}

static void lav_avframe_free(LAVFrame *frame)
{
  ASSERT(frame->priv_data);
  av_frame_free((AVFrame **)&frame->priv_data);
}

STDMETHODIMP CDecAvcodec::Decode(const BYTE *buffer, int buflen, REFERENCE_TIME rtStartIn, REFERENCE_TIME rtStopIn, BOOL bSyncPoint, BOOL bDiscontinuity)
{
  CheckPointer(m_pAVCtx, E_UNEXPECTED);

  int     got_picture = 0;
  int     used_bytes  = 0;
  BOOL    bFlush = (buffer == nullptr);
  BOOL    bEndOfSequence = FALSE;

  AVPacket avpkt;
  av_init_packet(&avpkt);

  if (m_pAVCtx->active_thread_type & FF_THREAD_FRAME) {
    if (!m_bFFReordering) {
      m_tcThreadBuffer[m_CurrentThread].rtStart = rtStartIn;
      m_tcThreadBuffer[m_CurrentThread].rtStop  = rtStopIn;
    }

    m_CurrentThread = (m_CurrentThread + 1) % m_pAVCtx->thread_count;
  } else if (m_bBFrameDelay) {
    m_tcBFrameDelay[m_nBFramePos].rtStart = rtStartIn;
    m_tcBFrameDelay[m_nBFramePos].rtStop = rtStopIn;
    m_nBFramePos = !m_nBFramePos;
  }

  uint8_t *pDataBuffer = nullptr;
  if (!bFlush && buflen > 0) {
    if (!m_bInputPadded && (!(m_pAVCtx->active_thread_type & FF_THREAD_FRAME) || m_pParser)) {
      // Copy bitstream into temporary buffer to ensure overread protection
      // Verify buffer size
      if (buflen > m_nFFBufferSize) {
        m_nFFBufferSize	= buflen;
        m_pFFBuffer = (BYTE *)av_realloc_f(m_pFFBuffer, m_nFFBufferSize + FF_INPUT_BUFFER_PADDING_SIZE, 1);
        if (!m_pFFBuffer) {
          m_nFFBufferSize = 0;
          return E_OUTOFMEMORY;
        }
      }
      
      memcpy(m_pFFBuffer, buffer, buflen);
      memset(m_pFFBuffer+buflen, 0, FF_INPUT_BUFFER_PADDING_SIZE);
      pDataBuffer = m_pFFBuffer;
    } else {
      pDataBuffer = (uint8_t *)buffer;
    }

    if (m_nCodecId == AV_CODEC_ID_VP8 && m_bWaitingForKeyFrame) {
      if (!(pDataBuffer[0] & 1)) {
        DbgLog((LOG_TRACE, 10, L"::Decode(): Found VP8 key-frame, resuming decoding"));
        m_bWaitingForKeyFrame = FALSE;
      } else {
        return S_OK;
      }
    }
  }

  while (buflen > 0 || bFlush) {
    REFERENCE_TIME rtStart = rtStartIn, rtStop = rtStopIn;

    if (!bFlush) {
      avpkt.data = pDataBuffer;
      avpkt.size = buflen;
      avpkt.pts = rtStartIn;
      if (rtStartIn != AV_NOPTS_VALUE && rtStopIn != AV_NOPTS_VALUE)
        avpkt.duration = (int)(rtStopIn - rtStartIn);
      else
        avpkt.duration = 0;
      avpkt.flags = AV_PKT_FLAG_KEY;

      if (m_bHasPalette) {
        m_bHasPalette = FALSE;
        uint32_t *pal = (uint32_t *)av_packet_new_side_data(&avpkt, AV_PKT_DATA_PALETTE, AVPALETTE_SIZE);
        int pal_size = FFMIN((1 << m_pAVCtx->bits_per_coded_sample) << 2, m_pAVCtx->extradata_size);
        uint8_t *pal_src = m_pAVCtx->extradata + m_pAVCtx->extradata_size - pal_size;

        for (int i = 0; i < pal_size/4; i++)
          pal[i] = 0xFF<<24 | AV_RL32(pal_src+4*i);
      }
    } else {
      avpkt.data = nullptr;
      avpkt.size = 0;
    }

    // Parse the data if a parser is present
    // This is mandatory for MPEG-1/2
    if (m_pParser) {
      BYTE *pOut = nullptr;
      int pOut_size = 0;

      used_bytes = av_parser_parse2(m_pParser, m_pAVCtx, &pOut, &pOut_size, avpkt.data, avpkt.size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

      if (used_bytes == 0 && pOut_size == 0 && !bFlush) {
        DbgLog((LOG_TRACE, 50, L"::Decode() - could not process buffer, starving?"));
        break;
      } else if (used_bytes > 0) {
        buflen -= used_bytes;
        pDataBuffer += used_bytes;
      }

      // Update start time cache
      // If more data was read then output, update the cache (incomplete frame)
      // If output is bigger, a frame was completed, update the actual rtStart with the cached value, and then overwrite the cache
      if (used_bytes > pOut_size) {
        if (rtStartIn != AV_NOPTS_VALUE)
          m_rtStartCache = rtStartIn;
      } else if (used_bytes == pOut_size || ((used_bytes + 9) == pOut_size)) {
        // Why +9 above?
        // Well, apparently there are some broken MKV muxers that like to mux the MPEG-2 PICTURE_START_CODE block (which is 9 bytes) in the package with the previous frame
        // This would cause the frame timestamps to be delayed by one frame exactly, and cause timestamp reordering to go wrong.
        // So instead of failing on those samples, lets just assume that 9 bytes are that case exactly.
        m_rtStartCache = rtStartIn = AV_NOPTS_VALUE;
      } else if (pOut_size > used_bytes) {
        rtStart = m_rtStartCache;
        m_rtStartCache = rtStartIn;
        // The value was used once, don't use it for multiple frames, that ends up in weird timings
        rtStartIn = AV_NOPTS_VALUE;
      }

      if (pOut_size > 0 || bFlush) {

        if (pOut && pOut_size > 0) {
          if (pOut_size > m_nFFBufferSize2) {
            m_nFFBufferSize2	= pOut_size;
            m_pFFBuffer2 = (BYTE *)av_realloc_f(m_pFFBuffer2, m_nFFBufferSize2 + FF_INPUT_BUFFER_PADDING_SIZE, 1);
            if (!m_pFFBuffer2) {
              m_nFFBufferSize2 = 0;
              return E_OUTOFMEMORY;
            }
          }
          memcpy(m_pFFBuffer2, pOut, pOut_size);
          memset(m_pFFBuffer2+pOut_size, 0, FF_INPUT_BUFFER_PADDING_SIZE);

          avpkt.data = m_pFFBuffer2;
          avpkt.size = pOut_size;
          avpkt.pts = rtStart;
          avpkt.duration = 0;

          const uint8_t *eosmarker = CheckForEndOfSequence(m_nCodecId, avpkt.data, avpkt.size, &m_MpegParserState);
          if (eosmarker) {
            bEndOfSequence = TRUE;
          }
        } else {
          avpkt.data = nullptr;
          avpkt.size = 0;
        }

        int ret2 = avcodec_decode_video2 (m_pAVCtx, m_pFrame, &got_picture, &avpkt);
        if (ret2 < 0) {
          DbgLog((LOG_TRACE, 50, L"::Decode() - decoding failed despite successfull parsing"));
          got_picture = 0;
        }
      } else {
        got_picture = 0;
      }
    } else {
      used_bytes = avcodec_decode_video2 (m_pAVCtx, m_pFrame, &got_picture, &avpkt);
      buflen = 0;
    }

    if (FAILED(PostDecode())) {
      av_frame_unref(m_pFrame);
      return E_FAIL;
    }

    // Decoding of this frame failed ... oh well!
    if (used_bytes < 0) {
      av_frame_unref(m_pFrame);
      return S_OK;
    }

    // Judge frame usability
    // This determines if a frame is artifact free and can be delivered.
    if (m_bResumeAtKeyFrame) {
      if (m_bWaitingForKeyFrame && got_picture) {
        if (m_pFrame->key_frame) {
          DbgLog((LOG_TRACE, 50, L"::Decode() - Found Key-Frame, resuming decoding at %I64d", m_pFrame->pkt_pts));
          m_bWaitingForKeyFrame = FALSE;
        } else {
          got_picture = 0;
        }
      }
    }

    // Handle B-frame delay for frame threading codecs
    if ((m_pAVCtx->active_thread_type & FF_THREAD_FRAME) && m_bBFrameDelay) {
      m_tcBFrameDelay[m_nBFramePos] = m_tcThreadBuffer[m_CurrentThread];
      m_nBFramePos = !m_nBFramePos;
    }

    if (!got_picture || !m_pFrame->data[0]) {
      if (!avpkt.size)
        bFlush = FALSE; // End flushing, no more frames
      av_frame_unref(m_pFrame);
      continue;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Determine the proper timestamps for the frame, based on different possible flags.
    ///////////////////////////////////////////////////////////////////////////////////////////////
    if (m_bFFReordering) {
      rtStart = m_pFrame->pkt_pts;
      if (m_pFrame->pkt_duration)
        rtStop = m_pFrame->pkt_pts + m_pFrame->pkt_duration;
      else
        rtStop = AV_NOPTS_VALUE;
    } else if (m_bBFrameDelay && m_pAVCtx->has_b_frames) {
      rtStart = m_tcBFrameDelay[m_nBFramePos].rtStart;
      rtStop  = m_tcBFrameDelay[m_nBFramePos].rtStop;
    } else if (m_pAVCtx->active_thread_type & FF_THREAD_FRAME) {
      unsigned index = m_CurrentThread;
      rtStart = m_tcThreadBuffer[index].rtStart;
      rtStop  = m_tcThreadBuffer[index].rtStop;
    }

    if (m_bRVDropBFrameTimings && m_pFrame->pict_type == AV_PICTURE_TYPE_B) {
      rtStart = AV_NOPTS_VALUE;
    }

    if (m_bCalculateStopTime)
      rtStop = AV_NOPTS_VALUE;

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // All required values collected, deliver the frame
    ///////////////////////////////////////////////////////////////////////////////////////////////
    LAVFrame *pOutFrame = nullptr;
    AllocateFrame(&pOutFrame);

    AVRational display_aspect_ratio;
    int64_t num = (int64_t)m_pFrame->sample_aspect_ratio.num * m_pFrame->width;
    int64_t den = (int64_t)m_pFrame->sample_aspect_ratio.den * m_pFrame->height;
    av_reduce(&display_aspect_ratio.num, &display_aspect_ratio.den, num, den, INT_MAX);

    pOutFrame->width        = m_pFrame->width;
    pOutFrame->height       = m_pFrame->height;
    pOutFrame->aspect_ratio = display_aspect_ratio;
    pOutFrame->repeat       = m_pFrame->repeat_pict;
    pOutFrame->key_frame    = m_pFrame->key_frame;
    pOutFrame->frame_type   = av_get_picture_type_char(m_pFrame->pict_type);
    pOutFrame->ext_format   = GetDXVA2ExtendedFlags(m_pAVCtx, m_pFrame);

    if (m_pFrame->interlaced_frame || (!m_pAVCtx->progressive_sequence && (m_nCodecId == AV_CODEC_ID_H264 || m_nCodecId == AV_CODEC_ID_MPEG2VIDEO)))
      m_iInterlaced = 1;
    else if (m_pAVCtx->progressive_sequence)
      m_iInterlaced = 0;

    if ((m_nCodecId == AV_CODEC_ID_H264 || m_nCodecId == AV_CODEC_ID_MPEG2VIDEO) && m_pFrame->repeat_pict)
      m_nSoftTelecine = 2;
    else if (m_nSoftTelecine > 0)
      m_nSoftTelecine--;

    // Don't apply aggressive deinterlacing to content that looks soft-telecined, as it would destroy the content
    bool bAggressiveFlag    = (m_iInterlaced == 1 && m_pSettings->GetDeinterlacingMode() == DeintMode_Aggressive) && !m_nSoftTelecine;

    pOutFrame->interlaced   = (m_pFrame->interlaced_frame || bAggressiveFlag || m_pSettings->GetDeinterlacingMode() == DeintMode_Force) && !(m_pSettings->GetDeinterlacingMode() == DeintMode_Disable);

    LAVDeintFieldOrder fo   = m_pSettings->GetDeintFieldOrder();
    pOutFrame->tff          = (fo == DeintFieldOrder_Auto) ? m_pFrame->top_field_first : (fo == DeintFieldOrder_TopFieldFirst);

    pOutFrame->rtStart      = rtStart;
    pOutFrame->rtStop       = rtStop;

    PixelFormatMapping map  = getPixFmtMapping((AVPixelFormat)m_pFrame->format);
    pOutFrame->format       = map.lavpixfmt;
    pOutFrame->bpp          = map.bpp;

    if (m_nCodecId == AV_CODEC_ID_MPEG2VIDEO || m_nCodecId == AV_CODEC_ID_MPEG1VIDEO)
      pOutFrame->avgFrameDuration = GetFrameDuration();

    if (map.conversion) {
      ConvertPixFmt(m_pFrame, pOutFrame);
    } else {
      AVFrame *pFrameRef = av_frame_alloc();
      av_frame_ref(pFrameRef, m_pFrame);

      for (int i = 0; i < 4; i++) {
        pOutFrame->data[i]   = pFrameRef->data[i];
        pOutFrame->stride[i] = pFrameRef->linesize[i];
      }

      pOutFrame->priv_data = pFrameRef;
      pOutFrame->destruct = lav_avframe_free;

      // Check alignment on rawvideo, which can be off depending on the source file
      if (m_nCodecId == AV_CODEC_ID_RAWVIDEO) {
        for (int i = 0; i < 4; i++) {
          if ((intptr_t)pOutFrame->data[i] % 16u || pOutFrame->stride[i] % 16u) {
            // copy the frame, its not aligned properly and would crash later
            CopyLAVFrameInPlace(pOutFrame);
            break;
          }
        }
      }
    }

    if (bEndOfSequence)
      pOutFrame->flags |= LAV_FRAME_FLAG_END_OF_SEQUENCE;

    if (pOutFrame->format == LAVPixFmt_DXVA2) {
      pOutFrame->data[0] = m_pFrame->data[4];
      HandleDXVA2Frame(pOutFrame);
    } else {
      Deliver(pOutFrame);
    }

    if (bEndOfSequence) {
      bEndOfSequence = FALSE;
      if (pOutFrame->format == LAVPixFmt_DXVA2) {
        HandleDXVA2Frame(m_pCallback->GetFlushFrame());
      } else {
        Deliver(m_pCallback->GetFlushFrame());
      }
    }

    if (bFlush) {
      m_CurrentThread = (m_CurrentThread + 1) % m_pAVCtx->thread_count;
    }
    av_frame_unref(m_pFrame);
  }

  return S_OK;
}

STDMETHODIMP CDecAvcodec::Flush()
{
  if (m_pAVCtx && avcodec_is_open(m_pAVCtx)) {
    avcodec_flush_buffers(m_pAVCtx);
  }

  if (m_pParser) {
    av_parser_close(m_pParser);
    m_pParser = av_parser_init(m_nCodecId);
  }

  m_CurrentThread = 0;
  m_rtStartCache = AV_NOPTS_VALUE;
  m_bWaitingForKeyFrame = TRUE;
  m_nSoftTelecine = 0;

  m_nBFramePos = 0;
  m_tcBFrameDelay[0].rtStart = m_tcBFrameDelay[0].rtStop = AV_NOPTS_VALUE;
  m_tcBFrameDelay[1].rtStart = m_tcBFrameDelay[1].rtStop = AV_NOPTS_VALUE;

  if (!(m_pCallback->GetDecodeFlags() & LAV_VIDEO_DEC_FLAG_DVD) && (m_nCodecId == AV_CODEC_ID_H264 || m_nCodecId == AV_CODEC_ID_MPEG2VIDEO)) {
    CDecAvcodec::InitDecoder(m_nCodecId, &m_pCallback->GetInputMediaType());
  }

  return __super::Flush();
}

STDMETHODIMP CDecAvcodec::EndOfStream()
{
  Decode(nullptr, 0, AV_NOPTS_VALUE, AV_NOPTS_VALUE, FALSE, FALSE);
  return S_OK;
}

STDMETHODIMP CDecAvcodec::GetPixelFormat(LAVPixelFormat *pPix, int *pBpp)
{
  AVPixelFormat pixfmt = m_pAVCtx ? m_pAVCtx->pix_fmt : AV_PIX_FMT_NONE;
  PixelFormatMapping mapping = getPixFmtMapping(pixfmt);
  if (pPix)
    *pPix = mapping.lavpixfmt;
  if (pBpp)
    *pBpp = mapping.bpp;
  return S_OK;
}

STDMETHODIMP CDecAvcodec::ConvertPixFmt(AVFrame *pFrame, LAVFrame *pOutFrame)
{
  // Allocate the buffers to write into
  AllocLAVFrameBuffers(pOutFrame);

  // Map to swscale compatible format
  AVPixelFormat dstFormat = getFFPixelFormatFromLAV(pOutFrame->format, pOutFrame->bpp);

  // Get a context
  m_pSwsContext = sws_getCachedContext(m_pSwsContext, pFrame->width, pFrame->height, (AVPixelFormat)pFrame->format, pFrame->width, pFrame->height, dstFormat, SWS_BILINEAR | SWS_FULL_CHR_H_INT | SWS_PRINT_INFO, nullptr, nullptr, nullptr);

  ptrdiff_t linesize[4];
  for (int i = 0; i < 4; i++)
    linesize[i] = pFrame->linesize[i];

  // Perform conversion
  sws_scale2(m_pSwsContext, pFrame->data, linesize, 0, pFrame->height, pOutFrame->data, pOutFrame->stride);

  return S_OK;
}

STDMETHODIMP_(REFERENCE_TIME) CDecAvcodec::GetFrameDuration()
{
  if (m_pAVCtx->time_base.den && m_pAVCtx->time_base.num)
    return (REF_SECOND_MULT * m_pAVCtx->time_base.num / m_pAVCtx->time_base.den) * m_pAVCtx->ticks_per_frame;
  return 0;
}

STDMETHODIMP_(BOOL) CDecAvcodec::IsInterlaced()
{
  return (m_iInterlaced != 0 || m_pSettings->GetDeinterlacingMode() == DeintMode_Force);
}
