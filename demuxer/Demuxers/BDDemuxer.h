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

#include "BaseDemuxer.h"
#include "LAVFDemuxer.h"

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
  STDMETHODIMP Start();
  REFERENCE_TIME GetDuration() const;
  STDMETHODIMP GetNextPacket(Packet **ppPacket) { return m_lavfDemuxer->GetNextPacket(ppPacket); }
  STDMETHODIMP Seek(REFERENCE_TIME rTime);
  STDMETHODIMP Reset() { return E_NOTIMPL; }
  const char *GetContainerFormat() const;
  virtual DWORD GetContainerFlags() { return LAVFMT_TS_DISCONT|LAVFMT_TS_DISCONT_NO_DOWNSTREAM; }

  CStreamList *GetStreams(StreamType type) { if (m_lavfDemuxer) return m_lavfDemuxer->GetStreams(type); else return __super::GetStreams(type);  }
  HRESULT SetActiveStream(StreamType type, int pid) { if (m_lavfDemuxer) { m_lavfDemuxer->SetActiveStream(type, pid); return S_OK; } else return E_FAIL; }

  void SettingsChanged(ILAVFSettingsInternal *pSettings) { if (m_lavfDemuxer) m_lavfDemuxer->SettingsChanged(pSettings); }

  const stream* SelectVideoStream() { return m_lavfDemuxer->SelectVideoStream(); }
  const stream* SelectAudioStream(std::list<std::string> prefLanguages) { return m_lavfDemuxer->SelectAudioStream(prefLanguages); }
  const stream* SelectSubtitleStream(std::list<CSubtitleSelector> subtitleSelectors, std::string audioLanguage) { return m_lavfDemuxer->SelectSubtitleStream(subtitleSelectors, audioLanguage); }

  STDMETHODIMP SetTitle(int idx);
  /*STDMETHODIMP GetTitleInfo(int idx, REFERENCE_TIME *rtDuration, WCHAR **ppszName);
  STDMETHODIMP GetNumTitles(int *count);*/

  // IAMExtendedSeeking
  STDMETHODIMP get_ExSeekCapabilities(long* pExCapabilities);
  STDMETHODIMP get_MarkerCount(long* pMarkerCount);
  STDMETHODIMP get_CurrentMarker(long* pCurrentMarker);
  STDMETHODIMP GetMarkerTime(long MarkerNum, double* pMarkerTime);
  STDMETHODIMP GetMarkerName(long MarkerNum, BSTR* pbstrMarkerName);
  STDMETHODIMP put_PlaybackSpeed(double Speed) {return E_NOTIMPL;}
  STDMETHODIMP get_PlaybackSpeed(double* pSpeed) {return E_NOTIMPL;}

  void ProcessBluRayMetadata();
  STDMETHODIMP ProcessPacket(Packet *pPacket);
  STDMETHODIMP FillMVCExtensionQueue(REFERENCE_TIME rtBase);

private:
  void ProcessClipInfo(struct clpi_cl *clpi, bool overwrite);
  void ProcessBDEvents();

  void CloseMVCExtensionDemuxer();
  STDMETHODIMP OpenMVCExtensionDemuxer(int playItem);

  static int BDByteStreamRead(void *opaque, uint8_t *buf, int buf_size);
  static int64_t CBDDemuxer::BDByteStreamSeek(void *opaque,  int64_t offset, int whence);

private:
  BLURAY      *m_pBD                      = nullptr;
  AVIOContext *m_pb                       = nullptr;
  char         m_cBDRootPath[4096]        = { 0 };

  ILAVFSettingsInternal *m_pSettings      = nullptr;
  CLAVFDemuxer          *m_lavfDemuxer    = nullptr;

  BLURAY_TITLE_INFO *m_pTitle             = nullptr;
  uint32_t           m_nTitleCount        = 0;

  uint16_t       *m_StreamClip            = nullptr;
  uint16_t        m_NewClip               = 0;
  REFERENCE_TIME *m_rtOffset              = nullptr;
  REFERENCE_TIME  m_rtNewOffset           = 0;
  int64_t         m_bNewOffsetPos         = 0;

  BOOL m_MVCPlayback                      = FALSE;
  int  m_MVCExtensionSubPathIndex         = 0;
  int  m_MVCExtensionClip                 = -1;

  AVFormatContext *m_MVCFormatContext     = nullptr;
  int              m_MVCStreamIndex       = -1;

  BOOL m_EndOfStreamPacketFlushProtection = FALSE;
};
