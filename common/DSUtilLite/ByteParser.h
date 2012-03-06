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

#pragma once

#include <stdint.h>

/**
* Byte Parser Utility Class
*/
class CByteParser
{
public:
  /** Construct a Byte Parser to parse the given BYTE array with the given length */
  CByteParser(const BYTE *pData, size_t length);
  virtual ~CByteParser();

  /** Read 1 to 32 Bits from the Byte Array. If peek is set, the data will just be returned, and the buffer not advanced. */
  unsigned int BitRead(unsigned int numBits, bool peek = false);

  /** Read a unsigned number in Exponential Golomb encoding (with k = 0) */
  unsigned int UExpGolombRead();
  /** Read a signed number in Exponential Golomb encoding (with k = 0) */
  int SExpGolombRead();

  /** Seek to the position (in bytes) in the byte array */
  void Seek(DWORD pos);

  /** Pointer to the start of the byte array */
  const BYTE *Start() const { return m_pData; }
  /** Pointer to the end of the byte array */
  const BYTE *End() const { return m_pEnd; }

  /** Overall length (in bytes) of the byte array */
  size_t Length() const { return m_dwLen; }
  /** Current byte position in the array. Any incomplete bytes ( buffer < 8 bits ) will not be counted */
  size_t Pos() const { return unsigned int(m_pCurrent - m_pData) - (m_bitLen>>3); }
  /** Number of bytes remaining in the array */
  size_t Remaining() const { return Length() - Pos(); }

  /** Number of bits remaining */
  size_t RemainingBits() const { return m_bitLen + (8 * (m_pEnd - m_pCurrent)); }

  /** Flush any bits currently in the buffer */
  void BitFlush() { m_bitLen = 0; m_bitBuffer = 0; }
  /** Skip bits until the next byte border */
  void BitByteAlign() { BitRead(m_bitLen & 7); }

private:
  // Pointer to the start of the data buffer
  const BYTE * const m_pData;

  // Pointer to the current position in the data buffer
  const BYTE *m_pCurrent;
  // Pointer to the end in the data buffer
  const BYTE * const m_pEnd;

  const size_t m_dwLen;

  unsigned __int64 m_bitBuffer;
  unsigned int m_bitLen;
};
