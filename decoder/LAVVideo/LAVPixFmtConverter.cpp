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
 */

#include "stdafx.h"
#include "LAVPixFmtConverter.h"
#include "Media.h"

#include "decoders/ILAVDecoder.h"

#include <MMReg.h>
#include "moreuuids.h"

#include <time.h>
#include "rand_sse.h"

/*
 * Availability of custom high-quality converters
 * x = formatter available, - = fallback using swscale
 * 1 = up to 14-bit only
 * in/out       YV12    NV12    YV16     YUY2    UYVY    YV24   AYUV    P010    P210    v210    Y410    v410    P016    P216    Y416   RGB24   RGB32
 * YUV420         x       x       -       x       x       -       -       -       -       -       -       -       -       -       -      x       x
 * YUV420bX       x       x       -       x1      x1      -       -       x       -       -       -       -       x       -       -      x       x
 * YUV422         -       -       x       x       x       -       -       -       -       -       -       -       -       -       -      x       x
 * YUV422bX       -       -       x       x       x       -       -       -       x       -       -       -       -       x       -      x       x
 * YUV444         -       -       -       -       -       x       x       -       -       -       -       -       -       -       -      x       x
 * YUV444bX       -       -       -       -       -       x       x       -       -       -       x       -       -       -       x      x       x
 * NV12           x       x       -       x       x       -       -       -       -       -       -       -       -       -       -      x       x
 * P010           -       x       -       -       -       -       -       x       -       -       -       -       x       -       -      x       x
 * YUY2           -       -       -       -       -       -       -       -       -       -       -       -       -       -       -      -       -
 * RGB24          -       -       -       -       -       -       -       -       -       -       -       -       -       -       -      x       -
 * RGB32          -       -       -       -       -       -       -       -       -       -       -       -       -       -       -      -       x
 * ARGB32         -       -       -       -       -       -       -       -       -       -       -       -       -       -       -      -       x
 * RGB48          -       -       -       -       -       -       -       -       -       -       -       -       -       -       -      x       x
 *
 * Every processing path has a swscale fallback (even those with a "-" above), every combination of input/output is possible, just not optimized (ugly and/or slow)
 */


typedef struct {
  LAVPixelFormat in_pix_fmt;
  int maxbpp;
  LAVOutPixFmts lav_pix_fmts[LAVOutPixFmt_NB + 1];
} LAV_INOUT_PIXFMT_MAP;

#define PIXOUT_420_8    LAVOutPixFmt_NV12, LAVOutPixFmt_YV12
#define PIXOUT_422_8    LAVOutPixFmt_YUY2, LAVOutPixFmt_UYVY, LAVOutPixFmt_YV16
#define PIXOUT_444_8    LAVOutPixFmt_YV24, LAVOutPixFmt_AYUV
#define PIXOUT_RGB_8    LAVOutPixFmt_RGB32, LAVOutPixFmt_RGB24

#define PIXOUT_420_10   LAVOutPixFmt_P010
#define PIXOUT_422_10   LAVOutPixFmt_P210, LAVOutPixFmt_v210
#define PIXOUT_444_10   LAVOutPixFmt_Y410, LAVOutPixFmt_v410

#define PIXOUT_420_16   LAVOutPixFmt_P016
#define PIXOUT_422_16   LAVOutPixFmt_P216
#define PIXOUT_444_16   LAVOutPixFmt_Y416
#define PIXOUT_RGB_16   LAVOutPixFmt_RGB48

