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
 */

#include "stdafx.h"
#include "DShowUtil.h"

#include <dvdmedia.h>
#include "moreuuids.h"

//
// Usage: SetThreadName (-1, "MainThread");
//
typedef struct tagTHREADNAME_INFO
{
   DWORD dwType; // must be 0x1000
   LPCSTR szName; // pointer to name (in user addr space)
   DWORD dwThreadID; // thread ID (-1=caller thread)
   DWORD dwFlags; // reserved for future use, must be zero
} THREADNAME_INFO;

void SetThreadName( DWORD dwThreadID, LPCSTR szThreadName)
{
   THREADNAME_INFO info;
   info.dwType = 0x1000;
   info.szName = szThreadName;
   info.dwThreadID = dwThreadID;
   info.dwFlags = 0;

   __try
   {
      RaiseException( 0x406D1388, 0, sizeof(info)/sizeof(DWORD), (ULONG_PTR*)&info );
   }
   __except(EXCEPTION_CONTINUE_EXECUTION)
   {
   }
}

#ifdef DEBUG

#include <Shlobj.h>
#include <Shlwapi.h>

extern HANDLE m_hOutput;
extern HRESULT  DbgUniqueProcessName(LPCTSTR inName, LPTSTR outName);
void DbgSetLogFile(LPCTSTR szFile)
{
  if (m_hOutput != INVALID_HANDLE_VALUE) {
    CloseHandle (m_hOutput);
    m_hOutput = INVALID_HANDLE_VALUE;
  }

  m_hOutput = CreateFile(szFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  if (INVALID_HANDLE_VALUE == m_hOutput &&
    GetLastError() == ERROR_SHARING_VIOLATION)
  {
    TCHAR uniqueName[MAX_PATH] = {0};
    if (SUCCEEDED(DbgUniqueProcessName(szFile, uniqueName)))
    {
      m_hOutput = CreateFile(uniqueName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }
  }
}

void DbgSetLogFileDesktop(LPCTSTR szFile)
{
  TCHAR szLogPath[512];
  SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, szLogPath);
  PathAppend(szLogPath, szFile);
  DbgSetLogFile(szLogPath);
}

void DbgCloseLogFile()
{
  if (m_hOutput != INVALID_HANDLE_VALUE) {
    FlushFileBuffers (m_hOutput);
    CloseHandle (m_hOutput);
    m_hOutput = INVALID_HANDLE_VALUE;
  }
}
#endif

void split(std::string& text, std::string& separators, std::list<std::string>& words)
{
    size_t n = text.length();
    size_t start, stop;
 
    start = text.find_first_not_of(separators);
    while ((start >= 0) && (start < n)) 
    {
        stop = text.find_first_of(separators, start);
        if ((stop < 0) || (stop > n)) stop = n;
        words.push_back(text.substr(start, stop - start));
        start = text.find_first_not_of(separators, stop+1);
    }
}

IBaseFilter* FindFilter(const GUID& clsid, IFilterGraph *pFG)
{
  IBaseFilter *pFilter = NULL;
  IEnumFilters *pEnumFilters = NULL;
  if(pFG && SUCCEEDED(pFG->EnumFilters(&pEnumFilters))) {
    for(IBaseFilter *pBF = NULL; S_OK == pEnumFilters->Next(1, &pBF, 0); ) {
      GUID clsid2;
      if(SUCCEEDED(pBF->GetClassID(&clsid2)) && clsid == clsid2) {
        pFilter = pBF;
        break;
      }
      SafeRelease(&pBF);
    }
    SafeRelease(&pEnumFilters);
  }

  return pFilter;
}

BOOL FilterInGraph(const GUID& clsid, IFilterGraph *pFG)
{
  BOOL bFound = FALSE;
  IBaseFilter *pFilter = NULL;

  pFilter = FindFilter(clsid, pFG);
  bFound = (pFilter != NULL);
  SafeRelease(&pFilter);

  return bFound;
}

BOOL FilterInGraphWithInputSubtype(const GUID& clsid, IFilterGraph *pFG, const GUID& clsidSubtype)
{
  BOOL bFound = FALSE;
  IBaseFilter *pFilter = NULL;

  pFilter = FindFilter(clsid, pFG);

  if (pFilter) {
    IEnumPins *pPinEnum = NULL;
    pFilter->EnumPins(&pPinEnum);
    IPin *pPin = NULL;
    while((S_OK == pPinEnum->Next(1, &pPin, NULL)) && pPin) {
      PIN_DIRECTION dir;
      pPin->QueryDirection(&dir);
      if (dir == PINDIR_INPUT) {
        AM_MEDIA_TYPE mt;
        pPin->ConnectionMediaType(&mt);

        if(mt.subtype == clsidSubtype) {
          bFound = TRUE;
        }
        FreeMediaType(mt);
      }
      SafeRelease(&pPin);

      if (bFound)
        break;
    }

    SafeRelease(&pPinEnum);
    SafeRelease(&pFilter);
  }

  return bFound;
}

std::wstring WStringFromGUID(const GUID& guid)
{
  WCHAR null[128] = {0}, buff[128];
  StringFromGUID2(GUID_NULL, null, 127);
  return std::wstring(StringFromGUID2(guid, buff, 127) > 0 ? buff : null);
}

BSTR ConvertCharToBSTR(const char *sz)
{
  if (!sz)
    return NULL;

  size_t len = MultiByteToWideChar(CP_UTF8, 0, sz, -1, NULL, 0);

  WCHAR *wide = (WCHAR *)CoTaskMemAlloc(len * sizeof(WCHAR));
  MultiByteToWideChar(CP_UTF8, 0, sz, -1, wide, (int)len);

  BSTR bstr = SysAllocString(wide);
  CoTaskMemFree(wide);

  return bstr;
}

IBaseFilter* GetFilterFromPin(IPin* pPin)
{
  CheckPointer(pPin, NULL);

  PIN_INFO pi;
  if(pPin && SUCCEEDED(pPin->QueryPinInfo(&pi))) {
    return pi.pFilter;
  }

  return NULL;
}

HRESULT NukeDownstream(IFilterGraph *pGraph, IPin *pPin)
{
  PIN_DIRECTION dir;
  if (pPin) {
    IPin *pPinTo = NULL;
    if (FAILED(pPin->QueryDirection(&dir)))
      return E_FAIL;
    if (dir == PINDIR_OUTPUT) {
      if (SUCCEEDED(pPin->ConnectedTo(&pPinTo)) && pPinTo) {
        if (IBaseFilter *pFilter = GetFilterFromPin(pPinTo)) {
          NukeDownstream(pGraph, pFilter);
          pGraph->Disconnect(pPinTo);
          pGraph->Disconnect(pPin);
          pGraph->RemoveFilter(pFilter);
          SafeRelease(&pFilter);
        }
        SafeRelease(&pPinTo);
      }
    }
  }

  return S_OK;
}

HRESULT NukeDownstream(IFilterGraph *pGraph, IBaseFilter *pFilter)
{
  IEnumPins *pEnumPins = NULL;
  if(pFilter && SUCCEEDED(pFilter->EnumPins(&pEnumPins))) {
    for(IPin *pPin = NULL; S_OK == pEnumPins->Next(1, &pPin, 0); pPin = NULL) {
      NukeDownstream(pGraph, pPin);
      SafeRelease(&pPin);
    }
    SafeRelease(&pEnumPins);
  }

  return S_OK;
}

// pPin - pin of our filter to start searching
// refiid - guid of the interface to find
// pUnknown - variable that'll receive the interface
HRESULT FindIntefaceInGraph(IPin *pPin, REFIID refiid, void **pUnknown)
{
  PIN_DIRECTION dir;
  pPin->QueryDirection(&dir);

  IPin *pOtherPin = NULL;
  if (SUCCEEDED(pPin->ConnectedTo(&pOtherPin)) && pOtherPin) {
    IBaseFilter *pFilter = GetFilterFromPin(pOtherPin);
    SafeRelease(&pOtherPin);

    HRESULT hrFilter = pFilter->QueryInterface(refiid, pUnknown);
    if (FAILED(hrFilter)) {
      IEnumPins *pPinEnum = NULL;
      pFilter->EnumPins(&pPinEnum);

      HRESULT hrPin = E_FAIL;
      for (IPin *pOtherPin2 = NULL; pPinEnum->Next(1, &pOtherPin2, 0) == S_OK; pOtherPin2 = NULL) {
        PIN_DIRECTION pinDir;
        pOtherPin2->QueryDirection(&pinDir);
        if (dir == pinDir) {
          hrPin = FindIntefaceInGraph(pOtherPin2, refiid, pUnknown);
        }
        SafeRelease(&pOtherPin2);
        if (SUCCEEDED(hrPin))
          break;
      }
      hrFilter = hrPin;
      SafeRelease(&pPinEnum);
    }
    SafeRelease(&pFilter);

    if (SUCCEEDED(hrFilter)) {
      return S_OK;
    }
  }
  return E_NOINTERFACE;
}

// pPin - pin of our filter to start searching
// refiid - guid of the interface to find
// pUnknown - variable that'll receive the interface
HRESULT FindPinIntefaceInGraph(IPin *pPin, REFIID refiid, void **pUnknown)
{
  PIN_DIRECTION dir;
  pPin->QueryDirection(&dir);

  IPin *pOtherPin = NULL;
  if (SUCCEEDED(pPin->ConnectedTo(&pOtherPin)) && pOtherPin) {
    IBaseFilter *pFilter = NULL;
    HRESULT hrFilter = pOtherPin->QueryInterface(refiid, pUnknown);

    if (FAILED(hrFilter)) {
      pFilter = GetFilterFromPin(pOtherPin);

      IEnumPins *pPinEnum = NULL;
      pFilter->EnumPins(&pPinEnum);

      HRESULT hrPin = E_FAIL;
      for (IPin *pOtherPin2 = NULL; pPinEnum->Next(1, &pOtherPin2, 0) == S_OK; pOtherPin2 = NULL) {
        PIN_DIRECTION pinDir;
        pOtherPin2->QueryDirection(&pinDir);
        if (dir == pinDir) {
          hrPin = FindPinIntefaceInGraph(pOtherPin2, refiid, pUnknown);
        }
        SafeRelease(&pOtherPin2);
        if (SUCCEEDED(hrPin))
          break;
      }
      hrFilter = hrPin;
      SafeRelease(&pPinEnum);
    }
    SafeRelease(&pFilter);
    SafeRelease(&pOtherPin);

    if (SUCCEEDED(hrFilter)) {
      return S_OK;
    }
  }
  return E_NOINTERFACE;
}

// pPin - pin of our filter to start searching
// guid - guid of the filter to find
// ppFilter - variable that'll receive a AddRef'd reference to the filter
HRESULT FindFilterSafe(IPin *pPin, const GUID &guid, IBaseFilter **ppFilter)
{
  CheckPointer(ppFilter, E_POINTER);
  CheckPointer(pPin, E_POINTER);

  PIN_DIRECTION dir;
  pPin->QueryDirection(&dir);

  IPin *pOtherPin = NULL;
  if (SUCCEEDED(pPin->ConnectedTo(&pOtherPin)) && pOtherPin) {
    IBaseFilter *pFilter = GetFilterFromPin(pOtherPin);
    SafeRelease(&pOtherPin);

    HRESULT hrFilter = E_NOINTERFACE;
    CLSID filterGUID;
    if (SUCCEEDED(pFilter->GetClassID(&filterGUID))) {
      if (filterGUID == guid) {
        *ppFilter = pFilter;
        hrFilter = S_OK;
      } else {
        IEnumPins *pPinEnum = NULL;
        pFilter->EnumPins(&pPinEnum);

        HRESULT hrPin = E_FAIL;
        for (IPin *pOtherPin2 = NULL; pPinEnum->Next(1, &pOtherPin2, 0) == S_OK; pOtherPin2 = NULL) {
          PIN_DIRECTION pinDir;
          pOtherPin2->QueryDirection(&pinDir);
          if (dir == pinDir) {
            hrPin = FindFilterSafe(pOtherPin2, guid, ppFilter);
          }
          SafeRelease(&pOtherPin2);
          if (SUCCEEDED(hrPin))
            break;
        }
        hrFilter = hrPin;
        SafeRelease(&pPinEnum);
        SafeRelease(&pFilter);
      }
    }

    if (SUCCEEDED(hrFilter)) {
      return S_OK;
    }
  }
  return E_NOINTERFACE;
}

BOOL FilterInGraphSafe(IPin *pPin, const GUID &guid)
{
  IBaseFilter *pFilter = NULL;
  HRESULT hr = FindFilterSafe(pPin, guid, &pFilter);
  if (SUCCEEDED(hr) && pFilter)  {
    SafeRelease(&pFilter);
    return TRUE;
  }
  return FALSE;
}

unsigned int lav_xiphlacing(unsigned char *s, unsigned int v)
{
    unsigned int n = 0;

    while(v >= 0xff) {
        *s++ = 0xff;
        v -= 0xff;
        n++;
    }
    *s = v;
    n++;
    return n;
}

void videoFormatTypeHandler(const AM_MEDIA_TYPE &mt, BITMAPINFOHEADER **pBMI, REFERENCE_TIME *prtAvgTime, DWORD *pDwAspectX, DWORD *pDwAspectY)
{
  videoFormatTypeHandler(mt.pbFormat, &mt.formattype, pBMI, prtAvgTime, pDwAspectX, pDwAspectY);
}

void videoFormatTypeHandler(const BYTE *format, const GUID *formattype, BITMAPINFOHEADER **pBMI, REFERENCE_TIME *prtAvgTime, DWORD *pDwAspectX, DWORD *pDwAspectY)
{
  REFERENCE_TIME rtAvg = 0;
  BITMAPINFOHEADER *bmi = NULL;
  DWORD dwAspectX = 0, dwAspectY = 0;

  if (*formattype == FORMAT_VideoInfo) {
    VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)format;
    rtAvg = vih->AvgTimePerFrame;
    bmi = &vih->bmiHeader;
  } else if (*formattype == FORMAT_VideoInfo2) {
    VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)format;
    rtAvg = vih2->AvgTimePerFrame;
    bmi = &vih2->bmiHeader;
    dwAspectX = vih2->dwPictAspectRatioX;
    dwAspectY = vih2->dwPictAspectRatioY;
  } else if (*formattype == FORMAT_MPEGVideo) {
    MPEG1VIDEOINFO *mp1vi = (MPEG1VIDEOINFO *)format;
    rtAvg = mp1vi->hdr.AvgTimePerFrame;
    bmi = &mp1vi->hdr.bmiHeader;
  } else if (*formattype == FORMAT_MPEG2Video) {
    MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)format;
    rtAvg = mp2vi->hdr.AvgTimePerFrame;
    bmi = &mp2vi->hdr.bmiHeader;
    dwAspectX = mp2vi->hdr.dwPictAspectRatioX;
    dwAspectY = mp2vi->hdr.dwPictAspectRatioY;
  } else {
    ASSERT(FALSE);
  }

  if (pBMI) {
    *pBMI = bmi;
  }
  if (prtAvgTime) {
    *prtAvgTime = rtAvg;
  }
  if (pDwAspectX && pDwAspectY) {
    *pDwAspectX = dwAspectX;
    *pDwAspectY = dwAspectY;
  }
}

