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

#include "stdafx.h"
#include "BDDemuxer.h"

#define BD_READ_BUFFER_SIZE (6144 * 5)

int BDByteStreamRead(void *opaque, uint8_t *buf, int buf_size)
{
  return bd_read((BLURAY *)opaque, buf, buf_size);
}

int64_t BDByteStreamSeek(void *opaque,  int64_t offset, int whence)
{
  BLURAY *bd = (BLURAY *)opaque;

  int64_t pos = 0;
  if (whence == SEEK_SET) {
    pos = offset;
  } else if (whence == SEEK_CUR) {
    if (offset == 0)
      return bd_tell(bd);
    pos = bd_tell(bd) + offset;
  } else if (whence == SEEK_END) {
    pos = bd_get_title_size(bd) - offset;
  } else if (whence == AVSEEK_SIZE) {
    return bd_get_title_size(bd);
  } else
    return -1;
  return bd_seek(bd, pos);
}

CBDDemuxer::CBDDemuxer(CCritSec *pLock, ILAVFSettings *pSettings)
  : CBaseDemuxer(L"bluray demuxer", pLock), m_lavfDemuxer(NULL), m_pb(NULL), m_pBD(NULL), m_pTitle(NULL), m_pSettings(pSettings), m_rtOffset(0), m_rtNewOffset(Packet::INVALID_TIME), m_bNewOffsetPos(-2), m_nTitleCount(0)
{
}


CBDDemuxer::~CBDDemuxer(void)
{
  if (m_pTitle) {
    bd_free_title_info(m_pTitle);
    m_pTitle = NULL;
  }

  if (m_pBD) {
    bd_close(m_pBD);
    m_pBD = NULL;
  }

  if (m_pb) {
    av_free(m_pb->buffer);
    av_free(m_pb);
  }

  SafeRelease(&m_lavfDemuxer);
}

STDMETHODIMP CBDDemuxer::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = NULL;

  return
    QI2(IAMExtendedSeeking)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

/////////////////////////////////////////////////////////////////////////////
// Demuxer Functions
STDMETHODIMP CBDDemuxer::Open(LPCOLESTR pszFileName)
{
  CAutoLock lock(m_pLock);
  HRESULT hr = S_OK;

  int ret; // return code from C functions

  // Convert the filename from wchar to char for libbluray
  char fileName[4096];
  ret = WideCharToMultiByte(CP_UTF8, 0, pszFileName, -1, fileName, 4096, NULL, NULL);

  size_t len = strlen(fileName);
  if (len > 16) {
    char *bd_path = fileName;
    if(_strcmpi(bd_path+strlen(bd_path) - 16, "\\BDMV\\index.bdmv") == 0) {
      bd_path[strlen(bd_path) - 15] = 0;
    } else if (len > 22 && _strcmpi(bd_path+strlen(bd_path) - 22, "\\BDMV\\MovieObject.bdmv") == 0) {
      bd_path[strlen(bd_path) - 21] = 0;
    } else {
      return E_FAIL;
    }
    // Open BluRay
    BLURAY *bd = bd_open(bd_path, NULL);
    m_pBD = bd;
    // Fetch titles
    m_nTitleCount = bd_get_titles(bd, TITLES_RELEVANT);

    if (m_nTitleCount <= 0) {
      return E_FAIL;
    }

    uint64_t longest_duration = 0;
    uint32_t longest_id = 0;
    for(uint32_t i = 0; i < m_nTitleCount; i++) {
      BLURAY_TITLE_INFO *info = bd_get_title_info(bd, i);
      if (info && info->duration > longest_duration) {
        longest_id = i;
        longest_duration = info->duration;
      }
      if (info) {
        bd_free_title_info(info);
      }
    }

    SetTitle(longest_id);
  }

  return hr;
}

REFERENCE_TIME CBDDemuxer::GetDuration() const
{
  if(m_pTitle) {
    return av_rescale(m_pTitle->duration, 1000, 9);
  }
  return m_lavfDemuxer->GetDuration();
}

