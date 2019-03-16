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
#include "AudioSettingsProp.h"
#include "Media.h"

#include <Commctrl.h>

#include "resource.h"
#include "version.h"

CLAVAudioSettingsProp::CLAVAudioSettingsProp(LPUNKNOWN pUnk, HRESULT* phr)
  : CBaseDSPropPage(NAME("LAVCAudioProp"), pUnk, IDD_PROPPAGE_AUDIO_SETTINGS, IDS_SETTINGS)
{
}


CLAVAudioSettingsProp::~CLAVAudioSettingsProp()
{
}

HRESULT CLAVAudioSettingsProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == nullptr)
  {
    return E_POINTER;
  }
  ASSERT(m_pAudioSettings == nullptr);
  return pUnk->QueryInterface(&m_pAudioSettings);
}

HRESULT CLAVAudioSettingsProp::OnDisconnect()
{
  SafeRelease(&m_pAudioSettings);
  return S_OK;
}


HRESULT CLAVAudioSettingsProp::OnApplyChanges()
{
  ASSERT(m_pAudioSettings != nullptr);
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

  // Fallback to audio decoding
  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_BS_FALLBACK, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetBitstreamingFallback(bFlag);

  // The other playback options
  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_AUTO_AVSYNC, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetAutoAVSync(bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_STANDARD_CH_LAYOUT, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetOutputStandardLayout(bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUTPUT51_LEGACY, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetOutput51LegacyLayout(bFlag);

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

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_S16_DITHER, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetSampleConvertDithering(bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_DELAY_ENABLED, BM_GETCHECK, 0, 0);
  WCHAR buffer[100];
  SendDlgItemMessage(m_Dlg, IDC_DELAY, WM_GETTEXT, 100, (LPARAM)&buffer);
  int delay = _wtoi(buffer);
  m_pAudioSettings->SetAudioDelay(bFlag, delay);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_TRAYICON, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetTrayIcon(bFlag);

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
  ASSERT(m_pAudioSettings != nullptr);

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

    SendDlgItemMessage(m_Dlg, IDC_BS_FALLBACK, BM_SETCHECK, m_bBitstreamingFallback, 0);
    addHint(IDC_BS_FALLBACK, L"Fallback to audio decoding if bitstreaming is not supported by the audio renderer/hardware.");

    SendDlgItemMessage(m_Dlg, IDC_AUTO_AVSYNC, BM_SETCHECK, m_bAutoAVSync, 0);
    addHint(IDC_AUTO_AVSYNC, L"Enables automatic tracking and correction of A/V sync.\n\nIf you encounter any sync issues, disabling this option can help in debugging the source of the problem.");

    SendDlgItemMessage(m_Dlg, IDC_STANDARD_CH_LAYOUT, BM_SETCHECK, m_bOutputStdLayout, 0);
    addHint(IDC_STANDARD_CH_LAYOUT, L"Converts all channel layouts to one of the \"standard\" layouts (5.1/6.1/7.1) by adding silent channels. This is required for sending the PCM over HDMI if not using another downstream mixer, for example when using WASAPI.");

    SendDlgItemMessage(m_Dlg, IDC_OUTPUT51_LEGACY, BM_SETCHECK, m_bOutput51Legacy, 0);
    addHint(IDC_OUTPUT51_LEGACY, L"Use the legacy 5.1 channel layout which uses back channels instead of side channels, required for some audio devices and/or software.");

    SendDlgItemMessage(m_Dlg, IDC_EXPAND_MONO, BM_SETCHECK, m_bExpandMono, 0);
    addHint(IDC_EXPAND_MONO, L"Plays Mono Audio in both Left/Right Front channels, instead of the center.");
    SendDlgItemMessage(m_Dlg, IDC_EXPAND61, BM_SETCHECK, m_bExpand61, 0);
    addHint(IDC_EXPAND61, L"Converts 6.1 Audio to 7.1 by copying the Back Center into both Back Left and Right channels.");

    SendDlgItemMessage(m_Dlg, IDC_OUT_S16, BM_SETCHECK, m_bSampleFormats[SampleFormat_16], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_S24, BM_SETCHECK, m_bSampleFormats[SampleFormat_24], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_S32, BM_SETCHECK, m_bSampleFormats[SampleFormat_32], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_FP32, BM_SETCHECK, m_bSampleFormats[SampleFormat_FP32], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_U8, BM_SETCHECK, m_bSampleFormats[SampleFormat_U8], 0);

    SendDlgItemMessage(m_Dlg, IDC_OUT_S16_DITHER, BM_SETCHECK, m_bDither, 0);

    SendDlgItemMessage(m_Dlg, IDC_DELAY_ENABLED, BM_SETCHECK, m_bAudioDelay, 0);
    EnableWindow(GetDlgItem(m_Dlg, IDC_DELAYSPIN), m_bAudioDelay);
    EnableWindow(GetDlgItem(m_Dlg, IDC_DELAY), m_bAudioDelay);

    SendDlgItemMessage(m_Dlg, IDC_DELAYSPIN, UDM_SETRANGE32, -1000*60*60*24, 1000*60*60*24);

    WCHAR stringBuffer[100];
    swprintf_s(stringBuffer, L"%d", m_iAudioDelay);
    SendDlgItemMessage(m_Dlg, IDC_DELAY, WM_SETTEXT, 0, (LPARAM)stringBuffer);

    SendDlgItemMessage(m_Dlg, IDC_TRAYICON, BM_SETCHECK, m_TrayIcon, 0);
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
  m_bBitstreamingFallback = m_pAudioSettings->GetBitstreamingFallback();
  m_bAutoAVSync = m_pAudioSettings->GetAutoAVSync();
  m_bOutputStdLayout = m_pAudioSettings->GetOutputStandardLayout();
  m_bOutput51Legacy = m_pAudioSettings->GetOutput51LegacyLayout();
  m_bExpandMono = m_pAudioSettings->GetExpandMono();
  m_bExpand61 = m_pAudioSettings->GetExpand61();

  for (unsigned i = 0; i < SampleFormat_NB; ++i)
    m_bSampleFormats[i] = m_pAudioSettings->GetSampleFormat((LAVAudioSampleFormat)i) != 0;
  m_bDither = m_pAudioSettings->GetSampleConvertDithering();

  m_pAudioSettings->GetAudioDelay(&m_bAudioDelay, &m_iAudioDelay);

  m_TrayIcon = m_pAudioSettings->GetTrayIcon();

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
    } else if (LOWORD(wParam) == IDC_BS_FALLBACK && HIWORD(wParam) == BN_CLICKED) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bFlag != m_bBitstreamingFallback)
        SetDirty();
    } else if (LOWORD(wParam) == IDC_AUTO_AVSYNC && HIWORD(wParam) == BN_CLICKED) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bFlag != m_bAutoAVSync)
        SetDirty();
    } else if (LOWORD(wParam) == IDC_STANDARD_CH_LAYOUT && HIWORD(wParam) == BN_CLICKED) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bFlag != m_bOutputStdLayout)
        SetDirty();
    } else if (LOWORD(wParam) == IDC_OUTPUT51_LEGACY && HIWORD(wParam) == BN_CLICKED) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bFlag != m_bOutput51Legacy)
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
    } else if (LOWORD(wParam) == IDC_OUT_S16_DITHER && HIWORD(wParam) == BN_CLICKED) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bFlag != m_bDither)
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
      size_t len = wcslen(buffer);
      if (delay == 0 && (buffer[0] != L'0' || len > 1)) {
        SendDlgItemMessage(m_Dlg, LOWORD(wParam), EM_UNDO, 0, 0);
      } else {
        swprintf_s(buffer, L"%d", delay);
        if (wcslen(buffer) != len)
          SendDlgItemMessage(m_Dlg, IDC_DELAY, WM_SETTEXT, 0, (LPARAM)buffer);
        if (delay != m_iAudioDelay)
          SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_TRAYICON && HIWORD(wParam) == BN_CLICKED) {
      BOOL bFlag = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0) != 0;
      if (bFlag != m_TrayIcon)
        SetDirty();
    }

    break;
  case WM_HSCROLL:
    lValue = SendDlgItemMessage(m_Dlg, IDC_DRC_LEVEL, TBM_GETPOS, 0, 0);
    if (lValue != m_iDRCLevel) {
      SetDirty();
    }
    WCHAR buffer[10];
    _snwprintf_s(buffer, _TRUNCATE, L"%d%%", (int)lValue);
    SendDlgItemMessage(m_Dlg, IDC_DRC_LEVEL_TEXT, WM_SETTEXT, 0, (LPARAM)buffer);
    break;
  }
  // Let the parent class handle the message.
  return __super::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Mixer Configurations

