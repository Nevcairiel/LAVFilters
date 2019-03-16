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
#include "VideoSettingsProp.h"
#include "Media.h"

#include <Commctrl.h>

#include "resource.h"
#include "version.h"

CLAVVideoSettingsProp::CLAVVideoSettingsProp(LPUNKNOWN pUnk, HRESULT* phr)
  : CBaseDSPropPage(NAME("LAVVideoProp"), pUnk, IDD_PROPPAGE_VIDEO_SETTINGS, IDS_SETTINGS)
{
}

CLAVVideoSettingsProp::~CLAVVideoSettingsProp()
{
}

HRESULT CLAVVideoSettingsProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == nullptr)
  {
    return E_POINTER;
  }
  ASSERT(m_pVideoSettings == nullptr);
  HRESULT hr = pUnk->QueryInterface(&m_pVideoSettings);
  if (FAILED(hr))
    return hr;
  hr =  pUnk->QueryInterface(&m_pVideoStatus);
  if (FAILED(hr))
    return hr;
  return S_OK;
}

HRESULT CLAVVideoSettingsProp::OnDisconnect()
{
  SafeRelease(&m_pVideoSettings);
  SafeRelease(&m_pVideoStatus);
  return S_OK;
}

HRESULT CLAVVideoSettingsProp::OnApplyChanges()
{
  ASSERT(m_pVideoSettings != nullptr);
  ASSERT(m_pVideoStatus != nullptr);
  HRESULT hr = S_OK;
  BOOL bFlag;
  DWORD dwVal;
  
  dwVal = (DWORD)SendDlgItemMessage(m_Dlg, IDC_STREAMAR, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetStreamAR(dwVal);

  dwVal = (DWORD)SendDlgItemMessage(m_Dlg, IDC_THREADS, CB_GETCURSEL, 0, 0);
  m_pVideoSettings->SetNumThreads(dwVal);

  dwVal = (DWORD)SendDlgItemMessage(m_Dlg, IDC_DEINT_FIELDORDER, CB_GETCURSEL, 0, 0);
  m_pVideoSettings->SetDeintFieldOrder((LAVDeintFieldOrder)dwVal);

  dwVal = (DWORD)SendDlgItemMessage(m_Dlg, IDC_DEINT_MODE, CB_GETCURSEL, 0, 0);
  m_pVideoSettings->SetDeinterlacingMode((LAVDeintMode)dwVal);

  m_bPixFmts[LAVOutPixFmt_YV12] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_YV12, BM_GETCHECK,0, 0);
  m_bPixFmts[LAVOutPixFmt_NV12] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_NV12, BM_GETCHECK,0, 0);
  m_bPixFmts[LAVOutPixFmt_P010] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_P010, BM_GETCHECK,0, 0);
  m_bPixFmts[LAVOutPixFmt_P016] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_P016, BM_GETCHECK,0, 0);
  m_bPixFmts[LAVOutPixFmt_YUY2] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_YUY2, BM_GETCHECK,0, 0);
  m_bPixFmts[LAVOutPixFmt_UYVY] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_UYVY, BM_GETCHECK,0, 0);
  m_bPixFmts[LAVOutPixFmt_P210] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_P210, BM_GETCHECK,0, 0);
  m_bPixFmts[LAVOutPixFmt_v210] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_V210, BM_GETCHECK,0, 0);
  m_bPixFmts[LAVOutPixFmt_P216] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_P216, BM_GETCHECK,0, 0);
  m_bPixFmts[LAVOutPixFmt_YV24] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_YV24, BM_GETCHECK,0, 0);
  m_bPixFmts[LAVOutPixFmt_AYUV] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_AYUV, BM_GETCHECK,0, 0);
  m_bPixFmts[LAVOutPixFmt_Y410] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_Y410, BM_GETCHECK,0, 0);
  m_bPixFmts[LAVOutPixFmt_v410] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_V410, BM_GETCHECK,0, 0);
  m_bPixFmts[LAVOutPixFmt_Y416] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_Y416, BM_GETCHECK,0, 0);
  m_bPixFmts[LAVOutPixFmt_RGB32] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_RGB32, BM_GETCHECK,0, 0);
  m_bPixFmts[LAVOutPixFmt_RGB24] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_RGB24, BM_GETCHECK,0, 0);
  m_bPixFmts[LAVOutPixFmt_RGB48] = (BOOL)SendDlgItemMessage(m_Dlg, IDC_OUT_RGB48, BM_GETCHECK,0, 0);

  for (int i = 0; i < LAVOutPixFmt_NB; ++i) {
    m_pVideoSettings->SetPixelFormat((LAVOutPixFmts)i, m_bPixFmts[i]);
  }

  BOOL bRGBAuto = (BOOL)SendDlgItemMessage(m_Dlg, IDC_RGBOUT_AUTO, BM_GETCHECK, 0, 0);
  BOOL bRGBTV = (BOOL)SendDlgItemMessage(m_Dlg, IDC_RGBOUT_TV, BM_GETCHECK, 0, 0);
  BOOL bRGBPC = (BOOL)SendDlgItemMessage(m_Dlg, IDC_RGBOUT_PC, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetRGBOutputRange(bRGBAuto ? 0 : bRGBTV ? 1 : 2);

  dwVal = (DWORD)SendDlgItemMessage(m_Dlg, IDC_HWACCEL, CB_GETCURSEL, 0, 0);
  m_pVideoSettings->SetHWAccel((LAVHWAccel)dwVal);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWACCEL_H264, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetHWAccelCodec(HWCodec_H264, bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWACCEL_VC1, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetHWAccelCodec(HWCodec_VC1, bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWACCEL_MPEG2, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetHWAccelCodec(HWCodec_MPEG2, bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWACCEL_MPEG4, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetHWAccelCodec(HWCodec_MPEG4, bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWACCEL_MPEG2_DVD, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetHWAccelCodec(HWCodec_MPEG2DVD, bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWACCEL_HEVC, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetHWAccelCodec(HWCodec_HEVC, bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWACCEL_VP9, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetHWAccelCodec(HWCodec_VP9, bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWACCEL_H264MVC, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetHWAccelCodec(HWCodec_H264MVC, bFlag);

  DWORD dwHWResFlags = 0;
  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWRES_SD, BM_GETCHECK, 0, 0);
  if (bFlag) dwHWResFlags |= LAVHWResFlag_SD;

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWRES_HD, BM_GETCHECK, 0, 0);
  if (bFlag) dwHWResFlags |= LAVHWResFlag_HD;

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWRES_UHD, BM_GETCHECK, 0, 0);
  if (bFlag) dwHWResFlags |= LAVHWResFlag_UHD;

  m_pVideoSettings->SetHWAccelResolutionFlags(dwHWResFlags);

  dwVal = (DWORD)SendDlgItemMessage(m_Dlg, IDC_HWACCEL_DEVICE_SELECT, CB_GETCURSEL, 0, 0);
  if (dwVal == 0)
    dwVal = LAVHWACCEL_DEVICE_DEFAULT;
  else
    dwVal--;
  m_pVideoSettings->SetHWAccelDeviceIndex(m_pVideoSettings->GetHWAccel(), dwVal, 0);

  BOOL bHWAccelCUVIDDXVA = (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWACCEL_CUVID_DXVA, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetHWAccelDeintHQ(bHWAccelCUVIDDXVA);

  BOOL bHWDeint = (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWDEINT_ENABLE, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetHWAccelDeintMode(bHWDeint ? HWDeintMode_Hardware : HWDeintMode_Weave);

  BOOL bFilm = (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWDEINT_OUT_FILM, BM_GETCHECK, 0, 0);
  //BOOL bVideo = (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWDEINT_OUT_VIDEO, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetHWAccelDeintOutput(bFilm ? DeintOutput_FramePer2Field : DeintOutput_FramePerField);

  dwVal = (DWORD)SendDlgItemMessage(m_Dlg, IDC_SWDEINT_MODE, CB_GETCURSEL, 0, 0);
  m_pVideoSettings->SetSWDeintMode((LAVSWDeintModes)dwVal);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_SWDEINT_OUT_FILM, BM_GETCHECK, 0, 0);
  //BOOL bVideo = (BOOL)SendDlgItemMessage(m_Dlg, IDC_SWDEINT_OUT_VIDEO, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetSWDeintOutput(bFlag ? DeintOutput_FramePer2Field : DeintOutput_FramePerField);

  BOOL bOrdered = (BOOL)SendDlgItemMessage(m_Dlg, IDC_DITHER_ORDERED, BM_GETCHECK, 0, 0);
  BOOL bRandom = (BOOL)SendDlgItemMessage(m_Dlg, IDC_DITHER_RANDOM, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetDitherMode(bOrdered ? LAVDither_Ordered : LAVDither_Random);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_TRAYICON, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetTrayIcon(bFlag);

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
  ASSERT(m_pVideoSettings != nullptr);

  const WCHAR *version = TEXT(LAV_VIDEO) L" " TEXT(LAV_VERSION_STR);
  SendDlgItemMessage(m_Dlg, IDC_LAVVIDEO_FOOTER, WM_SETTEXT, 0, (LPARAM)version);

  WCHAR stringBuffer[512] = L"Auto";

  // Init the Combo Box
  SendDlgItemMessage(m_Dlg, IDC_THREADS, CB_RESETCONTENT, 0, 0);
  SendDlgItemMessage(m_Dlg, IDC_THREADS, CB_ADDSTRING, 0, (LPARAM)stringBuffer);

  for (unsigned i = 1; i <= 32; ++i) {
    swprintf_s(stringBuffer, L"%d", i);
    SendDlgItemMessage(m_Dlg, IDC_THREADS, CB_ADDSTRING, 0, (LPARAM)stringBuffer);
  }

  addHint(IDC_THREADS, L"Enable Multi-Threading for codecs that support it.\nAuto will automatically use the maximum number of threads suitable for your CPU. Using 1 thread disables multi-threading.\n\nMT decoding is supported for H264, MPEG2, MPEG4, VP8, VP3/Theora, DV and HuffYUV");

  addHint(IDC_STREAMAR, L"Checked - Stream AR will be used.\nUnchecked - Frame AR will not be used.\nIndeterminate (Auto) - Stream AR will not be used on files with a container AR (recommended).");

  WCHAR hwAccelNone[] = L"None";
  WCHAR hwAccelCUDA[] = L"NVIDIA CUVID";
  WCHAR hwAccelQuickSync[] = L"Intel\xae QuickSync";
  WCHAR hwAccelDXVA2CB[] = L"DXVA2 (copy-back)";
  WCHAR hwAccelDXVA2N[] = L"DXVA2 (native)";
  WCHAR hwAccelD3D11[] = L"D3D11";
  SendDlgItemMessage(m_Dlg, IDC_HWACCEL, CB_ADDSTRING, 0, (LPARAM)hwAccelNone);
  SendDlgItemMessage(m_Dlg, IDC_HWACCEL, CB_ADDSTRING, 0, (LPARAM)hwAccelCUDA);
  SendDlgItemMessage(m_Dlg, IDC_HWACCEL, CB_ADDSTRING, 0, (LPARAM)hwAccelQuickSync);
  SendDlgItemMessage(m_Dlg, IDC_HWACCEL, CB_ADDSTRING, 0, (LPARAM)hwAccelDXVA2CB);
  SendDlgItemMessage(m_Dlg, IDC_HWACCEL, CB_ADDSTRING, 0, (LPARAM)hwAccelDXVA2N);
  SendDlgItemMessage(m_Dlg, IDC_HWACCEL, CB_ADDSTRING, 0, (LPARAM)hwAccelD3D11);

  // Init the fieldorder Combo Box
  SendDlgItemMessage(m_Dlg, IDC_DEINT_FIELDORDER, CB_RESETCONTENT, 0, 0);
  WideStringFromResource(stringBuffer, IDS_FIELDORDER_AUTO);
  SendDlgItemMessage(m_Dlg, IDC_DEINT_FIELDORDER, CB_ADDSTRING, 0, (LPARAM)stringBuffer);
  WideStringFromResource(stringBuffer, IDS_FIELDORDER_TOP);
  SendDlgItemMessage(m_Dlg, IDC_DEINT_FIELDORDER, CB_ADDSTRING, 0, (LPARAM)stringBuffer);
  WideStringFromResource(stringBuffer, IDS_FIELDORDER_BOTTOM);
  SendDlgItemMessage(m_Dlg, IDC_DEINT_FIELDORDER, CB_ADDSTRING, 0, (LPARAM)stringBuffer);

  // Deint Mode combo box
  SendDlgItemMessage(m_Dlg, IDC_DEINT_MODE, CB_RESETCONTENT, 0, 0);
  WideStringFromResource(stringBuffer, IDS_DEINTMODE_AUTO);
  SendDlgItemMessage(m_Dlg, IDC_DEINT_MODE, CB_ADDSTRING, 0, (LPARAM)stringBuffer);
  WideStringFromResource(stringBuffer, IDS_DEINTMODE_AGGRESSIVE);
  SendDlgItemMessage(m_Dlg, IDC_DEINT_MODE, CB_ADDSTRING, 0, (LPARAM)stringBuffer);
  WideStringFromResource(stringBuffer, IDS_DEINTMODE_FORCE);
  SendDlgItemMessage(m_Dlg, IDC_DEINT_MODE, CB_ADDSTRING, 0, (LPARAM)stringBuffer);
  WideStringFromResource(stringBuffer, IDS_DEINTMODE_DISABLE);
  SendDlgItemMessage(m_Dlg, IDC_DEINT_MODE, CB_ADDSTRING, 0, (LPARAM)stringBuffer);

  // SW Deint Mode
  WCHAR swdeintNone[] = L"No Software Deinterlacing";
  WCHAR swdeintYADIF[] = L"YADIF";
  WCHAR swdeintW3FDIFS[] = L"Weston Three Field (Simple)";
  WCHAR swdeintW3FDIFC[] = L"Weston Three Field (Complex)";
  SendDlgItemMessage(m_Dlg, IDC_SWDEINT_MODE, CB_ADDSTRING, 0, (LPARAM)swdeintNone);
  SendDlgItemMessage(m_Dlg, IDC_SWDEINT_MODE, CB_ADDSTRING, 0, (LPARAM)swdeintYADIF);
  SendDlgItemMessage(m_Dlg, IDC_SWDEINT_MODE, CB_ADDSTRING, 0, (LPARAM)swdeintW3FDIFS);
  SendDlgItemMessage(m_Dlg, IDC_SWDEINT_MODE, CB_ADDSTRING, 0, (LPARAM)swdeintW3FDIFC);

  addHint(IDC_HWACCEL_MPEG4, L"EXPERIMENTAL! The MPEG4-ASP decoder is known to be unstable! Use at your own peril!");
  addHint(IDC_HWACCEL_CUVID_DXVA, L"Enable DXVA video processing for CUVID decoding, enables hybrid decoding and can affect deinterlacing quality.\n\nNote: Using DXVA2-CopyBack is recommended for hybrid decoding instead of using CUVID in DXVA mode.");

  addHint(IDC_HWRES_SD, L"Use Hardware Decoding for Standard-definition content (DVD, SDTV)\n\nThis affects all videos with a resolution less than 1024x576 (DVD resolution)");
  addHint(IDC_HWRES_HD, L"Use Hardware Decoding for High-definition content (Blu-ray, HDTV)\n\nAffects all videos above SD resolution, up to Full-HD, 1920x1200");
  addHint(IDC_HWRES_UHD, L"Use Hardware Decoding for Ultra-high-definition content (4K, UHDTV)\n\nAffects all videos above HD resolution. Note that not all hardware supports decoding 4K/UHD content. On AMD GPUs, 4K support is very fragile, and may even cause crashes or BSODs, use at your own risk.");

  addHint(IDC_DEINT_MODE, L"Controls how interlaced material is handled.\n\nAuto: Frame flags are used to determine content type.\nAggressive: All frames in an interlaced streams are handled interlaced.\nForce: All frames are handles as interlaced.\nDisabled: All frames are handled as progressive.");

  addHint(IDC_HWDEINT_OUT_FILM, L"Deinterlace in \"Film\" Mode.\nFor every pair of interlaced fields, one frame will be created, resulting in 25/30 fps.");
  addHint(IDC_HWDEINT_OUT_VIDEO, L"Deinterlace in \"Video\" Mode. (Recommended)\nFor every interlaced field, one frame will be created, resulting in 50/60 fps.");

  addHint(IDC_DITHER_ORDERED, L"Ordered Dithering uses a static pattern, resulting in very smooth and regular pattern. However, in some cases the regular pattern can be visible and distracting.");
  addHint(IDC_DITHER_RANDOM, L"Random Dithering uses random noise to dither the video frames. This has the advantage of not creating any visible pattern, at the downside of increasing the noise floor slightly.");

  hr = LoadData();
  if (SUCCEEDED(hr)) {
    SendDlgItemMessage(m_Dlg, IDC_THREADS, CB_SETCURSEL, m_dwNumThreads, 0);

    SendDlgItemMessage(m_Dlg, IDC_STREAMAR, BM_SETCHECK, m_StreamAR, 0);

    SendDlgItemMessage(m_Dlg, IDC_DEINT_FIELDORDER, CB_SETCURSEL, m_DeintFieldOrder, 0);
    SendDlgItemMessage(m_Dlg, IDC_DEINT_MODE, CB_SETCURSEL, m_DeintMode, 0);

    SendDlgItemMessage(m_Dlg, IDC_OUT_YV12, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_YV12], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_NV12, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_NV12], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_P010, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_P010], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_P016, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_P016], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_YUY2, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_YUY2], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_UYVY, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_UYVY], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_P210, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_P210], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_V210, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_v210], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_P216, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_P216], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_YV24, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_YV24], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_AYUV, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_AYUV], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_Y410, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_Y410], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_V410, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_v410], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_Y416, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_Y416], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_RGB32, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_RGB32], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_RGB24, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_RGB24], 0);
    SendDlgItemMessage(m_Dlg, IDC_OUT_RGB48, BM_SETCHECK, m_bPixFmts[LAVOutPixFmt_RGB48], 0);

    SendDlgItemMessage(m_Dlg, IDC_RGBOUT_AUTO, BM_SETCHECK, (m_dwRGBOutput == 0), 0);
    SendDlgItemMessage(m_Dlg, IDC_RGBOUT_TV, BM_SETCHECK, (m_dwRGBOutput == 1), 0);
    SendDlgItemMessage(m_Dlg, IDC_RGBOUT_PC, BM_SETCHECK, (m_dwRGBOutput == 2), 0);

    SendDlgItemMessage(m_Dlg, IDC_HWACCEL, CB_SETCURSEL, m_HWAccel, 0);
    SendDlgItemMessage(m_Dlg, IDC_HWACCEL_H264, BM_SETCHECK, m_HWAccelCodecs[HWCodec_H264], 0);
    SendDlgItemMessage(m_Dlg, IDC_HWACCEL_VC1, BM_SETCHECK, m_HWAccelCodecs[HWCodec_VC1], 0);
    SendDlgItemMessage(m_Dlg, IDC_HWACCEL_MPEG2, BM_SETCHECK, m_HWAccelCodecs[HWCodec_MPEG2], 0);
    SendDlgItemMessage(m_Dlg, IDC_HWACCEL_MPEG4, BM_SETCHECK, m_HWAccelCodecs[HWCodec_MPEG4], 0);
    SendDlgItemMessage(m_Dlg, IDC_HWACCEL_MPEG2_DVD, BM_SETCHECK, m_HWAccelCodecs[HWCodec_MPEG2DVD], 0);
    SendDlgItemMessage(m_Dlg, IDC_HWACCEL_HEVC, BM_SETCHECK, m_HWAccelCodecs[HWCodec_HEVC], 0);
    SendDlgItemMessage(m_Dlg, IDC_HWACCEL_VP9, BM_SETCHECK, m_HWAccelCodecs[HWCodec_VP9], 0);
    SendDlgItemMessage(m_Dlg, IDC_HWACCEL_H264MVC, BM_SETCHECK, m_HWAccelCodecs[HWCodec_H264MVC], 0);

    SendDlgItemMessage(m_Dlg, IDC_HWRES_SD, BM_SETCHECK, !!(m_HWRes & LAVHWResFlag_SD), 0);
    SendDlgItemMessage(m_Dlg, IDC_HWRES_HD, BM_SETCHECK, !!(m_HWRes & LAVHWResFlag_HD), 0);
    SendDlgItemMessage(m_Dlg, IDC_HWRES_UHD, BM_SETCHECK, !!(m_HWRes & LAVHWResFlag_UHD), 0);

    SendDlgItemMessage(m_Dlg, IDC_HWACCEL_CUVID_DXVA, BM_SETCHECK, m_HWAccelCUVIDDXVA, 0);

    SendDlgItemMessage(m_Dlg, IDC_HWDEINT_ENABLE, BM_SETCHECK, (m_HWDeintAlgo == HWDeintMode_Hardware), 0);

    SendDlgItemMessage(m_Dlg, IDC_HWDEINT_OUT_FILM, BM_SETCHECK, (m_HWDeintOutMode == DeintOutput_FramePer2Field), 0);
    SendDlgItemMessage(m_Dlg, IDC_HWDEINT_OUT_VIDEO, BM_SETCHECK, (m_HWDeintOutMode == DeintOutput_FramePerField), 0);

    SendDlgItemMessage(m_Dlg, IDC_SWDEINT_MODE, CB_SETCURSEL, m_SWDeint, 0);
    SendDlgItemMessage(m_Dlg, IDC_SWDEINT_OUT_FILM, BM_SETCHECK, (m_SWDeintOutMode == DeintOutput_FramePer2Field), 0);
    SendDlgItemMessage(m_Dlg, IDC_SWDEINT_OUT_VIDEO, BM_SETCHECK, (m_SWDeintOutMode == DeintOutput_FramePerField), 0);

    SendDlgItemMessage(m_Dlg, IDC_DITHER_ORDERED, BM_SETCHECK, (m_DitherMode == LAVDither_Ordered), 0);
    SendDlgItemMessage(m_Dlg, IDC_DITHER_RANDOM, BM_SETCHECK, (m_DitherMode == LAVDither_Random), 0);

    SendDlgItemMessage(m_Dlg, IDC_TRAYICON, BM_SETCHECK, m_TrayIcon, 0);

    UpdateHWOptions();
    UpdateYADIFOptions();
  }

  const WCHAR *decoder = m_pVideoStatus->GetActiveDecoderName();
  SendDlgItemMessage(m_Dlg, IDC_ACTIVE_DECODER, WM_SETTEXT, 0, (LPARAM)(decoder ? decoder : L"<inactive>"));

  BSTR bstrHWDevice = nullptr;
  if (SUCCEEDED(m_pVideoStatus->GetHWAccelActiveDevice(&bstrHWDevice))) {
    SendDlgItemMessage(m_Dlg, IDC_HWACCEL_DEVICE, WM_SETTEXT, 0, (LPARAM)bstrHWDevice);
    SysFreeString(bstrHWDevice);
  }
  else {
    SendDlgItemMessage(m_Dlg, IDC_HWACCEL_DEVICE, WM_SETTEXT, 0, (LPARAM)L"<none>");
  }

  return hr;
}

