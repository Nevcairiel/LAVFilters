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
#include "LAVVideoSubtitleInputPin.h"
#include "LAVVideo.h"

#include "moreuuids.h"

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

CLAVVideoSubtitleInputPin::CLAVVideoSubtitleInputPin(TCHAR* pObjectName, CLAVVideo* pFilter, CCritSec *pcsFilter, HRESULT* phr, LPWSTR pName)
  : CBaseInputPin(pObjectName, pFilter, pcsFilter, phr, pName)
  , m_pLAVVideo(pFilter)
  , CDeCSSPinHelper()
{
}

CLAVVideoSubtitleInputPin::~CLAVVideoSubtitleInputPin(void)
{
  SafeRelease(&m_pConsumer);
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
    if(*lav_subtitle_codecs[i].clsMajorType == mtIn->majortype && *lav_subtitle_codecs[i].clsMinorType == mtIn->subtype
      && (mtIn->formattype == FORMAT_SubtitleInfo || mtIn->formattype == FORMAT_MPEG2_VIDEO)) {
      return S_OK;
    }
  }
  return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CLAVVideoSubtitleInputPin::SetMediaType(const CMediaType *pmt)
{
  CAutoLock lock(&m_csReceive);
  SetCSSMediaType(pmt);

  ASSERT(m_pConsumer);
  SafeRelease(&m_pProvider);

  AVCodecID codecId = AV_CODEC_ID_NONE;
  for(int i = 0; i < countof(lav_subtitle_codecs); i++) {
    if(*lav_subtitle_codecs[i].clsMajorType == pmt->majortype && *lav_subtitle_codecs[i].clsMinorType == pmt->subtype) {
      codecId = lav_subtitle_codecs[i].nFFCodec;
    }
  }

  if (codecId == AV_CODEC_ID_NONE)
    return VFW_E_TYPE_NOT_ACCEPTED;

  m_pProvider = new CLAVSubtitleProvider(m_pLAVVideo, m_pConsumer);
  m_pProvider->AddRef();
  HRESULT hr = m_pProvider->InitDecoder(pmt, codecId);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"Subtitle Decoder Init failed...."));
    return hr;
  }
  return __super::SetMediaType(pmt);
}

HRESULT CLAVVideoSubtitleInputPin::BreakConnect()
{
  if (m_pProvider) {
    m_pProvider->DisconnectConsumer();
    SafeRelease(&m_pProvider);
  }
  return __super::BreakConnect();
}

STDMETHODIMP CLAVVideoSubtitleInputPin::BeginFlush()
{
  return __super::BeginFlush();
}

STDMETHODIMP CLAVVideoSubtitleInputPin::EndFlush()
{
  CAutoLock lock(&m_csReceive);
  DbgLog((LOG_TRACE, 10, L"CLAVVideoSubtitleInputPin::EndFlush()"));
  if (m_pProvider) {
    m_pProvider->Flush();
  }
  return __super::EndFlush();
}

HRESULT CLAVVideoSubtitleInputPin::SetSubtitleConsumer(ISubRenderConsumer *pConsumer)
{
  if (pConsumer == m_pConsumer)
    return S_OK;

  SafeRelease(&m_pConsumer);
  m_pConsumer = pConsumer;
  m_pConsumer->AddRef();

  if (m_pProvider) {
    m_pProvider->DisconnectConsumer();
    m_pProvider->SetConsumer(pConsumer);
  }

  return S_OK;
}

STDMETHODIMP CLAVVideoSubtitleInputPin::Receive(IMediaSample* pSample)
{
  CAutoLock lock(&m_csReceive);
  HRESULT hr = S_OK;
  Decrypt(pSample);

  ASSERT(m_pProvider);

  hr = CBaseInputPin::Receive(pSample);
  if (hr == S_OK) {
    long len = pSample->GetActualDataLength();
    BYTE *pBuffer = nullptr;
    if (FAILED(hr = pSample->GetPointer(&pBuffer))) {
      DbgLog((LOG_TRACE, 10, L"CLAVVideoSubtitleInputPin::Receive() GetPointer failed"));
      return S_OK;
    }

    StripPacket(pBuffer, len);

    REFERENCE_TIME rtStart, rtStop;
    hr = pSample->GetTime(&rtStart, &rtStop);
    if (hr == VFW_S_NO_STOP_TIME) {
      rtStop = AV_NOPTS_VALUE;
    } else if (FAILED(hr)) {
      rtStart = rtStop = AV_NOPTS_VALUE;
    }

    m_pProvider->Decode(pBuffer, len, rtStart, rtStop);
  }

  return S_OK;
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
      AM_PROPERTY_SPPAL* pSPPAL = (AM_PROPERTY_SPPAL*)pPropertyData;
      if (m_pProvider) {
        m_pProvider->SetDVDPalette(pSPPAL);
      }
    }
    break;
  case AM_PROPERTY_DVDSUBPIC_HLI:
    {
      AM_PROPERTY_SPHLI* pSPHLI = (AM_PROPERTY_SPHLI*)pPropertyData;
      if (pSPHLI->HLISS) {
        if (m_pProvider) {
          m_pProvider->SetDVDHLI(pSPHLI);
        }
      } else {
        if (m_pProvider) {
          m_pProvider->SetDVDHLI(nullptr);
        }
      }
    }
    break;
  case AM_PROPERTY_DVDSUBPIC_COMPOSIT_ON:
    {
      AM_PROPERTY_COMPOSIT_ON* pCompositOn = (AM_PROPERTY_COMPOSIT_ON*)pPropertyData;
      DbgLog((LOG_TRACE, 10, L"Composit Event - on: %d", *pCompositOn));
      m_pProvider->SetDVDComposit(*pCompositOn);
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
