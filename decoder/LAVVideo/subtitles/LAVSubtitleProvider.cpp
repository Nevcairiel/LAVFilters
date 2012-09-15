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
{
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
  CheckPointer(m_pConsumer, S_FALSE);
  m_pConsumer->Disconnect();
  SafeRelease(&m_pConsumer);

  return S_OK;
}

STDMETHODIMP CLAVSubtitleProvider::RequestFrame(REFERENCE_TIME start, REFERENCE_TIME stop)
{
  ASSERT(m_pConsumer);
  return S_OK;
}

STDMETHODIMP CLAVSubtitleProvider::Disconnect(void)
{
  SafeRelease(&m_pConsumer);
  return S_OK;
}

STDMETHODIMP CLAVSubtitleProvider::InitDecoder(const CMediaType *pmt, AVCodecID codecId)
{
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

STDMETHODIMP CLAVSubtitleProvider::Decode(IMediaSample *pSample)
{
  ASSERT(m_pAVCtx);
  return E_NOTIMPL;
}
