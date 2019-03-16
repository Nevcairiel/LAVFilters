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
#include "LAVSubtitleProvider.h"

#include "moreuuids.h"
#include "libavutil/colorspace.h"

#include "LAVVideo.h"

#include "version.h"

#define FAST_DIV255(x) ((((x) + 128) * 257) >> 16)
#define SUBTITLE_PTS_TIMEOUT (AV_NOPTS_VALUE + 1)

#define OFFSET(x) offsetof(LAVSubtitleProviderContext, x)
static const SubRenderOption options[] = {
  { "name",           OFFSET(name),            SROPT_TYPE_STRING, SROPT_FLAG_READONLY },
  { "version",        OFFSET(version),         SROPT_TYPE_STRING, SROPT_FLAG_READONLY },
  { "yuvMatrix",      OFFSET(yuvMatrix),       SROPT_TYPE_STRING, SROPT_FLAG_READONLY },
  { "isBitmap",       OFFSET(isBitmap),        SROPT_TYPE_BOOL,   SROPT_FLAG_READONLY },
  { "isMovable",      OFFSET(isMovable),       SROPT_TYPE_BOOL,   SROPT_FLAG_READONLY },
  { "combineBitmaps", OFFSET(combineBitmaps),  SROPT_TYPE_BOOL,   0                   },
  { 0 }
};

CLAVSubtitleProvider::CLAVSubtitleProvider(CLAVVideo *pLAVVideo, ISubRenderConsumer *pConsumer)
  : CSubRenderOptionsImpl(::options, &context)
  , CUnknown(L"CLAVSubtitleProvider", nullptr)
  , m_pLAVVideo(pLAVVideo)
{
  m_ControlThread = new CLAVSubtitleProviderControlThread();

  ASSERT(pConsumer);
  ZeroMemory(&context, sizeof(context));
  context.name = TEXT(LAV_VIDEO);
  context.version = TEXT(LAV_VERSION_STR);
  context.yuvMatrix = _T("PC.601");
  context.isBitmap = true;
  context.isMovable = true;
  AddRef();

  SetConsumer(pConsumer);
}

CLAVSubtitleProvider::~CLAVSubtitleProvider(void)
{
  Flush();
  CloseDecoder();
  DisconnectConsumer();
  SAFE_DELETE(m_ControlThread);
}

void CLAVSubtitleProvider::CloseDecoder()
{
  CAutoLock lock(this);
  m_pAVCodec = nullptr;
  if (m_pAVCtx) {
    if (m_pAVCtx->extradata)
      av_freep(&m_pAVCtx->extradata);
    avcodec_close(m_pAVCtx);
    av_freep(&m_pAVCtx);
  }
  if (m_pParser) {
    av_parser_close(m_pParser);
    m_pParser = nullptr;
  }
}

STDMETHODIMP CLAVSubtitleProvider::SetConsumer(ISubRenderConsumer *pConsumer)
{
  CAutoLock lock(this);
  if (m_pConsumer)
    DisconnectConsumer();

  CheckPointer(pConsumer, E_FAIL);

  m_pConsumer = pConsumer;
  m_pConsumer->AddRef();
  m_pConsumer->Connect(this);

  if (FAILED(m_pConsumer->QueryInterface(&m_pConsumer2)))
    m_pConsumer2 = nullptr;

  m_ControlThread->SetConsumer2(m_pConsumer2);

  return S_OK;
}

STDMETHODIMP CLAVSubtitleProvider::DisconnectConsumer(void)
{
  CAutoLock lock(this);
  CheckPointer(m_pConsumer, S_FALSE);
  m_ControlThread->SetConsumer2(nullptr);
  m_pConsumer->Disconnect();
  SafeRelease(&m_pConsumer);
  SafeRelease(&m_pConsumer2);

  return S_OK;
}

#define PTS2RT(pts) (10000i64 * pts / 90)

