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

#pragma once

#include "LAVVideoSettings.h"
#include "BaseDSPropPage.h"
#include "Media.h"

// {278407C2-558C-4BED-83A0-B6FA454200BD}
DEFINE_GUID(CLSID_LAVVideoSettingsProp, 
0x278407c2, 0x558c, 0x4bed, 0x83, 0xa0, 0xb6, 0xfa, 0x45, 0x42, 0x0, 0xbd);

// {2D4D6F88-8B41-40A2-B297-3D722816648B}
DEFINE_GUID(CLSID_LAVVideoFormatsProp, 
0x2d4d6f88, 0x8b41, 0x40a2, 0xb2, 0x97, 0x3d, 0x72, 0x28, 0x16, 0x64, 0x8b);

class CLAVVideoSettingsProp : public CBaseDSPropPage
{
public:
  CLAVVideoSettingsProp(LPUNKNOWN pUnk, HRESULT* phr);
  ~CLAVVideoSettingsProp();

  HRESULT OnActivate();
  HRESULT OnConnect(IUnknown *pUnk);
  HRESULT OnDisconnect();
  HRESULT OnApplyChanges();
  INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
  HRESULT LoadData();
  HRESULT UpdateHWOptions();
  HRESULT UpdateYADIFOptions();

  void SetDirty()
  {
    m_bDirty = TRUE;
    if (m_pPageSite)
    {
      m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
    }
  }

private:
  ILAVVideoSettings *m_pVideoSettings = nullptr;
  ILAVVideoStatus   *m_pVideoStatus   = nullptr;

  DWORD m_dwNumThreads;
  DWORD m_StreamAR;
  DWORD m_DeintFieldOrder;
  LAVDeintMode m_DeintMode;
  DWORD m_dwRGBOutput;

  BOOL  m_bPixFmts[LAVOutPixFmt_NB];
  DWORD m_HWAccel;
  BOOL  m_HWAccelCodecs[HWCodec_NB];
  BOOL  m_HWAccelCUVIDDXVA;

  DWORD m_HWRes;
  DWORD m_HWDeviceIndex;

  DWORD m_HWDeintAlgo;
  DWORD m_HWDeintOutMode;
  LAVSWDeintModes m_SWDeint;
  DWORD m_SWDeintOutMode;

  DWORD m_DitherMode;
  BOOL m_TrayIcon;
};

class CLAVVideoFormatsProp : public CBaseDSPropPage
{
public:
  CLAVVideoFormatsProp(LPUNKNOWN pUnk, HRESULT* phr);
  ~CLAVVideoFormatsProp();

  HRESULT OnActivate();
  HRESULT OnConnect(IUnknown *pUnk);
  HRESULT OnDisconnect();
  HRESULT OnApplyChanges();
  INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
  HRESULT LoadData();

  void SetDirty()
  {
    m_bDirty = TRUE;
    if (m_pPageSite)
    {
      m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
    }
  }

private:
  ILAVVideoSettings *m_pVideoSettings = nullptr;

  BOOL m_bFormats[Codec_VideoNB];
  BOOL m_bWMVDMO;
  BOOL m_bDVD;
};
