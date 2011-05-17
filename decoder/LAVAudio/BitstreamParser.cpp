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

#include "libavcodec/aac_ac3_parser.h"

#include "parser/dts.h"


CBitstreamParser::CBitstreamParser()
{
  init_dts_parser((DTSParserContext **)&m_pParserContext);
  Reset();
}

CBitstreamParser::~CBitstreamParser()
{
  close_dts_parser((DTSParserContext **)&m_pParserContext);
}

void CBitstreamParser::Reset()
{
  m_dwBlocks     = 0;
  m_dwFrameSize  = 0;
  m_dwSampleRate = 0;
  m_dwBitRate    = 0;
  m_dwSamples    = 0;
  m_bDTSHD       = FALSE;
  memset(&m_DTSHeader, 0, sizeof(m_DTSHeader));
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
  parse_dts_header((DTSParserContext *)m_pParserContext, &m_DTSHeader, pBuffer, (unsigned)dwSize);
  m_bDTSHD = m_bDTSHD || m_DTSHeader.IsHD;

  m_dwBlocks      = m_DTSHeader.Blocks;
  m_dwSampleRate  = m_DTSHeader.SampleRate;
  m_dwBitRate     = m_DTSHeader.Bitrate;
  m_dwFrameSize   = m_DTSHeader.FrameSize;
  m_dwSamples     = m_DTSHeader.SamplesPerBlock * m_dwBlocks;

  return S_OK;
}

HRESULT CBitstreamParser::ParseAC3(BYTE *pBuffer, DWORD dwSize, void *pParserContext)
{
  AACAC3ParseContext *ctx = (AACAC3ParseContext *)pParserContext;

  m_dwBitRate    = ctx->bit_rate;
  m_dwSampleRate = ctx->sample_rate;
  return S_OK;
}