void audioFormatTypeHandler(const BYTE *format, const GUID *formattype, DWORD *pnSamples, WORD *pnChannels, WORD *pnBitsPerSample, WORD *pnBlockAlign, DWORD *pnBytesPerSec)
{
  DWORD nSamples       = 0;
  WORD  nChannels      = 0;
  WORD  nBitsPerSample = 0;
  WORD  nBlockAlign    = 0;
  DWORD nBytesPerSec   = 0;

  if (*formattype == FORMAT_WaveFormatEx) {
    WAVEFORMATEX *wfex = (WAVEFORMATEX *)format;
    nSamples       = wfex->nSamplesPerSec;
    nChannels      = wfex->nChannels;
    nBitsPerSample = wfex->wBitsPerSample;
    nBlockAlign    = wfex->nBlockAlign;
    nBytesPerSec   = wfex->nAvgBytesPerSec;
  } else if (*formattype == FORMAT_VorbisFormat2) {
    VORBISFORMAT2 *vf2 = (VORBISFORMAT2 *)format;
    nSamples       = vf2->SamplesPerSec;
    nChannels      = (WORD)vf2->Channels;
    nBitsPerSample = (WORD)vf2->BitsPerSample;
  }

  if (pnSamples)
    *pnSamples = nSamples;
  if (pnChannels)
    *pnChannels = nChannels;
  if (pnBitsPerSample)
    *pnBitsPerSample = nBitsPerSample;
  if (pnBlockAlign)
    *pnBlockAlign = nBlockAlign;
  if (pnBytesPerSec)
    *pnBytesPerSec = nBytesPerSec;
}

