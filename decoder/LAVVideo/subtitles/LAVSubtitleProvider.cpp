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
#include "LAVSubtitleProvider.h"

#include "moreuuids.h"
#include "libavutil/colorspace.h"

#define FAST_DIV255(x) ((((x) + 128) * 257) >> 16)

#define OFFSET(x) offsetof(LAVSubtitleProviderContext, x)
static const SubRenderOption options[] = {
  { "name",           OFFSET(name),            SROPT_TYPE_STRING, SROPT_FLAG_READONLY },
  { "version",        OFFSET(version),         SROPT_TYPE_STRING, SROPT_FLAG_READONLY },
  { "combineBitmaps", OFFSET(combineBitmaps),  SROPT_TYPE_BOOL,   0                   },
  { 0 }
};

CLAVSubtitleProvider::CLAVSubtitleProvider(ISubRenderConsumer *pConsumer)
  : CSubRenderOptionsImpl(::options, &context)
  , CUnknown(L"CLAVSubtitleProvider", NULL)
  , m_pConsumer(pConsumer)
  , m_pAVCodec(NULL)
  , m_pAVCtx(NULL)
  , m_pParser(NULL)
  , m_rtStartCache(AV_NOPTS_VALUE)
  , m_SubPicId(0)
{
  avcodec_register_all();

  ASSERT(m_pConsumer);
  ZeroMemory(&context, sizeof(context));
  context.name = TEXT(LAV_VIDEO);
  context.version = TEXT(LAV_VERSION_STR);
  AddRef();

  m_pConsumer->AddRef();
  m_pConsumer->Connect(this);
}

CLAVSubtitleProvider::~CLAVSubtitleProvider(void)
{
  CloseDecoder();
  DisconnectConsumer();
}

void CLAVSubtitleProvider::CloseDecoder()
{
  CAutoLock lock(this);
  m_pAVCodec = NULL;
  if (m_pAVCtx) {
    if (m_pAVCtx->extradata)
      av_freep(&m_pAVCtx->extradata);
    avcodec_close(m_pAVCtx);
    av_freep(&m_pAVCtx);
  }
  if (m_pParser) {
    av_parser_close(m_pParser);
    m_pParser = NULL;
  }
}

STDMETHODIMP CLAVSubtitleProvider::DisconnectConsumer(void)
{
  CAutoLock lock(this);
  CheckPointer(m_pConsumer, S_FALSE);
  m_pConsumer->Disconnect();
  SafeRelease(&m_pConsumer);

  return S_OK;
}

STDMETHODIMP CLAVSubtitleProvider::RequestFrame(REFERENCE_TIME start, REFERENCE_TIME stop)
{
  CAutoLock lock(this);
  ASSERT(m_pConsumer);

  // Create a new frame
  CLAVSubtitleFrame *subtitleFrame = new CLAVSubtitleFrame();
  subtitleFrame->AddRef();

  RECT outputRect;
  ::SetRect(&outputRect, 0, 0, m_pAVCtx->width, m_pAVCtx->height);
  subtitleFrame->SetOutputRect(outputRect);

  for (auto it = m_SubFrames.begin(); it != m_SubFrames.end(); it++) {
    LAVSubRect *rect = *it;
    if ((rect->rtStart == AV_NOPTS_VALUE || rect->rtStart <= start)
      && (rect->rtStop == AV_NOPTS_VALUE || rect->rtStop >= stop)) {
      subtitleFrame->AddBitmap(*rect);
    }
  }

  if (subtitleFrame->Empty()) {
    SAFE_DELETE(subtitleFrame);
  }

  // Deliver Frame
  m_pConsumer->DeliverFrame(start, stop, subtitleFrame);

  return S_OK;
}

STDMETHODIMP CLAVSubtitleProvider::Disconnect(void)
{
  SafeRelease(&m_pConsumer);
  return S_OK;
}

