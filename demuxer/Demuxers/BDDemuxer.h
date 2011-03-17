/*
 *      Copyright (C) 2011 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "BaseDemuxer.h"
#include "LAVFDemuxer.h"

#pragma comment(lib, "libbluray.lib")

class CBDDemuxer : public CBaseDemuxer
{
public:
  CBDDemuxer(CCritSec *pLock);
  ~CBDDemuxer(void);

  // IUnknown
  DECLARE_IUNKNOWN
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  // CBaseDemuxer
  STDMETHODIMP Open(LPCOLESTR pszFileName);
  REFERENCE_TIME GetDuration() const;
  STDMETHODIMP GetNextPacket(Packet **ppPacket);
  STDMETHODIMP Seek(REFERENCE_TIME rTime);
  const char *GetContainerFormat() const;
  HRESULT StreamInfo(DWORD streamId, LCID *plcid, WCHAR **ppszName) const;

  virtual CStreamList *GetStreams(StreamType type) { if (m_lavfDemuxer) return m_lavfDemuxer->GetStreams(type); else return __super::GetStreams(type);  }
  virtual HRESULT SetActiveStream(StreamType type, int pid) { if (m_lavfDemuxer) { m_lavfDemuxer->SetActiveStream(type, pid); return S_OK; } else return E_FAIL; }

  const stream* SelectVideoStream() { return m_lavfDemuxer->SelectVideoStream(); }
  const stream* SelectAudioStream(std::list<std::string> prefLanguages) { return m_lavfDemuxer->SelectAudioStream(prefLanguages); }
  const stream* SelectSubtitleStream(std::list<std::string> prefLanguages, int subtitleMode, BOOL bOnlyMatching) { return m_lavfDemuxer->SelectSubtitleStream(prefLanguages, subtitleMode, bOnlyMatching); }

private:
  REFERENCE_TIME Convert90KhzToDSTime(uint64_t timestamp);
  uint64_t ConvertDSTimeTo90Khz(REFERENCE_TIME timestamp);

private:
  BLURAY *m_pBD;
  AVIOContext *m_pb;

  CLAVFDemuxer *m_lavfDemuxer;

  BLURAY_TITLE_INFO *m_pTitle;
};
