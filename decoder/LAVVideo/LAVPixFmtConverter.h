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

// Important, when adding new pixel formats, they need to be added in LAVPixFmtConverter.cpp as well to the format descriptors// Important, when adding new pixel formats, they need to be added in LAVPixFmtConverter.cpp as well to the format descriptors
typedef struct {
  GUID subtype;
  int bpp;
  int planes;
  int planeHeight[4];
  int planeWidth[4];
} LAVPixFmtDesc;

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
  CMediaType GetMediaType(int index, LONG biWidth, LONG biHeight, DWORD dwAspectX, DWORD dwAspectY, REFERENCE_TIME rtAvgTime);
  BOOL IsAllowedSubtype(const GUID *guid);

  HRESULT Convert(AVFrame *pFrame, BYTE *pOut, int width, int height, int dstStride);

private:
  int GetFilteredFormatCount();
  LAVVideoPixFmts GetFilteredFormat(int index);

  HRESULT swscale_scale(enum PixelFormat srcPix, enum PixelFormat dstPix, AVFrame *pFrame, BYTE *pOut, int width, int height, int dstStride, LAVPixFmtDesc pixFmtDesc, bool swapPlanes12 = false);
  HRESULT ConvertToAYUV(AVFrame *pFrame, BYTE *pOut, int width, int height, int stride);
  HRESULT ConvertToPX1X(AVFrame *pFrame, BYTE *pOut, int width, int height, int stride, int chromaVertical);
  HRESULT ConvertToY410(AVFrame *pFrame, BYTE *pOut, int width, int height, int stride);
  HRESULT ConvertToY416(AVFrame *pFrame, BYTE *pOut, int width, int height, int stride);

  void DestroySWScale() { if (m_pSwsContext) sws_freeContext(m_pSwsContext); m_pSwsContext = NULL; };
  SwsContext *GetSWSContext(int width, int height, enum PixelFormat srcPix, enum PixelFormat dstPix, int flags);

private:
  enum PixelFormat     m_InputPixFmt;
  enum LAVVideoPixFmts m_OutputPixFmt;

  int swsWidth, swsHeight;
  AVColorSpace swsColorSpace;
  AVColorRange swsColorRange;

  SwsContext *m_pSwsContext;

  ILAVVideoSettings *m_pSettings;
};
