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
 *
 *  Contributions by Ti-BEN from the XBMC DSPlayer Project, also under GPLv2
 */

#include "stdafx.h"
#include "LAVFUtils.h"
#include "LAVFVideoHelper.h"
#include "moreuuids.h"
#include "BaseDemuxer.h"

#include "ExtradataParser.h"
#include "H264Nalu.h"

// 125fps is the highest we accept as "sane"
#define MIN_TIME_PER_FRAME 80000

CLAVFVideoHelper g_VideoHelper;

// Map codec ids to media subtypes
static FormatMapping video_map[] = {
  { CODEC_ID_H263,       &MEDIASUBTYPE_H263,         NULL,                   NULL },
  { CODEC_ID_H263I,      &MEDIASUBTYPE_H263,         NULL,                   NULL },
  { CODEC_ID_H264,       &MEDIASUBTYPE_AVC1,         MKTAG('A','V','C','1'), &FORMAT_MPEG2Video },
  { CODEC_ID_MPEG1VIDEO, &MEDIASUBTYPE_MPEG1Payload, NULL,                   &FORMAT_MPEGVideo  },
  { CODEC_ID_MPEG2VIDEO, &MEDIASUBTYPE_MPEG2_VIDEO,  NULL,                   &FORMAT_MPEG2Video },
  { CODEC_ID_VC1,        &MEDIASUBTYPE_WVC1,         MKTAG('W','V','C','1'), &FORMAT_VideoInfo2 },
  { CODEC_ID_RV10,       &MEDIASUBTYPE_RV10,         MKTAG('R','V','1','0'), NULL },
  { CODEC_ID_RV20,       &MEDIASUBTYPE_RV20,         MKTAG('R','V','2','0'), NULL },
  { CODEC_ID_RV30,       &MEDIASUBTYPE_RV30,         MKTAG('R','V','3','0'), NULL },
  { CODEC_ID_RV40,       &MEDIASUBTYPE_RV40,         MKTAG('R','V','4','0'), NULL },
};

CMediaType CLAVFVideoHelper::initVideoType(CodecID codecId, unsigned int &codecTag, std::string container)
{
  CMediaType mediaType;
  mediaType.InitMediaType();
  mediaType.majortype = MEDIATYPE_Video;
  mediaType.subtype = FOURCCMap(codecTag);
  mediaType.formattype = FORMAT_VideoInfo; //default value

    // Check against values from the map above
  for(unsigned i = 0; i < countof(video_map); ++i) {
    if (video_map[i].codec == codecId) {
      if (video_map[i].subtype)
        mediaType.subtype = *video_map[i].subtype;
      if (video_map[i].codecTag)
        codecTag = video_map[i].codecTag;
      if (video_map[i].format)
         mediaType.formattype = *video_map[i].format;
      break;
    }
  }

  switch(codecId)
  {
  // All these codecs should use VideoInfo2
  case CODEC_ID_ASV1:
  case CODEC_ID_ASV2:
  case CODEC_ID_FLV1:
  case CODEC_ID_HUFFYUV:
  case CODEC_ID_RV10:
  case CODEC_ID_RV20:
  case CODEC_ID_RV30:
  case CODEC_ID_RV40:
  case CODEC_ID_WMV3:
    mediaType.formattype = FORMAT_VideoInfo2;
    break;
  case CODEC_ID_MPEG4:
    if (container == "mp4") {
      mediaType.formattype = FORMAT_MPEG2Video;
    } else {
      mediaType.formattype = FORMAT_VideoInfo2;
    }
    break;
  }

  return mediaType;
}

DWORD avc_quant(BYTE *src, BYTE *dst, int extralen)
{
  DWORD cb = 0;
  BYTE* src_end = (BYTE *) src + extralen;
  BYTE* dst_end = (BYTE *) dst + extralen;
  src += 5;
  // Two runs, for sps and pps
  for (int i = 0; i < 2; i++)
  {
    for (int n = *(src++) & 0x1f; n > 0; n--)
    {
      unsigned len = (((unsigned)src[0] << 8) | src[1]) + 2;
      if(src + len > src_end || dst + len > dst_end) { ASSERT(0); break; }
      memcpy(dst, src, len);
      src += len;
      dst += len;
      cb += len;
    }
  }
  return cb;
}

