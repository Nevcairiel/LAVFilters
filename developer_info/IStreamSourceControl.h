/*
 *      Copyright (C) 2010-2016 Hendrik Leppkes
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

// NOTICE: This interface is still experimental and may change in the future. Do NOT use it yet unless you absolutely know what you are doing.

// Interface for high-level streaming source filters
// The source can implement proper seeking and duration retrieval based on its underlying protocol.
// This interface should be exposed on the output pin or the filter itself of the streaming source, similar to the IAsyncReader interface

// {C0BE9565-4C05-4644-9492-57547A4048DC}
DEFINE_GUID(IID_IStreamSourceControl,
  0xc0be9565, 0x4c05, 0x4644, 0x94, 0x92, 0x57, 0x54, 0x7a, 0x40, 0x48, 0xdc);

interface __declspec(uuid("C0BE9565-4C05-4644-9492-57547A4048DC"))
IStreamSourceControl : public IUnknown
{
  // Get the duration of the stream being played.
  // Duration is in DirectShow reference time, 100ns units.
  STDMETHOD(GetStreamDuration) (REFERENCE_TIME *prtDuration) PURE;
  
  // Seek the stream to a specified time
  //
  // Position is in DirectShow reference time, 100ns units.
  //
  // If the source returns a failure code, the demuxer will do byte-based seeking itself (ie. when the stream supports this)
  // On success, it'll re-open the stream and start reading from the start (byte position 0).
  STDMETHOD(SeekStream) (REFERENCE_TIME rtPosition) PURE;
};
