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

#pragma once

#include "LAVVideoSettings.h"
#include "decoders/ILAVDecoder.h"

#include <emmintrin.h>

#define CONV_FUNC_PARAMS (const uint8_t* const src[4], const ptrdiff_t srcStride[4], uint8_t* dst[4], const ptrdiff_t dstStride[4], int width, int height, LAVPixelFormat inputFormat, int bpp, LAVOutPixFmts outputFormat)

#define DECLARE_CONV_FUNC(name) \
  HRESULT name CONV_FUNC_PARAMS

#define DECLARE_CONV_FUNC_IMPL(name) \
  DECLARE_CONV_FUNC(CLAVPixFmtConverter::name)

// Important, when adding new pixel formats, they need to be added in LAVPixFmtConverter.cpp as well to the format descriptors
typedef struct {
  GUID subtype;
  int bpp;
  int codedbytes;
  int planes;
  int planeHeight[4];
  int planeWidth[4];
} LAVOutPixFmtDesc;

typedef struct _RGBCoeffs {
  __m128i Ysub;
  __m128i CbCr_center;
  __m128i rgb_add;
  __m128i cy;
  __m128i cR_Cr;
  __m128i cG_Cb_cG_Cr;
  __m128i cB_Cb;
} RGBCoeffs;

typedef int (__stdcall *YUVRGBConversionFunc)(const uint8_t *srcY, const uint8_t *srcU, const uint8_t *srcV, uint8_t *dst, int width, int height, ptrdiff_t srcStrideY, ptrdiff_t srcStrideUV, ptrdiff_t dstStride, ptrdiff_t sliceYStart, ptrdiff_t sliceYEnd, const RGBCoeffs *coeffs, const uint16_t *dithers);

extern LAVOutPixFmtDesc lav_pixfmt_desc[];

class CLAVPixFmtConverter
{
public:
  CLAVPixFmtConverter();
  ~CLAVPixFmtConverter();

  void SetSettings(ILAVVideoSettings *pSettings) { m_pSettings = pSettings; }

  BOOL SetInputFmt(enum LAVPixelFormat pixfmt, int bpp) { if (m_InputPixFmt != pixfmt || m_InBpp != bpp) { m_InputPixFmt = pixfmt; m_InBpp = bpp; DestroySWScale(); SelectConvertFunction(); return TRUE; } return FALSE; }
  HRESULT SetOutputPixFmt(enum LAVOutPixFmts pix_fmt) { m_OutputPixFmt = pix_fmt; DestroySWScale(); SelectConvertFunction(); return S_OK; }
  
  LAVOutPixFmts GetOutputBySubtype(const GUID *guid);
  LAVOutPixFmts GetPreferredOutput();

  LAVOutPixFmts GetOutputPixFmt() { return m_OutputPixFmt; }
  void SetColorProps(DXVA2_ExtendedFormat props, int RGBOutputRange) { if (props.value != m_ColorProps.value || swsOutputRange != RGBOutputRange) { DestroySWScale(); m_ColorProps = props; swsOutputRange = RGBOutputRange; } }

  int GetNumMediaTypes();
  void GetMediaType(CMediaType *mt, int index, LONG biWidth, LONG biHeight, DWORD dwAspectX, DWORD dwAspectY, REFERENCE_TIME rtAvgTime, BOOL bInterlaced = TRUE, BOOL bVIH1 = FALSE);
  BOOL IsAllowedSubtype(const GUID *guid);

  HRESULT Convert(const uint8_t * const src[4], const ptrdiff_t srcStride[4], uint8_t *dst, int width, int height, ptrdiff_t dstStride, int planeHeight);
  HRESULT ConvertDirect(LAVFrame *pFrame, uint8_t *dst, int width, int height, ptrdiff_t dstStride, int planeHeight);

  BOOL IsRGBConverterActive() { return m_bRGBConverter; }
  BOOL IsDirectModeSupported(uintptr_t dst, ptrdiff_t stride);

  DWORD GetImageSize(int width, int height, LAVOutPixFmts pixFmt = LAVOutPixFmt_None);

private:
  AVPixelFormat GetFFInput() {
    return getFFPixelFormatFromLAV(m_InputPixFmt, m_InBpp);
  }

  int GetFilteredFormatCount();
  LAVOutPixFmts GetFilteredFormat(int index);

  void SelectConvertFunction();
  void SelectConvertFunctionDirect();

