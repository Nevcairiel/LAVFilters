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
#include "VideoSettingsProp.h"
#include "Media.h"

#include <Commctrl.h>

#include "resource.h"
#include "version.h"

CLAVVideoSettingsProp::CLAVVideoSettingsProp(LPUNKNOWN pUnk, HRESULT* phr)
  : CBaseDSPropPage(NAME("LAVVideoProp"), pUnk, IDD_PROPPAGE_VIDEO_SETTINGS, IDS_SETTINGS), m_pVideoSettings(NULL)
{
}

CLAVVideoSettingsProp::~CLAVVideoSettingsProp()
{
}

HRESULT CLAVVideoSettingsProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == NULL)
  {
    return E_POINTER;
  }
  ASSERT(m_pVideoSettings == NULL);
  return pUnk->QueryInterface(&m_pVideoSettings);
}

HRESULT CLAVVideoSettingsProp::OnDisconnect()
{
  SafeRelease(&m_pVideoSettings);
  return S_OK;
}

HRESULT CLAVVideoSettingsProp::OnApplyChanges()
{
  ASSERT(m_pVideoSettings != NULL);
  HRESULT hr = S_OK;
  
  // TODO: Read fields, submit via API

  LoadData();

  return hr;
} 

HRESULT CLAVVideoSettingsProp::OnActivate()
{
  HRESULT hr = S_OK;
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
  if (InitCommonControlsEx(&icc) == FALSE)
  {
    return E_FAIL;
  }
  ASSERT(m_pVideoSettings != NULL);

  const WCHAR *version = TEXT(LAV_VIDEO) L" " TEXT(LAV_VERSION_STR);
  SendDlgItemMessage(m_Dlg, IDC_LAVVIDEO_FOOTER, WM_SETTEXT, 0, (LPARAM)version);

  hr = LoadData();
  if (SUCCEEDED(hr)) {
    // TODO: Fill Fields
  }

  return hr;
}

HRESULT CLAVVideoSettingsProp::LoadData()
{
  HRESULT hr = S_OK;
  
  // TODO: Read data from API, write into local vars

  return hr;
}

INT_PTR CLAVVideoSettingsProp::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  LRESULT lValue;
  switch (uMsg)
  {
  case WM_COMMAND:
    break;
  }
  // Let the parent class handle the message.
  return __super::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Format Configurations

CLAVVideoFormatsProp::CLAVVideoFormatsProp(LPUNKNOWN pUnk, HRESULT* phr)
  : CBaseDSPropPage(NAME("LAVVideoFormats"), pUnk, IDD_PROPPAGE_FORMATS, IDS_FORMATS), m_pVideoSettings(NULL)
{
}

CLAVVideoFormatsProp::~CLAVVideoFormatsProp()
{
}

HRESULT CLAVVideoFormatsProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == NULL)
  {
    return E_POINTER;
  }
  ASSERT(m_pVideoSettings == NULL);
  return pUnk->QueryInterface(&m_pVideoSettings);
}

HRESULT CLAVVideoFormatsProp::OnDisconnect()
{
  SafeRelease(&m_pVideoSettings);
  return S_OK;
}


HRESULT CLAVVideoFormatsProp::OnApplyChanges()
{
  ASSERT(m_pVideoSettings != NULL);
  HRESULT hr = S_OK;

  HWND hlv = GetDlgItem(m_Dlg, IDC_CODECS);

  // TODO: Read fields, submit via API

  LoadData();

  return hr;
}

HRESULT CLAVVideoFormatsProp::OnActivate()
{
  HRESULT hr = S_OK;
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
  if (InitCommonControlsEx(&icc) == FALSE)
  {
    return E_FAIL;
  }
  ASSERT(m_pVideoSettings != NULL);

  // Setup ListView control for format configuration
  HWND hlv = GetDlgItem(m_Dlg, IDC_CODECS);
  ListView_SetExtendedListViewStyle(hlv, LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);

  int nCol = 1;
  LVCOLUMN lvc = {LVCF_WIDTH, 0, 20, 0};
  ListView_InsertColumn(hlv, 0, &lvc);
  ListView_AddCol(hlv, nCol,  75, L"Codec", false);
  ListView_AddCol(hlv, nCol, 177, L"Description", false);

  ListView_DeleteAllItems(hlv);
  // TODO: ListView_SetItemCount(hlv, Codec_NB);

  // Create entrys for the formats
  LVITEM lvi;
  memset(&lvi, 0, sizeof(lvi));
  lvi.mask = LVIF_TEXT|LVIF_PARAM;

  int nItem = 0;
  // TODO:
  /*for (nItem = 0; nItem < Codec_NB; ++nItem) {
    const codec_config_t *config = get_codec_config((LAVAudioCodec)nItem);

    // Create main entry
    lvi.iItem = nItem + 1;
    ListView_InsertItem(hlv, &lvi);

    // Set sub item texts
    ListView_SetItemText(hlv, nItem, 1, (LPWSTR)config->name);
    ListView_SetItemText(hlv, nItem, 2, (LPWSTR)config->description);
  } */

  hr = LoadData();
  if (SUCCEEDED(hr)) {
    // Set checked state
    for (nItem = 0; nItem < ListView_GetItemCount(hlv); nItem++) {
      // TODO: ListView_SetCheckState(hlv, nItem, m_bFormats[nItem]);
    }
  }

  return hr;
}

HRESULT CLAVVideoFormatsProp::LoadData()
{
  HRESULT hr = S_OK;
  
  // TODO: Read Config

  return hr;
}

INT_PTR CLAVVideoFormatsProp::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_NOTIFY:
    NMHDR *hdr = (LPNMHDR)lParam;
    if (hdr->idFrom == IDC_CODECS) {
      switch (hdr->code) {
      case LVN_ITEMCHANGED:
        LPNMLISTVIEW nmlv = (LPNMLISTVIEW)lParam;
        bool check = ListView_GetCheckState(hdr->hwndFrom, nmlv->iItem) ? true : false;
        // TODO:
        /*if (check != m_bFormats[nmlv->iItem]) {
          SetDirty();
        } */
        return TRUE;
      }
    }
    break;
  }
  // Let the parent class handle the message.
  return __super::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}
