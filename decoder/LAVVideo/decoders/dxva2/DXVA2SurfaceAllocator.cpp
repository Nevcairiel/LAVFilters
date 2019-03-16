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
 *
 *  Initial Design and Concept taken from MPC-HC, licensed under GPLv2
 */

#include "stdafx.h"
#include "decoders/dxva2dec.h"
#include "DXVA2SurfaceAllocator.h"

#include "moreuuids.h"
#include <evr.h>
#include <Mferror.h>

CDXVA2Sample::CDXVA2Sample(CDXVA2SurfaceAllocator *pAlloc, HRESULT *phr)
  : CMediaSampleSideData(NAME("CDXVA2Sample"), (CBaseAllocator*)pAlloc, phr, nullptr, 0)
{
}

CDXVA2Sample::~CDXVA2Sample()
{
  SafeRelease(&m_pSurface);
}

//Note: CMediaSample does not derive from CUnknown, so we cannot use the
//		DECLARE_IUNKNOWN macro that is used by most of the filter classes.

STDMETHODIMP CDXVA2Sample::QueryInterface(REFIID riid, __deref_out void **ppv)
{
  CheckPointer(ppv,E_POINTER);
  ValidateReadWritePtr(ppv,sizeof(PVOID));

  if (riid == __uuidof(IMFGetService)) {
    return GetInterface((IMFGetService*) this, ppv);
  }
  else if (riid == __uuidof(ILAVDXVA2Sample)) {
    return GetInterface((ILAVDXVA2Sample*) this, ppv);
  } else {
    return __super::QueryInterface(riid, ppv);
  }
}

STDMETHODIMP_(ULONG) CDXVA2Sample::AddRef()
{
  return __super::AddRef();
}

STDMETHODIMP_(ULONG) CDXVA2Sample::Release()
{
  // Return a temporary variable for thread safety.
  ULONG cRef = __super::Release();
  return cRef;
}

// IMFGetService::GetService
STDMETHODIMP CDXVA2Sample::GetService(REFGUID guidService, REFIID riid, LPVOID *ppv)
{
  if (guidService != MR_BUFFER_SERVICE) {
    return MF_E_UNSUPPORTED_SERVICE;
  } else if (m_pSurface == nullptr) {
    return E_NOINTERFACE;
  } else {
    return m_pSurface->QueryInterface(riid, ppv);
  }
}

// Override GetPointer because this class does not manage a system memory buffer.
// The EVR uses the MR_BUFFER_SERVICE service to get the Direct3D surface.
STDMETHODIMP CDXVA2Sample::GetPointer(BYTE ** ppBuffer)
{
  return E_NOTIMPL;
}

// Sets the pointer to the Direct3D surface.
void CDXVA2Sample::SetSurface(DWORD surfaceId, IDirect3DSurface9 *pSurf)
{
  SafeRelease(&m_pSurface);
  m_pSurface = pSurf;
  m_dwSurfaceId = surfaceId;
  if (m_pSurface)
    m_pSurface->AddRef();
}

STDMETHODIMP_(int) CDXVA2Sample::GetDXSurfaceId()
{
  return m_dwSurfaceId;
}

CDXVA2SurfaceAllocator::CDXVA2SurfaceAllocator(CDecDXVA2 *m_pDXVA2Dec, HRESULT* phr)
  : CBaseAllocator(NAME("CDXVA2SurfaceAllocator"), nullptr, phr)
  , m_pDec(m_pDXVA2Dec)
{
}

CDXVA2SurfaceAllocator::~CDXVA2SurfaceAllocator(void)
{
  if (m_pDec && m_pDec->m_pDXVA2Allocator == this)
    m_pDec->m_pDXVA2Allocator = nullptr;
}

// IUnknown
STDMETHODIMP CDXVA2SurfaceAllocator::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = nullptr;

  return
    QI(ILAVDXVA2SurfaceAllocator)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CDXVA2SurfaceAllocator::Alloc()
{
  DbgLog((LOG_TRACE, 10, L"CDXVA2SurfaceAllocator::Alloc()"));
  HRESULT hr = S_OK;
  IDirectXVideoDecoderService *pDXVA2Service = nullptr;

  if (!m_pDec)
    return E_FAIL;

  CheckPointer(m_pDec->m_pD3DDevMngr, E_UNEXPECTED);
  hr = m_pDec->m_pD3DDevMngr->GetVideoService (m_pDec->m_hDevice, IID_IDirectXVideoDecoderService, (void**)&pDXVA2Service);
  CheckPointer (pDXVA2Service, E_UNEXPECTED);
  CAutoLock lock(this);

  hr = __super::Alloc();

  if (SUCCEEDED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Releasing old resources"));
    // Free the old resources.
    m_pDec->FlushFromAllocator();
    Free();

    m_nSurfaceArrayCount = m_lCount;

    // Allocate a new array of pointers.
    m_ppRTSurfaceArray = new IDirect3DSurface9*[m_lCount];
    if (m_ppRTSurfaceArray == nullptr) {
      hr = E_OUTOFMEMORY;
    } else {
      ZeroMemory(m_ppRTSurfaceArray, sizeof(IDirect3DSurface9*) * m_lCount);
    }
  }

  // Allocate the surfaces.
  if (SUCCEEDED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Allocating surfaces"));
    hr = pDXVA2Service->CreateSurface(
      m_pDec->m_dwSurfaceWidth,
      m_pDec->m_dwSurfaceHeight,
      m_lCount - 1,
      m_pDec->m_eSurfaceFormat,
      D3DPOOL_DEFAULT,
      0,
      DXVA2_VideoDecoderRenderTarget,
      m_ppRTSurfaceArray,
      nullptr
      );
  }

  if (SUCCEEDED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Creating samples"));
    // Important : create samples in reverse order !
    for (int i = m_lCount-1; i >= 0; i--) {
      CDXVA2Sample *pSample = new CDXVA2Sample(this, &hr);
      if (pSample == nullptr) {
        hr = E_OUTOFMEMORY;
        break;
      }
      if (FAILED(hr)) {
        break;
      }
      // Assign the Direct3D surface pointer and the index.
      pSample->SetSurface(i, m_ppRTSurfaceArray[i]);

      // Add to the sample list.
      m_lFree.Add(pSample);
    }

    hr = m_pDec->CreateDXVA2Decoder(m_lCount, m_ppRTSurfaceArray);
    if (FAILED (hr)) {
      Free();
    }
  }

  m_lAllocated = m_lCount;

  if (SUCCEEDED(hr)) {
    m_bChanged = FALSE;
  }
  SafeRelease(&pDXVA2Service);
  return hr;
}

void CDXVA2SurfaceAllocator::Free()
{
  DbgLog((LOG_TRACE, 10, L"CDXVA2SurfaceAllocator::Free()"));
  CMediaSample *pSample = nullptr;

  CAutoLock lock(this);

  do {
    pSample = m_lFree.RemoveHead();
    if (pSample) {
      delete pSample;
    }
  } while (pSample);

  if (m_ppRTSurfaceArray) {
    for (UINT i = 0; i < m_nSurfaceArrayCount; i++) {
      if (m_ppRTSurfaceArray[i] != nullptr) {
        m_ppRTSurfaceArray[i]->Release();
      }
    }

    delete [] m_ppRTSurfaceArray;
    m_ppRTSurfaceArray = nullptr;
  }
  m_lAllocated = 0;
  m_nSurfaceArrayCount = 0;
}
