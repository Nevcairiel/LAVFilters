/*
*      Copyright (C) 2010-2021 Hendrik Leppkes
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
*  Contributions by Ti-BEN from the XBMC DSPlayer Project, also under GPLv2
*/

#pragma once

// {08CA03D5-1803-4FDC-8515-5B9DEC92437A}
DEFINE_GUID(IID_MediaSideDataFFMpeg,
  0x8ca03d5, 0x1803, 0x4fdc, 0x85, 0x15, 0x5b, 0x9d, 0xec, 0x92, 0x43, 0x7a);

extern "C" {
#include "libavcodec/avcodec.h"
}

#pragma pack(push, 1)
struct MediaSideDataFFMpeg
{
  AVPacketSideData *side_data;
  int side_data_elems;
};
#pragma pack(pop)