void CBDDemuxer::ProcessBDEvents()
{
  // Check for clip change
  BD_EVENT event;
  while(bd_get_event(m_pBD, &event)) {
    if (event.event == BD_EVENT_PLAYITEM) {
      uint64_t clip_start, clip_in, bytepos;
      int ret = bd_get_clip_infos(m_pBD, event.param, &clip_start, &clip_in, &bytepos);
      if (ret) {
        m_rtNewOffset = Convert90KhzToDSTime(clip_start - clip_in) + m_lavfDemuxer->GetStartTime();
        m_bNewOffsetPos = bytepos;
        DbgLog((LOG_TRACE, 10, L"New clip! offset: %I64d bytepos: %I64u", m_rtNewOffset, bytepos));
      }
    }
  }
}

STDMETHODIMP CBDDemuxer::GetNextPacket(Packet **ppPacket)
{
  HRESULT hr = m_lavfDemuxer->GetNextPacket(ppPacket);

  ProcessBDEvents();

  if (hr == S_OK && *ppPacket && (*ppPacket)->rtStart != Packet::INVALID_TIME) {
    if (m_rtNewOffset != Packet::INVALID_TIME && (*ppPacket)->bPosition >= m_bNewOffsetPos) {
      DbgLog((LOG_TRACE, 10, L"Actual clip change detected; time: %I64u, old offset: %I64u, new offset: %I64u", (*ppPacket)->rtStart, m_rtOffset, m_rtNewOffset));
      m_rtOffset = m_rtNewOffset;
      m_rtNewOffset = Packet::INVALID_TIME;
    }
    /*if ((*ppPacket)->StreamId == 0)
      DbgLog((LOG_TRACE, 10, L"Frame: start: %I64u, corrected: %I64u, bytepos: %I64u", (*ppPacket)->rtStart, (*ppPacket)->rtStart + m_rtOffset, (*ppPacket)->bPosition)); */
    (*ppPacket)->rtStart += m_rtOffset;
    (*ppPacket)->rtStop += m_rtOffset;
  }
  return hr;
}

STDMETHODIMP CBDDemuxer::SetTitle(int idx)
{
  int ret; // return values
  if (m_pTitle) {
    bd_free_title_info(m_pTitle);
  }
  m_pTitle = bd_get_title_info(m_pBD, idx);
  ret = bd_select_title(m_pBD, idx);
  if (ret == 0) {
    return E_FAIL;
  }

  if (m_pb) {
    av_free(m_pb->buffer);
    av_free(m_pb);
  }

  uint8_t *buffer = (uint8_t *)av_mallocz(BD_READ_BUFFER_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);
  m_pb = avio_alloc_context(buffer, BD_READ_BUFFER_SIZE, 0, m_pBD, BDByteStreamRead, NULL, BDByteStreamSeek);

  SafeRelease(&m_lavfDemuxer);

  m_lavfDemuxer = new CLAVFDemuxer(m_pLock, m_pSettings);
  m_lavfDemuxer->OpenInputStream(m_pb);
  m_lavfDemuxer->AddRef();
  m_lavfDemuxer->SeekByte(0, 0);

  DbgLog((LOG_TRACE, 20, L"Opened BD title with %d clips and %d chapters", m_pTitle->clip_count, m_pTitle->chapter_count));
  ASSERT(m_pTitle->clip_count >= 1 && m_pTitle->clips);
  for (uint32_t i = 0; i < m_pTitle->clip_count; ++i) {
    BLURAY_CLIP_INFO *clip = &m_pTitle->clips[i];
    ProcessStreams(clip->raw_stream_count, clip->raw_streams);
  }

  return S_OK;
}

STDMETHODIMP CBDDemuxer::GetNumTitles(uint32_t *count)
{
  CheckPointer(count, E_POINTER);

  *count = m_nTitleCount;

  return S_OK;
}

STDMETHODIMP CBDDemuxer::GetTitleInfo(uint32_t idx, REFERENCE_TIME *rtDuration, WCHAR **ppszName)
{
  if (idx >= m_nTitleCount) { return E_FAIL; }

  BLURAY_TITLE_INFO *info = bd_get_title_info(m_pBD, idx);
  if (info) {
    if (rtDuration) {
      *rtDuration = Convert90KhzToDSTime(info->duration);
    }
    if (ppszName) {
      WCHAR buffer[80];
      swprintf_s(buffer, L"Title %d", idx + 1);
      size_t size = (wcslen(buffer) + 1) * sizeof(WCHAR);
      *ppszName = (WCHAR *)CoTaskMemAlloc(size);
      memcpy(*ppszName, buffer, size);
    }

    return S_OK;
  }

  return E_FAIL;
}

