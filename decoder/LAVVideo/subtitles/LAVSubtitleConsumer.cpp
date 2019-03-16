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
#include "LAVSubtitleConsumer.h"
#include "LAVVideo.h"
#include "Media.h"
#include "version.h"

#define OFFSET(x) offsetof(LAVSubtitleConsumerContext, x)
static const SubRenderOption options[] = {
  { "name",           OFFSET(name),            SROPT_TYPE_STRING, SROPT_FLAG_READONLY },
  { "version",        OFFSET(version),         SROPT_TYPE_STRING, SROPT_FLAG_READONLY },
  { "originalVideoSize", OFFSET(originalVideoSize), SROPT_TYPE_SIZE, SROPT_FLAG_READONLY },
  { 0 }
};

#define FAST_DIV255(x) ((((x) + 128) * 257) >> 16)

CLAVSubtitleConsumer::CLAVSubtitleConsumer(CLAVVideo *pLAVVideo)
  : CSubRenderOptionsImpl(::options, &context)
  , CUnknown(L"CLAVSubtitleConsumer", nullptr)
  , m_pLAVVideo(pLAVVideo)
{
  ZeroMemory(&context, sizeof(context));
  context.name = TEXT(LAV_VIDEO);
  context.version = TEXT(LAV_VERSION_STR);
  m_evFrame.Reset();
}

CLAVSubtitleConsumer::~CLAVSubtitleConsumer(void)
{
  if (m_pProvider) {
    m_pProvider->Disconnect();
  }
  Disconnect();
}

STDMETHODIMP CLAVSubtitleConsumer::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = nullptr;

  return
    QI(ISubRenderConsumer)
    QI(ISubRenderConsumer2)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP CLAVSubtitleConsumer::Connect(ISubRenderProvider *subtitleRenderer)
{
  SafeRelease(&m_pProvider);
  m_pProvider = subtitleRenderer;
  return S_OK;
}

STDMETHODIMP CLAVSubtitleConsumer::Disconnect(void)
{
  SafeRelease(&m_pProvider);
  if (m_pSwsContext) {
    sws_freeContext(m_pSwsContext);
    m_pSwsContext = nullptr;
  }
  return S_OK;
}

STDMETHODIMP CLAVSubtitleConsumer::DeliverFrame(REFERENCE_TIME start, REFERENCE_TIME stop, LPVOID context, ISubRenderFrame *subtitleFrame)
{
  ASSERT(m_SubtitleFrame == nullptr);
  if (subtitleFrame)
    subtitleFrame->AddRef();
  m_SubtitleFrame = subtitleFrame;
  m_evFrame.Set();

  return S_OK;
}

STDMETHODIMP CLAVSubtitleConsumer::Clear(REFERENCE_TIME clearNewerThan)
{
  m_pLAVVideo->RedrawStillImage();
  return S_OK;
}

STDMETHODIMP CLAVSubtitleConsumer::RequestFrame(REFERENCE_TIME rtStart, REFERENCE_TIME rtStop)
{
  CheckPointer(m_pProvider, E_FAIL);
  return m_pProvider->RequestFrame(rtStart, rtStop, nullptr);
}

