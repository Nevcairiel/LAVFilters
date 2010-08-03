/*
 *      Copyright (C) 2005-2010 Team XBMC
 *      http://www.xbmc.org
 *
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
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 */

// pre-compiled header

#ifndef __LAVF_STDAFX_H__
#define __LAVF_STDAFX_H__

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN

// some common macros
#define SAFE_DELETE(pPtr) { delete pPtr; pPtr = NULL; }
#define CHECK_HR(hr) if (FAILED(hr)) { goto done; }
#define QI(i) (riid == __uuidof(i)) ? GetInterface((i*)this, ppv) :
#define QI2(i) (riid == IID_##i) ? GetInterface((i*)this, ppv) :
#define countof( array ) ( sizeof( array )/sizeof( array[0] ) )

// include headers
#include <Windows.h>
#include <atlstr.h>

#include <intsafe.h>
#include <streams.h>

extern "C" {
#define __STDC_CONSTANT_MACROS
#include "libavformat/avformat.h"
}

#endif
