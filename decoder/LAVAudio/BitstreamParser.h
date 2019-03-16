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

#pragma once

#include "parser/dts.h"

struct GetBitContext;

class CBitstreamParser {
public:
  CBitstreamParser();
  ~CBitstreamParser();

  HRESULT Parse(AVCodecID codec, BYTE *pBuffer, DWORD dwSize, void *pParserContext);
  void Reset();

private:
  HRESULT ParseDTS(BYTE *pBuffer, DWORD dwSize);
  HRESULT ParseAC3(BYTE *pBuffer, DWORD dwSize, void *pParserContext);
  HRESULT ParseTrueHD(BYTE *pBuffer, DWORD dwSize);

public:
  DWORD m_dwSampleRate = 0;
  DWORD m_dwBlocks     = 0;
  DWORD m_dwFrameSize  = 0;
  DWORD m_dwSamples    = 0;

  BOOL m_bDTSHD        = FALSE;
  DTSHeader m_DTSHeader;

private:
  GetBitContext *m_gb    = nullptr;
  void *m_pParserContext = nullptr;
};
