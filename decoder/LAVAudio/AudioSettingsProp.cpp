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

#include <Commctrl.h>

#include "resource.h"

// static constructor
CUnknown* WINAPI CLAVAudioSettingsProp::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr)
{
  CLAVAudioSettingsProp *propPage = new CLAVAudioSettingsProp(pUnk);
  if (!propPage) {
    *phr = E_OUTOFMEMORY;
  }
  return propPage;
}

CLAVAudioSettingsProp::CLAVAudioSettingsProp(IUnknown *pUnk)
  : CBasePropertyPage(NAME("LAVCAudioProp"), pUnk, IDD_PROPPAGE_AUDIO_SETTINGS, IDS_SETTINGS), m_pAudioSettings(NULL), m_bDRCEnabled(FALSE), m_iDRCLevel(100)
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

  int iDRCLevel = SendDlgItemMessage(m_Dlg, IDC_DRC_LEVEL, TBM_GETPOS, 0, 0);
  BOOL bDRC = SendDlgItemMessage(m_Dlg, IDC_DRC, BM_GETCHECK, 0, 0);
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
  switch (uMsg)
  {
  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_DRC && HIWORD(wParam) == BN_CLICKED) {
      BOOL bDRC = SendDlgItemMessage(m_Dlg, IDC_DRC, BM_GETCHECK, 0, 0);
      if (bDRC != m_bDRCEnabled) {
        SetDirty();
        EnableWindow(GetDlgItem(m_Dlg, IDC_DRC_LEVEL), bDRC);
      }
    }
    break;
  case WM_HSCROLL:
    int lValue = SendDlgItemMessage(m_Dlg, IDC_DRC_LEVEL,TBM_GETPOS, 0, 0);
    if (lValue != m_iDRCLevel) {
      SetDirty();
    }
    WCHAR buffer[10];
    _snwprintf_s(buffer, _TRUNCATE, L"%d%%", lValue);
    SendDlgItemMessage(m_Dlg, IDC_DRC_LEVEL_TEXT, WM_SETTEXT, 0, (LPARAM)buffer);
    break;
  }
  // Let the parent class handle the message.
  return CBasePropertyPage::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}


// static constructor
CUnknown* WINAPI CLAVAudioStatusProp::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr)
{
  CLAVAudioStatusProp *propPage = new CLAVAudioStatusProp(pUnk);
  if (!propPage) {
    *phr = E_OUTOFMEMORY;
  }
  return propPage;
}

CLAVAudioStatusProp::CLAVAudioStatusProp(IUnknown *pUnk)
  : CBasePropertyPage(NAME("LAVCAudioStatusProp"), pUnk, IDD_PROPPAGE_STATUS, IDS_STATUS), m_pAudioStatus(NULL)
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
  icc.dwICC = ICC_STANDARD_CLASSES;
  if (InitCommonControlsEx(&icc) == FALSE)
  {
    return E_FAIL;
  }
  ASSERT(m_pAudioStatus != NULL);

  // Set CPU Flag buttons
  int flags = av_get_cpu_flags();
  SendDlgItemMessage(m_Dlg, IDC_CPU_MMX, BM_SETCHECK, flags & AV_CPU_FLAG_MMX, 0);
  SendDlgItemMessage(m_Dlg, IDC_CPU_MMX2, BM_SETCHECK, flags & AV_CPU_FLAG_MMX2, 0);
  SendDlgItemMessage(m_Dlg, IDC_CPU_3DNOW, BM_SETCHECK, flags & AV_CPU_FLAG_3DNOW, 0);
  SendDlgItemMessage(m_Dlg, IDC_CPU_3DNOWEXT, BM_SETCHECK, flags & AV_CPU_FLAG_3DNOWEXT, 0);
  SendDlgItemMessage(m_Dlg, IDC_CPU_SSE, BM_SETCHECK, flags & AV_CPU_FLAG_SSE, 0);
  SendDlgItemMessage(m_Dlg, IDC_CPU_SSE2, BM_SETCHECK, flags & AV_CPU_FLAG_SSE2, 0);
  SendDlgItemMessage(m_Dlg, IDC_CPU_SSE3, BM_SETCHECK, flags & AV_CPU_FLAG_SSE3, 0);
  SendDlgItemMessage(m_Dlg, IDC_CPU_SSSE3, BM_SETCHECK, flags & AV_CPU_FLAG_SSSE3, 0);
  SendDlgItemMessage(m_Dlg, IDC_CPU_SSE41, BM_SETCHECK, flags & AV_CPU_FLAG_SSE4, 0);
  SendDlgItemMessage(m_Dlg, IDC_CPU_SSE42, BM_SETCHECK, flags & AV_CPU_FLAG_SSE42, 0);

  const char *codec = NULL;
  int nChannels;
  int nSampleRate;
  hr = m_pAudioStatus->GetInputDetails(&codec, &nChannels, &nSampleRate);
  if (SUCCEEDED(hr)) {
    WCHAR buffer[100];
    _snwprintf_s(buffer, _TRUNCATE, L"%d", nChannels);
    SendDlgItemMessage(m_Dlg, IDC_CHANNELS, WM_SETTEXT, 0, (LPARAM)buffer);

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
  DWORD dwChannelMask;
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

  return hr;
}
