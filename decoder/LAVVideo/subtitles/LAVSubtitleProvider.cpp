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
 */

#include "stdafx.h"
#include "LAVSubtitleProvider.h"

#define OFFSET(x) offsetof(LAVSubtitleProviderContext, x)
static const SubRenderOption options[] = {
  { "name", OFFSET(name), SROPT_TYPE_STRING, SROPT_FLAG_READONLY },
  { "frameRate", OFFSET(avgTimePerFrame), SROPT_TYPE_ULONGLONG, 0 },
  { 0 }
};

CLAVSubtitleProvider::CLAVSubtitleProvider(void)
  : CSubRenderOptionsImpl(::options, &context)
  , CUnknown(L"CLAVSubtitleProvider", NULL)
{
  context.name = L"LAV Video";
  context.avgTimePerFrame = 666666;
}

CLAVSubtitleProvider::~CLAVSubtitleProvider(void)
{
}
