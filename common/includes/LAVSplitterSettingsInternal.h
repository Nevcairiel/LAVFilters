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

#pragma once

#include "LAVSplitterSettings.h"
#include <set>

class FormatInfo {
public:
  FormatInfo() : strName(NULL), strDescription(NULL) {}
  FormatInfo(const char *name, const char *desc) : strName(name), strDescription(desc) {}
  const char *strName;
  const char *strDescription;

  // Comparison operators for sorting (NULL safe)
  bool FormatInfo::operator < (const FormatInfo& rhs) const { return strName ? (rhs.strName ? _stricmp(strName, rhs.strName) < 0 : false) : true; }
  bool FormatInfo::operator > (const FormatInfo& rhs) const { return !(*this < rhs); }
  bool FormatInfo::operator == (const FormatInfo& rhs) const { return (strName == rhs.strName) || (strName && rhs.strName && (_stricmp(strName, rhs.strName) == 0)); }
};

// GUID: 72b2c5fa-a7a5-4463-9c1b-9f4749c35c79
DEFINE_GUID(IID_ILAVFSettingsInternal, 0x72b2c5fa, 0xa7a5, 
0x4463, 0x9c, 0x1b, 0x9f, 0x47, 0x49, 0xc3, 0x5c, 0x79);

[uuid("72b2c5fa-a7a5-4463-9c1b-9f4749c35c79")]
interface ILAVFSettingsInternal : public ILAVFSettings
{
  // Query if the current filter graph requires VC1 Correction
  STDMETHOD_(BOOL,IsVC1CorrectionRequired)() = 0;

  STDMETHOD_(const char*, GetInputFormat)() = 0;
  STDMETHOD_(std::set<FormatInfo>&, GetInputFormats)() = 0;
};
