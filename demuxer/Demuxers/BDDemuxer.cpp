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

static inline REFERENCE_TIME Convert90KhzToDSTime(int64_t timestamp)
{
  return av_rescale(timestamp, 1000, 9);
}

static inline int64_t ConvertDSTimeTo90Khz(REFERENCE_TIME timestamp)
{
  return av_rescale(timestamp, 9, 1000);
}

#ifdef DEBUG
static void bd_log(const char *log) {
  const char *path = __FILE__;
  const char *subpath = "libbluray\\src\\";
  // skip the path these two have in common
  while (*log == *path) {
    log++;
    path++;
  }
  while (*log == *subpath) {
    log++;
    subpath++;
  }
  wchar_t line[4096] = {0};
  size_t len = strlen(log);
  if (log[len-1] == '\n') {
    len--;
  }
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, log, (int)len, line, 4096);
  DbgLog((LOG_TRACE, 40, L"[BD] %s", line));
}
#endif

CBDDemuxer::CBDDemuxer(CCritSec *pLock, ILAVFSettingsInternal *pSettings)
  : CBaseDemuxer(L"bluray demuxer", pLock)
  , m_lavfDemuxer(NULL)
  , m_pb(NULL)
  , m_pBD(NULL)
  , m_pTitle(NULL)
  , m_pSettings(pSettings)
  , m_rtOffset(NULL)
  , m_rtNewOffset(0)
  , m_bNewOffsetPos(0)
  , m_nTitleCount(0)
{
#ifdef DEBUG
  bd_set_debug_mask(DBG_FILE|DBG_BLURAY|DBG_DIR|DBG_NAV|DBG_CRIT);
  bd_set_debug_handler(&bd_log);
#endif
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
  SAFE_CO_FREE(m_rtOffset);
}

STDMETHODIMP CBDDemuxer::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = NULL;

  if (m_lavfDemuxer && (riid == __uuidof(IKeyFrameInfo) || riid == __uuidof(ITrackInfo))) {
    return m_lavfDemuxer->QueryInterface(riid, ppv);
  }

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

  int iPlaylist = -1;

  DbgLog((LOG_TRACE, 10, L"Initializing BluRay Demuxer; Entry Point: %s", pszFileName));

  size_t len = strlen(fileName);
  if (len > 16) {
    char *bd_path = fileName;
    if(_strcmpi(bd_path+strlen(bd_path) - 16, "\\BDMV\\index.bdmv") == 0) {
      bd_path[strlen(bd_path) - 15] = 0;
    } else if (len > 22 && _strcmpi(bd_path+strlen(bd_path) - 22, "\\BDMV\\MovieObject.bdmv") == 0) {
      bd_path[strlen(bd_path) - 21] = 0;
    } else if (len > 25 && _strnicmp(bd_path+strlen(bd_path) - 25, "\\BDMV\\PLAYLIST\\", 15) == 0) {
      char *playlist = &bd_path[strlen(bd_path) - 10];
      bd_path[strlen(bd_path) - 24] = 0;

      playlist[5] = 0;
      iPlaylist = atoi(playlist);
    } else {
      return E_FAIL;
    }
    // Open BluRay
    BLURAY *bd = bd_open(bd_path, NULL);
    if(!bd) {
      return E_FAIL;
    }
    m_pBD = bd;

    uint32_t timelimit = (iPlaylist != -1) ? 0 : 180;
    uint8_t flags = (iPlaylist != -1) ? TITLES_ALL : TITLES_RELEVANT;

    // Fetch titles
fetchtitles:
    m_nTitleCount = bd_get_titles(bd, flags, timelimit);

    if (m_nTitleCount <= 0) {
      if (timelimit > 0) {
        timelimit = 0;
        goto fetchtitles;
      }
      if (flags != TITLES_ALL) {
        flags = TITLES_ALL;
        goto fetchtitles;
      }
      return E_FAIL;
    }

    DbgLog((LOG_TRACE, 20, L"Found %d titles", m_nTitleCount));
    DbgLog((LOG_TRACE, 20, L" ------ Begin Title Listing ------"));

    uint64_t longest_duration = 0;
    uint32_t title_id = 0;
    boolean found = false;
    for(uint32_t i = 0; i < m_nTitleCount; i++) {
      BLURAY_TITLE_INFO *info = bd_get_title_info(bd, i, 0);
      if (info) {
        DbgLog((LOG_TRACE, 20, L"Title %u, Playlist %u (%u clips, %u chapters), Duration %I64u (%I64u seconds)", i, info->playlist, info->clip_count, info->chapter_count, info->duration, Convert90KhzToDSTime(info->duration) / DSHOW_TIME_BASE));
        if (iPlaylist != -1 && info->playlist == iPlaylist) {
          title_id = i;
          found = true;
        } else if (iPlaylist == -1 && info->duration > longest_duration) {
          title_id = i;
          longest_duration = info->duration;
        }
        bd_free_title_info(info);
      }
      if (found)
        break;
    }
    DbgLog((LOG_TRACE, 20, L" ------ End Title Listing ------"));

    hr = SetTitle(title_id);
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
      int ret = bd_get_clip_infos(m_pBD, event.param, &clip_start, &clip_in, &bytepos, NULL);
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

  Packet * const pPacket = *ppPacket;

  if (hr == S_OK && pPacket && pPacket->rtStart != Packet::INVALID_TIME) {
    REFERENCE_TIME rtOffset = m_rtOffset[pPacket->StreamId];
    if (rtOffset != m_rtNewOffset && pPacket->bPosition != -1 && pPacket->bPosition >= m_bNewOffsetPos) {
      DbgLog((LOG_TRACE, 10, L"Actual clip change detected in stream %d; time: %I64d, old offset: %I64d, new offset: %I64d", pPacket->StreamId, pPacket->rtStart, rtOffset, m_rtNewOffset));
      rtOffset = m_rtOffset[pPacket->StreamId] = m_rtNewOffset;
    }
    //DbgLog((LOG_TRACE, 10, L"Frame: stream: %d, start: %I64d, corrected: %I64d, bytepos: %I64d", pPacket->StreamId, pPacket->rtStart, pPacket->rtStart + rtOffset, pPacket->bPosition));
    pPacket->rtStart += rtOffset;
    pPacket->rtStop += rtOffset;
  }
  return hr;
}

