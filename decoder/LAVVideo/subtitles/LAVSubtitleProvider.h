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

class CLAVVideo;

typedef struct LAVSubtitleProviderContext {
  LPWSTR name;                    ///< name of the Provider
  LPWSTR version;                 ///< Version of the Provider
  LPWSTR yuvMatrix;               ///< YUV Matrix

  bool combineBitmaps;            ///< Control if the provider combines all bitmaps into one
  bool isBitmap;
  bool isMovable;
} LAVSubtitleProviderContext;

struct _AM_PROPERTY_SPPAL;
struct _AM_PROPERTY_SPHLI;

class CLAVSubtitleProviderControlThread : public CAMThread, protected CCritSec
{
public:
  CLAVSubtitleProviderControlThread();
  ~CLAVSubtitleProviderControlThread();

  void SetConsumer2(ISubRenderConsumer2 * pConsumer2);

protected:
  DWORD ThreadProc();

private:
  ISubRenderConsumer2 *m_pConsumer2 = nullptr;
};

class CLAVSubtitleProvider : public ISubRenderProvider, public CSubRenderOptionsImpl, public CUnknown, private CCritSec
{
public:
  CLAVSubtitleProvider(CLAVVideo *pLAVVideo, ISubRenderConsumer *pConsumer);
  ~CLAVSubtitleProvider(void);
  DECLARE_IUNKNOWN;
  DECLARE_ISUBRENDEROPTIONS;

  // ISubRenderProvider
  STDMETHODIMP RequestFrame(REFERENCE_TIME start, REFERENCE_TIME stop, LPVOID context);
  STDMETHODIMP Disconnect(void);

  // CLAVSubtitleProvider public
  STDMETHODIMP SetConsumer(ISubRenderConsumer *pConsumer);
  STDMETHODIMP DisconnectConsumer(void);

  STDMETHODIMP InitDecoder(const CMediaType *pmt, AVCodecID codecId);
  STDMETHODIMP Decode(BYTE *buf, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop);

  STDMETHODIMP Flush();

  STDMETHODIMP SetDVDPalette(struct _AM_PROPERTY_SPPAL *pPal);
  STDMETHODIMP SetDVDHLI(struct _AM_PROPERTY_SPHLI *pHLI);
  STDMETHODIMP SetDVDComposit(BOOL bComposit);

private:
  void CloseDecoder();

  void ProcessSubtitleFrame(AVSubtitle *sub, REFERENCE_TIME rtStart);
  void ProcessSubtitleRect(AVSubtitleRect *rect, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop);
  void AddSubtitleRect(CLAVSubRect *rect);
  CLAVSubRect* ProcessDVDHLI(CLAVSubRect *rect);
  void ClearSubtitleRects();
  void TimeoutSubtitleRects(REFERENCE_TIME rtStop);

  enum { CNTRL_EXIT, CNTRL_FLUSH };
  HRESULT ControlCmd(DWORD cmd) {
    return m_ControlThread->CallWorker(cmd);
  }

private:
  friend class CLAVSubtitleProviderControlThread;
  LAVSubtitleProviderContext context;
  CLAVVideo *m_pLAVVideo            = nullptr;

  ISubRenderConsumer  *m_pConsumer  = nullptr;
  ISubRenderConsumer2 *m_pConsumer2 = nullptr;

  const AVCodec        *m_pAVCodec  = nullptr;
  AVCodecContext       *m_pAVCtx    = nullptr;
  AVCodecParserContext *m_pParser   = nullptr;

  REFERENCE_TIME        m_rtLastFrame  = AV_NOPTS_VALUE;
  REFERENCE_TIME        m_rtStartCache = AV_NOPTS_VALUE;
  ULONGLONG             m_SubPicId     = 0;
  BOOL                  m_bComposit    = TRUE;

  std::list<CLAVSubRect *> m_SubFrames;

  struct _AM_PROPERTY_SPHLI *m_pHLI = nullptr;

  CLAVSubtitleProviderControlThread *m_ControlThread = nullptr;
};
