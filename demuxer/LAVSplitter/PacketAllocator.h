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
 *  Initial design and concept by Gabest and the MPC-HC Team, copyright under GPLv2
 *  Contributions by Ti-BEN from the XBMC DSPlayer Project, also under GPLv2
 */

#pragma once

#include "Packet.h"
#include "IMediaSideData.h"
#include "IMediaSideDataFFmpeg.h"
#include "ILAVDynamicAllocator.h"

interface __declspec(uuid("0B2EE323-0ED8-452D-B31E-B9B4DE2C0C39"))
ILAVMediaSample : public IUnknown  {
  STDMETHOD(SetPacket)(Packet *pPacket) PURE;
};


class CMediaPacketSample : public CMediaSample, public ILAVMediaSample, public IMediaSideData
{
public:
  CMediaPacketSample(LPCTSTR pName, CBaseAllocator *pAllocator, HRESULT *phr);
  
  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
  STDMETHODIMP_(ULONG) AddRef();
  STDMETHODIMP_(ULONG) Release();

  // ILAVMediaSamples
  STDMETHODIMP SetPacket(Packet *pPacket);

  // IMediaSideData
  STDMETHODIMP SetSideData(GUID guidType, const BYTE *pData, size_t size);
  STDMETHODIMP GetSideData(GUID guidType, const BYTE **pData, size_t *pSize);

protected:
  Packet *m_pPacket = nullptr;
  MediaSideDataFFMpeg *m_pSideData = nullptr;
};

class CPacketAllocator : public CBaseAllocator, public ILAVDynamicAllocator
{
protected:
  BOOL m_bAllocated = FALSE;

  // override to free the memory when decommit completes
  // - we actually do nothing, and save the memory until deletion.
  void Free(void);

  // called from the destructor (and from Alloc if changing size/count) to
  // actually free up the memory
  void ReallyFree(void);

  // overriden to allocate the memory when commit called
  HRESULT Alloc(void);

public:
  CPacketAllocator(LPCTSTR pName, LPUNKNOWN pUnk, HRESULT *phr);
  virtual ~CPacketAllocator(void);

  // CUnknown support
  DECLARE_IUNKNOWN;
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);

  // CBaseAllocator overrides
  STDMETHODIMP SetProperties(ALLOCATOR_PROPERTIES* pRequest, ALLOCATOR_PROPERTIES* pActual);
  STDMETHODIMP GetBuffer(IMediaSample **ppBuffer, REFERENCE_TIME *pStartTime, REFERENCE_TIME *pEndTime, DWORD dwFlags);

  // ILAVDynamicAllocator
  STDMETHODIMP_(BOOL) IsDynamicAllocator() { return TRUE; }
};
