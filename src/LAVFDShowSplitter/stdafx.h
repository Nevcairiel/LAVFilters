/*
 *      Copyright (C) 2010 Hendrik Leppkes
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
 */

// pre-compiled header

#pragma once

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN

// include headers
#include <Windows.h>

extern "C" {
#define __STDC_CONSTANT_MACROS
#include <stdint.h>
#include "libavformat/avformat.h"
}
#include "streams.h"

#include "DShowUtil.h"
#include "BaseDemuxer.h"