static LAV_INOUT_PIXFMT_MAP lav_pixfmt_map[] = {
  // Default
  { LAVPixFmt_None, 8,      { PIXOUT_420_8, PIXOUT_420_10, PIXOUT_420_16, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8 } },

  // 4:2:0
  { LAVPixFmt_YUV420, 8,    { PIXOUT_420_8, PIXOUT_420_10, PIXOUT_420_16, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8 } },
  { LAVPixFmt_NV12,   8,    { PIXOUT_420_8, PIXOUT_420_10, PIXOUT_420_16, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8 } },
  { LAVPixFmt_P016,   10,   { PIXOUT_420_10, PIXOUT_420_16, PIXOUT_420_8, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8 } },
  { LAVPixFmt_P016,   16,   { PIXOUT_420_16, PIXOUT_420_10, PIXOUT_420_8, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8 } },

  { LAVPixFmt_YUV420bX, 10, { PIXOUT_420_10, PIXOUT_420_16, PIXOUT_420_8, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8 } },
  { LAVPixFmt_YUV420bX, 16, { PIXOUT_420_16, PIXOUT_420_10, PIXOUT_420_8, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8 } },

  // 4:2:2
  { LAVPixFmt_YUV422, 8,    { PIXOUT_422_8, PIXOUT_422_10, PIXOUT_422_16, PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8, PIXOUT_420_16, PIXOUT_420_10, PIXOUT_420_8 } },
  { LAVPixFmt_YUY2,   8,    { PIXOUT_422_8, PIXOUT_422_10, PIXOUT_422_16, PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8, PIXOUT_420_16, PIXOUT_420_10, PIXOUT_420_8 } },

  { LAVPixFmt_YUV422bX, 10, { PIXOUT_422_10, PIXOUT_422_16, PIXOUT_422_8, PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8, PIXOUT_420_16, PIXOUT_420_10, PIXOUT_420_8 } },
  { LAVPixFmt_YUV422bX, 16, { PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8, PIXOUT_420_16, PIXOUT_420_10, PIXOUT_420_8 } },
  
  // 4:4:4
  { LAVPixFmt_YUV444,    8, { PIXOUT_444_8, PIXOUT_444_10, PIXOUT_444_16, PIXOUT_RGB_16, PIXOUT_RGB_8, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_420_16, PIXOUT_420_10, PIXOUT_420_8 } },
  { LAVPixFmt_YUV444bX, 10, { PIXOUT_444_10, PIXOUT_444_16, PIXOUT_444_8, PIXOUT_RGB_16, PIXOUT_RGB_8, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_420_16, PIXOUT_420_10, PIXOUT_420_8 } },
  { LAVPixFmt_YUV444bX, 16, { PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8, PIXOUT_RGB_16, PIXOUT_RGB_8, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_420_16, PIXOUT_420_10, PIXOUT_420_8 } },

  // RGB
  { LAVPixFmt_RGB24,  8,    { LAVOutPixFmt_RGB24, LAVOutPixFmt_RGB32, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_420_16, PIXOUT_420_10, PIXOUT_420_8 } },
  { LAVPixFmt_RGB32,  8,    { PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_420_16, PIXOUT_420_10, PIXOUT_420_8 } },
  { LAVPixFmt_ARGB32, 8,    { PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_420_16, PIXOUT_420_10, PIXOUT_420_8 } },
  { LAVPixFmt_RGB48,  8,    { PIXOUT_RGB_16, PIXOUT_RGB_8, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_420_16, PIXOUT_420_10, PIXOUT_420_8 } },

  { LAVPixFmt_DXVA2,  8,    { PIXOUT_420_8, PIXOUT_420_10, PIXOUT_420_16, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_RGB_8, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8, PIXOUT_RGB_16 } },
  { LAVPixFmt_DXVA2, 10,    { PIXOUT_420_10, PIXOUT_420_16, PIXOUT_420_8, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8 } },
  { LAVPixFmt_DXVA2, 16,    { PIXOUT_420_16, PIXOUT_420_10, PIXOUT_420_8, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8 } },

  { LAVPixFmt_D3D11,  8,    { PIXOUT_420_8, PIXOUT_420_10, PIXOUT_420_16, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_RGB_8, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8, PIXOUT_RGB_16 } },
  { LAVPixFmt_D3D11, 10,    { PIXOUT_420_10, PIXOUT_420_16, PIXOUT_420_8, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8 } },
  { LAVPixFmt_D3D11, 16,    { PIXOUT_420_16, PIXOUT_420_10, PIXOUT_420_8, PIXOUT_422_16, PIXOUT_422_10, PIXOUT_422_8, PIXOUT_RGB_8, PIXOUT_RGB_16, PIXOUT_444_16, PIXOUT_444_10, PIXOUT_444_8 } },
};

