/*
 * $Id: IBufferInfo.h 2786 2010-12-17 16:42:55Z XhmikosR $
 *
 * (C) 2003-2006 Gabest
 * (C) 2006-2010 see AUTHORS
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
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

interface __declspec(uuid("46070104-1318-4A82-8822-E99AB7CD15C1"))
IBufferInfo :
public IUnknown {
	// Number of Buffers
	STDMETHOD_(int, GetCount()) = 0;
	// Get Info about Buffer "i" (0-based index up to count)
	// sampels: number of frames in the buffer
	// size: total size in bytes of the buffer
	STDMETHOD(GetStatus(int i, int& samples, int& size)) = 0;
	// Get priority of the demuxing thread
	STDMETHOD_(DWORD, GetPriority()) = 0;
};
