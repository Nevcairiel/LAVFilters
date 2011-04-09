/*
 *      Copyright (C) 2011 Hendrik Leppkes
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
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 *  Initial design and concept by Gabest and the MPC-HC Team, copyright under GPLv2
 */

#include "stdafx.h"
#include "StreamParser.h"

#include "OutputPin.h"
#include "H264Nalu.h"


CStreamParser::CStreamParser(CLAVOutputPin *pPin, const char *szContainer)
  : m_pPin(pPin), m_strContainer(szContainer), m_pPacketBuffer(NULL), m_gSubtype(GUID_NULL), m_fHasAccessUnitDelimiters(false)
{
}


CStreamParser::~CStreamParser()
{
  Flush();
}

HRESULT CStreamParser::Parse(const GUID &gSubtype, Packet *pPacket)
{
  if (gSubtype != m_gSubtype) {
    m_gSubtype = gSubtype;
    Flush();
  }
  
  if (!pPacket || (pPacket->dwFlags & LAV_PACKET_PARSED)) {
    Queue(pPacket);
  } else if ((m_strContainer == "mpegts" ||m_strContainer == "mpeg") && m_gSubtype == MEDIASUBTYPE_AVC1) {
    ParseH264AnnexB(pPacket);
  } else if ((m_strContainer == "mpegts" || m_strContainer == "mpeg") && (m_gSubtype == MEDIASUBTYPE_WVC1 || m_gSubtype == MEDIASUBTYPE_WVC1_ARCSOFT || m_gSubtype == MEDIASUBTYPE_WVC1_CYBERLINK)) {
    ParseVC1(pPacket);
  } else if (m_gSubtype == MEDIASUBTYPE_HDMV_LPCM_AUDIO) {
    pPacket->RemoveHead(4);
    Queue(pPacket);
  } else {
    Queue(pPacket);
  }

  return S_OK;
}

HRESULT CStreamParser::Flush()
{
  SAFE_DELETE(m_pPacketBuffer);
  m_queue.Clear();

  return S_OK;
}

HRESULT CStreamParser::Queue(Packet *pPacket) const
{
  return m_pPin->QueueFromParser(pPacket);
}

static Packet *InitPacket(Packet *pSource)
{
  Packet *pNew = new Packet();

  pNew = new Packet();
  pNew->StreamId = pSource->StreamId;
  pNew->bDiscontinuity = pSource->bDiscontinuity;
  pSource->bDiscontinuity = FALSE;

  pNew->bSyncPoint = pSource->bSyncPoint;
  pSource->bSyncPoint = FALSE;

  pNew->rtStart = pSource->rtStart;
  pSource->rtStart = Packet::INVALID_TIME;

  pNew->rtStop = pSource->rtStop;
  pSource->rtStop = Packet::INVALID_TIME;

  return pNew;
}

HRESULT CStreamParser::ParseH264AnnexB(Packet *pPacket)
{
  if (!m_pPacketBuffer) {
    m_pPacketBuffer = InitPacket(pPacket);
  }

  m_pPacketBuffer->Append(pPacket);

  BYTE *start = m_pPacketBuffer->GetData();
  BYTE *end = start + m_pPacketBuffer->GetDataSize();

  // Look for MPEG Start Code
  while(start <= end-4 && *(DWORD *)start != 0x01000000) {
    start++;
  }

  while(start <= end-4) {
    BYTE *next = start + 1;

    // Look for next start code.
    while(next <= end-4 && *(DWORD*)next != 0x01000000) {
      next++;
    }

    // End of buffer reached
    if(next >= end-4) {
      break;
    }

    size_t size = next - start;

    CH264Nalu Nalu;
    Nalu.SetBuffer(start, size, 0);

    Packet *p2 = NULL;

    while (Nalu.ReadNext()) {
      DWORD	dwNalLength =
        ((Nalu.GetDataLength() >> 24) & 0x000000ff) |
        ((Nalu.GetDataLength() >>  8) & 0x0000ff00) |
        ((Nalu.GetDataLength() <<  8) & 0x00ff0000) |
        ((Nalu.GetDataLength() << 24) & 0xff000000);

      Packet *p3 = new Packet();
      p3->SetDataSize(Nalu.GetDataLength() + sizeof(dwNalLength));
      memcpy(p3->GetData(), &dwNalLength, sizeof(dwNalLength));
      memcpy(p3->GetData() + sizeof(dwNalLength), Nalu.GetDataBuffer(), Nalu.GetDataLength());

      if (!p2) {
        p2 = p3;
      } else {
        p2->Append(p3);
        SAFE_DELETE(p3);
      }
    }

    p2->StreamId = m_pPacketBuffer->StreamId;
    p2->bDiscontinuity = m_pPacketBuffer->bDiscontinuity;
    m_pPacketBuffer->bDiscontinuity = FALSE;

    p2->bSyncPoint = m_pPacketBuffer->bSyncPoint;
    m_pPacketBuffer->bSyncPoint = FALSE;

    p2->rtStart = m_pPacketBuffer->rtStart;
    m_pPacketBuffer->rtStart = Packet::INVALID_TIME;
    p2->rtStop = m_pPacketBuffer->rtStop;
    m_pPacketBuffer->rtStop = Packet::INVALID_TIME;

    p2->pmt = m_pPacketBuffer->pmt;
    m_pPacketBuffer->pmt = NULL;

    m_queue.Queue(p2, FALSE);

    if(pPacket->rtStart != Packet::INVALID_TIME) {
      m_pPacketBuffer->rtStart = pPacket->rtStart;
      m_pPacketBuffer->rtStop = pPacket->rtStop;
      pPacket->rtStart = Packet::INVALID_TIME;
    }
    if(pPacket->bDiscontinuity) {
      m_pPacketBuffer->bDiscontinuity = pPacket->bDiscontinuity;
      pPacket->bDiscontinuity = FALSE;
    }
    if(pPacket->bSyncPoint) {
      m_pPacketBuffer->bSyncPoint = pPacket->bSyncPoint;
      pPacket->bSyncPoint = FALSE;
    }
    if(m_pPacketBuffer->pmt) {
      DeleteMediaType(m_pPacketBuffer->pmt);
    }

    m_pPacketBuffer->pmt = pPacket->pmt;
    pPacket->pmt = NULL;

    start = next;
  }

  if(start > m_pPacketBuffer->GetData()) {
    m_pPacketBuffer->RemoveHead(start - m_pPacketBuffer->GetData());
  }

  SAFE_DELETE(pPacket);

  do {
    pPacket = NULL;

    std::deque<Packet *>::iterator it;
    for (it = m_queue.GetQueue()->begin(); it != m_queue.GetQueue()->end(); ++it) {
      // Skip the first
      if (it == m_queue.GetQueue()->begin()) {
        continue;
      }

      Packet *p = *it;
      BYTE* pData = p->GetData();

      if((pData[4] & 0x1f) == 0x09) {
        m_fHasAccessUnitDelimiters = true;
      }

      if ((pData[4] & 0x1f) == 0x09 || (!m_fHasAccessUnitDelimiters && p->rtStart != Packet::INVALID_TIME)) {
        pPacket = p;
        break;
      }
    }

    if (pPacket) {
      Packet *p = m_queue.Get();
      Packet *p2 = NULL;
      while ((p2 = m_queue.Get()) != pPacket) {
        p->Append(p2);
        SAFE_DELETE(p2);
      }
      // Return
      m_queue.GetQueue()->push_front(pPacket);
      Queue(p);
    }
  } while (pPacket != NULL);

  return S_OK;
}

