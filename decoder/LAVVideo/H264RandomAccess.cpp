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
#include "H264RandomAccess.h"

#include "H264Nalu.h"
#include "ByteParser.h"

/* NAL unit types */
enum {
    NAL_SLICE=1,
    NAL_DPA,
    NAL_DPB,
    NAL_DPC,
    NAL_IDR_SLICE,
    NAL_SEI,
    NAL_SPS,
    NAL_PPS,
    NAL_AUD,
    NAL_END_SEQUENCE,
    NAL_END_STREAM,
    NAL_FILLER_DATA,
    NAL_SPS_EXT,
    NAL_AUXILIARY_SLICE=19
};

/**
 * SEI message types
 */
typedef enum {
    SEI_BUFFERING_PERIOD             =  0, ///< buffering period (H.264, D.1.1)
    SEI_TYPE_PIC_TIMING              =  1, ///< picture timing
    SEI_TYPE_USER_DATA_UNREGISTERED  =  5, ///< unregistered user data
    SEI_TYPE_RECOVERY_POINT          =  6  ///< recovery point (frame # to decoder sync)
} SEI_Type;

CH264RandomAccess::CH264RandomAccess()
{
  flush(0);
}

CH264RandomAccess::~CH264RandomAccess()
{
}

void CH264RandomAccess::flush(int threadCount)
{
  m_RecoveryMode = 1;
  m_RecoveryFrameCount = 0;
  m_ThreadDelay = threadCount;
}

BOOL CH264RandomAccess::searchRecoveryPoint(const uint8_t *buf, size_t buf_size)
{
  if (m_RecoveryMode == 1) {
    int recoveryPoint = parseForRecoveryPoint(buf, buf_size, &m_RecoveryFrameCount);
    switch (recoveryPoint) {
    case 3: // IDRS
      m_RecoveryMode = 0;
      return TRUE;
    case 2: // SEI recovery
      m_RecoveryMode = 2;
      return TRUE;
    case 1: // I Frame
      m_RecoveryMode = 2;
      m_RecoveryFrameCount = 0;
      return TRUE;
    default:
      return FALSE;
    }
  } else {
    return TRUE;
  }
}

int CH264RandomAccess::decode_sei_recovery_point(CByteParser *pParser)
{
  while(pParser->Remaining() > 2) {
    int size, type;
    
    type = 0;
    do {
      type += pParser->BitRead(8, true);
    } while (pParser->BitRead(8) == 255);

    size = 0;
    do {
      size += pParser->BitRead(8, true);
    } while (pParser->BitRead(8) == 255);

    if (type == SEI_TYPE_RECOVERY_POINT) {
      int recovery_count = (int)pParser->UExpGolombRead();
      return recovery_count;
    }
  }
  return -1;
}

void CH264RandomAccess::judgeFrameUsability(AVFrame *pFrame, int *got_picture_ptr)
{
  if ((m_ThreadDelay > 1 && --m_ThreadDelay > 0) || m_RecoveryMode == 0 || pFrame->h264_max_frame_num == 0) {
    return;
  }

  if (m_RecoveryMode == 1 || m_RecoveryMode == 2) {
    m_RecoveryFrameCount = (m_RecoveryFrameCount + pFrame->h264_frame_num_decoded) % pFrame->h264_max_frame_num;
    m_RecoveryMode = 3;
  }

  if (m_RecoveryMode == 3) {
    if (m_RecoveryFrameCount <= pFrame->h264_frame_num_decoded) {
      m_RecoveryPOC = pFrame->h264_poc_decoded;
      m_RecoveryMode = 4;
    }
  }

  if (m_RecoveryMode == 4) {
    if (pFrame->h264_poc_outputed >= m_RecoveryPOC) {
      m_RecoveryMode = 0;
    }
  }

  if (m_RecoveryMode != 0) {
    *got_picture_ptr = 0;
  }
}

int CH264RandomAccess::parseForRecoveryPoint(const uint8_t *buf, size_t buf_size, int *recoveryFrameCount)
{
  int found = 0;

  CH264Nalu nal;
  nal.SetBuffer(buf, buf_size, m_AVCNALSize);
  while(nal.ReadNext()) {
    const BYTE *pData = nal.GetDataBuffer() + 1;
    size_t len = nal.GetDataLength() - 1;
    CByteParser parser(pData, len);

    switch (nal.GetType()) {
    case NAL_IDR_SLICE:
      DbgLog((LOG_TRACE, 10, L"h264RandomAccess::parseForRecoveryPoint(): Found IDR slice"));
      found = 3;
      goto end;
    case NAL_SEI:
      if (pData[0] == SEI_TYPE_RECOVERY_POINT) {
        int ret = decode_sei_recovery_point(&parser);
        if (ret >= 0) {
          DbgLog((LOG_TRACE, 10, L"h264RandomAccess::parseForRecoveryPoint(): Found SEI recovery point (count: %d)", ret));
          *recoveryFrameCount = ret;
          if (found < 2)
            found = 2;
        }
      }
      break;
    case NAL_AUD:
      {
        int primary_pic_type = parser.BitRead(3);
        if (!found && (primary_pic_type == 0 || primary_pic_type == 3)) { // I Frame
          DbgLog((LOG_TRACE, 10, L"h264RandomAccess::parseForRecoveryPoint(): Found I frame"));
          found = 1;
        }
      }
      break;
    case NAL_SLICE:
    case NAL_DPA:
      if (!found) {
        parser.UExpGolombRead();
        int slice_type = (int)parser.UExpGolombRead();
        if (slice_type == 2 || slice_type == 4 || slice_type == 7 || slice_type == 9) { // I/SI slice
          DbgLog((LOG_TRACE, 10, L"h264RandomAccess::parseForRecoveryPoint(): Detected I/SI slice"));
          found = 1;
        } else {
          found = 0;
          goto end;
        }
      }
      break;
    }
  }
end:
  return found;
}