STDMETHODIMP CLAVSubtitleProvider::RequestFrame(REFERENCE_TIME start, REFERENCE_TIME stop, LPVOID context)
{
  ASSERT(m_pConsumer);

  // Create a new frame
  CLAVSubtitleFrame *subtitleFrame = new CLAVSubtitleFrame();
  subtitleFrame->AddRef();

  if (m_pAVCtx->width == 720 && m_pAVCtx->height == 480) {
    SIZE videoSize;
    m_pConsumer->GetSize("originalVideoSize", &videoSize);
    if (videoSize.cx == 720) {
      m_pAVCtx->height = videoSize.cy;
    }
  }

  RECT outputRect;
  ::SetRect(&outputRect, 0, 0, m_pAVCtx->width, m_pAVCtx->height);
  subtitleFrame->SetOutputRect(outputRect);

  REFERENCE_TIME mid = start + ((stop-start) >> 1);

  // Scope this so we limit the provider-lock to the part where its needed
  {
    CAutoLock lock(this);
    for (auto it = m_SubFrames.begin(); it != m_SubFrames.end(); it++) {
      CLAVSubRect *pRect = *it;
      if ((pRect->rtStart == AV_NOPTS_VALUE || pRect->rtStart <= mid)
        && (pRect->rtStop == AV_NOPTS_VALUE || pRect->rtStop > mid)
        && (m_bComposit || pRect->forced)) {

        if (m_pHLI && PTS2RT(m_pHLI->StartPTM) <= mid && PTS2RT(m_pHLI->EndPTM) >= mid) {
          pRect = ProcessDVDHLI(pRect);
        }
        subtitleFrame->AddBitmap(pRect);
      }
    }

    m_rtLastFrame = start;
  }

  if (subtitleFrame->Empty()) {
    SafeRelease(&subtitleFrame);
  }

  // Deliver Frame
  m_pConsumer->DeliverFrame(start, stop, context, subtitleFrame);
  SafeRelease(&subtitleFrame);

  TimeoutSubtitleRects(stop);

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
  getExtraData((const BYTE *)pmt->Format(), pmt->FormatType(), pmt->FormatLength(), nullptr, &extralen);

  if (extralen > 0) {
    // Just copy extradata
    BYTE *extra = (uint8_t *)av_mallocz(extralen + AV_INPUT_BUFFER_PADDING_SIZE);
    getExtraData((const BYTE *)pmt->Format(), pmt->FormatType(), pmt->FormatLength(), extra, nullptr);

    m_pAVCtx->extradata = extra;
    m_pAVCtx->extradata_size = (int)extralen;
  }

  if (pmt->formattype == FORMAT_SubtitleInfo) {
    // Not much info in here
  } else {
    // Try video info
    BITMAPINFOHEADER *bmi = nullptr;
    videoFormatTypeHandler(*pmt, &bmi, nullptr, nullptr, nullptr);
    m_pAVCtx->width = bmi->biWidth;
    m_pAVCtx->height = bmi->biHeight;
  }

  int ret = avcodec_open2(m_pAVCtx, m_pAVCodec, nullptr);
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
  ClearSubtitleRects();
  SAFE_DELETE(m_pHLI);

  m_rtLastFrame = AV_NOPTS_VALUE;
  context.isMovable = true;
  m_pLAVVideo->SetInDVDMenu(false);

  return S_OK;
}

void CLAVSubtitleProvider::ClearSubtitleRects()
{
  CAutoLock lock(this);
  for (auto it = m_SubFrames.begin(); it != m_SubFrames.end(); it++) {
    (*it)->Release();
  }
  m_SubFrames.clear();
}

void CLAVSubtitleProvider::TimeoutSubtitleRects(REFERENCE_TIME rt)
{
  CAutoLock lock(this);
  REFERENCE_TIME timestamp = rt - 10 * 10000000; // Timeout all subs 10 seconds in the past
  auto it = m_SubFrames.begin();
  while (it != m_SubFrames.end()) {
    if ((*it)->rtStop != AV_NOPTS_VALUE && (*it)->rtStop < timestamp) {
      DbgLog((LOG_TRACE, 10, L"Timed out subtitle at %I64d", (*it)->rtStart));
      (*it)->Release();
      it = m_SubFrames.erase(it);
    } else {
      it++;
    }
  }
}

STDMETHODIMP CLAVSubtitleProvider::Decode(BYTE *buf, int buflen, REFERENCE_TIME rtStartIn, REFERENCE_TIME rtStopIn)
{
  ASSERT(m_pAVCtx);

  AVPacket avpkt;
  av_init_packet(&avpkt);

  AVSubtitle sub;
  memset(&sub, 0, sizeof(sub));

  if (!buflen || !buf) {
    return S_OK;
  }

  while (buflen > 0) {
    REFERENCE_TIME rtStart = rtStartIn, rtStop = rtStopIn;
    int used_bytes = 0;
    int got_sub = 0;
    if (m_pParser) {
      uint8_t *pOut = nullptr;
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
      ProcessSubtitleFrame(&sub, rtStart);
    }

    avsubtitle_free(&sub);
  }

  return S_OK;
}

