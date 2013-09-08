/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <string>
#include <vector>
#include <map>

// IDSMResourceBag

interface __declspec(uuid("EBAFBCBE-BDE0-489A-9789-05D5692E3A93"))
IDSMResourceBag :
public IUnknown {
    STDMETHOD_(DWORD, ResGetCount)() PURE;
    STDMETHOD(ResGet)(DWORD iIndex, BSTR * ppName, BSTR * ppDesc, BSTR * ppMime, BYTE** ppData, DWORD * pDataLen, DWORD_PTR * pTag) PURE;
    STDMETHOD(ResSet)(DWORD iIndex, LPCWSTR pName, LPCWSTR pDesc, LPCWSTR pMime, const BYTE * pData, DWORD len, DWORD_PTR tag) PURE;
    STDMETHOD(ResAppend)(LPCWSTR pName, LPCWSTR pDesc, LPCWSTR pMime, BYTE * pData, DWORD len, DWORD_PTR tag) PURE;
    STDMETHOD(ResRemoveAt)(DWORD iIndex) PURE;
    STDMETHOD(ResRemoveAll)(DWORD_PTR tag) PURE;
};

class CDSMResource
{
public:
    DWORD_PTR tag;
    std::wstring name, desc, mime;
    std::vector<BYTE> data;
    CDSMResource();
    CDSMResource(const CDSMResource& r);
    CDSMResource(LPCWSTR name, LPCWSTR desc, LPCWSTR mime, BYTE* pData, int len, DWORD_PTR tag = 0);
    virtual ~CDSMResource();
    CDSMResource& operator=(const CDSMResource& r);

    // global access to all resources
    static CCritSec m_csResources;
    static std::map<uintptr_t, CDSMResource*> m_resources;
};

class IDSMResourceBagImpl : public IDSMResourceBag
{
protected:
    std::vector<CDSMResource> m_resources;

public:
    IDSMResourceBagImpl();

    void operator+=(const CDSMResource& r) { m_resources.push_back(r); }

    // IDSMResourceBag

    STDMETHODIMP_(DWORD) ResGetCount();
    STDMETHODIMP ResGet(DWORD iIndex, BSTR* ppName, BSTR* ppDesc, BSTR* ppMime,
                        BYTE** ppData, DWORD* pDataLen, DWORD_PTR* pTag = nullptr);
    STDMETHODIMP ResSet(DWORD iIndex, LPCWSTR pName, LPCWSTR pDesc, LPCWSTR pMime,
                        const BYTE* pData, DWORD len, DWORD_PTR tag = 0);
    STDMETHODIMP ResAppend(LPCWSTR pName, LPCWSTR pDesc, LPCWSTR pMime,
                           BYTE* pData, DWORD len, DWORD_PTR tag = 0);
    STDMETHODIMP ResRemoveAt(DWORD iIndex);
    STDMETHODIMP ResRemoveAll(DWORD_PTR tag = 0);
};
