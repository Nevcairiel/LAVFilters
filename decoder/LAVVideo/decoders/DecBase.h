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

#include "ILAVDecoder.h"

class CDecBase : public ILAVDecoder
{
public:
  CDecBase(void) : m_pSettings(NULL), m_pCallback(NULL) {}
  virtual ~CDecBase(void) {}

  // ILAVDecoder
  STDMETHOD(InitInterfaces)(ILAVVideoSettings *pSettings, ILAVVideoCallback *pCallback) { m_pSettings = pSettings; m_pCallback = pCallback; return S_OK; };
  STDMETHOD(InitDecoder)(CodecID codec, CMediaType *pmt) PURE;
  STDMETHOD(Decode)(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity) PURE;
  STDMETHOD(Flush)() PURE;
  STDMETHOD(EndOfStream)() PURE;

protected:
  // Convenience wrapper around m_pCallback
  inline HRESULT Deliver(LAVFrame *pFrame) {
    return m_pCallback->Deliver(pFrame);
  }

  inline HRESULT AllocateFrame(LAVFrame **ppFrame) {
    return m_pCallback->AllocateFrame(ppFrame);
  }

  inline HRESULT ReleaseFrame(LAVFrame **ppFrame) {
    return m_pCallback->ReleaseFrame(ppFrame);
  }

  inline HRESULT FilterInGraph(const GUID &clsid) {
    return m_pCallback->FilterInGraph(clsid);
  }

protected:
  ILAVVideoSettings *m_pSettings;
  ILAVVideoCallback *m_pCallback;
};
