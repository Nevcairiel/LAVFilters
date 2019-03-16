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
#include "BDDemuxer.h"
#include "libbluray/bdnav/mpls_parse.h"

extern "C" {
#include "libavutil/avstring.h"
};

#define BD_READ_BUFFER_SIZE (6144 * 20)

int CBDDemuxer::BDByteStreamRead(void *opaque, uint8_t *buf, int buf_size)
{
  CBDDemuxer *demux = (CBDDemuxer *)opaque;
  int ret = bd_read(demux->m_pBD, buf, buf_size);
  return (ret != 0) ? ret : AVERROR_EOF;
}

int64_t CBDDemuxer::BDByteStreamSeek(void *opaque,  int64_t offset, int whence)
{
  CBDDemuxer *demux = (CBDDemuxer *)opaque;
  BLURAY *bd = demux->m_pBD;

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
  if (pos < 0)
    pos = 0;
  int64_t achieved = bd_seek(bd, pos);
  if (pos > achieved) {
    offset = pos - achieved;
    DbgLog((LOG_TRACE, 10, L"BD Seek to %I64d, achieved %I64d, correcting target by %I64d", pos, achieved, offset));
    uint8_t *dump_buffer = (uint8_t *)CoTaskMemAlloc(6144);
    while (offset > 0) {
      int to_read  = (int)min(offset, 6144);
      int did_read = bd_read(bd, dump_buffer, to_read);
      offset -= did_read;
      if (did_read < to_read) {
        DbgLog((LOG_TRACE, 10, L"Reached EOF with %I64d offset remaining", offset));
        break;
      }
    }
    CoTaskMemFree(dump_buffer);
    achieved = bd_tell(bd);
  }

  demux->ProcessBDEvents();
  return achieved;
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
  SafeMultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, log, (int)len, line, 4096);
  DbgLog((LOG_TRACE, 40, L"[BD] %s", line));
}
#endif

CBDDemuxer::CBDDemuxer(CCritSec *pLock, ILAVFSettingsInternal *pSettings)
  : CBaseDemuxer(L"bluray demuxer", pLock)
  , m_pSettings(pSettings)
{
#ifdef DEBUG
  bd_set_debug_mask(DBG_FILE|DBG_BLURAY|DBG_DIR|DBG_NAV|DBG_CRIT);
  bd_set_debug_handler(&bd_log);
#else
  bd_set_debug_mask(0);
#endif
}


CBDDemuxer::~CBDDemuxer(void)
{
  CloseMVCExtensionDemuxer();

  if (m_pTitle) {
    bd_free_title_info(m_pTitle);
    m_pTitle = nullptr;
  }

  if (m_pBD) {
    bd_close(m_pBD);
    m_pBD = nullptr;
  }

  if (m_pb) {
    av_free(m_pb->buffer);
    av_free(m_pb);
  }

  SafeRelease(&m_lavfDemuxer);
  SAFE_CO_FREE(m_StreamClip);
  SAFE_CO_FREE(m_rtOffset);
}

STDMETHODIMP CBDDemuxer::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = nullptr;

  if (m_lavfDemuxer && (riid == __uuidof(IKeyFrameInfo) || riid == __uuidof(ITrackInfo) || riid == __uuidof(IPropertyBag))) {
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
  ret = SafeWideCharToMultiByte(CP_UTF8, 0, pszFileName, -1, fileName, 4096, nullptr, nullptr);

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
    BLURAY *bd = bd_open(bd_path, nullptr);
    if(!bd) {
      return E_FAIL;
    }
    m_pBD = bd;
    strcpy_s(m_cBDRootPath, bd_path);

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

STDMETHODIMP CBDDemuxer::Start()
{
  HRESULT hr = m_lavfDemuxer->Start();
  if (m_MVCPlayback && !m_lavfDemuxer->m_bH264MVCCombine) {
    CloseMVCExtensionDemuxer();
    m_MVCPlayback = FALSE;
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
      int ret = bd_get_clip_infos(m_pBD, event.param, &clip_start, &clip_in, &bytepos, nullptr);
      if (ret && m_lavfDemuxer->GetStartTime() != AV_NOPTS_VALUE) {
        m_rtNewOffset = Convert90KhzToDSTime(clip_start - clip_in) + m_lavfDemuxer->GetStartTime();
        m_bNewOffsetPos = bytepos-4;
        m_NewClip = event.param;
        DbgLog((LOG_TRACE, 10, L"New clip! offset: %I64d bytepos: %I64u", m_rtNewOffset, bytepos));
      }
      m_EndOfStreamPacketFlushProtection = FALSE;
    } else if (event.event == BD_EVENT_END_OF_TITLE) {
      m_EndOfStreamPacketFlushProtection = TRUE;
    } else if (event.event == BD_EVENT_SEEK) {
      m_EndOfStreamPacketFlushProtection = FALSE;
    }
  }
}