void CBDDemuxer::ProcessStreams(int count, BLURAY_STREAM_INFO *streams)
{
  if (count <= 0 || streams == NULL) return;
  for (int i = 0; i < count; ++i) {
    BLURAY_STREAM_INFO *stream = &streams[i];
    AVStream *avstream = m_lavfDemuxer->GetAVStreamByPID(stream->pid);
    if (avstream) {
      if (stream->lang[0] != 0)
        av_metadata_set2(&avstream->metadata, "language", (const char *)stream->lang, 0);
    }
  }
}

STDMETHODIMP CBDDemuxer::Seek(REFERENCE_TIME rTime)
{
  uint64_t prev = bd_tell(m_pBD);

  int64_t target = bd_find_seek_point(m_pBD, ConvertDSTimeTo90Khz(rTime));

  DbgLog((LOG_TRACE, 1, "Seek Request: %I64u (time); %I64u (byte), %I64u (prev byte)", rTime, target, prev));
  return m_lavfDemuxer->SeekByte(target, prev > target ? AVSEEK_FLAG_BACKWARD : 0);
}

const char *CBDDemuxer::GetContainerFormat() const
{
  return "bluray";
}

HRESULT CBDDemuxer::StreamInfo(DWORD streamId, LCID *plcid, WCHAR **ppszName) const
{
  return m_lavfDemuxer->StreamInfo(streamId, plcid, ppszName);
}

REFERENCE_TIME CBDDemuxer::Convert90KhzToDSTime(uint64_t timestamp)
{
  return av_rescale(timestamp, 1000, 9);
}

uint64_t CBDDemuxer::ConvertDSTimeTo90Khz(REFERENCE_TIME timestamp)
{
  return av_rescale(timestamp, 9, 1000);
}

/////////////////////////////////////////////////////////////////////////////
// IAMExtendedSeeking
STDMETHODIMP CBDDemuxer::get_ExSeekCapabilities(long* pExCapabilities)
{
  CheckPointer(pExCapabilities, E_POINTER);
  *pExCapabilities = AM_EXSEEK_CANSEEK;
  if(m_pTitle->chapter_count > 1) *pExCapabilities |= AM_EXSEEK_MARKERSEEK;
  return S_OK;
}

STDMETHODIMP CBDDemuxer::get_MarkerCount(long* pMarkerCount)
{
  CheckPointer(pMarkerCount, E_POINTER);
  *pMarkerCount = (long)m_pTitle->chapter_count;
  return S_OK;
}

STDMETHODIMP CBDDemuxer::get_CurrentMarker(long* pCurrentMarker)
{
  CheckPointer(pCurrentMarker, E_POINTER);
  *pCurrentMarker = bd_get_current_chapter(m_pBD) + 1;
  return E_FAIL;
}


STDMETHODIMP CBDDemuxer::GetMarkerTime(long MarkerNum, double* pMarkerTime)
{
  CheckPointer(pMarkerTime, E_POINTER);
  // Chapters go by a 1-based index, doh
  unsigned int index = MarkerNum - 1;
  if(index >= m_pTitle->chapter_count) { return E_FAIL; }

  REFERENCE_TIME rt = Convert90KhzToDSTime(m_pTitle->chapters[index].start);
  *pMarkerTime = (double)rt / DSHOW_TIME_BASE;

  return S_OK;
}

STDMETHODIMP CBDDemuxer::GetMarkerName(long MarkerNum, BSTR* pbstrMarkerName)
{
  CheckPointer(pbstrMarkerName, E_POINTER);
  // Chapters go by a 1-based index, doh
  unsigned int index = MarkerNum - 1;
  if(index >= m_pTitle->chapter_count) { return E_FAIL; }
  // Get the title, or generate one
  OLECHAR wTitle[128];
  swprintf_s(wTitle, L"Chapter %d", MarkerNum);
  *pbstrMarkerName = SysAllocString(wTitle);
  return S_OK;
}
