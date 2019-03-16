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
 *  Initial design and concept by Gabest and the MPC-HC Team, copyright under GPLv2
 */

#include "stdafx.h"
#include "StreamParser.h"

#include "OutputPin.h"
#include "H264Nalu.h"

#pragma warning( push )
#pragma warning( disable : 4101 )
extern "C" {
#define AVCODEC_X86_MATHOPS_H
#include "libavcodec/get_bits.h"
}
#pragma warning( pop )

#define AAC_ADTS_HEADER_SIZE 7

//#define DEBUG_PGS_PARSER

CStreamParser::CStreamParser(CLAVOutputPin *pPin, const char *szContainer)
  : m_pPin(pPin), m_strContainer(szContainer)
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
  } else if (m_gSubtype == MEDIASUBTYPE_AVC1 && (m_strContainer == "mpegts" || pPacket->dwFlags & LAV_PACKET_H264_ANNEXB)) {
    ParseH264AnnexB(pPacket);
  } else if (m_gSubtype == MEDIASUBTYPE_HDMVSUB) {
    ParsePGS(pPacket);
  } else if (m_gSubtype == MEDIASUBTYPE_HDMV_LPCM_AUDIO) {
    pPacket->RemoveHead(4);
    Queue(pPacket);
  } else if (m_gSubtype == MEDIASUBTYPE_DVD_LPCM_AUDIO) {
    pPacket->RemoveHead(3);
    Queue(pPacket);
  } else if (pPacket->dwFlags & LAV_PACKET_MOV_TEXT) {
    ParseMOVText(pPacket);
  } else if (m_gSubtype == MEDIASUBTYPE_AAC && (m_strContainer != "matroska" && m_strContainer != "mp4")) {
    ParseAAC(pPacket);
  } else if (m_gSubtype == MEDIASUBTYPE_UTF8 && (pPacket->dwFlags & LAV_PACKET_SRT)) {
    ParseSRT(pPacket);
  } else if ((m_gSubtype == MEDIASUBTYPE_PCM || m_gSubtype == MEDIASUBTYPE_PCM_TWOS) && (pPacket->dwFlags & LAV_PACKET_PLANAR_PCM)) {
    ParsePlanarPCM(pPacket);
  } else {
    Queue(pPacket);
  }

  return S_OK;
}

HRESULT CStreamParser::Flush()
{
  DbgLog((LOG_TRACE, 10, L"CStreamParser::Flush()"));
  SAFE_DELETE(m_pPacketBuffer);
  m_queue.Clear();
  m_bPGSDropState = FALSE;
  m_bHasAccessUnitDelimiters = false;

  return S_OK;
}

HRESULT CStreamParser::Queue(Packet *pPacket) const
{
  return m_pPin->QueueFromParser(pPacket);
}

static Packet *InitPacket(Packet *pSource)
{
  Packet *pNew = nullptr;

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

  pNew->pmt = pSource->pmt;
  pSource->pmt = nullptr;

  return pNew;
}

#define MOVE_TO_H264_START_CODE(b, e) while(b <= e-4 && !((*(DWORD *)b == 0x01000000) || ((*(DWORD *)b & 0x00FFFFFF) == 0x00010000))) b++; if((b <= e-4) && *(DWORD *)b == 0x01000000) b++;

