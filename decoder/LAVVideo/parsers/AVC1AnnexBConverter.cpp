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
#include "AVC1AnnexBConverter.h"

#include "libavutil/intreadwrite.h"

CAVC1AnnexBConverter::CAVC1AnnexBConverter(void)
{
}

CAVC1AnnexBConverter::~CAVC1AnnexBConverter(void)
{
}

static HRESULT alloc_and_copy(uint8_t **poutbuf, int *poutbuf_size, const uint8_t *in, uint32_t in_size) {
  uint32_t offset = *poutbuf_size;
  uint8_t nal_header_size = offset ? 3 : 4;
  void *tmp;

  *poutbuf_size += in_size+nal_header_size;
  tmp = av_realloc(*poutbuf, *poutbuf_size);
  if (!tmp)
    return E_OUTOFMEMORY;
  *poutbuf = (uint8_t *)tmp;
  memcpy(*poutbuf+nal_header_size+offset, in, in_size);
  if (!offset) {
    AV_WB32(*poutbuf, 1);
  } else {
    (*poutbuf+offset)[0] = (*poutbuf+offset)[1] = 0;
    (*poutbuf+offset)[2] = 1;
  }

  return S_OK;
}

HRESULT CAVC1AnnexBConverter::Convert(BYTE **poutbuf, int *poutbuf_size, const BYTE *buf, int buf_size)
{
  int32_t nal_size;
  const uint8_t *buf_end = buf + buf_size;

  *poutbuf_size = 0;

  do {
    if (buf + m_NaluSize > buf_end)
      goto fail;

    if (m_NaluSize == 1) {
      nal_size = buf[0];
    } else if (m_NaluSize == 2) {
      nal_size = AV_RB16(buf);
    } else {
      nal_size = AV_RB32(buf);
      if (m_NaluSize == 3)
        nal_size >>= 8;
    }

    buf += m_NaluSize;

    if (buf + nal_size > buf_end || nal_size < 0)
      goto fail;

    if (FAILED(alloc_and_copy(poutbuf, poutbuf_size, buf, nal_size)))
      goto fail;

    buf += nal_size;
    buf_size -= (nal_size + m_NaluSize);
  } while (buf_size > 0);

  return S_OK;
fail:
  av_freep(poutbuf);
  return E_FAIL;
}