CLAVAudioMixingProp::CLAVAudioMixingProp(LPUNKNOWN pUnk, HRESULT* phr)
  : CBaseDSPropPage(NAME("LAVCAudioMixing"), pUnk, IDD_PROPPAGE_AUDIO_MIXING, IDS_MIXER)
{
}


CLAVAudioMixingProp::~CLAVAudioMixingProp()
{
}

HRESULT CLAVAudioMixingProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == nullptr)
  {
    return E_POINTER;
  }
  ASSERT(m_pAudioSettings == nullptr);
  return pUnk->QueryInterface(&m_pAudioSettings);
}

HRESULT CLAVAudioMixingProp::OnDisconnect()
{
  SafeRelease(&m_pAudioSettings);
  return S_OK;
}

static DWORD dwSpkLayouts[] = {
  AV_CH_LAYOUT_MONO,
  AV_CH_LAYOUT_STEREO,
  AV_CH_LAYOUT_2_2,
  AV_CH_LAYOUT_5POINT1,
  AV_CH_LAYOUT_6POINT1,
  AV_CH_LAYOUT_7POINT1,
};
static DWORD get_speaker_index(DWORD dwLayout) {
  int i = 0;
  for(i = 0; i < countof(dwSpkLayouts); i++) {
    if (dwSpkLayouts[i] == dwLayout)
      return i;
  }
  return (DWORD)-1;
}