LAVOutPixFmtDesc lav_pixfmt_desc[] = {
  { MEDIASUBTYPE_YV12,  12, 1, 3, { 1, 2, 2 }, { 1, 2, 2 } },        // YV12
  { MEDIASUBTYPE_NV12,  12, 1, 2, { 1, 2 }, { 1, 1 } },              // NV12
  { MEDIASUBTYPE_YUY2,  16, 2, 0, { 1 }, { 1 } },                    // YUY2 (packed)
  { MEDIASUBTYPE_UYVY,  16, 2, 0, { 1 }, { 1 } },                    // UYVY (packed)
  { MEDIASUBTYPE_AYUV,  32, 4, 0, { 1 }, { 1 } },                    // AYUV (packed)
  { MEDIASUBTYPE_P010,  24, 2, 2, { 1, 2 }, { 1, 1 } },              // P010
  { MEDIASUBTYPE_P210,  32, 2, 2, { 1, 1 }, { 1, 1 } },              // P210
  { FOURCCMap('014Y'),  32, 4, 0, { 1 }, { 1 }  },                   // Y410 (packed)
  { MEDIASUBTYPE_P016,  24, 2, 2, { 1, 2 }, { 1, 1 } },              // P016
  { MEDIASUBTYPE_P216,  32, 2, 2, { 1, 1 }, { 1, 1 } },              // P216
  { FOURCCMap('614Y'),  64, 8, 0, { 1 }, { 1 } },                    // Y416 (packed)
  { MEDIASUBTYPE_RGB32, 32, 4, 0, { 1 }, { 1 } },                    // RGB32
  { MEDIASUBTYPE_RGB24, 24, 3, 0, { 1 }, { 1 } },                    // RGB24
  { FOURCCMap('012v'),  24, 4, 0, { 1 }, { 1 } },                    // v210 (packed)
  { FOURCCMap('014v'),  32, 4, 0, { 1 }, { 1 }  },                   // v410 (packed)
  { MEDIASUBTYPE_YV16,  16, 1, 3, { 1, 1, 1 }, { 1, 2, 2 } },        // YV16
  { MEDIASUBTYPE_YV24,  24, 1, 3, { 1, 1, 1 }, { 1, 1, 1 } },        // YV24
  { FOURCCMap('0BGR'),  48, 6, 0, { 1 }, { 1 } },                    // RGB48
};

static LAV_INOUT_PIXFMT_MAP *lookupFormatMap(LAVPixelFormat informat, int bpp, BOOL bFallbackToDefault = TRUE)
{
  for (int i = 0; i < countof(lav_pixfmt_map); ++i) {
    if (lav_pixfmt_map[i].in_pix_fmt == informat && bpp <= lav_pixfmt_map[i].maxbpp) {
      return &lav_pixfmt_map[i];
    }
  }
  if (bFallbackToDefault)
    return &lav_pixfmt_map[0];
  return nullptr;
}

CLAVPixFmtConverter::CLAVPixFmtConverter()
{
  convert = &CLAVPixFmtConverter::convert_generic;
  convert_direct = nullptr;

  m_NumThreads = min(8, max(1, av_cpu_count() / 2));

  ZeroMemory(&m_ColorProps, sizeof(m_ColorProps));
}

CLAVPixFmtConverter::~CLAVPixFmtConverter()
{
  DestroySWScale();
  av_freep(&m_pAlignedBuffer);
}

LAVOutPixFmts CLAVPixFmtConverter::GetOutputBySubtype(const GUID *guid)
{
  for (int i = 0; i < countof(lav_pixfmt_desc); ++i) {
    if (lav_pixfmt_desc[i].subtype == *guid) {
      return (LAVOutPixFmts)i;
    }
  }
  return LAVOutPixFmt_None;
}

static bool IsDXVAPixFmt(LAVPixelFormat inputFormat, LAVOutPixFmts outputFormat, int bpp)
{
  if (inputFormat != LAVPixFmt_DXVA2 && inputFormat != LAVPixFmt_D3D11)
    return false;

  if (bpp == 8 && outputFormat == LAVOutPixFmt_NV12)
    return true;
  else if (bpp == 10 && outputFormat == LAVOutPixFmt_P010)
    return true;
  else if (bpp == 12 && outputFormat == LAVOutPixFmt_P016)
    return true;

  return false;
}

int CLAVPixFmtConverter::GetFilteredFormatCount()
{
  LAV_INOUT_PIXFMT_MAP *pixFmtMap = lookupFormatMap(m_InputPixFmt, m_InBpp);
  int count = 0;
  for (int i = 0; i < LAVOutPixFmt_NB; ++i) {
    if (m_pSettings->GetPixelFormat(pixFmtMap->lav_pix_fmts[i]) || IsDXVAPixFmt(m_InputPixFmt, pixFmtMap->lav_pix_fmts[i], m_InBpp))
      count++;
  }

  if (count == 0)
    count = LAVOutPixFmt_NB;

  return count;
}

LAVOutPixFmts CLAVPixFmtConverter::GetFilteredFormat(int index)
{
  LAV_INOUT_PIXFMT_MAP *pixFmtMap = lookupFormatMap(m_InputPixFmt, m_InBpp);
  int actualIndex = -1;
  for (int i = 0; i < LAVOutPixFmt_NB; ++i) {
    if (m_pSettings->GetPixelFormat(pixFmtMap->lav_pix_fmts[i]) || IsDXVAPixFmt(m_InputPixFmt, pixFmtMap->lav_pix_fmts[i], m_InBpp))
      actualIndex++;
    if (index == actualIndex)
      return pixFmtMap->lav_pix_fmts[i];
  }

  // If no format is enabled, we use the fallback formats to avoid catastrophic failure
  if (index >= LAVOutPixFmt_NB)
    index = 0;
  return lav_pixfmt_map[0].lav_pix_fmts[index];
}

