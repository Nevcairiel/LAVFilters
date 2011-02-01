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

// {5B7DCFA5-589F-407C-8E32-AB2D0EFDBFCC}
DEFINE_GUID(IID_ILAVCAudioSettings, 
0x5b7dcfa5, 0x589f, 0x407c, 0x8e, 0x32, 0xab, 0x2d, 0xe, 0xfd, 0xbf, 0xcc);

typedef enum LAVCSampleFormat {
  SampleFormat_16,
  SampleFormat_24,
  SampleFormat_32,
  SampleFormat_U8,
  SampleFormat_FP32
};

[uuid("5B7DCFA5-589F-407C-8E32-AB2D0EFDBFCC")]
interface ILAVCAudioSettings : public IUnknown
{
  
  // Interfaces used by the Status page
  STDMETHOD_(BOOL,IsSampleFormatSupported)(LAVCSampleFormat sfCheck) = 0;
  STDMETHOD(GetInputDetails)(const char **pCodec, int *pnChannels, int *pSampleRate) = 0;
};
