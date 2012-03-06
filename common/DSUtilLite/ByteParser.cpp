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
 *
 *  Initial design and concept by Gabest and the MPC-HC Team, copyright under GPLv2
 */

#include "stdafx.h"
#include "ByteParser.h"

CByteParser::CByteParser(const BYTE *pData, size_t length)
  : m_pData(pData), m_pCurrent(pData), m_pEnd(pData+length), m_dwLen(length), m_bitBuffer(0), m_bitLen(0)
{
}

CByteParser::~CByteParser()
{
}

unsigned int CByteParser::BitRead(unsigned int numBits, bool peek)
{
  ASSERT(numBits <= 32);
  ASSERT(numBits <= RemainingBits());

  if (numBits == 0 || RemainingBits() < numBits) { return 0; }

  bool atEnd = false;
  // Read more data in the buffer
  while(m_bitLen < numBits) {
    m_bitBuffer <<= 8;
    m_bitBuffer += !atEnd ? *m_pCurrent : 0;
    // Just a safety check so we don't cross the array boundary
    if (m_pCurrent < m_pEnd) { m_pCurrent++; } else { atEnd = true; }
    m_bitLen += 8;
  }

  // Number of superfluous bits in the buffer
  int bitlen = m_bitLen - numBits;

  // Compose the return value
  // Shift the value so the superfluous bits fall off, and then crop the result with an AND
  unsigned int ret = (m_bitBuffer >> bitlen) & ((1ui64 << numBits) - 1);

  // If we're not peeking, then update the buffer and remove the data we just read
  if(!peek) {
    m_bitBuffer &= ((1ui64 << bitlen) - 1);
    m_bitLen = bitlen;
  }

  return ret;
}

// Exponential Golomb Coding (with k = 0)
// As used in H.264/MPEG-4 AVC
// http://en.wikipedia.org/wiki/Exponential-Golomb_coding

unsigned CByteParser::UExpGolombRead() {
  int n = -1;
  for(BYTE b = 0; !b && RemainingBits(); n++) {
    b = BitRead(1);
  }
  if (!RemainingBits())
    return 0;
  return ((1 << n) | BitRead(n)) - 1;
}

int CByteParser::SExpGolombRead() {
  int k = UExpGolombRead() + 1;
  // Negative numbers are interleaved in the series
  // unsigned: 0, 1,  2, 3,  4, 5,  6, ...
  //   signed: 0, 1, -1, 2, -2, 3, -3, ....
  // So all even numbers are negative (last bit = 0)
  // Note that we added 1 to the unsigned value already, so the check is inverted
 if (k&1)
   return -(k>>1);
 else
   return (k>>1);
}

void CByteParser::Seek(DWORD pos)
{
  m_pCurrent = m_pData + min(m_dwLen, pos);
  BitFlush();
}
