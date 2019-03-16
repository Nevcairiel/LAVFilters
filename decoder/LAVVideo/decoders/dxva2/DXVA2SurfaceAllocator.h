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

#pragma once

#include <Mfidl.h>
#include "MediaSampleSideData.h"

class CDecDXVA2;

interface __declspec(uuid("50A8A9A1-FF44-45C1-9DC2-79066ED1E576"))
ILAVDXVA2Sample :
public IUnknown {
  STDMETHOD_(int, GetDXSurfaceId()) = 0;
};

class CDXVA2Sample : public CMediaSampleSideData, public IMFGetService, public ILAVDXVA2Sample
{
  friend class CDXVA2SurfaceAllocator;

public:
  CDXVA2Sample(CDXVA2SurfaceAllocator *pAlloc, HRESULT *phr);
  virtual ~CDXVA2Sample();

  STDMETHODIMP          QueryInterface(REFIID riid, __deref_out void **ppv);
  STDMETHODIMP_(ULONG)  AddRef();
  STDMETHODIMP_(ULONG)  Release();

  // IMFGetService::GetService
  STDMETHODIMP GetService(REFGUID guidService, REFIID riid, LPVOID *ppv);

  // ILAVDXVA2Sample
  STDMETHODIMP_(int) GetDXSurfaceId();

  // Override GetPointer because this class does not manage a system memory buffer.
  // The EVR uses the MR_BUFFER_SERVICE service to get the Direct3D surface.
  STDMETHODIMP GetPointer(BYTE ** ppBuffer);

private:

  // Sets the pointer to the Direct3D surface.
  void SetSurface(DWORD surfaceId, IDirect3DSurface9 *pSurf);

  IDirect3DSurface9 *m_pSurface   = nullptr;
  DWORD             m_dwSurfaceId = 0;
};

interface __declspec(uuid("23F80BD8-2654-4F74-B7CC-621868D0A850"))
ILAVDXVA2SurfaceAllocator :
public IUnknown {
  STDMETHOD_(void,DecoderDestruct)() = 0;
};

class CDXVA2SurfaceAllocator : public CBaseAllocator, public ILAVDXVA2SurfaceAllocator
{
public:
  CDXVA2SurfaceAllocator(CDecDXVA2 *m_pDXVA2Dec, HRESULT* phr);
  virtual ~CDXVA2SurfaceAllocator(void);

  DECLARE_IUNKNOWN;
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  HRESULT Alloc();
  void Free();
  STDMETHODIMP_(BOOL) DecommitInProgress() { CAutoLock cal(this); return m_bDecommitInProgress; }
  STDMETHODIMP_(BOOL) IsCommited() { CAutoLock cal(this); return m_bCommitted; }

  STDMETHODIMP_(void) DecoderDestruct() { m_pDec = nullptr; }

private:
  CDecDXVA2 *m_pDec = nullptr;

  IDirect3DSurface9 **m_ppRTSurfaceArray   = nullptr;
  UINT                m_nSurfaceArrayCount = 0;
};
