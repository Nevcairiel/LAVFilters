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
#include "DShowUtil.h"

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