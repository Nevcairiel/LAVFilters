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

#pragma once

#include "ILAVDecoder.h"

class CDecBase : public ILAVDecoder
{
public:
  CDecBase(void) : m_pSettings(NULL), m_pCallback(NULL) {}
  virtual ~CDecBase(void) {}

  STDMETHOD(Init)() PURE;

  // ILAVDecoder
  STDMETHODIMP InitInterfaces(ILAVVideoSettings *pSettings, ILAVVideoCallback *pCallback) { m_pSettings = pSettings; m_pCallback = pCallback; return Init(); };
  STDMETHOD_(REFERENCE_TIME, GetFrameDuration)() { return 0; }

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