HRESULT CLAVAudioMixingProp::OnApplyChanges()
{
  ASSERT(m_pAudioSettings != nullptr);
  HRESULT hr = S_OK;
  DWORD dwVal = 0;
  BOOL bVal = FALSE;

  dwVal = (DWORD)SendDlgItemMessage(m_Dlg, IDC_OUTPUT_SPEAKERS, CB_GETCURSEL, 0, 0);
  m_pAudioSettings->SetMixingLayout(dwSpkLayouts[dwVal]);

  bVal = (BOOL)SendDlgItemMessage(m_Dlg, IDC_MIXING, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetMixingEnabled(bVal);

  DWORD dwMixingFlags = 0;
  bVal = (BOOL)SendDlgItemMessage(m_Dlg, IDC_UNTOUCHED_STEREO, BM_GETCHECK, 0, 0);
  if (bVal) dwMixingFlags |= LAV_MIXING_FLAG_UNTOUCHED_STEREO;

  bVal = (BOOL)SendDlgItemMessage(m_Dlg, IDC_NORMALIZE_MATRIX, BM_GETCHECK, 0, 0);
  if (bVal) dwMixingFlags |= LAV_MIXING_FLAG_NORMALIZE_MATRIX;

  bVal = (BOOL)SendDlgItemMessage(m_Dlg, IDC_CLIP_PROTECTION, BM_GETCHECK, 0, 0);
  if (bVal) dwMixingFlags |= LAV_MIXING_FLAG_CLIP_PROTECTION;

  m_pAudioSettings->SetMixingFlags(dwMixingFlags);

  BOOL bNormal = (BOOL)SendDlgItemMessage(m_Dlg, IDC_MIXMODE_NORMAL, BM_GETCHECK, 0, 0);
  BOOL bDolby = (BOOL)SendDlgItemMessage(m_Dlg, IDC_MIXMODE_DOLBY, BM_GETCHECK, 0, 0);
  BOOL bDPL2 = (BOOL)SendDlgItemMessage(m_Dlg, IDC_MIXMODE_DPL2, BM_GETCHECK, 0, 0);
  m_pAudioSettings->SetMixingMode(bDolby ? MatrixEncoding_Dolby : (bDPL2 ? MatrixEncoding_DPLII : MatrixEncoding_None));

  DWORD dwMixCenter = (DWORD)SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_CENTER, TBM_GETPOS, 0, 0);
  DWORD dwMixSurround = (DWORD)SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_SURROUND, TBM_GETPOS, 0, 0);
  DWORD dwMixLFE = (DWORD)SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_LFE, TBM_GETPOS, 0, 0);
  m_pAudioSettings->SetMixingLevels(dwMixCenter, dwMixSurround, dwMixLFE);

  LoadData();

  return hr;
}

