/*
 *      Copyright (C) 2003-2006 Gabest
 *      Copyright (C) 2010-2016 Hendrik Leppkes
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

interface __declspec(uuid("EBAFBCBE-BDE0-489A-9789-05D5692E3A93"))
IDSMResourceBag :
public IUnknown {
	STDMETHOD_(DWORD, ResGetCount)() PURE;
	STDMETHOD(ResGet)(DWORD iIndex, BSTR * ppName, BSTR * ppDesc, BSTR * ppMime, BYTE** ppData, DWORD * pDataLen, DWORD_PTR * pTag) PURE;
	STDMETHOD(ResSet)(DWORD iIndex, LPCWSTR pName, LPCWSTR pDesc, LPCWSTR pMime, const BYTE * pData, DWORD len, DWORD_PTR tag) PURE;
	STDMETHOD(ResAppend)(LPCWSTR pName, LPCWSTR pDesc, LPCWSTR pMime, BYTE * pData, DWORD len, DWORD_PTR tag) PURE;
	STDMETHOD(ResRemoveAt)(DWORD iIndex) PURE;
	STDMETHOD(ResRemoveAll)(DWORD_PTR tag) PURE;
};
