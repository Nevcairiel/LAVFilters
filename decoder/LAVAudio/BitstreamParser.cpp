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
  m_dwSamples    = 0;
  m_bDTSHD       = FALSE;
  memset(&m_DTSHeader, 0, sizeof(m_DTSHeader));
}


HRESULT CBitstreamParser::Parse(AVCodecID codec, BYTE *pBuffer, DWORD dwSize, void *pParserContext)
{
  switch (codec) {
  case AV_CODEC_ID_DTS:
    return ParseDTS(pBuffer, dwSize);
  case AV_CODEC_ID_AC3:
  case AV_CODEC_ID_EAC3:
    return ParseAC3(pBuffer, dwSize, pParserContext);
  case AV_CODEC_ID_TRUEHD:
    return ParseTrueHD(pBuffer, dwSize);
  }
  return S_OK;
}

HRESULT CBitstreamParser::ParseDTS(BYTE *pBuffer, DWORD dwSize)
{
  int ret = parse_dts_header((DTSParserContext *)m_pParserContext, &m_DTSHeader, pBuffer, (unsigned)dwSize);
  if (ret < 0)
    return E_FAIL;

  m_bDTSHD = m_bDTSHD || m_DTSHeader.IsHD;

  m_dwBlocks      = m_DTSHeader.Blocks;
  m_dwSampleRate  = m_DTSHeader.SampleRate;
  m_dwFrameSize   = m_DTSHeader.FrameSize;
  m_dwSamples     = m_DTSHeader.SamplesPerBlock * m_dwBlocks;

  return S_OK;
}

HRESULT CBitstreamParser::ParseAC3(BYTE *pBuffer, DWORD dwSize, void *pParserContext)
{
  AACAC3ParseContext *ctx = (AACAC3ParseContext *)pParserContext;

  m_dwSampleRate = ctx->sample_rate;

  // E-AC3 always combines 6 blocks, resulting in 1536 samples
  m_dwSamples    = (ctx->codec_id == AV_CODEC_ID_EAC3) ? (6 * 256) : ctx->samples;

  return S_OK;
}

HRESULT CBitstreamParser::ParseTrueHD(BYTE *pBuffer, DWORD dwSize)
{
  if (AV_RB32(pBuffer + 4) == 0xf8726fba)
  {
    int nRatebits = pBuffer[8] >> 4;

    m_dwSampleRate = (nRatebits & 8 ? 44100 : 48000) << (nRatebits & 7);
    m_dwSamples = 24 * (40 << (nRatebits & 7));
  }

  return S_OK;
}
