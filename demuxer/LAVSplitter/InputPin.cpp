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
#include "InputPin.h"

#include "LAVSplitter.h"

#define READ_BUFFER_SIZE 131072


CLAVInputPin::CLAVInputPin(TCHAR *pName, CLAVSplitter *pFilter, CCritSec *pLock, HRESULT *phr)
  : CBasePin(pName, pFilter, pLock, phr, L"Input", PINDIR_INPUT)
{
}

CLAVInputPin::~CLAVInputPin(void)
{
  if (m_pAVIOContext) {
    av_free(m_pAVIOContext->buffer);
    av_free(m_pAVIOContext);
    m_pAVIOContext = nullptr;
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

  IAsyncReader *pReader = nullptr;
  if (FAILED(hr = pPin->QueryInterface(&pReader)) || pReader == nullptr) {
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
  SafeRelease(&m_pStreamControl);

  if (m_pAVIOContext) {
    av_free(m_pAVIOContext->buffer);
    av_free(m_pAVIOContext);
    m_pAVIOContext = nullptr;
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
  if (FAILED(hr = pPin->QueryInterface(&m_pAsyncReader)) || m_pAsyncReader == nullptr) {
    return E_FAIL;
  }

  m_llPos = 0;
  m_bURLSource = false;

  if (FAILED(pPin->QueryInterface(&m_pStreamControl)))
    m_pStreamControl = nullptr;

  PIN_INFO pinInfo = {0};
  if (SUCCEEDED(pPin->QueryPinInfo(&pinInfo)) && pinInfo.pFilter) {
    CLSID clsidFilter = GUID_NULL;
    if (SUCCEEDED(pinInfo.pFilter->GetClassID(&clsidFilter))) {
      m_bURLSource = (clsidFilter == CLSID_URLReader);
    }

    if (m_pStreamControl == nullptr)
      if (FAILED(pinInfo.pFilter->QueryInterface(&m_pStreamControl)))
        m_pStreamControl = nullptr;

    SafeRelease(&(pinInfo.pFilter));
  }

  if(FAILED(hr = (static_cast<CLAVSplitter *>(m_pFilter))->CompleteInputConnection())) {
    return hr;
  }

  return S_OK;
}

int CLAVInputPin::Read(void *opaque, uint8_t *buf, int buf_size)
{
  CLAVInputPin *pin = static_cast<CLAVInputPin *>(opaque);
  CAutoLock lock(pin);

  // The URL source doesn't properly signal EOF in all cases, so make sure no stale data is in the buffer
  if (pin->m_bURLSource)
    memset(buf, 0, buf_size);

  HRESULT hr = pin->m_pAsyncReader->SyncRead(pin->m_llPos, buf_size, buf);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"Read failed at pos: %I64d, hr: 0x%X", pin->m_llPos, hr));
    return -1;
  }
  if (hr == S_FALSE) {
    LONGLONG total = 0, available = 0;
    int read = 0;
    if (S_OK == pin->m_pAsyncReader->Length(&total, &available) && total >= pin->m_llPos && total <= (pin->m_llPos + buf_size)) {
      read = (int)(total - pin->m_llPos);
      DbgLog((LOG_TRACE, 10, L"At EOF, pos: %I64d, size: %I64d, remainder: %d", pin->m_llPos, total, read));
    } else {
      DbgLog((LOG_TRACE, 10, L"We're at EOF (pos: %I64d), but Length seems unreliable, trying reading manually", pin->m_llPos));
      do {
        hr = pin->m_pAsyncReader->SyncRead(pin->m_llPos, 1, buf+read);
      } while(hr == S_OK && (++read) < buf_size);
      DbgLog((LOG_TRACE, 10, L"-> Read %d bytes", read));
    }
    pin->m_llPos += read;
    return read > 0 ? read : AVERROR_EOF;
  }
  pin->m_llPos += buf_size;
  return buf_size;
}

int64_t CLAVInputPin::Seek(void *opaque,  int64_t offset, int whence)
{
  CLAVInputPin *pin = static_cast<CLAVInputPin *>(opaque);
  CAutoLock lock(pin);

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
  else if (pin->m_llPos < 0)
    pin->m_llPos = 0;

  return pin->m_llPos;
}

HRESULT CLAVInputPin::GetAVIOContext(AVIOContext** ppContext)
{
  CheckPointer(m_pAsyncReader, E_UNEXPECTED);
  CheckPointer(ppContext, E_POINTER);

  if (!m_pAVIOContext) {
    uint8_t *buffer = (uint8_t *)av_mallocz(READ_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
    m_pAVIOContext = avio_alloc_context(buffer, READ_BUFFER_SIZE, 0, this, Read, nullptr, Seek);

    LONGLONG total = 0;
    LONGLONG available = 0;
    HRESULT hr = m_pAsyncReader->Length(&total, &available);
    if (FAILED(hr) || total == 0) {
      DbgLog((LOG_TRACE, 10, L"CLAVInputPin::GetAVIOContext(): getting file length failed, disabling seeking"));
      m_pAVIOContext->seekable = 0;
      m_pAVIOContext->seek = nullptr;
      m_pAVIOContext->buffer_size = READ_BUFFER_SIZE / 4;
    }
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

STDMETHODIMP CLAVInputPin::SeekStream(REFERENCE_TIME rtPosition)
{
	CheckPointer(m_pStreamControl, E_NOTIMPL);
	HRESULT hr = m_pStreamControl->SeekStream(rtPosition);
	if (SUCCEEDED(hr))
	{
		// flush the avio context to remove any buffered data
		if (m_pAVIOContext) {
			avio_flush(m_pAVIOContext);
			m_pAVIOContext->pos = 0;
		}
	}

	return hr;
}
