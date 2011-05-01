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
 */

#include "stdafx.h"
#include "LAVAudio.h"

#define LAV_BITSTREAM_BUFFER_SIZE 4096

HRESULT CLAVAudio::InitBitstreaming()
{
  if (m_avioBitstream)
    ShutdownBitstreaming();

  // Alloc buffer for the AVIO context
  BYTE *buffer = (BYTE *)CoTaskMemAlloc(LAV_BITSTREAM_BUFFER_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);
  if(!buffer)
    return E_FAIL;
  
  // Create AVIO context
  m_avioBitstream = avio_alloc_context(buffer, LAV_BITSTREAM_BUFFER_SIZE, 1, this, NULL, BSWriteBuffer, NULL);
  if(!m_avioBitstream) {
    SAFE_CO_FREE(buffer);
    return E_FAIL;
  }

  return S_OK;
}

HRESULT CLAVAudio::ShutdownBitstreaming()
{
  if (m_avioBitstream) {
    SAFE_CO_FREE(m_avioBitstream->buffer);
    av_freep(&m_avioBitstream);
  }
  return S_OK;
}

// Static function for the AVIO context that writes the buffer into our own output buffer
int CLAVAudio::BSWriteBuffer(void *opaque, uint8_t *buf, int buf_size)
{
  CLAVAudio *filter = (CLAVAudio *)opaque;
  filter->m_bsOutput.Append(buf, buf_size);
  return buf_size;
}
