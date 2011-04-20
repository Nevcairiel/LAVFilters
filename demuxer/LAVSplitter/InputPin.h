/*
 *      Copyright (C) 2011 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 */

#pragma once

class CLAVSplitter;

class CLAVInputPin : public CBasePin
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

protected:
  static int Read(void *opaque, uint8_t *buf, int buf_size);
  static int64_t Seek(void *opaque, int64_t offset, int whence);

  LONGLONG m_llPos;

private:
  IAsyncReader *m_pAsyncReader;

  AVIOContext *m_pAVIOContext;
};
