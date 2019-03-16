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
#include "CCOutputPin.h"

#include "LAVVideo.h"

CCCOutputPin::CCCOutputPin(TCHAR* pObjectName, CLAVVideo* pFilter, CCritSec *pcsFilter, HRESULT* phr, LPWSTR pName)
  : CBaseOutputPin(pObjectName, pFilter, pcsFilter, phr, pName)
{
  m_CCmt.SetType(&MEDIATYPE_DTVCCData);
  m_CCmt.SetSubtype(&IID_MediaSideDataEIA608CC);
  m_CCmt.SetFormatType(&FORMAT_None);
  m_CCmt.SetVariableSize();
  m_CCmt.SetSampleSize(4096);
}

CCCOutputPin::~CCCOutputPin()
{
}

HRESULT CCCOutputPin::Active(void)
{
  return __super::Active();
}

STDMETHODIMP CCCOutputPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  return
    __super::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CCCOutputPin::DecideBufferSize(IMemAllocator * pAlloc, ALLOCATOR_PROPERTIES * pProperties)
{
  CheckPointer(pAlloc, E_POINTER);
  CheckPointer(pProperties, E_POINTER);

  HRESULT hr = S_OK;

  pProperties->cBuffers = max(pProperties->cBuffers, 1);
  pProperties->cbBuffer = max((ULONG)pProperties->cbBuffer, 4096);

  // Sanity checks
  ALLOCATOR_PROPERTIES Actual;
  if (FAILED(hr = pAlloc->SetProperties(pProperties, &Actual))) return hr;
  if (Actual.cbBuffer < pProperties->cbBuffer) return E_FAIL;
  ASSERT(Actual.cBuffers >= pProperties->cBuffers);

  return S_OK;
}

HRESULT CCCOutputPin::CheckMediaType(const CMediaType* pmt)
{
  if (*pmt == m_CCmt)
    return S_OK;

  return E_INVALIDARG;
}

HRESULT CCCOutputPin::GetMediaType(int iPosition, CMediaType* pmt)
{
  if (iPosition < 0 || iPosition > 0) return E_INVALIDARG;
  *pmt = m_CCmt;

  return S_OK;
}

STDMETHODIMP CCCOutputPin::DeliverCCData(BYTE *pDataIn, size_t size, REFERENCE_TIME rtTime)
{
  HRESULT hr;
  IMediaSample *pSample = nullptr;

  CHECK_HR(hr = GetDeliveryBuffer(&pSample, nullptr, nullptr, 0));

  // Resize buffer if it is too small
  // This can cause a playback hick-up, we should avoid this if possible by setting a big enough buffer size
  if (size > (size_t)pSample->GetSize()) {
    SafeRelease(&pSample);
    ALLOCATOR_PROPERTIES props, actual;
    CHECK_HR(hr = m_pAllocator->GetProperties(&props));
    // Give us 2 times the requested size, so we don't resize every time
    props.cbBuffer = size * 2;
    if (props.cBuffers > 1) {
      CHECK_HR(hr = __super::DeliverBeginFlush());
      CHECK_HR(hr = __super::DeliverEndFlush());
    }
    CHECK_HR(hr = m_pAllocator->Decommit());
    CHECK_HR(hr = m_pAllocator->SetProperties(&props, &actual));
    CHECK_HR(hr = m_pAllocator->Commit());
    CHECK_HR(hr = GetDeliveryBuffer(&pSample, nullptr, nullptr, 0));
  }

  // Fill the sample
  BYTE* pData = nullptr;
  if (FAILED(hr = pSample->GetPointer(&pData)) || !pData) goto done;

  memcpy(pData, pDataIn, size);

  // set properties
  CHECK_HR(hr = pSample->SetActualDataLength(size));
  CHECK_HR(hr = pSample->SetTime(rtTime != AV_NOPTS_VALUE ? &rtTime : nullptr, rtTime != AV_NOPTS_VALUE ? &rtTime : nullptr));
  CHECK_HR(hr = pSample->SetMediaTime(nullptr, nullptr));

  // Deliver
  CHECK_HR(hr = Deliver(pSample));

done:
  SafeRelease(&pSample);
  return hr;
}