LAVOutPixFmts CLAVPixFmtConverter::GetPreferredOutput()
{
  return GetFilteredFormat(0);
}

int CLAVPixFmtConverter::GetNumMediaTypes()
{
  return GetFilteredFormatCount();
}

void CLAVPixFmtConverter::GetMediaType(CMediaType *mt, int index, LONG biWidth, LONG biHeight, DWORD dwAspectX, DWORD dwAspectY, REFERENCE_TIME rtAvgTime, BOOL bInterlaced, BOOL bVIH1)
{
  if (index < 0 || index >= GetFilteredFormatCount())
    index = 0;

  LAVOutPixFmts pixFmt = GetFilteredFormat(index);

  GUID guid = lav_pixfmt_desc[pixFmt].subtype;

  mt->SetType(&MEDIATYPE_Video);
  mt->SetSubtype(&guid);

  BITMAPINFOHEADER *pBIH = nullptr;
  if (bVIH1) {
    mt->SetFormatType(&FORMAT_VideoInfo);

    VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)mt->ReallocFormatBuffer(sizeof(VIDEOINFOHEADER));
    memset(vih, 0, sizeof(VIDEOINFOHEADER));

    vih->rcSource.right = vih->rcTarget.right = biWidth;
    vih->rcSource.bottom = vih->rcTarget.bottom = abs(biHeight);
    vih->AvgTimePerFrame = rtAvgTime;

    pBIH = &vih->bmiHeader;
  } else {
    mt->SetFormatType(&FORMAT_VideoInfo2);

    VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)mt->ReallocFormatBuffer(sizeof(VIDEOINFOHEADER2));
    memset(vih2, 0, sizeof(VIDEOINFOHEADER2));

    // Validate the Aspect Ratio - an AR of 0 crashes VMR-9
    if (dwAspectX == 0 || dwAspectY == 0) {
      dwAspectX = biWidth;
      dwAspectY = abs(biHeight);
    }

    // Always reduce the AR to the smalles fraction
    int dwX = 0, dwY = 0;
    av_reduce(&dwX, &dwY, dwAspectX, dwAspectY, max(dwAspectX, dwAspectY));

    vih2->rcSource.right = vih2->rcTarget.right = biWidth;
    vih2->rcSource.bottom = vih2->rcTarget.bottom = abs(biHeight);
    vih2->AvgTimePerFrame = rtAvgTime;
    vih2->dwPictAspectRatioX = dwX;
    vih2->dwPictAspectRatioY = dwY;

    if (bInterlaced)
      vih2->dwInterlaceFlags = AMINTERLACE_IsInterlaced | AMINTERLACE_DisplayModeBobOrWeave;

    pBIH = &vih2->bmiHeader;
  }

  pBIH->biSize = sizeof(BITMAPINFOHEADER);
  pBIH->biWidth = biWidth;
  pBIH->biHeight = biHeight;
  pBIH->biBitCount = lav_pixfmt_desc[pixFmt].bpp;
  pBIH->biPlanes = 1;
  pBIH->biSizeImage = GetImageSize(biWidth, abs(biHeight), pixFmt);
  pBIH->biCompression = guid.Data1;

  if (guid == MEDIASUBTYPE_RGB32 || guid == MEDIASUBTYPE_RGB24) {
    pBIH->biCompression = BI_RGB;
  }

  // Correct implied stride for v210
  if (guid == FOURCCMap('012v')) {
    pBIH->biWidth = FFALIGN(biWidth, 48);
  }

  mt->SetSampleSize(pBIH->biSizeImage);
  mt->SetTemporalCompression(0);
}

DWORD CLAVPixFmtConverter::GetImageSize(int width, int height, LAVOutPixFmts pixFmt)
{
  if (pixFmt == LAVOutPixFmt_None)
    pixFmt = m_OutputPixFmt;

  if (pixFmt == LAVOutPixFmt_None) {
    ASSERT(0);
    // Safe value that should work for all pixel formats, just in case this happens
    return width * height * 16;
  }

  if (pixFmt == LAVOutPixFmt_v210)
    return ((width + 47) / 48) * 128 * height;
  else {
    return (width * height * lav_pixfmt_desc[pixFmt].bpp) >> 3;
  }
}

BOOL CLAVPixFmtConverter::IsAllowedSubtype(const GUID *guid)
{
  for (int i = 0; i < GetFilteredFormatCount(); ++i) {
    if (lav_pixfmt_desc[GetFilteredFormat(i)].subtype == *guid)
      return TRUE;
  }

  return FALSE;
}

#define OUTPUT_RGB (m_OutputPixFmt == LAVOutPixFmt_RGB32 || m_OutputPixFmt == LAVOutPixFmt_RGB24)