STDMETHODIMP CBDDemuxer::ProcessPacket(Packet *pPacket)
{
  ProcessBDEvents();

  if (pPacket && pPacket->rtStart != Packet::INVALID_TIME) {
    REFERENCE_TIME rtOffset = m_rtOffset[pPacket->StreamId];
    if (m_StreamClip[pPacket->StreamId] != m_NewClip && pPacket->bPosition != -1 && pPacket->bPosition >= m_bNewOffsetPos) {
      DbgLog((LOG_TRACE, 10, L"Actual clip change detected in stream %d; time: %I64d, old offset: %I64d, new offset: %I64d", pPacket->StreamId, pPacket->rtStart, rtOffset, m_rtNewOffset));
      rtOffset = m_rtOffset[pPacket->StreamId] = m_rtNewOffset;
      m_StreamClip[pPacket->StreamId] = m_NewClip;

      // Flush MVC extensions on stream change, it'll re-fill automatically
      if (m_MVCPlayback && pPacket->StreamId == m_lavfDemuxer->m_nH264MVCBaseStream && m_MVCExtensionClip != m_NewClip) {
        m_lavfDemuxer->FlushMVCExtensionQueue();
        CloseMVCExtensionDemuxer();
        OpenMVCExtensionDemuxer(m_NewClip);
      }
    }
    //DbgLog((LOG_TRACE, 10, L"Frame: stream: %d, start: %I64d, corrected: %I64d, bytepos: %I64d", pPacket->StreamId, pPacket->rtStart, pPacket->rtStart + rtOffset, pPacket->bPosition));
    pPacket->rtStart += rtOffset;
    pPacket->rtStop += rtOffset;
  }

  if (m_EndOfStreamPacketFlushProtection && pPacket && pPacket->bPosition != -1) {
    if (pPacket->bPosition < m_bNewOffsetPos) {
      DbgLog((LOG_TRACE, 10, L"Dropping packet from a previous segment (pos %I64d, segment started at %I64d) at EOS, from stream %d", pPacket->bPosition, m_bNewOffsetPos, pPacket->StreamId));
      return S_FALSE;
    }
  }

  return S_OK;
}

void CBDDemuxer::CloseMVCExtensionDemuxer()
{
  if (m_MVCFormatContext)
    avformat_close_input(&m_MVCFormatContext);

  m_MVCExtensionClip = -1;
}

STDMETHODIMP CBDDemuxer::OpenMVCExtensionDemuxer(int playItem)
{
  int ret;
  MPLS_PL *pl = bd_get_title_mpls(m_pBD);
  if (!pl)
    return E_FAIL;

  const char *clip_id = pl->ext_sub_path[m_MVCExtensionSubPathIndex].sub_play_item[playItem].clip->clip_id;
  char *fileName = av_asprintf("%sBDMV\\STREAM\\%s.m2ts", m_cBDRootPath, clip_id);

  DbgLog((LOG_TRACE, 10, "CBDDemuxer::OpenMVCExtensionDemuxer(): Opening MVC extension stream at %s", fileName));

  // Try to open the MVC stream
  AVInputFormat *format = av_find_input_format("mpegts");
  ret = avformat_open_input(&m_MVCFormatContext, fileName, format, nullptr);
  if (ret < 0) {
    DbgLog((LOG_TRACE, 10, "-> Opening MVC demuxing context failed (%d)", ret));
    goto fail;
  }

  av_opt_set_int(m_MVCFormatContext, "correct_ts_overflow", 0, 0);
  m_MVCFormatContext->flags |= AVFMT_FLAG_KEEP_SIDE_DATA;

  // Find the streams
  ret = avformat_find_stream_info(m_MVCFormatContext, nullptr);
  if (ret < 0) {
    DbgLog((LOG_TRACE, 10, "-> avformat_find_stream_info failed (%d)", ret));
    goto fail;
  }

  // Find and select our MVC stream
  DbgLog((LOG_TRACE, 10, "-> MVC m2ts has %d streams", m_MVCFormatContext->nb_streams));
  for (unsigned i = 0; i < m_MVCFormatContext->nb_streams; i++)
  {
    if (m_MVCFormatContext->streams[i]->codecpar->codec_id == AV_CODEC_ID_H264_MVC
      && m_MVCFormatContext->streams[i]->codecpar->extradata_size > 0)
    {
      m_MVCStreamIndex = i;
      break;
    }
    else {
      m_MVCFormatContext->streams[i]->discard = AVDISCARD_ALL;
    }
  }

  if (m_MVCStreamIndex < 0) {
    DbgLog((LOG_TRACE, 10, "-> MVC Stream not found"));
    goto fail;
  }

  m_MVCExtensionClip = playItem;

  return S_OK;
fail:
  CloseMVCExtensionDemuxer();
  return E_FAIL;
}