DWORD avc_parse_annexb(BYTE *extra, int extrasize, BYTE *dst)
{
  DWORD dstSize = 0;

  CH264Nalu Nalu;
  Nalu.SetBuffer(extra, extrasize, 0);
  while (Nalu.ReadNext()) {
    const BYTE *data = Nalu.GetDataBuffer();
    if (((*data & 0x9f) == NALU_TYPE_SPS || (*data & 0x9f) == NALU_TYPE_PPS) && (*data & 0x60) != 0) {
      int16_t len = Nalu.GetDataLength();
      AV_WB16(dst+dstSize, len);
      dstSize += 2;
      memcpy(dst+dstSize, Nalu.GetDataBuffer(), Nalu.GetDataLength());
      dstSize += Nalu.GetDataLength();
    }
  }
  return dstSize;
}

VIDEOINFOHEADER *CLAVFVideoHelper::CreateVIH(const AVStream* avstream, ULONG *size)
{
  VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER*)CoTaskMemAlloc(ULONG(sizeof(VIDEOINFOHEADER) + avstream->codec->extradata_size));
  memset(pvi, 0, sizeof(VIDEOINFOHEADER));
  // Get the frame rate
  REFERENCE_TIME r_avg = av_rescale(DSHOW_TIME_BASE, avstream->r_frame_rate.den, avstream->r_frame_rate.num);
  if (avstream->r_frame_rate.den > 0 &&  avstream->r_frame_rate.num > 0 && (r_avg > MIN_TIME_PER_FRAME)) {
    pvi->AvgTimePerFrame = r_avg;
  } else if (avstream->avg_frame_rate.den > 0 &&  avstream->avg_frame_rate.num > 0) {
    pvi->AvgTimePerFrame = av_rescale(DSHOW_TIME_BASE, avstream->avg_frame_rate.den, avstream->avg_frame_rate.num);
  }
  pvi->dwBitErrorRate = 0;
  pvi->dwBitRate = avstream->codec->bit_rate;
  RECT empty_tagrect = {0,0,0,0};
  pvi->rcSource = empty_tagrect;//Some codecs like wmv are setting that value to the video current value
  pvi->rcTarget = empty_tagrect;
  pvi->rcTarget.right = pvi->rcSource.right = avstream->codec->width;
  pvi->rcTarget.bottom = pvi->rcSource.bottom = avstream->codec->height;

  memcpy((BYTE*)&pvi->bmiHeader + sizeof(BITMAPINFOHEADER), avstream->codec->extradata, avstream->codec->extradata_size);
  pvi->bmiHeader.biSize = ULONG(sizeof(BITMAPINFOHEADER) + avstream->codec->extradata_size);

  pvi->bmiHeader.biWidth = avstream->codec->width;
  pvi->bmiHeader.biHeight = avstream->codec->height;
  pvi->bmiHeader.biBitCount = avstream->codec->bits_per_coded_sample;
  // Validate biBitCount is set to something useful
  if (pvi->bmiHeader.biBitCount == 0 || avstream->codec->codec_id == CODEC_ID_RAWVIDEO) {
    pvi->bmiHeader.biBitCount = av_get_bits_per_pixel2(avstream->codec->pix_fmt);
  }
  pvi->bmiHeader.biSizeImage = DIBSIZE(pvi->bmiHeader); // Calculating this value doesn't really make alot of sense, but apparently some decoders freak out if its 0

  pvi->bmiHeader.biCompression = avstream->codec->codec_tag;
  //TOFIX The bitplanes is depending on the subtype
  pvi->bmiHeader.biPlanes = 1;
  pvi->bmiHeader.biClrUsed = 0;
  pvi->bmiHeader.biClrImportant = 0;
  pvi->bmiHeader.biYPelsPerMeter = 0;
  pvi->bmiHeader.biXPelsPerMeter = 0;

  *size = sizeof(VIDEOINFOHEADER) + avstream->codec->extradata_size;
  return pvi;
}

