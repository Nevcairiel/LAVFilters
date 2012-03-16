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

#pragma once

#include "ILAVDecoder.h"

class CDecBase : public ILAVDecoder
{
public:
  CDecBase(void) : m_pSettings(NULL), m_pCallback(NULL) {}
  virtual ~CDecBase(void) {}

  STDMETHOD(Init)() PURE;
  STDMETHOD(Decode)(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity) { return E_NOTIMPL; }

  // ILAVDecoder
  STDMETHODIMP InitInterfaces(ILAVVideoSettings *pSettings, ILAVVideoCallback *pCallback) { m_pSettings = pSettings; m_pCallback = pCallback; return Init(); }
  STDMETHODIMP Check() { return S_FALSE; }
  STDMETHODIMP_(REFERENCE_TIME) GetFrameDuration() { return 0; }
  STDMETHODIMP_(BOOL) IsInterlaced() { return TRUE; }
  STDMETHODIMP InitAllocator(IMemAllocator **ppAlloc) { return E_NOTIMPL; }
  STDMETHODIMP PostConnect(IPin *pPin) { return S_FALSE; }
  STDMETHODIMP_(long) GetBufferCount() { return 4; }

  STDMETHODIMP HasThreadSafeBuffers() { return S_FALSE; }

  STDMETHODIMP Decode(IMediaSample *pSample) {
    HRESULT hr;

    // Retrieve buffer
    BYTE *pData = NULL;
    if (FAILED(hr = pSample->GetPointer(&pData))) {
      return hr;
    }

    // Retrieve timestamps
    REFERENCE_TIME rtStart, rtStop;
    hr = pSample->GetTime(&rtStart, &rtStop);

    if (FAILED(hr)) {
      rtStart = rtStop = AV_NOPTS_VALUE;
    } else if (hr == VFW_S_NO_STOP_TIME || rtStop-1 <= rtStart) {
      rtStop = AV_NOPTS_VALUE;
    }
    return Decode(pData, pSample->GetActualDataLength(), rtStart, rtStop, pSample->IsSyncPoint() == S_OK, pSample->IsDiscontinuity() == S_OK);
  }

protected:
  // Convenience wrapper around m_pCallback
  inline HRESULT Deliver(LAVFrame *pFrame) {
    return m_pCallback->Deliver(pFrame);
  }

  inline HRESULT AllocateFrame(LAVFrame **ppFrame) {
    return m_pCallback->AllocateFrame(ppFrame);
  }

  inline HRESULT ReleaseFrame(LAVFrame **ppFrame) {
    return m_pCallback->ReleaseFrame(ppFrame);
  }

  inline BOOL FilterInGraph(PIN_DIRECTION dir, const GUID &clsid) {
    return m_pCallback->FilterInGraph(dir, clsid);
  }

protected:
  ILAVVideoSettings *m_pSettings;
  ILAVVideoCallback *m_pCallback;
};