#define MVC_DEMUX_COUNT 100
STDMETHODIMP CBDDemuxer::FillMVCExtensionQueue(REFERENCE_TIME rtBase)
{
  if (!m_MVCFormatContext)
    return E_FAIL;

  int ret, count = 0;
  bool found = (rtBase == Packet::INVALID_TIME);

  AVPacket mvcPacket = { 0 };
  av_init_packet(&mvcPacket);

  while (count < MVC_DEMUX_COUNT) {
    ret = av_read_frame(m_MVCFormatContext, &mvcPacket);

    if (ret == AVERROR(EINTR) || ret == AVERROR(EAGAIN)) {
      continue;
    }
    else if (ret == AVERROR_EOF) {
      DbgLog((LOG_TRACE, 10, L"EOF reading MVC extension data"));
      break;
    }
    else if (mvcPacket.size <= 0 || mvcPacket.stream_index != m_MVCStreamIndex) {
      av_packet_unref(&mvcPacket);
      continue;
    }
    else {
      AVStream *stream = m_MVCFormatContext->streams[mvcPacket.stream_index];

      REFERENCE_TIME rtDTS = m_lavfDemuxer->ConvertTimestampToRT(mvcPacket.dts, stream->time_base.num, stream->time_base.den);
      REFERENCE_TIME rtPTS = m_lavfDemuxer->ConvertTimestampToRT(mvcPacket.pts, stream->time_base.num, stream->time_base.den);

      if (rtBase == Packet::INVALID_TIME || rtDTS == Packet::INVALID_TIME) {
        // do nothing, can't compare timestamps when they are not set
      } else if (rtDTS < rtBase) {
        DbgLog((LOG_TRACE, 10, L"CBDDemuxer::FillMVCExtensionQueue(): Dropping MVC extension at %I64d, base is %I64d", rtDTS, rtBase));
        av_packet_unref(&mvcPacket);
        continue;
      }
      else if (rtDTS == rtBase) {
        found = true;
      }

      Packet *pPacket = new Packet();
      if (!pPacket) {
        av_packet_unref(&mvcPacket);
        return E_OUTOFMEMORY;
      }

      pPacket->SetPacket(&mvcPacket);
      pPacket->rtDTS = rtDTS;
      pPacket->rtPTS = rtPTS;

      m_lavfDemuxer->QueueMVCExtension(pPacket);
      av_packet_unref(&mvcPacket);

      count++;
    }
  };

  if (found)
    return S_OK;
  else if (count > 0)
    return S_FALSE;
  else
    return E_FAIL;
}

