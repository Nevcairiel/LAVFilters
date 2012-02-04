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
#include "PacketQueue.h"

// Queue a new packet at the end of the list
void CPacketQueue::Queue(Packet *pPacket, BOOL tryAppend)
{
  CAutoLock cAutoLock(this);

  if (pPacket)
    m_dataSize += pPacket->GetDataSize();

  if (tryAppend && pPacket) {
    if (pPacket->bAppendable && !pPacket->bDiscontinuity && !pPacket->pmt
      && pPacket->rtStart == Packet::INVALID_TIME && !IsEmpty()
      && m_queue.back()->rtStart != Packet::INVALID_TIME) {
        Packet* tail = m_queue.back();
        tail->Append(pPacket);
        delete pPacket;
        return;
    }
  }
  m_queue.push_back(pPacket);

#ifdef DEBUG
  if (m_queue.size() > MAX_PACKETS_IN_QUEUE && !m_bWarnedFull) {
    DbgLog((LOG_TRACE, 20, L"CPacketQueue::Queue() - Queue is Full (%d elements)", m_queue.size()));
    m_bWarnedFull = true;
  } else if (m_queue.size() > 10*MAX_PACKETS_IN_QUEUE && !m_bWarnedExtreme) {
    DbgLog((LOG_TRACE, 20, L"CPacketQueue::Queue() - Queue is Extremely Full (%d elements)", m_queue.size()));
    m_bWarnedExtreme = true;
  } else if (m_queue.size() < MAX_PACKETS_IN_QUEUE/2) {
    m_bWarnedFull = m_bWarnedExtreme = false;
  }
#endif
}

// Get a packet from the beginning of the list
Packet *CPacketQueue::Get()
{
  CAutoLock cAutoLock(this);

  if (m_queue.size() == 0) {
    return NULL;
  }
  Packet *pPacket = m_queue.front();
  m_queue.pop_front();

  if (pPacket)
    m_dataSize -= pPacket->GetDataSize();

  return pPacket;
}

// Get the size of the queue
size_t CPacketQueue::Size()
{
  CAutoLock cAutoLock(this);

  return m_queue.size();
}

// Get the size of the queue
size_t CPacketQueue::DataSize()
{
  CAutoLock cAutoLock(this);

  return m_dataSize;
}

// Clear the List (all elements are free'ed)
void CPacketQueue::Clear()
{
  CAutoLock cAutoLock(this);

  DbgLog((LOG_TRACE, 10, L"CPacketQueue::Clear() - clearing queue with %d entrys", m_queue.size()));

  std::deque<Packet *>::iterator it;
  for (it = m_queue.begin(); it != m_queue.end(); ++it) {
    delete *it;
  }
  m_queue.clear();
  m_dataSize = 0;
}
