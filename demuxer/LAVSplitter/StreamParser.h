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

#include "PacketQueue.h"
#include "growarray.h"

class CLAVOutputPin;

class CStreamParser
{
public:
  CStreamParser(CLAVOutputPin *pPin, const char *szContainer);
  ~CStreamParser();

  HRESULT Parse(const GUID &gSubtype, Packet *pPacket);
  HRESULT Flush();

private:
  HRESULT ParseH264AnnexB(Packet *pPacket);
  HRESULT ParsePGS(Packet *pPacket);
  HRESULT ParseMOVText(Packet *pPacket);
  HRESULT ParseRawSSA(Packet *pPacket);
  HRESULT ParseAAC(Packet *pPacket);

  HRESULT Queue(Packet *pPacket) const;

private:
  CLAVOutputPin * const m_pPin;
  std::string m_strContainer;

  GUID m_gSubtype;

  Packet *m_pPacketBuffer;

  BOOL m_bPGSDropState;
  GrowableArray<BYTE> m_pgsBuffer;

  CPacketQueue m_queue;

  bool m_bHasAccessUnitDelimiters;
};

