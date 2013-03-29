/*
 *      Copyright (C) 2010-2013 Hendrik Leppkes
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

class CLAVSubRect : public IUnknown
{
public:
  CLAVSubRect() : m_cRef(0), rtStart(AV_NOPTS_VALUE), rtStop(AV_NOPTS_VALUE), id(0), pixels(NULL), pixelsPal(NULL), pitch(0), forced(false), freePixels(false) { memset(&position, 0, sizeof(position)); memset(&size, 0, sizeof(size));}
  ~CLAVSubRect() { SAFE_CO_FREE(pixels); SAFE_CO_FREE(pixelsPal); }

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject)
  {
    if (riid == IID_IUnknown) {
      AddRef();
      *ppvObject = (IUnknown *)this;
    } else {
      return E_NOINTERFACE;
    }
    return S_OK;
  }
  STDMETHODIMP_(ULONG) AddRef() { ULONG lRef = InterlockedIncrement( &m_cRef ); return max(ULONG(lRef),1ul); }
  STDMETHODIMP_(ULONG) Release() { ULONG lRef = InterlockedDecrement( &m_cRef ); if (lRef == 0) { m_cRef++; delete this; return 0; } return max(ULONG(lRef),1ul); }

  // Reset the reference count to 0, can be used after a copy-constructor
  STDMETHODIMP ResetRefCount() { m_cRef = 0ul; return S_OK; }

public:
  REFERENCE_TIME rtStart;  ///< Start Time
  REFERENCE_TIME rtStop;   ///< Stop time
  ULONGLONG id;            ///< Unique Identifier (same ID = same subtitle)
  POINT position;          ///< Position (relative to outputRect)
  SIZE size;               ///< Size
  LPVOID pixels;           ///< Pixel Data
  LPVOID pixelsPal;        ///< Pixel Data (in paletted form, required by dvd HLI)
  int pitch;               ///< Pitch of the subtitle lines
  bool forced;             ///< Forced/Menu

  bool freePixels;         ///< If true, pixel data is free'ed upon destroy

private:
  ULONG m_cRef;
};

class CLAVSubtitleFrame : public ISubRenderFrame, public CUnknown
{
public:
  CLAVSubtitleFrame(void);
  virtual ~CLAVSubtitleFrame(void);
  DECLARE_IUNKNOWN;

  // ISubRenderFrame
  STDMETHODIMP GetOutputRect(RECT *outputRect);
  STDMETHODIMP GetClipRect(RECT *clipRect);
  STDMETHODIMP GetBitmapCount(int *count);
  STDMETHODIMP GetBitmap(int index, ULONGLONG *id, POINT *position, SIZE *size, LPCVOID *pixels, int *pitch);

  STDMETHODIMP SetOutputRect(RECT outputRect);
  STDMETHODIMP SetClipRect(RECT clipRect);
  STDMETHODIMP AddBitmap(CLAVSubRect *subRect);

  BOOL Empty() const { return m_NumBitmaps == 0; };

private:
  RECT m_outputRect;
  RECT m_clipRect;

  CLAVSubRect **m_Bitmaps;
  int m_NumBitmaps;
};
