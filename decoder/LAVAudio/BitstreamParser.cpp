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
#include "BitstreamParser.h"

#include "libavcodec/get_bits.h"
#include "libavcodec/dca.h" // contains DCA_HD_MARKER

#pragma warning( push )
#pragma warning( disable : 4305 )
#include "libavcodec/dcadata.h"
#pragma warning( pop )

#include "libavcodec/aac_ac3_parser.h"


CBitstreamParser::CBitstreamParser()
{
  m_gb = new GetBitContext();
  Reset();
}

CBitstreamParser::~CBitstreamParser()
{
  delete m_gb;
}

void CBitstreamParser::Reset()
{
  m_dwBlocks     = 0;
  m_dwFrameSize  = 0;
  m_dwSampleRate = 0;
  m_dwBitRate    = 0;
  m_dwSamples    = 0;
  m_bDTSHD       = FALSE;
}


HRESULT CBitstreamParser::Parse(CodecID codec, BYTE *pBuffer, DWORD dwSize, void *pParserContext)
{
  switch (codec) {
  case CODEC_ID_DTS:
    return ParseDTS(pBuffer, dwSize);
  case CODEC_ID_AC3:
    return ParseAC3(pBuffer, dwSize, pParserContext);
  }
  return S_OK;
}

HRESULT CBitstreamParser::ParseDTS(BYTE *pBuffer, DWORD dwSize)
{
  if (!m_bDTSHD) {
    uint32_t state;
    for (DWORD i = 0; i < dwSize; ++i) {
      state = (state << 8) | pBuffer[i];
      if (state == DCA_HD_MARKER) {
        m_bDTSHD = TRUE;
        break;
      }
    }
  }
  
  init_get_bits(m_gb, pBuffer, dwSize * 8);

  /* Sync code */
  get_bits(m_gb, 32);

  /* Frame type */
  get_bits(m_gb, 1);
  /* Samples deficit */
  DWORD dwSamplesPerBlock = get_bits(m_gb, 5) + 1;
  /* CRC present */
  get_bits(m_gb, 1);
  m_dwBlocks         = get_bits(m_gb, 7) + 1;
  m_dwFrameSize      = get_bits(m_gb, 14) + 1;
  /* amode */
  get_bits(m_gb, 6);
  m_dwSampleRate     = dca_sample_rates[get_bits(m_gb, 4)];
  
  int bit_rate_index = get_bits(m_gb, 5);
  m_dwBitRate        = dca_bit_rates[bit_rate_index];

  m_dwSamples        = dwSamplesPerBlock * m_dwBlocks;

  return S_OK;
}

HRESULT CBitstreamParser::ParseAC3(BYTE *pBuffer, DWORD dwSize, void *pParserContext)
{
  AACAC3ParseContext *ctx = (AACAC3ParseContext *)pParserContext;

  m_dwBitRate    = ctx->bit_rate;
  m_dwSampleRate = ctx->sample_rate;
  return S_OK;
}
