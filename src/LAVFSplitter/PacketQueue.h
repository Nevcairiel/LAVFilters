/*
 *      Copyright (C) 2005-2010 Team XBMC
 *      http://www.xbmc.org
 *
 *      Copyright (C) 2010 Hendrik Leppkes
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
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 */

#pragma once

#include <deque>

#define MIN_PACKETS_IN_QUEUE 10           // Below this is considered "drying pin"
#define MAX_PACKETS_IN_QUEUE 100

// Data Packet for queue storage
class Packet
{
public:
  static const REFERENCE_TIME INVALID_TIME = _I64_MIN;

  DWORD StreamId;
  BOOL bDiscontinuity, bSyncPoint, bAppendable;
  REFERENCE_TIME rtStart, rtStop;
  AM_MEDIA_TYPE* pmt;

  Packet() { pmt = NULL; m_pbData = NULL; bDiscontinuity = bSyncPoint = bAppendable = FALSE; rtStart = rtStop = INVALID_TIME; m_dSize = 0; }
  virtual ~Packet() { if(pmt) DeleteMediaType(pmt); if(m_pbData) free(m_pbData); }

  // Getter
  DWORD GetDataSize() { return m_dSize; }
  BYTE *GetData() { return m_pbData; }
  BYTE GetAt(DWORD pos) { return m_pbData[pos]; }
  bool IsEmpty() { return m_dSize == 0; }

  // Setter
  void SetDataSize(DWORD len) { m_dSize = len; m_pbData = (BYTE *)realloc(m_pbData, len); }
  void SetData(const void* ptr, DWORD len) { SetDataSize(len); memcpy(m_pbData, ptr, len); }

  // Append the data of the package to our data buffer
  void Append(Packet *ptr) {
    DWORD prevSize = m_dSize;
    SetDataSize(prevSize + ptr->GetDataSize());
    memcpy(m_pbData+prevSize, ptr->GetData(), ptr->GetDataSize());
  }

  // Remove count bytes from position index
  void RemoveHead(DWORD count) {
    count = max(count, m_dSize);
    memmove(m_pbData, m_pbData+count, m_dSize-count);
    SetDataSize(m_dSize - count);
  }

private:
  DWORD m_dSize;
  BYTE *m_pbData;
};

// FIFO Packet Queue
class CPacketQueue : public CCritSec
{
public:
  // Queue a new packet at the end of the list
  void Queue(Packet *pPacket);

  // Get a packet from the beginning of the list
  Packet *Get();

  // Get the size of the queue
  int Size();

  // Clear the List (all elements are free'ed)
  void Clear();

  // Get access to the internal queue
  std::deque<Packet *> *GetQueue() { return &m_queue; }

  bool IsEmpty() { return Size() == 0; }
private:
  // The actual storage class
  std::deque<Packet *> m_queue;
};
