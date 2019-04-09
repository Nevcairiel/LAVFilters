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
#include "PacketQueue.h"
#include "BaseDemuxer.h"

// Queue a new packet at the end of the list
void CPacketQueue::Queue(Packet *pPacket)
{
  CAutoLock cAutoLock(this);

  if (pPacket)
    m_dataSize += (size_t)pPacket->GetDataSize();

  m_queue.push_back(pPacket);
}

// Get a packet from the beginning of the list
Packet *CPacketQueue::Get()
{
  CAutoLock cAutoLock(this);

  if (m_queue.size() == 0) {
    return nullptr;
  }
  Packet *pPacket = m_queue.front();
  m_queue.pop_front();

  if (pPacket)
    m_dataSize -= (size_t)pPacket->GetDataSize();

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

  DbgLog((LOG_TRACE, 10, L"CPacketQueue::Clear() - clearing queue with %d entries", m_queue.size()));

  std::deque<Packet *>::iterator it;
  for (it = m_queue.begin(); it != m_queue.end(); ++it) {
    delete *it;
  }
  m_queue.clear();
  m_dataSize = 0;
}
