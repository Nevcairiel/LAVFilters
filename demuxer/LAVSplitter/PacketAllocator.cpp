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
 *
 *  Initial design and concept by Gabest and the MPC-HC Team, copyright under GPLv2
 *  Contributions by Ti-BEN from the XBMC DSPlayer Project, also under GPLv2
 */

#include "stdafx.h"
#include "PacketAllocator.h"

CMediaPacketSample::CMediaPacketSample(LPCTSTR pName, CBaseAllocator *pAllocator, HRESULT *phr)
  : CMediaSample(pName, pAllocator, phr)
  , m_pPacket(NULL)
{
}

STDMETHODIMP CMediaPacketSample::QueryInterface(REFIID riid, void **ppv)
{
  if (riid == __uuidof(ILAVMediaSample)) {
    return GetInterface((ILAVMediaSample *) this, ppv);
  }
  return CMediaSample::QueryInterface(riid, ppv);
}

STDMETHODIMP_(ULONG) CMediaPacketSample::AddRef()
{
  return CMediaSample::AddRef();
}

STDMETHODIMP_(ULONG) CMediaPacketSample::Release()
{
  /* Decrement our own private reference count */
  LONG lRef;
  if (m_cRef == 1) {
    lRef = 0;
    m_cRef = 0;
  } else {
    lRef = InterlockedDecrement(&m_cRef);
  }
  ASSERT(lRef >= 0);

  DbgLog((LOG_MEMORY,3,TEXT("    Unknown %X ref-- = %d"),
    this, m_cRef));

  /* Did we release our final reference count */
  if (lRef == 0) {
    /* Free all resources */
    if (m_dwFlags & Sample_TypeChanged) {
      SetMediaType(NULL);
    }
    ASSERT(m_pMediaType == NULL);
    m_dwFlags = 0;
    m_dwTypeSpecificFlags = 0;
    m_dwStreamId = AM_STREAM_MEDIA;
    
    SAFE_DELETE(m_pPacket);
    SetPointer(NULL, 0);

    /* This may cause us to be deleted */
    // Our refcount is reliably 0 thus no-one will mess with us
    m_pAllocator->ReleaseBuffer(this);
  }
  return (ULONG)lRef;
}

STDMETHODIMP CMediaPacketSample::SetPacket(Packet *pPacket)
{
  SAFE_DELETE(m_pPacket);
  m_pPacket = pPacket;
  SetPointer(pPacket->GetData(), pPacket->GetDataSize());

  return S_OK;
}

CPacketAllocator::CPacketAllocator(LPCTSTR pName, LPUNKNOWN pUnk, HRESULT *phr)
  : CBaseAllocator(pName, pUnk, phr, TRUE, TRUE)
  , m_bAllocated(FALSE)
{
}

CPacketAllocator::~CPacketAllocator(void)
{
  Decommit();
  ReallyFree();
}

STDMETHODIMP CPacketAllocator::SetProperties(ALLOCATOR_PROPERTIES* pRequest, ALLOCATOR_PROPERTIES* pActual)
{
  CheckPointer(pActual,E_POINTER);
  ValidateReadWritePtr(pActual,sizeof(ALLOCATOR_PROPERTIES));
  CAutoLock cObjectLock(this);

  ZeroMemory(pActual, sizeof(ALLOCATOR_PROPERTIES));

  ASSERT(pRequest->cbBuffer > 0);

  SYSTEM_INFO SysInfo;
  GetSystemInfo(&SysInfo);

  /*  Check the alignment request is a power of 2 */
  if ((-pRequest->cbAlign & pRequest->cbAlign) != pRequest->cbAlign) {
    DbgLog((LOG_ERROR, 1, TEXT("Alignment requested 0x%x not a power of 2!"),
      pRequest->cbAlign));
  }
  /*  Check the alignment requested */
  if (pRequest->cbAlign == 0 ||
    (SysInfo.dwAllocationGranularity & (pRequest->cbAlign - 1)) != 0) {
      DbgLog((LOG_ERROR, 1, TEXT("Invalid alignment 0x%x requested - granularity = 0x%x"),
        pRequest->cbAlign, SysInfo.dwAllocationGranularity));
      return VFW_E_BADALIGN;
  }

  /* Can't do this if already committed, there is an argument that says we
  should not reject the SetProperties call if there are buffers still
  active. However this is called by the source filter, which is the same
  person who is holding the samples. Therefore it is not unreasonable
  for them to free all their samples before changing the requirements */

  if (m_bCommitted == TRUE) {
    return VFW_E_ALREADY_COMMITTED;
  }

  /* Must be no outstanding buffers */

  if (m_lFree.GetCount() < m_lAllocated) {
    return VFW_E_BUFFERS_OUTSTANDING;
  }

  /* There isn't any real need to check the parameters as they
  will just be rejected when the user finally calls Commit */

  // round length up to alignment - remember that prefix is included in
  // the alignment
  LONG lSize = pRequest->cbBuffer + pRequest->cbPrefix;
  LONG lRemainder = lSize % pRequest->cbAlign;
  if (lRemainder != 0) {
    lSize = lSize - lRemainder + pRequest->cbAlign;
  }
  pActual->cbBuffer = m_lSize = (lSize - pRequest->cbPrefix);

  pActual->cBuffers = m_lCount = pRequest->cBuffers;
  pActual->cbAlign = m_lAlignment = pRequest->cbAlign;
  pActual->cbPrefix = m_lPrefix = pRequest->cbPrefix;

  m_bChanged = TRUE;
  return NOERROR;
}