STDMETHODIMP CLAVSubtitleConsumer::ProcessFrame(LAVFrame *pFrame)
{
  CheckPointer(m_pProvider, E_FAIL);
  HRESULT hr = S_OK;
  LPDIRECT3DSURFACE9 pSurface = nullptr;

  // Wait for the requested frame
  m_evFrame.Wait();

  if (m_SubtitleFrame != nullptr) {
    int count = 0;
    if (FAILED(m_SubtitleFrame->GetBitmapCount(&count))) {
      count = 0;
    }

    if (count == 0) {
      SafeRelease(&m_SubtitleFrame);
      return S_FALSE;
    }

    BYTE *data[4] = {0};
    ptrdiff_t stride[4] = {0};
    LAVPixelFormat format = pFrame->format;
    int bpp = pFrame->bpp;

    if (pFrame->format == LAVPixFmt_DXVA2) {
      // Copy the surface, if required
      if (!(pFrame->flags & LAV_FRAME_FLAG_BUFFER_MODIFY)) {
        IMediaSample *pOrigSample = (IMediaSample *)pFrame->data[0];
        LPDIRECT3DSURFACE9 pOrigSurface = (LPDIRECT3DSURFACE9)pFrame->data[3];

        hr = m_pLAVVideo->GetD3DBuffer(pFrame);
        if (FAILED(hr)) {
          DbgLog((LOG_TRACE, 10, L"CLAVSubtitleConsumer::ProcessFrame: getting a new D3D buffer failed"));
        } else {
          IMediaSample *pNewSample = (IMediaSample *)pFrame->data[0];
          pSurface = (LPDIRECT3DSURFACE9)pFrame->data[3];
          IDirect3DDevice9 *pDevice = nullptr;
          if (SUCCEEDED(hr = pSurface->GetDevice(&pDevice))) {
            hr = pDevice->StretchRect(pOrigSurface, nullptr, pSurface, nullptr, D3DTEXF_NONE);
            if (SUCCEEDED(hr)) {
              pFrame->flags |= LAV_FRAME_FLAG_BUFFER_MODIFY|LAV_FRAME_FLAG_DXVA_NOADDREF;
              pOrigSurface = nullptr;

              // Release the surface, we only want to hold a ref on the media buffer
              pSurface->Release();
            }
            SafeRelease(&pDevice);
          }
          if (FAILED(hr)) {
            DbgLog((LOG_TRACE, 10, L"CLAVSubtitleConsumer::ProcessFrame: processing d3d buffer failed, restoring previous buffer"));
            pNewSample->Release();
            pSurface->Release();
            pFrame->data[0] = (BYTE *)pOrigSample;
            pFrame->data[3] = (BYTE *)pOrigSurface;
          }
        }
      }
      pSurface = (LPDIRECT3DSURFACE9)pFrame->data[3];

      D3DSURFACE_DESC surfaceDesc;
      pSurface->GetDesc(&surfaceDesc);

      D3DLOCKED_RECT LockedRect;
      hr = pSurface->LockRect(&LockedRect, nullptr, 0);
      if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 10, L"pSurface->LockRect failed (hr: %X)", hr));
        SafeRelease(&m_SubtitleFrame);
        return E_FAIL;
      }

      data[0] = (BYTE *)LockedRect.pBits;
      data[1] = data[0] + (surfaceDesc.Height * LockedRect.Pitch);
      stride[0] = LockedRect.Pitch;
      stride[1] = LockedRect.Pitch;

      format = LAVPixFmt_NV12;
      bpp = 8;
    } else if (pFrame->format == LAVPixFmt_D3D11) {
      // TODO D3D11
      SafeRelease(&m_SubtitleFrame);
      return E_FAIL;
    } else {
      if (!(pFrame->flags & LAV_FRAME_FLAG_BUFFER_MODIFY)) {
        CopyLAVFrameInPlace(pFrame);
      }
      memcpy(&data, &pFrame->data, sizeof(pFrame->data));
      memcpy(&stride, &pFrame->stride, sizeof(pFrame->stride));
    }

    RECT videoRect;
    ::SetRect(&videoRect, 0, 0, pFrame->width, pFrame->height);

    RECT subRect;
    m_SubtitleFrame->GetOutputRect(&subRect);

    ULONGLONG id;
    POINT position;
    SIZE size;
    const uint8_t *rgbData;
    int pitch;
    for (int i = 0; i < count; i++) {
      if (FAILED(m_SubtitleFrame->GetBitmap(i, &id, &position, &size, (LPCVOID *)&rgbData, &pitch))) {
        DbgLog((LOG_TRACE, 10, L"GetBitmap() failed on index %d", i));
        break;
      }
      ProcessSubtitleBitmap(format, bpp, videoRect, data, stride, subRect, position, size, rgbData, pitch);
    }

    if (pSurface)
      pSurface->UnlockRect();

    SafeRelease(&m_SubtitleFrame);
    return S_OK;
  }

  return S_FALSE;
}

static struct {
  LAVPixelFormat pixfmt;
  AVPixelFormat ffpixfmt;
} lav_ff_subtitle_pixfmt_map[] = {
  { LAVPixFmt_YUV420,   AV_PIX_FMT_YUVA420P },
  { LAVPixFmt_YUV420bX, AV_PIX_FMT_YUVA420P },
  { LAVPixFmt_YUV422,   AV_PIX_FMT_YUVA422P },
  { LAVPixFmt_YUV422bX, AV_PIX_FMT_YUVA422P },
  { LAVPixFmt_YUV444,   AV_PIX_FMT_YUVA444P },
  { LAVPixFmt_YUV444bX, AV_PIX_FMT_YUVA444P },
  { LAVPixFmt_NV12,     AV_PIX_FMT_YUVA420P },
  { LAVPixFmt_P016,     AV_PIX_FMT_YUVA420P },
  { LAVPixFmt_YUY2,     AV_PIX_FMT_YUVA422P },
  { LAVPixFmt_RGB24,    AV_PIX_FMT_BGRA     },
  { LAVPixFmt_RGB32,    AV_PIX_FMT_BGRA     },
  { LAVPixFmt_ARGB32,   AV_PIX_FMT_BGRA     },
};

