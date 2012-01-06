/*
 *      Copyright (C) 2010-2012 Hendrik Leppkes
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

#pragma once

#include <string>
#include "StreamInfo.h"

class CLAVFStreamInfo : public CStreamInfo
{
public:
  CLAVFStreamInfo(AVFormatContext *avctx, AVStream *avstream, const char* containerFormat, HRESULT &hr);
  ~CLAVFStreamInfo();

  STDMETHODIMP CreateAudioMediaType(AVFormatContext *avctx, AVStream *avstream);
  STDMETHODIMP CreateVideoMediaType(AVFormatContext *avctx, AVStream *avstream);
  STDMETHODIMP CreateSubtitleMediaType(AVFormatContext *avctx, AVStream *avstream);

private:
  std::string m_containerFormat;
};