HRESULT CLAVAudioMixingProp::OnActivate()
{
  HRESULT hr = S_OK;
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
  if (InitCommonControlsEx(&icc) == FALSE)
  {
    return E_FAIL;
  }
  ASSERT(m_pAudioSettings != nullptr);

  WCHAR spkMono[] = L"Mono";
  WCHAR spkStereo[] = L"Stereo";
  WCHAR spkQuadro[] = L"4.0";
  WCHAR spk51Surround[] = L"5.1";
  WCHAR spk61Surround[] = L"6.1";
  WCHAR spk71Surround[] = L"7.1";

  SendDlgItemMessage(m_Dlg, IDC_OUTPUT_SPEAKERS, CB_RESETCONTENT, 0, 0);
  SendDlgItemMessage(m_Dlg, IDC_OUTPUT_SPEAKERS, CB_ADDSTRING, 0, (LPARAM)spkMono);
  SendDlgItemMessage(m_Dlg, IDC_OUTPUT_SPEAKERS, CB_ADDSTRING, 0, (LPARAM)spkStereo);
  SendDlgItemMessage(m_Dlg, IDC_OUTPUT_SPEAKERS, CB_ADDSTRING, 0, (LPARAM)spkQuadro);
  SendDlgItemMessage(m_Dlg, IDC_OUTPUT_SPEAKERS, CB_ADDSTRING, 0, (LPARAM)spk51Surround);
  SendDlgItemMessage(m_Dlg, IDC_OUTPUT_SPEAKERS, CB_ADDSTRING, 0, (LPARAM)spk61Surround);
  SendDlgItemMessage(m_Dlg, IDC_OUTPUT_SPEAKERS, CB_ADDSTRING, 0, (LPARAM)spk71Surround);

  SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_CENTER, TBM_SETRANGE, 0, MAKELONG(0, 10000));
  SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_CENTER, TBM_SETTICFREQ, 100, 0);
  SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_CENTER, TBM_SETLINESIZE, 0, 100);
  SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_CENTER, TBM_SETPAGESIZE, 0, 100);
  SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_SURROUND, TBM_SETRANGE, 0, MAKELONG(0, 10000));
  SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_SURROUND, TBM_SETTICFREQ, 100, 0);
  SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_SURROUND, TBM_SETLINESIZE, 0, 100);
  SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_SURROUND, TBM_SETPAGESIZE, 0, 100);
  SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_LFE, TBM_SETRANGE, 0, MAKELONG(0, 30000));
  SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_LFE, TBM_SETTICFREQ, 100, 0);
  SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_LFE, TBM_SETLINESIZE, 0, 100);
  SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_LFE, TBM_SETPAGESIZE, 0, 100);

  addHint(IDC_UNTOUCHED_STEREO, L"With this option on, stereo sources will not be mixed. This is useful when you want to mix all surround sources to e.g. 5.1, but leave stereo untouched.");
  addHint(IDC_NORMALIZE_MATRIX, L"Normalizing the matrix will apply a global attenuation to the audio, in effect making it quieter to ensure that there is a consistent volume throughout the file, and no clipping occurs.\n\n"
                                L"This mode will produce inconsistent volumes between different source formats (stereo will be louder than 5.1), but the volume during playback of one file will not change.");
  addHint(IDC_CLIP_PROTECTION, L"Clipping protection analyzes the audio, and reduces the volume if clipping is detected.\n\n"
                               L"This mode tries to preserve the original volume of the audio, and is generally more consistent between different source formats. It may however cause a sudden volume change during playback. "
                               L"In addition, this mode has a higher volume than a normalized matrix and is preferred on weak speakers or headphones.");

  hr = LoadData();
  if (SUCCEEDED(hr)) {
    SendDlgItemMessage(m_Dlg, IDC_MIXING, BM_SETCHECK, m_bMixing, 0);
    SendDlgItemMessage(m_Dlg, IDC_OUTPUT_SPEAKERS, CB_SETCURSEL, get_speaker_index(m_dwSpeakerLayout), 0);

    SendDlgItemMessage(m_Dlg, IDC_UNTOUCHED_STEREO, BM_SETCHECK, !!(m_dwFlags & LAV_MIXING_FLAG_UNTOUCHED_STEREO), 0);
    SendDlgItemMessage(m_Dlg, IDC_NORMALIZE_MATRIX, BM_SETCHECK, !!(m_dwFlags & LAV_MIXING_FLAG_NORMALIZE_MATRIX), 0);
    SendDlgItemMessage(m_Dlg, IDC_CLIP_PROTECTION, BM_SETCHECK, !!(m_dwFlags & LAV_MIXING_FLAG_CLIP_PROTECTION), 0);

    SendDlgItemMessage(m_Dlg, IDC_MIXMODE_NORMAL, BM_SETCHECK, (m_dwMixingMode == MatrixEncoding_None), 0);
    SendDlgItemMessage(m_Dlg, IDC_MIXMODE_DOLBY, BM_SETCHECK, (m_dwMixingMode == MatrixEncoding_Dolby), 0);
    SendDlgItemMessage(m_Dlg, IDC_MIXMODE_DPL2, BM_SETCHECK, (m_dwMixingMode == MatrixEncoding_DPLII), 0);

    SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_CENTER, TBM_SETPOS, 1, m_dwMixCenter);
    SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_SURROUND, TBM_SETPOS, 1, m_dwMixSurround);
    SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_LFE, TBM_SETPOS, 1, m_dwMixLFE);

    WCHAR buffer[10];
    _snwprintf_s(buffer, _TRUNCATE, L"%.2f", (double)m_dwMixCenter / 10000.0);
    SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_CENTER_TEXT, WM_SETTEXT, 0, (LPARAM)buffer);
    _snwprintf_s(buffer, _TRUNCATE, L"%.2f", (double)m_dwMixSurround / 10000.0);
    SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_SURROUND_TEXT, WM_SETTEXT, 0, (LPARAM)buffer);
    _snwprintf_s(buffer, _TRUNCATE, L"%.2f", (double)m_dwMixLFE / 10000.0);
    SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_LFE_TEXT, WM_SETTEXT, 0, (LPARAM)buffer);
  }

  return hr;
}

