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
#include "SubRenderOptionsImpl.h"

static const SubRenderOption* sropt_next(const SubRenderOption *options, const SubRenderOption *last)
{
  if (!last && options[0].name) return options;
  if (last && last[1].name) return ++last;
  return nullptr;
}

static const SubRenderOption* sropt_find_option(const SubRenderOption* options, LPCSTR name, int flags)
{
  const SubRenderOption *o = nullptr;
  while (o = sropt_next(options, o)) {
    if (!_stricmp(o->name, name) && !(o->flags & flags)) {
      return o;
    }
  }
  return nullptr;
}

#define GET_OPT_AND_VALIDATE(t)                                    \
  const SubRenderOption* o = sropt_find_option(options, field, 0); \
  CheckPointer(value, E_POINTER);                                  \
  CheckPointer(o, E_INVALIDARG);                                   \
  if (o->type != t) return E_INVALIDARG;

#define GET_VALUE                                                  \
  memcpy(value, ((uint8_t*)context)+o->offset, sizeof(*value));

#define SET_OPT_AND_VALIDATE(t)                                                      \
  const SubRenderOption* o = sropt_find_option(options, field, SROPT_FLAG_READONLY); \
  CheckPointer(o, E_INVALIDARG);                                                     \
  if (o->type != t) return E_INVALIDARG;

#define SET_VALUE                                                \
  memcpy(((uint8_t*)context)+o->offset, &value, sizeof(value));  \
  OnSubOptionSet(field);

STDMETHODIMP CSubRenderOptionsImpl::GetBool(LPCSTR field, bool *value)
{
  GET_OPT_AND_VALIDATE(SROPT_TYPE_BOOL)
  GET_VALUE
  return S_OK;
}

STDMETHODIMP CSubRenderOptionsImpl::GetInt(LPCSTR field, int *value)
{
  GET_OPT_AND_VALIDATE(SROPT_TYPE_INT)
  GET_VALUE
  return S_OK;
}

STDMETHODIMP CSubRenderOptionsImpl::GetSize(LPCSTR field, SIZE *value)
{
  GET_OPT_AND_VALIDATE(SROPT_TYPE_SIZE)
  GET_VALUE
  return S_OK;
}

STDMETHODIMP CSubRenderOptionsImpl::GetRect(LPCSTR field, RECT *value)
{
  GET_OPT_AND_VALIDATE(SROPT_TYPE_RECT)
  GET_VALUE
  return S_OK;
}

STDMETHODIMP CSubRenderOptionsImpl::GetUlonglong(LPCSTR field, ULONGLONG *value)
{
  GET_OPT_AND_VALIDATE(SROPT_TYPE_ULONGLONG)
  GET_VALUE
  return S_OK;
}

STDMETHODIMP CSubRenderOptionsImpl::GetDouble(LPCSTR field, double *value)
{
  GET_OPT_AND_VALIDATE(SROPT_TYPE_DOUBLE)
  GET_VALUE
  return S_OK;
}

STDMETHODIMP CSubRenderOptionsImpl::GetString(LPCSTR field, LPWSTR *value, int *chars)
{
  GET_OPT_AND_VALIDATE(SROPT_TYPE_STRING)
  CheckPointer(chars, E_POINTER);

  const LPWSTR string = *(LPWSTR*)(((uint8_t*)context) + o->offset);
  if (!string) {
    *value = nullptr;
    *chars = 0;
    return S_OK;
  }

  *chars = (int)wcslen(string);
  *value = (LPWSTR)LocalAlloc(0, sizeof(WCHAR) * (*chars + 1));
  CheckPointer(*value, E_OUTOFMEMORY);
  wcscpy_s(*value, *chars + 1, string);

  return S_OK;
}

STDMETHODIMP CSubRenderOptionsImpl::GetBin(LPCSTR field, LPVOID *value, int *size)
{
  GET_OPT_AND_VALIDATE(SROPT_TYPE_BIN)
  CheckPointer(size, E_POINTER);
  return E_NOTIMPL;
}

STDMETHODIMP CSubRenderOptionsImpl::SetBool(LPCSTR field, bool value)
{
  SET_OPT_AND_VALIDATE(SROPT_TYPE_BOOL)
  SET_VALUE
  return S_OK;
}

STDMETHODIMP CSubRenderOptionsImpl::SetInt(LPCSTR field, int value)
{
  SET_OPT_AND_VALIDATE(SROPT_TYPE_INT)
  SET_VALUE
  return S_OK;
}

STDMETHODIMP CSubRenderOptionsImpl::SetSize(LPCSTR field, SIZE value)
{
  SET_OPT_AND_VALIDATE(SROPT_TYPE_SIZE)
  SET_VALUE
  return S_OK;
}

STDMETHODIMP CSubRenderOptionsImpl::SetRect(LPCSTR field, RECT value)
{
  SET_OPT_AND_VALIDATE(SROPT_TYPE_RECT)
  SET_VALUE
  return S_OK;
}

STDMETHODIMP CSubRenderOptionsImpl::SetUlonglong(LPCSTR field, ULONGLONG value)
{
  SET_OPT_AND_VALIDATE(SROPT_TYPE_ULONGLONG)
  SET_VALUE
  return S_OK;
}

STDMETHODIMP CSubRenderOptionsImpl::SetDouble(LPCSTR field, double value)
{
  SET_OPT_AND_VALIDATE(SROPT_TYPE_DOUBLE)
  SET_VALUE
  return S_OK;
}

STDMETHODIMP CSubRenderOptionsImpl::SetString(LPCSTR field, LPWSTR value, int chars)
{
  SET_OPT_AND_VALIDATE(SROPT_TYPE_STRING)
  return E_NOTIMPL;
}

STDMETHODIMP CSubRenderOptionsImpl::SetBin(LPCSTR field, LPVOID value, int size)
{
  SET_OPT_AND_VALIDATE(SROPT_TYPE_BIN)
  return E_NOTIMPL;
}
