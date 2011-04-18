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
#include "AudioSettingsProp.h"
#include "Media.h"

#include <Commctrl.h>

#include "resource.h"
#include "version.h"

CLAVAudioSettingsProp::CLAVAudioSettingsProp(LPUNKNOWN pUnk, HRESULT* phr)
  : CBaseDSPropPage(NAME("LAVCAudioProp"), pUnk, IDD_PROPPAGE_AUDIO_SETTINGS, IDS_SETTINGS), m_pAudioSettings(NULL), m_bDRCEnabled(FALSE), m_iDRCLevel(100)
{
}


CLAVAudioSettingsProp::~CLAVAudioSettingsProp()
{
}

HRESULT CLAVAudioSettingsProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == NULL)
  {
    return E_POINTER;
  }
  ASSERT(m_pAudioSettings == NULL);
  return pUnk->QueryInterface(&m_pAudioSettings);
}

HRESULT CLAVAudioSettingsProp::OnDisconnect()
{
  SafeRelease(&m_pAudioSettings);
  return S_OK;
}


HRESULT CLAVAudioSettingsProp::OnApplyChanges()
{
  ASSERT(m_pAudioSettings != NULL);
  HRESULT hr = S_OK;

  int iDRCLevel = (int)SendDlgItemMessage(m_Dlg, IDC_DRC_LEVEL, TBM_GETPOS, 0, 0);
  BOOL bDRC = (BOOL)SendDlgItemMessage(m_Dlg, IDC_DRC, BM_GETCHECK, 0, 0);
  hr = m_pAudioSettings->SetDRC(bDRC, iDRCLevel);

  LoadData();

  return hr;
} 

HRESULT CLAVAudioSettingsProp::OnActivate()
{
  HRESULT hr = S_OK;
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
  if (InitCommonControlsEx(&icc) == FALSE)
  {
    return E_FAIL;
  }
  ASSERT(m_pAudioSettings != NULL);

  const WCHAR *version = TEXT(LAV_AUDIO) L" " TEXT(LAV_VERSION_STR);
  SendDlgItemMessage(m_Dlg, IDC_LAVAUDIO_FOOTER, WM_SETTEXT, 0, (LPARAM)version);

  hr = LoadData();
  if (SUCCEEDED(hr)) {
    SendDlgItemMessage(m_Dlg, IDC_DRC, BM_SETCHECK, m_bDRCEnabled, 0);

    EnableWindow(GetDlgItem(m_Dlg, IDC_DRC_LEVEL), m_bDRCEnabled);
    SendDlgItemMessage(m_Dlg, IDC_DRC_LEVEL, TBM_SETRANGE, 0, MAKELONG(0, 100));
    SendDlgItemMessage(m_Dlg, IDC_DRC_LEVEL, TBM_SETTICFREQ, 10, 0);
    SendDlgItemMessage(m_Dlg, IDC_DRC_LEVEL, TBM_SETPOS, 1, m_iDRCLevel);

    WCHAR buffer[10];
    _snwprintf_s(buffer, _TRUNCATE, L"%d%%", m_iDRCLevel);
    SendDlgItemMessage(m_Dlg, IDC_DRC_LEVEL_TEXT, WM_SETTEXT, 0, (LPARAM)buffer);
  }

  return hr;
}

HRESULT CLAVAudioSettingsProp::LoadData()
{
  HRESULT hr = S_OK;
  hr = m_pAudioSettings->GetDRC(&m_bDRCEnabled, &m_iDRCLevel);
  return hr;
}

INT_PTR CLAVAudioSettingsProp::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  LRESULT lValue;
  switch (uMsg)
  {
  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_DRC && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, IDC_DRC, BM_GETCHECK, 0, 0);
      if (lValue != m_bDRCEnabled) {
        SetDirty();
      }
      EnableWindow(GetDlgItem(m_Dlg, IDC_DRC_LEVEL), lValue);
    }
    break;
  case WM_HSCROLL:
    lValue = SendDlgItemMessage(m_Dlg, IDC_DRC_LEVEL, TBM_GETPOS, 0, 0);
    if (lValue != m_iDRCLevel) {
      SetDirty();
    }
    WCHAR buffer[10];
    _snwprintf_s(buffer, _TRUNCATE, L"%d%%", lValue);
    SendDlgItemMessage(m_Dlg, IDC_DRC_LEVEL_TEXT, WM_SETTEXT, 0, (LPARAM)buffer);
    break;
  }
  // Let the parent class handle the message.
  return __super::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Format Configurations

CLAVAudioFormatsProp::CLAVAudioFormatsProp(LPUNKNOWN pUnk, HRESULT* phr)
  : CBaseDSPropPage(NAME("LAVCAudioFormats"), pUnk, IDD_PROPPAGE_FORMATS, IDS_FORMATS), m_pAudioSettings(NULL)
{
}


CLAVAudioFormatsProp::~CLAVAudioFormatsProp()
{
}

