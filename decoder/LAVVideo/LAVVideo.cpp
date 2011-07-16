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
#include "LAVVideo.h"
#include "Media.h"
#include <dvdmedia.h>

#include "moreuuids.h"

// static constructor
CUnknown* WINAPI CLAVVideo::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr)
{
  return new CLAVVideo(pUnk, phr);
}

CLAVVideo::CLAVVideo(LPUNKNOWN pUnk, HRESULT* phr)
  : CTransformFilter(NAME("LAV Video Decoder"), 0, __uuidof(CLAVVideo))
  , m_bDXVA(FALSE)
  , m_nCodecId(CODEC_ID_NONE)
  , m_pAVCodec(NULL)
  , m_pAVCtx(NULL)
  , m_rtAvrTimePerFrame(0)
  , m_pFrame(NULL)
  , m_pFFBuffer(NULL)
  , m_nFFBufferSize(0)
  , m_pSwsContext(NULL)
  , m_bProcessExtradata(FALSE)
  , m_bReorderBFrame(TRUE)
  , m_nPosB(1)
  , m_bH264OnMPEG2(NULL)
  , m_rtPrevStart(0)
  , m_rtPrevStop(0)
  , m_bDiscontinuity(FALSE)
  , m_nThreads(1)
{
  avcodec_init();
  avcodec_register_all();

#ifdef DEBUG
  DbgSetModuleLevel (LOG_TRACE, DWORD_MAX);
  DbgSetModuleLevel (LOG_CUSTOM1, DWORD_MAX); // FFMPEG messages use custom1
#endif
}

CLAVVideo::~CLAVVideo()
{
  av_free(m_pFFBuffer);
}

void CLAVVideo::ffmpeg_shutdown()
{
  m_pAVCodec	= NULL;
  if (m_pAVCtx) {
    avcodec_close(m_pAVCtx);
    av_free(m_pAVCtx->extradata);
    av_free(m_pAVCtx);
    m_pAVCtx = NULL;
  }
  if (m_pFrame) {
    av_free(m_pFrame);
    m_pFrame = NULL;
  }

  m_nCodecId = CODEC_ID_NONE;
}

// CTransformFilter
HRESULT CLAVVideo::CheckInputType(const CMediaType *mtIn)
{
  for(int i = 0; i < sudPinTypesInCount; i++) {
    if(*sudPinTypesIn[i].clsMajorType == mtIn->majortype
      && *sudPinTypesIn[i].clsMinorType == mtIn->subtype && (mtIn->formattype == FORMAT_VideoInfo || mtIn->formattype == FORMAT_VideoInfo2 || mtIn->formattype == FORMAT_MPEG2Video)) {
        return S_OK;
    }
  }
  return VFW_E_TYPE_NOT_ACCEPTED;
}