STDMETHODIMP CLAVSubtitleProvider::InitDecoder(const CMediaType *pmt, AVCodecID codecId)
{
  CAutoLock lock(this);
  m_pAVCodec = avcodec_find_decoder(codecId);
  CheckPointer(m_pAVCodec, VFW_E_TYPE_NOT_ACCEPTED);

  m_pAVCtx = avcodec_alloc_context3(m_pAVCodec);
  CheckPointer(m_pAVCtx, E_POINTER);

  m_pParser = av_parser_init(codecId);

  size_t extralen = 0;
  getExtraData((const BYTE *)pmt->Format(), pmt->FormatType(), pmt->FormatLength(), NULL, &extralen);

  if (extralen > 0) {
    // Just copy extradata
    BYTE *extra = (uint8_t *)av_mallocz(extralen + FF_INPUT_BUFFER_PADDING_SIZE);
    getExtraData((const BYTE *)pmt->Format(), pmt->FormatType(), pmt->FormatLength(), extra, NULL);

    m_pAVCtx->extradata = extra;
    m_pAVCtx->extradata_size = extralen;
  }

  if (pmt->formattype == FORMAT_SubtitleInfo) {
    // Not much info in here
  } else {
    // Try video info
    BITMAPINFOHEADER *bmi = NULL;
    videoFormatTypeHandler(*pmt, &bmi, NULL, NULL, NULL);
    m_pAVCtx->width = bmi->biWidth;
    m_pAVCtx->height = bmi->biHeight;
  }

  int ret = avcodec_open2(m_pAVCtx, m_pAVCodec, NULL);
  if (ret < 0) {
    DbgLog((LOG_TRACE, 10, L"CLAVSubtitleProvider::InitDecoder(): avocdec_open2 failed with %d", ret));
    CloseDecoder();
    return VFW_E_TYPE_NOT_ACCEPTED;
  }

  return S_OK;
}

STDMETHODIMP CLAVSubtitleProvider::Flush()
{
  CAutoLock lock(this);
  for (auto it = m_SubFrames.begin(); it != m_SubFrames.end(); it++) {
    CoTaskMemFree((LPVOID)(*it)->pixels);
    delete *it;
  }
  std::list<LAVSubRect*>().swap(m_SubFrames);
  return S_OK;
}

