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

#include <assert.h>
#include <Commctrl.h>

#include "SettingsProp.h"

#include "DShowUtil.h"
#include "version.h"


#define LANG_BUFFER_SIZE 256

// static constructor
CUnknown* WINAPI CLAVSplitterSettingsProp::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr)
{
  CLAVSplitterSettingsProp *propPage = new CLAVSplitterSettingsProp(pUnk);
  if (!propPage) {
    *phr = E_OUTOFMEMORY;
  }
  return propPage;
}

CLAVSplitterSettingsProp::CLAVSplitterSettingsProp(IUnknown *pUnk)
  : CBaseDSPropPage(NAME("LAVF Settings"), pUnk, IDD_PROPPAGE_LAVFSETTINGS, IDS_PAGE_TITLE)
  , m_pLAVF(NULL), m_pszPrefLang(NULL), m_pszPrefSubLang(NULL), m_videoParsing(TRUE), m_audioParsing(TRUE)
{
}


CLAVSplitterSettingsProp::~CLAVSplitterSettingsProp(void)
{
  SAFE_CO_FREE(m_pszPrefLang);
  SAFE_CO_FREE(m_pszPrefSubLang);
}

HRESULT CLAVSplitterSettingsProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == NULL)
  {
    return E_POINTER;
  }
  ASSERT(m_pLAVF == NULL);
  return pUnk->QueryInterface(&m_pLAVF);
}

HRESULT CLAVSplitterSettingsProp::OnDisconnect()
{
  if (m_pLAVF) {
    m_pLAVF->Release();
    m_pLAVF = NULL;
  }
  return S_OK;
}

HRESULT CLAVSplitterSettingsProp::OnApplyChanges()
{
  ASSERT(m_pLAVF != NULL);
  HRESULT hr = S_OK;
  DWORD dwVal;
  BOOL bFlag;

  WCHAR buffer[LANG_BUFFER_SIZE];
  // Save audio language
  SendDlgItemMessage(m_Dlg, IDC_PREF_LANG, WM_GETTEXT, LANG_BUFFER_SIZE, (LPARAM)&buffer);
  CHECK_HR(hr = m_pLAVF->SetPreferredLanguages(buffer));

  // Save subtitle language
  SendDlgItemMessage(m_Dlg, IDC_PREF_LANG_SUBS, WM_GETTEXT, LANG_BUFFER_SIZE, (LPARAM)&buffer);
  CHECK_HR(hr = m_pLAVF->SetPreferredSubtitleLanguages(buffer));

  // Save subtitle mode
  dwVal = (DWORD)SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_GETCURSEL, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetSubtitleMode(dwVal));

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_SUBMODE_ONLY_MATCHING, BM_GETCHECK, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetSubtitleMatchingLanguage(bFlag));

  int vc1flag = (int)SendDlgItemMessage(m_Dlg, IDC_VC1TIMESTAMP, BM_GETCHECK, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetVC1TimestampMode(vc1flag));

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_SUBSTREAMS, BM_GETCHECK, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetSubstreamsEnabled(bFlag));

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_VIDEOPARSING, BM_GETCHECK, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetVideoParsingEnabled(bFlag));

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_AUDIOPARSING, BM_GETCHECK, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetAudioParsingEnabled(bFlag));

  LoadData();

done:    
  return hr;
} 

