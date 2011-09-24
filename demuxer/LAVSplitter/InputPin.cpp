/*
 *      Copyright (C) 2011 Hendrik Leppkes
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
#include "InputPin.h"

#include "LAVSplitter.h"

#define READ_BUFFER_SIZE 32768


CLAVInputPin::CLAVInputPin(TCHAR *pName, CLAVSplitter *pFilter, CCritSec *pLock, HRESULT *phr)
  : CBasePin(pName, pFilter, pLock, phr, L"Input", PINDIR_INPUT), m_pAsyncReader(NULL), m_pAVIOContext(NULL), m_llPos(NULL)
{
}

CLAVInputPin::~CLAVInputPin(void)
{
  if (m_pAVIOContext) {
    av_free(m_pAVIOContext->buffer);
    av_free(m_pAVIOContext);
    m_pAVIOContext = NULL;
  }
}

STDMETHODIMP CLAVInputPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  return
    __super::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CLAVInputPin::CheckMediaType(const CMediaType* pmt)
{
  //return pmt->majortype == MEDIATYPE_Stream ? S_OK : VFW_E_TYPE_NOT_ACCEPTED;
  return S_OK;
}

HRESULT CLAVInputPin::CheckConnect(IPin* pPin)
{
  HRESULT hr;
  if(FAILED(hr = __super::CheckConnect(pPin))) {
    return hr;
  }

  IAsyncReader *pReader = NULL;
  if (FAILED(hr = pPin->QueryInterface(&pReader)) || pReader == NULL) {
    return E_FAIL;
  }

  SafeRelease(&pReader);

  return S_OK;
}

HRESULT CLAVInputPin::BreakConnect()
{
  HRESULT hr;

  if(FAILED(hr = __super::BreakConnect())) {
    return hr;
  }

  if(FAILED(hr = (static_cast<CLAVSplitter *>(m_pFilter))->BreakInputConnection())) {
    return hr;
  }

  SafeRelease(&m_pAsyncReader);

  if (m_pAVIOContext) {
    av_free(m_pAVIOContext->buffer);
    av_free(m_pAVIOContext);
    m_pAVIOContext = NULL;
  }

  return S_OK;
}

HRESULT CLAVInputPin::CompleteConnect(IPin* pPin)
{
  HRESULT hr;

  if(FAILED(hr = __super::CompleteConnect(pPin))) {
    return hr;
  }

  CheckPointer(pPin, E_POINTER);
  if (FAILED(hr = pPin->QueryInterface(&m_pAsyncReader)) || m_pAsyncReader == NULL) {
    return E_FAIL;
  }

  m_llPos = 0;

  if(FAILED(hr = (static_cast<CLAVSplitter *>(m_pFilter))->CompleteInputConnection())) {
    return hr;
  }

  return S_OK;
}

int CLAVInputPin::Read(void *opaque, uint8_t *buf, int buf_size)
{
  CLAVInputPin *pin = static_cast<CLAVInputPin *>(opaque);
  CAutoLock lock(pin);

  HRESULT hr = pin->m_pAsyncReader->SyncRead(pin->m_llPos, buf_size, buf);
  if (FAILED(hr)) {
    return -1;
  }
  if (hr == S_FALSE) {
    // read single bytes, its internally buffered..
    int count = 0;
    do {
      hr = pin->m_pAsyncReader->SyncRead(pin->m_llPos, 1, buf+count);
      pin->m_llPos++;
    } while(hr == S_OK && (++count) < buf_size);

    return count;
  }
  pin->m_llPos += buf_size;
  return buf_size;
}

int64_t CLAVInputPin::Seek(void *opaque,  int64_t offset, int whence)
{
  CLAVInputPin *pin = static_cast<CLAVInputPin *>(opaque);
  CAutoLock lock(pin);

  int64_t pos = 0;

  LONGLONG total = 0;
  LONGLONG available = 0;
  pin->m_pAsyncReader->Length(&total, &available);

  if (whence == SEEK_SET) {
    pin->m_llPos = offset;
  } else if (whence == SEEK_CUR) {
    pin->m_llPos += offset;
  } else if (whence == SEEK_END) {
    pin->m_llPos = total - offset;
  } else if (whence == AVSEEK_SIZE) {
    return total;
  } else
    return -1;

  if (pin->m_llPos > available)
    pin->m_llPos = available;

  return pin->m_llPos;
}

HRESULT CLAVInputPin::GetAVIOContext(AVIOContext** ppContext)
{
  CheckPointer(m_pAsyncReader, E_UNEXPECTED);
  CheckPointer(ppContext, E_POINTER);

  if (!m_pAVIOContext) {
    uint8_t *buffer = (uint8_t *)av_mallocz(READ_BUFFER_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);
    m_pAVIOContext = avio_alloc_context(buffer, READ_BUFFER_SIZE, 0, this, Read, NULL, Seek);
  }
  *ppContext = m_pAVIOContext;

  return S_OK;
}

STDMETHODIMP CLAVInputPin::BeginFlush()
{
  return E_UNEXPECTED;
}

STDMETHODIMP CLAVInputPin::EndFlush()
{
  return E_UNEXPECTED;
}
