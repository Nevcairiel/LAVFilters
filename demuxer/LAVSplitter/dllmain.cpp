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
#include "LAVSplitter.h"
#include "moreuuids.h"

#include "registry.h"
#include "IGraphRebuildDelegate.h"
#include "IMediaSideDataFFmpeg.h"
#include "ILAVDynamicAllocator.h"

// The GUID we use to register the splitter media types
DEFINE_GUID(MEDIATYPE_LAVSplitter,
  0x9c53931c, 0x7d5a, 0x4a75, 0xb2, 0x6f, 0x4e, 0x51, 0x65, 0x4d, 0xb2, 0xc0);

// --- COM factory table and registration code --------------

const AMOVIESETUP_MEDIATYPE 
  sudMediaTypes[] = {
    { &MEDIATYPE_Stream, &MEDIASUBTYPE_NULL },
};

const AMOVIESETUP_PIN sudOutputPins[] = 
{
  {
    L"Output",            // pin name
      FALSE,              // is rendered?    
      TRUE,               // is output?
      FALSE,              // zero instances allowed?
      TRUE,               // many instances allowed?
      &CLSID_NULL,        // connects to filter (for bridge pins)
      nullptr,            // connects to pin (for bridge pins)
      0,                  // count of registered media types
      nullptr             // list of registered media types
  },
  {
    L"Input",             // pin name
      FALSE,              // is rendered?    
      FALSE,              // is output?
      FALSE,              // zero instances allowed?
      FALSE,              // many instances allowed?
      &CLSID_NULL,        // connects to filter (for bridge pins)
      nullptr,            // connects to pin (for bridge pins)
      1,                  // count of registered media types
      &sudMediaTypes[0]   // list of registered media types
  }
};

const AMOVIESETUP_FILTER sudFilterReg =
{
  &__uuidof(CLAVSplitter),        // filter clsid
  L"LAV Splitter",                // filter name
  MERIT_PREFERRED + 4,            // merit
  2,                              // count of registered pins
  sudOutputPins,                  // list of pins to register
  CLSID_LegacyAmFilterCategory
};

const AMOVIESETUP_FILTER sudFilterRegSource =
{
  &__uuidof(CLAVSplitterSource),  // filter clsid
  L"LAV Splitter Source",         // filter name
  MERIT_PREFERRED + 4,            // merit
  1,                              // count of registered pins
  sudOutputPins,                  // list of pins to register
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
      CreateInstance<CLAVSplitter>,
      nullptr,
      &sudFilterReg
  },
  {
    sudFilterRegSource.strName,
      sudFilterRegSource.clsID,
      CreateInstance<CLAVSplitterSource>,
      nullptr,
      &sudFilterRegSource
  },
  // This entry is for the property page.
  { 
      L"LAV Splitter Properties",
      &CLSID_LAVSplitterSettingsProp,
      CreateInstance<CLAVSplitterSettingsProp>,
      nullptr, nullptr
  },
  {
      L"LAV Splitter Input Formats",
      &CLSID_LAVSplitterFormatsProp,
      CreateInstance<CLAVSplitterFormatsProp>,
      nullptr, nullptr
  }
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

// self-registration entrypoint
STDAPI DllRegisterServer()
{
  std::list<LPCWSTR> chkbytes;

  // BluRay
  chkbytes.clear();
  chkbytes.push_back(L"0,4,,494E4458"); // INDX (index.bdmv)
  chkbytes.push_back(L"0,4,,4D4F424A"); // MOBJ (MovieObject.bdmv)
  chkbytes.push_back(L"0,4,,4D504C53"); // MPLS
  RegisterSourceFilter(__uuidof(CLAVSplitterSource),
    MEDIASUBTYPE_LAVBluRay, chkbytes, nullptr);

  // base classes will handle registration using the factory template table
  return AMovieDllRegisterServer2(true);
}

STDAPI DllUnregisterServer()
{
  UnRegisterSourceFilter(MEDIASUBTYPE_LAVBluRay);

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
  CUnknown *pInstance = CreateInstance<CLAVSplitter>(nullptr, &hr);
  IBaseFilter *pFilter = nullptr;
  pInstance->NonDelegatingQueryInterface(IID_IBaseFilter, (void **)&pFilter);
  if (pFilter) {
    pFilter->AddRef();
    CBaseDSPropPage::ShowPropPageDialog(pFilter);
  }
  delete pInstance;
}
