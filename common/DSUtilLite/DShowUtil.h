/*
 *      Copyright (C) 2010-2012 Hendrik Leppkes
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
 *
 *  Initial design and concept by Gabest and the MPC-HC Team, copyright under GPLv2
 */

#pragma once

#include <list>
#include <string>
#include <DShow.h>

#define LCID_NOSUBTITLES			-1

// SafeRelease Template, for type safety
template <class T> void SafeRelease(T **ppT)
{
  if (*ppT)
  {
    (*ppT)->Release();
    *ppT = NULL;
  }
}

#ifdef _DEBUG
#define DBG_TIMING(x,l,y) { DWORD start = timeGetTime(); y; DWORD end = timeGetTime(); if(end-start>l) DbgLog((LOG_CUSTOM5, 10, L"TIMING: %S took %u ms", x, end-start)); }
extern void DbgSetLogFile(LPCTSTR szLogFile);
extern void DbgSetLogFileDesktop(LPCTSTR szLogFile);
extern void DbgCloseLogFile();
#else
#define DBG_TIMING(x,l,y) y;
#define DbgSetLogFile(sz)
#define DbgSetLogFileDesktop(sz)
#define DbgCloseLogFile()
#endif


// SAFE_ARRAY_DELETE macro.
// Deletes an array allocated with new [].

#ifndef SAFE_ARRAY_DELETE
#define SAFE_ARRAY_DELETE(x) if (x) { delete [] x; x = NULL; }
#endif

// some common macros
#define SAFE_DELETE(pPtr) { delete pPtr; pPtr = NULL; }
#define SAFE_CO_FREE(pPtr) { CoTaskMemFree(pPtr); pPtr = NULL; }
#define CHECK_HR(hr) if (FAILED(hr)) { goto done; }
#define QI(i) (riid == __uuidof(i)) ? GetInterface((i*)this, ppv) :
#define QI2(i) (riid == IID_##i) ? GetInterface((i*)this, ppv) :
#define countof( array ) ( sizeof( array )/sizeof( array[0] ) )

// Gennenric IUnknown creation function
template <class T>
static CUnknown* WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT* phr)
{
  *phr = S_OK;
  CUnknown *punk = new T(lpunk, phr);
  if(punk == NULL) {
    *phr = E_OUTOFMEMORY;
  }
  return punk;
}

extern void SetThreadName( DWORD dwThreadID, LPCSTR szThreadName);

void split(std::string& text, std::string& separators, std::list<std::string>& words);

// Filter Registration
extern void RegisterSourceFilter(const CLSID& clsid, const GUID& subtype2, LPCWSTR chkbytes, ...);
extern void RegisterSourceFilter(const CLSID& clsid, const GUID& subtype2, std::list<LPCWSTR> chkbytes, ...);
extern void UnRegisterSourceFilter(const GUID& subtype);

extern void RegisterProtocolSourceFilter(const CLSID& clsid, LPCWSTR protocol);
extern void UnRegisterProtocolSourceFilter(LPCWSTR protocol);

// Locale
extern std::string ISO6391ToLanguage(LPCSTR code);
extern std::string ISO6392ToLanguage(LPCSTR code);
extern std::string ProbeLangForLanguage(LPCSTR code);
extern LCID ISO6391ToLcid(LPCSTR code);
extern LCID ISO6392ToLcid(LPCSTR code);
extern LCID ProbeLangForLCID(LPCSTR code);
extern std::string ISO6391To6392(LPCSTR code);
extern std::string ISO6392To6391(LPCSTR code);
extern std::string ProbeForISO6392(LPCSTR lang);

// FilterGraphUtils
extern HRESULT FilterGraphCleanup(IFilterGraph *pGraph);
extern IBaseFilter *FindFilter(const GUID& clsid, IFilterGraph *pFG);
extern BOOL FilterInGraph(const GUID& clsid, IFilterGraph *pFG);
extern BOOL FilterInGraphWithInputSubtype(const GUID& clsid, IFilterGraph *pFG, const GUID& clsidSubtype);
extern IBaseFilter* GetFilterFromPin(IPin* pPin);
extern HRESULT NukeDownstream(IFilterGraph *pGraph, IPin *pPin);
extern HRESULT NukeDownstream(IFilterGraph *pGraph, IBaseFilter *pFilter);
extern HRESULT FindIntefaceInGraph(IPin *pPin, REFIID refiid, void **pUnknown);
extern HRESULT FindPinIntefaceInGraph(IPin *pPin, REFIID refiid, void **pUnknown);
extern HRESULT FindFilterSafe(IPin *pPin, const GUID &guid, IBaseFilter **ppFilter);
extern BOOL FilterInGraphSafe(IPin *pPin, const GUID &guid);

std::wstring WStringFromGUID(const GUID& guid);
BSTR ConvertCharToBSTR(const char *sz);

unsigned int lav_xiphlacing(unsigned char *s, unsigned int v);

void videoFormatTypeHandler(const AM_MEDIA_TYPE &mt, BITMAPINFOHEADER **pBMI = NULL, REFERENCE_TIME *prtAvgTime = NULL, DWORD *pDwAspectX = NULL, DWORD *pDwAspectY = NULL);
void videoFormatTypeHandler(const BYTE *format, const GUID *formattype, BITMAPINFOHEADER **pBMI = NULL, REFERENCE_TIME *prtAvgTime = NULL, DWORD *pDwAspectX = NULL, DWORD *pDwAspectY = NULL);
void audioFormatTypeHandler(const BYTE *format, const GUID *formattype, DWORD *pnSamples, WORD *pnChannels, WORD *pnBitsPerSample, WORD *pnBlockAlign, DWORD *pnBytesPerSec);
void getExtraData(const AM_MEDIA_TYPE &mt, BYTE *extra, size_t *extralen);
void getExtraData(const BYTE *format, const GUID *formattype, const size_t formatlen, BYTE *extra, size_t *extralen);
