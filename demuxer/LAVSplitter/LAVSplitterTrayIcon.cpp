/*
 *      Copyright (C) 2010-2019 Hendrik Leppkes
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

#include <Qnetwork.h>

#define STREAM_CMD_OFFSET 100
#define CHAPTER_CMD_OFFSET 500

CLAVSplitterTrayIcon::CLAVSplitterTrayIcon(IBaseFilter *pFilter, const WCHAR *wszName, int resIcon)
  : CBaseTrayIcon(pFilter, wszName, resIcon)
{
}


CLAVSplitterTrayIcon::~CLAVSplitterTrayIcon(void)
{
}

HMENU CLAVSplitterTrayIcon::GetPopupMenu()
{
  CheckPointer(m_pFilter, nullptr);
  CPopupMenu menu;

  IAMStreamSelect *pStreamSelect = nullptr;
  if (SUCCEEDED(m_pFilter->QueryInterface(&pStreamSelect))) {
    DWORD dwStreams = 0;
    if (FAILED(pStreamSelect->Count(&dwStreams)))
      dwStreams = 0;
    DWORD dwLastGroup = DWORD_MAX;
    for (DWORD i = 0; i < dwStreams; i++) {
      DWORD dwFlags = 0, dwGroup = 0;
      LPWSTR pszName = nullptr;
      if (FAILED(pStreamSelect->Info(i, nullptr, &dwFlags, nullptr, &dwGroup, &pszName, nullptr, nullptr)))
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

  // Chapters
  IAMExtendedSeeking *pExSeeking = nullptr;
  if (SUCCEEDED(m_pFilter->QueryInterface(IID_IAMExtendedSeeking, (void **)&pExSeeking))) {
    long count = 0, current = 0;
    if (FAILED(pExSeeking->get_MarkerCount(&count)))
      count = 0;

    if (FAILED(pExSeeking->get_CurrentMarker(&current)))
      current = 0;

    CPopupMenu chapters;
    for (long i = 1; i <= count; i++) {
      double markerTime;
      if (FAILED(pExSeeking->GetMarkerTime(i, &markerTime)))
        continue;

      BSTR bstrName = nullptr;
      if (FAILED(pExSeeking->GetMarkerName(i, &bstrName)))
        continue;

      // Create compound chapter name
      int total_seconds = (int)(markerTime + 0.5);
      int seconds = total_seconds % 60;
      int minutes = total_seconds / 60 % 60;
      int hours   = total_seconds / 3600;
      WCHAR chapterName[512];
      _snwprintf_s(chapterName, _TRUNCATE, L"%s\t[%02d:%02d:%02d]", bstrName, hours, minutes, seconds);

      // Sanitize any tab chars in the chapter name (replace by space)
      // More then one tab in the string messes with the popup menu rendering
      WCHAR *nextMatch, *tabMatch = wcsstr(chapterName, L"\t");
      while (nextMatch = wcsstr(tabMatch+1, L"\t")) {
        *tabMatch = L' ';
        tabMatch = nextMatch;
      }

      chapters.AddItem(CHAPTER_CMD_OFFSET + i, chapterName, i == current);
      SysFreeString(bstrName);
    }
    if (count) {
      menu.AddSubmenu(chapters.Finish(), L"Chapters");
      menu.AddSeparator();
    }
    m_NumChapters = count;
    SafeRelease(&pExSeeking);
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
    IAMStreamSelect *pStreamSelect = nullptr;
    if (SUCCEEDED(m_pFilter->QueryInterface(&pStreamSelect))) {
      pStreamSelect->Enable(cmd - STREAM_CMD_OFFSET, AMSTREAMSELECTENABLE_ENABLE);
      SafeRelease(&pStreamSelect);
    }
  } else if (cmd > CHAPTER_CMD_OFFSET && cmd <= m_NumChapters + CHAPTER_CMD_OFFSET) {
    IAMExtendedSeeking *pExSeeking = nullptr;
    if (SUCCEEDED(m_pFilter->QueryInterface(IID_IAMExtendedSeeking, (void **)&pExSeeking))) {
      double markerTime;
      if (FAILED(pExSeeking->GetMarkerTime(cmd - CHAPTER_CMD_OFFSET, &markerTime)))
        goto failchapterseek;

      REFERENCE_TIME rtMarkerTime = (REFERENCE_TIME)(markerTime * 10000000.0);

      // Try to get the graph to seek on, its much safer than directly trying to seek on LAV
      FILTER_INFO info;
      if (FAILED(m_pFilter->QueryFilterInfo(&info)) || !info.pGraph)
        goto failchapterseek;

      IMediaSeeking *pSeeking = nullptr;
      if (SUCCEEDED(info.pGraph->QueryInterface(&pSeeking))) {
        pSeeking->SetPositions(&rtMarkerTime, AM_SEEKING_AbsolutePositioning, nullptr, AM_SEEKING_NoPositioning);
        SafeRelease(&pSeeking);
      }
      SafeRelease(&info.pGraph);

failchapterseek:
      SafeRelease(&pExSeeking);
    }
  } else if (cmd == STREAM_CMD_OFFSET - 1) {
    OpenPropPage();
  } else {
    return E_UNEXPECTED;
  }
  return S_OK;
}