// Check if the types are compatible
HRESULT CLAVVideo::CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut)
{
  if (SUCCEEDED(CheckInputType(mtIn)) && mtOut->majortype == MEDIATYPE_Video) {
    for(int i = 0; i < sudPinTypesOutCount; i++) {
      if(*sudPinTypesOut[i].clsMinorType == mtOut->subtype) {
        return S_OK;
      }
    }
  }
  return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CLAVVideo::DecideBufferSize(IMemAllocator* pAllocator, ALLOCATOR_PROPERTIES* pProperties)
{
  if(m_pInput->IsConnected() == FALSE) {
    return E_UNEXPECTED;
  }

  BITMAPINFOHEADER *pBIH = NULL;
  CMediaType mtOut = m_pOutput->CurrentMediaType();
  formatTypeHandler(mtOut.Format(), mtOut.FormatType(), &pBIH, NULL);

  pProperties->cBuffers = 1;
  pProperties->cbBuffer = pBIH->biSizeImage;
  pProperties->cbAlign  = 1;
  pProperties->cbPrefix = 0;

  HRESULT hr;
  ALLOCATOR_PROPERTIES Actual;
  if(FAILED(hr = pAllocator->SetProperties(pProperties, &Actual))) {
    return hr;
  }

  return pProperties->cBuffers > Actual.cBuffers || pProperties->cbBuffer > Actual.cbBuffer
    ? E_FAIL : S_OK;
}

HRESULT CLAVVideo::GetMediaType(int iPosition, CMediaType *pMediaType)
{
  if(m_pInput->IsConnected() == FALSE) {
    return E_UNEXPECTED;
  }
  if(iPosition < 0) {
    return E_INVALIDARG;
  }

  if(iPosition > 0) {
    return VFW_S_NO_MORE_ITEMS;
  }

  CMediaType mtIn = m_pInput->CurrentMediaType();

  BITMAPINFOHEADER *pBIH = NULL;
  REFERENCE_TIME rtAvgTime;
  DWORD dwAspectX = 0, dwAspectY = 0;
  formatTypeHandler(mtIn.Format(), mtIn.FormatType(), &pBIH, &rtAvgTime, &dwAspectX, &dwAspectY);

  *pMediaType = CreateMediaType(pBIH->biWidth, pBIH->biHeight, dwAspectX, dwAspectY, rtAvgTime);

  return S_OK;
}

HRESULT CLAVVideo::ffmpeg_init(CodecID codec, const CMediaType *pmt)
{
  ffmpeg_shutdown();

  BITMAPINFOHEADER *pBMI = NULL;
  formatTypeHandler((const BYTE *)pmt->Format(), pmt->FormatType(), &pBMI, &m_rtAvrTimePerFrame);

  m_pAVCodec    = avcodec_find_decoder(codec);
  CheckPointer(m_pAVCodec, VFW_E_UNSUPPORTED_VIDEO);

  m_pAVCtx = avcodec_alloc_context3(m_pAVCodec);
  CheckPointer(m_pAVCtx, E_POINTER);

  m_pAVCtx->codec_type            = AVMEDIA_TYPE_VIDEO;
  m_pAVCtx->codec_id              = (CodecID)codec;
  m_pAVCtx->codec_tag             = pBMI->biCompression;
  
  /*if(m_pAVCodec->capabilities & CODEC_CAP_TRUNCATED)
    m_pAVCtx->flags |= CODEC_FLAG_TRUNCATED;   */

  m_pAVCtx->width = pBMI->biWidth;
  m_pAVCtx->height = pBMI->biHeight;
  m_pAVCtx->codec_tag = pBMI->biCompression;
  m_pAVCtx->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  m_pAVCtx->error_recognition = FF_ER_CAREFUL;
  m_pAVCtx->workaround_bugs = FF_BUG_AUTODETECT;

  m_pFrame = avcodec_alloc_frame();
  CheckPointer(m_pFrame, E_POINTER);

  m_bReorderBFrame = TRUE;

  BYTE *extra = NULL;
  unsigned int extralen = 0;

  getExtraData((const BYTE *)pmt->Format(), pmt->FormatType(), NULL, &extralen);

  if (extralen > 0) {
    // Reconstruct AVC1 extradata format
    if (pmt->formattype == FORMAT_MPEG2Video && (m_pAVCtx->codec_tag == MAKEFOURCC('a','v','c','1') || m_pAVCtx->codec_tag == MAKEFOURCC('A','V','C','1') || m_pAVCtx->codec_tag == MAKEFOURCC('C','C','V','1'))) {
      MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)pmt->Format();
      extralen += 7;
      extra = (uint8_t *)av_mallocz(extralen + FF_INPUT_BUFFER_PADDING_SIZE);
      extra[0] = 1;
      extra[1] = (BYTE)mp2vi->dwProfile;
      extra[2] = 0;
      extra[3] = (BYTE)mp2vi->dwLevel;
      extra[4] = (BYTE)(mp2vi->dwFlags ? mp2vi->dwFlags : 2) - 1;

      // Actually copy the metadata into our new buffer
      unsigned int actual_len;
      getExtraData((const BYTE *)pmt->Format(), pmt->FormatType(), extra+6, &actual_len);

      // Count the number of SPS/PPS in them and set the length
      // We'll put them all into one block and add a second block with 0 elements afterwards
      // The parsing logic does not care what type they are, it just expects 2 blocks.
      BYTE *p = extra+6, *end = extra+6+actual_len;
      int count = 0;
      while (p+1 < end) {
        unsigned len = (((unsigned)p[0] << 8) | p[1]) + 2;
        if (p + len > end) {
          break;
        }
        count++;
        p += len;
      }
      extra[5] = count;
      extra[extralen-1] = 0;

    } else {
      // Just copy extradata for other formats
      extra = (uint8_t *)av_mallocz(extralen + FF_INPUT_BUFFER_PADDING_SIZE);
      getExtraData((const BYTE *)pmt->Format(), pmt->FormatType(), extra, NULL);
    }
    m_pAVCtx->extradata = extra;
    m_pAVCtx->extradata_size = extralen;

    m_bProcessExtradata = (codec == CODEC_ID_MPEG2VIDEO);
  }

  m_nCodecId                      = codec;

  m_bH264OnMPEG2 = false;
  if (codec == CODEC_ID_H264 && !(pmt->subtype == MEDIASUBTYPE_AVC1 || pmt->subtype == MEDIASUBTYPE_avc1 || pmt->subtype == MEDIASUBTYPE_CCV1)) {
    m_bH264OnMPEG2 = true;
  }

  m_bReorderWithoutStop = (pmt->formattype == FORMAT_MPEG2Video || codec == CODEC_ID_VC1 );

  int ret = avcodec_open2(m_pAVCtx, m_pAVCodec, NULL);
  if (ret >= 0) {

  } else {
    return VFW_E_UNSUPPORTED_VIDEO;
  }

  return S_OK;
}

