/*
 *      Copyright (C) 2010-2017 Hendrik Leppkes
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

// {8FBB906B-D1DB-4528-9498-563241CCD43D}
DEFINE_GUID(IID_ILAVDynamicAllocator, 
0x8fbb906b, 0xd1db, 0x4528, 0x94, 0x98, 0x56, 0x32, 0x41, 0xcc, 0xd4, 0x3d);

interface __declspec(uuid("8FBB906B-D1DB-4528-9498-563241CCD43D")) ILAVDynamicAllocator : public IUnknown
{
  // Query wether this allocator is using dynamic allocation of samples and will not run out of samples
  STDMETHOD_(BOOL,IsDynamicAllocator)() PURE;
};
