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

#include "stdafx.h"
#include "LAVVideo.h"
#include "Media.h"

#include <MMReg.h>

#include "moreuuids.h"

typedef struct {
  const CLSID*        clsMinorType;
  const enum CodecID  nFFCodec;
} FFMPEG_SUBTYPE_MAP;

// Map Media Subtype <> FFMPEG Codec Id
FFMPEG_SUBTYPE_MAP lavc_video_codecs[] = {
  // H264
  { &MEDIASUBTYPE_H264, CODEC_ID_H264 },
  { &MEDIASUBTYPE_h264, CODEC_ID_H264 },
  { &MEDIASUBTYPE_X264, CODEC_ID_H264 },
  { &MEDIASUBTYPE_x264, CODEC_ID_H264 },
  { &MEDIASUBTYPE_AVC1, CODEC_ID_H264 },
  { &MEDIASUBTYPE_avc1, CODEC_ID_H264 },
  { &MEDIASUBTYPE_CCV1, CODEC_ID_H264 }, // Used by Haali Splitter

  // MPEG2
  { &MEDIASUBTYPE_MPEG2_VIDEO, CODEC_ID_MPEG2VIDEO },

  // MJPEG
  { &MEDIASUBTYPE_MJPG, CODEC_ID_MJPEG },

  // VC-1
  { &MEDIASUBTYPE_WVC1, CODEC_ID_VC1 },
  { &MEDIASUBTYPE_wvc1, CODEC_ID_VC1 },

  // WMV3
  { &MEDIASUBTYPE_WMV3, CODEC_ID_WMV3 },
  { &MEDIASUBTYPE_wmv3, CODEC_ID_WMV3 },

  // MPEG4 ASP
  { &MEDIASUBTYPE_XVID, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_xvid, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_DIVX, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_divx, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_DX50, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_dx50, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_MP4V, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_mp4v, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_M4S2, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_m4s2, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_MP4S, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_mp4s, CODEC_ID_MPEG4 },

  // Real
  { &MEDIASUBTYPE_RV40, CODEC_ID_RV40 },
};

// Define Input Media Types
const AMOVIESETUP_MEDIATYPE CLAVVideo::sudPinTypesIn[] = {
  // H264
  { &MEDIATYPE_Video, &MEDIASUBTYPE_H264 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_h264 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_X264 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_x264 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_AVC1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_avc1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_CCV1 },

  // MPEG2
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MPEG2_VIDEO },

  // MJPEG
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MJPG },

  // VC-1
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WVC1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wvc1 },

  // WMV3
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WMV3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wmv3 },

  // MPEG4 ASP
  { &MEDIATYPE_Video, &MEDIASUBTYPE_XVID },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_xvid },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DIVX },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_divx },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DX50 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_dx50 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MP4V },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mp4v },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_M4S2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_m4s2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MP4S },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mp4s },

  // Real
  { &MEDIATYPE_Video, &MEDIASUBTYPE_RV40 },
};
const int CLAVVideo::sudPinTypesInCount = countof(CLAVVideo::sudPinTypesIn);

// Define Output Media Types
const AMOVIESETUP_MEDIATYPE CLAVVideo::sudPinTypesOut[] = {
  { &MEDIATYPE_Video, &MEDIASUBTYPE_NV12 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_YV12 },
};
const int CLAVVideo::sudPinTypesOutCount = countof(CLAVVideo::sudPinTypesOut);

// Crawl the lavc_video_codecs array for the proper codec
CodecID FindCodecId(const CMediaType *mt)
{
  for (int i=0; i<countof(lavc_video_codecs); ++i) {
    if (mt->subtype == *lavc_video_codecs[i].clsMinorType) {
      return lavc_video_codecs[i].nFFCodec;
    }
  }
  return CODEC_ID_NONE;
}

