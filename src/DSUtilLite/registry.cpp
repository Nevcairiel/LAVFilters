/*
 *      Copyright (C) 2010 Hendrik Leppkes
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
#include "registry.h"

bool CreateRegistryKey(HKEY hKeyRoot, LPCTSTR pszSubKey)
{
  HKEY hKey;
  LONG  lRet;

  lRet = RegCreateKeyEx(
    hKeyRoot,
    pszSubKey,
    0,
    NULL,
    REG_OPTION_NON_VOLATILE,
    KEY_WRITE,
    NULL,
    &hKey,
    NULL
    );

  if(lRet == ERROR_SUCCESS)
  {
    RegCloseKey(hKey);
    hKey = (HKEY)NULL;
    return true;
  }

  SetLastError((DWORD)lRet);
  return false;
}

CRegistry::CRegistry() : m_key(NULL)
{
}

CRegistry::CRegistry(HKEY hkeyRoot, LPCTSTR pszSubKey, HRESULT &hr)
  : m_key(NULL)
{
  hr = Open(hkeyRoot, pszSubKey);
}

CRegistry::~CRegistry()
{
  if (m_key)
    RegCloseKey(*m_key);
  delete m_key;
}

HRESULT CRegistry::Open(HKEY hkeyRoot, LPCTSTR pszSubKey)
{
  LONG lRet;
  
  if (m_key != NULL) { return E_UNEXPECTED; }
  
  m_key = new HKEY();
  lRet = RegOpenKeyEx(hkeyRoot, pszSubKey, 0, KEY_READ | KEY_WRITE, m_key);
  if (lRet != ERROR_SUCCESS) {
    delete m_key;
    m_key = NULL;
    return E_FAIL;
  }
  return S_OK;
}

std::wstring CRegistry::ReadString(LPCTSTR pszKey, HRESULT &hr)
{
  LONG lRet;
  DWORD dwSize;
  std::wstring result;

  hr = S_OK;

  if (m_key == NULL) { hr = E_UNEXPECTED;  return result; }

  lRet = RegQueryValueEx(*m_key, pszKey, NULL, NULL, NULL, &dwSize);

  if (lRet == ERROR_SUCCESS) {
    // Alloc Buffer to fit the data
    WCHAR *buffer = (WCHAR *)CoTaskMemAlloc(dwSize);
    memset(buffer, 0, dwSize);
    lRet = RegQueryValueEx(*m_key, pszKey, NULL, NULL, (LPBYTE)buffer, &dwSize);
    result = std::wstring(buffer);
    CoTaskMemFree(buffer);
  }
  
  if (lRet != ERROR_SUCCESS) {
    hr = E_FAIL;
  }

  return result;
}

HRESULT CRegistry::WriteString(LPCTSTR pszKey, const LPCTSTR pszValue)
{
  LONG lRet;
  HRESULT hr;

  hr = S_OK;

  if (m_key == NULL) { return E_UNEXPECTED; }

  lRet = RegSetValueEx(*m_key, pszKey, 0, REG_SZ, (const BYTE *)pszValue, (wcslen(pszValue) + 1) * sizeof(WCHAR));
  if (lRet != ERROR_SUCCESS) {
    return E_FAIL;
  }
  return S_OK;
}
