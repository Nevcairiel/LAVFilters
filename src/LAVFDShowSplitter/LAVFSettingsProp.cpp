/*
 *      Copyright (C) 2010 Hendrik Leppkes
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

#include "LAVFSettingsProp.h"

#include "DShowUtil.h"


#define LANG_BUFFER_SIZE 256

// static constructor
CUnknown* WINAPI CLAVFSettingsProp::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr)
{
  CLAVFSettingsProp *propPage = new CLAVFSettingsProp(pUnk);
  if (!propPage) {
    *phr = E_OUTOFMEMORY;
  }
  return propPage;
}

CLAVFSettingsProp::CLAVFSettingsProp(IUnknown *pUnk)
  : CBasePropertyPage(NAME("LAVF Settings"), pUnk, IDD_PROPPAGE_LAVFSETTINGS, IDS_PAGE_TITLE)
  , m_pLAVF(NULL), m_pszPrefLang(NULL), m_pszPrefSubLang(NULL)
{
}


CLAVFSettingsProp::~CLAVFSettingsProp(void)
{
  SAFE_CO_FREE(m_pszPrefLang);
  SAFE_CO_FREE(m_pszPrefSubLang);
}

HRESULT CLAVFSettingsProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == NULL)
  {
    return E_POINTER;
  }
  ASSERT(m_pLAVF == NULL);
  return pUnk->QueryInterface(&m_pLAVF);
}

HRESULT CLAVFSettingsProp::OnDisconnect()
{
  if (m_pLAVF) {
    m_pLAVF->Release();
    m_pLAVF = NULL;
  }
  return S_OK;
}

HRESULT CLAVFSettingsProp::OnApplyChanges()
{
  ASSERT(m_pLAVF != NULL);
  HRESULT hr = S_OK;
  DWORD dwVal;

  WCHAR buffer[LANG_BUFFER_SIZE];
  // Save audio language
  SendDlgItemMessage(m_Dlg, IDC_PREF_LANG, WM_GETTEXT, LANG_BUFFER_SIZE, (LPARAM)&buffer);
  CHECK_HR(hr = m_pLAVF->SetPreferredLanguages(buffer));

  // Save subtitle language
  SendDlgItemMessage(m_Dlg, IDC_PREF_LANG_SUBS, WM_GETTEXT, LANG_BUFFER_SIZE, (LPARAM)&buffer);
  CHECK_HR(hr = m_pLAVF->SetPreferredSubtitleLanguages(buffer));

  // Save subtitle mode
  dwVal = SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_GETCURSEL, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetSubtitleMode(dwVal));

  BOOL flag = SendDlgItemMessage(m_Dlg, IDC_SUBMODE_ONLY_MATCHING, BM_GETCHECK, 0, 0);
  CHECK_HR(hr = m_pLAVF->SetSubtitleMatchingLanguage(flag));

  LoadData();

done:    
  return hr;
} 

HRESULT CLAVFSettingsProp::OnActivate()
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

  SendDlgItemMessage(m_Dlg, IDC_SUBMODE_ONLY_MATCHING, BM_SETCHECK, m_subtitleMatching, 0);

  return hr;
}
HRESULT CLAVFSettingsProp::LoadData()
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

done:
  return hr;
}

BOOL CLAVFSettingsProp::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
          dirty = wcslen(buffer);
        }

        if(dirty != 0) {
          SetDirty();
        }
      } else if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_SUBTITLE_MODE) {
        DWORD dwVal = SendDlgItemMessage(m_Dlg, IDC_SUBTITLE_MODE, CB_GETCURSEL, 0, 0);
        if (dwVal != m_subtitleMode) {
          SetDirty();
        }
      } else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_SUBMODE_ONLY_MATCHING) {
          BOOL bFlag = SendDlgItemMessage(m_Dlg, IDC_SUBMODE_ONLY_MATCHING, BM_GETCHECK, 0, 0);
          if (bFlag != m_subtitleMatching) {
            SetDirty();
          }
        }
      }
    break;
  }
  // Let the parent class handle the message.
  return CBasePropertyPage::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}
