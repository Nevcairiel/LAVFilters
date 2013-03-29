/*
 *      Copyright (C) 2010-2013 Hendrik Leppkes
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

// Data Packet for queue storage
class Packet
{
public:
  static const REFERENCE_TIME INVALID_TIME = _I64_MIN;

  Packet();
  ~Packet();

  int GetDataSize() const { return m_DataSize; }
  BYTE *GetData() { return m_Data; }

  int SetDataSize(int len);
  int SetData(const void* ptr, int len);
  int SetPacket(AVPacket *pkt);

  // Append the data of the package to our data buffer
  int Append(Packet *ptr);
  int AppendData(const void* ptr, int len);
  // Remove count bytes from position index
  int RemoveHead(int count);

public:
  DWORD StreamId;
  BOOL bDiscontinuity, bSyncPoint;
  REFERENCE_TIME rtStart, rtStop;
  LONGLONG bPosition;
  AM_MEDIA_TYPE* pmt;

#define LAV_PACKET_PARSED           0x0001
#define LAV_PACKET_MOV_TEXT         0x0002
#define LAV_PACKET_FORCED_SUBTITLE  0x0004
#define LAV_PACKET_H264_ANNEXB      0x0008
#define LAV_PACKET_SRT              0x0010
  DWORD dwFlags;

private:
  int m_DataSize;
  BYTE *m_Data;
  AVBufferRef *m_Buf;
};