HRESULT CLAVVideoSettingsProp::UpdateHWOptions()
{
  LAVHWAccel hwAccel = (LAVHWAccel)SendDlgItemMessage(m_Dlg, IDC_HWACCEL, CB_GETCURSEL, 0, 0);

  DWORD dwSupport = m_pVideoSettings->CheckHWAccelSupport(hwAccel);
  BOOL bEnabled = (hwAccel != HWAccel_None) && dwSupport;
  BOOL bHWDeint = bEnabled && (hwAccel == HWAccel_CUDA || hwAccel == HWAccel_QuickSync);
  BOOL bHWDeintEnabled = bHWDeint && (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWDEINT_ENABLE, BM_GETCHECK, 0, 0);
  BOOL bCUDAOnly = bEnabled && (hwAccel == HWAccel_CUDA);
  BOOL bDVD = bEnabled && (BOOL)SendDlgItemMessage(m_Dlg, IDC_HWACCEL_MPEG2, BM_GETCHECK, 0, 0);
  BOOL bHEVC = bEnabled && (hwAccel != HWAccel_QuickSync);
  BOOL bVP9 = bEnabled && (hwAccel != HWAccel_QuickSync);

  ShowWindow(GetDlgItem(m_Dlg, IDC_HWACCEL_CUVID_DXVA), (hwAccel == HWAccel_CUDA && !IsWindows10OrNewer()) ? SW_SHOW : SW_HIDE);

  EnableWindow(GetDlgItem(m_Dlg, IDC_HWACCEL_H264), bEnabled);
  EnableWindow(GetDlgItem(m_Dlg, IDC_HWACCEL_VC1), bEnabled);
  EnableWindow(GetDlgItem(m_Dlg, IDC_HWACCEL_MPEG2), bEnabled);
  EnableWindow(GetDlgItem(m_Dlg, IDC_HWACCEL_MPEG2_DVD), bDVD);

  EnableWindow(GetDlgItem(m_Dlg, IDC_HWACCEL_MPEG4), bCUDAOnly);
  EnableWindow(GetDlgItem(m_Dlg, IDC_HWACCEL_HEVC), bHEVC);
  EnableWindow(GetDlgItem(m_Dlg, IDC_HWACCEL_VP9), bVP9);

  EnableWindow(GetDlgItem(m_Dlg, IDC_HWRES_SD), bEnabled);
  EnableWindow(GetDlgItem(m_Dlg, IDC_HWRES_HD), bEnabled);
  EnableWindow(GetDlgItem(m_Dlg, IDC_HWRES_UHD), bEnabled);

  EnableWindow(GetDlgItem(m_Dlg, IDC_HWDEINT_ENABLE), bHWDeint);
  EnableWindow(GetDlgItem(m_Dlg, IDC_LBL_HWDEINT_MODE), bHWDeintEnabled);
  EnableWindow(GetDlgItem(m_Dlg, IDC_HWDEINT_OUT_FILM), bHWDeintEnabled);
  EnableWindow(GetDlgItem(m_Dlg, IDC_HWDEINT_OUT_VIDEO), bHWDeintEnabled);

  WCHAR hwAccelEmpty[] = L"";
  WCHAR hwAccelUnavailable[] = L"N/A";
  WCHAR hwAccelAvailable[]   = L"OK";

  SendDlgItemMessage(m_Dlg, IDC_HWACCEL_AVAIL, WM_SETTEXT, 0, (LPARAM)(hwAccel == HWAccel_None ? hwAccelEmpty : dwSupport == 0 ? hwAccelUnavailable : hwAccelAvailable));

  const WCHAR hwHintNoDeviceChoice[] = L"The selected Hardware Decoder does not support using a specific device.";
  const WCHAR hwHintDXVA2Display[] = L"DXVA2 requires an active display for GPUs to be available.\nNote that GPUs are listed once for each connected display.";
  const WCHAR hwHintD3D11NotSupported[] = L"D3D11 requires Windows 8 or newer, and is not supported on this OS.";
  const WCHAR hwHintD3D11DeviceHint[] = L"Selecting a specific device for D3D11 disables Native mode and forces Copy-Back, use Automatic for the best performance.";


  SendDlgItemMessage(m_Dlg, IDC_HWACCEL_DEVICE_SELECT, CB_RESETCONTENT, 0, 0);
  SendDlgItemMessage(m_Dlg, IDC_HWACCEL_DEVICE_SELECT, CB_ADDSTRING, 0, (hwAccel == HWAccel_D3D11) ? (LPARAM)L"Automatic (Native)" : (LPARAM)L"Automatic");

  DWORD dwnDevices = m_pVideoSettings->GetHWAccelNumDevices(hwAccel);
  for (DWORD dwDevice = 0; dwDevice < dwnDevices; dwDevice++)
  {
    BSTR bstrDeviceName = nullptr;
    HRESULT hr = m_pVideoSettings->GetHWAccelDeviceInfo(hwAccel, dwDevice, &bstrDeviceName, NULL);
    if (SUCCEEDED(hr)) {
      SendDlgItemMessage(m_Dlg, IDC_HWACCEL_DEVICE_SELECT, CB_ADDSTRING, 0, (LPARAM)bstrDeviceName);
      SysFreeString(bstrDeviceName);
    }
  }

  if (hwAccel == HWAccel_D3D11 && !IsWindows8OrNewer())
  {
    m_HWDeviceIndex = 0;
    dwnDevices = 0;
    SendDlgItemMessage(m_Dlg, IDC_LBL_HWACCEL_DEVICE_HINT, WM_SETTEXT, 0, (LPARAM)hwHintD3D11NotSupported);
  }
  else if (dwnDevices == 0) {
    m_HWDeviceIndex = 0;
    SendDlgItemMessage(m_Dlg, IDC_LBL_HWACCEL_DEVICE_HINT, WM_SETTEXT, 0, (LPARAM)hwHintNoDeviceChoice);
  }
  else {
    DWORD dwDeviceId = 0;
    m_HWDeviceIndex = m_pVideoSettings->GetHWAccelDeviceIndex(hwAccel, &dwDeviceId);
    if (m_HWDeviceIndex == LAVHWACCEL_DEVICE_DEFAULT)
      m_HWDeviceIndex = 0;
    else
      m_HWDeviceIndex++;

    if (hwAccel == HWAccel_DXVA2CopyBack)
      SendDlgItemMessage(m_Dlg, IDC_LBL_HWACCEL_DEVICE_HINT, WM_SETTEXT, 0, (LPARAM)hwHintDXVA2Display);
    else if (hwAccel == HWAccel_D3D11)
      SendDlgItemMessage(m_Dlg, IDC_LBL_HWACCEL_DEVICE_HINT, WM_SETTEXT, 0, (LPARAM)hwHintD3D11DeviceHint);
    else
      SendDlgItemMessage(m_Dlg, IDC_LBL_HWACCEL_DEVICE_HINT, WM_SETTEXT, 0, (LPARAM)L"");
  }

  EnableWindow(GetDlgItem(m_Dlg, IDC_LBL_HWACCEL_DEVICE_SELECT), (dwnDevices > 0));
  EnableWindow(GetDlgItem(m_Dlg, IDC_HWACCEL_DEVICE_SELECT), (dwnDevices > 0));

  SendDlgItemMessage(m_Dlg, IDC_HWACCEL_DEVICE_SELECT, CB_SETCURSEL, m_HWDeviceIndex, 0);

  return S_OK;
}

