/*
 *      Copyright (C) 2010 Hendrik Leppkes
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
 */

#include "stdafx.h"
#include "BaseDemuxer.h"

#include "moreuuids.h"

CBaseDemuxer::CBaseDemuxer(LPCTSTR pName, CCritSec *pLock)
  : CUnknown(pName, NULL), m_pLock(pLock)
{
  for(int i = 0; i < unknown; i++) {
    m_dActiveStreams[i] = -1;
  }
}

void CBaseDemuxer::CreateNoSubtitleStream()
{
  stream s;
  s.pid = NO_SUBTITLE_PID;
  s.streamInfo = new CStreamInfo();
  s.streamInfo->mtype.majortype = MEDIATYPE_Subtitle;
  s.streamInfo->mtype.subtype = MEDIASUBTYPE_NULL;
  s.streamInfo->mtype.formattype = FORMAT_SubtitleInfo;
  // Create SUBTITLEINFO and populate it
  SUBTITLEINFO* psi = (SUBTITLEINFO *)s.streamInfo->mtype.AllocFormatBuffer(sizeof(SUBTITLEINFO));
  memset(psi, 0, sizeof(SUBTITLEINFO));
  strcpy_s(psi->IsoLang, "---");
  wcscpy_s(psi->TrackName, L"No Subtitles");
  
  // Append it to the list
  m_streams[subpic].push_front(s);
}

// CStreamList
const WCHAR* CBaseDemuxer::CStreamList::ToString(int type)
{
  return 
    type == video ? L"Video" :
    type == audio ? L"Audio" :
    type == subpic ? L"Subtitle" :
    L"Unknown";
}

const CBaseDemuxer::stream* CBaseDemuxer::CStreamList::FindStream(DWORD pid)
{
  std::deque<stream>::iterator it;
  for ( it = begin(); it != end(); it++ ) {
    if ((*it).pid == pid) {
      return &(*it);
    }
  }

  return NULL;
}

void CBaseDemuxer::CStreamList::Clear()
{
  std::deque<stream>::iterator it;
  for ( it = begin(); it != end(); it++ ) {
    delete (*it).streamInfo;
  }
  __super::clear();
}
