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
  BOOL bFlag;
  DWORD dwVal;
  
  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_STREAMAR, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetStreamAR(bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_INTERLACE_FLAGS, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetReportInterlacedFlags(bFlag);

  dwVal = (DWORD)SendDlgItemMessage(m_Dlg, IDC_THREADS, CB_GETCURSEL, 0, 0);
  m_pVideoSettings->SetNumThreads(dwVal);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_HQ, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetHighQualityPixelFormatConversion(bFlag);

  BOOL bPixFmts[LAVPixFmt_NB] = {0};
  bPixFmts[LAVPixFmt_YV12] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_YV12, BM_GETCHECK,0, 0);
  bPixFmts[LAVPixFmt_NV12] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_NV12, BM_GETCHECK,0, 0);
  bPixFmts[LAVPixFmt_P010] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_P010, BM_GETCHECK,0, 0);
  bPixFmts[LAVPixFmt_P016] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_P016, BM_GETCHECK,0, 0);
  bPixFmts[LAVPixFmt_YUY2] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_YUY2, BM_GETCHECK,0, 0);
  bPixFmts[LAVPixFmt_UYVY] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_UYVY, BM_GETCHECK,0, 0);
  bPixFmts[LAVPixFmt_P210] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_P210, BM_GETCHECK,0, 0);
  bPixFmts[LAVPixFmt_P216] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_P216, BM_GETCHECK,0, 0);
  bPixFmts[LAVPixFmt_AYUV] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_AYUV, BM_GETCHECK,0, 0);
  bPixFmts[LAVPixFmt_Y410] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_Y410, BM_GETCHECK,0, 0);
  bPixFmts[LAVPixFmt_Y416] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_Y416, BM_GETCHECK,0, 0);
  bPixFmts[LAVPixFmt_RGB32] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_RGB32, BM_GETCHECK,0, 0);
  bPixFmts[LAVPixFmt_RGB24] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_RGB24, BM_GETCHECK,0, 0);

  for (int i = 0; i < LAVPixFmt_NB; ++i) {
    m_pVideoSettings->SetPixelFormat((LAVVideoPixFmts)i, bPixFmts[i]);
  }

  BOOL bRGBAuto = (BOOL)SendDlgItemMessage(m_Dlg, IDC_RGBOUT_AUTO, BM_GETCHECK, 0, 0);
  BOOL bRGBTV = (BOOL)SendDlgItemMessage(m_Dlg, IDC_RGBOUT_TV, BM_GETCHECK, 0, 0);
  BOOL bRGBPC = (BOOL)SendDlgItemMessage(m_Dlg, IDC_RGBOUT_PC, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetRGBOutputRange(bRGBAuto ? 0 : bRGBTV ? 1 : 2);

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

  WCHAR stringBuffer[10] = L"Auto";

  // Init the Combo Box
  SendDlgItemMessage(m_Dlg, IDC_THREADS, CB_RESETCONTENT, 0, 0);
  SendDlgItemMessage(m_Dlg, IDC_THREADS, CB_ADDSTRING, 0, (LPARAM)stringBuffer);

  for (unsigned i = 1; i <= 16; ++i) {
    swprintf_s(stringBuffer, L"%d", i);
    SendDlgItemMessage(m_Dlg, IDC_THREADS, CB_ADDSTRING, 0, (LPARAM)stringBuffer);
  }

  addHint(IDC_THREADS, L"Enable Multi-Threading for codecs that support it.\nAuto will automatically use the maximum number of threads suitable for your CPU. Using 1 thread disables multi-threading.\n\nMT decoding is supported for H264, MPEG2, MPEG4, VP8, VP3/Theora, DV and HuffYUV");
  addHint(IDC_OUT_HQ, L"Enable High-Quality conversion for swscale.\nOnly swscale processing is affected, all other conversion are always high-quality.\nNote that this mode can be very slow!");

  hr = LoadData();
  if (SUCCEEDED(hr)) {
    SendDlgItemMessage(m_Dlg, IDC_STREAMAR, BM_SETCHECK, m_bStreamAR, 0);
    SendDlgItemMessage(m_Dlg, IDC_INTERLACE_FLAGS, BM_SETCHECK, m_bInterlaceFlags, 0);
    SendDlgItemMessage(m_Dlg, IDC_THREADS, CB_SETCURSEL, m_dwNumThreads, 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_HQ, BM_SETCHECK, m_bHighQualityPixelConv, 0);

    SendDlgItemMessage(m_Dlg, IDC_OUT_YV12, BM_SETCHECK, m_bPixFmts[LAVPixFmt_YV12], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_NV12, BM_SETCHECK, m_bPixFmts[LAVPixFmt_NV12], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_P010, BM_SETCHECK, m_bPixFmts[LAVPixFmt_P010], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_P016, BM_SETCHECK, m_bPixFmts[LAVPixFmt_P016], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_YUY2, BM_SETCHECK, m_bPixFmts[LAVPixFmt_YUY2], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_UYVY, BM_SETCHECK, m_bPixFmts[LAVPixFmt_UYVY], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_P210, BM_SETCHECK, m_bPixFmts[LAVPixFmt_P210], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_P216, BM_SETCHECK, m_bPixFmts[LAVPixFmt_P216], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_AYUV, BM_SETCHECK, m_bPixFmts[LAVPixFmt_AYUV], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_Y410, BM_SETCHECK, m_bPixFmts[LAVPixFmt_Y410], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_Y416, BM_SETCHECK, m_bPixFmts[LAVPixFmt_Y416], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_RGB32, BM_SETCHECK, m_bPixFmts[LAVPixFmt_RGB32], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_RGB24, BM_SETCHECK, m_bPixFmts[LAVPixFmt_RGB24], 0);

    SendDlgItemMessage(m_Dlg, IDC_RGBOUT_AUTO, BM_SETCHECK, (m_dwRGBOutput == 0), 0);
    SendDlgItemMessage(m_Dlg, IDC_RGBOUT_TV, BM_SETCHECK, (m_dwRGBOutput == 1), 0);
    SendDlgItemMessage(m_Dlg, IDC_RGBOUT_PC, BM_SETCHECK, (m_dwRGBOutput == 2), 0);
  }

  return hr;
}