void CLAVPixFmtConverter::SelectConvertFunction()
{
  m_RequiredAlignment = 16;
  m_bRGBConverter = FALSE;
  convert = nullptr;

  int cpu = av_get_cpu_flags();
  if (m_OutputPixFmt == LAVOutPixFmt_v210 || m_OutputPixFmt == LAVOutPixFmt_v410) {
    // We assume that every filter that understands v210 will also properly handle it
    m_RequiredAlignment = 0;
  } else if ((m_OutputPixFmt == LAVOutPixFmt_RGB32 && (m_InputPixFmt == LAVPixFmt_RGB32 || m_InputPixFmt == LAVPixFmt_ARGB32))
    || (m_OutputPixFmt == LAVOutPixFmt_RGB24 && m_InputPixFmt == LAVPixFmt_RGB24) || (m_OutputPixFmt == LAVOutPixFmt_RGB48 && m_InputPixFmt == LAVPixFmt_RGB48)
    || (m_OutputPixFmt == LAVOutPixFmt_NV12 && m_InputPixFmt == LAVPixFmt_NV12)
    || ((m_OutputPixFmt == LAVOutPixFmt_P010 || m_OutputPixFmt == LAVOutPixFmt_P016) && m_InputPixFmt == LAVPixFmt_P016)) {
    if (cpu & AV_CPU_FLAG_SSE2)
      convert = &CLAVPixFmtConverter::plane_copy_sse2;
    else
      convert = &CLAVPixFmtConverter::plane_copy;
    m_RequiredAlignment = 0;
  } else if (m_InputPixFmt == LAVPixFmt_RGB48 && m_OutputPixFmt == LAVOutPixFmt_RGB32 && (cpu & AV_CPU_FLAG_SSSE3)) {
    convert = &CLAVPixFmtConverter::convert_rgb48_rgb32_ssse3;
  } else if (cpu & AV_CPU_FLAG_SSE2) {
    if (m_OutputPixFmt == LAVOutPixFmt_AYUV && m_InputPixFmt == LAVPixFmt_YUV444bX) {
      convert = &CLAVPixFmtConverter::convert_yuv444_ayuv_dither_le;
    } else if (m_OutputPixFmt == LAVOutPixFmt_AYUV && m_InputPixFmt == LAVPixFmt_YUV444) {
      convert = &CLAVPixFmtConverter::convert_yuv444_ayuv;
    } else if (m_OutputPixFmt == LAVOutPixFmt_Y410 && m_InputPixFmt == LAVPixFmt_YUV444bX && m_InBpp <= 10) {
      convert = &CLAVPixFmtConverter::convert_yuv444_y410;
    } else if (((m_OutputPixFmt == LAVOutPixFmt_YV12 || m_OutputPixFmt == LAVOutPixFmt_NV12) && m_InputPixFmt == LAVPixFmt_YUV420bX)
             || (m_OutputPixFmt == LAVOutPixFmt_YV16 && m_InputPixFmt == LAVPixFmt_YUV422bX)
             || (m_OutputPixFmt == LAVOutPixFmt_YV24 && m_InputPixFmt == LAVPixFmt_YUV444bX)) {
      if (m_OutputPixFmt == LAVOutPixFmt_NV12) {
        convert = &CLAVPixFmtConverter::convert_yuv_yv_nv12_dither_le<TRUE>;
      } else {
        convert = &CLAVPixFmtConverter::convert_yuv_yv_nv12_dither_le<FALSE>;
      }
      m_RequiredAlignment = 32;
    } else if (((m_OutputPixFmt == LAVOutPixFmt_P010 || m_OutputPixFmt == LAVOutPixFmt_P016) && m_InputPixFmt == LAVPixFmt_YUV420bX)
            || ((m_OutputPixFmt == LAVOutPixFmt_P210 || m_OutputPixFmt == LAVOutPixFmt_P216) && m_InputPixFmt == LAVPixFmt_YUV422bX)) {
      convert = &CLAVPixFmtConverter::convert_yuv420_px1x_le;
    } else if (m_OutputPixFmt == LAVOutPixFmt_NV12 && m_InputPixFmt == LAVPixFmt_YUV420) {
      convert = &CLAVPixFmtConverter::convert_yuv420_nv12;
      m_RequiredAlignment = 32;
    } else if (m_OutputPixFmt == LAVOutPixFmt_YUY2 && m_InputPixFmt == LAVPixFmt_YUV422) {
      convert = &CLAVPixFmtConverter::convert_yuv422_yuy2_uyvy<0>;
      m_RequiredAlignment = 32;
    } else if (m_OutputPixFmt == LAVOutPixFmt_UYVY && m_InputPixFmt == LAVPixFmt_YUV422) {
      convert = &CLAVPixFmtConverter::convert_yuv422_yuy2_uyvy<1>;
      m_RequiredAlignment = 32;
    } else if ((m_OutputPixFmt == LAVOutPixFmt_RGB32 || m_OutputPixFmt == LAVOutPixFmt_RGB24)
            && (m_InputPixFmt == LAVPixFmt_YUV420 || m_InputPixFmt == LAVPixFmt_YUV420bX
             || m_InputPixFmt == LAVPixFmt_YUV422 || m_InputPixFmt == LAVPixFmt_YUV422bX
             || m_InputPixFmt == LAVPixFmt_YUV444 || m_InputPixFmt == LAVPixFmt_YUV444bX
             || m_InputPixFmt == LAVPixFmt_NV12   || m_InputPixFmt == LAVPixFmt_P016)) {
      convert = &CLAVPixFmtConverter::convert_yuv_rgb;
      if (m_OutputPixFmt == LAVOutPixFmt_RGB32) {
        m_RequiredAlignment = 4;
      }
      m_bRGBConverter = TRUE;
    } else if (m_OutputPixFmt == LAVOutPixFmt_YV12 && m_InputPixFmt == LAVPixFmt_NV12) {
      convert = &CLAVPixFmtConverter::convert_nv12_yv12;
      m_RequiredAlignment = 32;
    } else if ((m_OutputPixFmt == LAVOutPixFmt_YUY2 || m_OutputPixFmt == LAVOutPixFmt_UYVY) && (m_InputPixFmt == LAVPixFmt_YUV420 || m_InputPixFmt == LAVPixFmt_NV12 || m_InputPixFmt == LAVPixFmt_YUV420bX) && m_InBpp <= 14) {
      if (m_OutputPixFmt == LAVOutPixFmt_YUY2) {
        convert = &CLAVPixFmtConverter::convert_yuv420_yuy2<0>;
      } else {
        convert = &CLAVPixFmtConverter::convert_yuv420_yuy2<1>;
      }
      m_RequiredAlignment = 8; // Pixel alignment of 8 guarantees a byte alignment of 16
    } else if ((m_OutputPixFmt == LAVOutPixFmt_YUY2 || m_OutputPixFmt == LAVOutPixFmt_UYVY) && m_InputPixFmt == LAVPixFmt_YUV422bX) {
      if (m_OutputPixFmt == LAVOutPixFmt_YUY2) {
        convert = &CLAVPixFmtConverter::convert_yuv422_yuy2_uyvy_dither_le<0>;
      } else {
        convert = &CLAVPixFmtConverter::convert_yuv422_yuy2_uyvy_dither_le<1>;
      }

      m_RequiredAlignment = 8; // Pixel alignment of 8 guarantees a byte alignment of 16
    } else if ((m_OutputPixFmt == LAVOutPixFmt_YV12 && m_InputPixFmt == LAVPixFmt_YUV420)
            || (m_OutputPixFmt == LAVOutPixFmt_YV16 && m_InputPixFmt == LAVPixFmt_YUV422)
            || (m_OutputPixFmt == LAVOutPixFmt_YV24 && m_InputPixFmt == LAVPixFmt_YUV444)) {
      convert = &CLAVPixFmtConverter::convert_yuv_yv;
      m_RequiredAlignment = 0;
    } else if (m_InputPixFmt == LAVPixFmt_RGB48 && (m_OutputPixFmt == LAVOutPixFmt_RGB24 || m_OutputPixFmt == LAVOutPixFmt_RGB32)) {
      if (m_OutputPixFmt == LAVOutPixFmt_RGB32)
        convert = &CLAVPixFmtConverter::convert_rgb48_rgb<1>;
      else
        convert = &CLAVPixFmtConverter::convert_rgb48_rgb<0>;
    } else if (m_InputPixFmt == LAVPixFmt_P016 && m_OutputPixFmt == LAVOutPixFmt_NV12) {
      convert = &CLAVPixFmtConverter::convert_p010_nv12_sse2;
    }
  }

  if (convert == nullptr) {
    convert = &CLAVPixFmtConverter::convert_generic;
  }

  SelectConvertFunctionDirect();
}