static LAVPixFmtDesc ff_sub_pixfmt_desc[] = {
  { 1, 4, { 1, 2, 2, 1 }, { 1, 2, 2, 1 } }, ///< PIX_FMT_YUVA420P
  { 1, 4, { 1, 2, 2, 1 }, { 1, 1, 1, 1 } }, ///< PIX_FMT_YUVA422P
  { 1, 4, { 1, 1, 1, 1 }, { 1, 1, 1, 1 } }, ///< PIX_FMT_YUVA444P
  { 4, 1, { 1 },          { 1 }          }, ///< PIX_FMT_BGRA
};

static LAVPixFmtDesc getFFSubPixelFormatDesc(AVPixelFormat pixFmt)
{
  int index = 0;
  switch(pixFmt) {
  case AV_PIX_FMT_YUVA420P:
    index = 0;
    break;
  case AV_PIX_FMT_YUVA422P:
    index = 1;
    break;
  case AV_PIX_FMT_YUVA444P:
    index = 2;
    break;
  case AV_PIX_FMT_BGRA:
    index = 3;
    break;
  default:
    ASSERT(0);
  }
  return ff_sub_pixfmt_desc[index];
}

static AVPixelFormat getFFPixFmtForSubtitle(LAVPixelFormat pixFmt)
{
  AVPixelFormat fmt = AV_PIX_FMT_NONE;
  for(int i = 0; i < countof(lav_ff_subtitle_pixfmt_map); i++) {
    if (lav_ff_subtitle_pixfmt_map[i].pixfmt == pixFmt) {
      return lav_ff_subtitle_pixfmt_map[i].ffpixfmt;
    }
  }
  ASSERT(0);
  return AV_PIX_FMT_NONE;
}

STDMETHODIMP CLAVSubtitleConsumer::SelectBlendFunction()
{
  switch (m_PixFmt) {
  case LAVPixFmt_RGB32:
  case LAVPixFmt_RGB24:
    blend = &CLAVSubtitleConsumer::blend_rgb_c;
    break;
  case LAVPixFmt_NV12:
    blend = &CLAVSubtitleConsumer::blend_yuv_c<uint8_t,1>;
    break;
  case LAVPixFmt_P016:
    blend = &CLAVSubtitleConsumer::blend_yuv_c<uint16_t, 1>;
    break;
  case LAVPixFmt_YUV420:
  case LAVPixFmt_YUV422:
  case LAVPixFmt_YUV444:
    blend = &CLAVSubtitleConsumer::blend_yuv_c<uint8_t,0>;
    break;
  case LAVPixFmt_YUV420bX:
  case LAVPixFmt_YUV422bX:
  case LAVPixFmt_YUV444bX:
    blend = &CLAVSubtitleConsumer::blend_yuv_c<uint16_t,0>;
    break;
  default:
    DbgLog((LOG_ERROR, 10, L"ProcessSubtitleBitmap(): No Blend function available"));
    blend = nullptr;
  }

  return S_OK;
}

