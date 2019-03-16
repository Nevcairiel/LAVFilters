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

#include "ILAVDecoder.h"
#include "DeCSS/DeCSSInputPin.h"

class CDecBase : public ILAVDecoder
{
public:
  CDecBase(void) {}
  virtual ~CDecBase(void) {}

  STDMETHOD(Init)() PURE;
  STDMETHOD(Decode)(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity, IMediaSample *pSample) PURE;

  // ILAVDecoder
  STDMETHODIMP InitInterfaces(ILAVVideoSettings *pSettings, ILAVVideoCallback *pCallback) { m_pSettings = pSettings; m_pCallback = pCallback; return Init(); }
  STDMETHODIMP Check() { return S_FALSE; }
  STDMETHODIMP_(REFERENCE_TIME) GetFrameDuration() { return 0; }
  STDMETHODIMP_(BOOL) IsInterlaced(BOOL bAllowGuess) { return TRUE; }
  STDMETHODIMP InitAllocator(IMemAllocator **ppAlloc) { return E_NOTIMPL; }
  STDMETHODIMP PostConnect(IPin *pPin) { return S_FALSE; }
  STDMETHODIMP BreakConnect() { return S_FALSE; }
  STDMETHODIMP_(long) GetBufferCount(long *pMaxBuffers = nullptr) { return 2; }

  STDMETHODIMP HasThreadSafeBuffers() { return S_FALSE; }

  STDMETHODIMP SetDirectOutput(BOOL bDirect) { return S_FALSE; }

  STDMETHODIMP_(DWORD) GetHWAccelNumDevices() { return 0; }
  STDMETHODIMP GetHWAccelDeviceInfo(DWORD dwIndex, BSTR *pstrDeviceName, DWORD *dwDeviceIdentifier) { return E_UNEXPECTED; }
  STDMETHODIMP GetHWAccelActiveDevice(BSTR *pstrDeviceName) { return E_UNEXPECTED; }

  STDMETHODIMP Decode(IMediaSample *pSample) {
    HRESULT hr;

    // Retrieve buffer
    BYTE *pData = nullptr;
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

    // DVD Stripping
    long nSize = pSample->GetActualDataLength();
    m_pCallback->DVDStripPacket(pData, nSize);

    if (m_pCallback->GetDecodeFlags() & LAV_VIDEO_DEC_FLAG_SAGE_HACK) {
      FFSWAP(REFERENCE_TIME, rtStart, m_rtTimestampBuffer);
      rtStop = AV_NOPTS_VALUE;
    }

    return Decode(pData, nSize, rtStart, rtStop, pSample->IsSyncPoint() == S_OK, pSample->IsDiscontinuity() == S_OK, pSample);
  }

  STDMETHODIMP Flush() {
    m_rtTimestampBuffer = 0;
    m_MpegParserState = (uint32_t)-1;
    return S_OK;
  };

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

protected:
  ILAVVideoSettings *m_pSettings = nullptr;
  ILAVVideoCallback *m_pCallback = nullptr;

  REFERENCE_TIME m_rtTimestampBuffer = AV_NOPTS_VALUE;

  uint32_t m_MpegParserState     = (uint32_t)-1;
};
