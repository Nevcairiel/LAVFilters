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
#include "AnnexBConverter.h"

#include "libavutil/intreadwrite.h"

CAnnexBConverter::CAnnexBConverter(void)
{
}

CAnnexBConverter::~CAnnexBConverter(void)
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

HRESULT CAnnexBConverter::Convert(BYTE **poutbuf, int *poutbuf_size, const BYTE *buf, int buf_size)
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

HRESULT CAnnexBConverter::ConvertHEVCExtradata(BYTE **poutbuf, int *poutbuf_size, const BYTE *buf, int buf_size)
{
  if (buf_size < 23)
    return E_INVALIDARG;

  SetNALUSize(2);

  *poutbuf = nullptr;
  *poutbuf_size = 0;

  int num_arrays = buf[22];
  int remaining_size = buf_size - 23;
  buf += 23;
  for (int i = 0; i < num_arrays; i++) {
    if (remaining_size < 3) break;
    int cnt = AV_RB16(buf+1);
    buf += 3; remaining_size -= 3;
    for (int j = 0; j < cnt; j++) {
      if (remaining_size < 2) break;
      int len = AV_RB16(buf) + 2;
      if (remaining_size < len) break;
      alloc_and_copy(poutbuf, poutbuf_size, buf + 2, len - 2);
      buf += len; remaining_size -= len;
    }
  }

  return S_OK;
}