HRESULT CLAVAudioMixingProp::LoadData()
{
  HRESULT hr = S_OK;

  m_dwSpeakerLayout = m_pAudioSettings->GetMixingLayout();
  m_bMixing         = m_pAudioSettings->GetMixingEnabled();
  m_dwFlags         = m_pAudioSettings->GetMixingFlags();
  m_dwMixingMode    = m_pAudioSettings->GetMixingMode();
  m_pAudioSettings->GetMixingLevels(&m_dwMixCenter, &m_dwMixSurround, &m_dwMixLFE);

  return hr;
}

INT_PTR CLAVAudioMixingProp::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  LRESULT lValue;
  switch (uMsg)
  {
  case WM_COMMAND:
    if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_OUTPUT_SPEAKERS) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), CB_GETCURSEL, 0, 0);
      if (dwSpkLayouts[lValue] != m_dwSpeakerLayout) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_MIXING && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue != m_bMixing) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_UNTOUCHED_STEREO && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue == !(m_dwFlags & LAV_MIXING_FLAG_UNTOUCHED_STEREO)) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_NORMALIZE_MATRIX && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue == !(m_dwFlags & LAV_MIXING_FLAG_NORMALIZE_MATRIX)) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_CLIP_PROTECTION && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue == !(m_dwFlags & LAV_MIXING_FLAG_CLIP_PROTECTION)) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_MIXMODE_NORMAL && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue != (m_dwMixingMode == MatrixEncoding_None)) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_MIXMODE_DOLBY && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue != (m_dwMixingMode == MatrixEncoding_Dolby)) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_MIXMODE_DPL2 && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue != (m_dwMixingMode == MatrixEncoding_DPLII)) {
        SetDirty();
      }
    }
    break;
  case WM_HSCROLL:
    if ((HWND)lParam == GetDlgItem(m_Dlg, IDC_MIX_LEVEL_CENTER)) {
      lValue = SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_CENTER, TBM_GETPOS, 0, 0);
      if (lValue != m_dwMixCenter) {
        SetDirty();
      }
      WCHAR buffer[10];
      _snwprintf_s(buffer, _TRUNCATE, L"%.2f", (double)lValue / 10000.0);
      SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_CENTER_TEXT, WM_SETTEXT, 0, (LPARAM)buffer);
    } else if ((HWND)lParam == GetDlgItem(m_Dlg, IDC_MIX_LEVEL_SURROUND)) {
      lValue = SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_SURROUND, TBM_GETPOS, 0, 0);
      if (lValue != m_dwMixSurround) {
        SetDirty();
      }
      WCHAR buffer[10];
      _snwprintf_s(buffer, _TRUNCATE, L"%.2f", (double)lValue / 10000.0);
      SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_SURROUND_TEXT, WM_SETTEXT, 0, (LPARAM)buffer);
    } else if ((HWND)lParam == GetDlgItem(m_Dlg, IDC_MIX_LEVEL_LFE)) {
      lValue = SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_LFE, TBM_GETPOS, 0, 0);
      if (lValue != m_dwMixLFE) {
        SetDirty();
      }
      WCHAR buffer[10];
      _snwprintf_s(buffer, _TRUNCATE, L"%.2f", (double)lValue / 10000.0);
      SendDlgItemMessage(m_Dlg, IDC_MIX_LEVEL_LFE_TEXT, WM_SETTEXT, 0, (LPARAM)buffer);
    }
    break;
  }
  // Let the parent class handle the message.
  return __super::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Format Configurations

