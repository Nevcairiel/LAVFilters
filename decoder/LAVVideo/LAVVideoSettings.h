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

// {FA40D6E9-4D38-4761-ADD2-71A9EC5FD32F}
DEFINE_GUID(IID_ILAVVideoSettings, 
0xfa40d6e9, 0x4d38, 0x4761, 0xad, 0xd2, 0x71, 0xa9, 0xec, 0x5f, 0xd3, 0x2f);

// Codecs supported in the LAV Video configuration
// Codecs not listed here cannot be turned off. You can request codecs to be added to this list, if you wish.
typedef enum LAVVideoCodec {
  Codec_H264,
  Codec_VC1,
  Codec_MPEG1,
  Codec_MPEG2,
  Codec_MPEG4,
  Codec_MSMPEG4,
  Codec_VP8,
  Codec_WMV3,
  Codec_WMV12,
  Codec_MJPEG,
  Codec_Theora,
  Codec_FLV1,
  Codec_VP6,
  Codec_SVQ,
  Codec_H261,
  Codec_H263,
  Codec_Indeo,
  Codec_TSCC,
  Codec_Fraps,
  Codec_HuffYUV,
  Codec_QTRle,
  Codec_DV,
  Codec_Bink,
  Codec_Smacker,
  Codec_RV12,
  Codec_RV34,
  Codec_Lagarith,
  Codec_Cinepak,
  Codec_Camstudio,
  Codec_QPEG,
  Codec_ZLIB,
  Codec_QTRpza,
  Codec_PNG,
  Codec_MSRLE,

  Codec_NB            // Number of entrys (do not use when dynamically linking)
};

// Supported output pixel formats
typedef enum LAVOutPixFmts {
  LAVOutPixFmt_None = -1,
  LAVOutPixFmt_YV12,            // 4:2:0, 8bit, planar
  LAVOutPixFmt_NV12,            // 4:2:0, 8bit, Y planar, U/V packed
  LAVOutPixFmt_YUY2,            // 4:2:2, 8bit, packed
  LAVOutPixFmt_UYVY,            // 4:2:2, 8bit, packed
  LAVOutPixFmt_AYUV,            // 4:4:4, 8bit, packed
  LAVOutPixFmt_P010,            // 4:2:0, 10bit, Y planar, U/V packed
  LAVOutPixFmt_P210,            // 4:2:2, 10bit, Y planar, U/V packed
  LAVOutPixFmt_Y410,            // 4:4:4, 10bit, packed
  LAVOutPixFmt_P016,            // 4:2:0, 16bit, Y planar, U/V packed
  LAVOutPixFmt_P216,            // 4:2:2, 16bit, Y planar, U/V packed
  LAVOutPixFmt_Y416,            // 4:4:4, 16bit, packed
  LAVOutPixFmt_RGB32,           // 32-bit RGB (BGRA)
  LAVOutPixFmt_RGB24,           // 24-bit RGB (BGR)

  LAVOutPixFmt_NB               // Number of formats
} LAVOutPixFmts;

// LAV Audio configuration interface
[uuid("FA40D6E9-4D38-4761-ADD2-71A9EC5FD32F")]
interface ILAVVideoSettings : public IUnknown
{
  // Switch to Runtime Config mode. This will reset all settings to default, and no changes to the settings will be saved
  // You can use this to programmatically configure LAV Audio without interfering with the users settings in the registry.
  // Subsequent calls to this function will reset all settings back to defaults, even if the mode does not change.
  //
  // Note that calling this function during playback is not supported and may exhibit undocumented behaviour. 
  // For smooth operations, it must be called before LAV Audio is connected to other filters.
  STDMETHOD(SetRuntimeConfig)(BOOL bRuntimeConfig) = 0;

  // Configure which codecs are enabled
  // If vCodec is invalid (possibly a version difference), Get will return FALSE, and Set E_FAIL.
  STDMETHOD_(BOOL,GetFormatConfiguration)(LAVVideoCodec vCodec) = 0;
  STDMETHOD(SetFormatConfiguration)(LAVVideoCodec vCodec, BOOL bEnabled) = 0;

  // Set the number of threads to use for Multi-Threaded decoding (where available)
  //  0 = Auto Detect (based on number of CPU cores)
  //  1 = 1 Thread -- No Multi-Threading
  // >1 = Multi-Threading with the specified number of threads
  STDMETHOD(SetNumThreads)(DWORD dwNum) = 0;

  // Get the number of threads to use for Multi-Threaded decoding (where available)
  //  0 = Auto Detect (based on number of CPU cores)
  //  1 = 1 Thread -- No Multi-Threading
  // >1 = Multi-Threading with the specified number of threads
  STDMETHOD_(DWORD,GetNumThreads)() = 0;

  // Set wether the aspect ratio encoded in the stream should be forwarded to the renderer,
  // or the aspect ratio specified by the source filter should be kept.
  // TRUE  = AR from the Stream
  // FALSE = AR from the source filter
  STDMETHOD(SetStreamAR)(BOOL bStreamAR) = 0;

  // Get wether the aspect ratio encoded in the stream should be forwarded to the renderer,
  // or the aspect ratio specified by the source filter should be kept.
  // TRUE  = AR from the Stream
  // FALSE = AR from the source filter
  STDMETHOD_(BOOL,GetStreamAR)() = 0;

  // Set wether interlaced flags are reported in the media type to the renderer.
  STDMETHOD(SetReportInterlacedFlags)(BOOL bEnabled) = 0;

  // Get wether interlaced flags are reported in the media type to the renderer.
  STDMETHOD_(BOOL,GetReportInterlacedFlags)() = 0;

  // Configure which pixel formats are enabled for output
  // If pixFmt is invalid, Get will return FALSE and Set E_FAIL
  STDMETHOD_(BOOL,GetPixelFormat)(LAVOutPixFmts pixFmt) = 0;
  STDMETHOD(SetPixelFormat)(LAVOutPixFmts pixFmt, BOOL bEnabled) = 0;

  // Set wether high-quality pixel format conversion is performed
  STDMETHOD(SetHighQualityPixelFormatConversion)(BOOL bEnabled) = 0;

  // Get wether high-quality pixel format conversion is performed
  STDMETHOD_(BOOL,GetHighQualityPixelFormatConversion)() = 0;

  // Set the RGB output range for the YUV->RGB conversion
  // 0 = Auto (same as input), 1 = Limited (16-235), 2 = Full (0-255)
  STDMETHOD(SetRGBOutputRange)(DWORD dwRange) = 0;

  // Get the RGB output range for the YUV->RGB conversion
  // 0 = Auto (same as input), 1 = Limited (16-235), 2 = Full (0-255)
  STDMETHOD_(DWORD,GetRGBOutputRange)() = 0;

};
