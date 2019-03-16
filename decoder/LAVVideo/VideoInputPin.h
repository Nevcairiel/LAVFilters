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

#pragma once

#include "DeCSS/DeCSSInputPin.h"
#include "LAVVideo.h"
#include "IPinSegmentEx.h"

class CVideoInputPin : public CDeCSSTransformInputPin, public IPinSegmentEx
{
public:
  CVideoInputPin(TCHAR* pObjectName, CLAVVideo* pFilter, HRESULT* phr, LPWSTR pName);

  // CUnknown
  DECLARE_IUNKNOWN
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  // IMemInputPin
  STDMETHODIMP NotifyAllocator(IMemAllocator * pAllocator, BOOL bReadOnly);

  // IPinSegmentEx
  STDMETHODIMP EndOfSegment();

  // IKsPropertySet
  STDMETHODIMP Set(REFGUID PropSet, ULONG Id, LPVOID InstanceData, ULONG InstanceLength, LPVOID PropertyData, ULONG DataLength);
  STDMETHODIMP Get(REFGUID PropSet, ULONG Id, LPVOID InstanceData, ULONG InstanceLength, LPVOID PropertyData, ULONG DataLength, ULONG* pBytesReturned);
  STDMETHODIMP QuerySupported(REFGUID PropSet, ULONG Id, ULONG* pTypeSupport);

  AM_SimpleRateChange GetDVDRateChange() { CAutoLock cAutoLock(&m_csRateLock); return m_ratechange; }

  BOOL HasDynamicAllocator() { return m_bDynamicAllocator; }
private:
  CLAVVideo *m_pLAVVideo = nullptr;
  CCritSec m_csRateLock;

  int m_CorrectTS = 0;
  AM_SimpleRateChange m_ratechange = AM_SimpleRateChange{AV_NOPTS_VALUE, 10000};

  BOOL m_bDynamicAllocator = FALSE;
};
