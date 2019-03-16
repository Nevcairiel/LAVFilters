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

// Based on the SampleParser Template by GDCL
// --------------------------------------------------------------------------------
// Copyright (c) GDCL 2004. All Rights Reserved.
// You are free to re-use this as the basis for your own filter development,
// provided you retain this copyright notice in the source.
// http://www.gdcl.co.uk
// --------------------------------------------------------------------------------

#include "stdafx.h"

// Initialize the GUIDs
#include <InitGuid.h>

#include <qnetwork.h>
#include <dxva2api.h>
#include "LAVVideo.h"
#include "VideoSettingsProp.h"
#include "moreuuids.h"
#include "IMediaSideDataFFmpeg.h"

#include "registry.h"

// --- COM factory table and registration code --------------

const AMOVIESETUP_PIN sudpPinsVideoDec[] = {
	{L"Input", FALSE, FALSE, FALSE, FALSE, &CLSID_NULL, nullptr, CLAVVideo::sudPinTypesInCount,  CLAVVideo::sudPinTypesIn},
	{L"Output", FALSE, TRUE, FALSE, FALSE, &CLSID_NULL, nullptr, CLAVVideo::sudPinTypesOutCount, CLAVVideo::sudPinTypesOut}
};

const AMOVIESETUP_FILTER sudFilterReg =
{
  &__uuidof(CLAVVideo),       // filter clsid
  L"LAV Video Decoder",       // filter name
  MERIT_PREFERRED + 3,        // merit
  countof(sudpPinsVideoDec),
  sudpPinsVideoDec,
  CLSID_LegacyAmFilterCategory
};

// --- COM factory table and registration code --------------

// DirectShow base class COM factory requires this table,
// declaring all the COM objects in this DLL
CFactoryTemplate g_Templates[] = {
  // one entry for each CoCreate-able object
  {
    sudFilterReg.strName,
      sudFilterReg.clsID,
      CreateInstance<CLAVVideo>,
      nullptr,
      &sudFilterReg
  },
  // This entry is for the property page.
  {
    L"LAV Video Properties",
    &CLSID_LAVVideoSettingsProp,
    CreateInstance<CLAVVideoSettingsProp>,
    nullptr, nullptr
  },
  {
    L"LAV Video Format Settings",
    &CLSID_LAVVideoFormatsProp,
    CreateInstance<CLAVVideoFormatsProp>,
    nullptr, nullptr
  }
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

// self-registration entrypoint
STDAPI DllRegisterServer()
{
  // base classes will handle registration using the factory template table
  return AMovieDllRegisterServer2(true);
}

STDAPI DllUnregisterServer()
{
  // base classes will handle de-registration using the factory template table
  return AMovieDllRegisterServer2(false);
}

// if we declare the correct C runtime entrypoint and then forward it to the DShow base
// classes we will be sure that both the C/C++ runtimes and the base classes are initialized
// correctly
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);
BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpReserved)
{
  return DllEntryPoint(reinterpret_cast<HINSTANCE>(hDllHandle), dwReason, lpReserved);
}

void CALLBACK OpenConfiguration(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
  HRESULT hr = S_OK;
  CUnknown *pInstance = CreateInstance<CLAVVideo>(nullptr, &hr);
  IBaseFilter *pFilter = nullptr;
  pInstance->NonDelegatingQueryInterface(IID_IBaseFilter, (void **)&pFilter);
  if (pFilter) {
    pFilter->AddRef();
    CBaseDSPropPage::ShowPropPageDialog(pFilter);
  }
  delete pInstance;
}
