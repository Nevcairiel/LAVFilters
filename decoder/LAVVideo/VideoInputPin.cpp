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

#include "stdafx.h"
#include "VideoInputPin.h"
#include "ILAVDynamicAllocator.h"

CVideoInputPin::CVideoInputPin(TCHAR* pObjectName, CLAVVideo* pFilter, HRESULT* phr, LPWSTR pName)
  : CDeCSSTransformInputPin(pObjectName, pFilter, phr, pName)
  , m_pLAVVideo(pFilter)
{
}

STDMETHODIMP CVideoInputPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  return
    QI(IPinSegmentEx)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP CVideoInputPin::NotifyAllocator(IMemAllocator * pAllocator, BOOL bReadOnly)
{
  HRESULT hr = __super::NotifyAllocator(pAllocator, bReadOnly);

  m_bDynamicAllocator = FALSE;
  if (SUCCEEDED(hr) && pAllocator) {
    ILAVDynamicAllocator *pDynamicAllocator = nullptr;
    if (SUCCEEDED(pAllocator->QueryInterface(&pDynamicAllocator))) {
      m_bDynamicAllocator = pDynamicAllocator->IsDynamicAllocator();
    }
    SafeRelease(&pDynamicAllocator);
  }

  return hr;
}

STDMETHODIMP CVideoInputPin::EndOfSegment()
{
  CAutoLock lck(&m_pLAVVideo->m_csReceive);
  HRESULT hr = CheckStreaming();
  if (S_OK == hr) {
    hr = m_pLAVVideo->EndOfSegment();
  }

  return hr;
}

// IKsPropertySet

STDMETHODIMP CVideoInputPin::Set(REFGUID PropSet, ULONG Id, LPVOID pInstanceData, ULONG InstanceLength, LPVOID pPropertyData, ULONG DataLength)
{
  if(PropSet != AM_KSPROPSETID_TSRateChange) {
    return __super::Set(PropSet, Id, pInstanceData, InstanceLength, pPropertyData, DataLength);
  }

  switch(Id) {
  case AM_RATE_SimpleRateChange: {
      AM_SimpleRateChange* p = (AM_SimpleRateChange*)pPropertyData;
      if (!m_CorrectTS) {
          return E_PROP_ID_UNSUPPORTED;
      }
      CAutoLock cAutoLock(&m_csRateLock);
      m_ratechange = *p;
    }
    break;
  case AM_RATE_UseRateVersion: {
      WORD* p = (WORD*)pPropertyData;
      if (*p > 0x0101) {
          return E_PROP_ID_UNSUPPORTED;
      }
    }
    break;
  case AM_RATE_CorrectTS: {
      LONG* p = (LONG*)pPropertyData;
      m_CorrectTS = *p;
    }
    break;
  default:
    return E_PROP_ID_UNSUPPORTED;
  }

  return S_OK;
}

STDMETHODIMP CVideoInputPin::Get(REFGUID PropSet, ULONG Id, LPVOID pInstanceData, ULONG InstanceLength, LPVOID pPropertyData, ULONG DataLength, ULONG* pBytesReturned)
{
  if(PropSet != AM_KSPROPSETID_TSRateChange) {
    return __super::Get(PropSet, Id, pInstanceData, InstanceLength, pPropertyData, DataLength, pBytesReturned);
  }

  switch(Id) {
  case AM_RATE_SimpleRateChange: {
      AM_SimpleRateChange* p = (AM_SimpleRateChange*)pPropertyData;
      CAutoLock cAutoLock(&m_csRateLock);
      *p = m_ratechange;
      *pBytesReturned = sizeof(AM_SimpleRateChange);
    }
    break;
  case AM_RATE_MaxFullDataRate: {
      AM_MaxFullDataRate* p = (AM_MaxFullDataRate*)pPropertyData;
      *p = 2 * 10000;
      *pBytesReturned = sizeof(AM_MaxFullDataRate);
    }    
    break;
  case AM_RATE_QueryFullFrameRate: {
      AM_QueryRate* p = (AM_QueryRate*)pPropertyData;
      p->lMaxForwardFullFrame = 2 * 10000;
      p->lMaxReverseFullFrame = 0;
      *pBytesReturned = sizeof(AM_QueryRate);
    }
    break;
  case AM_RATE_QueryLastRateSegPTS: {
      //REFERENCE_TIME* p = (REFERENCE_TIME*)pPropertyData;
      return E_PROP_ID_UNSUPPORTED;
    }
    break;
  default:
    return E_PROP_ID_UNSUPPORTED;
  }

  return S_OK;
}

STDMETHODIMP CVideoInputPin::QuerySupported(REFGUID PropSet, ULONG Id, ULONG* pTypeSupport)
{
  if(PropSet != AM_KSPROPSETID_TSRateChange) {
    return __super::QuerySupported(PropSet, Id, pTypeSupport);
  }

  switch (Id) {
    case AM_RATE_SimpleRateChange:
        *pTypeSupport = KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET;
        break;
    case AM_RATE_MaxFullDataRate:
        *pTypeSupport = KSPROPERTY_SUPPORT_GET;
        break;
    case AM_RATE_UseRateVersion:
        *pTypeSupport = KSPROPERTY_SUPPORT_SET;
        break;
    case AM_RATE_QueryFullFrameRate:
        *pTypeSupport = KSPROPERTY_SUPPORT_GET;
        break;
    case AM_RATE_QueryLastRateSegPTS:
        *pTypeSupport = KSPROPERTY_SUPPORT_GET;
        break;
    case AM_RATE_CorrectTS:
        *pTypeSupport = KSPROPERTY_SUPPORT_SET;
        break;
    default:
        return E_PROP_ID_UNSUPPORTED;
  }

  return S_OK;
}