  // Helper functions for convert_generic
  HRESULT swscale_scale(enum AVPixelFormat srcPix, enum AVPixelFormat dstPix, const uint8_t* const src[4], const ptrdiff_t srcStride[4], uint8_t* dst[4], int width, int height, const ptrdiff_t dstStride[4], LAVOutPixFmtDesc pixFmtDesc, bool swapPlanes12 = false);
  HRESULT ConvertTo422Packed(const uint8_t* const src[4], const ptrdiff_t srcStride[4], uint8_t *dst[4], int width, int height, const ptrdiff_t dstStride[4]);
  HRESULT ConvertToAYUV(const uint8_t* const src[4], const ptrdiff_t srcStride[4], uint8_t *dst[4], int width, int height, const ptrdiff_t dstStride[4]);
  HRESULT ConvertToPX1X(const uint8_t* const src[4], const ptrdiff_t srcStride[4], uint8_t *dst[4], int width, int height, const ptrdiff_t dstStride[4], int chromaVertical);
  HRESULT ConvertToY410(const uint8_t* const src[4], const ptrdiff_t srcStride[4], uint8_t *dst[4], int width, int height, const ptrdiff_t dstStride[4]);
  HRESULT ConvertToY416(const uint8_t* const src[4], const ptrdiff_t srcStride[4], uint8_t *dst[4], int width, int height, const ptrdiff_t dstStride[4]);
  HRESULT ConvertTov210(const uint8_t* const src[4], const ptrdiff_t srcStride[4], uint8_t *dst[4], int width, int height, const ptrdiff_t dstStride[4]);
  HRESULT ConvertTov410(const uint8_t* const src[4], const ptrdiff_t srcStride[4], uint8_t *dst[4], int width, int height, const ptrdiff_t dstStride[4]);

  void DestroySWScale() { if (m_pSwsContext) sws_freeContext(m_pSwsContext); m_pSwsContext = nullptr; if (m_rgbCoeffs) _aligned_free(m_rgbCoeffs); m_rgbCoeffs = nullptr; if (m_pRandomDithers) _aligned_free(m_pRandomDithers); m_pRandomDithers = nullptr; };
  SwsContext *GetSWSContext(int width, int height, enum AVPixelFormat srcPix, enum AVPixelFormat dstPix, int flags);

  void ChangeStride(const uint8_t* src, ptrdiff_t srcStride, uint8_t *dst, ptrdiff_t dstStride, int width, int height, int planeHeight, LAVOutPixFmts format);

  typedef HRESULT (CLAVPixFmtConverter::*ConverterFn) CONV_FUNC_PARAMS;

  // Conversion function pointer
  ConverterFn convert;
  ConverterFn convert_direct;

  // Pixel Implementations
  DECLARE_CONV_FUNC(convert_generic);
  DECLARE_CONV_FUNC(plane_copy);
  DECLARE_CONV_FUNC(plane_copy_sse2);
  DECLARE_CONV_FUNC(convert_yuv444_ayuv);
  DECLARE_CONV_FUNC(convert_yuv444_ayuv_dither_le);
  DECLARE_CONV_FUNC(convert_yuv444_y410);
  DECLARE_CONV_FUNC(convert_yuv420_px1x_le);
  DECLARE_CONV_FUNC(convert_yuv420_nv12);
  DECLARE_CONV_FUNC(convert_yuv_yv);
  DECLARE_CONV_FUNC(convert_nv12_yv12);
  DECLARE_CONV_FUNC(convert_p010_nv12_sse2);
  template <int uyvy> DECLARE_CONV_FUNC(convert_yuv420_yuy2);
  template <int uyvy> DECLARE_CONV_FUNC(convert_yuv422_yuy2_uyvy);
  template <int uyvy> DECLARE_CONV_FUNC(convert_yuv422_yuy2_uyvy_dither_le);
  template <int nv12> DECLARE_CONV_FUNC(convert_yuv_yv_nv12_dither_le);

  DECLARE_CONV_FUNC(convert_rgb48_rgb32_ssse3);
  template <int out32> DECLARE_CONV_FUNC(convert_rgb48_rgb);

  DECLARE_CONV_FUNC(plane_copy_direct_sse4);
  DECLARE_CONV_FUNC(convert_nv12_yv12_direct_sse4);
  DECLARE_CONV_FUNC(convert_p010_nv12_direct_sse4);

  DECLARE_CONV_FUNC(convert_yuv_rgb);
  const RGBCoeffs* getRGBCoeffs(int width, int height);
  void InitRGBConvDispatcher();

  const uint16_t* GetRandomDitherCoeffs(int height, int coeffs, int bits, int line);

private:
  LAVPixelFormat  m_InputPixFmt  = LAVPixFmt_None;
  LAVOutPixFmts   m_OutputPixFmt = LAVOutPixFmt_YV12;
  int             m_InBpp        = 0;

  BOOL m_bDirectMode = false;

  int swsWidth       = 0;
  int swsHeight      = 0;
  int swsOutputRange = 0;

  DXVA2_ExtendedFormat m_ColorProps;

  ptrdiff_t m_RequiredAlignment  = 0;

  SwsContext *m_pSwsContext     = nullptr;

  size_t   m_nAlignedBufferSize = 0;
  uint8_t *m_pAlignedBuffer     = nullptr;

  int m_NumThreads              = 1;

  ILAVVideoSettings *m_pSettings = nullptr;

  RGBCoeffs *m_rgbCoeffs = nullptr;
  BOOL m_bRGBConverter   = FALSE;
  BOOL m_bRGBConvInit    = FALSE;

  // [out32][dithermode][ycgco][format][shift]
  YUVRGBConversionFunc m_RGBConvFuncs[2][2][2][LAVPixFmt_NB][9];

  uint16_t *m_pRandomDithers = nullptr;
  int m_ditherWidth  = 0;
  int m_ditherHeight = 0;
  int m_ditherBits   = 0;
};