HRESULT CLAVVideoSettingsProp::LoadData()
{
  HRESULT hr = S_OK;
  
  m_dwNumThreads    = m_pVideoSettings->GetNumThreads();
  m_bStreamAR       = m_pVideoSettings->GetStreamAR();
  m_bInterlaceFlags = m_pVideoSettings->GetReportInterlacedFlags();
  m_bHighQualityPixelConv = m_pVideoSettings->GetHighQualityPixelFormatConversion();
  m_dwRGBOutput     = m_pVideoSettings->GetRGBOutputRange();

  for (int i = 0; i < LAVPixFmt_NB; ++i) {
    m_bPixFmts[i] = m_pVideoSettings->GetPixelFormat((LAVVideoPixFmts)i);
  }

  return hr;
}

INT_PTR CLAVVideoSettingsProp::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  LRESULT lValue;
  BOOL bValue;
  switch (uMsg)
  {
  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_STREAMAR && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue != m_bStreamAR) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_INTERLACE_FLAGS && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue != m_bInterlaceFlags) {
        SetDirty();
      }
    } else if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_THREADS) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), CB_GETCURSEL, 0, 0);
      if (lValue != m_dwNumThreads) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_HQ && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue != m_bHighQualityPixelConv) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_YV12 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVPixFmt_YV12]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_NV12 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVPixFmt_NV12]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_P010 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVPixFmt_P010]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_P016 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVPixFmt_P016]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_YUY2 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVPixFmt_YUY2]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_UYVY && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVPixFmt_UYVY]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_P210 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVPixFmt_P210]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_P216 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVPixFmt_P216]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_AYUV && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVPixFmt_AYUV]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_Y410 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVPixFmt_Y410]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_Y416 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVPixFmt_Y416]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_RGB32 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVPixFmt_RGB32]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_RGB24 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVPixFmt_RGB24]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_RGBOUT_AUTO && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue != (m_dwRGBOutput == 0)) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_RGBOUT_TV && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue != (m_dwRGBOutput == 1)) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_RGBOUT_PC && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue != (m_dwRGBOutput == 2)) {
        SetDirty();
      }
    }
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

  // Get checked state
  BOOL bFlag;
  for (int nItem = 0; nItem < ListView_GetItemCount(hlv); nItem++) {
    bFlag = ListView_GetCheckState(hlv, nItem);
    m_pVideoSettings->SetFormatConfiguration((LAVVideoCodec)nItem, bFlag);
  }

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
  ListView_SetItemCount(hlv, Codec_NB);

  // Create entrys for the formats
  LVITEM lvi;
  memset(&lvi, 0, sizeof(lvi));
  lvi.mask = LVIF_TEXT|LVIF_PARAM;

  int nItem = 0;
  for (nItem = 0; nItem < Codec_NB; ++nItem) {
    const codec_config_t *config = get_codec_config((LAVVideoCodec)nItem);

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

HRESULT CLAVVideoFormatsProp::LoadData()
{
  HRESULT hr = S_OK;
  
 for (unsigned i = 0; i < Codec_NB; ++i)
    m_bFormats[i] = m_pVideoSettings->GetFormatConfiguration((LAVVideoCodec)i) != 0;

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
        BOOL check = ListView_GetCheckState(hdr->hwndFrom, nmlv->iItem);
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