STDMETHODIMP CBDDemuxer::SetTitle(uint32_t idx)
{
  HRESULT hr = S_OK;
  int ret; // return values
  if (m_pTitle) {
    bd_free_title_info(m_pTitle);
  }

  // Init Event Queue
  bd_get_event(m_pBD, NULL);

  // Select title
  m_pTitle = bd_get_title_info(m_pBD, idx, 0);
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
  SAFE_CO_FREE(m_rtOffset);

  m_lavfDemuxer = new CLAVFDemuxer(m_pLock, m_pSettings);
  m_lavfDemuxer->AddRef();
  m_lavfDemuxer->SetBluRay(this);
  if (FAILED(hr = m_lavfDemuxer->OpenInputStream(m_pb))) {
    SafeRelease(&m_lavfDemuxer);
    return hr;
  }

  m_lavfDemuxer->SeekByte(0, 0);

  // space for storing stream offsets
  m_rtOffset = (REFERENCE_TIME *)CoTaskMemAlloc(sizeof(REFERENCE_TIME) * m_lavfDemuxer->GetNumStreams());
  memset(m_rtOffset, 0, sizeof(REFERENCE_TIME) * m_lavfDemuxer->GetNumStreams());

  DbgLog((LOG_TRACE, 20, L"Opened BD title with %d clips and %d chapters", m_pTitle->clip_count, m_pTitle->chapter_count));
  return S_OK;
}

void CBDDemuxer::ProcessClipLanguages()
{
  ASSERT(m_pTitle->clip_count >= 1 && m_pTitle->clips);
  for (uint32_t i = 0; i < m_pTitle->clip_count; ++i) {
    BLURAY_CLIP_INFO *clip = &m_pTitle->clips[i];
    ProcessStreams(clip->raw_stream_count, clip->raw_streams);
  }
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

  BLURAY_TITLE_INFO *info = bd_get_title_info(m_pBD, idx, 0);
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
    if (!avstream) {
      DbgLog((LOG_TRACE, 10, "CBDDemuxer::ProcessStreams(): Stream with PID 0x%04x not found, trying to add it..", stream->pid));
      m_lavfDemuxer->AddMPEGTSStream(stream->pid, stream->coding_type);
      avstream = m_lavfDemuxer->GetAVStreamByPID(stream->pid);
    }
    if (avstream) {
      if (stream->lang[0] != 0)
        av_dict_set(&avstream->metadata, "language", (const char *)stream->lang, 0);
      if (avstream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
        if (avstream->codec->width == 0 || avstream->codec->height == 0) {
          switch(stream->format) {
          case BLURAY_VIDEO_FORMAT_480I:
          case BLURAY_VIDEO_FORMAT_480P:
            avstream->codec->width = 720;
            avstream->codec->height = 480;
            break;
          case BLURAY_VIDEO_FORMAT_576I:
          case BLURAY_VIDEO_FORMAT_576P:
            avstream->codec->width = 720;
            avstream->codec->height = 576;
            break;
          case BLURAY_VIDEO_FORMAT_720P:
            avstream->codec->width = 1280;
            avstream->codec->height = 720;
            break;
          case BLURAY_VIDEO_FORMAT_1080I:
          case BLURAY_VIDEO_FORMAT_1080P:
          default:
            avstream->codec->width = 1920;
            avstream->codec->height = 1080;
            break;
          }
        }
      } else if (avstream->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
        if (avstream->codec->channels == 0) {
          avstream->codec->channels = (stream->format == BLURAY_AUDIO_FORMAT_MONO) ? 1 : (stream->format == BLURAY_AUDIO_FORMAT_STEREO) ? 2 : 6;
          avstream->codec->channel_layout = av_get_default_channel_layout(avstream->codec->channels);
          avstream->codec->sample_rate = (stream->rate == BLURAY_AUDIO_RATE_96) ? 96000 : (stream->rate == BLURAY_AUDIO_RATE_192) ? 192000 : 48000;
        }
      }
    }
  }
}

STDMETHODIMP CBDDemuxer::Seek(REFERENCE_TIME rTime)
{
  int64_t prev = bd_tell(m_pBD);

  int64_t target = bd_find_seek_point(m_pBD, ConvertDSTimeTo90Khz(rTime));

  DbgLog((LOG_TRACE, 1, "Seek Request: %I64u (time); %I64u (byte), %I64u (prev byte)", rTime, target, prev));
  return m_lavfDemuxer->SeekByte(target, AVSEEK_FLAG_BACKWARD);
}

const char *CBDDemuxer::GetContainerFormat() const
{
  return "mpegts";
}

HRESULT CBDDemuxer::StreamInfo(const CBaseDemuxer::stream &s, LCID *plcid, WCHAR **ppszName) const
{
  return m_lavfDemuxer->StreamInfo(s, plcid, ppszName);
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
