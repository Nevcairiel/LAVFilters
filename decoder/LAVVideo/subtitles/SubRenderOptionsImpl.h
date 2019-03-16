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

#pragma once

/** Enum for the different types */
enum SubRenderOptionType {
  SROPT_TYPE_BOOL,
  SROPT_TYPE_INT,
  SROPT_TYPE_SIZE,
  SROPT_TYPE_RECT,
  SROPT_TYPE_ULONGLONG,
  SROPT_TYPE_DOUBLE,
  SROPT_TYPE_STRING,
  SROPT_TYPE_BIN
};

typedef struct SubRenderOption {
  /**
   * Name of the field
   */
  LPCSTR name;

  /**
   * The offset relative to the options structure
   */
  int offset;

  /**
   * The type of the options
   */
  enum SubRenderOptionType type;

  /**
   * Flags
   */
  int flags;
#define SROPT_FLAG_READONLY 1
} SubRenderOption;

class CSubRenderOptionsImpl : public ISubRenderOptions
{
public:
  CSubRenderOptionsImpl(const struct SubRenderOption *options, void *context) : options(options), context(context) {};
  virtual ~CSubRenderOptionsImpl(void) {};

  // ISubRenderOptions
  STDMETHODIMP GetBool     (LPCSTR field, bool      *value);
  STDMETHODIMP GetInt      (LPCSTR field, int       *value);
  STDMETHODIMP GetSize     (LPCSTR field, SIZE      *value);
  STDMETHODIMP GetRect     (LPCSTR field, RECT      *value);
  STDMETHODIMP GetUlonglong(LPCSTR field, ULONGLONG *value);
  STDMETHODIMP GetDouble   (LPCSTR field, double    *value);
  STDMETHODIMP GetString   (LPCSTR field, LPWSTR    *value, int *chars);
  STDMETHODIMP GetBin      (LPCSTR field, LPVOID    *value, int *size );

  STDMETHODIMP SetBool     (LPCSTR field, bool      value);
  STDMETHODIMP SetInt      (LPCSTR field, int       value);
  STDMETHODIMP SetSize     (LPCSTR field, SIZE      value);
  STDMETHODIMP SetRect     (LPCSTR field, RECT      value);
  STDMETHODIMP SetUlonglong(LPCSTR field, ULONGLONG value);
  STDMETHODIMP SetDouble   (LPCSTR field, double    value);
  STDMETHODIMP SetString   (LPCSTR field, LPWSTR    value, int chars);
  STDMETHODIMP SetBin      (LPCSTR field, LPVOID    value, int size );

  virtual STDMETHODIMP OnSubOptionSet(LPCSTR field) { return E_NOTIMPL; }

private:
  const SubRenderOption *options = nullptr;
  void *context                  = nullptr;
};

#define DECLARE_ISUBRENDEROPTIONS                                                                                                         \
  STDMETHODIMP GetBool     (LPCSTR field, bool      *value) { return CSubRenderOptionsImpl::GetBool(field, value); }                      \
  STDMETHODIMP GetInt      (LPCSTR field, int       *value) { return CSubRenderOptionsImpl::GetInt(field, value); }                       \
  STDMETHODIMP GetSize     (LPCSTR field, SIZE      *value) { return CSubRenderOptionsImpl::GetSize(field, value); }                      \
  STDMETHODIMP GetRect     (LPCSTR field, RECT      *value) { return CSubRenderOptionsImpl::GetRect(field, value); }                      \
  STDMETHODIMP GetUlonglong(LPCSTR field, ULONGLONG *value) { return CSubRenderOptionsImpl::GetUlonglong(field, value); }                 \
  STDMETHODIMP GetDouble   (LPCSTR field, double    *value) { return CSubRenderOptionsImpl::GetDouble(field, value); }                    \
  STDMETHODIMP GetString   (LPCSTR field, LPWSTR    *value, int *chars) { return CSubRenderOptionsImpl::GetString(field, value, chars); } \
  STDMETHODIMP GetBin      (LPCSTR field, LPVOID    *value, int *size ) { return CSubRenderOptionsImpl::GetBin(field, value, size); }     \
                                                                                                                                          \
  STDMETHODIMP SetBool     (LPCSTR field, bool      value) { return CSubRenderOptionsImpl::SetBool(field, value); }                       \
  STDMETHODIMP SetInt      (LPCSTR field, int       value) { return CSubRenderOptionsImpl::SetInt(field, value); }                        \
  STDMETHODIMP SetSize     (LPCSTR field, SIZE      value) { return CSubRenderOptionsImpl::SetSize(field, value); }                       \
  STDMETHODIMP SetRect     (LPCSTR field, RECT      value) { return CSubRenderOptionsImpl::SetRect(field, value); }                       \
  STDMETHODIMP SetUlonglong(LPCSTR field, ULONGLONG value) { return CSubRenderOptionsImpl::SetUlonglong(field, value); }                  \
  STDMETHODIMP SetDouble   (LPCSTR field, double    value) { return CSubRenderOptionsImpl::SetDouble(field, value); }                     \
  STDMETHODIMP SetString   (LPCSTR field, LPWSTR    value, int chars) { return CSubRenderOptionsImpl::SetString(field, value, chars); }   \
  STDMETHODIMP SetBin      (LPCSTR field, LPVOID    value, int size ) { return CSubRenderOptionsImpl::SetBin(field, value, size); }
