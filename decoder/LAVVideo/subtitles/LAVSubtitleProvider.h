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

#pragma once

#include "SubRenderOptionsImpl.h"
#include "LAVSubtitleFrame.h"

typedef struct LAVSubtitleProviderContext {
  LPWSTR name;                    ///< name of the Provider
  LPWSTR version;                 ///< Version of the Provider

  bool combineBitmaps;            ///< Control if the provider combines all bitmaps into one
} LAVSubtitleProviderContext;

class CLAVSubtitleProvider : public ISubRenderProvider, public CSubRenderOptionsImpl, public CUnknown, private CCritSec
{
public:
  CLAVSubtitleProvider(ISubRenderConsumer *pConsumer);
  ~CLAVSubtitleProvider(void);
  DECLARE_IUNKNOWN;
  DECLARE_ISUBRENDEROPTIONS;

  // ISubRenderProvider
  STDMETHODIMP RequestFrame(REFERENCE_TIME start, REFERENCE_TIME stop);
  STDMETHODIMP Disconnect(void);

  // CLAVSubtitleProvider public
  STDMETHODIMP DisconnectConsumer(void);

  STDMETHODIMP InitDecoder(const CMediaType *pmt, AVCodecID codecId);
  STDMETHODIMP Decode(BYTE *buf, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop);

  STDMETHODIMP Flush();

private:
  void CloseDecoder();

  void ProcessSubtitleRect(AVSubtitle *sub, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop);
  void AddSubtitleRect(LAVSubRect *rect);

private:
  LAVSubtitleProviderContext context;

  ISubRenderConsumer *m_pConsumer;

  const AVCodec        *m_pAVCodec;
  AVCodecContext       *m_pAVCtx;
  AVCodecParserContext *m_pParser;

  REFERENCE_TIME        m_rtStartCache;
  ULONGLONG             m_SubPicId;

  std::list<LAVSubRect*> m_SubFrames;
};
