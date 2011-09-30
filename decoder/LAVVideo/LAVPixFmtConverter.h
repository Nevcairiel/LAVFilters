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
 */

#pragma once

#include "LAVVideoSettings.h"
#include "decoders/ILAVDecoder.h"

#define CONV_FUNC_PARAMS (const uint8_t* const src[4], const int srcStride[4], uint8_t *dst, int dstStride, int width, int height, LAVPixelFormat inputFormat, int bpp, LAVOutPixFmts outputFormat)

#define DECLARE_CONV_FUNC(name) \
  HRESULT name CONV_FUNC_PARAMS

#define DECLARE_CONV_FUNC_IMPL(name) \
  DECLARE_CONV_FUNC(CLAVPixFmtConverter::name)

// Important, when adding new pixel formats, they need to be added in LAVPixFmtConverter.cpp as well to the format descriptors// Important, when adding new pixel formats, they need to be added in LAVPixFmtConverter.cpp as well to the format descriptors
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
  void GetMediaType(CMediaType *mt, int index, LONG biWidth, LONG biHeight, DWORD dwAspectX, DWORD dwAspectY, REFERENCE_TIME rtAvgTime, BOOL bVIH1 = FALSE, BOOL bHWDeint = FALSE);
  BOOL IsAllowedSubtype(const GUID *guid);

  inline HRESULT Convert(LAVFrame *pFrame, uint8_t *dst, int width, int height, int dstStride) {
    uint8_t *out = dst;
    int outStride = dstStride;
    if (m_RequiredAlignment && FFALIGN(dstStride, m_RequiredAlignment) != dstStride) {
      outStride = FFALIGN(dstStride, m_RequiredAlignment);
      size_t requiredSize = (outStride * height * lav_pixfmt_desc[m_OutputPixFmt].bpp) << 3;
      if (requiredSize > m_nAlignedBufferSize) {
        DbgLog((LOG_TRACE, 10, L"::Convert(): Conversion requires a bigger stride (need: %d, have: %d), allocating buffer...", outStride, dstStride));
        av_freep(&m_pAlignedBuffer);
        m_nAlignedBufferSize = requiredSize;
        m_pAlignedBuffer = (uint8_t *)av_malloc(m_nAlignedBufferSize+FF_INPUT_BUFFER_PADDING_SIZE);
      }
      out = m_pAlignedBuffer;
    }
    HRESULT hr = (this->*convert)(pFrame->data, pFrame->stride, out, outStride, width, height, m_InputPixFmt, m_InBpp, m_OutputPixFmt);
    if (outStride != dstStride) {
      ChangeStride(out, outStride, dst, dstStride, width, height, m_OutputPixFmt);
    }
    return hr;
  }

  BOOL IsRGBConverterActive() { return m_bRGBConverter; }

private:
  PixelFormat GetFFInput() {
    return getFFPixelFormatFromLAV(m_InputPixFmt, m_InBpp);
  }

  int GetFilteredFormatCount();
  LAVOutPixFmts GetFilteredFormat(int index);

  void SelectConvertFunction();

  // Helper functions for convert_generic
  HRESULT swscale_scale(enum PixelFormat srcPix, enum PixelFormat dstPix, const uint8_t* const src[], const int srcStride[], BYTE *pOut, int width, int height, int stride, LAVOutPixFmtDesc pixFmtDesc, bool swapPlanes12 = false);
  HRESULT ConvertTo422Packed(const uint8_t* const src[4], const int srcStride[4], uint8_t *dst, int width, int height, int dstStride);
  HRESULT ConvertToAYUV(const uint8_t* const src[4], const int srcStride[4], uint8_t *dst, int width, int height, int dstStride);
  HRESULT ConvertToPX1X(const uint8_t* const src[4], const int srcStride[4], uint8_t *dst, int width, int height, int dstStride, int chromaVertical);
  HRESULT ConvertToY410(const uint8_t* const src[4], const int srcStride[4], uint8_t *dst, int width, int height, int dstStride);
  HRESULT ConvertToY416(const uint8_t* const src[4], const int srcStride[4], uint8_t *dst, int width, int height, int dstStride);

  void DestroySWScale() { if (m_pSwsContext) sws_freeContext(m_pSwsContext); m_pSwsContext = NULL; if (m_rgbCoeffs) _aligned_free(m_rgbCoeffs); m_rgbCoeffs = NULL; };
  SwsContext *GetSWSContext(int width, int height, enum PixelFormat srcPix, enum PixelFormat dstPix, int flags);

  void ChangeStride(const uint8_t* src, int srcStride, uint8_t *dst, int dstStride, int width, int height, LAVOutPixFmts format);

  typedef HRESULT (CLAVPixFmtConverter::*ConverterFn) CONV_FUNC_PARAMS;

  // Conversion function pointer
  ConverterFn convert;

  // Pixel Implementations
  DECLARE_CONV_FUNC(convert_generic);
  DECLARE_CONV_FUNC(plane_copy);
  DECLARE_CONV_FUNC(convert_yuv444_ayuv);
  DECLARE_CONV_FUNC(convert_yuv444_ayuv_dither_le);
  DECLARE_CONV_FUNC(convert_yuv444_y410);
  template <int shift> DECLARE_CONV_FUNC(convert_yuv420_px1x_le);
  DECLARE_CONV_FUNC(convert_yuv420_nv12);
  DECLARE_CONV_FUNC(convert_yuv420_yv12);
  DECLARE_CONV_FUNC(convert_nv12_yv12);
  template <int uyvy> DECLARE_CONV_FUNC(convert_yuv420_yuy2);
  template <int uyvy> DECLARE_CONV_FUNC(convert_yuv422_yuy2_uyvy);
  template <int nv12> DECLARE_CONV_FUNC(convert_yuv420_yv12_nv12_dither_le);

  template <int out32> DECLARE_CONV_FUNC(convert_yuv_rgb);
  RGBCoeffs* getRGBCoeffs(int width, int height);

private:
  LAVPixelFormat  m_InputPixFmt;
  LAVOutPixFmts   m_OutputPixFmt;
  int                  m_InBpp;

  int swsWidth, swsHeight;
  int swsOutputRange;

  DXVA2_ExtendedFormat m_ColorProps;

  unsigned m_RequiredAlignment;

  SwsContext *m_pSwsContext;

  size_t   m_nAlignedBufferSize;
  uint8_t *m_pAlignedBuffer;

  int m_NumThreads;

  ILAVVideoSettings *m_pSettings;

  RGBCoeffs *m_rgbCoeffs;
  BOOL m_bRGBConverter;
};
