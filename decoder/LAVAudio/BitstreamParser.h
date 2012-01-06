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

#pragma once

#include "parser/dts.h"

struct GetBitContext;

class CBitstreamParser {
public:
  CBitstreamParser();
  ~CBitstreamParser();

  HRESULT Parse(CodecID codec, BYTE *pBuffer, DWORD dwSize, void *pParserContext);
  void Reset();

private:
  HRESULT ParseDTS(BYTE *pBuffer, DWORD dwSize);
  HRESULT ParseAC3(BYTE *pBuffer, DWORD dwSize, void *pParserContext);

public:
  DWORD m_dwSampleRate;
  DWORD m_dwBlocks;
  DWORD m_dwFrameSize;
  DWORD m_dwBitRate;
  DWORD m_dwSamples;

  BOOL m_bDTSHD;
  DTSHeader m_DTSHeader;

private:
  GetBitContext *m_gb;

  void *m_pParserContext;
};
