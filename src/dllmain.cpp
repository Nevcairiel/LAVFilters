#include "stdafx.h"
#include "LAVFSplitter.h"

// --- COM factory table and registration code --------------

const AMOVIESETUP_MEDIATYPE 
  sudMediaTypes[] = 
{
  {
    &MEDIATYPE_Video,
      &MEDIASUBTYPE_NULL
  },
  {
    &MEDIATYPE_Audio,
      &MEDIASUBTYPE_NULL
    }
};

const AMOVIESETUP_PIN sudOutputPins[] = 
{
  {
    L"Video Output",    // pin name
      FALSE,              // is rendered?    
      TRUE,               // is output?
      FALSE,              // zero instances allowed?
      FALSE,              // many instances allowed?
      &CLSID_NULL,        // connects to filter (for bridge pins)
      NULL,               // connects to pin (for bridge pins)
      1,                  // count of registered media types
      &sudMediaTypes[0]       // list of registered media types    
  },
  {
    L"Audio Output",    // pin name
      FALSE,              // is rendered?    
      TRUE,               // is output?
      FALSE,              // zero instances allowed?
      FALSE,              // many instances allowed?
      &CLSID_NULL,        // connects to filter (for bridge pins)
      NULL,               // connects to pin (for bridge pins)
      1,                  // count of registered media types
      &sudMediaTypes[1]       // list of registered media types    
  },
};

const AMOVIESETUP_FILTER sudFilterReg =
{
  &__uuidof(CLAVFSplitter),         // filter clsid
  L"lavf dshow source",           // filter name
  MERIT_NORMAL,                   // merit
  2,                              // count of registered pins
  sudOutputPins                   // list of pins to register
};

// --- COM factory table and registration code --------------

// DirectShow base class COM factory requires this table, 
// declaring all the COM objects in this DLL
CFactoryTemplate g_Templates[] = {
  // one entry for each CoCreate-able object
  {
    sudFilterReg.strName,
      sudFilterReg.clsID,
      CLAVFSplitter::CreateInstance,
      NULL,
      &sudFilterReg
  },
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

// self-registration entrypoint
STDAPI DllRegisterServer()
{
  // base classes will handle registration using the factory template table
  HRESULT hr = AMovieDllRegisterServer2(true);

  return hr;
}

STDAPI DllUnregisterServer()
{
  // base classes will handle de-registration using the factory template table
  HRESULT hr = AMovieDllRegisterServer2(false);

  return hr;
}

// if we declare the correct C runtime entrypoint and then forward it to the DShow base
// classes we will be sure that both the C/C++ runtimes and the base classes are initialized
// correctly
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);
BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpReserved)
{
  return DllEntryPoint(reinterpret_cast<HINSTANCE>(hDllHandle), dwReason, lpReserved);
}