HRESULT CLAVSplitterSettingsProp::OnActivate()
{
  HRESULT hr = S_OK;
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
  if (InitCommonControlsEx(&icc) == FALSE)
  {
    return E_FAIL;
  }
  ASSERT(m_pLAVF != NULL);

  const WCHAR *version = TEXT(LAV_SPLITTER) L" " TEXT(LAV_VERSION_STR);
  SendDlgItemMessage(m_Dlg, IDC_SPLITTER_FOOTER, WM_SETTEXT, 0, (LPARAM)version);

  hr = LoadData();

  // Notify the UI about those settings
  SendDlgItemMessage(m_Dlg, IDC_PREF_LANG, WM_SETTEXT, 0, (LPARAM)m_pszPrefLang);
  SendDlgItemMessage(m_Dlg, IDC_PREF_LANG_SUBS, WM_SETTEXT, 0, (LPARAM)m_pszPrefSubLang);

  // Init the Combo Box
  SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_RESETCONTENT, 0, 0);
  WideStringFromResource(stringBuffer, IDS_SUBMODE_NO_SUBS);
  SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_ADDSTRING, 0, (LPARAM)stringBuffer);
  WideStringFromResource(stringBuffer, IDS_SUBMODE_FORCED_SUBS);
  SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_ADDSTRING, 0, (LPARAM)stringBuffer);
  WideStringFromResource(stringBuffer, IDS_SUBMODE_ALL_SUBS);
  SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_ADDSTRING, 0, (LPARAM)stringBuffer);

  SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_SETCURSEL, m_subtitleMode, 0);
  addHint(IDC_SUBTITLE_MODE, L"Configure which kinds of subtitles should be enabled by default.");

  SendDlgItemMessage(m_Dlg, IDC_SUBMODE_ONLY_MATCHING, BM_SETCHECK, m_subtitleMatching, 0);
  addHint(IDC_SUBMODE_ONLY_MATCHING, L"If set, subtitles will only be enabled if they match one of the languages configured above, otherwise any subtitle will be enabled if no match was found.");

  SendDlgItemMessage(m_Dlg, IDC_VC1TIMESTAMP, BM_SETCHECK, m_VC1Mode, 0);
  addHint(IDC_VC1TIMESTAMP, L"Checked - Frame timings will be corrected.\nUnchecked - Frame timings will be sent untouched.\nIndeterminate (Auto) - Enabled, except for decoders that do their own correction.");

  SendDlgItemMessage(m_Dlg, IDC_SUBSTREAMS, BM_SETCHECK, m_substreams, 0);
  addHint(IDC_SUBSTREAMS, L"Controls if sub-streams should be exposed as a separate stream.\nSub-streams are typically streams for backwards compatibility, for example the AC3 part of TrueHD streams on BluRays");

  SendDlgItemMessage(m_Dlg, IDC_VIDEOPARSING, BM_SETCHECK, m_videoParsing, 0);
  addHint(IDC_VIDEOPARSING, L"Enables parsing and repacking of video streams.\n\nSome decoders might not work with this disabled, however in conjunction with some renderers, you might get more fluid playback if this is off.");

  SendDlgItemMessage(m_Dlg, IDC_AUDIOPARSING, BM_SETCHECK, m_audioParsing, 0);
  addHint(IDC_AUDIOPARSING, L"Enables parsing and repacking of audio streams.\n\nIts not recommended to turn this off.");

  return hr;
}
HRESULT CLAVSplitterSettingsProp::LoadData()
{
  HRESULT hr = S_OK;

  // Free old strings
  SAFE_CO_FREE(m_pszPrefLang);
  SAFE_CO_FREE(m_pszPrefSubLang);

  // Query current settings
  CHECK_HR(hr = m_pLAVF->GetPreferredLanguages(&m_pszPrefLang));
  CHECK_HR(hr = m_pLAVF->GetPreferredSubtitleLanguages(&m_pszPrefSubLang));
  m_subtitleMode = m_pLAVF->GetSubtitleMode();
  m_subtitleMatching = m_pLAVF->GetSubtitleMatchingLanguage();
  m_VC1Mode = m_pLAVF->GetVC1TimestampMode();
  m_substreams = m_pLAVF->GetSubstreamsEnabled();

  m_videoParsing = m_pLAVF->GetVideoParsingEnabled();
  m_audioParsing = m_pLAVF->GetAudioParsingEnabled();

done:
  return hr;
}

INT_PTR CLAVSplitterSettingsProp::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_COMMAND:
    // Mark the page dirty if the text changed
    if (IsPageDirty() != S_OK) {
      if(HIWORD(wParam) == EN_CHANGE
        && (LOWORD(wParam) == IDC_PREF_LANG || LOWORD(wParam) == IDC_PREF_LANG_SUBS)) {

          WCHAR buffer[LANG_BUFFER_SIZE];
          SendDlgItemMessage(m_Dlg, LOWORD(wParam), WM_GETTEXT, LANG_BUFFER_SIZE, (LPARAM)&buffer);

          int dirty = 0;
          WCHAR *source = NULL;
          if(LOWORD(wParam) == IDC_PREF_LANG) {
            source = m_pszPrefLang;
          } else {
            source = m_pszPrefSubLang;
          }

          if (source) {
            dirty = _wcsicmp(buffer, source);
          } else {
            dirty = (int)wcslen(buffer);
          }

          if(dirty != 0) {
            SetDirty();
          }
      } else if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_SUBTITLE_MODE) {
        DWORD dwVal = (DWORD)SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_GETCURSEL, 0, 0);
        if (dwVal != m_subtitleMode) {
          SetDirty();
        }
      } else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_SUBMODE_ONLY_MATCHING) {
        BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_SUBMODE_ONLY_MATCHING, BM_GETCHECK, 0, 0);
        if (bFlag != m_subtitleMatching) {
          SetDirty();
        }
      } else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_VC1TIMESTAMP) {
        int iFlag = (int)SendDlgItemMessage(m_Dlg, IDC_VC1TIMESTAMP, BM_GETCHECK, 0, 0);
        if (iFlag != m_VC1Mode) {
          SetDirty();
        }
      } else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_SUBSTREAMS) {
        BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_SUBSTREAMS, BM_GETCHECK, 0, 0);
        if (bFlag != m_substreams) {
          SetDirty();
        }
      } else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_VIDEOPARSING) {
        BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_VIDEOPARSING, BM_GETCHECK, 0, 0);
        if (bFlag != m_videoParsing) {
          SetDirty();
        }
      } else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_AUDIOPARSING) {
        BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_AUDIOPARSING, BM_GETCHECK, 0, 0);
        if (bFlag != m_audioParsing) {
          SetDirty();
        }
      }
    }
    break;
  }
  // Let the parent class handle the message.
  return __super::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}
