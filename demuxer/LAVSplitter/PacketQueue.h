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

#include <deque>

#define MIN_PACKETS_IN_QUEUE 100           // Below this is considered "drying pin"
#define MAX_PACKETS_IN_QUEUE 1000

// 256MB maximum for a single queue
#define MAX_QUEUE_SIZE       (256 * 1024 * 1024)

class Packet;

// FIFO Packet Queue
class CPacketQueue : public CCritSec
{
public:
  CPacketQueue() : m_dataSize(0) {};

  // Queue a new packet at the end of the list
  void Queue(Packet *pPacket, BOOL tryAppend = TRUE);

  // Get a packet from the beginning of the list
  Packet *Get();

  // Get the size of the queue
  size_t Size();

  // Get the size of the queue in bytes
  size_t DataSize();

  // Clear the List (all elements are free'ed)
  void Clear();

  // Get access to the internal queue
  std::deque<Packet *> *GetQueue() { return &m_queue; }

  bool IsEmpty() { CAutoLock cAutoLock(this); return m_queue.empty(); }
private:
  // The actual storage class
  std::deque<Packet *> m_queue;
  size_t m_dataSize;

#ifdef DEBUG
  bool m_bWarnedFull;
  bool m_bWarnedExtreme;
#endif
};