HRESULT CLAVVideo::swscale_init()
{
  if (m_pSwsContext != NULL)
    return S_OK;

  m_pSwsContext = sws_getContext(m_pAVCtx->width, m_pAVCtx->height, m_pAVCtx->pix_fmt,
                                 m_pAVCtx->width, m_pAVCtx->height, PIX_FMT_NV12,
                                 SWS_POINT|SWS_PRINT_INFO, NULL, NULL, NULL);
  CheckPointer(m_pSwsContext, E_POINTER);

  CMediaType mtOut = m_pOutput->CurrentMediaType();
  BITMAPINFOHEADER *pBIH = NULL;
  formatTypeHandler(mtOut.Format(), mtOut.FormatType(), &pBIH);

  m_pOutSize.cx = pBIH->biWidth;
  m_pOutSize.cy = abs(pBIH->biHeight);

  return S_OK;
}

HRESULT CLAVVideo::SetMediaType(PIN_DIRECTION dir, const CMediaType *pmt)
{
  HRESULT hr = S_OK;
  DbgLog((LOG_TRACE, 5, L"SetMediaType -- %S", dir == PINDIR_INPUT ? "in" : "out"));
  if (dir == PINDIR_INPUT) {
    CodecID codec = CODEC_ID_NONE;
    const void *format = pmt->Format();
    GUID format_type = pmt->formattype;

    codec = FindCodecId(pmt);

    if (codec == CODEC_ID_NONE) {
      return VFW_E_TYPE_NOT_ACCEPTED;
    }

    hr = ffmpeg_init(codec, pmt);
    if (FAILED(hr)) {
      return hr;
    }
  }
  return __super::SetMediaType(dir, pmt);
}

HRESULT CLAVVideo::EndOfStream()
{
  DbgLog((LOG_TRACE, 1, L"EndOfStream"));
  CAutoLock cAutoLock(&m_csReceive);
  return __super::EndOfStream();
}

HRESULT CLAVVideo::BeginFlush()
{
  DbgLog((LOG_TRACE, 1, L"BeginFlush"));
  return __super::BeginFlush();
}

HRESULT CLAVVideo::EndFlush()
{
  DbgLog((LOG_TRACE, 1, L"EndFlush"));
  m_rtPrevStop = 0;
  m_nPosB = 1;
  return __super::EndFlush();
}

HRESULT CLAVVideo::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
  DbgLog((LOG_TRACE, 1, L"NewSegment - %d / %d", tStart, tStop));
  CAutoLock cAutoLock(&m_csReceive);

  m_nPosB = 1;
  for (int pos = 0 ; pos < countof(m_BFrames) ; pos++) {
    m_BFrames[pos].rtStart = AV_NOPTS_VALUE;
    m_BFrames[pos].rtStop = AV_NOPTS_VALUE;
  }

  if (m_pAVCtx) {
    avcodec_flush_buffers (m_pAVCtx);
  }

  return __super::NewSegment(tStart, tStop, dRate);
}

HRESULT CLAVVideo::GetDeliveryBuffer(int w, int h, IMediaSample** ppOut)
{
  CheckPointer(ppOut, E_POINTER);

  HRESULT hr;

  /*if(FAILED(hr = ReconnectOutput(w, h))) {
    return hr;
  } */

  if(FAILED(hr = m_pOutput->GetDeliveryBuffer(ppOut, NULL, NULL, 0))) {
    return hr;
  }

  AM_MEDIA_TYPE* pmt;
  if(SUCCEEDED((*ppOut)->GetMediaType(&pmt)) && pmt) {
    CMediaType mt = *pmt;
    m_pOutput->SetMediaType(&mt);
    DeleteMediaType(pmt);
  }

  (*ppOut)->SetDiscontinuity(FALSE);
  (*ppOut)->SetSyncPoint(TRUE);

  return S_OK;
}

