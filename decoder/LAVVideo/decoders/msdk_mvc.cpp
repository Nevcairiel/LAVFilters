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
#include "msdk_mvc.h"
#include "moreuuids.h"
#include "ByteParser.h"
#include "H264Nalu.h"

////////////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////////////

ILAVDecoder *CreateDecoderMSDKMVC() {
  return new CDecMSDKMVC();
}

////////////////////////////////////////////////////////////////////////////////
// Bitstream buffering
////////////////////////////////////////////////////////////////////////////////

class CBitstreamBuffer {
public:
  CBitstreamBuffer(GrowableArray<BYTE> * arrStorage) : m_pStorage(arrStorage) {}

  ~CBitstreamBuffer() {
    ASSERT(m_nConsumed <= m_pStorage->GetCount());
    if (m_nConsumed < m_pStorage->GetCount()) {
      BYTE *p = m_pStorage->Ptr();
      memmove(p, p + m_nConsumed, m_pStorage->GetCount() - m_nConsumed);
      m_pStorage->SetSize(DWORD(m_pStorage->GetCount() - m_nConsumed));
    }
    else {
      m_pStorage->Clear();
    }
  }

  void Allocate(size_t size) {
    m_pStorage->Allocate((DWORD)size);
  }

  void Append(const BYTE * buffer, size_t size) {
    m_pStorage->Append(buffer, DWORD(size));
  }

  void Consume(size_t count) {
    m_nConsumed += min(count, GetBufferSize());
  }

  void Clear() {
    m_nConsumed += GetBufferSize();
  }

  BYTE * GetBuffer() {
    return m_pStorage->Ptr() + m_nConsumed;
  }

