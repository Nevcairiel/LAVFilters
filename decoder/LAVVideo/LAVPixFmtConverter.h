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

#include "LAVVideoSettings.h"
#include "pixconv/pixconv_internal.h"

#define CONV_FUNC_PARAMS (const uint8_t* const src[4], const int srcStride[4], uint8_t *dst, int dstStride, int width, int height, PixelFormat inputFormat, LAVVideoPixFmts outputFormat)

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
} LAVPixFmtDesc;

extern LAVPixFmtDesc lav_pixfmt_desc[];

class CLAVPixFmtConverter
{
public:
  CLAVPixFmtConverter();
  ~CLAVPixFmtConverter();

  void SetSettings(ILAVVideoSettings *pSettings) { m_pSettings = pSettings; }

  HRESULT SetInputPixFmt(enum PixelFormat pix_fmt) { m_InputPixFmt = pix_fmt; DestroySWScale(); return S_OK; }
  HRESULT SetOutputPixFmt(enum LAVVideoPixFmts pix_fmt) { m_OutputPixFmt = pix_fmt; DestroySWScale(); return S_OK; }
  
  LAVVideoPixFmts GetOutputBySubtype(const GUID *guid);
  LAVVideoPixFmts GetPreferredOutput();

  PixelFormat GetInputPixFmt() { return m_InputPixFmt; }
  void SetColorProps(AVColorSpace colorspace, AVColorRange range) { if (swsColorSpace != colorspace || swsColorRange != range) { DestroySWScale(); swsColorSpace = colorspace; swsColorRange = range; } }

  int GetNumMediaTypes();
  CMediaType GetMediaType(int index, LONG biWidth, LONG biHeight, DWORD dwAspectX, DWORD dwAspectY, REFERENCE_TIME rtAvgTime, BOOL bVIH1 = FALSE);
  BOOL IsAllowedSubtype(const GUID *guid);

  inline HRESULT Convert(AVFrame *pFrame, uint8_t *dst, int width, int height, int dstStride) {
    return (this->*convert)(pFrame->data, pFrame->linesize, dst, dstStride, width, height, m_InputPixFmt, m_OutputPixFmt);
  }

private:
  int GetFilteredFormatCount();
  LAVVideoPixFmts GetFilteredFormat(int index);

  // Helper functions for convert_generic
  HRESULT swscale_scale(enum PixelFormat srcPix, enum PixelFormat dstPix, const uint8_t* const src[], const int srcStride[], BYTE *pOut, int width, int height, int stride, LAVPixFmtDesc pixFmtDesc, bool swapPlanes12 = false);
  HRESULT ConvertTo422Packed(const uint8_t* const src[4], const int srcStride[4], uint8_t *dst, int width, int height, int dstStride);
  HRESULT ConvertToAYUV(const uint8_t* const src[4], const int srcStride[4], uint8_t *dst, int width, int height, int dstStride);
  HRESULT ConvertToPX1X(const uint8_t* const src[4], const int srcStride[4], uint8_t *dst, int width, int height, int dstStride, int chromaVertical);
  HRESULT ConvertToY410(const uint8_t* const src[4], const int srcStride[4], uint8_t *dst, int width, int height, int dstStride);
  HRESULT ConvertToY416(const uint8_t* const src[4], const int srcStride[4], uint8_t *dst, int width, int height, int dstStride);

  void DestroySWScale() { if (m_pSwsContext) sws_freeContext(m_pSwsContext); m_pSwsContext = NULL; };
  SwsContext *GetSWSContext(int width, int height, enum PixelFormat srcPix, enum PixelFormat dstPix, int flags);

  void ChangeStride(const uint8_t* src, int srcStride, uint8_t *dst, int dstStride, int width, int height, LAVVideoPixFmts format);

  typedef HRESULT (CLAVPixFmtConverter::*ConverterFn) CONV_FUNC_PARAMS;

  // Conversion function pointer
  ConverterFn convert;

  // Pixel Implementations
  DECLARE_CONV_FUNC(convert_generic);

private:
  enum PixelFormat     m_InputPixFmt;
  enum LAVVideoPixFmts m_OutputPixFmt;

  int swsWidth, swsHeight;
  AVColorSpace swsColorSpace;
  AVColorRange swsColorRange;

  SwsContext *m_pSwsContext;

  ILAVVideoSettings *m_pSettings;
};
