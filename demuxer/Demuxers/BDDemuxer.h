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

#pragma once

#include "BaseDemuxer.h"
#include "LAVFDemuxer.h"

#pragma comment(lib, "libbluray.lib")

class CBDDemuxer : public CBaseDemuxer, public IAMExtendedSeeking
{
public:
  CBDDemuxer(CCritSec *pLock, ILAVFSettingsInternal *pSettings);
  ~CBDDemuxer(void);

  // IUnknown
  DECLARE_IUNKNOWN
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  // IDispatch
  STDMETHODIMP GetTypeInfoCount(UINT* pctinfo) {return E_NOTIMPL;}
  STDMETHODIMP GetTypeInfo(UINT itinfo, LCID lcid, ITypeInfo** pptinfo) {return E_NOTIMPL;}
  STDMETHODIMP GetIDsOfNames(REFIID riid, OLECHAR** rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid) {return E_NOTIMPL;}
  STDMETHODIMP Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo, UINT* puArgErr) {return E_NOTIMPL;}

  // CBaseDemuxer
  STDMETHODIMP Open(LPCOLESTR pszFileName);
  REFERENCE_TIME GetDuration() const;
  STDMETHODIMP GetNextPacket(Packet **ppPacket);
  STDMETHODIMP Seek(REFERENCE_TIME rTime);
  const char *GetContainerFormat() const;
  HRESULT StreamInfo(const CBaseDemuxer::stream &s, LCID *plcid, WCHAR **ppszName) const;

  CStreamList *GetStreams(StreamType type) { if (m_lavfDemuxer) return m_lavfDemuxer->GetStreams(type); else return __super::GetStreams(type);  }
  HRESULT SetActiveStream(StreamType type, int pid) { if (m_lavfDemuxer) { m_lavfDemuxer->SetActiveStream(type, pid); return S_OK; } else return E_FAIL; }

  void SettingsChanged(ILAVFSettingsInternal *pSettings) { if (m_lavfDemuxer) m_lavfDemuxer->SettingsChanged(pSettings); }

  const stream* SelectVideoStream() { return m_lavfDemuxer->SelectVideoStream(); }
  const stream* SelectAudioStream(std::list<std::string> prefLanguages) { return m_lavfDemuxer->SelectAudioStream(prefLanguages); }
  const stream* SelectSubtitleStream(std::list<CSubtitleSelector> subtitleSelectors, std::string audioLanguage) { return m_lavfDemuxer->SelectSubtitleStream(subtitleSelectors, audioLanguage); }

  STDMETHODIMP SetTitle(uint32_t idx);
  STDMETHODIMP GetTitleInfo(uint32_t idx, REFERENCE_TIME *rtDuration, WCHAR **ppszName);
  STDMETHODIMP GetNumTitles(uint32_t *count);

  // IAMExtendedSeeking
  STDMETHODIMP get_ExSeekCapabilities(long* pExCapabilities);
  STDMETHODIMP get_MarkerCount(long* pMarkerCount);
  STDMETHODIMP get_CurrentMarker(long* pCurrentMarker);
  STDMETHODIMP GetMarkerTime(long MarkerNum, double* pMarkerTime);
  STDMETHODIMP GetMarkerName(long MarkerNum, BSTR* pbstrMarkerName);
  STDMETHODIMP put_PlaybackSpeed(double Speed) {return E_NOTIMPL;}
  STDMETHODIMP get_PlaybackSpeed(double* pSpeed) {return E_NOTIMPL;}

  void ProcessClipLanguages();

private:
  void ProcessStreams(int count, BLURAY_STREAM_INFO *streams);
  void ProcessBDEvents();

private:
  BLURAY *m_pBD;
  AVIOContext *m_pb;

  ILAVFSettingsInternal *m_pSettings;
  CLAVFDemuxer *m_lavfDemuxer;

  BLURAY_TITLE_INFO *m_pTitle;
  uint32_t          m_nTitleCount;

  REFERENCE_TIME *m_rtOffset;
  REFERENCE_TIME m_rtNewOffset;
  int64_t       m_bNewOffsetPos;
};
