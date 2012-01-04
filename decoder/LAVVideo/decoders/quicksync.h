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
#include "DecBase.h"

#include "quicksync/IQuickSyncDecoder.h"

typedef IQuickSyncDecoder* __stdcall pcreateQuickSync();
typedef void               __stdcall pdestroyQuickSync(IQuickSyncDecoder*);

#include <queue>

class CDecQuickSync : public CDecBase
{
public:
  CDecQuickSync(void);
  virtual ~CDecQuickSync(void);

  // ILAVDecoder
  STDMETHODIMP InitDecoder(CodecID codec, const CMediaType *pmt);
  STDMETHODIMP Decode(IMediaSample *pSample);
  STDMETHODIMP Flush();
  STDMETHODIMP EndOfStream();
  STDMETHODIMP GetPixelFormat(LAVPixelFormat *pPix, int *pBpp);
  STDMETHODIMP_(REFERENCE_TIME) GetFrameDuration();
  STDMETHODIMP_(BOOL) IsInterlaced();

  // CDecBase
  STDMETHODIMP Init();

private:
  STDMETHODIMP DestroyDecoder(bool bFull);

  static HRESULT QS_DeliverSurfaceCallback(void* obj, QsFrameData* data);
  STDMETHODIMP HandleFrame(QsFrameData *data);

private:
  struct {
    HMODULE quickSyncLib;

    pcreateQuickSync *create;
    pdestroyQuickSync *destroy;
  } qs;

  IQuickSyncDecoder *m_pDecoder;

  BOOL m_bInterlaced;
  BOOL m_bAVC1;
  int  m_nAVCNalSize;

  FOURCC m_Codec;
};
