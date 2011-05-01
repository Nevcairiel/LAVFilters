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
DEFINE_GUID(IID_ILAVAudioSettings, 
0x5b7dcfa5, 0x589f, 0x407c, 0x8e, 0x32, 0xab, 0x2d, 0xe, 0xfd, 0xbf, 0xcc);

// {A668B8F2-BA87-4F63-9D41-768F7DE9C50E}
DEFINE_GUID(IID_ILAVAudioStatus,
0xa668b8f2, 0xba87, 0x4f63, 0x9d, 0x41, 0x76, 0x8f, 0x7d, 0xe9, 0xc5, 0xe);


typedef enum LAVAudioSampleFormat {
  SampleFormat_16,
  SampleFormat_24,
  SampleFormat_32,
  SampleFormat_U8,
  SampleFormat_FP32,
  SampleFormat_Bitstream,
  SampleFormat_NB   // Dummy, do not use
};

[uuid("5B7DCFA5-589F-407C-8E32-AB2D0EFDBFCC")]
interface ILAVAudioSettings : public IUnknown
{
  STDMETHOD(GetDRC)(BOOL *pbDRCEnabled, int *piDRCLevel) = 0;
  STDMETHOD(SetDRC)(BOOL bDRCEnabled, int iDRCLevel) = 0;
  STDMETHOD(GetFormatConfiguration)(bool *bFormat) = 0;
  STDMETHOD(SetFormatConfiguration)(bool *bFormat) = 0;
  STDMETHOD(GetBitstreamConfig)(bool *bBitstreaming) = 0;
  STDMETHOD(SetBitstreamConfig)(bool *bBitstreaming) = 0;
};

[uuid("A668B8F2-BA87-4F63-9D41-768F7DE9C50E")]
interface ILAVAudioStatus : public IUnknown
{
  STDMETHOD_(BOOL,IsSampleFormatSupported)(LAVAudioSampleFormat sfCheck) = 0;
  STDMETHOD(GetInputDetails)(const char **pCodec, int *pnChannels, int *pSampleRate) = 0;
  STDMETHOD(GetOutputDetails)(const char **pDecodeFormat, const char **pOutputFormat, DWORD *pChannelMask) = 0;
  STDMETHOD(EnableVolumeStats)() = 0;
  STDMETHOD(DisableVolumeStats)() = 0;
  STDMETHOD(GetChannelVolumeAverage)(WORD nChannel, float *pfDb) = 0;
};
