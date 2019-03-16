/*
*      Copyright (C) 2019 Hendrik Leppkes
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
#include "D3D11SurfaceAllocator.h"
#include "decoders/d3d11va.h"

extern "C" {
#include "libavutil/hwcontext.h"
}

CD3D11MediaSample::CD3D11MediaSample(CD3D11SurfaceAllocator *pAllocator, AVFrame *pFrame, HRESULT *phr)
  : CMediaSampleSideData(NAME("CD3D11MediaSample"), (CBaseAllocator*)pAllocator, phr, nullptr, 0)
  , m_pFrame(pFrame)
{
  ASSERT(m_pFrame && m_pFrame->format == AV_PIX_FMT_D3D11);
}

CD3D11MediaSample::~CD3D11MediaSample()
{
  av_frame_free(&m_pFrame);
}

//Note: CMediaSample does not derive from CUnknown, so we cannot use the
//		DECLARE_IUNKNOWN macro that is used by most of the filter classes.

STDMETHODIMP CD3D11MediaSample::QueryInterface(REFIID riid, __deref_out void **ppv)
{
  CheckPointer(ppv, E_POINTER);
  ValidateReadWritePtr(ppv, sizeof(PVOID));

  if (riid == __uuidof(IMediaSampleD3D11)) {
    return GetInterface((IMediaSampleD3D11 *) this, ppv);
  }
  else {
    return __super::QueryInterface(riid, ppv);
  }
}

STDMETHODIMP_(ULONG) CD3D11MediaSample::AddRef()
{
  return __super::AddRef();
}

STDMETHODIMP_(ULONG) CD3D11MediaSample::Release()
{
  // Return a temporary variable for thread safety.
  ULONG cRef = __super::Release();
  return cRef;
}

STDMETHODIMP CD3D11MediaSample::GetD3D11Texture(int nView, ID3D11Texture2D **ppTexture, UINT *pArraySlice)
{
  CheckPointer(ppTexture, E_POINTER);
  CheckPointer(pArraySlice, E_POINTER);

  // only view 0 is implemented at this time
  if (nView != 0)
    return E_INVALIDARG;

  if (m_pFrame)
  {
    *ppTexture   = (ID3D11Texture2D *)m_pFrame->data[0];
    *pArraySlice = (UINT)(intptr_t)m_pFrame->data[1];

    (*ppTexture)->AddRef();

    return S_OK;
  }

  return E_FAIL;
}

static void bufref_release_sample(void *opaque, uint8_t *data)
{
  CD3D11MediaSample *pSample = (CD3D11MediaSample *)opaque;
  pSample->Release();
}

STDMETHODIMP CD3D11MediaSample::GetAVFrameBuffer(AVFrame *pFrame)
{
  CheckPointer(pFrame, E_POINTER);

  // reference bufs
  for (int i = 0; i < AV_NUM_DATA_POINTERS; i++)
  {
    // copy existing refs
    if (m_pFrame->buf[i])
    {
      pFrame->buf[i] = av_buffer_ref(m_pFrame->buf[i]);
      if (pFrame->buf[i] == 0)
        return E_OUTOFMEMORY;
    }
    else
    {
      // and add a ref to this sample
      pFrame->buf[i] = av_buffer_create((uint8_t*)this, 1, bufref_release_sample, this, 0);
      if (pFrame->buf[i] == 0)
        return E_OUTOFMEMORY;

      AddRef();
      break;
    }
  }

  // ref the hwframes ctx
  pFrame->hw_frames_ctx = av_buffer_ref(m_pFrame->hw_frames_ctx);

  // copy data into the new frame
  pFrame->data[0] = m_pFrame->data[0];
  pFrame->data[1] = m_pFrame->data[1];
  pFrame->data[3] = (uint8_t *)this;

  pFrame->format = AV_PIX_FMT_D3D11;

  return S_OK;
}

CD3D11SurfaceAllocator::CD3D11SurfaceAllocator(CDecD3D11 *pDec, HRESULT *phr)
  : CBaseAllocator(NAME("CD3D11SurfaceAllocator"), nullptr, phr)
  , m_pDec(pDec)
{
}

CD3D11SurfaceAllocator::~CD3D11SurfaceAllocator()
{
}

HRESULT CD3D11SurfaceAllocator::Alloc(void)
{
  DbgLog((LOG_TRACE, 10, L"CD3D11SurfaceAllocator::Alloc()"));
  HRESULT hr = S_OK;

  CAutoLock cObjectLock(this);

  if (m_pDec == nullptr)
    return E_FAIL;

  hr = __super::Alloc();
  if (FAILED(hr))
    return hr;

  // free old resources
  //m_pDec->FlushFromAllocator();
  Free();

  // get the frames context from the decoder
  AVBufferRef *pDecoderFramesCtx = m_pDec->m_pFramesCtx;
  if (pDecoderFramesCtx == nullptr)
    return S_FALSE;

  m_pFramesCtx = av_buffer_ref(pDecoderFramesCtx);
  if (m_pFramesCtx == nullptr)
    return E_FAIL;

  // create samples
  for (int i = 0; i < m_lCount; i++)
  {
    AVFrame *pFrame = av_frame_alloc();
    int ret = av_hwframe_get_buffer(m_pFramesCtx, pFrame, 0);
    if (ret < 0)
    {
      av_frame_free(&pFrame);
      Free();
      return E_FAIL;
    }

    CD3D11MediaSample *pSample = new CD3D11MediaSample(this, pFrame, &hr);
    if (pSample == nullptr || FAILED(hr))
    {
      delete pSample;
      Free();
      return E_FAIL;
    }

    m_lFree.Add(pSample);
  }

  m_lAllocated = m_lCount;
  m_bChanged = FALSE;

  return S_OK;
}

void CD3D11SurfaceAllocator::Free(void)
{
  CAutoLock lock(this);
  CMediaSample *pSample = nullptr;

  do {
    pSample = m_lFree.RemoveHead();
    if (pSample) {
      delete pSample;
    }
  } while (pSample);

  m_lAllocated = 0;
  av_buffer_unref(&m_pFramesCtx);
}