VIDEOINFOHEADER2 *CLAVFVideoHelper::CreateVIH2(const AVStream* avstream, ULONG *size, std::string container)
{
  int extra = 0;
  BYTE *extradata = NULL;
  BOOL bZeroPad = (avstream->codec->codec_id == CODEC_ID_VC1 && (container == "mpegts" || container == "mpeg"));

  // Create a VIH that we'll convert
  VIDEOINFOHEADER *vih = CreateVIH(avstream, size);

  if(avstream->codec->extradata_size > 0) {
    extra = avstream->codec->extradata_size;
    //increase extra size by one, because VIH2 requires one 0 byte between header and extra data
    if (bZeroPad) {
      extra++;
    }

    extradata = avstream->codec->extradata;
  }

  VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)CoTaskMemAlloc(sizeof(VIDEOINFOHEADER2) + extra); 
  memset(vih2, 0, sizeof(VIDEOINFOHEADER2));

  vih2->rcSource = vih->rcSource;
  vih2->rcTarget = vih->rcTarget;
  vih2->dwBitRate = vih->dwBitRate;
  vih2->dwBitErrorRate = vih->dwBitErrorRate;
  vih2->AvgTimePerFrame = vih->AvgTimePerFrame;

  // Calculate aspect ratio
  AVRational r = avstream->sample_aspect_ratio;
  AVRational rc = avstream->codec->sample_aspect_ratio;
  int num = vih->bmiHeader.biWidth, den = vih->bmiHeader.biHeight;
  if (r.den > 0 && r.num > 0 && (r.den > 1 || r.num > 1)) {
    av_reduce(&num, &den, (int64_t)r.num * num, (int64_t)r.den * den, 255);
  } else if (rc.den > 0 && rc.num > 0 && (rc.den > 1 || rc.num > 1)) {
    av_reduce(&num, &den, (int64_t)rc.num * num, (int64_t)rc.den * den, 255);
  } else {
    av_reduce(&num, &den, num, den, num);
  }
  vih2->dwPictAspectRatioX = num;
  vih2->dwPictAspectRatioY = den;

  memcpy(&vih2->bmiHeader, &vih->bmiHeader, sizeof(BITMAPINFOHEADER));
  vih2->bmiHeader.biSize = sizeof(BITMAPINFOHEADER) + extra;

  vih2->dwInterlaceFlags = 0;
  vih2->dwCopyProtectFlags = 0;
  vih2->dwControlFlags = 0;
  vih2->dwReserved2 = 0;

  if(extra) {
    // The first byte after the infoheader has to be 0 in mpeg-ts
    if (bZeroPad) {
      *((BYTE*)vih2 + sizeof(VIDEOINFOHEADER2)) = 0;
      // after that, the extradata .. size reduced by one again
      memcpy((BYTE*)vih2 + sizeof(VIDEOINFOHEADER2) + 1, extradata, extra - 1);
    } else {
      memcpy((BYTE*)vih2 + sizeof(VIDEOINFOHEADER2), extradata, extra);
    }
  }

  // Free the VIH that we converted
  CoTaskMemFree(vih);

  *size = sizeof(VIDEOINFOHEADER2) + extra;
  return vih2;
}

