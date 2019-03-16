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
*
* This code was inspired by the ffdshow-tryouts TaudioCodecBitstream module, licensed under GPL 2.0
*/

#include "stdafx.h"
#include "LAVAudio.h"

extern "C"
{
#include "libavformat/spdif.h"
}

#define MAT_BUFFER_SIZE (61440)
#define MAT_BUFFER_LIMIT (MAT_BUFFER_SIZE - 24 /* MAT end code size */)
#define MAT_POS_MIDDLE (30708 /* middle point*/ + 8 /* IEC header in front */)

static const BYTE mat_start_code[20] = { 0x07, 0x9E, 0x00, 0x03, 0x84, 0x01, 0x01, 0x01, 0x80, 0x00, 0x56, 0xA5, 0x3B, 0xF4, 0x81, 0x83, 0x49, 0x80, 0x77, 0xE0 };
static const BYTE mat_middle_code[12] = { 0xC3, 0xC1, 0x42, 0x49, 0x3B, 0xFA, 0x82, 0x83, 0x49, 0x80, 0x77, 0xE0 };
static const BYTE mat_end_code[24] = { 0xC3, 0xC2, 0xC0, 0xC4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x97, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

void CLAVAudio::MATWriteHeader()
{
  ASSERT(m_bsOutput.GetCount() == 0);

  DWORD dwSize = BURST_HEADER_SIZE + sizeof(mat_start_code);

  // reserve size for the IEC header and the MAT start code
  m_bsOutput.SetSize(dwSize);

  BYTE *p = m_bsOutput.Ptr();

  // IEC burst header
  AV_WB16(p + 0, SYNCWORD1);
  AV_WB16(p + 2, SYNCWORD2);
  AV_WB16(p + 4, IEC61937_TRUEHD);
  AV_WB16(p + 6, 61424);

  // MAT start code
  memcpy(p + BURST_HEADER_SIZE, mat_start_code, sizeof(mat_start_code));

  // unless the start code falls into the padding,  its considered part of the current MAT frame
  // Note that audio frames are not always aligned with MAT frames, so we might already have a partial frame at this point
  m_TrueHDMATState.mat_framesize += dwSize;

  // The MAT metadata counts as padding, if we're scheduled to write any, which mean the start bytes should reduce any further padding.
  if (m_TrueHDMATState.padding > 0)
  {
    // if the header fits into the padding of the last frame, just reduce the amount of needed padding
    if (m_TrueHDMATState.padding > dwSize)
    {
      m_TrueHDMATState.padding -= dwSize;
      m_TrueHDMATState.mat_framesize = 0;
    }
    else // otherwise, consume all padding and set the size of the next MAT frame to the remaining data
    {
      m_TrueHDMATState.mat_framesize = (dwSize - m_TrueHDMATState.padding);
      m_TrueHDMATState.padding = 0;
    }
  }
}

void CLAVAudio::MATWritePadding()
{
  if (m_TrueHDMATState.padding > 0)
  {
    // allocate padding (on the stack of possible)
    BYTE *padding = (BYTE*)_malloca(m_TrueHDMATState.padding);

    memset(padding, 0, m_TrueHDMATState.padding);
    int remaining = MATFillDataBuffer(padding, m_TrueHDMATState.padding, true);

    // free the padding block
    _freea(padding);

    // not all padding could be written to the buffer, write it later
    if (remaining >= 0)
    {
      m_TrueHDMATState.padding = remaining;
      m_TrueHDMATState.mat_framesize = 0;
    }
    else // more padding then requested was written, eg. there was a MAT middle/end marker that needed to be written
    {
      m_TrueHDMATState.padding = 0;
      m_TrueHDMATState.mat_framesize = -remaining;
    }
  }
}

void CLAVAudio::MATAppendData(const BYTE *p, int size)
{
  m_bsOutput.Append(p, size);
  m_TrueHDMATState.mat_framesize += size;
}

int CLAVAudio::MATFillDataBuffer(const BYTE *p, int size, bool padding)
{
  if (m_bsOutput.GetCount() >= MAT_BUFFER_LIMIT)
    return size;

  int remaining = size;

  // Write MAT middle marker, if needed
  // The MAT middle marker always needs to be in the exact same spot, any audio data will be split.
  // If we're currently writing padding, then the marker will be considered as padding data and reduce the amount of padding still required.
  if (m_bsOutput.GetCount() <= MAT_POS_MIDDLE && m_bsOutput.GetCount() + size > MAT_POS_MIDDLE)
  {
    // write as much data before the middle code as we can
    int nBytesBefore = MAT_POS_MIDDLE - m_bsOutput.GetCount();
    MATAppendData(p, nBytesBefore);
    remaining -= nBytesBefore;

    // write the MAT middle code
    MATAppendData(mat_middle_code, sizeof(mat_middle_code));

    // if we're writing padding, deduct the size of the code from it
    if (padding)
      remaining -= sizeof(mat_middle_code);

    // write remaining data after the MAT marker
    if (remaining > 0)
    {
      remaining = MATFillDataBuffer(p + nBytesBefore, remaining, padding);
    }

    return remaining;
  }

  // not enough room in the buffer to write all the data, write as much as we can and add the MAT footer
  if (m_bsOutput.GetCount() + size >= MAT_BUFFER_LIMIT)
  {
    // write as much data before the middle code as we can
    int nBytesBefore = MAT_BUFFER_LIMIT - m_bsOutput.GetCount();
    MATAppendData(p, nBytesBefore);
    remaining -= nBytesBefore;

    // write the MAT end code
    MATAppendData(mat_end_code, sizeof(mat_end_code));

    ASSERT(m_bsOutput.GetCount() == MAT_BUFFER_SIZE);

    // MAT markers don't displace padding, so reduce the amount of padding
    if (padding)
      remaining -= sizeof(mat_end_code);

    // any remaining data will be written in future calls
    return remaining;
  }

  MATAppendData(p, size);

  return 0;
}

void CLAVAudio::MATFlushPacket(HRESULT *hrDeliver)
{
  if (m_bsOutput.GetCount() > 0) {
    ASSERT(m_bsOutput.GetCount() == 61440);

    // Deliver MAT packet to the audio renderer
    *hrDeliver = DeliverBitstream(m_nCodecId, m_bsOutput.Ptr(), m_bsOutput.GetCount(), m_rtStartInputCache, m_rtStopInputCache, true);
    m_bsOutput.SetSize(0);
  }
}

HRESULT CLAVAudio::BitstreamTrueHD(const BYTE *p, int buffsize, HRESULT *hrDeliver)
{
  // On a high level, a MAT frame consists of a sequence of padded TrueHD frames
  // The size of the padded frame can be determined from the frame time/sequence code in the frame header,
  // since it varies to accommodate spikes in bitrate.
  // In average all frames are always padded to 2560 bytes, so that 24 frames fit in one MAT frame, however
  // due to bitrate spikes single sync frames have been observed to use up to twice that size, in which
  // case they'll be preceded by smaller frames to keep the average bitrate constant.
  // A constant padding to 2560 bytes can work (this is how the ffmpeg spdifenc module works), however
  // high-bitrate streams can overshoot this size and therefor require proper handling of dynamic padding.

  // get the ratebits from the sync frame
  if (AV_RB32(p + 4) == 0xf8726fba)
  {
    m_TrueHDMATState.ratebits = p[8] >> 4;
  }
  else if (m_TrueHDMATState.prev_frametime_valid == false)
  {
    // only start streaming on a major sync frame
    m_rtBitstreamCache = AV_NOPTS_VALUE;
    return S_FALSE;
  }

  uint16_t frame_time = AV_RB16(p + 2);
  uint32_t space_size = 0;

  // compute final padded size for the previous frame, if any
  if (m_TrueHDMATState.prev_frametime_valid)
    space_size = uint16_t(frame_time - m_TrueHDMATState.prev_frametime) * (64 >> (m_TrueHDMATState.ratebits & 7));

  // compute padding (ie. difference to the size of the previous frame)
  ASSERT(space_size >= m_TrueHDMATState.prev_mat_framesize);

  // if for some reason the space_size fails, align the actual frame size
  if (space_size < m_TrueHDMATState.prev_mat_framesize)
    space_size = FFALIGN(m_TrueHDMATState.prev_mat_framesize, (64 >> (m_TrueHDMATState.ratebits & 7)));

  m_TrueHDMATState.padding += (space_size - m_TrueHDMATState.prev_mat_framesize);

  // store frame time of the previous frame
  m_TrueHDMATState.prev_frametime = frame_time;
  m_TrueHDMATState.prev_frametime_valid = true;

  // Write the MAT header into the fresh buffer
  if (m_bsOutput.GetCount() == 0)
  {
    MATWriteHeader();

    // initial header, don't count it for the frame size
    if (m_TrueHDMATState.init == false)
    {
      m_TrueHDMATState.init = true;
      m_TrueHDMATState.mat_framesize = 0;
    }
  }

  // write padding of the previous frame (if any)
  while (m_TrueHDMATState.padding > 0)
  {
    MATWritePadding();

    ASSERT(m_TrueHDMATState.padding == 0 || m_bsOutput.GetCount() == MAT_BUFFER_SIZE);

    // Buffer is full, submit it
    if (m_bsOutput.GetCount() == MAT_BUFFER_SIZE)
    {
      MATFlushPacket(hrDeliver);

      // and setup a new buffer
      MATWriteHeader();
    }
  }

  // write actual audio data to the buffer
  int remaining = MATFillDataBuffer(p, buffsize);

  // not all data could be written, or the buffer is full
  if (remaining || m_bsOutput.GetCount() == MAT_BUFFER_SIZE)
  {
    // flush out old data
    MATFlushPacket(hrDeliver);

    if (remaining)
    {
      // .. setup a new buffer
      MATWriteHeader();

      // and write the remaining data
      remaining = MATFillDataBuffer(p + (buffsize - remaining), remaining);

      ASSERT(remaining == 0);
    }
  }

  // store the size of the current MAT frame, so we can add padding later
  m_TrueHDMATState.prev_mat_framesize = m_TrueHDMATState.mat_framesize;
  m_TrueHDMATState.mat_framesize = 0;

  return S_OK;
}
