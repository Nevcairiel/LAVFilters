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
#include "ILAVDecoder.h"

static LAVPixFmtDesc lav_pixfmt_desc[] = {
  { 1, 3, { 1, 2, 2 }, { 1, 2, 2 } },       ///< LAVPixFmt_YUV420
  { 2, 3, { 1, 2, 2 }, { 1, 2, 2 } },       ///< LAVPixFmt_YUV420bX
  { 1, 3, { 1, 2, 2 }, { 1, 1, 1 } },       ///< LAVPixFmt_YUV422
  { 2, 3, { 1, 2, 2 }, { 1, 1, 1 } },       ///< LAVPixFmt_YUV422bX
  { 1, 3, { 1, 1, 1 }, { 1, 1, 1 } },       ///< LAVPixFmt_YUV444
  { 2, 3, { 1, 1, 1 }, { 1, 1, 1 } },       ///< LAVPixFmt_YUV444bX
  { 1, 2, { 1, 1 },    { 1, 2 }    },       ///< LAVPixFmt_NV12
  { 2, 1, { 1 },       { 1 }       },       ///< LAVPixFmt_YUY2
  { 3, 1, { 1 },       { 1 }       },       ///< LAVPixFmt_RGB24
  { 4, 1, { 1 },       { 1 }       },       ///< LAVPixFmt_RGB32
  { 4, 1, { 1 },       { 1 }       },       ///< LAVPixFmt_ARGB32
};

LAVPixFmtDesc getPixelFormatDesc(LAVPixelFormat pixFmt)
{
  return lav_pixfmt_desc[pixFmt];
}
