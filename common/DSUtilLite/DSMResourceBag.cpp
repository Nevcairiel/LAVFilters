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

#include "stdafx.h"
#include "DSMResourceBag.h"


//
// CDSMResource
//

CCritSec CDSMResource::m_csResources;
std::map<uintptr_t, CDSMResource*> CDSMResource::m_resources;

CDSMResource::CDSMResource()
    : mime(L"application/octet-stream")
    , tag(0)
{
    CAutoLock cAutoLock(&m_csResources);
    m_resources[reinterpret_cast<uintptr_t>(this)] = this;
}

CDSMResource::CDSMResource(const CDSMResource& r)
{
    *this = r;

    CAutoLock cAutoLock(&m_csResources);
    m_resources[reinterpret_cast<uintptr_t>(this)] = this;
}

CDSMResource::CDSMResource(LPCWSTR name, LPCWSTR desc, LPCWSTR mime, BYTE* pData, int len, DWORD_PTR tag)
{
    this->name = name;
    this->desc = desc;
    this->mime = mime;
    data.resize(len);
    memcpy(data.data(), pData, data.size());
    this->tag = tag;

    CAutoLock cAutoLock(&m_csResources);
    m_resources[reinterpret_cast<uintptr_t>(this)] = this;
}

CDSMResource::~CDSMResource()
{
    CAutoLock cAutoLock(&m_csResources);
    m_resources.erase(reinterpret_cast<uintptr_t>(this));
}

CDSMResource& CDSMResource::operator=(const CDSMResource& r)
{
    if (this != &r) {
        tag = r.tag;
        name = r.name;
        desc = r.desc;
        mime = r.mime;
        data = r.data;
    }
    return *this;
}

//
// IDSMResourceBagImpl
//

IDSMResourceBagImpl::IDSMResourceBagImpl()
{
}

// IDSMResourceBag

STDMETHODIMP_(DWORD) IDSMResourceBagImpl::ResGetCount()
{
    return (DWORD)m_resources.size();
}

STDMETHODIMP IDSMResourceBagImpl::ResGet(DWORD iIndex, BSTR* ppName, BSTR* ppDesc, BSTR* ppMime, BYTE** ppData, DWORD* pDataLen, DWORD_PTR* pTag)
{
    if (ppData) {
        CheckPointer(pDataLen, E_POINTER);
    }

    if (iIndex >= (DWORD)m_resources.size()) {
        return E_INVALIDARG;
    }

    CDSMResource& r = m_resources[iIndex];

    if (ppName) {
        *ppName = SysAllocString(r.name.data());
        if (*ppName == NULL)
            return E_OUTOFMEMORY;
    }
    if (ppDesc) {
        *ppDesc = SysAllocString(r.desc.data());
        if (*ppDesc == NULL)
            return E_OUTOFMEMORY;
    }
    if (ppMime) {
        *ppMime = SysAllocString(r.mime.data());
        if (*ppMime == NULL)
            return E_OUTOFMEMORY;
    }
    if (ppData) {
        *pDataLen = (DWORD)r.data.size();
        memcpy(*ppData = (BYTE*)CoTaskMemAlloc(*pDataLen), r.data.data(), *pDataLen);
    }
    if (pTag) {
        *pTag = r.tag;
    }

    return S_OK;
}

STDMETHODIMP IDSMResourceBagImpl::ResSet(DWORD iIndex, LPCWSTR pName, LPCWSTR pDesc, LPCWSTR pMime, const BYTE* pData, DWORD len, DWORD_PTR tag)
{
    if (iIndex >= (DWORD)m_resources.size()) {
        return E_INVALIDARG;
    }

    CDSMResource& r = m_resources[iIndex];

    if (pName) {
        r.name = pName;
    }
    if (pDesc) {
        r.desc = pDesc;
    }
    if (pMime) {
        r.mime = pMime;
    }
    if (pData || len == 0) {
        r.data.resize(len);
        if (pData) {
            memcpy(r.data.data(), pData, r.data.size());
        }
    }
    r.tag = tag;

    return S_OK;
}

STDMETHODIMP IDSMResourceBagImpl::ResAppend(LPCWSTR pName, LPCWSTR pDesc, LPCWSTR pMime, BYTE* pData, DWORD len, DWORD_PTR tag)
{
    m_resources.push_back(CDSMResource());
    return ResSet((DWORD)m_resources.size() - 1, pName, pDesc, pMime, pData, len, tag);
}

STDMETHODIMP IDSMResourceBagImpl::ResRemoveAt(DWORD iIndex)
{
    if (iIndex >= (DWORD)m_resources.size()) {
        return E_INVALIDARG;
    }

    m_resources.erase(m_resources.cbegin() + iIndex);

    return S_OK;
}

STDMETHODIMP IDSMResourceBagImpl::ResRemoveAll(DWORD_PTR tag)
{
    if (tag) {
        for (auto crit = m_resources.cend() - 1; crit >= m_resources.begin(); --crit) {
            if (crit->tag == tag) {
                m_resources.erase(crit);
            }
        }
    } else {
        m_resources.clear();
    }

    return S_OK;
}