HRESULT CLAVVideoSettingsProp::UpdateYADIFOptions()
{
  DWORD dwVal = (DWORD)SendDlgItemMessage(m_Dlg, IDC_SWDEINT_MODE, CB_GETCURSEL, 0, 0);

  EnableWindow(GetDlgItem(m_Dlg, IDC_LBL_SWDEINT_MODE), (dwVal == SWDeintMode_YADIF));
  EnableWindow(GetDlgItem(m_Dlg, IDC_SWDEINT_OUT_FILM), (dwVal == SWDeintMode_YADIF));
  EnableWindow(GetDlgItem(m_Dlg, IDC_SWDEINT_OUT_VIDEO), (dwVal == SWDeintMode_YADIF));

  return S_OK;
}

HRESULT CLAVVideoSettingsProp::LoadData()
{
  HRESULT hr = S_OK;
  
  m_dwNumThreads    = m_pVideoSettings->GetNumThreads();
  m_StreamAR        = m_pVideoSettings->GetStreamAR();
  m_DeintFieldOrder = m_pVideoSettings->GetDeintFieldOrder();
  m_DeintMode       = m_pVideoSettings->GetDeinterlacingMode();
  m_dwRGBOutput     = m_pVideoSettings->GetRGBOutputRange();

  for (int i = 0; i < LAVOutPixFmt_NB; ++i) {
    m_bPixFmts[i] = m_pVideoSettings->GetPixelFormat((LAVOutPixFmts)i);
  }

  m_HWAccel = m_pVideoSettings->GetHWAccel();
  for (int i = 0; i < HWCodec_NB; ++i) {
    m_HWAccelCodecs[i] = m_pVideoSettings->GetHWAccelCodec((LAVVideoHWCodec)i);
  }

  m_HWRes = m_pVideoSettings->GetHWAccelResolutionFlags();
  m_HWAccelCUVIDDXVA = m_pVideoSettings->GetHWAccelDeintHQ();

  m_HWDeintAlgo = m_pVideoSettings->GetHWAccelDeintMode();
  m_HWDeintOutMode = m_pVideoSettings->GetHWAccelDeintOutput();

  m_SWDeint = m_pVideoSettings->GetSWDeintMode();
  m_SWDeintOutMode = m_pVideoSettings->GetSWDeintOutput();

  m_DitherMode = m_pVideoSettings->GetDitherMode();

  m_TrayIcon = m_pVideoSettings->GetTrayIcon();

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
      if (lValue != m_StreamAR) {
        SetDirty();
      }
    } else if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_THREADS) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), CB_GETCURSEL, 0, 0);
      if (lValue != m_dwNumThreads) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_DEINT_FIELDORDER && HIWORD(wParam) == CBN_SELCHANGE) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), CB_GETCURSEL, 0, 0);
      if (lValue != m_DeintFieldOrder) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_DEINT_MODE && HIWORD(wParam) == CBN_SELCHANGE) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), CB_GETCURSEL, 0, 0);
      if (lValue != m_DeintMode) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_YV12 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_YV12]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_NV12 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_NV12]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_P010 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_P010]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_P016 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_P016]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_YUY2 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_YUY2]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_UYVY && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_UYVY]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_P210 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_P210]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_V210 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_v210]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_P216 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_P216]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_YV24 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_YV24]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_AYUV && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_AYUV]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_Y410 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_Y410]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_V410 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_v410]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_Y416 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_Y416]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_RGB32 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_RGB32]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_RGB24 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_RGB24]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_OUT_RGB48 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bPixFmts[LAVOutPixFmt_RGB48]) {
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
    } else if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_HWACCEL) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), CB_GETCURSEL, 0, 0);
      if (lValue != m_HWAccel) {
        SetDirty();
      }
      UpdateHWOptions();
    } else if (LOWORD(wParam) == IDC_HWACCEL_H264 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_HWAccelCodecs[HWCodec_H264]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_HWACCEL_VC1 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_HWAccelCodecs[HWCodec_VC1]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_HWACCEL_MPEG2 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_HWAccelCodecs[HWCodec_MPEG2]) {
        SetDirty();
      }
      UpdateHWOptions();
    } else if (LOWORD(wParam) == IDC_HWACCEL_MPEG4 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_HWAccelCodecs[HWCodec_MPEG4]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_HWACCEL_MPEG2_DVD && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_HWAccelCodecs[HWCodec_MPEG2DVD]) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_HWACCEL_HEVC && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_HWAccelCodecs[HWCodec_HEVC]) {
        SetDirty();
      }
    }
    else if (LOWORD(wParam) == IDC_HWACCEL_VP9 && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_HWAccelCodecs[HWCodec_VP9]) {
        SetDirty();
      }
    }
    else if (LOWORD(wParam) == IDC_HWACCEL_H264MVC && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_HWAccelCodecs[HWCodec_H264MVC]) {
        SetDirty();
      }
    }
    else if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_HWACCEL_DEVICE_SELECT) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), CB_GETCURSEL, 0, 0);
      if (lValue != m_HWDeviceIndex) {
        SetDirty();
      }
    }
    else if (LOWORD(wParam) == IDC_HWACCEL_CUVID_DXVA && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_HWAccelCUVIDDXVA) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_HWDEINT_ENABLE && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != (m_HWDeintAlgo == HWDeintMode_Hardware)) {
        SetDirty();
      }
      UpdateHWOptions();
    } else if (LOWORD(wParam) == IDC_HWDEINT_OUT_FILM && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != (m_HWDeintOutMode == DeintOutput_FramePer2Field)) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_HWDEINT_OUT_VIDEO && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != (m_HWDeintOutMode == DeintOutput_FramePerField)) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_SWDEINT_OUT_FILM && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != (m_SWDeintOutMode == DeintOutput_FramePer2Field)) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_SWDEINT_OUT_VIDEO && HIWORD(wParam) == BN_CLICKED) {
      bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != (m_SWDeintOutMode == DeintOutput_FramePerField)) {
        SetDirty();
      }
    } else if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_SWDEINT_MODE) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), CB_GETCURSEL, 0, 0);
      if (lValue != m_SWDeint) {
        SetDirty();
      }
      UpdateYADIFOptions();
    } else if (LOWORD(wParam) == IDC_DITHER_ORDERED && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue != (m_DitherMode == LAVDither_Ordered)) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_DITHER_RANDOM && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue != (m_DitherMode == LAVDither_Random)) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_HWRES_SD && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue == !(m_HWRes & LAVHWResFlag_SD)) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_HWRES_HD && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue == !(m_HWRes & LAVHWResFlag_HD)) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_HWRES_UHD && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue == !(m_HWRes & LAVHWResFlag_UHD)) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_TRAYICON && HIWORD(wParam) == BN_CLICKED) {
      lValue = SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (lValue != m_TrayIcon) {
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
  : CBaseDSPropPage(NAME("LAVVideoFormats"), pUnk, IDD_PROPPAGE_FORMATS, IDS_FORMATS)
{
}

CLAVVideoFormatsProp::~CLAVVideoFormatsProp()
{
}

HRESULT CLAVVideoFormatsProp::OnConnect(IUnknown *pUnk)
{
  if (pUnk == nullptr)
  {
    return E_POINTER;
  }
  ASSERT(m_pVideoSettings == nullptr);
  return pUnk->QueryInterface(&m_pVideoSettings);
}

HRESULT CLAVVideoFormatsProp::OnDisconnect()
{
  SafeRelease(&m_pVideoSettings);
  return S_OK;
}


HRESULT CLAVVideoFormatsProp::OnApplyChanges()
{
  ASSERT(m_pVideoSettings != nullptr);
  HRESULT hr = S_OK;

  HWND hlv = GetDlgItem(m_Dlg, IDC_CODECS);

  // Get checked state
  BOOL bFlag;
  for (int nItem = 0; nItem < ListView_GetItemCount(hlv); nItem++) {
    bFlag = ListView_GetCheckState(hlv, nItem);
    m_pVideoSettings->SetFormatConfiguration((LAVVideoCodec)nItem, bFlag);
  }

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_CODECS_MSWMVDMO, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetUseMSWMV9Decoder(bFlag);

  bFlag = (BOOL)SendDlgItemMessage(m_Dlg, IDC_DVD_VIDEO, BM_GETCHECK, 0, 0);
  m_pVideoSettings->SetDVDVideoSupport(bFlag);

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
  ASSERT(m_pVideoSettings != nullptr);

  // Setup ListView control for format configuration
  SendDlgItemMessage(m_Dlg, IDC_CODECS, CCM_DPISCALE, TRUE, 0);

  HWND hlv = GetDlgItem(m_Dlg, IDC_CODECS);
  ListView_SetExtendedListViewStyle(hlv, LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);

  int nCol = 1;
  LVCOLUMN lvc = {LVCF_WIDTH, 0, 20, 0};
  ListView_InsertColumn(hlv, 0, &lvc);
  ListView_AddCol(hlv, nCol,  85, L"Codec", false);
  ListView_AddCol(hlv, nCol, 400, L"Description", false);

  ListView_DeleteAllItems(hlv);
  ListView_SetItemCount(hlv, Codec_VideoNB);

  // Create entries for the formats
  LVITEM lvi;
  memset(&lvi, 0, sizeof(lvi));
  lvi.mask = LVIF_TEXT|LVIF_PARAM;

  int nItem = 0;
  for (nItem = 0; nItem < Codec_VideoNB; ++nItem) {
    const codec_config_t *config = get_codec_config((LAVVideoCodec)nItem);

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

  SendDlgItemMessage(m_Dlg, IDC_CODECS_MSWMVDMO, BM_SETCHECK, m_bWMVDMO, 0);
  SendDlgItemMessage(m_Dlg, IDC_DVD_VIDEO, BM_SETCHECK, m_bDVD, 0);

  return hr;
}

HRESULT CLAVVideoFormatsProp::LoadData()
{
  HRESULT hr = S_OK;
  
  for (unsigned i = 0; i < Codec_VideoNB; ++i)
    m_bFormats[i] = m_pVideoSettings->GetFormatConfiguration((LAVVideoCodec)i) != 0;

  m_bWMVDMO = m_pVideoSettings->GetUseMSWMV9Decoder();
  m_bDVD    = m_pVideoSettings->GetDVDVideoSupport();

  return hr;
}

INT_PTR CLAVVideoFormatsProp::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_CODECS_MSWMVDMO && HIWORD(wParam) == BN_CLICKED) {
      BOOL bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bWMVDMO) {
        SetDirty();
      }
    } else if (LOWORD(wParam) == IDC_DVD_VIDEO && HIWORD(wParam) == BN_CLICKED) {
      BOOL bValue = (BOOL)SendDlgItemMessage(m_Dlg, LOWORD(wParam), BM_GETCHECK, 0, 0);
      if (bValue != m_bDVD) {
        SetDirty();
      }
    }
    break;
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