STDMETHODIMP CLAVSubtitleConsumer::ProcessSubtitleBitmap(LAVPixelFormat pixFmt, int bpp, RECT videoRect, BYTE *videoData[4], ptrdiff_t videoStride[4], RECT subRect, POINT subPosition, SIZE subSize, const uint8_t *rgbData, ptrdiff_t pitch)
{
  if (subRect.left != 0 || subRect.top != 0) {
    DbgLog((LOG_ERROR, 10, L"ProcessSubtitleBitmap(): Left/Top in SubRect non-zero"));
  }

  BOOL bNeedScaling = FALSE;

  // We need scaling if the width is not the same, or the subtitle rect is higher then the video rect
  if (subRect.right != videoRect.right || subRect.bottom > videoRect.bottom) {
    bNeedScaling = TRUE;
  }

  if (pixFmt != LAVPixFmt_RGB32 && pixFmt != LAVPixFmt_RGB24) {
    bNeedScaling = TRUE;
  }

  if (m_PixFmt != pixFmt) {
    m_PixFmt = pixFmt;
    SelectBlendFunction();
  }

  // P010/P016 is always handled like its 16 bpp to compensate for having the data in the high bits
  if (pixFmt == LAVPixFmt_P016)
    bpp = 16;

  BYTE *subData[4] = { nullptr, nullptr, nullptr, nullptr };
  ptrdiff_t subStride[4] = { 0, 0, 0, 0 };

  // If we need scaling (either scaling or pixel conversion), do it here before starting the blend process
  if (bNeedScaling) {
    uint8_t *tmpBuf = nullptr;
    const AVPixelFormat avPixFmt = getFFPixFmtForSubtitle(pixFmt);

    // Calculate scaled size
    // We must ensure that the scaled subs still fit into the video

    // HACK: Scale to video size. In the future, we should take AR and the likes into account
    RECT newRect = videoRect;
    /*
    float subAR = (float)subRect.right / (float)subRect.bottom;
    if (newRect.right != videoRect.right) {
      newRect.right = videoRect.right;
      newRect.bottom = (LONG)(newRect.right / subAR);
    }
    if (newRect.bottom > videoRect.bottom) {
      newRect.bottom = videoRect.bottom;
      newRect.right = (LONG)(newRect.bottom * subAR);
    }*/

    SIZE newSize;
    newSize.cx = (LONG)av_rescale(subSize.cx, newRect.right, subRect.right);
    newSize.cy = (LONG)av_rescale(subSize.cy, newRect.bottom, subRect.bottom);

    // And scaled position
    subPosition.x = (LONG)av_rescale(subPosition.x, newSize.cx, subSize.cx);
    subPosition.y = (LONG)av_rescale(subPosition.y, newSize.cy, subSize.cy);

    m_pSwsContext = sws_getCachedContext(m_pSwsContext, subSize.cx, subSize.cy, AV_PIX_FMT_BGRA, newSize.cx, newSize.cy, avPixFmt, SWS_BILINEAR|SWS_FULL_CHR_H_INP, nullptr, nullptr, nullptr);

    const uint8_t *src[4] = { (const uint8_t *)rgbData, nullptr, nullptr, nullptr };
    const ptrdiff_t srcStride[4] = { pitch, 0, 0, 0 };

    const LAVPixFmtDesc desc = getFFSubPixelFormatDesc(avPixFmt);
    const ptrdiff_t stride = FFALIGN(newSize.cx, 64) * desc.codedbytes;

    for (int plane = 0; plane < desc.planes; plane++) {
      subStride[plane]  = stride / desc.planeWidth[plane];
      const size_t size = subStride[plane] * FFALIGN(newSize.cy, 2) / desc.planeHeight[plane];
      subData[plane]    = (BYTE *)av_mallocz(size + AV_INPUT_BUFFER_PADDING_SIZE);
      if (subData[plane] == nullptr)
        goto fail;
    }

    // Un-pre-multiply alpha for YUV formats
    // TODO: Can we SIMD this? See ARGBUnattenuateRow_C/SSE2 in libyuv
    if (avPixFmt != AV_PIX_FMT_BGRA) {
      tmpBuf = (uint8_t *)av_malloc(pitch * subSize.cy);
      if (tmpBuf == nullptr)
        goto fail;

      memcpy(tmpBuf, rgbData, pitch * subSize.cy);
      for (int line = 0; line < subSize.cy; line++) {
        uint8_t *p = tmpBuf + line * pitch;
        for (int col = 0; col < subSize.cx; col++) {
          if (p[3] != 0 && p[3] != 255) {
            p[0] = av_clip_uint8(p[0] * 255 / p[3]);
            p[1] = av_clip_uint8(p[1] * 255 / p[3]);
            p[2] = av_clip_uint8(p[2] * 255 / p[3]);
          }
          p += 4;
        }
      }
      src[0] = tmpBuf;
    }

    int ret = sws_scale2(m_pSwsContext, src, srcStride, 0, subSize.cy, subData, subStride);
    subSize = newSize;

    if (tmpBuf)
      av_free(tmpBuf);
  } else {
    subData[0] = (BYTE *)rgbData;
    subStride[0] = pitch;
  }

  ASSERT((subPosition.x + subSize.cx) <= videoRect.right);
  ASSERT((subPosition.y + subSize.cy) <= videoRect.bottom);

  if (blend)
    (this->*blend)(videoData, videoStride, videoRect, subData, subStride, subPosition, subSize, pixFmt, bpp);

  if (bNeedScaling) {
    for (int i = 0; i < 4; i++) {
      av_freep(&subData[i]);
    }
  }

  return S_OK;

fail:
  for (int i = 0; i < 4; i++) {
    av_freep(&subData[i]);
  }
  return E_OUTOFMEMORY;
}