STDMETHODIMP CLAVSubtitleProvider::Decode(BYTE *buf, int buflen, REFERENCE_TIME rtStartIn, REFERENCE_TIME rtStopIn)
{
  CAutoLock lock(this);
  ASSERT(m_pAVCtx);

  AVPacket avpkt;
  av_init_packet(&avpkt);

  AVSubtitle sub;
  memset(&sub, 0, sizeof(sub));

  if (!buflen || !buf) {
    return S_OK;
  }

  DbgLog((LOG_TRACE, 10, L"Packet! rtStart: %I64d, rtStop: %I64d, size: %d", rtStartIn, rtStopIn, buflen));

  while (buflen > 0) {
    REFERENCE_TIME rtStart = rtStartIn, rtStop = rtStopIn;
    int used_bytes = 0;
    int got_sub = 0;
    if (m_pParser) {
      uint8_t *pOut = NULL;
      int pOut_size = 0;
      used_bytes = av_parser_parse2(m_pParser, m_pAVCtx, &pOut, &pOut_size, buf, buflen, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
      if (used_bytes == 0 && pOut_size == 0) {
        DbgLog((LOG_TRACE, 50, L"CLAVSubtitleProvider::Decode - could not process buffer"));
        break;
      }

      if (used_bytes > pOut_size) {
        if (rtStartIn != AV_NOPTS_VALUE)
          m_rtStartCache = rtStartIn;
      } else if (used_bytes == pOut_size) {
        m_rtStartCache = rtStartIn = AV_NOPTS_VALUE;
      } else if (pOut_size > used_bytes) {
        rtStart = m_rtStartCache;
        m_rtStartCache = rtStartIn;
        // The value was used once, don't use it for multiple frames, that ends up in weird timings
        rtStartIn = AV_NOPTS_VALUE;
      }

      if (pOut_size > 0) {
        avpkt.data = pOut;
        avpkt.size = pOut_size;
        avpkt.pts = rtStart;
        avpkt.duration = 0;

        int ret = avcodec_decode_subtitle2(m_pAVCtx, &sub, &got_sub, &avpkt);
        if (ret < 0) {
          DbgLog((LOG_TRACE, 50, L"CLAVSubtitleProvider::Decode - decoding failed despite successfull parsing"));
          got_sub = 0;
        }
      } else {
        got_sub = 0;
      }
    }

    if (used_bytes < 0) {
      return S_OK;
    }

    if (!m_pParser && (!got_sub && used_bytes == 0)) {
      buflen = 0;
    } else {
      buf += used_bytes;
      buflen -= used_bytes;
    }

    if (got_sub) {
      REFERENCE_TIME rtSubStart = rtStart, rtSubStop = AV_NOPTS_VALUE;
      if (rtSubStart != AV_NOPTS_VALUE) {
        if (sub.end_display_time > 0) {
          rtSubStop = rtSubStart + (sub.end_display_time * 10000i64);
        }
        rtSubStart += sub.start_display_time * 10000i64;
      }
      DbgLog((LOG_TRACE, 10, L"Decoded Sub: rtStart: %I64d, rtStop: %I64d, num_rects: %u", rtSubStart, rtSubStop, sub.num_rects));

      ProcessSubtitleRect(&sub, rtSubStart, rtSubStop);
    }

    avsubtitle_free(&sub);
  }

  return S_OK;
}

void CLAVSubtitleProvider::ProcessSubtitleRect(AVSubtitle *sub, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop)
{
  if (sub->num_rects > 0) {
    for (unsigned i = 0; i < sub->num_rects; i++) {
      AVSubtitleRect *rect = sub->rects[i];
      BYTE *rgbSub = (BYTE *)CoTaskMemAlloc(rect->pict.linesize[0] * rect->h * 4);
      BYTE *rgbSubStart = rgbSub;
      const BYTE *palSub = rect->pict.data[0];
      const BYTE *palette = rect->pict.data[1];
      for (int y = 0; y < rect->h; y++) {
        for (int x = 0; x < rect->w; x++) {
          // Read paletted value
          int idx = palSub[x];

          // Skip invalid values
          if (idx >= rect->nb_colors)
            continue;

          // Read RGB values from palette
          BYTE b = palette[(idx << 2) + 0];
          BYTE g = palette[(idx << 2) + 1];
          BYTE r = palette[(idx << 2) + 2];
          BYTE a = palette[(idx << 2) + 3];
          // Store as RGBA pixel, pre-multiplied
          rgbSub[(x << 2) + 0] = FAST_DIV255(b * a);
          rgbSub[(x << 2) + 1] = FAST_DIV255(g * a);
          rgbSub[(x << 2) + 2] = FAST_DIV255(r * a);
          rgbSub[(x << 2) + 3] = a;
        }
        palSub += rect->pict.linesize[0];
        rgbSub += rect->pict.linesize[0] * 4;
      }

      // Store the rect
      POINT position = { rect->x, rect->y };
      SIZE size = { rect->w, rect->h };
      LAVSubRect *lavRect = new LAVSubRect();
      lavRect->id       = m_SubPicId++;
      lavRect->pitch    = rect->pict.linesize[0];
      lavRect->pixels   = rgbSubStart;
      lavRect->position = position;
      lavRect->size     = size;
      lavRect->rtStart  = rtStart;
      lavRect->rtStop   = rtStop;

      // Ensure the width/height in avctx are valid
      m_pAVCtx->width  = FFMAX(m_pAVCtx->width,  rect->x + rect->w);
      m_pAVCtx->height = FFMAX(m_pAVCtx->height, rect->y + rect->h);

      // HACK: Since we're only dealing with DVDs so far, do some trickery here
      if (m_pAVCtx->height > 480 && m_pAVCtx->height < 576)
        m_pAVCtx->height = 576;

      AddSubtitleRect(lavRect);
    }
  } else {
    // TODO: timeout subs
  }
}

void CLAVSubtitleProvider::AddSubtitleRect(LAVSubRect *rect)
{
  CAutoLock lock(this);
  m_SubFrames.push_back(rect);
}

typedef struct DVDSubContext
{
  uint32_t palette[16];
  int      has_palette;
  uint8_t  colormap[4];
  uint8_t  alpha[256];
} DVDSubContext;

#define MAX_NEG_CROP 1024
extern "C" __declspec(dllimport) uint8_t ff_cropTbl[256 + 2 * MAX_NEG_CROP];

STDMETHODIMP CLAVSubtitleProvider::SetDVDPalette(AM_PROPERTY_SPPAL *pPal)
{
  CAutoLock lock(this);
  if (!m_pAVCtx || m_pAVCtx->codec_id != AV_CODEC_ID_DVD_SUBTITLE || !pPal) {
    return E_FAIL;
  }

  DVDSubContext *ctx = (DVDSubContext *)m_pAVCtx->priv_data;
  ctx->has_palette = 1;

  uint8_t r,g,b;
  int i, y, cb, cr;
  int r_add, g_add, b_add;
  uint8_t *cm = ff_cropTbl + MAX_NEG_CROP;
  for (i = 0; i < 16; i++) {
     y  = pPal->sppal[i].Y;
     cb = pPal->sppal[i].U;
     cr = pPal->sppal[i].V;
     YUV_TO_RGB1_CCIR(cb, cr);
     YUV_TO_RGB2_CCIR(r, g, b, y);
     ctx->palette[i] = (0xFF << 24) | (r << 16) | (g << 8) | b;
  }

  return S_OK;
}
