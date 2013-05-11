/*
 *      Copyright (C) 2010-2013 Hendrik Leppkes
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
#include "LAVSplitterTrayIcon.h"
#include "PopupMenu.h"

#define STREAM_CMD_OFFSET 100

CLAVSplitterTrayIcon::CLAVSplitterTrayIcon(IBaseFilter *pFilter, const WCHAR *wszName, int resIcon)
  : CBaseTrayIcon(pFilter, wszName, resIcon)
  , m_NumStreams(0)
{
}


CLAVSplitterTrayIcon::~CLAVSplitterTrayIcon(void)
{
}

HMENU CLAVSplitterTrayIcon::GetPopupMenu()
{
  CheckPointer(m_pFilter, NULL);
  HRESULT hr = S_OK;
  CPopupMenu menu;

  IAMStreamSelect *pStreamSelect = NULL;
  if (SUCCEEDED(m_pFilter->QueryInterface(&pStreamSelect))) {
    DWORD dwStreams = 0;
    if (FAILED(pStreamSelect->Count(&dwStreams)))
      dwStreams = 0;
    DWORD dwLastGroup = DWORD_MAX;
    for (DWORD i = 0; i < dwStreams; i++) {
      DWORD dwFlags = 0, dwGroup = 0;
      LPWSTR pszName = NULL;
      if (FAILED(pStreamSelect->Info(i, NULL, &dwFlags, NULL, &dwGroup, &pszName, NULL, NULL)))
        continue;
      
      if (dwGroup != dwLastGroup) {
        switch (dwGroup) {
        case 0:
          menu.AddItem(dwGroup, L"Video", FALSE, FALSE);
          break;
        case 1:
          menu.AddItem(dwGroup, L"Audio", FALSE, FALSE);
          break;
        case 2:
          menu.AddItem(dwGroup, L"Subtitles", FALSE, FALSE);
          break;
        case 18:
          menu.AddItem(dwGroup, L"Editions", FALSE, FALSE);
          break;
        default:
          menu.AddSeparator();
          break;
        }
        dwLastGroup = dwGroup;
      }
      menu.AddItem(STREAM_CMD_OFFSET + i, pszName, dwFlags & AMSTREAMSELECTINFO_ENABLED);
      CoTaskMemFree(pszName);
    }
    if (dwStreams)
      menu.AddSeparator();
    m_NumStreams = dwStreams;
    SafeRelease(&pStreamSelect);
  }
  menu.AddItem(STREAM_CMD_OFFSET - 1, L"Properties");

  HMENU hMenu = menu.Finish();
  return hMenu;
}

HRESULT CLAVSplitterTrayIcon::ProcessMenuCommand(HMENU hMenu, int cmd)
{
  CheckPointer(m_pFilter, E_FAIL);
  DbgLog((LOG_TRACE, 10, L"Menu Command %d", cmd));
  if (cmd >= STREAM_CMD_OFFSET && cmd < m_NumStreams + STREAM_CMD_OFFSET) {
    IAMStreamSelect *pStreamSelect = NULL;
    if (SUCCEEDED(m_pFilter->QueryInterface(&pStreamSelect))) {
      pStreamSelect->Enable(cmd - STREAM_CMD_OFFSET, AMSTREAMSELECTENABLE_ENABLE);
      SafeRelease(&pStreamSelect);
    }
    return S_OK;
  } else if (cmd == STREAM_CMD_OFFSET - 1) {
    OpenPropPage();
  }
  return E_UNEXPECTED;
}
