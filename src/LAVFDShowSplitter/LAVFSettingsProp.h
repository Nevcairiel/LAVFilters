/*
 *      Copyright (C) 2010 Hendrik Leppkes
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

#include "cprop.h"

// GUID: a19de2f2-2f74-4927-8436-61129d26c141
DEFINE_GUID(CLSID_LAVFSettingsProp, 0xa19de2f2, 0x2f74, 
  0x4927, 0x84, 0x36, 0x61, 0x12, 0x9d, 0x26, 0xc1, 0x41);

class CLAVFSettingsProp :
  public CBasePropertyPage
{
public:
  static CUnknown* WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT* phr);

  virtual ~CLAVFSettingsProp(void);

  HRESULT OnActivate();

private:
  CLAVFSettingsProp(IUnknown *pUnk);

};