HRESULT CStreamParser::ParseH264AnnexB(Packet *pPacket)
{
  if (!m_pPacketBuffer) {
    m_pPacketBuffer = InitPacket(pPacket);
  }

  m_pPacketBuffer->Append(pPacket);

  BYTE *start = m_pPacketBuffer->GetData();
  BYTE *end = start + m_pPacketBuffer->GetDataSize();

  MOVE_TO_H264_START_CODE(start, end);

  while(start <= end-4) {
    BYTE *next = start + 1;

    MOVE_TO_H264_START_CODE(next, end);

    // End of buffer reached
    if(next >= end-4) {
      break;
    }

    size_t size = next - start;

    CH264Nalu Nalu;
    Nalu.SetBuffer(start, (int)size, 0);

    Packet *p2 = nullptr;

    while (Nalu.ReadNext()) {
      Packet *p3 = new Packet();
      p3->SetDataSize((int)Nalu.GetDataLength() + 4);

      // Write size of the NALU (Big Endian)
      AV_WB32(p3->GetData(), (uint32_t)Nalu.GetDataLength());
      memcpy(p3->GetData() + 4, Nalu.GetDataBuffer(), Nalu.GetDataLength());

      if (!p2) {
        p2 = p3;
      } else {
        p2->Append(p3);
        SAFE_DELETE(p3);
      }
    }

    if (!p2)
      break;

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
    m_pPacketBuffer->pmt = nullptr;

    m_queue.Queue(p2);

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
    pPacket->pmt = nullptr;

    start = next;
  }

  if(start > m_pPacketBuffer->GetData()) {
    m_pPacketBuffer->RemoveHead((int)(start - m_pPacketBuffer->GetData()));
  }

  SAFE_DELETE(pPacket);

  do {
    pPacket = nullptr;

    REFERENCE_TIME rtStart = Packet::INVALID_TIME, rtStop = rtStart = Packet::INVALID_TIME;

    std::deque<Packet *>::iterator it;
    for (it = m_queue.GetQueue()->begin(); it != m_queue.GetQueue()->end(); ++it) {
      // Skip the first
      if (it == m_queue.GetQueue()->begin()) {
        continue;
      }

      Packet *p = *it;
      BYTE* pData = p->GetData();

      if((pData[4]&0x1f) == 0x09) {
        m_bHasAccessUnitDelimiters = true;
      }

      if ((pData[4]&0x1f) == 0x09 || (!m_bHasAccessUnitDelimiters && p->rtStart != Packet::INVALID_TIME)) {
        pPacket = p;
        if (p->rtStart == Packet::INVALID_TIME && rtStart != Packet::INVALID_TIME) {
          p->rtStart = rtStart;
          p->rtStop = rtStop;
        }
        break;
      }

      if (rtStart == Packet::INVALID_TIME) {
        rtStart = p->rtStart;
        rtStop = p->rtStop;
      }
    }

    if (pPacket) {
      Packet *p = m_queue.Get();
      Packet *p2 = nullptr;
      while ((p2 = m_queue.Get()) != pPacket) {
        p->Append(p2);
        SAFE_DELETE(p2);
      }
      // Return
      m_queue.GetQueue()->push_front(pPacket);

      Queue(p);
    }
  } while (pPacket != nullptr);

  return S_OK;
}

HRESULT CStreamParser::ParsePGS(Packet *pPacket)
{
  const uint8_t *buf = pPacket->GetData();
  const size_t buf_size = pPacket->GetDataSize();

  const uint8_t *buf_end = buf + buf_size;
  uint8_t       segment_type;
  size_t        segment_length;

  if (buf_size < 3) {
    DbgLog((LOG_TRACE, 30, L"::ParsePGS(): Way too short PGS packet"));
    goto done;
  }

  m_pgsBuffer.SetSize(0);

  while((buf + 3) <= buf_end) {
    const uint8_t *segment_start = buf;
    const size_t segment_buf_len = buf_end - buf;
    segment_type   = AV_RB8(buf);
    segment_length = AV_RB16(buf+1);

    if (segment_length > (segment_buf_len - 3)) {
      DbgLog((LOG_TRACE, 10, "::ParsePGS(): segment_length bigger then input buffer: %d (buffer: %d)", segment_length, segment_buf_len));
      segment_length = segment_buf_len - 3;
    }

    buf += 3;

#ifdef DEBUG_PGS_PARSER
    DbgLog((LOG_TRACE, 50, L"::ParsePGS(): segment_type: 0x%x, segment_length: %d", segment_type, segment_length));
#endif

    // Presentation segment
    if (segment_type == 0x16 && segment_length > 10) {
      // Segment Layout
      // 2 bytes width
      // 2 bytes height
      // 1 unknown byte
      // 2 bytes id
      // 1 byte composition state (0x00 = normal, 0x40 = ACQU_POINT (?), 0x80 = epoch start (new frame), 0xC0 = epoch continue)
      // 2 unknown bytes
      // 1 byte object number
      uint8_t objectNumber = buf[10];
      // 2 bytes object ref id
      if (objectNumber == 0) {
        m_bPGSDropState = FALSE;
      } else if (segment_length >= 0x13) {
        // 1 byte window_id
        // 1 byte object_cropped_flag: 0x80, forced_on_flag = 0x040, 6bit reserved
        uint8_t forced_flag = buf[14];
        m_bPGSDropState = !(forced_flag & 0x40);
        // 2 bytes x
        // 2 bytes y
        // total length = 19 bytes
      }
#ifdef DEBUG_PGS_PARSER
      DbgLog((LOG_TRACE, 50, L"::ParsePGS(): Presentation Segment! obj.num: %d; state: 0x%x; dropping: %d", objectNumber, buf[7], m_bPGSDropState));
#endif
    }
    if (!m_bPGSDropState) {
      m_pgsBuffer.Append(segment_start, (DWORD)(segment_length + 3));
    }

    buf += segment_length;
  }

  if (m_pgsBuffer.GetCount() > 0) {
    pPacket->SetData(m_pgsBuffer.Ptr(), m_pgsBuffer.GetCount());
  } else {
    delete pPacket;
    return S_OK;
  }

done:
  return Queue(pPacket);
}

