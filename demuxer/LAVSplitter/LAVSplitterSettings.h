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

#include <Unknwn.h>       // IUnknown and GUID Macros

// GUID: 72b2c5fa-a7a5-4463-9c1b-9f4749c35c79
DEFINE_GUID(IID_ILAVFSettings, 0x72b2c5fa, 0xa7a5, 
0x4463, 0x9c, 0x1b, 0x9f, 0x47, 0x49, 0xc3, 0x5c, 0x79);

[uuid("72b2c5fa-a7a5-4463-9c1b-9f4749c35c79")]
interface ILAVFSettings : public IUnknown
{
  // Retrieve the preferred languages as ISO 639-2 language codes, comma seperated
  // If the result is NULL, no language has been set
  // Memory for the string will be allocated, and has to be free'ed by the caller with CoTaskMemFree
  STDMETHOD(GetPreferredLanguages)(WCHAR **ppLanguages) = 0;

  // Set the preferred languages as ISO 639-2 language codes, comma seperated
  // To reset to no preferred language, pass NULL or the empty string
  STDMETHOD(SetPreferredLanguages)(WCHAR *pLanguages) = 0;
  
  // Retrieve the preferred subtitle languages as ISO 639-2 language codes, comma seperated
  // If the result is NULL, no language has been set
  // If no subtitle language is set, the main language preference is used.
  // Memory for the string will be allocated, and has to be free'ed by the caller with CoTaskMemFree
  STDMETHOD(GetPreferredSubtitleLanguages)(WCHAR **ppLanguages) = 0;

  // Set the preferred subtitle languages as ISO 639-2 language codes, comma seperated
  // To reset to no preferred language, pass NULL or the empty string
  // If no subtitle language is set, the main language preference is used.
  STDMETHOD(SetPreferredSubtitleLanguages)(WCHAR *pLanguages) = 0;

  // Get the current subtitle mode
  // 0 = No Subs; 1 = Forced Subs; 2 = All subs
  STDMETHOD_(DWORD,GetSubtitleMode)() = 0;

  // Set the current subtitle mode
  // 0 = No Subs; 1 = Forced Subs; 2 = All subs
  STDMETHOD(SetSubtitleMode)(DWORD dwMode) = 0;

  // Get the subtitle matching language flag
  // TRUE = Only subtitles with a language in the preferred list will be used; FALSE = All subtitles will be used
  STDMETHOD_(BOOL,GetSubtitleMatchingLanguage)() = 0;

  // Set the subtitle matching language flag
  // TRUE = Only subtitles with a language in the preferred list will be used; FALSE = All subtitles will be used
  STDMETHOD(SetSubtitleMatchingLanguage)(BOOL dwMode) = 0;
};