void formatTypeHandler(const BYTE *format, const GUID *formattype, BITMAPINFOHEADER **pBMI, REFERENCE_TIME *prtAvgTime, DWORD *pDwAspectX, DWORD *pDwAspectY)
{
  REFERENCE_TIME rtAvg = 0;
  BITMAPINFOHEADER *bmi = NULL;
  DWORD dwAspectX = 0, dwAspectY = 0;

  if (*formattype == FORMAT_VideoInfo) {
    VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)format;
    rtAvg = vih->AvgTimePerFrame;
    bmi = &vih->bmiHeader;
  } else if (*formattype == FORMAT_VideoInfo2) {
    VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)format;
    rtAvg = vih2->AvgTimePerFrame;
    bmi = &vih2->bmiHeader;
    dwAspectX = vih2->dwPictAspectRatioX;
    dwAspectY = vih2->dwPictAspectRatioY;
  } else if (*formattype == FORMAT_MPEG2Video) {
    MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)format;
    rtAvg = mp2vi->hdr.AvgTimePerFrame;
    bmi = &mp2vi->hdr.bmiHeader;
    dwAspectX = mp2vi->hdr.dwPictAspectRatioX;
    dwAspectY = mp2vi->hdr.dwPictAspectRatioY;
  } else {
    ASSERT(FALSE);
  }

  if (pBMI) {
    *pBMI = bmi;
  }
  if (prtAvgTime) {
    *prtAvgTime = rtAvg;
  }
  if (pDwAspectX && pDwAspectY) {
    *pDwAspectX = dwAspectX;
    *pDwAspectY = dwAspectY;
  }
}

void getExtraData(const BYTE *format, const GUID *formattype, BYTE *extra, unsigned int *extralen)
{
  if (*formattype == FORMAT_VideoInfo || *formattype == FORMAT_VideoInfo2) {
    BITMAPINFOHEADER *pBMI = NULL;
    formatTypeHandler(format, formattype, &pBMI, NULL);
    if (extra)
      memcpy(extra, (BYTE *)pBMI + sizeof(BITMAPINFOHEADER), pBMI->biSize - sizeof(BITMAPINFOHEADER));
    if (extralen)
      *extralen = pBMI->biSize - sizeof(BITMAPINFOHEADER);
  } else if (*formattype == FORMAT_MPEG2Video) {
    MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)format;
    if (extra)
      memcpy(extra, (BYTE *)mp2vi->dwSequenceHeader, mp2vi->cbSequenceHeader);
    if (extralen)
      *extralen = mp2vi->cbSequenceHeader;
  }
}

CMediaType CreateMediaType(LONG biWidth, LONG biHeight, DWORD dwAspectX, DWORD dwAspectY, REFERENCE_TIME rtAvgTime)
{
  CMediaType mt;
  mt.SetType(&MEDIATYPE_Video);
  mt.SetSubtype(&MEDIASUBTYPE_NV12);    // TODO: don't hardcode
  mt.SetFormatType(&FORMAT_VideoInfo2);

  VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)mt.AllocFormatBuffer(sizeof(VIDEOINFOHEADER2));
  memset(vih2, 0, sizeof(VIDEOINFOHEADER2));

  vih2->rcSource.right = vih2->rcTarget.right = biWidth;
  vih2->rcSource.bottom = vih2->rcTarget.bottom = biHeight;
  vih2->AvgTimePerFrame = rtAvgTime;
  vih2->dwPictAspectRatioX = dwAspectX;
  vih2->dwPictAspectRatioY = dwAspectY;
  vih2->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  vih2->bmiHeader.biWidth = biWidth;
  vih2->bmiHeader.biHeight = biHeight;
  vih2->bmiHeader.biBitCount = 12; // TODO: don't hardcode
  vih2->bmiHeader.biPlanes = 3;    // TODO: don't hardcode
  vih2->bmiHeader.biSizeImage = biWidth * biHeight * vih2->bmiHeader.biBitCount >> 3;
  vih2->bmiHeader.biCompression = '21VN';  // TODO: don't hardcode

  // Always set interlace flags, the samples will be flagged appropriately then.
  vih2->dwInterlaceFlags = AMINTERLACE_IsInterlaced | AMINTERLACE_DisplayModeBobOrWeave;

  mt.SetSampleSize(vih2->bmiHeader.biSizeImage);

  return mt;
}