void CLAVSubtitleProvider::ProcessSubtitleFrame(AVSubtitle *sub, REFERENCE_TIME rtStart)
{
  DbgLog((LOG_TRACE, 10, L"Decoded Sub: rtStart: %I64d, start_display_time: %d, end_display_time: %d, num_rects: %u, num_dvd_palette: %d", rtStart, sub->start_display_time, sub->end_display_time, sub->num_rects, sub->num_dvd_palette));
  if (sub->num_rects > 0) {
    if (m_pAVCtx->codec_id == AV_CODEC_ID_DVD_SUBTITLE) {
      CAutoLock lock(this);

      // DVD subs have the limitation that only one subtitle can be shown at a given time,
      // so we need to timeout unlimited subs when a new one appears, as well as limit the duration of timed subs
      // to prevent overlapping subtitles
      REFERENCE_TIME rtSubTimeout = (rtStart != AV_NOPTS_VALUE) ? rtStart - 1 : SUBTITLE_PTS_TIMEOUT;
      for (auto it = m_SubFrames.begin(); it != m_SubFrames.end(); it++) {
        if ((*it)->rtStop == AV_NOPTS_VALUE || rtStart == AV_NOPTS_VALUE || (*it)->rtStop > rtStart) {
          (*it)->rtStop = rtSubTimeout;
        }
      }

      // Override subtitle timestamps if we have a timeout, and are not in a menu
      if (rtStart == AV_NOPTS_VALUE && sub->end_display_time > 0 && !(sub->rects[0]->flags & AV_SUBTITLE_FLAG_FORCED)) {
        DbgLog((LOG_TRACE, 10, L" -> Overriding subtitle timestamp to %I64d", m_rtLastFrame));
        rtStart = m_rtLastFrame;
      }
    }

    REFERENCE_TIME rtStop = AV_NOPTS_VALUE;
    if (rtStart != AV_NOPTS_VALUE) {
      if (sub->end_display_time > 0) {
        rtStop = rtStart + (sub->end_display_time * 10000i64);
      }
      rtStart += sub->start_display_time * 10000i64;
    }

    for (unsigned i = 0; i < sub->num_rects; i++) {
      if (sub->num_dvd_palette > 1 && rtStart != AV_NOPTS_VALUE) {
        REFERENCE_TIME rtStartRect = rtStart - (sub->start_display_time * 10000i64);
        REFERENCE_TIME rtStopRect = rtStart;
        for (unsigned k = 0; k < sub->num_dvd_palette; k++) {
          // Start is the stop of the previous part
          rtStartRect = rtStopRect;

          // Stop is either the start of the next part, or the final stop
          if (k < (sub->num_dvd_palette-1))
            rtStopRect = rtStart + (sub->dvd_palette[k+1]->start_display_time * 10000i64);
          else
            rtStopRect = rtStop;

          // Update palette with new alpha values
          for (unsigned j = 0; j < 4; j++)
            sub->rects[i]->data[1][(j << 2) + 3] = sub->dvd_palette[k]->alpha[j] * 17;

          ProcessSubtitleRect(sub->rects[i], rtStartRect, rtStopRect);
        }
      } else {
        ProcessSubtitleRect(sub->rects[i], rtStart, rtStop);
      }
    }
  }
}