HRESULT CStreamParser::ParseMOVText(Packet *pPacket)
{
  size_t avail = pPacket->GetDataSize();
  BYTE *ptr = pPacket->GetData();
  if (avail > 2) {  
    unsigned size = (ptr[0] << 8) | ptr[1];
    if (size <= avail-2) {
      pPacket->RemoveHead(2);
      pPacket->SetDataSize(size);
      return Queue(pPacket);
    }
  }
  SAFE_DELETE(pPacket);
  return S_FALSE;
}

HRESULT CStreamParser::ParseAAC(Packet *pPacket)
{
  BYTE *pData = pPacket->GetData();

  GetBitContext gb;
  init_get_bits(&gb, pData, AAC_ADTS_HEADER_SIZE*8);
  // Check if its really ADTS
  if (get_bits(&gb, 12) == 0xfff)  {
    skip_bits1(&gb);              /* id */
    skip_bits(&gb, 2);            /* layer */
    int crc_abs = get_bits1(&gb); /* protection_absent */

    pPacket->RemoveHead(AAC_ADTS_HEADER_SIZE + 2*!crc_abs);
  }
  return Queue(pPacket);
}

static const char *read_srt_ts(const char *buf, int *ts_start, int *ts_end, int *x1, int *y1, int *x2, int *y2)
{
    int i, hs, ms, ss, he, me, se;

    for (i=0; i<2; i++) {
        /* try to read timestamps in either the first or second line */
        int c = sscanf_s(buf, "%d:%2d:%2d%*1[,.]%3d --> %d:%2d:%2d%*1[,.]%3d"
                       "%*[ ]X1:%u X2:%u Y1:%u Y2:%u",
                       &hs, &ms, &ss, ts_start, &he, &me, &se, ts_end,
                       x1, x2, y1, y2);
        buf += strcspn(buf, "\n") + 1;
        if (c >= 8) {
            *ts_start = 100*(ss + 60*(ms + 60*hs)) + *ts_start/10;
            *ts_end   = 100*(se + 60*(me + 60*he)) + *ts_end  /10;
            return buf;
        }
    }
    return nullptr;
}

HRESULT CStreamParser::ParseSRT(Packet *pPacket)
{
  int ts_start, ts_end, x1 = -1, y1 = -1, x2 = -1, y2 = -1;

  const char *ptr = (const char *)pPacket->GetData();
  const char *end = ptr + pPacket->GetDataSize();

  if (pPacket->GetDataSize() == 0)
    return S_FALSE;

  while(ptr && end > ptr && *ptr) {
    // Read the embeded timestamp and find the start of the subtitle
    ptr = read_srt_ts(ptr, &ts_start, &ts_end, &x1, &y1, &x2, &y2);
    if (ptr) {
      const char *linestart = ptr;
      while (end > ptr) {
        if (*ptr != ' ' && *ptr != '\n' && *ptr != '\r') {
          ptr += strcspn(ptr, "\n") + 1;
        } else {
          if (*ptr++ == '\n')
            break;
        }
      }
      size_t size = ptr - linestart;

      Packet *p = new Packet();
      p->pmt            = pPacket->pmt; pPacket->pmt = nullptr;
      p->bDiscontinuity = pPacket->bDiscontinuity;
      p->bSyncPoint     = pPacket->bSyncPoint;
      p->StreamId       = pPacket->StreamId;
      p->rtStart        = pPacket->rtStart;
      p->rtStop         = pPacket->rtStop;
      p->AppendData(linestart, (int)size);
      Queue(p);
    }
  }

  SAFE_DELETE(pPacket);
  return S_FALSE;
}

HRESULT CStreamParser::ParsePlanarPCM(Packet *pPacket)
{
  CMediaType mt = m_pPin->GetActiveMediaType();

  WORD nChannels = 0, nBPS = 0, nBlockAlign = 0;
  audioFormatTypeHandler(mt.Format(), mt.FormatType(), nullptr, &nChannels, &nBPS, &nBlockAlign, nullptr);

  // Mono needs no special handling
  if (nChannels == 1)
    return Queue(pPacket);

  Packet *out = new Packet();
  out->CopyProperties(pPacket);
  out->SetDataSize(pPacket->GetDataSize());

  int nBytesPerChannel = nBPS / 8;
  int nAudioBlocks = pPacket->GetDataSize() / nChannels;
  BYTE *out_data = out->GetData();
  const BYTE *in_data = pPacket->GetData();

  for (int i = 0; i < nAudioBlocks; i += nBytesPerChannel) {
    // interleave the channels into audio blocks
    for (int c = 0; c < nChannels; c++) {
      memcpy(out_data + (c * nBytesPerChannel), in_data + (nAudioBlocks * c), nBytesPerChannel);
    }
    // Skip to the next output block
    out_data += nChannels * nBytesPerChannel;

    // skip to the next input sample
    in_data += nBytesPerChannel;
  }

  return Queue(out);
}
