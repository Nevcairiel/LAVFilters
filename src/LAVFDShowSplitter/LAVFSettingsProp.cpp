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

#include "LAVFSettingsProp.h"

#include <Commctrl.h>

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
  if(m_pszPrefLang)
    CoTaskMemFree(m_pszPrefLang);
  if(m_pszPrefSubLang)
    CoTaskMemFree(m_pszPrefSubLang);
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

  WCHAR buffer[LANG_BUFFER_SIZE];
  // Save audio language
  SendDlgItemMessage(m_Dlg, IDC_PREF_LANG, WM_GETTEXT, LANG_BUFFER_SIZE, (LPARAM)&buffer);
  CHECK_HR(hr = m_pLAVF->SetPreferredLanguages(buffer));

  // Save subtitle language
  SendDlgItemMessage(m_Dlg, IDC_PREF_LANG_SUBS, WM_GETTEXT, LANG_BUFFER_SIZE, (LPARAM)&buffer);
  CHECK_HR(hr = m_pLAVF->SetPreferredSubtitleLanguages(buffer));

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
  // Query current settings
  CHECK_HR(hr = m_pLAVF->GetPreferredLanguages(&m_pszPrefLang));
  CHECK_HR(hr = m_pLAVF->GetPreferredSubtitleLanguages(&m_pszPrefSubLang));

  // Notify the UI about those settings
  SendDlgItemMessage(m_Dlg, IDC_PREF_LANG, WM_SETTEXT, 0, (LPARAM)m_pszPrefLang);
  SendDlgItemMessage(m_Dlg, IDC_PREF_LANG_SUBS, WM_SETTEXT, 0, (LPARAM)m_pszPrefSubLang);

done:
  return hr;
}

BOOL CLAVFSettingsProp::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_COMMAND:
    // Mark the page dirty if the text changed
    if (IsPageDirty() != S_OK && HIWORD(wParam) == EN_CHANGE
      && (LOWORD(wParam) == IDC_PREF_LANG || LOWORD(wParam) == IDC_PREF_LANG_SUBS)) {
      
      WCHAR buffer[LANG_BUFFER_SIZE];
      SendDlgItemMessage(m_Dlg, LOWORD(wParam), WM_GETTEXT, LANG_BUFFER_SIZE, (LPARAM)&buffer);

      int dirty = 0;
      if(LOWORD(wParam) == IDC_PREF_LANG) {
        dirty = _wcsicmp(buffer, m_pszPrefLang);
      } else {
        dirty = _wcsicmp(buffer, m_pszPrefSubLang);
      }
      
      if(dirty != 0) {
        SetDirty();
      }
    }
    break;
  }
  // Let the parent class handle the message.
  return CBasePropertyPage::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}
