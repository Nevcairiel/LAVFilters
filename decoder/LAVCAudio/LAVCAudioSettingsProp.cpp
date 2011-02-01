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
#include "LAVCAudioSettingsProp.h"

#include <Commctrl.h>

#include "resource.h"

// static constructor
CUnknown* WINAPI CLAVCAudioSettingsProp::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr)
{
  CLAVCAudioSettingsProp *propPage = new CLAVCAudioSettingsProp(pUnk);
  if (!propPage) {
    *phr = E_OUTOFMEMORY;
  }
  return propPage;
}

CLAVCAudioSettingsProp::CLAVCAudioSettingsProp(IUnknown *pUnk)
  : CBasePropertyPage(NAME("LAVCAudioProp"), pUnk, IDD_PROPPAGE_AUDIO_SETTINGS, IDS_SETTINGS), m_pAudioSettings(NULL)
{
}


CLAVCAudioSettingsProp::~CLAVCAudioSettingsProp()
{
}

HRESULT CLAVCAudioSettingsProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == NULL)
  {
    return E_POINTER;
  }
  ASSERT(m_pAudioSettings == NULL);
  return pUnk->QueryInterface(&m_pAudioSettings);
}

HRESULT CLAVCAudioSettingsProp::OnDisconnect()
{
  SafeRelease(&m_pAudioSettings);
  return S_OK;
}


HRESULT CLAVCAudioSettingsProp::OnApplyChanges()
{
  ASSERT(m_pAudioSettings != NULL);
  HRESULT hr = S_OK;
  DWORD dwVal;

  LoadData();

done:    
  return hr;
} 

HRESULT CLAVCAudioSettingsProp::OnActivate()
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

  return hr;
}

HRESULT CLAVCAudioSettingsProp::LoadData()
{
  HRESULT hr = S_OK;

done:
  return hr;
}

INT_PTR CLAVCAudioSettingsProp::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_COMMAND:
    // Mark the page dirty if an option changed
    if (IsPageDirty() != S_OK) {
      
    }
    break;
  }
  // Let the parent class handle the message.
  return CBasePropertyPage::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}


// static constructor
CUnknown* WINAPI CLAVCAudioStatusProp::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr)
{
  CLAVCAudioStatusProp *propPage = new CLAVCAudioStatusProp(pUnk);
  if (!propPage) {
    *phr = E_OUTOFMEMORY;
  }
  return propPage;
}

CLAVCAudioStatusProp::CLAVCAudioStatusProp(IUnknown *pUnk)
  : CBasePropertyPage(NAME("LAVCAudioStatusProp"), pUnk, IDD_PROPPAGE_STATUS, IDS_STATUS), m_pAudioSettings(NULL)
{
}


CLAVCAudioStatusProp::~CLAVCAudioStatusProp()
{
}

HRESULT CLAVCAudioStatusProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == NULL)
  {
    return E_POINTER;
  }
  ASSERT(m_pAudioSettings == NULL);
  return pUnk->QueryInterface(&m_pAudioSettings);
}

HRESULT CLAVCAudioStatusProp::OnDisconnect()
{
  SafeRelease(&m_pAudioSettings);
  return S_OK;
}

HRESULT CLAVCAudioStatusProp::OnActivate()
{
  HRESULT hr = S_OK;
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icc.dwICC = ICC_STANDARD_CLASSES;
  if (InitCommonControlsEx(&icc) == FALSE)
  {
    return E_FAIL;
  }
  ASSERT(m_pAudioSettings != NULL);

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
  hr = m_pAudioSettings->GetInputDetails(&codec, &nChannels, &nSampleRate);
  if (SUCCEEDED(hr)) {
    WCHAR buffer[100];
    _snwprintf_s(buffer, _TRUNCATE, L"%d", nChannels);
    SendDlgItemMessage(m_Dlg, IDC_CHANNELS, WM_SETTEXT, 0, (LPARAM)buffer);

    _snwprintf_s(buffer, _TRUNCATE, L"%d", nSampleRate);
    SendDlgItemMessage(m_Dlg, IDC_SAMPLE_RATE, WM_SETTEXT, 0, (LPARAM)buffer);

    _snwprintf_s(buffer, _TRUNCATE, L"%S", codec);
    SendDlgItemMessage(m_Dlg, IDC_CODEC, WM_SETTEXT, 0, (LPARAM)buffer);
  }

  SendDlgItemMessage(m_Dlg, IDC_INT8, BM_SETCHECK, m_pAudioSettings->IsSampleFormatSupported(SampleFormat_U8), 0);
  SendDlgItemMessage(m_Dlg, IDC_INT16, BM_SETCHECK, m_pAudioSettings->IsSampleFormatSupported(SampleFormat_16), 0);
  SendDlgItemMessage(m_Dlg, IDC_INT24, BM_SETCHECK, m_pAudioSettings->IsSampleFormatSupported(SampleFormat_24), 0);
  SendDlgItemMessage(m_Dlg, IDC_INT32, BM_SETCHECK, m_pAudioSettings->IsSampleFormatSupported(SampleFormat_32), 0);
  SendDlgItemMessage(m_Dlg, IDC_FP32, BM_SETCHECK, m_pAudioSettings->IsSampleFormatSupported(SampleFormat_FP32), 0);

  return hr;
}
