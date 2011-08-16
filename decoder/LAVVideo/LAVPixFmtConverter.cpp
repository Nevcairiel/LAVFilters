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

#include <MMReg.h>
#include "moreuuids.h"

extern "C" {
#include "libavutil/intreadwrite.h"
};

#define ALIGN(x,a) (((x)+(a)-1UL)&~((a)-1UL))

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

static LAVPixFmtDesc lav_pixfmt_desc[] = {
  { MEDIASUBTYPE_YV12,  12, 3, { 1, 2, 2 }, { 1, 2, 2 } },        // YV12
  { MEDIASUBTYPE_NV12,  12, 2, { 1, 2 }, { 1, 1 } },              // NV12
  { MEDIASUBTYPE_YUY2,  16, 0 },                                  // YUY2 (packed)
  { MEDIASUBTYPE_UYVY,  16, 0 },                                  // UYVY (packed)
  { MEDIASUBTYPE_AYUV,  32, 0 },                                  // AYUV (packed)
  { MEDIASUBTYPE_P010,  15, 2, { 1, 2 }, { 1, 1 } },              // P010
  { MEDIASUBTYPE_P210,  20, 2, { 1, 1 }, { 1, 1 } },              // P210
  { FOURCCMap('014Y'),  32, 0 },                                  // Y410 (packed)
  { MEDIASUBTYPE_P016,  24, 2, { 1, 2 }, { 1, 1 } },              // P016
  { MEDIASUBTYPE_P216,  32, 2, { 1, 1 }, { 1, 1 } },              // P216
  { FOURCCMap('614Y'),  64, 0 },                                  // Y416 (packed)
  { MEDIASUBTYPE_RGB32, 32, 0 },                                  // RGB32
  { MEDIASUBTYPE_RGB24, 24, 0 },                                  // RGB24
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
{
}

CLAVPixFmtConverter::~CLAVPixFmtConverter()
{
  DestroySWScale();
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

inline SwsContext *CLAVPixFmtConverter::GetSWSContext(int width, int height, enum PixelFormat srcPix, enum PixelFormat dstPix, int flags)
{
  if (!m_pSwsContext || swsWidth != width || swsHeight != height) {
    // Map full-range formats to their limited-range variants
    // All target formats we have are limited range and we don't want compression
    if (dstPix != PIX_FMT_BGRA && dstPix != PIX_FMT_BGR24) {
      if (srcPix == PIX_FMT_YUVJ420P)
        srcPix = PIX_FMT_YUV420P;
      else if (srcPix == PIX_FMT_YUVJ422P)
        srcPix = PIX_FMT_YUV422P;
      else if (srcPix == PIX_FMT_YUVJ440P)
        srcPix = PIX_FMT_YUV440P;
      else if (srcPix == PIX_FMT_YUVJ444P)
        srcPix = PIX_FMT_YUV444P;
    }

    flags |= (SWS_FULL_CHR_H_INT|SWS_ACCURATE_RND);

    // Get context
    m_pSwsContext = sws_getCachedContext(m_pSwsContext,
                                 width, height, srcPix,
                                 width, height, dstPix,
                                 flags|SWS_PRINT_INFO, NULL, NULL, NULL);

    int *inv_tbl = NULL, *tbl = NULL;
    int srcRange, dstRange, brightness, contrast, saturation;
    int ret = sws_getColorspaceDetails(m_pSwsContext, &inv_tbl, &srcRange, &tbl, &dstRange, &brightness, &contrast, &saturation);
    if (ret >= 0) {
      const int *rgbTbl = NULL;
      if (swsColorSpace != AVCOL_SPC_UNSPECIFIED) {
        rgbTbl = sws_getCoefficients(swsColorSpace);
      } else {
        BOOL isHD = (height >= 720 || width >= 1280);
        rgbTbl = sws_getCoefficients(isHD ? SWS_CS_ITU709 : SWS_CS_ITU601);
      }
      if (swsColorRange != AVCOL_RANGE_UNSPECIFIED) {
        srcRange = dstRange = swsColorRange - 1;
      }
      sws_setColorspaceDetails(m_pSwsContext, rgbTbl, srcRange, tbl, dstRange, brightness, contrast, saturation);
    }
    swsWidth = width;
    swsHeight = height;
  }
  return m_pSwsContext;
}

HRESULT CLAVPixFmtConverter::swscale_scale(enum PixelFormat srcPix, enum PixelFormat dstPix, AVFrame *pFrame, BYTE *pOut, int width, int height, int stride, LAVPixFmtDesc pixFmtDesc, bool swapPlanes12)
{
  uint8_t *dst[4];
  int     dstStride[4];
  int     i, ret;

  SwsContext *ctx = GetSWSContext(width, height, srcPix, dstPix, SWS_BICUBIC);
  CheckPointer(m_pSwsContext, E_POINTER);

  memset(dst, 0, sizeof(dst));
  memset(dstStride, 0, sizeof(dstStride));

  dst[0] = pOut;
  dstStride[0] = stride;
  for (i = 1; i < pixFmtDesc.planes; ++i) {
    dst[i] = dst[i-1] + (stride / pixFmtDesc.planeWidth[i-1]) * (height / pixFmtDesc.planeHeight[i-1]);
    dstStride[i] = stride / pixFmtDesc.planeWidth[i];
  }
  
  if (swapPlanes12) {
    BYTE *tmp = dst[1];
    dst[1] = dst[2];
    dst[2] = tmp;
  }
  ret = sws_scale(ctx, pFrame->data, pFrame->linesize, 0, height, dst, dstStride);

  return S_OK;
}

HRESULT CLAVPixFmtConverter::Convert(AVFrame *pFrame, BYTE *pOut, int width, int height, int dstStride)
{
  HRESULT hr = S_OK;
  switch (m_OutputPixFmt) {
  case LAVPixFmt_YV12:
    hr = swscale_scale(m_InputPixFmt, PIX_FMT_YUV420P, pFrame, pOut, width, height, dstStride, lav_pixfmt_desc[m_OutputPixFmt], true);
    break;
  case LAVPixFmt_NV12:
    hr = swscale_scale(m_InputPixFmt, PIX_FMT_NV12, pFrame, pOut, width, height, dstStride, lav_pixfmt_desc[m_OutputPixFmt]);
    break;
  case LAVPixFmt_YUY2:
    hr = swscale_scale(m_InputPixFmt, PIX_FMT_YUYV422, pFrame, pOut, width, height, dstStride * 2, lav_pixfmt_desc[m_OutputPixFmt]);
    break;
  case LAVPixFmt_UYVY:
    hr = swscale_scale(m_InputPixFmt, PIX_FMT_UYVY422, pFrame, pOut, width, height, dstStride * 2, lav_pixfmt_desc[m_OutputPixFmt]);
    break;
  case LAVPixFmt_AYUV:
    hr = ConvertToAYUV(pFrame, pOut, width, height, dstStride);
    break;
  case LAVPixFmt_P010:
    hr = ConvertToPX1X(pFrame, pOut, width, height, dstStride, 2);
    break;
  case LAVPixFmt_P016:
    hr = ConvertToPX1X(pFrame, pOut, width, height, dstStride, 2);
    break;
  case LAVPixFmt_P210:
    hr = ConvertToPX1X(pFrame, pOut, width, height, dstStride, 1);
    break;
  case LAVPixFmt_P216:
    hr = ConvertToPX1X(pFrame, pOut, width, height, dstStride, 1);
    break;
  case LAVPixFmt_Y410:
    hr = ConvertToY410(pFrame, pOut, width, height, dstStride);
    break;
  case LAVPixFmt_Y416:
    hr = ConvertToY416(pFrame, pOut, width, height, dstStride);
    break;
  case LAVPixFmt_RGB32:
    hr = swscale_scale(m_InputPixFmt, PIX_FMT_BGRA, pFrame, pOut, width, height, dstStride * 4, lav_pixfmt_desc[m_OutputPixFmt]);
    break;
  case LAVPixFmt_RGB24:
    hr = swscale_scale(m_InputPixFmt, PIX_FMT_BGR24, pFrame, pOut, width, height, dstStride * 3, lav_pixfmt_desc[m_OutputPixFmt]);
    break;
  default:
    ASSERT(0);
    hr = E_FAIL;
    break;
  }
  return hr;
}

HRESULT CLAVPixFmtConverter::ConvertToAYUV(AVFrame *pFrame, BYTE *pOut, int width, int height, int stride)
{
  const BYTE *y = NULL;
  const BYTE *u = NULL;
  const BYTE *v = NULL;
  int line, i = 0;
  int srcStride = 0;
  BYTE *pTmpBuffer = NULL;

  if (m_InputPixFmt != PIX_FMT_YUV444P && m_InputPixFmt != PIX_FMT_YUVJ444P) {
    uint8_t *dst[4] = {NULL};
    int     dstStride[4] = {0};

    pTmpBuffer = (BYTE *)av_malloc(height * stride * 3);

    dst[0] = pTmpBuffer;
    dst[1] = dst[0] + (height * stride);
    dst[2] = dst[1] + (height * stride);
    dst[3] = NULL;
    dstStride[0] = stride;
    dstStride[1] = stride;
    dstStride[2] = stride;
    dstStride[3] = 0;

    SwsContext *ctx = GetSWSContext(width, height, m_InputPixFmt, PIX_FMT_YUV444P, SWS_POINT);
    sws_scale(ctx, pFrame->data, pFrame->linesize, 0, height, dst, dstStride);

    y = dst[0];
    u = dst[1];
    v = dst[2];
    srcStride = stride;
  } else {
    y = pFrame->data[0];
    u = pFrame->data[1];
    v = pFrame->data[2];
    srcStride = pFrame->linesize[0];
  }

  int alignedWidth     = ALIGN(width, 8);
  int unalignedBytes   = 0;
  if(alignedWidth > srcStride || alignedWidth > stride) {
    alignedWidth = width & ~7;
    unalignedBytes = width - alignedWidth;
  }

  BYTE *out = pOut;
  for (line = 0; line < height; ++line) {
    int32_t *idst = (int32_t *)out;
    for (i = 0; i < alignedWidth; i+=8) {
      *idst++ = v[i+0] | (u[i+0] << 8) | (y[i+0] << 16) | (0xff << 24);
      *idst++ = v[i+1] | (u[i+1] << 8) | (y[i+1] << 16) | (0xff << 24);
      *idst++ = v[i+2] | (u[i+2] << 8) | (y[i+2] << 16) | (0xff << 24);
      *idst++ = v[i+3] | (u[i+3] << 8) | (y[i+3] << 16) | (0xff << 24);
      *idst++ = v[i+4] | (u[i+4] << 8) | (y[i+4] << 16) | (0xff << 24);
      *idst++ = v[i+5] | (u[i+5] << 8) | (y[i+5] << 16) | (0xff << 24);
      *idst++ = v[i+6] | (u[i+6] << 8) | (y[i+6] << 16) | (0xff << 24);
      *idst++ = v[i+7] | (u[i+7] << 8) | (y[i+7] << 16) | (0xff << 24);
    }
    if (unalignedBytes) {
      for (i = alignedWidth; i < width; ++i) {
        *idst++ = v[i] | (u[i] << 8) | (y[i] << 16) | (0xff << 24);
      }
    }
    y += srcStride;
    u += srcStride;
    v += srcStride;
    out += stride << 2;
  }

  av_freep(&pTmpBuffer);

  return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertToPX1X(AVFrame *pFrame, BYTE *pOut, int width, int height, int stride, int chromaVertical)
{
  const BYTE *y = NULL;
  const BYTE *u = NULL;
  const BYTE *v = NULL;
  int line, i = 0;
  int srcStride = 0;

  int shift = 0;
  BOOL bBigEndian = FALSE;

  // Stride needs to be doubled for 16-bit per pixel
  stride *= 2;

  BYTE *pTmpBuffer = NULL;

  if ((chromaVertical == 1 && m_InputPixFmt != PIX_FMT_YUV422P16LE && m_InputPixFmt != PIX_FMT_YUV422P16BE && m_InputPixFmt != PIX_FMT_YUV422P10LE && m_InputPixFmt != PIX_FMT_YUV422P10BE)
    || (chromaVertical == 2 && m_InputPixFmt != PIX_FMT_YUV420P16LE && m_InputPixFmt != PIX_FMT_YUV420P16BE && m_InputPixFmt != PIX_FMT_YUV420P10LE && m_InputPixFmt != PIX_FMT_YUV420P10BE
        && m_InputPixFmt != PIX_FMT_YUV420P9LE && m_InputPixFmt != PIX_FMT_YUV420P9BE)) {
    uint8_t *dst[4] = {NULL};
    int     dstStride[4] = {0};

    pTmpBuffer = (BYTE *)av_malloc(height * stride * 2);

    dst[0] = pTmpBuffer;
    dst[1] = dst[0] + (height * stride);
    dst[2] = dst[1] + ((height / chromaVertical) * (stride / 2));
    dst[3] = NULL;
    dstStride[0] = stride;
    dstStride[1] = stride / 2;
    dstStride[2] = stride / 2;
    dstStride[3] = 0;

    SwsContext *ctx = GetSWSContext(width, height, m_InputPixFmt, chromaVertical == 1 ? PIX_FMT_YUV422P16LE : PIX_FMT_YUV420P16LE, SWS_POINT);
    sws_scale(ctx, pFrame->data, pFrame->linesize, 0, height, dst, dstStride);

    y = dst[0];
    u = dst[1];
    v = dst[2];
    srcStride = stride;
  } else {
    y = pFrame->data[0];
    u = pFrame->data[1];
    v = pFrame->data[2];
    srcStride = pFrame->linesize[0];

    if (m_InputPixFmt == PIX_FMT_YUV422P10LE || m_InputPixFmt == PIX_FMT_YUV422P10BE || m_InputPixFmt == PIX_FMT_YUV420P10LE || m_InputPixFmt == PIX_FMT_YUV420P10BE)
      shift = 6;
    else if (m_InputPixFmt == PIX_FMT_YUV420P9LE || m_InputPixFmt == PIX_FMT_YUV420P9BE)
      shift = 7;

    bBigEndian = (m_InputPixFmt == PIX_FMT_YUV422P16BE || m_InputPixFmt == PIX_FMT_YUV422P10BE || m_InputPixFmt == PIX_FMT_YUV420P16BE || m_InputPixFmt == PIX_FMT_YUV420P10BE || m_InputPixFmt == PIX_FMT_YUV420P9BE);
  }

  // copy Y
  BYTE *pLineOut = pOut;
  const BYTE *pLineIn = y;
  for (line = 0; line < height; ++line) {
    if (shift == 0 && !bBigEndian) {
      memcpy(pLineOut, pLineIn, width * 2);
    } else {
      const int16_t *yc = (int16_t *)pLineIn;
      int16_t *idst = (int16_t *)pLineOut;
      for (i = 0; i < width; ++i) {
        int32_t yv;
        if (bBigEndian) yv = AV_RB16(yc+i); else yv = AV_RL16(yc+i);
        if (shift) yv <<= shift;
        *idst++ = yv;
      }
    }
    pLineOut += stride;
    pLineIn += srcStride;
  }

  srcStride >>= 2;

  // Merge U/V
  BYTE *out = pLineOut;
  const int16_t *uc = (int16_t *)u;
  const int16_t *vc = (int16_t *)v;
  for (line = 0; line < height/chromaVertical; ++line) {
    int32_t *idst = (int32_t *)out;
    for (i = 0; i < width/2; ++i) {
      int32_t uv, vv;
      if (bBigEndian) {
        uv = AV_RB16(uc+i);
        vv = AV_RB16(vc+i);
      } else {
        uv = AV_RL16(uc+i);
        vv = AV_RL16(vc+i);
      }
      if (shift) {
        uv <<= shift;
        vv <<= shift;
      }
      *idst++ = uv | (vv << 16);
    }
    uc += srcStride;
    vc += srcStride;
    out += stride;
  }

  av_freep(&pTmpBuffer);

  return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertToY410(AVFrame *pFrame, BYTE *pOut, int width, int height, int stride)
{
  const int16_t *y = NULL;
  const int16_t *u = NULL;
  const int16_t *v = NULL;
  int line, i = 0;
  int srcStride = 0;
  bool bBigEndian = false, b9Bit = false;

  BYTE *pTmpBuffer = NULL;

  if (m_InputPixFmt != PIX_FMT_YUV444P10BE && m_InputPixFmt != PIX_FMT_YUV444P10LE && m_InputPixFmt != PIX_FMT_YUV444P9BE && m_InputPixFmt != PIX_FMT_YUV444P9LE) {
    uint8_t *dst[4] = {NULL};
    int     dstStride[4] = {0};

    pTmpBuffer = (BYTE *)av_malloc(height * stride * 6);

    dst[0] = pTmpBuffer;
    dst[1] = dst[0] + (height * stride * 2);
    dst[2] = dst[1] + (height * stride * 2);
    dst[3] = NULL;
    dstStride[0] = stride * 2;
    dstStride[1] = stride * 2;
    dstStride[2] = stride * 2;
    dstStride[3] = 0;

    SwsContext *ctx = GetSWSContext(width, height, m_InputPixFmt, PIX_FMT_YUV444P10LE, SWS_POINT);
    sws_scale(ctx, pFrame->data, pFrame->linesize, 0, height, dst, dstStride);

    y = (int16_t *)dst[0];
    u = (int16_t *)dst[1];
    v = (int16_t *)dst[2];
    srcStride = stride;
  } else {
    y = (int16_t *)pFrame->data[0];
    u = (int16_t *)pFrame->data[1];
    v = (int16_t *)pFrame->data[2];
    srcStride = pFrame->linesize[0] / 2;

    bBigEndian = (m_InputPixFmt == PIX_FMT_YUV444P10BE || m_InputPixFmt == PIX_FMT_YUV444P9BE);
    b9Bit = (m_InputPixFmt == PIX_FMT_YUV444P9BE || m_InputPixFmt == PIX_FMT_YUV444P9LE);
  }

  // 32-bit per pixel
  stride *= 4;

  BYTE *out = pOut;
  for (line = 0; line < height; ++line) {
    int32_t *idst = (int32_t *)out;
    for (i = 0; i < width; ++i) {
      int32_t yv, uv, vv;
      if (bBigEndian) {
        yv = AV_RB16(y+i);
        uv = AV_RB16(u+i);
        vv = AV_RB16(v+i);
      } else {
        yv = AV_RL16(y+i);
        uv = AV_RL16(u+i);
        vv = AV_RL16(v+i);
      }
      if (b9Bit) {
        yv <<= 1;
        uv <<= 1;
        vv <<= 1;
      }
      *idst++ = (uv & 0x3FF) | ((yv & 0x3FF) << 10) | ((vv & 0x3FF) << 20) | (3 << 30);
    }
    y += srcStride;
    u += srcStride;
    v += srcStride;
    out += stride;
  }

  av_freep(&pTmpBuffer);

  return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertToY416(AVFrame *pFrame, BYTE *pOut, int width, int height, int stride)
{
  const int16_t *y = NULL;
  const int16_t *u = NULL;
  const int16_t *v = NULL;
  int line, i = 0;
  int srcStride = 0;
  bool bBigEndian = false;

  BYTE *pTmpBuffer = NULL;

  if (m_InputPixFmt != PIX_FMT_YUV444P16BE && m_InputPixFmt != PIX_FMT_YUV444P16LE) {
    uint8_t *dst[4] = {NULL};
    int     dstStride[4] = {0};

    pTmpBuffer = (BYTE *)av_malloc(height * stride * 6);

    dst[0] = pTmpBuffer;
    dst[1] = dst[0] + (height * stride * 2);
    dst[2] = dst[1] + (height * stride * 2);
    dst[3] = NULL;
    dstStride[0] = stride * 2;
    dstStride[1] = stride * 2;
    dstStride[2] = stride * 2;
    dstStride[3] = 0;

    SwsContext *ctx = GetSWSContext(width, height, m_InputPixFmt, PIX_FMT_YUV444P16LE, SWS_POINT);
    sws_scale(ctx, pFrame->data, pFrame->linesize, 0, height, dst, dstStride);

    y = (int16_t *)dst[0];
    u = (int16_t *)dst[1];
    v = (int16_t *)dst[2];
    srcStride = stride;
  } else {
    y = (int16_t *)pFrame->data[0];
    u = (int16_t *)pFrame->data[1];
    v = (int16_t *)pFrame->data[2];
    srcStride = pFrame->linesize[0] / 2;

    bBigEndian = (m_InputPixFmt == PIX_FMT_YUV444P16BE);
  }

  // 64-bit per pixel
  stride *= 8;

  BYTE *out = pOut;
  for (line = 0; line < height; ++line) {
    int32_t *idst = (int32_t *)out;
    for (i = 0; i < width; ++i) {
      int32_t yv, uv, vv;
      if (bBigEndian) {
        yv = AV_RB16(y+i);
        uv = AV_RB16(u+i);
        vv = AV_RB16(v+i);
      } else {
        yv = AV_RL16(y+i);
        uv = AV_RL16(u+i);
        vv = AV_RL16(v+i);
      }
      *idst++ = 0xFFFF | (vv << 16);
      *idst++ = yv | (uv << 16);
    }
    y += srcStride;
    u += srcStride;
    v += srcStride;
    out += stride;
  }

  av_freep(&pTmpBuffer);

  return S_OK;
}