void CLAVSubtitleProvider::ProcessSubtitleRect(AVSubtitleRect *rect, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop)
{
  DbgLog((LOG_TRACE, 10, L"Subtitle Rect, start: %I64d, stop: %I64d", rtStart, rtStop));
  // Skip zero-length subs
  if (rtStart != AV_NOPTS_VALUE && rtStart == rtStop) return;

  int hpad = rect->x & 1;
  int vpad = rect->y & 1;

  int width = rect->w + hpad;
  if (width & 1) width++;

  int height = rect->h + vpad;
  if (height & 1) height++;

  int rgbStride = FFALIGN(width, 16);

  BYTE *rgbSub = (BYTE *)CoTaskMemAlloc(rgbStride * height * 4);
  if (!rgbSub) return;
  BYTE *rgbSubStart = rgbSub;
  const BYTE *palSub = rect->data[0];
  const BYTE *palette = rect->data[1];

  memset(rgbSub, 0, rgbStride * height * 4);

  rgbSub += (rgbStride * vpad + hpad) * 4;

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
    palSub += rect->linesize[0];
    rgbSub += rgbStride * 4;
  }

  // Store the rect
  POINT position = { rect->x - hpad, rect->y - vpad };
  SIZE size = { width, height };
  CLAVSubRect *lavRect = new CLAVSubRect();
  if (!lavRect) return;
  lavRect->id       = m_SubPicId++;
  lavRect->pitch    = rgbStride;
  lavRect->pixels   = rgbSubStart;
  lavRect->position = position;
  lavRect->size     = size;
  lavRect->rtStart  = rtStart;
  lavRect->rtStop   = rtStop;
  lavRect->forced   = rect->flags & AV_SUBTITLE_FLAG_FORCED;

  if (m_pAVCtx->codec_id == AV_CODEC_ID_DVD_SUBTITLE) {
    lavRect->pixelsPal = CoTaskMemAlloc(lavRect->pitch * lavRect->size.cy);
    if (!lavRect->pixelsPal) return;

    int paletteTransparent = 0;
    for (int i = 0; i < rect->nb_colors; i++) {
      if (palette[(i << 2) + 3] == 0) {
        paletteTransparent = i;
        break;
      }
    }
    memset(lavRect->pixelsPal, paletteTransparent, lavRect->pitch * lavRect->size.cy);
    BYTE *palPixels = (BYTE *)lavRect->pixelsPal;
    palSub = rect->data[0];

    palPixels += lavRect->pitch * vpad + hpad;
    for (int y = 0; y < rect->h; y++) {
      memcpy(palPixels, palSub, rect->w);
      palPixels += lavRect->pitch;
      palSub += rect->linesize[0];
    }
  }

  // Ensure the width/height in avctx are valid
  m_pAVCtx->width  = FFMAX(m_pAVCtx->width,  position.x + size.cx);
  m_pAVCtx->height = FFMAX(m_pAVCtx->height, position.y + size.cy);

  // HACK: Since we're only dealing with DVDs so far, do some trickery here
  if (m_pAVCtx->height > 480 && m_pAVCtx->height < 576)
    m_pAVCtx->height = 576;

  AddSubtitleRect(lavRect);
}

void CLAVSubtitleProvider::AddSubtitleRect(CLAVSubRect *rect)
{
  CAutoLock lock(this);
  rect->AddRef();
  m_SubFrames.push_back(rect);
}

typedef struct DVDSubContext
{
  AVClass *avclass;
  uint32_t palette[16];
  char    *palette_str;
  char    *ifo_str;
  int      has_palette;
  uint8_t  colormap[4];
  uint8_t  alpha[256];
  uint8_t  buf[0x10000];
  int      buf_size;
  int      forced_subs_only;
} DVDSubContext;

#define MAX_NEG_CROP 1024
extern "C" __declspec(dllimport) uint8_t ff_crop_tab[256 + 2 * MAX_NEG_CROP];

STDMETHODIMP CLAVSubtitleProvider::SetDVDPalette(AM_PROPERTY_SPPAL *pPal)
{
  DbgLog((LOG_TRACE, 10, L"CLAVSubtitleProvider(): Setting new DVD Palette"));
  CAutoLock lock(this);
  if (!m_pAVCtx || m_pAVCtx->codec_id != AV_CODEC_ID_DVD_SUBTITLE || !pPal) {
    return E_FAIL;
  }

  DVDSubContext *ctx = (DVDSubContext *)m_pAVCtx->priv_data;
  ctx->has_palette = 1;

  uint8_t r,g,b;
  int i, y, cb, cr;
  int r_add, g_add, b_add;
  uint8_t *cm = ff_crop_tab + MAX_NEG_CROP;
  for (i = 0; i < 16; i++) {
    y  = pPal->sppal[i].Y;
    cb = pPal->sppal[i].V;
    cr = pPal->sppal[i].U;
    YUV_TO_RGB1_CCIR(cb, cr);
    YUV_TO_RGB2_CCIR(r, g, b, y);
    ctx->palette[i] = (0xFF << 24) | (r << 16) | (g << 8) | b;
  }

  return S_OK;
}

STDMETHODIMP CLAVSubtitleProvider::SetDVDHLI(struct _AM_PROPERTY_SPHLI *pHLI)
{
  bool redraw = false;

  // Scoped lock so the lock is lifted when the redraw is issued
  // Otherwise we can deadlock in the decoder - this one holding the provider lock, the decoder holding the decoder lock...
  {
    CAutoLock lock(this);
    if (pHLI) {
  #define DHLI(var) (pHLI->var != m_pHLI->var)
      if (!m_pHLI || DHLI(StartX) || DHLI(StopX) || DHLI(StartY) || DHLI(StopY) || memcmp(&pHLI->ColCon, &m_pHLI->ColCon, sizeof(pHLI->ColCon)) != 0) {
        DbgLog((LOG_TRACE, 10, L"CLAVSubtitleProvider(): DVD HLI event. HLISS: %u, x: %u->%u, y: %u->%u, StartPTM: %u, EndPTM: %u", pHLI->HLISS, pHLI->StartX, pHLI->StopX, pHLI->StartY, pHLI->StopY, pHLI->StartPTM, pHLI->EndPTM));
        SAFE_DELETE(m_pHLI);
        m_pHLI = new AM_PROPERTY_SPHLI(*pHLI);
        redraw = true;
      }
      context.isMovable = false;
      m_pLAVVideo->SetInDVDMenu(true);
    } else {
      SAFE_DELETE(m_pHLI);
    }
  }

  if (redraw)
    ControlCmd(CNTRL_FLUSH);

  return S_OK;
}

