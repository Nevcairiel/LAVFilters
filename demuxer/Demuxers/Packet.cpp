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

#include <stdafx.h>
#include "Packet.h"

Packet::Packet()
{
}

Packet::~Packet()
{
  DeleteMediaType(pmt);
  av_packet_free(&m_Packet);
}

int Packet::SetDataSize(int len)
{
  if (len < 0)
    return -1;

  if (len <= GetDataSize()) {
    av_shrink_packet(m_Packet, len);
    return 0;
  }

  if (!m_Packet) {
    m_Packet = av_packet_alloc();
    if (av_new_packet(m_Packet, len) < 0)
      return -1;
  }
  else
  {
    if (av_grow_packet(m_Packet, (len - m_Packet->size)) < 0)
      return -1;
  }

  return 0;
}

int Packet::SetData(const void* ptr, int len)
{
  if (!ptr || len < 0)
    return -1;
  int ret = SetDataSize(len);
  if (ret < 0)
    return ret;

  memcpy(m_Packet->data, ptr, len);
  return 0;
}

int Packet::SetPacket(AVPacket *pkt)
{
  ASSERT(!m_Packet);

  m_Packet = av_packet_alloc();
  if (!m_Packet)
    return -1;

  return av_packet_ref(m_Packet, pkt);
}

int Packet::Append(Packet *ptr)
{
  return AppendData(ptr->GetData(), ptr->GetDataSize());
}

int Packet::AppendData(const void* ptr, int len)
{
  int prevSize = GetDataSize();
  int ret = SetDataSize(prevSize + len);
  if (ret < 0)
    return ret;
  memcpy(m_Packet->data+prevSize, ptr, len);
  return 0;
}

int Packet::RemoveHead(int count)
{
  m_Packet->data += count;
  m_Packet->size -= (int)count;
  return 0;
}

bool Packet::CopyProperties(const Packet *src)
{
  StreamId = src->StreamId;
  bDiscontinuity = src->bDiscontinuity;
  bSyncPoint = src->bSyncPoint;
  bPosition = src->bPosition;
  rtStart = src->rtStart;
  rtStop = src->rtStop;
  rtPTS = src->rtPTS;
  rtDTS = src->rtDTS;
  if (src->pmt)
    pmt = CreateMediaType(src->pmt);
  dwFlags = src->dwFlags;

  return true;
}
