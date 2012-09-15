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
#include "LAVVideoSubtitleInputPin.h"


#pragma warning(push)
#pragma warning(disable:4355)
CLAVVideoSubtitleInputPin::CLAVVideoSubtitleInputPin(TCHAR* pObjectName, CBaseFilter* pFilter, CCritSec *pcsFilter, HRESULT* phr, LPWSTR pName)
  : CBaseInputPin(pObjectName, pFilter, pcsFilter, phr, pName)
  , CDeCSSPinHelper(this)
  , m_pProvider(NULL)
{
}
#pragma warning(pop)

CLAVVideoSubtitleInputPin::~CLAVVideoSubtitleInputPin(void)
{
  SafeRelease(&m_pProvider);
}

STDMETHODIMP CLAVVideoSubtitleInputPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  return
    QI(IKsPropertySet)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CLAVVideoSubtitleInputPin::CheckMediaType(const CMediaType *mtIn)
{
  return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CLAVVideoSubtitleInputPin::CreateSubtitleProvider(ISubRenderConsumer *pConsumer)
{
  ASSERT(m_pProvider == NULL);
  m_pProvider = new CLAVSubtitleProvider(pConsumer);
  m_pProvider->AddRef();

  return S_OK;
}

STDMETHODIMP CLAVVideoSubtitleInputPin::Receive(IMediaSample* pSample)
{
  CAutoLock lock(&m_csReceive);
  Decrypt(pSample);

  HRESULT hr = CBaseInputPin::Receive(pSample);
  if (hr == S_OK) {

  }

  return hr;
}