void CLAVPixFmtConverter::SelectConvertFunctionDirect()
{
  convert_direct = nullptr;
  m_bDirectMode = FALSE;

  int cpu = av_get_cpu_flags();
  if ((m_InputPixFmt == LAVPixFmt_NV12 && m_OutputPixFmt == LAVOutPixFmt_NV12)
   || (m_InputPixFmt == LAVPixFmt_P016 && (m_OutputPixFmt == LAVOutPixFmt_P010 || m_OutputPixFmt == LAVOutPixFmt_P016))) {
    if (cpu & AV_CPU_FLAG_SSE4)
      convert_direct = &CLAVPixFmtConverter::plane_copy_direct_sse4;
    else if (cpu & AV_CPU_FLAG_SSE2)
      convert_direct = &CLAVPixFmtConverter::plane_copy_sse2;
    else
      convert_direct = &CLAVPixFmtConverter::plane_copy;
  } else if (m_InputPixFmt == LAVPixFmt_P016 && m_OutputPixFmt == LAVOutPixFmt_NV12) {
    if (cpu & AV_CPU_FLAG_SSE4)
      convert_direct = &CLAVPixFmtConverter::convert_p010_nv12_direct_sse4;
    else if (cpu & AV_CPU_FLAG_SSE2)
      convert_direct = &CLAVPixFmtConverter::convert_p010_nv12_sse2;
  } else if (m_InputPixFmt == LAVPixFmt_NV12 && m_OutputPixFmt == LAVOutPixFmt_YV12) {
    if (cpu & AV_CPU_FLAG_SSE4)
      convert_direct = &CLAVPixFmtConverter::convert_nv12_yv12_direct_sse4;
    else if (cpu & AV_CPU_FLAG_SSE2)
      convert_direct = &CLAVPixFmtConverter::convert_nv12_yv12;
  }

  if (convert_direct != nullptr)
    m_bDirectMode = TRUE;
}

