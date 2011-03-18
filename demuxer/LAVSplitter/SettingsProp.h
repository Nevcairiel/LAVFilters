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

#include "cprop.h"
#include "LAVSplitterSettings.h"

#include <Commctrl.h>

// GUID: a19de2f2-2f74-4927-8436-61129d26c141
DEFINE_GUID(CLSID_LAVSplitterSettingsProp, 0xa19de2f2, 0x2f74, 
  0x4927, 0x84, 0x36, 0x61, 0x12, 0x9d, 0x26, 0xc1, 0x41);

class CLAVSplitterSettingsProp :
  public CBasePropertyPage
{
public:
  static CUnknown* WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT* phr);

  virtual ~CLAVSplitterSettingsProp(void);

  HRESULT OnActivate();
  HRESULT OnConnect(IUnknown *pUnk);
  HRESULT OnDisconnect();
  HRESULT OnApplyChanges();
  INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
  CLAVSplitterSettingsProp(IUnknown *pUnk);

  HRESULT LoadData();

  void SetDirty()
  {
    m_bDirty = TRUE;
    if (m_pPageSite)
    {
      m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
    }
  }

  HWND createHintWindow(HWND parent, int timePop = 1700, int timeInit = 70, int timeReshow = 7);
  TOOLINFO addHint(int id, const LPWSTR text);

private:
  ILAVFSettings *m_pLAVF;

  // Settings
  WCHAR *m_pszPrefLang;
  WCHAR *m_pszPrefSubLang;

  DWORD m_subtitleMode;
  BOOL m_subtitleMatching;
  int m_VC1Mode;
  BOOL m_substreams;

  WCHAR stringBuffer[256];

  HWND m_hHint;
};
