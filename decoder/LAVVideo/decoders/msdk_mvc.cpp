/*
*      Copyright (C) 2010-2016 Hendrik Leppkes
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
#include "msdk_mvc.h"
#include "moreuuids.h"

////////////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////////////

ILAVDecoder *CreateDecoderMSDKMVC() {
  return new CDecMSDKMVC();
}

////////////////////////////////////////////////////////////////////////////////
// MSDK MVC decoder implementation
////////////////////////////////////////////////////////////////////////////////

CDecMSDKMVC::CDecMSDKMVC()
{
}

CDecMSDKMVC::~CDecMSDKMVC()
{
  DestroyDecoder(true);
}

STDMETHODIMP CDecMSDKMVC::Init()
{
  mfxIMPL impl = MFX_IMPL_AUTO_ANY;
  mfxVersion version = { 8, 1 };

  mfxStatus sts = MFXInit(impl, &version, &m_mfxSession);
  if (sts != MFX_ERR_NONE) {
    DbgLog((LOG_TRACE, 10, L"CDevMSDKMVC::Init(): MSDK not available"));
    return E_NOINTERFACE;
  }

  // query actual API version
  MFXQueryVersion(m_mfxSession, &m_mfxVersion);

#ifdef DEBUG
  MFXQueryIMPL(m_mfxSession, &impl);
  DbgLog((LOG_TRACE, 10, L"CDevMSDKMVC::Init(): MSDK Initialized, version %d.%d, impl 0x%04x", m_mfxVersion.Major, m_mfxVersion.Minor, impl));
#endif

  return S_OK;
}

void CDecMSDKMVC::DestroyDecoder(bool bFull)
{
  if (m_bDecodeReady) {
    MFXVideoDECODE_Close(m_mfxSession);
    m_bDecodeReady = FALSE;
  }

  {
    CAutoLock lock(&m_BufferCritSec);
    for (int i = 0; i < ASYNC_DEPTH; i++) {
      ReleaseBuffer(&m_pOutputQueue[i]->surface);
    }
    memset(m_pOutputQueue, 0, sizeof(m_pOutputQueue));

    for (auto it = m_BufferQueue.begin(); it != m_BufferQueue.end(); it++) {
      if (!(*it)->queued) {
        av_freep(&(*it)->surface.Data.Y);
        delete (*it);
      }
    }
    m_BufferQueue.clear();
  }

  // delete MVC sequence buffers
  SAFE_DELETE(m_mfxExtMVCSeq.View);
  SAFE_DELETE(m_mfxExtMVCSeq.ViewId);
  SAFE_DELETE(m_mfxExtMVCSeq.OP);

  SAFE_DELETE(m_pAnnexBConverter);

  if (bFull) {
    if (m_mfxSession) {
      MFXClose(m_mfxSession);
      m_mfxSession = nullptr;
    }
  }
}

STDMETHODIMP CDecMSDKMVC::InitDecoder(AVCodecID codec, const CMediaType *pmt)
{
  if (codec != AV_CODEC_ID_H264_MVC)
    return E_UNEXPECTED;

  if (*pmt->FormatType() != FORMAT_MPEG2Video)
    return E_UNEXPECTED;

  DestroyDecoder(false);

  // Init and reset video param arrays
  memset(&m_mfxVideoParams, 0, sizeof(m_mfxVideoParams));
  m_mfxVideoParams.mfx.CodecId = MFX_CODEC_AVC;

  memset(&m_mfxExtMVCSeq, 0, sizeof(m_mfxExtMVCSeq));
  m_mfxExtMVCSeq.Header.BufferId = MFX_EXTBUFF_MVC_SEQ_DESC;
  m_mfxExtMVCSeq.Header.BufferSz = sizeof(m_mfxExtMVCSeq);
  m_mfxExtParam[0] = (mfxExtBuffer *)&m_mfxExtMVCSeq;

  // Attach ext params to VideoParams
  m_mfxVideoParams.ExtParam = m_mfxExtParam;
  m_mfxVideoParams.NumExtParam = 1;

  MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)pmt->Format();

  if (*pmt->Subtype() == MEDIASUBTYPE_MVC1) {
    m_pAnnexBConverter = new CAnnexBConverter();
    m_pAnnexBConverter->SetNALUSize(2);

    // Decode sequence header from the media type
    if (mp2vi->cbSequenceHeader) {
      HRESULT hr = Decode((const BYTE *)mp2vi->dwSequenceHeader, mp2vi->cbSequenceHeader, AV_NOPTS_VALUE, AV_NOPTS_VALUE, TRUE, TRUE);
      if (FAILED(hr))
        return hr;
    }

    m_pAnnexBConverter->SetNALUSize(mp2vi->dwFlags);
  }

  return S_OK;
}

STDMETHODIMP CDecMSDKMVC::AllocateMVCExtBuffers()
{
  mfxU32 i;
  SAFE_DELETE(m_mfxExtMVCSeq.View);
  SAFE_DELETE(m_mfxExtMVCSeq.ViewId);
  SAFE_DELETE(m_mfxExtMVCSeq.OP);

  m_mfxExtMVCSeq.View = new mfxMVCViewDependency[m_mfxExtMVCSeq.NumView];
  CheckPointer(m_mfxExtMVCSeq.View, E_OUTOFMEMORY);
  for (i = 0; i < m_mfxExtMVCSeq.NumView; ++i)
  {
    memset(&m_mfxExtMVCSeq.View[i], 0, sizeof(m_mfxExtMVCSeq.View[i]));
  }
  m_mfxExtMVCSeq.NumViewAlloc = m_mfxExtMVCSeq.NumView;

  m_mfxExtMVCSeq.ViewId = new mfxU16[m_mfxExtMVCSeq.NumViewId];
  CheckPointer(m_mfxExtMVCSeq.ViewId, E_OUTOFMEMORY);
  for (i = 0; i < m_mfxExtMVCSeq.NumViewId; ++i)
  {
    memset(&m_mfxExtMVCSeq.ViewId[i], 0, sizeof(m_mfxExtMVCSeq.ViewId[i]));
  }
  m_mfxExtMVCSeq.NumViewIdAlloc = m_mfxExtMVCSeq.NumViewId;

  m_mfxExtMVCSeq.OP = new mfxMVCOperationPoint[m_mfxExtMVCSeq.NumOP];
  CheckPointer(m_mfxExtMVCSeq.OP, E_OUTOFMEMORY);
  for (i = 0; i < m_mfxExtMVCSeq.NumOP; ++i)
  {
    memset(&m_mfxExtMVCSeq.OP[i], 0, sizeof(m_mfxExtMVCSeq.OP[i]));
  }
  m_mfxExtMVCSeq.NumOPAlloc = m_mfxExtMVCSeq.NumOP;

  return S_OK;
}

MVCBuffer * CDecMSDKMVC::GetBuffer()
{
  CAutoLock lock(&m_BufferCritSec);
  MVCBuffer *pBuffer = nullptr;
  for (auto it = m_BufferQueue.begin(); it != m_BufferQueue.end(); it++) {
    if (!(*it)->surface.Data.Locked && !(*it)->queued) {
      pBuffer = *it;
      break;
    }
  }

  if (!pBuffer) {
    pBuffer = new MVCBuffer();

    pBuffer->surface.Info = m_mfxVideoParams.mfx.FrameInfo;
    pBuffer->surface.Info.FourCC = MFX_FOURCC_NV12;

    pBuffer->surface.Data.PitchLow = FFALIGN(m_mfxVideoParams.mfx.FrameInfo.Width, 64);
    pBuffer->surface.Data.Y  = (mfxU8 *)av_malloc(pBuffer->surface.Data.PitchLow * FFALIGN(m_mfxVideoParams.mfx.FrameInfo.Height, 64) * 3 / 2);
    pBuffer->surface.Data.UV = pBuffer->surface.Data.Y + (pBuffer->surface.Data.PitchLow * FFALIGN(m_mfxVideoParams.mfx.FrameInfo.Height, 64));

    m_BufferQueue.push_back(pBuffer);
    DbgLog((LOG_TRACE, 10, L"Allocated new MSDK MVC buffer (%d total)", m_BufferQueue.size()));
  }

  return pBuffer;
}

MVCBuffer * CDecMSDKMVC::FindBuffer(mfxFrameSurface1 * pSurface)
{
  CAutoLock lock(&m_BufferCritSec);
  bool bFound = false;
  for (auto it = m_BufferQueue.begin(); it != m_BufferQueue.end(); it++) {
    if (&(*it)->surface == pSurface) {
      return *it;
    }
  }

  return nullptr;
}

void CDecMSDKMVC::ReleaseBuffer(mfxFrameSurface1 * pSurface)
{
  if (!pSurface)
    return;

  CAutoLock lock(&m_BufferCritSec);
  MVCBuffer * pBuffer = FindBuffer(pSurface);

  if (pBuffer) {
    pBuffer->queued = 0;
    pBuffer->sync = nullptr;
  }
}

STDMETHODIMP CDecMSDKMVC::Decode(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity)
{
  if (!m_mfxSession)
    return E_UNEXPECTED;

  HRESULT hr = S_OK;
  mfxStatus sts = MFX_ERR_NONE;
  mfxBitstream bs = { 0 };
  BOOL bBuffered = FALSE, bFlush = (buffer == nullptr);

  if (rtStart >= 0 && rtStart != AV_NOPTS_VALUE)
    bs.TimeStamp = rtStart;
  else
    bs.TimeStamp = MFX_TIMESTAMP_UNKNOWN;

  bs.DecodeTimeStamp = MFX_TIMESTAMP_UNKNOWN;

  if (!bFlush) {
    if (m_pAnnexBConverter) {
      BYTE *pOutBuffer = nullptr;
      int pOutSize = 0;
      hr = m_pAnnexBConverter->Convert(&pOutBuffer, &pOutSize, buffer, buflen);
      if (FAILED(hr))
        return hr;

      m_buff.Append(pOutBuffer, pOutSize);
      bs.Data = m_buff.Ptr();
      bs.DataLength = m_buff.GetCount();
      bs.MaxLength = bs.DataLength;

      av_freep(&pOutBuffer);
    }
    else {
      ASSERT(0);
    }
  }

  if (!m_bDecodeReady) {
    sts = MFXVideoDECODE_DecodeHeader(m_mfxSession, &bs, &m_mfxVideoParams);
    if (sts == MFX_ERR_NOT_ENOUGH_BUFFER) {
      hr = AllocateMVCExtBuffers();
      if (FAILED(hr))
        return hr;

      sts = MFXVideoDECODE_DecodeHeader(m_mfxSession, &bs, &m_mfxVideoParams);
    }

    if (sts == MFX_ERR_NONE) {
      m_mfxVideoParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
      m_mfxVideoParams.AsyncDepth = ASYNC_DEPTH - 2;

      sts = MFXVideoDECODE_Init(m_mfxSession, &m_mfxVideoParams);
      if (sts != MFX_ERR_NONE) {
        DbgLog((LOG_TRACE, 10, L"CDevMSDKMVC::Decode(): Error initializing the MSDK decoder (%d)", sts));
        return E_FAIL;
      }

      if (m_mfxExtMVCSeq.NumView != 2) {
        DbgLog((LOG_TRACE, 10, L"CDevMSDKMVC::Decode(): Only MVC with two views is supported"));
        return E_FAIL;
      }

      DbgLog((LOG_TRACE, 10, L"CDevMSDKMVC::Decode(): Initialized MVC with View Ids %d, %d", m_mfxExtMVCSeq.View[0].ViewId, m_mfxExtMVCSeq.View[1].ViewId));

      m_bDecodeReady = TRUE;
    }
  }

  if (!m_bDecodeReady)
    return S_FALSE;

  mfxSyncPoint sync = nullptr;

  // Loop over the decoder to ensure all data is being consumed
  while (1) {
    MVCBuffer *pInputBuffer = GetBuffer();
    mfxFrameSurface1 *outsurf = nullptr;
    sts = MFXVideoDECODE_DecodeFrameAsync(m_mfxSession, bFlush ? nullptr : &bs, &pInputBuffer->surface, &outsurf, &sync);

    if (sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) {
      m_buff.Clear();
      bFlush = TRUE;
      m_bDecodeReady = FALSE;
      continue;
    }

    if (sync) {
      MVCBuffer * pOutputBuffer = FindBuffer(outsurf);
      pOutputBuffer->queued = 1;
      pOutputBuffer->sync = sync;
      HandleOutput(pOutputBuffer);
      continue;
    }

    if (sts != MFX_ERR_MORE_SURFACE && sts < 0)
      break;
  }

  if (!bs.DataOffset && !sync && !bFlush) {
    DbgLog((LOG_TRACE, 10, L"CDevMSDKMVC::Decode(): Decoder did not consume any data, discarding"));
    bs.DataOffset = m_buff.GetCount();
  }

  if (bs.DataOffset < m_buff.GetCount()) {
    BYTE *p = m_buff.Ptr();
    memmove(p, p + bs.DataOffset, m_buff.GetCount() - bs.DataOffset);
    m_buff.SetSize(m_buff.GetCount() - bs.DataOffset);
  } else {
    m_buff.Clear();
  }

  if (sts != MFX_ERR_MORE_DATA && sts < 0) {
    DbgLog((LOG_TRACE, 10, L"CDevMSDKMVC::Decode(): Error from Decode call (%d)", sts));
    return S_FALSE;
  }

  return S_OK;
}

HRESULT CDecMSDKMVC::HandleOutput(MVCBuffer * pOutputBuffer)
{
  int nCur = m_nOutputQueuePosition, nNext = (m_nOutputQueuePosition + 1) % ASYNC_DEPTH;
  if (m_pOutputQueue[nCur] && m_pOutputQueue[nNext]) {
    DeliverOutput(m_pOutputQueue[nCur], m_pOutputQueue[nNext]);

    m_pOutputQueue[nCur] = nullptr;
    m_pOutputQueue[nNext] = nullptr;
  }
  else if (m_pOutputQueue[nCur]) {
    DbgLog((LOG_TRACE, 10, L"CDevMSDKMVC::HandleOutput(): Dropping unpaired frame"));

    ReleaseBuffer(&m_pOutputQueue[nCur]->surface);
    m_pOutputQueue[nCur]->sync = nullptr;
    m_pOutputQueue[nCur] = nullptr;
  }

  m_pOutputQueue[nCur] = pOutputBuffer;
  m_nOutputQueuePosition = nNext;

  return S_OK;
}

HRESULT CDecMSDKMVC::DeliverOutput(MVCBuffer * pBaseView, MVCBuffer * pExtraView)
{
  mfxStatus sts = MFX_ERR_NONE;

  ASSERT(pBaseView->surface.Info.FrameId.ViewId == 0 && pExtraView->surface.Info.FrameId.ViewId > 0);
  ASSERT(pBaseView->surface.Data.FrameOrder == pExtraView->surface.Data.FrameOrder);

  // Sync base view
  do {
    sts = MFXVideoCORE_SyncOperation(m_mfxSession, pBaseView->sync, 1000);
  } while (sts == MFX_WRN_IN_EXECUTION);
  pBaseView->sync = nullptr;

  // Sync extra view
  do {
    sts = MFXVideoCORE_SyncOperation(m_mfxSession, pExtraView->sync, 1000);
  } while (sts == MFX_WRN_IN_EXECUTION);
  pExtraView->sync = nullptr;

  LAVFrame *pFrame = nullptr;
  AllocateFrame(&pFrame);

  pFrame->width  = pBaseView->surface.Info.CropW;
  pFrame->height = pBaseView->surface.Info.CropH;

  pFrame->data[0]   = pBaseView->surface.Data.Y;
  pFrame->data[1]   = pBaseView->surface.Data.UV;
  pFrame->stereo[0] = pExtraView->surface.Data.Y;
  pFrame->stereo[1] = pExtraView->surface.Data.UV;
  pFrame->data[2]   = (uint8_t *)pBaseView;
  pFrame->data[3]   = (uint8_t *)pExtraView;
  pFrame->stride[0] = pBaseView->surface.Data.PitchLow;
  pFrame->stride[1] = pBaseView->surface.Data.PitchLow;

  pFrame->format = LAVPixFmt_NV12;
  pFrame->bpp    = 8;
  pFrame->flags |= LAV_FRAME_FLAG_MVC;

  pFrame->rtStart = pBaseView->surface.Data.TimeStamp;

  int64_t num = (int64_t)pBaseView->surface.Info.AspectRatioW * pFrame->width;
  int64_t den = (int64_t)pBaseView->surface.Info.AspectRatioH * pFrame->height;
  av_reduce(&pFrame->aspect_ratio.num, &pFrame->aspect_ratio.den, num, den, INT_MAX);

  pFrame->destruct = msdk_buffer_destruct;
  pFrame->priv_data = this;

  return Deliver(pFrame);
}

void CDecMSDKMVC::msdk_buffer_destruct(LAVFrame *pFrame)
{
  CDecMSDKMVC *pDec = (CDecMSDKMVC *)pFrame->priv_data;
  CAutoLock lock(&pDec->m_BufferCritSec);

  MVCBuffer * pBaseBuffer   = (MVCBuffer *)pFrame->data[2];
  MVCBuffer * pStoredBuffer = pDec->FindBuffer(&pBaseBuffer->surface);
  if (pStoredBuffer) {
    pDec->ReleaseBuffer(&pBaseBuffer->surface);
  } else {
    av_free(pBaseBuffer->surface.Data.Y);
    SAFE_DELETE(pBaseBuffer);
  }

  MVCBuffer * pExtraBuffer = (MVCBuffer *)pFrame->data[3];
  pStoredBuffer = pDec->FindBuffer(&pExtraBuffer->surface);
  if (pStoredBuffer) {
    pDec->ReleaseBuffer(&pExtraBuffer->surface);
  } else {
    av_free(pExtraBuffer->surface.Data.Y);
    SAFE_DELETE(pExtraBuffer);
  }
}

STDMETHODIMP CDecMSDKMVC::Flush()
{
  m_buff.Clear();

  if (m_mfxSession) {
    if (m_bDecodeReady)
      MFXVideoDECODE_Reset(m_mfxSession, &m_mfxVideoParams);
    // TODO: decode sequence data

    for (int i = 0; i < ASYNC_DEPTH; i++) {
      ReleaseBuffer(&m_pOutputQueue[i]->surface);
    }
    memset(m_pOutputQueue, 0, sizeof(m_pOutputQueue));
  }

  return __super::Flush();
}

STDMETHODIMP CDecMSDKMVC::EndOfStream()
{
  if (!m_bDecodeReady)
    return S_FALSE;

  // Flush frames out of the decoder
  Decode(nullptr, 0, AV_NOPTS_VALUE, AV_NOPTS_VALUE, FALSE, FALSE);

  // Process all remaining frames in the queue
  for (int i = 0; i < ASYNC_DEPTH; i++) {
    int nCur = (m_nOutputQueuePosition + i) % ASYNC_DEPTH, nNext = (m_nOutputQueuePosition + i + 1) % ASYNC_DEPTH;
    if (m_pOutputQueue[nCur] && m_pOutputQueue[nNext]) {
      DeliverOutput(m_pOutputQueue[nCur], m_pOutputQueue[nNext]);

      m_pOutputQueue[nCur] = nullptr;
      m_pOutputQueue[nNext] = nullptr;

      i++;
    }
    else if (m_pOutputQueue[nCur]) {
      DbgLog((LOG_TRACE, 10, L"CDevMSDKMVC::EndOfStream(): Dropping unpaired frame"));

      ReleaseBuffer(&m_pOutputQueue[nCur]->surface);
      m_pOutputQueue[nCur] = nullptr;
    }
  }
  m_nOutputQueuePosition = 0;

  return S_OK;
}
