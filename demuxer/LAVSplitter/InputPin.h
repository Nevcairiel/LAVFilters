/*
 *      Copyright (C) 2010-2015 Hendrik Leppkes
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

#include "IStreamSourceControl.h"

class CLAVSplitter;

class CLAVInputPin : public CBasePin, public CCritSec, public IStreamSourceControl
{
public:
  CLAVInputPin(TCHAR* pName, CLAVSplitter *pFilter, CCritSec* pLock, HRESULT* phr);
  ~CLAVInputPin(void);

  HRESULT GetAVIOContext(AVIOContext** ppContext);

  DECLARE_IUNKNOWN;
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  HRESULT CheckMediaType(const CMediaType* pmt);
  HRESULT CheckConnect(IPin* pPin);
  HRESULT BreakConnect();
  HRESULT CompleteConnect(IPin* pPin);

  STDMETHODIMP BeginFlush();
  STDMETHODIMP EndFlush();

  CMediaType& CurrentMediaType() { return m_mt; }

  // IStreamSourceControl
  STDMETHODIMP GetStreamDuration(REFERENCE_TIME *prtDuration) { CheckPointer(m_pStreamControl, E_NOTIMPL); return m_pStreamControl->GetStreamDuration(prtDuration); }
  STDMETHODIMP SeekStream(REFERENCE_TIME rtPosition) { CheckPointer(m_pStreamControl, E_NOTIMPL); return m_pStreamControl->SeekStream(rtPosition); }

protected:
  static int Read(void *opaque, uint8_t *buf, int buf_size);
  static int64_t Seek(void *opaque, int64_t offset, int whence);

  LONGLONG m_llPos = 0;

private:
  IAsyncReader *m_pAsyncReader = nullptr;
  AVIOContext *m_pAVIOContext  = nullptr;

  IStreamSourceControl *m_pStreamControl = nullptr;

  BOOL m_bURLSource = false;
};
