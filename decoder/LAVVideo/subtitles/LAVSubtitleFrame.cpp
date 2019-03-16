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
#include "LAVSubtitleFrame.h"


CLAVSubtitleFrame::CLAVSubtitleFrame(void)
  : CUnknown(L"CLAVSubtitleFrame", nullptr)
{
  ZeroMemory(&m_outputRect, sizeof(m_outputRect));
  ZeroMemory(&m_clipRect, sizeof(m_clipRect));
}


CLAVSubtitleFrame::~CLAVSubtitleFrame(void)
{
  for (int i = 0; i < m_NumBitmaps; i++) {
    m_Bitmaps[i]->Release();
  }
  SAFE_CO_FREE(m_Bitmaps);
}

STDMETHODIMP CLAVSubtitleFrame::SetOutputRect(RECT outputRect)
{
  m_outputRect = m_clipRect = outputRect;
  return S_OK;
}

STDMETHODIMP CLAVSubtitleFrame::SetClipRect(RECT clipRect)
{
  m_clipRect = clipRect;
  return S_OK;
}

STDMETHODIMP CLAVSubtitleFrame::AddBitmap(CLAVSubRect *subRect)
{
  // Allocate memory for the new block
  void *mem = CoTaskMemRealloc(m_Bitmaps, sizeof(*m_Bitmaps) * (m_NumBitmaps+1));
  if (!mem) {
    return E_OUTOFMEMORY;
  }

  m_Bitmaps = (CLAVSubRect **)mem;
  m_Bitmaps[m_NumBitmaps] = subRect;
  m_NumBitmaps++;

  // Hold reference on the subtitle rect
  subRect->AddRef();

  return S_OK;
}

STDMETHODIMP CLAVSubtitleFrame::GetOutputRect(RECT *outputRect)
{
  CheckPointer(outputRect, E_POINTER);
  *outputRect = m_outputRect;
  return S_OK;
}

STDMETHODIMP CLAVSubtitleFrame::GetClipRect(RECT *clipRect)
{
  CheckPointer(clipRect, E_POINTER);
  *clipRect = m_clipRect;
  return S_OK;
}

STDMETHODIMP CLAVSubtitleFrame::GetBitmapCount(int *count)
{
  CheckPointer(count, E_POINTER);
  *count = m_NumBitmaps;
  return S_OK;
}

STDMETHODIMP CLAVSubtitleFrame::GetBitmap(int index, ULONGLONG *id, POINT *position, SIZE *size, LPCVOID *pixels, int *pitch)
{
  if (index < 0 || index >= m_NumBitmaps)
    return E_INVALIDARG;
  CheckPointer(id,       E_POINTER);
  CheckPointer(position, E_POINTER);
  CheckPointer(size,     E_POINTER);
  CheckPointer(pixels,   E_POINTER);
  CheckPointer(pitch,    E_POINTER);

  *id       = m_Bitmaps[index]->id;
  *position = m_Bitmaps[index]->position;
  *size     = m_Bitmaps[index]->size;
  *pixels   = m_Bitmaps[index]->pixels;
  *pitch    = m_Bitmaps[index]->pitch * 4;

  return S_OK;
}
