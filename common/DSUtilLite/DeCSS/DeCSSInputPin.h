/*
 *      Copyright (C) 2003-2006 Gabest
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
 *
 */

#pragma once

class CDeCSSPinHelper : public IKsPropertySet
{
  int m_varient;
  BYTE m_Challenge[10], m_KeyCheck[5], m_Key[10];
  BYTE m_DiscKey[6], m_TitleKey[6];

  CMediaType m_mt;

public:
  CDeCSSPinHelper();

  void Decrypt(IMediaSample* pSample);
  void StripPacket(BYTE*& p, long& len);

  void SetCSSMediaType(const CMediaType *pmt) { m_mt = *pmt; }

  // IKsPropertySet
  STDMETHODIMP Set(REFGUID PropSet, ULONG Id, LPVOID InstanceData, ULONG InstanceLength, LPVOID PropertyData, ULONG DataLength);
  STDMETHODIMP Get(REFGUID PropSet, ULONG Id, LPVOID InstanceData, ULONG InstanceLength, LPVOID PropertyData, ULONG DataLength, ULONG* pBytesReturned);
  STDMETHODIMP QuerySupported(REFGUID PropSet, ULONG Id, ULONG* pTypeSupport);
};

class CDeCSSTransformInputPin : public CTransformInputPin, public CDeCSSPinHelper
{
public:
  CDeCSSTransformInputPin(TCHAR* pObjectName, CTransformFilter* pFilter, HRESULT* phr, LPWSTR pName);

  DECLARE_IUNKNOWN
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  // IMemInputPin
  STDMETHODIMP Receive(IMediaSample* pSample);
  HRESULT SetMediaType(const CMediaType *pmt);
};