STDMETHODIMP CBDDemuxer::SetTitle(int idx)
{
  HRESULT hr = S_OK;
  int ret; // return values
  if (m_pTitle) {
    bd_free_title_info(m_pTitle);
  }

  // Init Event Queue
  bd_get_event(m_pBD, nullptr);

  // Select title
  m_pTitle = bd_get_title_info(m_pBD, idx, 0);
  ret = bd_select_title(m_pBD, idx);
  if (ret == 0) {
    return E_FAIL;
  }

  MPLS_PL * mpls = bd_get_title_mpls(m_pBD);
  if (mpls) {
    for (int i = 0; i < mpls->ext_sub_count; i++)
    {
      if (mpls->ext_sub_path[i].type == 8
        && mpls->ext_sub_path[i].sub_playitem_count == mpls->list_count)
      {
        DbgLog((LOG_TRACE, 20, L"CBDDemuxer::SetTitle(): Enabling BD3D MVC demuxing"));
        DbgLog((LOG_TRACE, 20, L" -> MVC_Base_view_R_flag: %d", m_pTitle->mvc_base_view_r_flag));
        m_MVCPlayback = TRUE;
        m_MVCExtensionSubPathIndex = i;
        break;
      }
    }
  }

  CloseMVCExtensionDemuxer();

  if (m_pb) {
    av_free(m_pb->buffer);
    av_free(m_pb);
  }

  uint8_t *buffer = (uint8_t *)av_mallocz(BD_READ_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
  m_pb = avio_alloc_context(buffer, BD_READ_BUFFER_SIZE, 0, this, BDByteStreamRead, nullptr, BDByteStreamSeek);

  SafeRelease(&m_lavfDemuxer);
  SAFE_CO_FREE(m_StreamClip);
  SAFE_CO_FREE(m_rtOffset);

  m_lavfDemuxer = new CLAVFDemuxer(m_pLock, m_pSettings);
  m_lavfDemuxer->AddRef();
  m_lavfDemuxer->SetBluRay(this);
  if (FAILED(hr = m_lavfDemuxer->OpenInputStream(m_pb, nullptr, "mpegts", TRUE))) {
    SafeRelease(&m_lavfDemuxer);
    return hr;
  }

  if (m_MVCPlayback && !m_lavfDemuxer->m_bH264MVCCombine)
  {
    DbgLog((LOG_TRACE, 10, L"CBDDemuxer::SetTitle(): MVC demuxing was requested, but main demuxer did not activate MVC mode, disabling."));
    CloseMVCExtensionDemuxer();
    m_MVCPlayback = FALSE;
  }

  m_lavfDemuxer->SeekByte(0, 0);

  // Process any events that occured during opening
  ProcessBDEvents();

  // Reset EOS protection
  m_EndOfStreamPacketFlushProtection = FALSE;

  // space for storing stream offsets
  m_StreamClip = (uint16_t *)CoTaskMemAlloc(sizeof(*m_StreamClip) * m_lavfDemuxer->GetNumStreams());
  if (!m_StreamClip)
    return E_OUTOFMEMORY;
  memset(m_StreamClip, -1, sizeof(*m_StreamClip) * m_lavfDemuxer->GetNumStreams());

  m_rtOffset = (REFERENCE_TIME *)CoTaskMemAlloc(sizeof(*m_rtOffset) * m_lavfDemuxer->GetNumStreams());
  if (!m_rtOffset)
    return E_OUTOFMEMORY;
  memset(m_rtOffset, 0, sizeof(*m_rtOffset) * m_lavfDemuxer->GetNumStreams());

  DbgLog((LOG_TRACE, 20, L"Opened BD title with %d clips and %d chapters", m_pTitle->clip_count, m_pTitle->chapter_count));
  return S_OK;
}

void CBDDemuxer::ProcessBluRayMetadata()
{
  if (m_MVCPlayback) {
    HRESULT hr = OpenMVCExtensionDemuxer(m_NewClip);
    if (SUCCEEDED(hr)) {
      AVStream *mvcStream = m_MVCFormatContext->streams[m_MVCStreamIndex];

      // Create a fake stream and set the appropriate properties
      m_lavfDemuxer->AddMPEGTSStream(mvcStream->id, 0x20);
      AVStream *avstream = m_lavfDemuxer->GetAVStreamByPID(mvcStream->id);
      if (avstream) {
        avstream->codecpar->codec_id = AV_CODEC_ID_H264_MVC;
        avstream->codecpar->extradata = (BYTE *)av_mallocz(mvcStream->codecpar->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
        avstream->codecpar->extradata_size = mvcStream->codecpar->extradata_size;
        memcpy(avstream->codecpar->extradata, mvcStream->codecpar->extradata, mvcStream->codecpar->extradata_size);
      }
    }
    else {
      m_MVCPlayback = FALSE;
    }
  }

  ASSERT(m_pTitle->clip_count >= 1 && m_pTitle->clips);
  int64_t max_clip_duration = 0;
  for (uint32_t i = 0; i < m_pTitle->clip_count; ++i) {
    int64_t clip_duration = (m_pTitle->clips[i].out_time - m_pTitle->clips[i].in_time);
    bool overwrite_info = false;
    if (clip_duration > max_clip_duration) {
      overwrite_info = true;
      max_clip_duration = clip_duration;
    }
    CLPI_CL *clpi = bd_get_clpi(m_pBD, i);
    ProcessClipInfo(clpi, overwrite_info);
    bd_free_clpi(clpi);
  }

  MPLS_PL * mpls = bd_get_title_mpls(m_pBD);
  if (mpls) {
    // Read the PG offsets and store them as metadata
    std::list<uint8_t> pg_sequences;
    for (int i = 0; i < mpls->play_item[0].stn.num_pg; i++) {
      AVStream *avstream = m_lavfDemuxer->GetAVStreamByPID(mpls->play_item[0].stn.pg[i].pid);
      if (mpls->play_item[0].stn.pg[i].ss_offset_sequence_id != 0xFF) {
        pg_sequences.push_back(mpls->play_item[0].stn.pg[i].ss_offset_sequence_id);
        if (avstream) {
          char offset[4];
          _itoa_s(mpls->play_item[0].stn.pg[i].ss_offset_sequence_id, offset, 10);
          av_dict_set(&avstream->metadata, "3d-plane", offset, 0);
        }
      }
    }

    // export the list of pg sequences
    if (pg_sequences.size() > 0)
    {
      // strip duplicate entries
      pg_sequences.sort();
      pg_sequences.unique();

      size_t size = pg_sequences.size() * 4;
      char *offsets = new char[size];
      offsets[0] = 0;

      // Append all offsets to the string
      for (auto it = pg_sequences.begin(); it != pg_sequences.end(); it++) {
        size_t len = strlen(offsets);
        if (len > 0) {
          offsets[len] = ',';
          len++;
        }
        _itoa_s(*it, offsets + len, size - len, 10);
      }

      av_dict_set(&m_lavfDemuxer->m_avFormat->metadata, "pg_offset_sequences", offsets, 0);
      delete[] offsets;
    }

    // Export a list of all IG offsets
    if (mpls->play_item[0].stn.num_ig > 0) {
      std::list<uint8_t> ig_sequences;
      for (int i = 0; i < mpls->play_item[0].stn.num_ig; i++) {
        if (mpls->play_item[0].stn.ig[i].ss_offset_sequence_id != 0xFF) {
          ig_sequences.push_back(mpls->play_item[0].stn.ig[i].ss_offset_sequence_id);
        }
      }

      if (ig_sequences.size() > 0) {
        // strip duplicate entries
        ig_sequences.unique();

        size_t size = ig_sequences.size() * 4;
        char *offsets = new char[size];
        offsets[0] = 0;

        // Append all offsets to the string
        for (auto it = ig_sequences.begin(); it != ig_sequences.end(); it++) {
          size_t len = strlen(offsets);
          if (len > 0) {
            offsets[len] = ',';
            len++;
          }
          _itoa_s(*it, offsets + len, size - len, 10);
        }

        av_dict_set(&m_lavfDemuxer->m_avFormat->metadata, "ig_offset_sequences", offsets, 0);
        delete[] offsets;
      }
    }
  }
}

/*STDMETHODIMP_(int) CBDDemuxer::GetNumTitles()
{
  return m_nTitleCount;
}

STDMETHODIMP CBDDemuxer::GetTitleInfo(int idx, REFERENCE_TIME *rtDuration, WCHAR **ppszName)
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
      if (*ppszName)
        memcpy(*ppszName, buffer, size);
    }

    return S_OK;
  }

  return E_FAIL;
}*/

void CBDDemuxer::ProcessClipInfo(CLPI_CL *clpi, bool overwrite)
{
  if (!clpi) { return; }
  for (int k = 0; k < clpi->program.num_prog; k++) {
    for (int i = 0; i < clpi->program.progs[k].num_streams; i++) {
      CLPI_PROG_STREAM *stream = &clpi->program.progs[k].streams[i];
      AVStream *avstream = m_lavfDemuxer->GetAVStreamByPID(stream->pid);
      if (!avstream) {
        DbgLog((LOG_TRACE, 10, "CBDDemuxer::ProcessStreams(): Stream with PID 0x%04x not found, trying to add it..", stream->pid));
        m_lavfDemuxer->AddMPEGTSStream(stream->pid, stream->coding_type);
        avstream = m_lavfDemuxer->GetAVStreamByPID(stream->pid);
      }
      if (avstream) {
        if (stream->lang[0] != 0)
          av_dict_set(&avstream->metadata, "language", (const char *)stream->lang, overwrite ? 0 : AV_DICT_DONT_OVERWRITE);
        if (avstream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
          if (avstream->codecpar->width == 0 || avstream->codecpar->height == 0) {
            switch(stream->format) {
            case BLURAY_VIDEO_FORMAT_480I:
            case BLURAY_VIDEO_FORMAT_480P:
              avstream->codecpar->width = 720;
              avstream->codecpar->height = 480;
              break;
            case BLURAY_VIDEO_FORMAT_576I:
            case BLURAY_VIDEO_FORMAT_576P:
              avstream->codecpar->width = 720;
              avstream->codecpar->height = 576;
              break;
            case BLURAY_VIDEO_FORMAT_720P:
              avstream->codecpar->width = 1280;
              avstream->codecpar->height = 720;
              break;
            case BLURAY_VIDEO_FORMAT_1080I:
            case BLURAY_VIDEO_FORMAT_1080P:
            default:
              avstream->codecpar->width = 1920;
              avstream->codecpar->height = 1080;
              break;
            case BLURAY_VIDEO_FORMAT_2160P:
              avstream->codecpar->width = 3840;
              avstream->codecpar->height = 2160;
              break;
            }
          }

          if (m_MVCPlayback && stream->coding_type == BLURAY_STREAM_TYPE_VIDEO_H264) {
            av_dict_set(&avstream->metadata, "stereo_mode", m_pTitle->mvc_base_view_r_flag ? "mvc_rl" : "mvc_lr", 0);
          }
        } else if (avstream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
          if (avstream->codecpar->channels == 0) {
            avstream->codecpar->channels = (stream->format == BLURAY_AUDIO_FORMAT_MONO) ? 1 : (stream->format == BLURAY_AUDIO_FORMAT_STEREO) ? 2 : 6;
            avstream->codecpar->channel_layout = av_get_default_channel_layout(avstream->codecpar->channels);
            avstream->codecpar->sample_rate = (stream->rate == BLURAY_AUDIO_RATE_96||stream->rate == BLURAY_AUDIO_RATE_96_COMBO) ? 96000 : (stream->rate == BLURAY_AUDIO_RATE_192||stream->rate == BLURAY_AUDIO_RATE_192_COMBO) ? 192000 : 48000;
            if (avstream->codecpar->codec_id == AV_CODEC_ID_DTS) {
              if (stream->coding_type == BLURAY_STREAM_TYPE_AUDIO_DTSHD)
                avstream->codecpar->profile = FF_PROFILE_DTS_HD_HRA;
              else if (stream->coding_type == BLURAY_STREAM_TYPE_AUDIO_DTSHD_MASTER)
                avstream->codecpar->profile = FF_PROFILE_DTS_HD_MA;
            }
          }
        }
      }
    }
  }
}

STDMETHODIMP CBDDemuxer::Seek(REFERENCE_TIME rTime)
{
  int64_t prev = bd_tell(m_pBD);

  int64_t target = bd_find_seek_point(m_pBD, ConvertDSTimeTo90Khz(rTime));
  m_EndOfStreamPacketFlushProtection = FALSE;

  DbgLog((LOG_TRACE, 1, "Seek Request: %I64u (time); %I64u (byte), %I64u (prev byte)", rTime, target, prev));
  HRESULT hr = m_lavfDemuxer->SeekByte(target + 4, AVSEEK_FLAG_BACKWARD);

  if (m_MVCPlayback && m_MVCFormatContext) {
    // Re-open to switch clip if needed
    CloseMVCExtensionDemuxer();
    if (FAILED(OpenMVCExtensionDemuxer(m_NewClip)))
      return E_FAIL;

    // Adjust for clip offset
    int64_t seek_pts = 0;
    if (rTime > 0) {
      AVStream *stream = m_MVCFormatContext->streams[m_MVCStreamIndex];

      rTime -= m_rtNewOffset;
      rTime -= 10000000; // seek one second before the target to ensure the MVC queue isn't out of sync for too long
      seek_pts = m_lavfDemuxer->ConvertRTToTimestamp(rTime, stream->time_base.num, stream->time_base.den);
    }

    if (seek_pts < 0)
      seek_pts = 0;

    av_seek_frame(m_MVCFormatContext, m_MVCStreamIndex, seek_pts, AVSEEK_FLAG_BACKWARD);
  }
  return hr;
}

const char *CBDDemuxer::GetContainerFormat() const
{
  return "mpegts";
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