HRESULT CStreamParser::ParseVC1(Packet *pPacket)
{
  if (!m_pPacketBuffer) {
    m_pPacketBuffer = InitPacket(pPacket);
  }

  m_pPacketBuffer->Append(pPacket);

  BYTE *start = m_pPacketBuffer->GetData();
  BYTE *end = start + m_pPacketBuffer->GetDataSize();

  bool bSeqFound = false;
  while(start <= end-4) {
    if (*(DWORD *)start == 0x0D010000) {
      bSeqFound = true;
      break;
    } else if (*(DWORD *)start == 0x0F010000) {
      break;
    }
    start++;
  }

  while(start <= end-4) {
    BYTE *next = start+1;

    while(next <= end-4) {
      if (*(DWORD *)next == 0x0D010000) {
        if (bSeqFound) {
          break;
        }
        bSeqFound = true;
      } else if (*(DWORD*)next == 0x0F010000) {
        break;
      }
      next++;
    }

    if(next >= end-4) {
      break;
    }

    int size = next - start - 4;

    Packet *p2 = new Packet();
    p2->StreamId = m_pPacketBuffer->StreamId;
    p2->bDiscontinuity = m_pPacketBuffer->bDiscontinuity;
    m_pPacketBuffer->bDiscontinuity = FALSE;

    p2->bSyncPoint = m_pPacketBuffer->bSyncPoint;
    m_pPacketBuffer->bSyncPoint = FALSE;

    p2->rtStart = m_pPacketBuffer->rtStart;
    m_pPacketBuffer->rtStart = Packet::INVALID_TIME;

    p2->rtStop = m_pPacketBuffer->rtStop;
    m_pPacketBuffer->rtStop = Packet::INVALID_TIME;

    p2->pmt = m_pPacketBuffer->pmt;
    m_pPacketBuffer->pmt = NULL;

    p2->SetData(start, next - start);

    Queue(p2);

    if (pPacket->rtStart != Packet::INVALID_TIME) {
      m_pPacketBuffer->rtStart = pPacket->rtStart;
      m_pPacketBuffer->rtStop = pPacket->rtStart;
      pPacket->rtStart = Packet::INVALID_TIME;
    }

    if (pPacket->bDiscontinuity) {
      m_pPacketBuffer->bDiscontinuity = pPacket->bDiscontinuity;
      pPacket->bDiscontinuity = FALSE;
    }

    if (pPacket->bSyncPoint) {
      m_pPacketBuffer->bSyncPoint = pPacket->bSyncPoint;
      pPacket->bSyncPoint = FALSE;
    }

    m_pPacketBuffer->pmt = pPacket->pmt;
    pPacket->pmt = NULL;

    start = next;
    bSeqFound = (*(DWORD*)start == 0x0D010000);
  }

  if(start > m_pPacketBuffer->GetData()) {
    m_pPacketBuffer->RemoveHead(start - m_pPacketBuffer->GetData());
  }

  delete pPacket;

  return S_OK;
}