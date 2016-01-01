/*
 *      Copyright (C) 2003-2006 Gabest
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

interface __declspec(uuid("01A5BBD3-FE71-487C-A2EC-F585918A8724"))
IKeyFrameInfo :
public IUnknown
{
	// Get the number of (known) keyframes in the file
	STDMETHOD (GetKeyFrameCount) (UINT& nKFs) = 0; // returns S_FALSE when every frame is a keyframe

	// Get the times of the key frames, if available.
	//
	// pFormat: GUID of the time format (see http://msdn.microsoft.com/en-us/library/dd407205(v=vs.85).aspx, usually TIME_FORMAT_MEDIA_TIME)
	// pKFs: Caller allocated memory for the timestamps of the keyframes (should be sizeof(REFERENCE_TIME) * nKFs at least!)
	// nKF: Number of keyframes requested/returned - no more then nKFs key frames will be returned.
	STDMETHOD (GetKeyFrames) (const GUID* pFormat, REFERENCE_TIME* pKFs, UINT& nKFs /* in, out*/) = 0;
};