CLAVSubRect* CLAVSubtitleProvider::ProcessDVDHLI(CLAVSubRect *rect)
{
  DVDSubContext *ctx = (DVDSubContext *)m_pAVCtx->priv_data;
  if (!m_pHLI || !rect->pixelsPal || !ctx->has_palette)
    return rect;

  LPVOID newPixels = CoTaskMemAlloc(rect->pitch * rect->size.cy * 4);
  if (!newPixels) return rect;

  // copy pixels before modification
  memcpy(newPixels, rect->pixels, rect->pitch * rect->size.cy * 4);

  uint8_t *originalPalPixels = (uint8_t *)rect->pixelsPal;

  // create new object
  rect = new CLAVSubRect(*rect);
  rect->ResetRefCount();
  rect->pixels = newPixels;
  rect->pixelsPal = nullptr;

  // Need to assign a new Id since we're modifying it here..
  rect->id = m_SubPicId++;

  uint8_t *palette   = (uint8_t *)ctx->palette;
  for (int y = 0; y < rect->size.cy; y++) {
    if (y+rect->position.y < m_pHLI->StartY || y+rect->position.y > m_pHLI->StopY)
      continue;
    uint8_t *pixelsPal = originalPalPixels + rect->pitch * y;
    uint8_t *pixels = ((uint8_t *)rect->pixels) + rect->pitch * y * 4;
    for (int x = 0; x < rect->size.cx; x++) {
      if (x+rect->position.x < m_pHLI->StartX || x+rect->position.x > m_pHLI->StopX)
        continue;
      uint8_t idx = pixelsPal[x];
      uint8_t alpha = 0;
      switch (idx) {
      case 0:
        idx = m_pHLI->ColCon.backcol;
        alpha = m_pHLI->ColCon.backcon;
        break;
      case 1:
        idx = m_pHLI->ColCon.patcol;
        alpha = m_pHLI->ColCon.patcon;
        break;
      case 2:
        idx = m_pHLI->ColCon.emph1col;
        alpha = m_pHLI->ColCon.emph1con;
        break;
      case 3:
        idx = m_pHLI->ColCon.emph2col;
        alpha = m_pHLI->ColCon.emph2con;
        break;
      }
      // Read RGB values from palette
      BYTE b = palette[(idx << 2) + 0];
      BYTE g = palette[(idx << 2) + 1];
      BYTE r = palette[(idx << 2) + 2];
      BYTE a = alpha << 4;
      // Store as RGBA pixel, pre-multiplied
      pixels[(x << 2) + 0] = FAST_DIV255(b * a);
      pixels[(x << 2) + 1] = FAST_DIV255(g * a);
      pixels[(x << 2) + 2] = FAST_DIV255(r * a);
      pixels[(x << 2) + 3] = a;
    }
  }
  return rect;
}

STDMETHODIMP CLAVSubtitleProvider::SetDVDComposit(BOOL bComposit)
{
  CAutoLock lock(this);
  m_bComposit = bComposit;
  return S_OK;
}

CLAVSubtitleProviderControlThread::CLAVSubtitleProviderControlThread()
  : CAMThread()
{
  Create();
}

CLAVSubtitleProviderControlThread::~CLAVSubtitleProviderControlThread()
{
  CallWorker(CLAVSubtitleProvider::CNTRL_EXIT);
  Close();
}

void CLAVSubtitleProviderControlThread::SetConsumer2(ISubRenderConsumer2 * pConsumer2)
{
  CAutoLock lock(this);
  m_pConsumer2 = pConsumer2;
}

DWORD CLAVSubtitleProviderControlThread::ThreadProc()
{
  SetThreadName(-1, "LAV Subtitle Control Thread");
  DWORD cmd;
  while (1) {
    cmd = GetRequest();
    switch (cmd) {
    case CLAVSubtitleProvider::CNTRL_EXIT:
      Reply(S_OK);
      return 0;
    case CLAVSubtitleProvider::CNTRL_FLUSH:
      Reply(S_OK);
      {
        CAutoLock lock(this);
        if (m_pConsumer2)
          m_pConsumer2->Clear();
      }
      break;
    }
  }
  return 1;
}