void getExtraData(const AM_MEDIA_TYPE &mt, BYTE *extra, size_t *extralen)
{
  return getExtraData(mt.pbFormat, &mt.formattype, mt.cbFormat, extra, extralen);
}

void getExtraData(const BYTE *format, const GUID *formattype, const size_t formatlen, BYTE *extra, size_t *extralen)
{
  const BYTE *extraposition = NULL;
  size_t extralength = 0;
  if (*formattype == FORMAT_WaveFormatEx) {
    WAVEFORMATEX *wfex = (WAVEFORMATEX *)format;
    extraposition = format + sizeof(WAVEFORMATEX);
    // Protected against over-reads
    extralength   = formatlen - sizeof(WAVEFORMATEX);
  } else if (*formattype == FORMAT_VorbisFormat2) {
    VORBISFORMAT2 *vf2 = (VORBISFORMAT2 *)format;
    BYTE *start = NULL, *end = NULL;
    unsigned offset = 1;
    if (extra) {
      *extra = 2;
      offset += lav_xiphlacing(extra+offset, vf2->HeaderSize[0]);
      offset += lav_xiphlacing(extra+offset, vf2->HeaderSize[1]);
      extra += offset;
    } else {
      BYTE dummy[100];
      offset += lav_xiphlacing(dummy, vf2->HeaderSize[0]);
      offset += lav_xiphlacing(dummy, vf2->HeaderSize[1]);
    }
    extralength = vf2->HeaderSize[0] + vf2->HeaderSize[1] + vf2->HeaderSize[2];
    extralength = min(extralength, formatlen - sizeof(VORBISFORMAT2));

    if (extra && extralength)
      memcpy(extra, format + sizeof(VORBISFORMAT2), extralength);
    if (extralen)
      *extralen = extralength + offset;

    return;
  } else if (*formattype == FORMAT_VideoInfo) {
    extraposition = format + sizeof(VIDEOINFOHEADER);
    extralength   = formatlen - sizeof(VIDEOINFOHEADER);
  } else if(*formattype == FORMAT_VideoInfo2) {
    extraposition = format + sizeof(VIDEOINFOHEADER2);
    extralength   = formatlen - sizeof(VIDEOINFOHEADER2);
  } else if (*formattype == FORMAT_MPEGVideo) {
    MPEG1VIDEOINFO *mp1vi = (MPEG1VIDEOINFO *)format;
    extraposition = (BYTE *)mp1vi->bSequenceHeader;
    extralength   =  min(mp1vi->cbSequenceHeader, formatlen - FIELD_OFFSET(MPEG1VIDEOINFO, bSequenceHeader[0]));
  } else if (*formattype == FORMAT_MPEG2Video) {
    MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)format;
    extraposition = (BYTE *)mp2vi->dwSequenceHeader;
    extralength   = min(mp2vi->cbSequenceHeader, formatlen - FIELD_OFFSET(MPEG2VIDEOINFO, dwSequenceHeader[0]));
  }

  if (extra && extralength)
    memcpy(extra, extraposition, extralength);
  if (extralen)
    *extralen = extralength;
}
