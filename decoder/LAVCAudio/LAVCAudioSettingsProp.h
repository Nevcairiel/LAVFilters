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

#pragma once

#include "LAVCAudioSettings.h"

// {2D8F1801-A70D-48F4-B76B-7F5AE022AB54}
DEFINE_GUID(CLSID_LAVCAudioSettingsProp, 
0x2d8f1801, 0xa70d, 0x48f4, 0xb7, 0x6b, 0x7f, 0x5a, 0xe0, 0x22, 0xab, 0x54);

// {20ED4A03-6AFD-4FD9-980B-2F6143AA0892}
DEFINE_GUID(CLSID_LAVCAudioStatusProp, 
0x20ed4a03, 0x6afd, 0x4fd9, 0x98, 0xb, 0x2f, 0x61, 0x43, 0xaa, 0x8, 0x92);



class CLAVCAudioSettingsProp : public CBasePropertyPage
{
public:
  static CUnknown* WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT* phr);

  ~CLAVCAudioSettingsProp();

  HRESULT OnActivate();
  HRESULT OnConnect(IUnknown *pUnk);
  HRESULT OnDisconnect();
  HRESULT OnApplyChanges();
  INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
  CLAVCAudioSettingsProp(IUnknown *pUnk);

  HRESULT LoadData();

private:
  ILAVCAudioSettings *m_pAudioSettings;
};

class CLAVCAudioStatusProp : public CBasePropertyPage
{
public:
  static CUnknown* WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT* phr);

  ~CLAVCAudioStatusProp();

  HRESULT OnActivate();
  HRESULT OnConnect(IUnknown *pUnk);
  HRESULT OnDisconnect();

private:
  CLAVCAudioStatusProp(IUnknown *pUnk);

private:
  ILAVCAudioStatus *m_pAudioStatus;
};
