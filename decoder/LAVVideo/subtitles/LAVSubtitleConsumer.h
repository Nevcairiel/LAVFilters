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

#pragma once

#include "SubRenderOptionsImpl.h"
#include "LAVSubtitleFrame.h"

#include "../decoders/ILAVDecoder.h"

#define BLEND_FUNC_PARAMS (BYTE* video[4], ptrdiff_t videoStride[4], RECT vidRect, BYTE* subData[4], ptrdiff_t subStride[4], POINT position, SIZE size, LAVPixelFormat pixFmt, int bpp)

#define DECLARE_BLEND_FUNC(name) \
  HRESULT name BLEND_FUNC_PARAMS

#define DECLARE_BLEND_FUNC_IMPL(name) \
  DECLARE_BLEND_FUNC(CLAVSubtitleConsumer::name)

typedef struct LAVSubtitleConsumerContext {
  LPWSTR name;                    ///< name of the Consumer
  LPWSTR version;                 ///< Version of the Consumer
  SIZE   originalVideoSize;       ///< Size of the video
} LAVSubtitleConsumerContext;

class CLAVVideo;

class CLAVSubtitleConsumer : public ISubRenderConsumer2, public CSubRenderOptionsImpl, public CUnknown
{
public:
  CLAVSubtitleConsumer(CLAVVideo *pLAVVideo);
  virtual ~CLAVSubtitleConsumer(void);
  DECLARE_IUNKNOWN;
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  DECLARE_ISUBRENDEROPTIONS;

  // ISubRenderConsumer2
  STDMETHODIMP GetMerit(ULONG *merit) { CheckPointer(merit, E_POINTER); *merit = 0x00010000; return S_OK; }
  STDMETHODIMP Connect(ISubRenderProvider *subtitleRenderer);
  STDMETHODIMP Disconnect(void);
  STDMETHODIMP DeliverFrame(REFERENCE_TIME start, REFERENCE_TIME stop, LPVOID context, ISubRenderFrame *subtitleFrame);
  STDMETHODIMP Clear(REFERENCE_TIME clearNewerThan = 0);

  // LAV Internal methods
  STDMETHODIMP RequestFrame(REFERENCE_TIME rtStart, REFERENCE_TIME rtStop);
  STDMETHODIMP ProcessFrame(LAVFrame *pFrame);

  STDMETHODIMP DisconnectProvider() { if (m_pProvider) m_pProvider->Disconnect(); SafeRelease(&m_pProvider); return S_OK; }

  BOOL HasProvider() const { return m_pProvider != nullptr; }

  void SetVideoSize(LONG w, LONG h) { context.originalVideoSize.cx = w; context.originalVideoSize.cy = h; }

private:
  STDMETHODIMP ProcessSubtitleBitmap(LAVPixelFormat pixFmt, int bpp, RECT videoRect, BYTE *videoData[4], ptrdiff_t videoStride[4], RECT subRect, POINT subPosition, SIZE subSize, const uint8_t *rgbData, ptrdiff_t pitch);

  STDMETHODIMP SelectBlendFunction();
  typedef HRESULT (CLAVSubtitleConsumer::*BlendFn) BLEND_FUNC_PARAMS;
  BlendFn blend = nullptr;

  DECLARE_BLEND_FUNC(blend_rgb_c);
  template <class pixT, int nv12> DECLARE_BLEND_FUNC(blend_yuv_c);

private:
  ISubRenderProvider *m_pProvider     = nullptr;
  ISubRenderFrame    *m_SubtitleFrame = nullptr;
  CAMEvent           m_evFrame{FALSE};

  SwsContext         *m_pSwsContext   = nullptr;
  LAVPixelFormat     m_PixFmt         = LAVPixFmt_None;

  LAVSubtitleConsumerContext context;

  CLAVVideo          *m_pLAVVideo     = nullptr;
};
