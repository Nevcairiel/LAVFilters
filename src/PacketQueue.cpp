#include "stdafx.h"
#include "PacketQueue.h"

// Queue a new packet at the end of the list
void CPacketQueue::Queue(Packet *pPacket)
{
  CAutoLock cAutoLock(this);

  // TODO merging appendable packets
  m_queue.push_back(pPacket);
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
  return pPacket;
}

// Get the size of the queue
int CPacketQueue::Size()
{
  CAutoLock cAutoLock(this);

  return m_queue.size();
}

// Clear the List (all elements are free'ed)
void CPacketQueue::Clear()
{
  CAutoLock cAutoLock(this);

  std::deque<Packet *>::iterator it;
  for (it = m_queue.begin(); it != m_queue.end(); it++) {
    free(*it);
  }
  m_queue.clear();
}
