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
#include "LAVAudio.h"
#include "AudioSettingsProp.h"
#include "moreuuids.h"

#include "registry.h"

// --- COM factory table and registration code --------------

// Workaround: graphedit crashes when a filter exposes more than 115 input MediaTypes!
const AMOVIESETUP_PIN sudpPinsAudioDec[] = {
	{L"Input", FALSE, FALSE, FALSE, FALSE, &CLSID_NULL, NULL, CLAVAudio::sudPinTypesInCount > 115 ? 115 : CLAVAudio::sudPinTypesInCount,  CLAVAudio::sudPinTypesIn},
	{L"Output", FALSE, TRUE, FALSE, FALSE, &CLSID_NULL, NULL, CLAVAudio::sudPinTypesOutCount, CLAVAudio::sudPinTypesOut}
};

const AMOVIESETUP_FILTER sudFilterReg =
{
  &__uuidof(CLAVAudio),       // filter clsid
  L"LAV Audio Decoder",       // filter name
  MERIT_PREFERRED + 3,        // merit
  countof(sudpPinsAudioDec),
  sudpPinsAudioDec,
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
      CreateInstance<CLAVAudio>,
      NULL,
      &sudFilterReg
  },
  // This entry is for the property page.
  { 
      L"LAV Audio Properties",
      &CLSID_LAVAudioSettingsProp,
      CreateInstance<CLAVAudioSettingsProp>,
      NULL, NULL
  },
  { 
    L"LAV Audio Format Settings",
      &CLSID_LAVAudioFormatsProp,
      CreateInstance<CLAVAudioFormatsProp>,
      NULL, NULL
  },
  {
      L"LAV Audio Status",
      &CLSID_LAVAudioStatusProp,
      CreateInstance<CLAVAudioStatusProp>,
      NULL, NULL
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

STDAPI OpenConfiguration()
{
  HRESULT hr = S_OK;
  CUnknown *pInstance = CreateInstance<CLAVAudio>(NULL, &hr);
  IBaseFilter *pFilter = NULL;
  pInstance->NonDelegatingQueryInterface(IID_IBaseFilter, (void **)&pFilter);
  if (pFilter) {
    pFilter->AddRef();
    CBaseDSPropPage::ShowPropPageDialog(pFilter);
  }
  delete pInstance;

  return 0;
}
