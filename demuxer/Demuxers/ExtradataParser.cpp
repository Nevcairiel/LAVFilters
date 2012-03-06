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
#include "ExtradataParser.h"

#define MARKER if(BitRead(1) != 1) {ASSERT(0); return 0;}

CExtradataParser::CExtradataParser(BYTE *pExtradata, size_t extra_len)
  : CByteParser(pExtradata, extra_len)
{
}

CExtradataParser::~CExtradataParser()
{
}

bool CExtradataParser::NextMPEGStartCode(BYTE &code)
{
  BitByteAlign();
  DWORD dw = (DWORD)-1;
  do
	{
		if(!Remaining()) return false;
		dw = (dw << 8) | (BYTE)BitRead(8);
	}
	while((dw&0xffffff00) != 0x00000100);
  code = (BYTE)(dw&0xff);
  return true;
}

size_t CExtradataParser::ParseMPEGSequenceHeader(BYTE *pTarget)
{
  BYTE id = 0;
  while(Remaining() && id != 0xb3) {
    if(!NextMPEGStartCode(id)) {
      return 0;
    }
  }

  if(id != 0xb3) {
    return 0;
  }

  size_t shpos = Pos() - 4;
  BitRead(12); // Width
  BitRead(12); // Height
  BitRead(4); // AR
  BitRead(4); // FPS
  BitRead(18); // Bitrate
  MARKER;
  BitRead(10); // VBV
  BitRead(1); // Constrained Flag
  // intra quantisizer matrix
  if(BitRead(1)) {
    for (uint8_t i = 0; i < 64; i++) {
      BitRead(8);
    }
  }
  // non-intra quantisizer matrix
  if(BitRead(1)) {
    for (uint8_t i = 0; i < 64; i++) {
      BitRead(8);
    }
  }
  
  size_t shlen = Pos() - shpos;

  size_t shextpos = 0;
  size_t shextlen = 0;

  if(NextMPEGStartCode(id) && id == 0xb5) { // sequence header ext
    shextpos = Pos() - 4;
    
    int startcode = BitRead(4); // Start Code Id; TODO: DIfferent start code ids mean different length of da2a
    ASSERT(startcode == 1);
    
    BitRead(1); // Profile Level Escape
    BitRead(3); // Profile
    BitRead(4); // Level
    BitRead(1); // Progressive
    BitRead(2); // Chroma
    BitRead(2); // Width Extension
    BitRead(2); // Height Extension
    BitRead(12); // Bitrate Extension
    MARKER;
    BitRead(8); // VBV Buffer Size Extension
    BitRead(1); // Low Delay
    BitRead(2); // FPS Extension n
    BitRead(5); // FPS Extension d

    shextlen = Pos() - shextpos;
  }

  memcpy(pTarget, Start()+shpos, shlen);
  if (shextpos) {
    memcpy(pTarget+shlen, Start()+shextpos, shextlen);
  }
  return shlen + shextlen;
}