HRESULT CLAVAudioFormatsProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == NULL)
  {
    return E_POINTER;
  }
  ASSERT(m_pAudioSettings == NULL);
  return pUnk->QueryInterface(&m_pAudioSettings);
}

HRESULT CLAVAudioFormatsProp::OnDisconnect()
{
  SafeRelease(&m_pAudioSettings);
  return S_OK;
}


HRESULT CLAVAudioFormatsProp::OnApplyChanges()
{
  ASSERT(m_pAudioSettings != NULL);
  HRESULT hr = S_OK;

  HWND hlv = GetDlgItem(m_Dlg, IDC_CODECS);

  bool bFormats[CC_NB];

  // Get checked state
  for (int nItem = 0; nItem < ListView_GetItemCount(hlv); nItem++) {
    bFormats[nItem] = ListView_GetCheckState(hlv, nItem) ? true : false;
  }
  m_pAudioSettings->SetFormatConfiguration(bFormats);

  LoadData();

  return hr;
}

HRESULT CLAVAudioFormatsProp::OnActivate()
{
  HRESULT hr = S_OK;
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
  if (InitCommonControlsEx(&icc) == FALSE)
  {
    return E_FAIL;
  }
  ASSERT(m_pAudioSettings != NULL);

  // Setup ListView control for format configuration
  HWND hlv = GetDlgItem(m_Dlg, IDC_CODECS);
  ListView_SetExtendedListViewStyle(hlv, LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);

  int nCol = 1;
  LVCOLUMN lvc = {LVCF_WIDTH, 0, 20, 0};
  ListView_InsertColumn(hlv, 0, &lvc);
  ListView_AddCol(hlv, nCol,  75, L"Codec", false);
  ListView_AddCol(hlv, nCol, 177, L"Description", false);

  ListView_DeleteAllItems(hlv);
  ListView_SetItemCount(hlv, CC_NB);

  // Create entrys for the formats
  LVITEM lvi;
  memset(&lvi, 0, sizeof(lvi));
  lvi.mask = LVIF_TEXT|LVIF_PARAM;

  int nItem = 0;
  for (nItem = 0; nItem < CC_NB; ++nItem) {
    const codec_config_t *config = get_codec_config((ConfigCodecs)nItem);

    // Create main entry
    lvi.iItem = nItem + 1;
    ListView_InsertItem(hlv, &lvi);

    // Set sub item texts
    ListView_SetItemText(hlv, nItem, 1, (LPWSTR)config->name);
    ListView_SetItemText(hlv, nItem, 2, (LPWSTR)config->description);
  }

  hr = LoadData();
  if (SUCCEEDED(hr)) {
    // Set checked state
    for (nItem = 0; nItem < ListView_GetItemCount(hlv); nItem++) {
      ListView_SetCheckState(hlv, nItem, m_bFormats[nItem]);
    }
  }

  return hr;
}

HRESULT CLAVAudioFormatsProp::LoadData()
{
  HRESULT hr = S_OK;
  hr = m_pAudioSettings->GetFormatConfiguration(m_bFormats);
  return hr;
}

INT_PTR CLAVAudioFormatsProp::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
        if (check != m_bFormats[nmlv->iItem]) {
          SetDirty();
        }
        return TRUE;
      }
    }
    break;
  }
  // Let the parent class handle the message.
  return __super::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Audio Status Panel

#define MAX_CHANNELS 8
static int iddVolumeControls[MAX_CHANNELS] = {IDC_VOLUME1, IDC_VOLUME2, IDC_VOLUME3, IDC_VOLUME4, IDC_VOLUME5, IDC_VOLUME6, IDC_VOLUME7, IDC_VOLUME8};
static int iddVolumeDescs[MAX_CHANNELS] = {IDC_VOLUME1_DESC, IDC_VOLUME2_DESC, IDC_VOLUME3_DESC, IDC_VOLUME4_DESC, IDC_VOLUME5_DESC, IDC_VOLUME6_DESC, IDC_VOLUME7_DESC, IDC_VOLUME8_DESC};


CLAVAudioStatusProp::CLAVAudioStatusProp(LPUNKNOWN pUnk, HRESULT* phr)
  : CBaseDSPropPage(NAME("LAVCAudioStatusProp"), pUnk, IDD_PROPPAGE_STATUS, IDS_STATUS), m_pAudioStatus(NULL), m_nChannels(0)
{
}


CLAVAudioStatusProp::~CLAVAudioStatusProp()
{
}

HRESULT CLAVAudioStatusProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == NULL)
  {
    return E_POINTER;
  }
  ASSERT(m_pAudioStatus == NULL);
  return pUnk->QueryInterface(&m_pAudioStatus);
}

HRESULT CLAVAudioStatusProp::OnDisconnect()
{
  SafeRelease(&m_pAudioStatus);
  return S_OK;
}

