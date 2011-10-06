/*
 *      Copyright (C) 2011 Hendrik Leppkes
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

#define LAV_VERSION_MAJOR 0
#define LAV_VERSION_MINOR 37

#define LAV_AUDIO "LAV Audio Decoder"
#define LAV_VIDEO "LAV Video Decoder"
#define LAV_SPLITTER "LAV Splitter"

/////////////////////////////////////////////////////////
#define DO_MAKE_STR(x) #x
#define MAKE_STR(x) DO_MAKE_STR(x)

#define LAV_VERSION LAV_VERSION_MAJOR.LAV_VERSION_MINOR
#define LAV_VERSION_TAG LAV_VERSION_MAJOR, LAV_VERSION_MINOR

#define LAV_VERSION_STR MAKE_STR(LAV_VERSION)

#define ENABLE_DEBUG_LOGFILE 0
