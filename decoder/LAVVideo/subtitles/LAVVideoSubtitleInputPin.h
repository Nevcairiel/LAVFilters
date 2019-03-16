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
#include "SubRenderIntf.h"

#include "LAVSubtitleProvider.h"

class CLAVVideo;

class CLAVVideoSubtitleInputPin : public CBaseInputPin, public CDeCSSPinHelper
{
public:
  CLAVVideoSubtitleInputPin(TCHAR* pObjectName, CLAVVideo* pFilter, CCritSec *pcsFilter, HRESULT* phr, LPWSTR pName);
  virtual ~CLAVVideoSubtitleInputPin(void);

  DECLARE_IUNKNOWN
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  // IMemInputPin
  STDMETHODIMP Receive(IMediaSample* pSample);

  // CBasePin
  HRESULT CheckMediaType(const CMediaType *mtIn);
  HRESULT SetMediaType(const CMediaType *pmt);
  HRESULT BreakConnect();
  STDMETHODIMP BeginFlush();
  STDMETHODIMP EndFlush();

  HRESULT SetSubtitleConsumer(ISubRenderConsumer *pConsumer);

  // KsPropertySet
  STDMETHODIMP Set(REFGUID PropSet, ULONG Id, LPVOID InstanceData, ULONG InstanceLength, LPVOID PropertyData, ULONG DataLength);
	STDMETHODIMP QuerySupported(REFGUID PropSet, ULONG Id, ULONG* pTypeSupport);

protected:
  CCritSec m_csReceive;

  ISubRenderConsumer   *m_pConsumer = nullptr;
  CLAVSubtitleProvider *m_pProvider = nullptr;
  CLAVVideo            *m_pLAVVideo = nullptr;
};
