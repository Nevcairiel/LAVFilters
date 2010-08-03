
#ifndef __PACKET_QUEUE_H__FFSPLITTER__
#define __PACKET_QUEUE_H__FFSPLITTER__

#include <deque>

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

  // Setter
  void SetDataSize(DWORD len) { m_dSize = len; m_pbData = (BYTE *)realloc(m_pbData, len); }
  void SetData(const void* ptr, DWORD len) { SetDataSize(len); memcpy(GetData(), ptr, len); }

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
private:
  // The actual storage class
  std::deque<Packet *> m_queue;
};

#endif