HRESULT CLAVPixFmtConverter::Convert(const BYTE* const src[4], const ptrdiff_t srcStride[4], uint8_t *dst, int width, int height, ptrdiff_t dstStride, int planeHeight) {
  uint8_t *out = dst;
  ptrdiff_t outStride = dstStride, i;
  planeHeight = max(height, planeHeight);
  // Check if we have proper pixel alignment and the dst memory is actually aligned
  if (m_RequiredAlignment && (FFALIGN(dstStride, m_RequiredAlignment) != dstStride || ((uintptr_t)dst % 16u))) {
    outStride = FFALIGN(dstStride, m_RequiredAlignment);
    size_t requiredSize = (outStride * planeHeight * lav_pixfmt_desc[m_OutputPixFmt].bpp) >> 3;
    if (requiredSize > m_nAlignedBufferSize || !m_pAlignedBuffer) {
      DbgLog((LOG_TRACE, 10, L"::Convert(): Conversion requires a bigger stride (need: %d, have: %d), allocating buffer...", outStride, dstStride));
      av_freep(&m_pAlignedBuffer);
      m_pAlignedBuffer = (uint8_t *)av_malloc(requiredSize+ AV_INPUT_BUFFER_PADDING_SIZE);
      if (!m_pAlignedBuffer) {
        return E_FAIL;
      }
      m_nAlignedBufferSize = requiredSize;
    }
    out = m_pAlignedBuffer;
  }

  uint8_t *dstArray[4] = {0};
  ptrdiff_t dstStrideArray[4] = {0};
  ptrdiff_t byteStride = outStride * lav_pixfmt_desc[m_OutputPixFmt].codedbytes;

  dstArray[0] = out;
  dstStrideArray[0] = byteStride;

  for (i = 1; i < lav_pixfmt_desc[m_OutputPixFmt].planes; ++i) {
    dstArray[i] = dstArray[i-1] + dstStrideArray[i-1] * (planeHeight / lav_pixfmt_desc[m_OutputPixFmt].planeHeight[i-1]);
    dstStrideArray[i] = byteStride / lav_pixfmt_desc[m_OutputPixFmt].planeWidth[i];
  }

  HRESULT hr = (this->*convert)(src, srcStride, dstArray, dstStrideArray, width, height, m_InputPixFmt, m_InBpp, m_OutputPixFmt);
  if (out != dst) {
    ChangeStride(out, outStride, dst, dstStride, width, height, planeHeight, m_OutputPixFmt);
  }
  return hr;
}

BOOL CLAVPixFmtConverter::IsDirectModeSupported(uintptr_t dst, ptrdiff_t stride) {
  const int stride_align = (m_OutputPixFmt == LAVOutPixFmt_YV12 ? 32 : 16);
  if (FFALIGN(stride, stride_align) != stride || (dst % 16u))
    return false;
  return m_bDirectMode;
}

