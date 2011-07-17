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

typedef struct {
  DWORD fourcc;
  int bpp;
  int planes;
  int planeHeight[4];
  int planeWidth[4];
} LAVPixFmtDesc;

// Important, when adding new pixel formats, they need to be added in LAVPixFmtConverter.cpp as well to the format descriptors
typedef enum LAVVideoPixFmts {
  LAVPixFmt_None = -1,
  LAVPixFmt_YV12,            // 4:2:0, 8bit, planar
  LAVPixFmt_NV12,            // 4:2:0, 8bit, Y planar, U/V packed
  LAVPixFmt_YUY2,            // 4:2:2, 8bit, packed
  LAVPixFmt_AYUV,            // 4:4:4, 8bit, packed
  LAVPixFmt_P010,            // 4:2:0, 10bit, Y planar, U/V packed
  LAVPixFmt_P210,            // 4:2:2, 10bit, Y planar, U/V packed 
  LAVPixFmt_Y410,            // 4:4:4, 10bit, packed
  LAVPixFmt_P016,            // 4:2:0, 16bit, Y planar, U/V packed
  LAVPixFmt_P216,            // 4:2:2, 16bit, Y planar, U/V packed
  LAVPixFmt_Y416,            // 4:4:4, 16bit, packed
  
  
  LAVPixFmt_NB               // Number of formats
};

class CLAVPixFmtConverter
{
public:
  CLAVPixFmtConverter();
  ~CLAVPixFmtConverter();

  HRESULT SetInputPixFmt(enum PixelFormat pix_fmt) { m_InputPixFmt = pix_fmt; return S_OK; }
  HRESULT SetOutputPixFmt(enum LAVVideoPixFmts pix_fmt) { m_OutputPixFmt = pix_fmt; return S_OK; }
  
  LAVVideoPixFmts GetOutputBySubtype(const GUID *guid);
  LAVVideoPixFmts GetPreferredOutput();

  PixelFormat GetInputPixFmt() { return m_InputPixFmt; }

  int GetNumMediaTypes();
  CMediaType GetMediaType(int index, LONG biWidth, LONG biHeight, DWORD dwAspectX, DWORD dwAspectY, REFERENCE_TIME rtAvgTime);

  HRESULT Convert(AVFrame *pFrame, BYTE *pOut, int width, int height, int dstStride);

private:
  HRESULT swscale_scale(enum PixelFormat srcPix, enum PixelFormat dstPix, AVFrame *pFrame, BYTE *pOut, int width, int height, int dstStride, LAVPixFmtDesc pixFmtDesc, bool swapPlanes12 = false);
  HRESULT ConvertToAYUV(AVFrame *pFrame, BYTE *pOut, int width, int height, int stride);
  HRESULT ConvertToPX1X(AVFrame *pFrame, BYTE *pOut, int width, int height, int stride, int chromaVertical);

private:
  enum PixelFormat     m_InputPixFmt;
  enum LAVVideoPixFmts m_OutputPixFmt;

  SwsContext *m_pSwsContext;
};
