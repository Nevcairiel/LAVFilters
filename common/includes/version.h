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
 */

#pragma once

#define LAV_VERSION_MAJOR 0
#define LAV_VERSION_MINOR 35

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
