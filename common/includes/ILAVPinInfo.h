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

// {FD220BF4-3F26-4AD4-A4A9-348C1273A141}
DEFINE_GUID(IID_ILAVPinInfo,
0xfd220bf4, 0x3f26, 0x4ad4, 0xa4, 0xa9, 0x34, 0x8c, 0x12, 0x73, 0xa1, 0x41);

[uuid("FD220BF4-3F26-4AD4-A4A9-348C1273A141")]
interface ILAVPinInfo : public IUnknown
{
  // Get a set of flags that convey a special information for this kind of stream
  STDMETHOD_(DWORD,GetStreamFlags)() PURE;
#define LAV_STREAM_FLAG_H264_DTS  0x0000001 ///< H264 stream has DTS timestamps (AVI, MKV in MS-Compat mode)
#define LAV_STREAM_FLAG_RV34_MKV  0x0000002 ///< RV30/40 in MKV or similar container with horrible timstamps

  // Get the pixel format detected for this video stream
  STDMETHOD_(int,GetPixelFormat)() PURE;
};
