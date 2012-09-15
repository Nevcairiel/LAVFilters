/*
 *      Copyright (C) 2010-2012 Hendrik Leppkes
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
#include "LAVVideoSubtitleInputPin.h"

#include <dvdmedia.h>

typedef struct {
  const CLSID*        clsMajorType;
  const CLSID*        clsMinorType;
  const enum AVCodecID  nFFCodec;
} LAV_TYPE_MAP;

static const LAV_TYPE_MAP lav_subtitle_codecs[] = {
  { &MEDIATYPE_DVD_ENCRYPTED_PACK, &MEDIASUBTYPE_DVD_SUBPICTURE, AV_CODEC_ID_DVD_SUBTITLE },
  { &MEDIATYPE_MPEG2_PACK,         &MEDIASUBTYPE_DVD_SUBPICTURE, AV_CODEC_ID_DVD_SUBTITLE },
  { &MEDIATYPE_MPEG2_PES,          &MEDIASUBTYPE_DVD_SUBPICTURE, AV_CODEC_ID_DVD_SUBTITLE },
  { &MEDIATYPE_Video,              &MEDIASUBTYPE_DVD_SUBPICTURE, AV_CODEC_ID_DVD_SUBTITLE }
};

#pragma warning(push)
#pragma warning(disable:4355)
CLAVVideoSubtitleInputPin::CLAVVideoSubtitleInputPin(TCHAR* pObjectName, CBaseFilter* pFilter, CCritSec *pcsFilter, HRESULT* phr, LPWSTR pName)
  : CBaseInputPin(pObjectName, pFilter, pcsFilter, phr, pName)
  , CDeCSSPinHelper(this)
  , m_pProvider(NULL)
{
}
#pragma warning(pop)

CLAVVideoSubtitleInputPin::~CLAVVideoSubtitleInputPin(void)
{
  SafeRelease(&m_pProvider);
}

STDMETHODIMP CLAVVideoSubtitleInputPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  return
    QI(IKsPropertySet)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CLAVVideoSubtitleInputPin::CheckMediaType(const CMediaType *mtIn)
{
  for(int i = 0; i < countof(lav_subtitle_codecs); i++) {
    if(*lav_subtitle_codecs[i].clsMajorType == mtIn->majortype && *lav_subtitle_codecs[i].clsMinorType == mtIn->subtype) {
        return S_OK;
    }
  }
  return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CLAVVideoSubtitleInputPin::SetSubtitleConsumer(ISubRenderConsumer *pConsumer)
{
  m_pConsumer = pConsumer;
  return S_OK;
}

STDMETHODIMP CLAVVideoSubtitleInputPin::Receive(IMediaSample* pSample)
{
  CAutoLock lock(&m_csReceive);
  Decrypt(pSample);

  HRESULT hr = CBaseInputPin::Receive(pSample);
  if (hr == S_OK) {

  }

  return hr;
}

// IKsPropertySet

STDMETHODIMP CLAVVideoSubtitleInputPin::Set(REFGUID PropSet, ULONG Id, LPVOID pInstanceData, ULONG InstanceLength, LPVOID pPropertyData, ULONG DataLength)
{
  if (PropSet != AM_KSPROPSETID_DvdSubPic) {
    return __super::Set(PropSet, Id, pInstanceData, InstanceLength, pPropertyData, DataLength);
  }

  switch (Id) {
  case AM_PROPERTY_DVDSUBPIC_PALETTE:
    {
      DbgLog((LOG_TRACE, 10, L"New Palette"));
      CAutoLock cAutoLock(&m_csReceive);
      AM_PROPERTY_SPPAL* pSPPAL = (AM_PROPERTY_SPPAL*)pPropertyData;

    }
    break;
  case AM_PROPERTY_DVDSUBPIC_HLI:
    {
      CAutoLock cAutoLock(&m_csReceive);
      AM_PROPERTY_SPHLI* pSPHLI = (AM_PROPERTY_SPHLI*)pPropertyData;
      DbgLog((LOG_TRACE, 10, L"HLI event. HLISS: %d", pSPHLI->HLISS));
    }
    break;
  case AM_PROPERTY_DVDSUBPIC_COMPOSIT_ON:
    {
      DbgLog((LOG_TRACE, 10, L"Composit Event"));
      CAutoLock cAutoLock(&m_csReceive);
      AM_PROPERTY_COMPOSIT_ON* pCompositOn = (AM_PROPERTY_COMPOSIT_ON*)pPropertyData;
    }
    break;
  default:
    return E_PROP_ID_UNSUPPORTED;
  }

  return S_OK;
}

STDMETHODIMP CLAVVideoSubtitleInputPin::QuerySupported(REFGUID PropSet, ULONG Id, ULONG* pTypeSupport)
{
  if (PropSet != AM_KSPROPSETID_DvdSubPic) {
    return __super::QuerySupported(PropSet, Id, pTypeSupport);
  }

  switch (Id) {
  case AM_PROPERTY_DVDSUBPIC_PALETTE:
    *pTypeSupport = KSPROPERTY_SUPPORT_SET;
    break;
  case AM_PROPERTY_DVDSUBPIC_HLI:
    *pTypeSupport = KSPROPERTY_SUPPORT_SET;
    break;
  case AM_PROPERTY_DVDSUBPIC_COMPOSIT_ON:
    *pTypeSupport = KSPROPERTY_SUPPORT_SET;
    break;
  default:
    return E_PROP_ID_UNSUPPORTED;
  }

  return S_OK;
}
