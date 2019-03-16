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

#include "DecBase.h"

#include <map>

#define AVCODEC_MAX_THREADS 32

typedef struct {
  REFERENCE_TIME rtStart;
  REFERENCE_TIME rtStop;
} TimingCache;

class CDecAvcodec : public CDecBase
{
public:
  CDecAvcodec(void);
  virtual ~CDecAvcodec(void);

  // ILAVDecoder
  STDMETHODIMP InitDecoder(AVCodecID codec, const CMediaType *pmt);
  STDMETHODIMP Decode(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity, IMediaSample *pSample);
  STDMETHODIMP Flush();
  STDMETHODIMP EndOfStream();
  STDMETHODIMP GetPixelFormat(LAVPixelFormat *pPix, int *pBpp);
  STDMETHODIMP_(REFERENCE_TIME) GetFrameDuration();
  STDMETHODIMP_(BOOL) IsInterlaced(BOOL bAllowGuess);
  STDMETHODIMP_(const WCHAR*) GetDecoderName() { return L"avcodec"; }
  STDMETHODIMP HasThreadSafeBuffers() { return S_OK; }

  // CDecBase
  STDMETHODIMP Init();

protected:
  virtual HRESULT AdditionaDecoderInit() { return S_FALSE; }
  virtual HRESULT PostDecode() { return S_FALSE; }
  virtual HRESULT HandleDXVA2Frame(LAVFrame *pFrame) { return S_FALSE; }
  STDMETHODIMP DestroyDecoder();

  STDMETHODIMP FillAVPacketData(AVPacket *avpkt, const BYTE *buffer, int buflen, IMediaSample *pSample, bool bRefCounting);
  STDMETHODIMP DecodePacket(AVPacket *avpkt, REFERENCE_TIME rtStartIn, REFERENCE_TIME rtStopIn);
  STDMETHODIMP ParsePacket(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, IMediaSample *pSample);

private:
  STDMETHODIMP ConvertPixFmt(AVFrame *pFrame, LAVFrame *pOutFrame);

protected:
  AVCodecContext       *m_pAVCtx   = nullptr;
  AVFrame              *m_pFrame   = nullptr;
  AVCodecID             m_nCodecId = AV_CODEC_ID_NONE;
  BOOL                  m_bInInit  = FALSE;

private:
  AVCodec              *m_pAVCodec   = nullptr;
  AVCodecParserContext *m_pParser    = nullptr;

  BYTE                 *m_pFFBuffer     = nullptr;
  unsigned int         m_nFFBufferSize  = 0;

  SwsContext           *m_pSwsContext   = nullptr;

  BOOL                 m_bHasPalette    = FALSE;

  // Timing settings
  BOOL                 m_bFFReordering        = FALSE;
  BOOL                 m_bCalculateStopTime   = FALSE;
  BOOL                 m_bRVDropBFrameTimings = FALSE;
  BOOL                 m_bInputPadded         = FALSE;

  BOOL                 m_bBFrameDelay         = TRUE;
  TimingCache          m_tcBFrameDelay[2];
  int                  m_nBFramePos           = 0;

  TimingCache          m_tcThreadBuffer[AVCODEC_MAX_THREADS];
  int                  m_CurrentThread        = 0;

  REFERENCE_TIME       m_rtStartCache         = AV_NOPTS_VALUE;
  BOOL                 m_bResumeAtKeyFrame    = FALSE;
  BOOL                 m_bWaitingForKeyFrame  = FALSE;
  int                  m_iInterlaced          = -1;
  int                  m_nSoftTelecine        = 0;
};
