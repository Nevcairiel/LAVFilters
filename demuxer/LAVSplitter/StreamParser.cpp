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


CStreamParser::CStreamParser(CLAVOutputPin *pPin, const char *szContainer)
  : m_pPin(pPin), m_strContainer(szContainer), m_pPacketBuffer(NULL), m_gSubtype(GUID_NULL)
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
  } else if (m_strContainer == "mpegts" && (m_gSubtype == MEDIASUBTYPE_WVC1 || m_gSubtype == MEDIASUBTYPE_WVC1_ARCSOFT || m_gSubtype == MEDIASUBTYPE_WVC1_CYBERLINK)) {
    ParseVC1(pPacket);
  } else {
    Queue(pPacket);
  }

  return S_OK;
}

HRESULT CStreamParser::Flush()
{
  SAFE_DELETE(m_pPacketBuffer);

  return S_OK;
}

HRESULT CStreamParser::Queue(Packet *pPacket) const
{
  return m_pPin->QueueFromParser(pPacket);
}

HRESULT CStreamParser::ParseVC1(Packet *pPacket)
{
  if (!m_pPacketBuffer) {
    m_pPacketBuffer = new Packet();
    m_pPacketBuffer->StreamId = pPacket->StreamId;
    m_pPacketBuffer->bDiscontinuity = pPacket->bDiscontinuity;
    pPacket->bDiscontinuity = FALSE;

    m_pPacketBuffer->bSyncPoint = pPacket->bSyncPoint;
    pPacket->bSyncPoint = FALSE;

    m_pPacketBuffer->rtStart = pPacket->rtStart;
    pPacket->rtStart = Packet::INVALID_TIME;

    m_pPacketBuffer->rtStop = pPacket->rtStop;
    pPacket->rtStop = Packet::INVALID_TIME;
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