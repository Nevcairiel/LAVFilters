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
 *
 *  Initial design and concept by Gabest and the MPC-HC Team, copyright under GPLv2
 */

#pragma once

#include "PacketQueue.h"

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
  HRESULT ParseVC1(Packet *pPacket);

  HRESULT Queue(Packet *pPacket) const;

private:
  CLAVOutputPin * const m_pPin;
  const std::string m_strContainer;

  GUID m_gSubtype;

  Packet *m_pPacketBuffer;

  CPacketQueue m_queue;

  // H264 specific
  bool m_fHasAccessUnitDelimiters;
};

