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
#include "LAVPixFmtConverter.h"
#include "Media.h"

#include <MMReg.h>
#include "moreuuids.h"

typedef struct {
  enum PixelFormat ff_pix_fmt; // ffmpeg pixel format
  int num_pix_fmt;
  enum LAVVideoPixFmts lav_pix_fmts[LAVPixFmt_NB];
} FF_LAV_PIXFMT_MAP;

static FF_LAV_PIXFMT_MAP lav_pixfmt_map[] = {
  // Default
  { PIX_FMT_NONE, 5, { LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_YUY2, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },

  // 4:2:0
  { PIX_FMT_YUV420P,  6, { LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },
  { PIX_FMT_YUVJ420P, 6, { LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },
  { PIX_FMT_NV12,     6, { LAVPixFmt_NV12, LAVPixFmt_YV12, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },
  { PIX_FMT_NV21,     6, { LAVPixFmt_NV12, LAVPixFmt_YV12, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },

  { PIX_FMT_YUV420P9BE,  7, { LAVPixFmt_P010, LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },
  { PIX_FMT_YUV420P9LE,  7, { LAVPixFmt_P010, LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },
  { PIX_FMT_YUV420P10BE, 7, { LAVPixFmt_P010, LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },
  { PIX_FMT_YUV420P10LE, 7, { LAVPixFmt_P010, LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },
  { PIX_FMT_YUV420P16BE, 8, { LAVPixFmt_P016, LAVPixFmt_P010, LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },
  { PIX_FMT_YUV420P16LE, 8, { LAVPixFmt_P016, LAVPixFmt_P010, LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },

  // 4:2:2
  { PIX_FMT_YUV422P,  6, { LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },
  { PIX_FMT_YUVJ422P, 6, { LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },
  { PIX_FMT_YUYV422,  6, { LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },
  { PIX_FMT_UYVY422,  6, { LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },

  { PIX_FMT_YUV422P10BE, 7, { LAVPixFmt_P210, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },
  { PIX_FMT_YUV422P10LE, 7, { LAVPixFmt_P210, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },
  { PIX_FMT_YUV422P16BE, 8, { LAVPixFmt_P216, LAVPixFmt_P210, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },
  { PIX_FMT_YUV422P16LE, 8, { LAVPixFmt_P216, LAVPixFmt_P210, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12, LAVPixFmt_RGB32, LAVPixFmt_RGB24 } },
  
  // 4:4:4
  { PIX_FMT_YUV444P,  7, { LAVPixFmt_AYUV, LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUVJ444P, 7, { LAVPixFmt_AYUV, LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },

  { PIX_FMT_YUV444P9BE,  8, { LAVPixFmt_Y410, LAVPixFmt_AYUV, LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV444P9LE,  8, { LAVPixFmt_Y410, LAVPixFmt_AYUV, LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV444P10BE, 8, { LAVPixFmt_Y410, LAVPixFmt_AYUV, LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV444P10LE, 8, { LAVPixFmt_Y410, LAVPixFmt_AYUV, LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV444P16BE, 9, { LAVPixFmt_Y416, LAVPixFmt_Y410, LAVPixFmt_AYUV, LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_YUV444P16LE, 9, { LAVPixFmt_Y416, LAVPixFmt_Y410, LAVPixFmt_AYUV, LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },

  // RGB
  { PIX_FMT_RGB24,    6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_BGR24,    6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_RGBA,     6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_ARGB,     6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_BGRA,     6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_ABGR,     6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_RGB565BE, 6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_RGB565LE, 6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_RGB555BE, 6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_RGB555LE, 6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_BGR565BE, 6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_BGR565LE, 6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_BGR555BE, 6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_BGR555LE, 6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_RGB444LE, 6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_RGB444BE, 6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_BGR444LE, 6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
  { PIX_FMT_BGR444BE, 6, { LAVPixFmt_RGB32, LAVPixFmt_RGB24, LAVPixFmt_YUY2, LAVPixFmt_UYVY, LAVPixFmt_YV12, LAVPixFmt_NV12 } },
};

LAVPixFmtDesc lav_pixfmt_desc[] = {
  { MEDIASUBTYPE_YV12,  12, 1, 3, { 1, 2, 2 }, { 1, 2, 2 } },        // YV12
  { MEDIASUBTYPE_NV12,  12, 1, 2, { 1, 2 }, { 1, 1 } },              // NV12
  { MEDIASUBTYPE_YUY2,  16, 2, 0 },                                  // YUY2 (packed)
  { MEDIASUBTYPE_UYVY,  16, 2, 0 },                                  // UYVY (packed)
  { MEDIASUBTYPE_AYUV,  32, 4, 0 },                                  // AYUV (packed)
  { MEDIASUBTYPE_P010,  15, 2, 2, { 1, 2 }, { 1, 1 } },              // P010
  { MEDIASUBTYPE_P210,  20, 2, 2, { 1, 1 }, { 1, 1 } },              // P210
  { FOURCCMap('014Y'),  32, 4, 0 },                                  // Y410 (packed)
  { MEDIASUBTYPE_P016,  24, 2, 2, { 1, 2 }, { 1, 1 } },              // P016
  { MEDIASUBTYPE_P216,  32, 2, 2, { 1, 1 }, { 1, 1 } },              // P216
  { FOURCCMap('614Y'),  64, 8, 0 },                                  // Y416 (packed)
  { MEDIASUBTYPE_RGB32, 32, 4, 0 },                                  // RGB32
  { MEDIASUBTYPE_RGB24, 24, 3, 0 },                                  // RGB24
};

static FF_LAV_PIXFMT_MAP *lookupFormatMap(PixelFormat ffFormat, BOOL bFallbackToDefault = TRUE)
{
  for (int i = 0; i < countof(lav_pixfmt_map); ++i) {
    if (lav_pixfmt_map[i].ff_pix_fmt == ffFormat) {
      return &lav_pixfmt_map[i];
    }
  }
  if (bFallbackToDefault)
    return &lav_pixfmt_map[0];
  return NULL;
}

CLAVPixFmtConverter::CLAVPixFmtConverter()
  : m_pSettings(NULL)
  , m_InputPixFmt(PIX_FMT_NONE)
  , m_OutputPixFmt(LAVPixFmt_YV12)
  , m_pSwsContext(NULL)
  , swsWidth(0), swsHeight(0)
  , swsColorSpace(AVCOL_SPC_UNSPECIFIED)
  , swsColorRange(AVCOL_RANGE_UNSPECIFIED)
  , m_RequiredAlignment(0)
  , m_nAlignedBufferSize(0)
  , m_pAlignedBuffer(NULL)
{
  convert = &CLAVPixFmtConverter::convert_generic;
}

CLAVPixFmtConverter::~CLAVPixFmtConverter()
{
  DestroySWScale();
  av_freep(&m_pAlignedBuffer);
}

LAVVideoPixFmts CLAVPixFmtConverter::GetOutputBySubtype(const GUID *guid)
{
  for (int i = 0; i < countof(lav_pixfmt_desc); ++i) {
    if (lav_pixfmt_desc[i].subtype == *guid) {
      return (LAVVideoPixFmts)i;
    }
  }
  return LAVPixFmt_None;
}

int CLAVPixFmtConverter::GetFilteredFormatCount()
{
  FF_LAV_PIXFMT_MAP *pixFmtMap = lookupFormatMap(m_InputPixFmt);
  int count = 0;
  for (int i = 0; i < pixFmtMap->num_pix_fmt; ++i) {
    if (m_pSettings->GetPixelFormat(pixFmtMap->lav_pix_fmts[i]))
      count++;
  }

  if (count == 0)
    count = lav_pixfmt_map[0].num_pix_fmt;

  return count;
}

LAVVideoPixFmts CLAVPixFmtConverter::GetFilteredFormat(int index)
{
  FF_LAV_PIXFMT_MAP *pixFmtMap = lookupFormatMap(m_InputPixFmt);
  int actualIndex = -1;
  for (int i = 0; i < pixFmtMap->num_pix_fmt; ++i) {
    if (m_pSettings->GetPixelFormat(pixFmtMap->lav_pix_fmts[i]))
      actualIndex++;
    if (index == actualIndex)
      return pixFmtMap->lav_pix_fmts[i];
  }

  // If no format is enabled, we use the fallback formats to avoid catastrophic failure
  if (index >= lav_pixfmt_map[0].num_pix_fmt)
    index = 0;
  return lav_pixfmt_map[0].lav_pix_fmts[index];
}

LAVVideoPixFmts CLAVPixFmtConverter::GetPreferredOutput()
{
  return GetFilteredFormat(0);
}

int CLAVPixFmtConverter::GetNumMediaTypes()
{
  return GetFilteredFormatCount();
}

CMediaType CLAVPixFmtConverter::GetMediaType(int index, LONG biWidth, LONG biHeight, DWORD dwAspectX, DWORD dwAspectY, REFERENCE_TIME rtAvgTime, BOOL bVIH1)
{
  if (index < 0 || index >= GetFilteredFormatCount())
    index = 0;

  LAVVideoPixFmts pixFmt = GetFilteredFormat(index);

  CMediaType mt;
  GUID guid = lav_pixfmt_desc[pixFmt].subtype;

  mt.SetType(&MEDIATYPE_Video);
  mt.SetSubtype(&guid);

  BITMAPINFOHEADER *pBIH = NULL;
  if (bVIH1) {
    mt.SetFormatType(&FORMAT_VideoInfo);

    VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)mt.AllocFormatBuffer(sizeof(VIDEOINFOHEADER));
    memset(vih, 0, sizeof(VIDEOINFOHEADER));

    vih->rcSource.right = vih->rcTarget.right = biWidth;
    vih->rcSource.bottom = vih->rcTarget.bottom = biHeight;
    vih->AvgTimePerFrame = rtAvgTime;

    pBIH = &vih->bmiHeader;
  } else {
    mt.SetFormatType(&FORMAT_VideoInfo2);

    VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)mt.AllocFormatBuffer(sizeof(VIDEOINFOHEADER2));
    memset(vih2, 0, sizeof(VIDEOINFOHEADER2));

    // Validate the Aspect Ratio - an AR of 0 crashes VMR-9
    if (dwAspectX == 0 || dwAspectY == 0) {
      int dwX = 0;
      int dwY = 0;
      av_reduce(&dwX, &dwY, biWidth, biHeight, max(biWidth, biHeight));

      dwAspectX = dwX;
      dwAspectY = dwY;
    }

    vih2->rcSource.right = vih2->rcTarget.right = biWidth;
    vih2->rcSource.bottom = vih2->rcTarget.bottom = biHeight;
    vih2->AvgTimePerFrame = rtAvgTime;
    vih2->dwPictAspectRatioX = dwAspectX;
    vih2->dwPictAspectRatioY = dwAspectY;

    // Always set interlace flags, the samples will be flagged appropriately then.
    if (m_pSettings->GetReportInterlacedFlags())
      vih2->dwInterlaceFlags = AMINTERLACE_IsInterlaced | AMINTERLACE_DisplayModeBobOrWeave;

    pBIH = &vih2->bmiHeader;
  }

  pBIH->biSize = sizeof(BITMAPINFOHEADER);
  pBIH->biWidth = biWidth;
  pBIH->biHeight = biHeight;
  pBIH->biBitCount = lav_pixfmt_desc[pixFmt].bpp;
  pBIH->biPlanes = lav_pixfmt_desc[pixFmt].planes;
  pBIH->biSizeImage = (biWidth * biHeight * pBIH->biBitCount) >> 3;
  pBIH->biCompression = guid.Data1;

  if (!pBIH->biPlanes) {
    pBIH->biPlanes = 1;
  }

  if (guid == MEDIASUBTYPE_RGB32 || guid == MEDIASUBTYPE_RGB24) {
    pBIH->biCompression = BI_RGB;
    pBIH->biHeight = -pBIH->biHeight;
  }

  mt.SetSampleSize(pBIH->biSizeImage);
  mt.SetTemporalCompression(0);

  return mt;
}


BOOL CLAVPixFmtConverter::IsAllowedSubtype(const GUID *guid)
{
  for (int i = 0; i < GetFilteredFormatCount(); ++i) {
    if (lav_pixfmt_desc[GetFilteredFormat(i)].subtype == *guid)
      return TRUE;
  }

  return FALSE;
}

void CLAVPixFmtConverter::SelectConvertFunction()
{
  m_RequiredAlignment = 16;
  if (m_OutputPixFmt == LAVPixFmt_AYUV && (m_InputPixFmt == PIX_FMT_YUV444P10LE || m_InputPixFmt == PIX_FMT_YUV444P9LE || m_InputPixFmt == PIX_FMT_YUV444P16LE)) {
    convert = &CLAVPixFmtConverter::convert_yuv444_ayuv_dither_le;
  } else if (m_OutputPixFmt == LAVPixFmt_AYUV && (m_InputPixFmt == PIX_FMT_YUV444P || m_InputPixFmt == PIX_FMT_YUVJ444P)) {
    convert = &CLAVPixFmtConverter::convert_yuv444_ayuv;
  } else if (m_OutputPixFmt == LAVPixFmt_Y410 && (m_InputPixFmt == PIX_FMT_YUV444P10LE || m_InputPixFmt == PIX_FMT_YUV444P9LE)) {
    convert = &CLAVPixFmtConverter::convert_yuv444_y410;
  } else if ((m_OutputPixFmt == LAVPixFmt_YV12 || m_OutputPixFmt == LAVPixFmt_NV12) && (m_InputPixFmt == PIX_FMT_YUV420P10LE || m_InputPixFmt == PIX_FMT_YUV420P9LE || m_InputPixFmt == PIX_FMT_YUV420P16LE)) {
    if (m_OutputPixFmt == LAVPixFmt_NV12) {
      convert = &CLAVPixFmtConverter::convert_yuv420_yv12_nv12_dither_le<TRUE>;
    } else {
      convert = &CLAVPixFmtConverter::convert_yuv420_yv12_nv12_dither_le<FALSE>;
      m_RequiredAlignment = 32; // the U/V planes need to be 16 aligned..
    }
  } else if (((m_OutputPixFmt == LAVPixFmt_P010 || m_OutputPixFmt == LAVPixFmt_P016) && (m_InputPixFmt == PIX_FMT_YUV420P10LE || m_InputPixFmt == PIX_FMT_YUV420P9LE || PIX_FMT_YUV420P16LE))
          || ((m_OutputPixFmt == LAVPixFmt_P210 || m_OutputPixFmt == LAVPixFmt_P216) && (m_InputPixFmt == PIX_FMT_YUV422P10LE || m_InputPixFmt == PIX_FMT_YUV422P16LE))) {
    convert = &CLAVPixFmtConverter::convert_yuv420_px1x_le;
  } else if (m_OutputPixFmt == LAVPixFmt_YV12 && (m_InputPixFmt == PIX_FMT_YUV420P || m_InputPixFmt == PIX_FMT_YUVJ420P)) {
    convert = &CLAVPixFmtConverter::convert_yuv420_yv12;
    m_RequiredAlignment = 0;
  } else if (m_OutputPixFmt == LAVPixFmt_NV12 && (m_InputPixFmt == PIX_FMT_YUV420P || m_InputPixFmt == PIX_FMT_YUVJ420P)) {
    convert = &CLAVPixFmtConverter::convert_yuv420_nv12;
    m_RequiredAlignment = 32;
  } else if (m_OutputPixFmt == LAVPixFmt_YUY2 && (m_InputPixFmt == PIX_FMT_YUV422P || m_InputPixFmt == PIX_FMT_YUVJ422P)) {
    convert = &CLAVPixFmtConverter::convert_yuv422_yuy2_uyvy<0>;
    m_RequiredAlignment = 32;
  } else if (m_OutputPixFmt == LAVPixFmt_UYVY && (m_InputPixFmt == PIX_FMT_YUV422P || m_InputPixFmt == PIX_FMT_YUVJ422P)) {
    convert = &CLAVPixFmtConverter::convert_yuv422_yuy2_uyvy<1>;
    m_RequiredAlignment = 32;
  } else {
    convert = &CLAVPixFmtConverter::convert_generic;
  }
}

void CLAVPixFmtConverter::ChangeStride(const uint8_t* src, int srcStride, uint8_t *dst, int dstStride, int width, int height, LAVVideoPixFmts format)
{
  LAVPixFmtDesc desc = lav_pixfmt_desc[format];

  int line = 0;

  // Copy first plane
  const int widthBytes = width * desc.codedbytes;
  const int srcStrideBytes = srcStride * desc.codedbytes;
  const int dstStrideBytes = dstStride * desc.codedbytes;
  for (line = 0; line < height; ++line) {
    memcpy(dst, src, widthBytes);
    src += srcStrideBytes;
    dst += dstStrideBytes;
  }

  for (int plane = 1; plane < desc.planes; ++plane) {
    const int planeWidth     = widthBytes     / desc.planeWidth[plane];
    const int planeHeight    = height         / desc.planeHeight[plane];
    const int srcPlaneStride = srcStrideBytes / desc.planeWidth[plane];
    const int dstPlaneStride = dstStrideBytes / desc.planeWidth[plane];
    for (line = 0; line < planeHeight; ++line) {
      memcpy(dst, src, planeWidth);
      src += srcPlaneStride;
      dst += dstPlaneStride;
    }
  }
}