CLAVAudioFormatsProp::CLAVAudioFormatsProp(LPUNKNOWN pUnk, HRESULT* phr)
  : CBaseDSPropPage(NAME("LAVCAudioFormats"), pUnk, IDD_PROPPAGE_FORMATS, IDS_FORMATS)
{
}


CLAVAudioFormatsProp::~CLAVAudioFormatsProp()
{
}

HRESULT CLAVAudioFormatsProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == nullptr)
  {
    return E_POINTER;
  }
  ASSERT(m_pAudioSettings == nullptr);
  return pUnk->QueryInterface(&m_pAudioSettings);
}

HRESULT CLAVAudioFormatsProp::OnDisconnect()
{
  SafeRelease(&m_pAudioSettings);
  return S_OK;
}


HRESULT CLAVAudioFormatsProp::OnApplyChanges()
{
  ASSERT(m_pAudioSettings != nullptr);
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
  ASSERT(m_pAudioSettings != nullptr);

  // Setup ListView control for format configuration
  SendDlgItemMessage(m_Dlg, IDC_CODECS, CCM_DPISCALE, TRUE, 0);

  HWND hlv = GetDlgItem(m_Dlg, IDC_CODECS);
  ListView_SetExtendedListViewStyle(hlv, LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);

  int nCol = 1;
  LVCOLUMN lvc = {LVCF_WIDTH, 0, 20, 0};
  ListView_InsertColumn(hlv, 0, &lvc);
  ListView_AddCol(hlv, nCol,  75, L"Codec", false);
  ListView_AddCol(hlv, nCol, 350, L"Description", false);

  ListView_DeleteAllItems(hlv);
  ListView_SetItemCount(hlv, Codec_AudioNB);

  // Create entries for the formats
  LVITEM lvi;
  memset(&lvi, 0, sizeof(lvi));
  lvi.mask = LVIF_TEXT|LVIF_PARAM;

  int nItem = 0;
  for (nItem = 0; nItem < Codec_AudioNB; ++nItem) {
    const codec_config_t *config = get_codec_config((LAVAudioCodec)nItem);

    // Create main entry
    lvi.iItem = nItem + 1;
    ListView_InsertItem(hlv, &lvi);

    // Set sub item texts
    ATL::CA2W name(config->name);
    ListView_SetItemText(hlv, nItem, 1, (LPWSTR)name);

    ATL::CA2W desc(config->description);
    ListView_SetItemText(hlv, nItem, 2, (LPWSTR)desc);
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
  for (unsigned i = 0; i < Codec_AudioNB; ++i)
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
  : CBaseDSPropPage(NAME("LAVCAudioStatusProp"), pUnk, IDD_PROPPAGE_STATUS, IDS_STATUS)
{
}


CLAVAudioStatusProp::~CLAVAudioStatusProp()
{
}

HRESULT CLAVAudioStatusProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == nullptr)
  {
    return E_POINTER;
  }
  ASSERT(m_pAudioStatus == nullptr);
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
  ASSERT(m_pAudioStatus != nullptr);

  m_nChannels = 0;

  const char *codec = nullptr;
  const char *decodeFormat = nullptr;
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

  const char *outputFormat = nullptr;
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

  SetTimer(m_Dlg, 1, 250, nullptr);
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