HRESULT CLAVPixFmtConverter::ConvertDirect(LAVFrame *pFrame, uint8_t *dst, int width, int height, ptrdiff_t dstStride, int planeHeight)
{
  HRESULT hr = S_OK;
  planeHeight = max(height, planeHeight);
  ASSERT(pFrame->direct && pFrame->direct_lock && pFrame->direct_unlock);

  LAVDirectBuffer buffer;
  if (pFrame->direct_lock(pFrame, &buffer)) {
    uint8_t *dstArray[4] = { 0 };
    ptrdiff_t dstStrideArray[4] = { 0 };
    ptrdiff_t byteStride = dstStride * lav_pixfmt_desc[m_OutputPixFmt].codedbytes;

    dstArray[0] = dst;
    dstStrideArray[0] = byteStride;

    for (int i = 1; i < lav_pixfmt_desc[m_OutputPixFmt].planes; ++i) {
      dstArray[i] = dstArray[i - 1] + dstStrideArray[i - 1] * (planeHeight / lav_pixfmt_desc[m_OutputPixFmt].planeHeight[i - 1]);
      dstStrideArray[i] = byteStride / lav_pixfmt_desc[m_OutputPixFmt].planeWidth[i];
    }

    hr = (this->*convert_direct)(buffer.data, buffer.stride, dstArray, dstStrideArray, width, height, m_InputPixFmt, m_InBpp, m_OutputPixFmt);
    pFrame->direct_unlock(pFrame);
  }

  return hr;
}

void CLAVPixFmtConverter::ChangeStride(const uint8_t* src, ptrdiff_t srcStride, uint8_t *dst, ptrdiff_t dstStride, int width, int height, int planeHeight, LAVOutPixFmts format)
{
  LAVOutPixFmtDesc desc = lav_pixfmt_desc[format];

  int line = 0;

  // Copy first plane
  const size_t widthBytes = width * desc.codedbytes;
  const ptrdiff_t srcStrideBytes = srcStride * desc.codedbytes;
  const ptrdiff_t dstStrideBytes = dstStride * desc.codedbytes;
  for (line = 0; line < height; ++line) {
    memcpy(dst, src, widthBytes);
    src += srcStrideBytes;
    dst += dstStrideBytes;
  }
  dst += (planeHeight - height) * dstStrideBytes;

  for (int plane = 1; plane < desc.planes; ++plane) {
    const size_t planeWidth        = widthBytes     / desc.planeWidth[plane];
    const int activePlaneHeight = height         / desc.planeHeight[plane];
    const int totalPlaneHeight  = planeHeight    / desc.planeHeight[plane];
    const ptrdiff_t srcPlaneStride    = srcStrideBytes / desc.planeWidth[plane];
    const ptrdiff_t dstPlaneStride    = dstStrideBytes / desc.planeWidth[plane];
    for (line = 0; line < activePlaneHeight; ++line) {
      memcpy(dst, src, planeWidth);
      src += srcPlaneStride;
      dst += dstPlaneStride;
    }
    dst += (totalPlaneHeight - activePlaneHeight) * dstPlaneStride;
  }
}

const uint16_t* CLAVPixFmtConverter::GetRandomDitherCoeffs(int height, int coeffs, int bits, int line)
{
  if (m_pSettings->GetDitherMode() != LAVDither_Random)
    return nullptr;

  int totalWidth = 8 * coeffs;
  if (!m_pRandomDithers || totalWidth > m_ditherWidth || height > m_ditherHeight || bits != m_ditherBits) {
    if (m_pRandomDithers)
      _aligned_free(m_pRandomDithers);
    m_pRandomDithers = nullptr;
    m_ditherWidth = totalWidth;
    m_ditherHeight = height;
    m_ditherBits = bits;
    m_pRandomDithers = (uint16_t *)_aligned_malloc(m_ditherWidth * m_ditherHeight * 2, 16);
    if (m_pRandomDithers == nullptr)
      return nullptr;

#ifdef DEBUG
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    DbgLog((LOG_TRACE, 10, L"Creating dither matrix"));
#endif

    // Seed random number generator
    time_t seed = time(nullptr);
    seed >>= 1;
    srand_sse((unsigned int)seed);

    bits = (1 << bits);
    for (int i = 0; i < m_ditherHeight; i++) {
      uint16_t *ditherline = m_pRandomDithers + (m_ditherWidth * i);
      for (int j = 0; j < m_ditherWidth; j += 4) {
        int rnds[4];
        rand_sse(rnds);
        ditherline[j+0] = rnds[0] % bits;
        ditherline[j+1] = rnds[1] % bits;
        ditherline[j+2] = rnds[2] % bits;
        ditherline[j+3] = rnds[3] % bits;
      }
    }
#ifdef DEBUG
    QueryPerformanceCounter(&end);
    double diff = (end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;
    DbgLog((LOG_TRACE, 10, L"Finished creating dither matrix (took %2.3fms)", diff));
#endif

  }

  if (line < 0 || line >= m_ditherHeight)
    line = rand() % m_ditherHeight;

  return &m_pRandomDithers[line * m_ditherWidth];
}