HRESULT CPacketAllocator::Alloc(void)
{
  CAutoLock lck(this);

  /* Check he has called SetProperties */
  HRESULT hr = CBaseAllocator::Alloc();
  if (FAILED(hr)) {
    return hr;
  }

  /* If the requirements haven't changed then don't reallocate */
  if (hr == S_FALSE) {
    return NOERROR;
  }
  ASSERT(hr == S_OK); // we use this fact in the loop below

  /* Free the old resources */
  if (m_bAllocated) {
    ReallyFree();
  }

  /* Make sure we've got reasonable values */
  if ( m_lSize < 0 || m_lPrefix < 0 || m_lCount < 0 ) {
    return E_OUTOFMEMORY;
  }

  /* Compute the aligned size */
  LONG lAlignedSize = m_lSize + m_lPrefix;

  /*  Check overflow */
  if (lAlignedSize < m_lSize) {
    return E_OUTOFMEMORY;
  }

  if (m_lAlignment > 1) {
    LONG lRemainder = lAlignedSize % m_lAlignment;
    if (lRemainder != 0) {
      LONG lNewSize = lAlignedSize + m_lAlignment - lRemainder;
      if (lNewSize < lAlignedSize) {
        return E_OUTOFMEMORY;
      }
      lAlignedSize = lNewSize;
    }
  }

  /* Create the contiguous memory block for the samples
  making sure it's properly aligned (64K should be enough!)
  */
  ASSERT(lAlignedSize % m_lAlignment == 0);

  LONGLONG lToAllocate = m_lCount * (LONGLONG)lAlignedSize;

  /*  Check overflow */
  if (lToAllocate > MAXLONG) {
    return E_OUTOFMEMORY;
  }

  m_bAllocated = TRUE;

  CMediaPacketSample *pSample = NULL;

  ASSERT(m_lAllocated == 0);

  // Create the new samples - we have allocated m_lSize bytes for each sample
  // plus m_lPrefix bytes per sample as a prefix. We set the pointer to
  // the memory after the prefix - so that GetPointer() will return a pointer
  // to m_lSize bytes.
  for (; m_lAllocated < m_lCount; m_lAllocated++) {
    pSample = new CMediaPacketSample(NAME("LAV Package media sample"), this, &hr);

    ASSERT(SUCCEEDED(hr));
    if (pSample == NULL) {
      return E_OUTOFMEMORY;
    }

    // This CANNOT fail
    m_lFree.Add(pSample);
  }

  m_bChanged = FALSE;
  return NOERROR;
}


// override this to free up any resources we have allocated.
// called from the base class on Decommit when all buffers have been
// returned to the free list.
//
// caller has already locked the object.

// in our case, we keep the memory until we are deleted, so
// we do nothing here. The memory is deleted in the destructor by
// calling ReallyFree()
void CPacketAllocator::Free(void)
{
  return;
}


// called from the destructor (and from Alloc if changing size/count) to
// actually free up the memory
void CPacketAllocator::ReallyFree(void)
{
  /* Should never be deleting this unless all buffers are freed */
  ASSERT(m_lAllocated == m_lFree.GetCount());

  /* Free up all the CMediaSamples */
  CMediaSample *pSample;
  for (;;) {
    pSample = m_lFree.RemoveHead();
    if (pSample != NULL) {
      delete pSample;
    } else {
      break;
    }
  }

  m_lAllocated = 0;
  m_bAllocated = FALSE;
}
