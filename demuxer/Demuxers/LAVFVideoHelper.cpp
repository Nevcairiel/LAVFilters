/*
 *      Copyright (C) 2010-2019 Hendrik Leppkes
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

// 250fps is the highest we accept as "sane"
#define MIN_TIME_PER_FRAME 40000
// 8fps is the lowest that is "sane" in our definition
#define MAX_TIME_PER_FRAME 1250000

CLAVFVideoHelper g_VideoHelper;

// Map codec ids to media subtypes
static FormatMapping video_map[] = {
  { AV_CODEC_ID_H263,       &MEDIASUBTYPE_H263,         0,                      nullptr },
  { AV_CODEC_ID_H263I,      &MEDIASUBTYPE_I263,         0,                      nullptr },
  { AV_CODEC_ID_H264,       &MEDIASUBTYPE_AVC1,         0,                      &FORMAT_MPEG2Video },
  { AV_CODEC_ID_HEVC,       &MEDIASUBTYPE_HEVC,         0,                      &FORMAT_MPEG2Video },
  { AV_CODEC_ID_MPEG1VIDEO, &MEDIASUBTYPE_MPEG1Payload, 0,                      &FORMAT_MPEGVideo  },
  { AV_CODEC_ID_MPEG2VIDEO, &MEDIASUBTYPE_MPEG2_VIDEO,  0,                      &FORMAT_MPEG2Video },
  { AV_CODEC_ID_RV10,       &MEDIASUBTYPE_RV10,         MKTAG('R','V','1','0'), &FORMAT_VideoInfo2 },
  { AV_CODEC_ID_RV20,       &MEDIASUBTYPE_RV20,         MKTAG('R','V','2','0'), &FORMAT_VideoInfo2 },
  { AV_CODEC_ID_RV30,       &MEDIASUBTYPE_RV30,         MKTAG('R','V','3','0'), &FORMAT_VideoInfo2 },
  { AV_CODEC_ID_RV40,       &MEDIASUBTYPE_RV40,         MKTAG('R','V','4','0'), &FORMAT_VideoInfo2 },
  { AV_CODEC_ID_AMV,        &MEDIASUBTYPE_AMVV,         MKTAG('A','M','V','V'), nullptr },
  { AV_CODEC_ID_TIFF,       &MEDIASUBTYPE_TIFF,         MKTAG('T','I','F','F'), nullptr },
  { AV_CODEC_ID_PNG,        &MEDIASUBTYPE_PNG,          MKTAG('P','N','G',' '), nullptr },
  { AV_CODEC_ID_BMP,        &MEDIASUBTYPE_BMP,          MKTAG('B','M','P',' '), nullptr },
  { AV_CODEC_ID_GIF,        &MEDIASUBTYPE_GIF,          MKTAG('G','I','F',' '), nullptr },
  { AV_CODEC_ID_TARGA,      &MEDIASUBTYPE_TGA,          MKTAG('T','G','A',' '), nullptr },
  { AV_CODEC_ID_VP8,        &MEDIASUBTYPE_VP80,         MKTAG('V','P','8','0'), &FORMAT_VideoInfo2 },
  { AV_CODEC_ID_VP9,        &MEDIASUBTYPE_VP90,         MKTAG('V','P','9','0'), &FORMAT_VideoInfo2 },
  { AV_CODEC_ID_AV1,        &MEDIASUBTYPE_AV01,         MKTAG('A','V','0','1'), &FORMAT_VideoInfo2 },
  { AV_CODEC_ID_CFHD,       &MEDIASUBTYPE_CFHD,         MKTAG('C','F','H','D'), &FORMAT_VideoInfo2 },
};

CMediaType CLAVFVideoHelper::initVideoType(AVCodecID codecId, unsigned int &codecTag, std::string container)
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
  case AV_CODEC_ID_ASV1:
  case AV_CODEC_ID_ASV2:
  case AV_CODEC_ID_FLV1:
  case AV_CODEC_ID_HUFFYUV:
  case AV_CODEC_ID_WMV3:
    mediaType.formattype = FORMAT_VideoInfo2;
    break;
  case AV_CODEC_ID_MPEG4:
    if (container == "mp4") {
      mediaType.formattype = FORMAT_MPEG2Video;
    } else if (container == "mpegts") {
      mediaType.formattype = FORMAT_VideoInfo2;
      mediaType.subtype = MEDIASUBTYPE_MP4V;
    } else {
      mediaType.formattype = FORMAT_VideoInfo2;
    }
    break;
  case AV_CODEC_ID_VC1:
    if (codecTag != MKTAG('W','M','V','A'))
      codecTag = MKTAG('W','V','C','1');
    mediaType.formattype = FORMAT_VideoInfo2;
    mediaType.subtype = FOURCCMap(codecTag);
    break;
  case AV_CODEC_ID_DVVIDEO:
    if (codecTag == 0)
      mediaType.subtype = MEDIASUBTYPE_DVCP;
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

size_t avc_parse_annexb(BYTE *extra, int extrasize, BYTE *dst)
{
  size_t dstSize = 0;

  CH264Nalu Nalu;
  Nalu.SetBuffer(extra, extrasize, 0);
  while (Nalu.ReadNext()) {
    if (Nalu.GetType() == NALU_TYPE_SPS || Nalu.GetType() == NALU_TYPE_PPS) {
      size_t len = Nalu.GetDataLength();
      AV_WB16(dst+dstSize, (uint16_t)len);
      dstSize += 2;
      memcpy(dst+dstSize, Nalu.GetDataBuffer(), Nalu.GetDataLength());
      dstSize += Nalu.GetDataLength();
    }
  }
  return dstSize;
}

VIDEOINFOHEADER *CLAVFVideoHelper::CreateVIH(const AVStream* avstream, ULONG *size, std::string container)
{
  VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER*)CoTaskMemAlloc(ULONG(sizeof(VIDEOINFOHEADER) + avstream->codecpar->extradata_size));
  if (!pvi) return nullptr;
  memset(pvi, 0, sizeof(VIDEOINFOHEADER));
  // Get the frame rate
  REFERENCE_TIME r_avg = 0, avg_avg = 0, tb_avg = 0;
  if (avstream->r_frame_rate.den > 0 &&  avstream->r_frame_rate.num > 0) {
    r_avg = av_rescale(DSHOW_TIME_BASE, avstream->r_frame_rate.den, avstream->r_frame_rate.num);
  }
  if (avstream->avg_frame_rate.den > 0 &&  avstream->avg_frame_rate.num > 0) {
    avg_avg = av_rescale(DSHOW_TIME_BASE, avstream->avg_frame_rate.den, avstream->avg_frame_rate.num);
  }
#pragma warning(push)
#pragma warning(disable: 4996)
  if (avstream->codec->time_base.den > 0 &&  avstream->codec->time_base.num > 0 && avstream->codec->ticks_per_frame > 0) {
    tb_avg = av_rescale(DSHOW_TIME_BASE, avstream->codec->time_base.num * avstream->codec->ticks_per_frame, avstream->codec->time_base.den);
  }
#pragma warning(pop)

  DbgLog((LOG_TRACE, 10, L"CreateVIH: r_avg: %I64d, avg_avg: %I64d, tb_avg: %I64d", r_avg, avg_avg, tb_avg));
  if (r_avg >= MIN_TIME_PER_FRAME && r_avg <= MAX_TIME_PER_FRAME)
    pvi->AvgTimePerFrame = r_avg;
  else if (avg_avg >= MIN_TIME_PER_FRAME && avg_avg <= MAX_TIME_PER_FRAME)
    pvi->AvgTimePerFrame = avg_avg;
  else if (tb_avg >= MIN_TIME_PER_FRAME && tb_avg <= MAX_TIME_PER_FRAME)
    pvi->AvgTimePerFrame = tb_avg;
  else
    pvi->AvgTimePerFrame = r_avg;

  if ((container == "matroska" || container == "mp4") && r_avg && tb_avg && (avstream->codecpar->codec_id == AV_CODEC_ID_H264 || avstream->codecpar->codec_id == AV_CODEC_ID_MPEG2VIDEO)) {
    float factor = (float)r_avg / (float)tb_avg;
    if ((factor > 0.4 && factor < 0.6) || (factor > 1.9 && factor < 2.1)) {
      pvi->AvgTimePerFrame = tb_avg;
    }
  }

  pvi->dwBitErrorRate = 0;
  pvi->dwBitRate = (DWORD)avstream->codecpar->bit_rate;
  RECT empty_tagrect = {0,0,0,0};
  pvi->rcSource = empty_tagrect;//Some codecs like wmv are setting that value to the video current value
  pvi->rcTarget = empty_tagrect;
  pvi->rcTarget.right = pvi->rcSource.right = avstream->codecpar->width;
  pvi->rcTarget.bottom = pvi->rcSource.bottom = avstream->codecpar->height;

  memcpy((BYTE*)&pvi->bmiHeader + sizeof(BITMAPINFOHEADER), avstream->codecpar->extradata, avstream->codecpar->extradata_size);
  pvi->bmiHeader.biSize = ULONG(sizeof(BITMAPINFOHEADER) + avstream->codecpar->extradata_size);

  pvi->bmiHeader.biWidth = avstream->codecpar->width;
  pvi->bmiHeader.biHeight = avstream->codecpar->height;
  pvi->bmiHeader.biBitCount = avstream->codecpar->bits_per_coded_sample;
  // Validate biBitCount is set to something useful
  if ((pvi->bmiHeader.biBitCount == 0 || avstream->codecpar->codec_id == AV_CODEC_ID_RAWVIDEO) && avstream->codecpar->format != AV_PIX_FMT_NONE) {
    const AVPixFmtDescriptor *pixdecs = av_pix_fmt_desc_get((AVPixelFormat)avstream->codecpar->format);
    if (pixdecs)
      pvi->bmiHeader.biBitCount = av_get_bits_per_pixel(pixdecs);
  }
  pvi->bmiHeader.biSizeImage = DIBSIZE(pvi->bmiHeader); // Calculating this value doesn't really make alot of sense, but apparently some decoders freak out if its 0

  pvi->bmiHeader.biCompression = avstream->codecpar->codec_tag;
  //TOFIX The bitplanes is depending on the subtype
  pvi->bmiHeader.biPlanes = 1;
  pvi->bmiHeader.biClrUsed = 0;
  pvi->bmiHeader.biClrImportant = 0;
  pvi->bmiHeader.biYPelsPerMeter = 0;
  pvi->bmiHeader.biXPelsPerMeter = 0;

  *size = sizeof(VIDEOINFOHEADER) + avstream->codecpar->extradata_size;
  return pvi;
}

#define VC1_CODE_RES0 0x00000100
#define IS_VC1_MARKER(x) (((x) & ~0xFF) == VC1_CODE_RES0)

VIDEOINFOHEADER2 *CLAVFVideoHelper::CreateVIH2(const AVStream* avstream, ULONG *size, std::string container)
{
  int extra = 0;
  BYTE *extradata = nullptr;
  BOOL bZeroPad = FALSE;
  if (avstream->codecpar->codec_id == AV_CODEC_ID_VC1 && avstream->codecpar->extradata_size) {
    int i = 0;
    for (i = 0; i < (avstream->codecpar->extradata_size-4); i++) {
      uint32_t code = AV_RB32(avstream->codecpar->extradata + i);
      if (IS_VC1_MARKER(code))
        break;
    }
    if (i == 0) {
      bZeroPad = TRUE;
    } else if (i > 1) {
      DbgLog((LOG_TRACE, 10, L"CLAVFVideoHelper::CreateVIH2(): VC-1 extradata does not start at position 0/1, but %d", i));
    }
  }

  // Create a VIH that we'll convert
  VIDEOINFOHEADER *vih = CreateVIH(avstream, size, container);
  if (!vih) return nullptr;

  if(avstream->codecpar->extradata_size > 0) {
    extra = avstream->codecpar->extradata_size;
    //increase extra size by one, because VIH2 requires one 0 byte between header and extra data
    if (bZeroPad) {
      extra++;
    }

    extradata = avstream->codecpar->extradata;
  }

  VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)CoTaskMemAlloc(sizeof(VIDEOINFOHEADER2) + extra); 
  if (!vih2) return nullptr;
  memset(vih2, 0, sizeof(VIDEOINFOHEADER2));

  vih2->rcSource = vih->rcSource;
  vih2->rcTarget = vih->rcTarget;
  vih2->dwBitRate = vih->dwBitRate;
  vih2->dwBitErrorRate = vih->dwBitErrorRate;
  vih2->AvgTimePerFrame = vih->AvgTimePerFrame;

  // Calculate aspect ratio
  AVRational r = avstream->sample_aspect_ratio;
  AVRational rc = avstream->codecpar->sample_aspect_ratio;
  int num = vih->bmiHeader.biWidth, den = vih->bmiHeader.biHeight;
  if (r.den > 0 && r.num > 0) {
    av_reduce(&num, &den, (int64_t)r.num * num, (int64_t)r.den * den, INT_MAX);
  } else if (rc.den > 0 && rc.num > 0) {
    av_reduce(&num, &den, (int64_t)rc.num * num, (int64_t)rc.den * den, INT_MAX);
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

MPEG1VIDEOINFO *CLAVFVideoHelper::CreateMPEG1VI(const AVStream* avstream, ULONG *size, std::string container)
{
  int extra = 0;
  BYTE *extradata = nullptr;

  // Create a VIH that we'll convert
  VIDEOINFOHEADER *vih = CreateVIH(avstream, size, container);
  if (!vih) return nullptr;

  if(avstream->codecpar->extradata_size > 0) {
    extra = avstream->codecpar->extradata_size;
    extradata = avstream->codecpar->extradata;
  }

  MPEG1VIDEOINFO *mp1vi = (MPEG1VIDEOINFO *)CoTaskMemAlloc(sizeof(MPEG1VIDEOINFO) + extra);
  if (!mp1vi) return nullptr;
  memset(mp1vi, 0, sizeof(MPEG1VIDEOINFO));

  // The MPEG1VI is a thin wrapper around a VIH, so its easy!
  memcpy(&mp1vi->hdr, vih, sizeof(VIDEOINFOHEADER));
  mp1vi->hdr.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

  mp1vi->dwStartTimeCode = 0; // is this not 0 anywhere..?
  mp1vi->hdr.bmiHeader.biPlanes = 1;
  mp1vi->hdr.bmiHeader.biCompression = 0;

  // copy extradata over
  if(extra) {
    CExtradataParser parser = CExtradataParser(extradata, extra);
    mp1vi->cbSequenceHeader = (DWORD)parser.ParseMPEGSequenceHeader(mp1vi->bSequenceHeader);
  }

  // Free the VIH that we converted
  CoTaskMemFree(vih);

  *size = SIZE_MPEG1VIDEOINFO(mp1vi);
  return mp1vi;
}

MPEG2VIDEOINFO *CLAVFVideoHelper::CreateMPEG2VI(const AVStream *avstream, ULONG *size, std::string container, BOOL bConvertToAVC1)
{
  int extra = 0;
  BYTE *extradata = nullptr;

  // Create a VIH that we'll convert
  VIDEOINFOHEADER2 *vih2 = CreateVIH2(avstream, size, container);
  if (!vih2) return nullptr;

  if(avstream->codecpar->extradata_size > 0) {
    extra = avstream->codecpar->extradata_size;
    extradata = avstream->codecpar->extradata;
  }

  MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)CoTaskMemAlloc(sizeof(MPEG2VIDEOINFO) + max(extra - 4, 0));
  if (!mp2vi) return nullptr;
  memset(mp2vi, 0, sizeof(MPEG2VIDEOINFO));
  memcpy(&mp2vi->hdr, vih2, sizeof(VIDEOINFOHEADER2));
  mp2vi->hdr.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

  // Set profile/level if we know them
  mp2vi->dwProfile = (avstream->codecpar->profile != FF_PROFILE_UNKNOWN) ? avstream->codecpar->profile : 0;
  mp2vi->dwLevel = (avstream->codecpar->level != FF_LEVEL_UNKNOWN) ? avstream->codecpar->level : 0;
  //mp2vi->dwFlags = 4; // where do we get flags otherwise..?

  if(extra > 0)
  {
    BOOL bCopyUntouched = FALSE;
    if(avstream->codecpar->codec_id == AV_CODEC_ID_H264)
    {
      int ret = ProcessH264Extradata(extradata, extra, mp2vi, bConvertToAVC1);
      if (ret < 0)
        bCopyUntouched = TRUE;
    } else if (avstream->codecpar->codec_id == AV_CODEC_ID_HEVC) {
      int ret = ProcessHEVCExtradata(extradata, extra, mp2vi);
      if (ret < 0)
        bCopyUntouched = TRUE;
    } else if (avstream->codecpar->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
      CExtradataParser parser = CExtradataParser(extradata, extra);
      mp2vi->cbSequenceHeader = (DWORD)parser.ParseMPEGSequenceHeader((BYTE *)&mp2vi->dwSequenceHeader[0]);
      mp2vi->hdr.bmiHeader.biPlanes = 1;
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


HRESULT CLAVFVideoHelper::ProcessH264Extradata(BYTE *extradata, int extradata_size, MPEG2VIDEOINFO *mp2vi, BOOL bAnnexB)
{
  if (*(char *)extradata == 1) {
    if (extradata[1])
      mp2vi->dwProfile = extradata[1];
    if (extradata[3])
      mp2vi->dwLevel = extradata[3];
    mp2vi->dwFlags = (extradata[4] & 3) + 1;
    mp2vi->cbSequenceHeader = avc_quant(extradata, (BYTE *)(&mp2vi->dwSequenceHeader[0]), extradata_size);
  } else {
    // MPEG-TS gets converted for improved compat.. for now!
    if (bAnnexB) {
      mp2vi->dwFlags = 4;
      mp2vi->cbSequenceHeader = (DWORD)avc_parse_annexb(extradata, extradata_size, (BYTE *)(&mp2vi->dwSequenceHeader[0]));
    } else {
      return -1;
    }
  }
  return 0;
}

HRESULT CLAVFVideoHelper::ProcessH264MVCExtradata(BYTE *extradata, int extradata_size, MPEG2VIDEOINFO *mp2vi)
{
  if (*(char *)extradata == 1) {
    // Find "mvcC" atom
    uint32_t state = -1;
    int i = 0;
    for (; i < extradata_size; i++) {
      state = (state << 8) | extradata[i];
      if (state == MKBETAG('m', 'v', 'c', 'C'))
        break;
    }

    if (i == extradata_size || i < 8)
      return E_FAIL;

    // Update pointers to the start of the mvcC atom
    extradata = extradata + i - 7;
    extradata_size = extradata_size - i + 7;
    int sizeAtom = AV_RB32(extradata);

    // verify size atom and actual size
    if ((sizeAtom + 4) > extradata_size || extradata_size < 14)
      return E_FAIL;

    // Skip atom headers
    extradata += 8;
    extradata_size -= 8;

    // Process as a normal "avcC" record
    ProcessH264Extradata(extradata, extradata_size, mp2vi, FALSE);

    return S_OK;
  }
  return E_FAIL;
}

HRESULT CLAVFVideoHelper::ProcessHEVCExtradata(BYTE *extradata, int extradata_size, MPEG2VIDEOINFO *mp2vi)
{
  if (extradata[0] || extradata[1] || extradata[2] > 1 && extradata_size > 25) {
    mp2vi->dwFlags = (extradata[21] & 3) + 1;
  }
  return -1;
}
