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

  // MPEG1/2
  { &MEDIASUBTYPE_MPEG1Payload, CODEC_ID_MPEG1VIDEO },
  { &MEDIASUBTYPE_MPEG1Video,   CODEC_ID_MPEG1VIDEO },
  { &MEDIASUBTYPE_MPEG2_VIDEO,  CODEC_ID_MPEG2VIDEO },

  // MJPEG
  { &MEDIASUBTYPE_MJPG, CODEC_ID_MJPEG },

  // VC-1
  { &MEDIASUBTYPE_WVC1, CODEC_ID_VC1 },
  { &MEDIASUBTYPE_wvc1, CODEC_ID_VC1 },

  // WMV
  { &MEDIASUBTYPE_WMV1, CODEC_ID_WMV1 },
  { &MEDIASUBTYPE_wmv1, CODEC_ID_WMV1 },
  { &MEDIASUBTYPE_WMV2, CODEC_ID_WMV2 },
  { &MEDIASUBTYPE_wmv2, CODEC_ID_WMV2 },
  { &MEDIASUBTYPE_WMV3, CODEC_ID_WMV3 },
  { &MEDIASUBTYPE_wmv3, CODEC_ID_WMV3 },

  // VP8
  { &MEDIASUBTYPE_VP80, CODEC_ID_VP8 },

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

  // Flash
  { &MEDIASUBTYPE_FLV1, CODEC_ID_FLV1 },
  { &MEDIASUBTYPE_VP60, CODEC_ID_VP6 },
  { &MEDIASUBTYPE_VP61, CODEC_ID_VP6 },
  { &MEDIASUBTYPE_VP62, CODEC_ID_VP6 },
  { &MEDIASUBTYPE_VP6A, CODEC_ID_VP6A },
  { &MEDIASUBTYPE_VP6F, CODEC_ID_VP6F },

  // Misc Formats
  { &MEDIASUBTYPE_SVQ1, CODEC_ID_SVQ1 },
  { &MEDIASUBTYPE_SVQ3, CODEC_ID_SVQ3 },
  { &MEDIASUBTYPE_H261, CODEC_ID_H261 },
  { &MEDIASUBTYPE_h261, CODEC_ID_H261 },
  { &MEDIASUBTYPE_H263, CODEC_ID_H263 },
  { &MEDIASUBTYPE_h263, CODEC_ID_H263 },
  { &MEDIASUBTYPE_S263, CODEC_ID_H263 },
  { &MEDIASUBTYPE_s263, CODEC_ID_H263 },
  { &MEDIASUBTYPE_THEORA, CODEC_ID_THEORA },
  { &MEDIASUBTYPE_theora, CODEC_ID_THEORA },
  { &MEDIASUBTYPE_TSCC, CODEC_ID_TSCC },
  { &MEDIASUBTYPE_IV50, CODEC_ID_INDEO5 },
  { &MEDIASUBTYPE_IV31, CODEC_ID_INDEO3 },
  { &MEDIASUBTYPE_IV32, CODEC_ID_INDEO3 },
  { &MEDIASUBTYPE_dvsd, CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DVSD, CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_CDVH, CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_dv25, CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DV25, CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_dv50, CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DV50, CODEC_ID_DVVIDEO },

  // MS-MPEG4
  { &MEDIASUBTYPE_MPG4, CODEC_ID_MSMPEG4V1 },
  { &MEDIASUBTYPE_mpg4, CODEC_ID_MSMPEG4V1 },
  { &MEDIASUBTYPE_MP41, CODEC_ID_MSMPEG4V1 },
  { &MEDIASUBTYPE_mp41, CODEC_ID_MSMPEG4V1 },
  { &MEDIASUBTYPE_DIV1, CODEC_ID_MSMPEG4V1 },
  { &MEDIASUBTYPE_div1, CODEC_ID_MSMPEG4V1 },

  { &MEDIASUBTYPE_MP42, CODEC_ID_MSMPEG4V2 },
  { &MEDIASUBTYPE_mp42, CODEC_ID_MSMPEG4V2 },
  { &MEDIASUBTYPE_DIV2, CODEC_ID_MSMPEG4V2 },
  { &MEDIASUBTYPE_div2, CODEC_ID_MSMPEG4V2 },

  { &MEDIASUBTYPE_MP43, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_mp43, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_DIV3, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_div3, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_MPG3, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_mpg3, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_DIV4, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_div4, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_DIV5, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_div5, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_DIV6, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_div6, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_DVX3, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_dvx3, CODEC_ID_MSMPEG4V3 },

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

  // MPEG1/2
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MPEG1Payload  },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MPEG1Video    },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MPEG2_VIDEO   },

  // MJPEG
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MJPG },

  // VC-1
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WVC1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wvc1 },

  // WMV
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WMV1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wmv1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WMV2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wmv2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WMV3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wmv3 },

  // VP8
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP80 },

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

  // Flash
  { &MEDIATYPE_Video, &MEDIASUBTYPE_FLV1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP60 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP61 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP62 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP6A },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP6F },

  // Misc Formats
  { &MEDIATYPE_Video, &MEDIASUBTYPE_SVQ1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_SVQ3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_H261 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_h261 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_H263 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_h263 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_S263 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_s263 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_THEORA },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_theora },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_TSCC },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_IV50 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_IV31 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_IV32 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_dvsd },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVSD },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_CDVH },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_dv25 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DV25 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_dv50 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DV50 },

   // MS-MPEG4
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MPG4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mpg4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MP41 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mp41 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DIV1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_div1 },

  { &MEDIATYPE_Video, &MEDIASUBTYPE_MP42 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mp42 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DIV2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_div2 },

  { &MEDIATYPE_Video, &MEDIASUBTYPE_MP43 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mp43 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DIV3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_div3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MPG3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mpg3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DIV4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_div4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DIV5 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_div5 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DIV6 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_div6 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVX3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_dvx3 },

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
  } else if (*formattype == FORMAT_MPEGVideo) {
    MPEG1VIDEOINFO *mp1vi = (MPEG1VIDEOINFO *)format;
    rtAvg = mp1vi->hdr.AvgTimePerFrame;
    bmi = &mp1vi->hdr.bmiHeader;
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
  } else if (*formattype == FORMAT_MPEGVideo) {
    MPEG1VIDEOINFO *mp1vi = (MPEG1VIDEOINFO *)format;
    if (extra)
      memcpy(extra, (BYTE *)mp1vi->bSequenceHeader, mp1vi->cbSequenceHeader);
    if (extralen)
      *extralen = mp1vi->cbSequenceHeader;
  } else if (*formattype == FORMAT_MPEG2Video) {
    MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)format;
    if (extra)
      memcpy(extra, (BYTE *)mp2vi->dwSequenceHeader, mp2vi->cbSequenceHeader);
    if (extralen)
      *extralen = mp2vi->cbSequenceHeader;
  }
}
