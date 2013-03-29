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
#include "DecBase.h"

#include "IQuickSyncDecoder.h"

typedef IQuickSyncDecoder* __stdcall pcreateQuickSync();
typedef void               __stdcall pdestroyQuickSync(IQuickSyncDecoder*);
typedef DWORD              __stdcall pcheck();

#include <queue>

class CDecQuickSync : public CDecBase
{
public:
  CDecQuickSync(void);
  virtual ~CDecQuickSync(void);

  // ILAVDecoder
  STDMETHODIMP Check();
  STDMETHODIMP InitDecoder(AVCodecID codec, const CMediaType *pmt);
  STDMETHODIMP Decode(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity);
  STDMETHODIMP Flush();
  STDMETHODIMP EndOfStream();
  STDMETHODIMP GetPixelFormat(LAVPixelFormat *pPix, int *pBpp);
  STDMETHODIMP_(REFERENCE_TIME) GetFrameDuration();
  STDMETHODIMP_(BOOL) IsInterlaced();
  STDMETHODIMP_(const WCHAR*) GetDecoderName() { return L"quicksync"; }

  STDMETHODIMP PostConnect(IPin *pPin);

  // CDecBase
  STDMETHODIMP Init();

private:
  STDMETHODIMP DestroyDecoder(bool bFull);

  static HRESULT QS_DeliverSurfaceCallback(void* obj, QsFrameData* data);
  STDMETHODIMP HandleFrame(QsFrameData *data);

  STDMETHODIMP CheckH264Sequence(const BYTE *buffer, size_t buflen, int nal_size, int *pRefFrames = NULL, int *pProfile = NULL, int *pLevel = NULL);

private:
  struct {
    HMODULE quickSyncLib;

    pcreateQuickSync *create;
    pdestroyQuickSync *destroy;
    pcheck *check;
  } qs;

  IQuickSyncDecoder *m_pDecoder;

  BOOL m_bNeedSequenceCheck;
  BOOL m_bInterlaced;
  bool m_bDI;
  BOOL m_bAVC1;
  int  m_nAVCNalSize;
  BOOL m_bEndOfSequence;

  BOOL                   m_bUseTimestampQueue;
  std::queue<REFERENCE_TIME> m_timestampQueue;

  DXVA2_ExtendedFormat m_DXVAExtendedFormat;

  FOURCC m_Codec;

  IDirect3DDeviceManager9 *m_pD3DDevMngr;
};
