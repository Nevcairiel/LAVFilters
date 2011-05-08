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

#pragma once

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

  BOOL m_bDTSHD;

private:
  GetBitContext *m_gb;
};