HRESULT CLAVVideo::Receive(IMediaSample *pIn)
{
  CAutoLock cAutoLock(&m_csReceive);
  HRESULT        hr = S_OK;
  BYTE           *pDataIn = NULL;
  int            nSize;
  REFERENCE_TIME rtStart = AV_NOPTS_VALUE;
  REFERENCE_TIME rtStop = AV_NOPTS_VALUE;

  AM_SAMPLE2_PROPERTIES const *pProps = m_pInput->SampleProps();
  if(pProps->dwStreamId != AM_STREAM_MEDIA) {
    return m_pOutput->Deliver(pIn);
  }

  if (FAILED(hr = pIn->GetPointer(&pDataIn))) {
    return hr;
  }

  nSize = pIn->GetActualDataLength();
  hr = pIn->GetTime(&rtStart, &rtStop);

  if (FAILED(hr)) {
    rtStart = rtStop = AV_NOPTS_VALUE;
  }

  if (rtStop-1 <= rtStart) {
    rtStop = AV_NOPTS_VALUE;
  }

  m_BFrames[m_nPosB].rtStart = rtStart;
  m_BFrames[m_nPosB].rtStop  = rtStop;
  m_nPosB++;
  if (m_nPosB >= (m_nThreads+1)) {
    m_nPosB = 0;
  }

  if (pIn->IsDiscontinuity() == S_OK) {
    m_bDiscontinuity = TRUE;
  }

  hr = Decode(pIn, pDataIn, nSize, rtStart, rtStop);

  return hr;
}