MPEG1VIDEOINFO *CLAVFVideoHelper::CreateMPEG1VI(const AVStream* avstream, ULONG *size)
{
  int extra = 0;
  BYTE *extradata = NULL;

  // Create a VIH that we'll convert
  VIDEOINFOHEADER *vih = CreateVIH(avstream, size);

  if(avstream->codec->extradata_size > 0) {
    extra = avstream->codec->extradata_size;
    extradata = avstream->codec->extradata;
  }

  MPEG1VIDEOINFO *mp1vi = (MPEG1VIDEOINFO *)CoTaskMemAlloc(sizeof(MPEG1VIDEOINFO) + extra);
  memset(mp1vi, 0, sizeof(MPEG1VIDEOINFO));

  // The MPEG1VI is a thin wrapper around a VIH, so its easy!
  memcpy(&mp1vi->hdr, vih, sizeof(VIDEOINFOHEADER));
  mp1vi->hdr.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

  mp1vi->dwStartTimeCode = 0; // is this not 0 anywhere..?
  mp1vi->hdr.bmiHeader.biPlanes = 0;
  mp1vi->hdr.bmiHeader.biCompression = 0;

  // copy extradata over
  if(extra) {
    CExtradataParser parser = CExtradataParser(extradata, extra);
    mp1vi->cbSequenceHeader = parser.ParseMPEGSequenceHeader(mp1vi->bSequenceHeader);
  }

  // Free the VIH that we converted
  CoTaskMemFree(vih);

  *size = SIZE_MPEG1VIDEOINFO(mp1vi);
  return mp1vi;
}

MPEG2VIDEOINFO *CLAVFVideoHelper::CreateMPEG2VI(const AVStream *avstream, ULONG *size, std::string container)
{
  int extra = 0;
  BYTE *extradata = NULL;

  // Create a VIH that we'll convert
  VIDEOINFOHEADER2 *vih2 = CreateVIH2(avstream, size);

  if(avstream->codec->extradata_size > 0) {
    extra = avstream->codec->extradata_size;
    extradata = avstream->codec->extradata;
  }

  MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)CoTaskMemAlloc(sizeof(MPEG2VIDEOINFO) + max(extra - 4, 0)); 
  memset(mp2vi, 0, sizeof(MPEG2VIDEOINFO));
  memcpy(&mp2vi->hdr, vih2, sizeof(VIDEOINFOHEADER2));
  mp2vi->hdr.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

  // Set profile/level if we know them
  mp2vi->dwProfile = (avstream->codec->profile != FF_PROFILE_UNKNOWN) ? avstream->codec->profile : 0;
  mp2vi->dwLevel = (avstream->codec->level != FF_LEVEL_UNKNOWN) ? avstream->codec->level : 0;
  //mp2vi->dwFlags = 4; // where do we get flags otherwise..?

  if(extra > 0)
  {
    BOOL bCopyUntouched = FALSE;
    // Don't even go there for mpeg-ts for now, we supply annex-b
    if(avstream->codec->codec_id == CODEC_ID_H264)
    {
      if (*(char *)extradata == 1) {
        if (extradata[1])
          mp2vi->dwProfile = extradata[1];
        if (extradata[3])
          mp2vi->dwLevel = extradata[3];
        mp2vi->dwFlags = (extradata[4] & 3) + 1;
        mp2vi->cbSequenceHeader = avc_quant(extradata,
          (BYTE *)(&mp2vi->dwSequenceHeader[0]), extra);
      } else {
        // MPEG-TS AnnexB
        mp2vi->dwFlags = sizeof(DWORD);
        mp2vi->cbSequenceHeader = avc_parse_annexb(extradata, extra, (BYTE *)(&mp2vi->dwSequenceHeader[0]));
      }
    } else if (avstream->codec->codec_id == CODEC_ID_MPEG2VIDEO) {
      CExtradataParser parser = CExtradataParser(extradata, extra);
      mp2vi->cbSequenceHeader = parser.ParseMPEGSequenceHeader((BYTE *)&mp2vi->dwSequenceHeader[0]);
      mp2vi->hdr.bmiHeader.biPlanes = 0;
      mp2vi->hdr.bmiHeader.biCompression = 0;
    } else {
      bCopyUntouched = TRUE;
    }
    if (bCopyUntouched) {
      mp2vi->cbSequenceHeader = extra;
      memcpy(&mp2vi->dwSequenceHeader[0], extradata, extra);
    }
  }

  // Free the VIH2 that we converted
  CoTaskMemFree(vih2);

  *size = SIZE_MPEG2VIDEOINFO(mp2vi);
  return mp2vi;
}
