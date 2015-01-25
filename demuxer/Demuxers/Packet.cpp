/*
 *      Copyright (C) 2010-2015 Hendrik Leppkes
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
  av_buffer_unref(&m_Buf);
}

int Packet::SetDataSize(size_t len)
{
  if (len < 0)
    return -1;

  // RemoveHead may have moved m_Data, make sure the data moved too.
  if (m_Buf && m_Buf->data != m_Data)
    memmove(m_Buf->data, m_Data, m_DataSize);

  // Re-allocate the buffer, if required
  if (!m_Buf || (size_t)m_Buf->size < (len + FF_INPUT_BUFFER_PADDING_SIZE)) {
    int ret = av_buffer_realloc(&m_Buf, (int)len + FF_INPUT_BUFFER_PADDING_SIZE);
    if (ret < 0)
      return ret;
  }
  m_Data = m_Buf->data;
  m_DataSize = len;

  return 0;
}

int Packet::SetData(const void* ptr, size_t len)
{
  if (!ptr || len < 0)
    return -1;
  int ret = SetDataSize(len);
  if (ret < 0)
    return ret;

  memcpy(m_Data, ptr, len);
  return 0;
}

int Packet::SetPacket(AVPacket *pkt)
{
  if (pkt->buf) {
    ASSERT(!m_Buf);
    m_Buf = av_buffer_ref(pkt->buf);
    if (!m_Buf)
      return -1;
    m_Data = m_Buf->data;
    m_DataSize = pkt->size;
    return 0;
  } else {
    return SetData(pkt->data, pkt->size);
  }
}

int Packet::Append(Packet *ptr)
{
  return AppendData(ptr->GetData(), ptr->GetDataSize());
}

int Packet::AppendData(const void* ptr, size_t len)
{
  size_t prevSize = m_DataSize;
  int ret = SetDataSize(m_DataSize + len);
  if (ret < 0)
    return ret;
  memcpy(m_Data+prevSize, ptr, len);
  return 0;
}

int Packet::RemoveHead(size_t count)
{
  count = min(count, m_DataSize);
  if (!m_Data || count < 0)
    return -1;
  m_Data += count;
  m_DataSize -= count;
  return 0;
}