HRESULT CLAVVideo::Decode(IMediaSample *pIn, const BYTE *pDataIn, int nSize, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop)
{
  HRESULT hr = S_OK;
  int     got_picture;
  int     used_bytes;

  IMediaSample *pOut = NULL;
  BYTE         *pDataOut = NULL;

  uint8_t *dst[4];
  int     srcStride[4];
  int     dstStride[4];

  AVPacket avpkt;
  av_init_packet(&avpkt);

  if (m_bProcessExtradata && m_pAVCtx->extradata && m_pAVCtx->extradata_size > 0) {
    avpkt.data = m_pAVCtx->extradata;
    avpkt.size = m_pAVCtx->extradata_size;
    used_bytes = avcodec_decode_video2 (m_pAVCtx, m_pFrame, &got_picture, &avpkt);
    av_free(m_pAVCtx->extradata);
    m_pAVCtx->extradata = NULL;
    m_pAVCtx->extradata_size = 0;
  }

  while (nSize > 0) {
    if (nSize+FF_INPUT_BUFFER_PADDING_SIZE > m_nFFBufferSize) {
      m_nFFBufferSize	= nSize + FF_INPUT_BUFFER_PADDING_SIZE;
      m_pFFBuffer = (BYTE *)av_realloc(m_pFFBuffer, m_nFFBufferSize);
    }

    memcpy(m_pFFBuffer, pDataIn, nSize);
    memset(m_pFFBuffer+nSize, 0, FF_INPUT_BUFFER_PADDING_SIZE);

    avpkt.data = m_pFFBuffer;
    avpkt.size = nSize;
    avpkt.pts = rtStart;
    //avpkt.dts = rtStop;
    //avpkt.duration = (int)(rtStart != _I64_MIN && rtStop != _I64_MIN ? rtStop - rtStart : 0);
    avpkt.flags = AV_PKT_FLAG_KEY;

    used_bytes = avcodec_decode_video2 (m_pAVCtx, m_pFrame, &got_picture, &avpkt);

    if (used_bytes < 0) {
      return S_OK;
    }

    nSize	-= used_bytes;
    pDataIn	+= used_bytes;

    if (!got_picture || !m_pFrame->data[0]) {
      //DbgLog((LOG_TRACE, 10, L"No picture"));
      continue;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // The next big block computes the proper timestamps
    ///////////////////////////////////////////////////////////////////////////////////////////////
    if (m_nCodecId == CODEC_ID_MPEG1VIDEO || m_nCodecId == CODEC_ID_MPEG2VIDEO) {
      if (m_bDiscontinuity && m_pFrame->pict_type == AV_PICTURE_TYPE_I) {
        rtStart = m_pFrame->pkt_pts;
        m_bDiscontinuity = FALSE;
      } else {
        rtStart = m_rtPrevStop;
      }

      REFERENCE_TIME duration = (REF_SECOND_MULT * m_pAVCtx->time_base.num / m_pAVCtx->time_base.den) * m_pAVCtx->ticks_per_frame;
      rtStop = rtStart + (duration * (m_pFrame->repeat_pict ? 3 : 2)  / 2);
    } else if (m_bH264OnMPEG2 || m_bReorderWithoutStop) {
      rtStart = m_pFrame->pkt_pts;
      rtStop = m_pFrame->pkt_dts;

      if (m_bReorderWithoutStop) {
        rtStop = AV_NOPTS_VALUE;
      }
    } else if (m_pAVCtx->has_b_frames && m_bReorderBFrame) {
      int pos = m_nPosB - 2;
      
      if (pos < 0) {
        pos += (m_nThreads+1);
      }

      rtStart = m_BFrames[pos].rtStart;
      rtStop = m_BFrames[pos].rtStop;
    }

    if (rtStart == AV_NOPTS_VALUE) {
      rtStart = m_rtPrevStop;
      rtStop = AV_NOPTS_VALUE;
    }

    if (rtStop == AV_NOPTS_VALUE) {
      REFERENCE_TIME duration = 0;

      CMediaType mt = m_pInput->CurrentMediaType();
      formatTypeHandler(mt.Format(), mt.FormatType(), NULL, &duration, NULL, NULL);
      if (!duration && m_pAVCtx->time_base.num && m_pAVCtx->time_base.den) {
        duration = (REF_SECOND_MULT * m_pAVCtx->time_base.num / m_pAVCtx->time_base.den) * m_pAVCtx->ticks_per_frame;
      } else if(!duration) {
        duration = 1;
      }
      
      rtStop = rtStart + (duration * (m_pFrame->repeat_pict ? 3 : 2)  / 2);
    }

    DbgLog((LOG_TRACE, 10, L"Frame, rtStart: %I64d, diff: %I64d", rtStart, rtStart-m_rtPrevStart));

    m_rtPrevStart = rtStart;
    m_rtPrevStop = rtStop;

    if (rtStart < 0) {
      return S_OK;
    }

    if(FAILED(hr = GetDeliveryBuffer(m_pAVCtx->width, m_pAVCtx->height, &pOut)) || FAILED(hr = pOut->GetPointer(&pDataOut))) {
      return hr;
    }

    pOut->SetTime(&rtStart, &rtStop);
    pOut->SetMediaTime(NULL, NULL);

    if (m_pSwsContext == NULL) {
      CHECK_HR(hr = swscale_init());
    }

    for (int i = 0; i < 4; ++i) {
      srcStride[i] = m_pFrame->linesize[i];
    }
    dstStride[0] = dstStride[1] = m_pOutSize.cx;
    dstStride[2] = dstStride[3] = 0;
    dst[0] = pDataOut;
    dst[1] = dst[0] + dstStride[0] * m_pOutSize.cy;
    dst[2] = NULL;
    dst[3] = NULL;
    sws_scale (m_pSwsContext, m_pFrame->data, srcStride, 0, m_pAVCtx->height, dst, dstStride);

    SetTypeSpecificFlags (pOut);
    hr = m_pOutput->Deliver(pOut);
    SafeRelease(&pOut);
  }
done:
  SafeRelease(&pOut);
  return hr;
}

HRESULT CLAVVideo::SetTypeSpecificFlags(IMediaSample* pMS)
{
  HRESULT hr = S_OK;
  IMediaSample2 *pMS2 = NULL;
  if (SUCCEEDED(hr = pMS->QueryInterface(&pMS2))) {
    AM_SAMPLE2_PROPERTIES props;
    if(SUCCEEDED(pMS2->GetProperties(sizeof(props), (BYTE*)&props))) {
      props.dwTypeSpecificFlags &= ~0x7f;

      if(!m_pFrame->interlaced_frame) {
        props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_WEAVE;
      } else {
        if(m_pFrame->top_field_first) {
          props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_FIELD1FIRST;
        }
      }

      switch (m_pFrame->pict_type) {
      case FF_I_TYPE :
      case FF_SI_TYPE :
        props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_I_SAMPLE;
        break;
      case FF_P_TYPE :
      case FF_SP_TYPE :
        props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_P_SAMPLE;
        break;
      default :
        props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_B_SAMPLE;
        break;
      }

      pMS2->SetProperties(sizeof(props), (BYTE*)&props);
    }
  }
  SafeRelease(&pMS2);
  return hr;
}