HRESULT CLAVAudioStatusProp::OnActivate()
{
  HRESULT hr = S_OK;
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icc.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
  if (InitCommonControlsEx(&icc) == FALSE)
  {
    return E_FAIL;
  }
  ASSERT(m_pAudioStatus != NULL);

  const char *codec = NULL;
  int nChannels = 0;
  int nSampleRate = 0;
  hr = m_pAudioStatus->GetInputDetails(&codec, &nChannels, &nSampleRate);
  if (SUCCEEDED(hr)) {
    WCHAR buffer[100];
    _snwprintf_s(buffer, _TRUNCATE, L"%d", nChannels);
    SendDlgItemMessage(m_Dlg, IDC_CHANNELS, WM_SETTEXT, 0, (LPARAM)buffer);
    m_nChannels = nChannels;

    _snwprintf_s(buffer, _TRUNCATE, L"%d", nSampleRate);
    SendDlgItemMessage(m_Dlg, IDC_SAMPLE_RATE, WM_SETTEXT, 0, (LPARAM)buffer);

    _snwprintf_s(buffer, _TRUNCATE, L"%S", codec);
    SendDlgItemMessage(m_Dlg, IDC_CODEC, WM_SETTEXT, 0, (LPARAM)buffer);
  }

  SendDlgItemMessage(m_Dlg, IDC_INT8, BM_SETCHECK, m_pAudioStatus->IsSampleFormatSupported(SampleFormat_U8), 0);
  SendDlgItemMessage(m_Dlg, IDC_INT16, BM_SETCHECK, m_pAudioStatus->IsSampleFormatSupported(SampleFormat_16), 0);
  SendDlgItemMessage(m_Dlg, IDC_INT24, BM_SETCHECK, m_pAudioStatus->IsSampleFormatSupported(SampleFormat_24), 0);
  SendDlgItemMessage(m_Dlg, IDC_INT32, BM_SETCHECK, m_pAudioStatus->IsSampleFormatSupported(SampleFormat_32), 0);
  SendDlgItemMessage(m_Dlg, IDC_FP32, BM_SETCHECK, m_pAudioStatus->IsSampleFormatSupported(SampleFormat_FP32), 0);

  const char *decodeFormat = NULL;
  const char *outputFormat = NULL;
  DWORD dwChannelMask = 0;
  hr = m_pAudioStatus->GetOutputDetails(&decodeFormat, &outputFormat, &dwChannelMask);
  if (SUCCEEDED(hr)) {
    WCHAR buffer[100];

    _snwprintf_s(buffer, _TRUNCATE, L"%S", decodeFormat);
    SendDlgItemMessage(m_Dlg, IDC_DECODE_FORMAT, WM_SETTEXT, 0, (LPARAM)buffer);

    _snwprintf_s(buffer, _TRUNCATE, L"%S", outputFormat);
    SendDlgItemMessage(m_Dlg, IDC_OUTPUT_FORMAT, WM_SETTEXT, 0, (LPARAM)buffer);

    _snwprintf_s(buffer, _TRUNCATE, L"0x%x", dwChannelMask);
    SendDlgItemMessage(m_Dlg, IDC_CHANNEL_MASK, WM_SETTEXT, 0, (LPARAM)buffer);
  }

  SetTimer(m_Dlg, 1, 250, NULL);
  m_pAudioStatus->EnableVolumeStats();

  WCHAR chBuffer[5];
  if (dwChannelMask == 0 && nChannels != 0) {
    // 0x4 is only front center, 0x3 is front left+right
    dwChannelMask = nChannels == 1 ? 0x4 : 0x3;
  }
  for(int i = 0; i < MAX_CHANNELS; ++i) {
    SendDlgItemMessage(m_Dlg, iddVolumeControls[i], PBM_SETRANGE, 0, MAKELPARAM(0, 50));
    _snwprintf_s(chBuffer, _TRUNCATE, L"%S", get_channel_desc(get_flag_from_channel(dwChannelMask, i)));
    SendDlgItemMessage(m_Dlg, iddVolumeDescs[i], WM_SETTEXT, 0, (LPARAM)chBuffer);
  }

  return hr;
}

HRESULT CLAVAudioStatusProp::OnDeactivate()
{
  KillTimer(m_Dlg, 1);
  m_pAudioStatus->DisableVolumeStats();
  return S_OK;
}

void CLAVAudioStatusProp::UpdateVolumeDisplay()
{
  for(int i = 0; i < m_nChannels; ++i) {
    float fDB = 0.0f;
    if (SUCCEEDED(m_pAudioStatus->GetChannelVolumeAverage(i, &fDB))) {
      int value = (int)fDB + 50;
      SendDlgItemMessage(m_Dlg, iddVolumeControls[i], PBM_SETPOS, value, 0);
    } else {
      SendDlgItemMessage(m_Dlg, iddVolumeControls[i], PBM_SETPOS, 0, 0);
    }
  }
}

INT_PTR CLAVAudioStatusProp::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_TIMER:
    UpdateVolumeDisplay();
    break;
  }
  // Let the parent class handle the message.
  return __super::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}
