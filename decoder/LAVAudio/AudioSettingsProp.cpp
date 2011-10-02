/*
 *      Copyright (C) 2011 Hendrik Leppkes
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
#include "AudioSettingsProp.h"
#include "Media.h"

#include <Commctrl.h>

#include "resource.h"

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
  BOOL bFlag;

  // DRC
  int iDRCLevel = (int)SendDlgItemMessage(m_Dlg, IDC_DRC_LEVEL, TBM_GETPOS, 0, 0);
  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_DRC, BM_GETCHECK, 0, 0);
  hr = m_pAudioSettings->SetDRC(bFlag, iDRCLevel);

  // Bitstreaming codec options
  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_BS_AC3, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetBitstreamConfig(Bitstream_AC3, bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_BS_EAC3, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetBitstreamConfig(Bitstream_EAC3, bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_BS_TRUEHD, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetBitstreamConfig(Bitstream_TRUEHD, bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_BS_DTS, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetBitstreamConfig(Bitstream_DTS, bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_BS_DTSHD, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetBitstreamConfig(Bitstream_DTSHD, bFlag);

  // DTS-HD framing
  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_BS_DTSHD_FRAMING, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetDTSHDFraming(bFlag);

  // The other playback options
  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_AUTO_AVSYNC, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetAutoAVSync(bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_STANDARD_CH_LAYOUT, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetOutputStandardLayout(bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_EXPAND_MONO, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetExpandMono(bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_EXPAND61, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetExpand61(bFlag);

  // Sample Formats
  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_S16, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetSampleFormat(SampleFormat_16, bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_S24, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetSampleFormat(SampleFormat_24, bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_S32, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetSampleFormat(SampleFormat_32, bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_FP32, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetSampleFormat(SampleFormat_FP32, bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_U8, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetSampleFormat(SampleFormat_U8, bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_DELAY_ENABLED, BM_GETCHECK, 0, 0);
  WCHAR buffer[100];
  SendDlgItemMessage(m_Dlg, IDC_DELAY, WM_GETTEXT, 100, (LPARAM)&buffer);
  int delay = _wtoi(buffer);
  m_pAudioSettings->SetAudioDelay(bFlag, delay);

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

    SendDlgItemMessage(m_Dlg, IDC_BS_AC3, BM_SETCHECK, m_bBitstreaming[Bitstream_AC3], 0);
    SendDlgItemMessage(m_Dlg, IDC_BS_EAC3, BM_SETCHECK, m_bBitstreaming[Bitstream_EAC3], 0);
    SendDlgItemMessage(m_Dlg, IDC_BS_TRUEHD, BM_SETCHECK, m_bBitstreaming[Bitstream_TRUEHD], 0);
    SendDlgItemMessage(m_Dlg, IDC_BS_DTS, BM_SETCHECK, m_bBitstreaming[Bitstream_DTS], 0);
    SendDlgItemMessage(m_Dlg, IDC_BS_DTSHD, BM_SETCHECK, m_bBitstreaming[Bitstream_DTSHD], 0);
    EnableWindow(GetDlgItem(m_Dlg, IDC_BS_DTSHD), m_bBitstreaming[Bitstream_DTS]);

    SendDlgItemMessage(m_Dlg, IDC_BS_DTSHD_FRAMING, BM_SETCHECK, m_bDTSHDFraming, 0);
    EnableWindow(GetDlgItem(m_Dlg, IDC_BS_DTSHD_FRAMING), m_bBitstreaming[Bitstream_DTSHD]);
    addHint(IDC_BS_DTSHD_FRAMING, L"With some Receivers, this setting might be needed to achieve the full features of DTS. However, on other Receivers, this option will cause DTS to not work at all.\n\nIf you do not experience any problems, its recommended to leave this setting untouched.");

    SendDlgItemMessage(m_Dlg, IDC_AUTO_AVSYNC, BM_SETCHECK, m_bAutoAVSync, 0);
    addHint(IDC_AUTO_AVSYNC, L"Enables automatic tracking and correction of A/V sync.\n\nIf you encounter any sync issues, disabling this option can help in debugging the source of the problem.");

    SendDlgItemMessage(m_Dlg, IDC_STANDARD_CH_LAYOUT, BM_SETCHECK, m_bOutputStdLayout, 0);
    addHint(IDC_STANDARD_CH_LAYOUT, L"Converts all channel layouts to one of the \"standard\" layouts (5.1/6.1/7.1) by adding silent channels. This is required for sending the PCM over HDMI if not using another downstream mixer, for example when using WASAPI.");

    SendDlgItemMessage(m_Dlg, IDC_EXPAND_MONO, BM_SETCHECK, m_bExpandMono, 0);
    addHint(IDC_EXPAND_MONO, L"Plays Mono Audio in both Left/Right Front channels, instead of the center.");
    SendDlgItemMessage(m_Dlg, IDC_EXPAND61, BM_SETCHECK, m_bExpand61, 0);
    addHint(IDC_EXPAND61, L"Converts 6.1 Audio to 7.1 by copying the Back Center into both Back Left and Right channels.");

    SendDlgItemMessage(m_Dlg, IDC_OUT_S16, BM_SETCHECK, m_bSampleFormats[SampleFormat_16], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_S24, BM_SETCHECK, m_bSampleFormats[SampleFormat_24], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_S32, BM_SETCHECK, m_bSampleFormats[SampleFormat_32], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_FP32, BM_SETCHECK, m_bSampleFormats[SampleFormat_FP32], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_U8, BM_SETCHECK, m_bSampleFormats[SampleFormat_U8], 0);

    SendDlgItemMessage(m_Dlg, IDC_DELAY_ENABLED, BM_SETCHECK, m_bAudioDelay, 0);
    EnableWindow(GetDlgItem(m_Dlg, IDC_DELAYSPIN), m_bAudioDelay);
    EnableWindow(GetDlgItem(m_Dlg, IDC_DELAY), m_bAudioDelay);

    SendDlgItemMessage(m_Dlg, IDC_DELAYSPIN, UDM_SETRANGE32, -1000*60*60*24, 1000*60*60*24);

    WCHAR stringBuffer[100];
    swprintf_s(stringBuffer, L"%d", m_iAudioDelay);
    SendDlgItemMessage(m_Dlg, IDC_DELAY, WM_SETTEXT, 0, (LPARAM)stringBuffer);
  }

  return hr;
}

HRESULT CLAVAudioSettingsProp::LoadData()
{
  HRESULT hr = S_OK;
  hr = m_pAudioSettings->GetDRC(&m_bDRCEnabled, &m_iDRCLevel);
  for (unsigned i = 0; i < Bitstream_NB; ++i)
    m_bBitstreaming[i] = m_pAudioSettings->GetBitstreamConfig((LAVBitstreamCodec)i) != 0;
  m_bDTSHDFraming = m_pAudioSettings->GetDTSHDFraming();
  m_bAutoAVSync = m_pAudioSettings->GetAutoAVSync();
  m_bOutputStdLayout = m_pAudioSettings->GetOutputStandardLayout();
  m_bExpandMono = m_pAudioSettings->GetExpandMono();
  m_bExpand61 = m_pAudioSettings->GetExpand61();

  for (unsigned i = 0; i < SampleFormat_NB; ++i)
    m_bSampleFormats[i] = m_pAudioSettings->GetSampleFormat((LAVAudioSampleFormat)i) != 0;

  m_pAudioSettings->GetAudioDelay(&m_bAudioDelay, &m_iAudioDelay);

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
      EnableWindow(GetDlgItem(m_Dlg, IDC_DRC_LEVEL), (BOOL)lValue);
    } else if (LOWORD(wParam) == IDC_BS_AC3 && HIWORD(wParam) == BN_CLICKED) {
      bool bFlag = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0) != 0;
      if (bFlag != m_bBitstreaming[Bitstream_AC3])
        SetDirty();
    } else if (LOWORD(wParam) == IDC_BS_EAC3 && HIWORD(wParam) == BN_CLICKED) {
      bool bFlag = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0) != 0;
      if (bFlag != m_bBitstreaming[Bitstream_EAC3])
        SetDirty();
    } else if (LOWORD(wParam) == IDC_BS_TRUEHD && HIWORD(wParam) == BN_CLICKED) {
      bool bFlag = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0) != 0;
      if (bFlag != m_bBitstreaming[Bitstream_TRUEHD])
        SetDirty();
    } else if (LOWORD(wParam) == IDC_BS_DTS && HIWORD(wParam) == BN_CLICKED) {
      bool bFlag = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0) != 0;
      if (bFlag != m_bBitstreaming[Bitstream_DTS])
        SetDirty();
      EnableWindow(GetDlgItem(m_Dlg, IDC_BS_DTSHD), bFlag);
    } else if (LOWORD(wParam) == IDC_BS_DTSHD && HIWORD(wParam) == BN_CLICKED) {
      bool bFlag = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0) != 0;
      if (bFlag != m_bBitstreaming[Bitstream_DTSHD])
        SetDirty();
      EnableWindow(GetDlgItem(m_Dlg, IDC_BS_DTSHD_FRAMING), bFlag);
    } else if (LOWORD(wParam) == IDC_BS_DTSHD_FRAMING && HIWORD(wParam) == BN_CLICKED) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bFlag != m_bDTSHDFraming)
        SetDirty();
    } else if (LOWORD(wParam) == IDC_AUTO_AVSYNC && HIWORD(wParam) == BN_CLICKED) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bFlag != m_bAutoAVSync)
        SetDirty();
    } else if (LOWORD(wParam) == IDC_STANDARD_CH_LAYOUT && HIWORD(wParam) == BN_CLICKED) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bFlag != m_bOutputStdLayout)
        SetDirty();
    } else if (LOWORD(wParam) == IDC_EXPAND_MONO && HIWORD(wParam) == BN_CLICKED) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bFlag != m_bExpandMono)
        SetDirty();
    } else if (LOWORD(wParam) == IDC_EXPAND61 && HIWORD(wParam) == BN_CLICKED) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bFlag != m_bExpand61)
        SetDirty();
    } else if (LOWORD(wParam) == IDC_OUT_S16 && HIWORD(wParam) == BN_CLICKED) {
      bool bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0) != 0;
      if (bFlag != m_bSampleFormats[SampleFormat_16])
        SetDirty();
    } else if (LOWORD(wParam) == IDC_OUT_S24 && HIWORD(wParam) == BN_CLICKED) {
      bool bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0) != 0;
      if (bFlag != m_bSampleFormats[SampleFormat_24])
        SetDirty();
    } else if (LOWORD(wParam) == IDC_OUT_S32 && HIWORD(wParam) == BN_CLICKED) {
      bool bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0) != 0;
      if (bFlag != m_bSampleFormats[SampleFormat_32])
        SetDirty();
    } else if (LOWORD(wParam) == IDC_OUT_FP32 && HIWORD(wParam) == BN_CLICKED) {
      bool bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0) != 0;
      if (bFlag != m_bSampleFormats[SampleFormat_FP32])
        SetDirty();
    } else if (LOWORD(wParam) == IDC_OUT_U8 && HIWORD(wParam) == BN_CLICKED) {
      bool bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0) != 0;
      if (bFlag != m_bSampleFormats[SampleFormat_U8])
        SetDirty();
    } else if (LOWORD(wParam) == IDC_DELAY_ENABLED && HIWORD(wParam) == BN_CLICKED) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bFlag != m_bAudioDelay)
        SetDirty();
      EnableWindow(GetDlgItem(m_Dlg, IDC_DELAYSPIN), bFlag);
      EnableWindow(GetDlgItem(m_Dlg, IDC_DELAY), bFlag);
    } else if (LOWORD(wParam) == IDC_DELAY && HIWORD(wParam) == EN_CHANGE) {
      WCHAR buffer[100];
      SendDlgItemMessage(m_Dlg, LOWORD(wParam), WM_GETTEXT, 100, (LPARAM)&buffer);
      int delay = _wtoi(buffer);
      int len = wcslen(buffer);
      if (delay == 0 && (buffer[0] != L'0' || len > 1)) {
        SendDlgItemMessage(m_Dlg, LOWORD(wParam), EM_UNDO, 0, 0);
      } else {
        swprintf_s(buffer, L"%d", delay);
        if (wcslen(buffer) != len)
          SendDlgItemMessage(m_Dlg, IDC_DELAY, WM_SETTEXT, 0, (LPARAM)buffer);
        if (delay != m_iAudioDelay)
          SetDirty();
      }
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

  // Get checked state
  BOOL bFlag;
  for (int nItem = 0; nItem < ListView_GetItemCount(hlv); nItem++) {
    bFlag = ListView_GetCheckState(hlv, nItem);
    m_pAudioSettings->SetFormatConfiguration((LAVAudioCodec)nItem, bFlag);
  }

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
  ListView_SetItemCount(hlv, Codec_NB);

  // Create entrys for the formats
  LVITEM lvi;
  memset(&lvi, 0, sizeof(lvi));
  lvi.mask = LVIF_TEXT|LVIF_PARAM;

  int nItem = 0;
  for (nItem = 0; nItem < Codec_NB; ++nItem) {
    const codec_config_t *config = get_codec_config((LAVAudioCodec)nItem);

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
  for (unsigned i = 0; i < Codec_NB; ++i)
    m_bFormats[i] = m_pAudioSettings->GetFormatConfiguration((LAVAudioCodec)i) != 0;
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

  m_nChannels = 0;

  const char *codec = NULL;
  const char *decodeFormat = NULL;
  int nDecodeChannels = 0;
  int nDecodeSampleRate = 0;
  DWORD dwDecodeChannelMask;
  hr = m_pAudioStatus->GetDecodeDetails(&codec, &decodeFormat, &nDecodeChannels, &nDecodeSampleRate, &dwDecodeChannelMask);
  if (SUCCEEDED(hr)) {
    WCHAR buffer[100];
    _snwprintf_s(buffer, _TRUNCATE, L"%d / 0x%x", nDecodeChannels, dwDecodeChannelMask);
    SendDlgItemMessage(m_Dlg, IDC_INPUT_CHANNEL, WM_SETTEXT, 0, (LPARAM)buffer);

    _snwprintf_s(buffer, _TRUNCATE, L"%d", nDecodeSampleRate);
    SendDlgItemMessage(m_Dlg, IDC_INPUT_SAMPLERATE, WM_SETTEXT, 0, (LPARAM)buffer);

    _snwprintf_s(buffer, _TRUNCATE, L"%S", codec);
    SendDlgItemMessage(m_Dlg, IDC_INPUT_CODEC, WM_SETTEXT, 0, (LPARAM)buffer);

    _snwprintf_s(buffer, _TRUNCATE, L"%S", decodeFormat);
    SendDlgItemMessage(m_Dlg, IDC_INPUT_FORMAT, WM_SETTEXT, 0, (LPARAM)buffer);
  }

  SendDlgItemMessage(m_Dlg, IDC_INT8, BM_SETCHECK, m_pAudioStatus->IsSampleFormatSupported(SampleFormat_U8), 0);
  SendDlgItemMessage(m_Dlg, IDC_INT16, BM_SETCHECK, m_pAudioStatus->IsSampleFormatSupported(SampleFormat_16), 0);
  SendDlgItemMessage(m_Dlg, IDC_INT24, BM_SETCHECK, m_pAudioStatus->IsSampleFormatSupported(SampleFormat_24), 0);
  SendDlgItemMessage(m_Dlg, IDC_INT32, BM_SETCHECK, m_pAudioStatus->IsSampleFormatSupported(SampleFormat_32), 0);
  SendDlgItemMessage(m_Dlg, IDC_FP32, BM_SETCHECK, m_pAudioStatus->IsSampleFormatSupported(SampleFormat_FP32), 0);

  const char *outputFormat = NULL;
  int nOutputChannels = 0;
  int nOutputSampleRate = 0;
  DWORD dwOutputChannelMask = 0;
  hr = m_pAudioStatus->GetOutputDetails(&outputFormat, &nOutputChannels, &nOutputSampleRate, &dwOutputChannelMask);
  if (SUCCEEDED(hr)) {
    WCHAR buffer[100];

    if (hr == S_OK) {
      _snwprintf_s(buffer, _TRUNCATE, L"%d / 0x%x", nOutputChannels, dwOutputChannelMask);
      SendDlgItemMessage(m_Dlg, IDC_OUTPUT_CHANNEL, WM_SETTEXT, 0, (LPARAM)buffer);

      _snwprintf_s(buffer, _TRUNCATE, L"%d", nOutputSampleRate);
      SendDlgItemMessage(m_Dlg, IDC_OUTPUT_SAMPLERATE, WM_SETTEXT, 0, (LPARAM)buffer);

      _snwprintf_s(buffer, _TRUNCATE, L"PCM");
      SendDlgItemMessage(m_Dlg, IDC_OUTPUT_CODEC, WM_SETTEXT, 0, (LPARAM)buffer);

      _snwprintf_s(buffer, _TRUNCATE, L"%S", outputFormat);
      SendDlgItemMessage(m_Dlg, IDC_OUTPUT_FORMAT, WM_SETTEXT, 0, (LPARAM)buffer);

      m_nChannels = nOutputChannels;
    } else {
      _snwprintf_s(buffer, _TRUNCATE, L"Bitstreaming");
      SendDlgItemMessage(m_Dlg, IDC_OUTPUT_CODEC, WM_SETTEXT, 0, (LPARAM)buffer);
    }
  }

  SetTimer(m_Dlg, 1, 250, NULL);
  m_pAudioStatus->EnableVolumeStats();

  WCHAR chBuffer[5];
  if (dwOutputChannelMask == 0 && nOutputChannels != 0) {
    // 0x4 is only front center, 0x3 is front left+right
    dwOutputChannelMask = nOutputChannels == 1 ? 0x4 : 0x3;
  }
  for(int i = 0; i < MAX_CHANNELS; ++i) {
    SendDlgItemMessage(m_Dlg, iddVolumeControls[i], PBM_SETRANGE, 0, MAKELPARAM(0, 50));
    _snwprintf_s(chBuffer, _TRUNCATE, L"%S", get_channel_desc(get_flag_from_channel(dwOutputChannelMask, i)));
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