  size_t GetBufferSize() {
    return m_pStorage->GetCount() - m_nConsumed;
  }

private:
  GrowableArray<BYTE> * m_pStorage = nullptr;
  size_t m_nConsumed = 0;
};

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
  mfxIMPL impls[] = { MFX_IMPL_AUTO_ANY, MFX_IMPL_SOFTWARE };
  mfxVersion version = { 8, 1 };

  // Check if HWAccel is allowed
  if (!m_pSettings->GetHWAccelCodec(HWCodec_H264MVC))
    impls[0] = MFX_IMPL_SOFTWARE;

  for (int i = 0; i < countof(impls); i++)
  {
    mfxStatus sts = MFXInit(impls[i], &version, &m_mfxSession);
    if (sts != MFX_ERR_NONE) {
      DbgLog((LOG_TRACE, 10, L"CDevMSDKMVC::Init(): MSDK not available"));
      return E_NOINTERFACE;
    }

    // query actual API version
    MFXQueryVersion(m_mfxSession, &m_mfxVersion);

    mfxIMPL impl = 0;
    MFXQueryIMPL(m_mfxSession, &impl);
    DbgLog((LOG_TRACE, 10, L"CDevMSDKMVC::Init(): MSDK Initialized, version %d.%d, impl 0x%04x", m_mfxVersion.Major, m_mfxVersion.Minor, impl));

    if ((impl & MFX_IMPL_VIA_MASK) == MFX_IMPL_VIA_D3D11 && !IsWindows8OrNewer())
    {
      DbgLog((LOG_TRACE, 10, L"CDevMSDKMVC::Init(): Skipping D3D11 acceleration on pre-Windows 8 system"));
      MFXClose(m_mfxSession);
      m_mfxSession = nullptr;
      continue;
    }

    m_mfxImpl = impl;

    break;
  }

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
    for (int i = 0; i < ASYNC_QUEUE_SIZE; i++) {
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

  m_nMP4NALUSize = 0;

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
    m_nMP4NALUSize = 2;

    // Decode sequence header from the media type
    if (mp2vi->cbSequenceHeader) {
      HRESULT hr = Decode((const BYTE *)mp2vi->dwSequenceHeader, mp2vi->cbSequenceHeader, AV_NOPTS_VALUE, AV_NOPTS_VALUE, TRUE, TRUE, nullptr);
      if (FAILED(hr))
        return hr;
    }

    m_nMP4NALUSize = mp2vi->dwFlags;
  }
  else if (*pmt->Subtype() == MEDIASUBTYPE_AMVC) {
    // Decode sequence header from the media type
    if (mp2vi->cbSequenceHeader) {
      HRESULT hr = Decode((const BYTE *)mp2vi->dwSequenceHeader, mp2vi->cbSequenceHeader, AV_NOPTS_VALUE, AV_NOPTS_VALUE, TRUE, TRUE, nullptr);
      if (FAILED(hr))
        return hr;
    }
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
    if (pBuffer->surface.Data.Y == nullptr) {
      delete pBuffer;
      return nullptr;
    }
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

static const BYTE s_AnnexBStartCode3[3] = { 0x00, 0x00, 0x01 };
static const BYTE s_AnnexBStartCode4[4] = { 0x00, 0x00, 0x00, 0x01 };

STDMETHODIMP CDecMSDKMVC::Decode(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity, IMediaSample *pSample)
{
  if (!m_mfxSession)
    return E_UNEXPECTED;

  HRESULT hr = S_OK;
  CBitstreamBuffer bsBuffer(&m_buff);
  mfxStatus sts = MFX_ERR_NONE;
  mfxBitstream bs = { 0 };
  BOOL bFlush = (buffer == nullptr);

  if (rtStart >= -TIMESTAMP_OFFSET && rtStart != AV_NOPTS_VALUE)
    bs.TimeStamp = rtStart + TIMESTAMP_OFFSET;
  else
    bs.TimeStamp = MFX_TIMESTAMP_UNKNOWN;

  bs.DecodeTimeStamp = MFX_TIMESTAMP_UNKNOWN;

  if (!bFlush) {
    bsBuffer.Allocate(bsBuffer.GetBufferSize() + buflen);

    // Check the buffer for SEI NALU, and some unwanted NALUs that need filtering
    // MSDK's SEI reading functionality is slightly buggy
    CH264Nalu nalu;
    nalu.SetBuffer(buffer, buflen, m_nMP4NALUSize);
    BOOL bNeedFilter = FALSE;
    while (nalu.ReadNext()) {
      // Don't include End-of-Sequence or AUD NALUs
      if (nalu.GetType() == NALU_TYPE_EOSEQ || nalu.GetType() == NALU_TYPE_AUD)
        continue;

      if (nalu.GetType() == NALU_TYPE_SEI) {
        CH264NALUnescape seiNalu(nalu.GetDataBuffer() + 1, nalu.GetDataLength() - 1);
        ParseSEI(seiNalu.GetBuffer(), seiNalu.GetSize(), bs.TimeStamp);
      }

      // copy filtered NALs into the data buffer
      if (m_nMP4NALUSize)
      {
        // write annex b nal
        if (nalu.GetNALPos() == 0 ||  nalu.GetType() == NALU_TYPE_SPS || nalu.GetType() == NALU_TYPE_PPS)
          bsBuffer.Append(s_AnnexBStartCode4, 4);
        else
          bsBuffer.Append(s_AnnexBStartCode3, 3);

        // append nal data
        bsBuffer.Append(nalu.GetDataBuffer(), nalu.GetDataLength());
      }
      else
      {
        bsBuffer.Append(nalu.GetNALBuffer(), nalu.GetLength());
      }
    }

    bs.Data = bsBuffer.GetBuffer();
    bs.DataLength = mfxU32(bsBuffer.GetBufferSize());
    bs.MaxLength = bs.DataLength;

    AddFrameToGOP(bs.TimeStamp);
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
      m_mfxVideoParams.AsyncDepth = ASYNC_DEPTH;

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
    if (pInputBuffer == nullptr)
      return E_OUTOFMEMORY;

    mfxFrameSurface1 *outsurf = nullptr;
    sts = MFXVideoDECODE_DecodeFrameAsync(m_mfxSession, bFlush ? nullptr : &bs, &pInputBuffer->surface, &outsurf, &sync);

    if (sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) {
      DbgLog((LOG_TRACE, 10, L"CDevMSDKMVC::Decode(): Incompatible video parameters detected, flushing decoder"));
      bsBuffer.Clear();
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
    bs.DataOffset = mfxU32(bsBuffer.GetBufferSize());
  }

  bsBuffer.Consume(bs.DataOffset);

  if (sts != MFX_ERR_MORE_DATA && sts < 0) {
    DbgLog((LOG_TRACE, 10, L"CDevMSDKMVC::Decode(): Error from Decode call (%d)", sts));
    return S_FALSE;
  }

  return S_OK;
}

HRESULT CDecMSDKMVC::ParseSEI(const BYTE *buffer, size_t size, mfxU64 timestamp)
{
  CByteParser seiParser(buffer, size);
  while (seiParser.RemainingBits() > 16 && seiParser.BitRead(16, true)) {
    int type = 0;
    unsigned size = 0;

    do {
      if (seiParser.RemainingBits() < 8)
        return E_FAIL;
      type += seiParser.BitRead(8, true);
    } while (seiParser.BitRead(8) == 0xFF);

    do {
      if (seiParser.RemainingBits() < 8)
        return E_FAIL;
      size += seiParser.BitRead(8, true);
    } while (seiParser.BitRead(8) == 0xFF);

    if (size > seiParser.Remaining()) {
      DbgLog((LOG_TRACE, 10, L"CDecMSDKMVC::ParseSEI(): SEI type %d size %d truncated, available: %d", type, size, seiParser.Remaining()));
      return E_FAIL;
    }

    switch (type) {
    case 5:
      ParseUnregUserDataSEI(buffer + seiParser.Pos(), size, timestamp);
      break;
    case 37:
      ParseMVCNestedSEI(buffer + seiParser.Pos(), size, timestamp);
      break;
    }

    seiParser.BitSkip(size * 8);
  }

  return S_OK;
}

HRESULT CDecMSDKMVC::ParseMVCNestedSEI(const BYTE *buffer, size_t size, mfxU64 timestamp)
{
  CByteParser seiParser(buffer, size);

  // Parse the MVC Scalable Nesting SEI first
  int op_flag = seiParser.BitRead(1);
  if (!op_flag) {
    int all_views_in_au = seiParser.BitRead(1);
    if (!all_views_in_au) {
      int num_views_min1 = seiParser.UExpGolombRead();
      for (int i = 0; i <= num_views_min1; i++) {
        seiParser.BitRead(10); // sei_view_id[i]
      }
    }
  }
  else {
    int num_views_min1 = seiParser.UExpGolombRead();
    for (int i = 0; i <= num_views_min1; i++) {
      seiParser.BitRead(10); // sei_op_view_id[i]
    }
    seiParser.BitRead(3); // sei_op_temporal_id
  }
  seiParser.BitByteAlign();

  // Parse nested SEI
  ParseSEI(buffer + seiParser.Pos(), seiParser.Remaining(), timestamp);

  return S_OK;
}

static const uint8_t uuid_iso_iec_11578[16] = {
  0x17, 0xee, 0x8c, 0x60, 0xf8, 0x4d, 0x11, 0xd9, 0x8c, 0xd6, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66
};

HRESULT CDecMSDKMVC::ParseUnregUserDataSEI(const BYTE *buffer, size_t size, mfxU64 timestamp)
{
  if (size < 20)
    return E_FAIL;

  if (memcmp(buffer, uuid_iso_iec_11578, 16) != 0) {
    DbgLog((LOG_TRACE, 10, L"CDecMSDKMVC::ParseUnregUserDataSEI(): Unknown User Data GUID"));
    return S_FALSE;
  }

  uint32_t type = AV_RB32(buffer + 16);

  // Offset metadata
  if (type == 0x4F464D44) {
    return ParseOffsetMetadata(buffer + 20, size - 20, timestamp);
  }

  return S_FALSE;
}

HRESULT CDecMSDKMVC::ParseOffsetMetadata(const BYTE *buffer, size_t size, mfxU64 timestamp)
{
  if (size < 10)
    return E_FAIL;

  // Skip PTS part, its not used. Start parsing at first marker bit after the PTS
  CByteParser offset(buffer + 6, size - 6);
  offset.BitSkip(2); // Skip marker and re served

  unsigned int nOffsets = offset.BitRead(6);
  unsigned int nFrames = offset.BitRead(8);
  DbgLog((LOG_CUSTOM2, 10, L"CDecMSDKMVC::ParseOffsetMetadata(): offset_metadata with %d offsets and %d frames for time %I64u", nOffsets, nFrames, timestamp));

  if (nOffsets > 32) {
    DbgLog((LOG_TRACE, 10, L"CDecMSDKMVC::ParseOffsetMetadata(): > 32 offsets is not supported"));
    return E_FAIL;
  }

  offset.BitSkip(16); // Skip marker and reserved
  if (nOffsets * nFrames > (size - 10)) {
    DbgLog((LOG_TRACE, 10, L"CDecMSDKMVC::ParseOffsetMetadata(): not enough data for all offsets (need %d, have %d)", nOffsets * nFrames, size - 4));
    return E_FAIL;
  }

  MVCGOP GOP;

  for (unsigned int o = 0; o < nOffsets; o++) {
    for (unsigned int f = 0; f < nFrames; f++) {
      if (o == 0) {
        MediaSideData3DOffset off = { (int)nOffsets };
        GOP.offsets.push_back(off);
      }

      int direction_flag = offset.BitRead(1);
      int value = offset.BitRead(7);

      if (direction_flag)
        value = -value;

      GOP.offsets[f].offset[o] = value;
    }
  }

  m_GOPs.push_back(GOP);

  return S_OK;
}

void CDecMSDKMVC::AddFrameToGOP(mfxU64 timestamp)
{
  if (m_GOPs.size() > 0)
    m_GOPs.back().timestamps.push_back(timestamp);
}

BOOL CDecMSDKMVC::RemoveFrameFromGOP(MVCGOP * pGOP, mfxU64 timestamp)
{
  if (pGOP->timestamps.empty() || pGOP->offsets.empty())
    return FALSE;

  auto e = std::find(pGOP->timestamps.begin(), pGOP->timestamps.end(), timestamp);
  if (e != pGOP->timestamps.end()) {
    pGOP->timestamps.erase(e);
    return TRUE;
  }

  return FALSE;
}

void CDecMSDKMVC::GetOffsetSideData(LAVFrame *pFrame, mfxU64 timestamp)
{
  MediaSideData3DOffset offset = { 255 };

  // Go over all stored GOPs and find an entry for our timestamp
  // In general it should be found in the first GOP, unless we lost frames in between or something else went wrong.
  for (auto it = m_GOPs.begin(); it != m_GOPs.end(); it++) {
    if (RemoveFrameFromGOP(&(*it), timestamp)) {
      offset = it->offsets.front();
      it->offsets.pop_front();

      // Erase previous GOPs when we start accessing a new one
      if (it != m_GOPs.begin()) {
#ifdef DEBUG
        // Check that all to-be-erased GOPs are empty
        for (auto itd = m_GOPs.begin(); itd < it; itd++) {
          if (!itd->offsets.empty()) {
            DbgLog((LOG_TRACE, 10, L"CDecMSDKMVC::GetOffsetSideData(): Switched to next GOP at %I64u with %Iu entries remaining", timestamp, itd->offsets.size()));
          }
        }
#endif
        m_GOPs.erase(m_GOPs.begin(), it);
      }
      break;
    }
  }

  if (offset.offset_count == 255) {
    DbgLog((LOG_TRACE, 10, L"CDecMSDKMVC::GetOffsetSideData():No offset for frame at %I64u", timestamp));
    offset = m_PrevOffset;
  }

  m_PrevOffset = offset;

  // Only set the offset when data is present
  if (offset.offset_count > 0) {
    MediaSideData3DOffset *FrameOffset = (MediaSideData3DOffset *)AddLAVFrameSideData(pFrame, IID_MediaSideData3DOffset, sizeof(MediaSideData3DOffset));
    if (FrameOffset)
      *FrameOffset = offset;
  }
}

HRESULT CDecMSDKMVC::HandleOutput(MVCBuffer * pOutputBuffer)
{
  int nCur = m_nOutputQueuePosition, nNext = (m_nOutputQueuePosition + 1) % ASYNC_QUEUE_SIZE;
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

  if (!(pBaseView->surface.Data.DataFlag & MFX_FRAMEDATA_ORIGINAL_TIMESTAMP))
    pBaseView->surface.Data.TimeStamp = MFX_TIMESTAMP_UNKNOWN;

  if (pBaseView->surface.Data.TimeStamp != MFX_TIMESTAMP_UNKNOWN) {
    pFrame->rtStart = pBaseView->surface.Data.TimeStamp;
    pFrame->rtStart -= TIMESTAMP_OFFSET;
  }
  else {
    pFrame->rtStart = AV_NOPTS_VALUE;
  }

  int64_t num = (int64_t)pBaseView->surface.Info.AspectRatioW * pFrame->width;
  int64_t den = (int64_t)pBaseView->surface.Info.AspectRatioH * pFrame->height;
  av_reduce(&pFrame->aspect_ratio.num, &pFrame->aspect_ratio.den, num, den, INT_MAX);

  pFrame->destruct = msdk_buffer_destruct;
  pFrame->priv_data = this;

  GetOffsetSideData(pFrame, pBaseView->surface.Data.TimeStamp);

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

    for (int i = 0; i < ASYNC_QUEUE_SIZE; i++) {
      ReleaseBuffer(&m_pOutputQueue[i]->surface);
    }
    memset(m_pOutputQueue, 0, sizeof(m_pOutputQueue));
  }

  m_GOPs.clear();
  memset(&m_PrevOffset, 0, sizeof(m_PrevOffset));

  return __super::Flush();
}

STDMETHODIMP CDecMSDKMVC::EndOfStream()
{
  if (!m_bDecodeReady)
    return S_FALSE;

  // Flush frames out of the decoder
  Decode(nullptr, 0, AV_NOPTS_VALUE, AV_NOPTS_VALUE, FALSE, FALSE, nullptr);

  // Process all remaining frames in the queue
  for (int i = 0; i < ASYNC_QUEUE_SIZE; i++) {
    int nCur = (m_nOutputQueuePosition + i) % ASYNC_QUEUE_SIZE, nNext = (m_nOutputQueuePosition + i + 1) % ASYNC_QUEUE_SIZE;
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
