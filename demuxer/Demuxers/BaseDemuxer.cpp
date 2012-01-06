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
#include "BaseDemuxer.h"

#include "moreuuids.h"

CBaseDemuxer::CBaseDemuxer(LPCTSTR pName, CCritSec *pLock)
  : CUnknown(pName, NULL), m_pLock(pLock)
{
  for(int i = 0; i < unknown; ++i) {
    m_dActiveStreams[i] = -1;
  }
}

void CBaseDemuxer::CreateNoSubtitleStream()
{
  stream s;
  s.pid = NO_SUBTITLE_PID;
  s.streamInfo = new CStreamInfo();
  s.language = "und";
  // Create the media type
  CMediaType mtype;
  mtype.majortype = MEDIATYPE_Text;
  mtype.subtype = MEDIASUBTYPE_NULL;
  mtype.formattype = MEDIASUBTYPE_NULL;
  s.streamInfo->mtypes.push_back(mtype);
  // Append it to the list
  m_streams[subpic].push_back(s);
}

void CBaseDemuxer::CreatePGSForcedSubtitleStream()
{
  stream s;
  s.pid = FORCED_SUBTITLE_PID;
  s.streamInfo = new CStreamInfo();
  s.language = "und";
  // Create the media type
  CMediaType mtype;
  mtype.majortype = MEDIATYPE_Subtitle;
  mtype.subtype = MEDIASUBTYPE_HDMVSUB;
  mtype.formattype = FORMAT_SubtitleInfo;
  SUBTITLEINFO *subInfo = (SUBTITLEINFO *)mtype.AllocFormatBuffer(sizeof(SUBTITLEINFO));
  memset(subInfo, 0, mtype.FormatLength());
  wcscpy_s(subInfo->TrackName, FORCED_SUB_STRING);
  subInfo->dwOffset = sizeof(SUBTITLEINFO);
  s.streamInfo->mtypes.push_back(mtype);
  // Append it to the list
  m_streams[subpic].push_back(s);
}

// CStreamList
const WCHAR* CBaseDemuxer::CStreamList::ToStringW(int type)
{
  return 
    type == video ? L"Video" :
    type == audio ? L"Audio" :
    type == subpic ? L"Subtitle" :
    L"Unknown";
}

const CHAR* CBaseDemuxer::CStreamList::ToString(int type)
{
  return
    type == video ? "Video" :
    type == audio ? "Audio" :
    type == subpic ? "Subtitle" :
    "Unknown";
}

CBaseDemuxer::stream* CBaseDemuxer::CStreamList::FindStream(DWORD pid)
{
  std::deque<stream>::iterator it;
  for ( it = begin(); it != end(); ++it ) {
    if ((*it).pid == pid) {
      return &(*it);
    }
  }

  return NULL;
}

void CBaseDemuxer::CStreamList::Clear()
{
  std::deque<stream>::iterator it;
  for ( it = begin(); it != end(); ++it ) {
    delete (*it).streamInfo;
  }
  __super::clear();
}
