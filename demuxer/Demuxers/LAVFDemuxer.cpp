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
 */

#include "stdafx.h"
#include "LAVFDemuxer.h"
#include "LAVFUtils.h"
#include "LAVFStreamInfo.h"

#include "LAVSplitterSettings.h"


#ifdef DEBUG
extern "C" {
#include "libavutil/log.h"
}
#endif

extern void lavf_get_iformat_infos(AVInputFormat *pFormat, const char **pszName, const char **pszDescription);

static const AVRational AV_RATIONAL_TIMEBASE = {1, AV_TIME_BASE};

void CLAVFDemuxer::ffmpeg_init()
{
  av_register_all();
}

std::set<FormatInfo> CLAVFDemuxer::GetFormatList()
{
  std::set<FormatInfo> formats;
  AVInputFormat *f = NULL;
  while (f = av_iformat_next(f)) {
    FormatInfo format;
    lavf_get_iformat_infos(f, &format.strName, &format.strDescription);
    if (format.strName)
      formats.insert(format);
  }
  return formats;
}

CLAVFDemuxer::CLAVFDemuxer(CCritSec *pLock, ILAVFSettings *settings)
  : CBaseDemuxer(L"lavf demuxer", pLock)
  , m_avFormat(NULL)
  , m_program(0)
  , m_rtCurrent(0)
  , m_bIsStream(FALSE)
  , m_bMatroska(FALSE)
  , m_bAVI(FALSE)
  , m_bMPEGTS(FALSE)
  , m_bVC1Correction(FALSE)
  , m_bVC1SeenTimestamp(FALSE)
  , m_bPGSNoParsing(TRUE)
  , m_pSettings(NULL)
  , m_stOrigParser(NULL)
  , m_pFontInstaller(NULL)
  , m_pszInputFormat(NULL)
{
  m_bSubStreams = settings->GetSubstreamsEnabled();
  m_bGenPTS = settings->GetGeneratePTS();

  m_pSettings = settings;

#ifdef DEBUG
  DbgSetModuleLevel (LOG_CUSTOM1, DWORD_MAX); // FFMPEG messages use custom1
  av_log_set_callback(lavf_log_callback);
#endif
}

CLAVFDemuxer::~CLAVFDemuxer()
{
  CleanupAVFormat();
  SAFE_DELETE(m_pFontInstaller);
}

STDMETHODIMP CLAVFDemuxer::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = NULL;

  return
    QI(IKeyFrameInfo)
    QI(ITrackInfo)
    QI2(IAMExtendedSeeking)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

/////////////////////////////////////////////////////////////////////////////
// Demuxer Functions
STDMETHODIMP CLAVFDemuxer::Open(LPCOLESTR pszFileName)
{
  CAutoLock lock(m_pLock);
  HRESULT hr = S_OK;

  int ret; // return code from avformat functions

  // Convert the filename from wchar to char for avformat
  char fileName[4096] = {0};
  ret = WideCharToMultiByte(CP_UTF8, 0, pszFileName, -1, fileName, 4096, NULL, NULL);

  ret = av_open_input_file(&m_avFormat, fileName, NULL, FFMPEG_FILE_BUFFER_SIZE, NULL);
  if (ret < 0) {
    DbgLog((LOG_ERROR, 0, TEXT("::Open(): av_open_input_file failed (%d)"), ret));
    goto done;
  }
  DbgLog((LOG_TRACE, 10, TEXT("::Open(): av_open_input_file opened file of type '%S'"), m_avFormat->iformat->name));

  CHECK_HR(hr = InitAVFormat(pszFileName));

  return S_OK;
done:
  CleanupAVFormat();
  return E_FAIL;
}

STDMETHODIMP CLAVFDemuxer::OpenInputStream(AVIOContext *byteContext, LPCOLESTR pszFileName)
{
  CAutoLock lock(m_pLock);
  HRESULT hr = S_OK;

  int ret; // return code from avformat functions

  AVInputFormat *fmt = NULL;

  m_bIsStream = true;

  ret = av_probe_input_buffer(byteContext, &fmt, "", NULL, 0, FFMPEG_FILE_BUFFER_SIZE);
  if (ret < 0) {
    DbgLog((LOG_ERROR, 0, TEXT("::OpenInputStream(): av_probe_input_buffer failed (%d)"), ret));
    goto done;
  }
  DbgLog((LOG_TRACE, 10, TEXT("::OpenInputStream(): av_probe_input_buffer detected format '%S'"), fmt->name));

  ret = av_open_input_stream(&m_avFormat, byteContext, "", fmt, NULL);
  if (ret < 0) {
    DbgLog((LOG_ERROR, 0, TEXT("::OpenInputStream(): av_open_input_stream failed (%d)"), ret));
    goto done;
  }

  CHECK_HR(hr = InitAVFormat(pszFileName));

  return S_OK;
done:
  CleanupAVFormat();
  return E_FAIL;
}

HRESULT CLAVFDemuxer::CheckBDM2TSCPLI(LPCOLESTR pszFileName)
{
  size_t len = wcslen(pszFileName);

  if (len <= 23 || _wcsnicmp(pszFileName+len - 23, L"\\BDMV\\STREAM\\", 13) != 0)
    return E_FAIL;

  // Get the base file name (should be a number, like 00000)
  const WCHAR *file = pszFileName + (len - 10);
  WCHAR basename[6];
  wcsncpy_s(basename, file, 5);
  basename[5] = 0;

  // Convert to UTF-8 path
  size_t a_len = WideCharToMultiByte(CP_UTF8, 0, pszFileName, -1, NULL, 0, NULL, NULL);
  a_len += 2;// one extra char because CLIPINF is 7 chars and STREAM is 6, and one for the terminating-zero

  char *path = (char *)CoTaskMemAlloc(a_len * sizeof(char));
  WideCharToMultiByte(CP_UTF8, 0, pszFileName, -1, path, a_len, NULL, NULL);

  // Remove file name itself
  PathRemoveFileSpecA(path);
  // Remove STREAM folder
  PathRemoveFileSpecA(path);

  // Write new path
  sprintf_s(path+strlen(path), a_len-strlen(path), "\\CLIPINF\\%S.clpi", basename);

  CLPI_CL *cl = clpi_parse(path, 0);
  if (!cl)
    return E_FAIL;

  // Clip Info was found, add the language metadata to the AVStreams
  for(unsigned i = 0; i < cl->program.num_prog; ++i) {
    CLPI_PROG *p = &cl->program.progs[i];
    for (unsigned k = 0; k < p->num_streams; ++k) {
      CLPI_PROG_STREAM *s = &p->streams[k];
      AVStream *avstream = GetAVStreamByPID(s->pid);
      if (avstream) {
        if (s->lang[0] != 0)
          av_metadata_set2(&avstream->metadata, "language", (const char *)s->lang, 0);
      }
    }
  }
  // Free the clip
  clpi_free(cl);
  cl = NULL;

  return S_OK;
}

STDMETHODIMP CLAVFDemuxer::InitAVFormat(LPCOLESTR pszFileName)
{
  HRESULT hr = S_OK;

  const char *format = NULL;
  lavf_get_iformat_infos(m_avFormat->iformat, &format, NULL);
  if (!format || !m_pSettings->IsFormatEnabled(format)) {
    DbgLog((LOG_TRACE, 20, L"::InitAVFormat() - format of type '%S' disabled, failing", format ? format : m_avFormat->iformat->name));
    return E_FAIL;
  }

  m_pszInputFormat = format ? format : m_avFormat->iformat->name;

  m_bVC1SeenTimestamp = FALSE;

  m_bMatroska = (_strnicmp(m_pszInputFormat, "matroska", 8) == 0);
  m_bAVI = (_strnicmp(m_pszInputFormat, "avi", 3) == 0);
  m_bMPEGTS = (_strnicmp(m_pszInputFormat, "mpegts", 6) == 0);

  if (m_bGenPTS) {
    m_avFormat->flags |= AVFMT_FLAG_GENPTS;
  }

  int ret = av_find_stream_info(m_avFormat);
  if (ret < 0) {
    DbgLog((LOG_ERROR, 0, TEXT("::InitAVFormat(): av_find_stream_info failed (%d)"), ret));
    goto done;
  }

  // Check if this is a m2ts in a BD structure, and if it is, read some extra stream properties out of the CLPI files
  if (pszFileName && m_bMPEGTS) {
    CheckBDM2TSCPLI(pszFileName);
  }

  SAFE_CO_FREE(m_stOrigParser);
  m_stOrigParser = (enum AVStreamParseType *)CoTaskMemAlloc(m_avFormat->nb_streams * sizeof(enum AVStreamParseType));

  for(unsigned int idx = 0; idx < m_avFormat->nb_streams; ++idx) {
    AVStream *st = m_avFormat->streams[idx];

    // Disable full stream parsing for these formats
    if (st->need_parsing == AVSTREAM_PARSE_FULL) {
      if (st->codec->codec_id == CODEC_ID_MPEG2VIDEO) {
        st->need_parsing = AVSTREAM_PARSE_NONE;
      }
    }

#ifdef DEBUG
    DbgLog((LOG_TRACE, 30, L"Stream %d (pid %d) - codec: %d; parsing: %S;", idx, st->id, st->codec->codec_id, lavf_get_parsing_string(st->need_parsing)));
#endif
    m_stOrigParser[idx] = st->need_parsing;

    UpdateSubStreams();

    if (st->codec->codec_type == AVMEDIA_TYPE_ATTACHMENT && st->codec->codec_id == CODEC_ID_TTF) {
      if (!m_pFontInstaller) {
        m_pFontInstaller = new CFontInstaller();
      }
      m_pFontInstaller->InstallFont(st->codec->extradata, st->codec->extradata_size);
    }
  }

  CHECK_HR(hr = CreateStreams());

  return S_OK;
done:
  CleanupAVFormat();
  return E_FAIL;
}

void CLAVFDemuxer::CleanupAVFormat()
{
  if (m_avFormat) {
    if (m_bIsStream)
      av_close_input_stream(m_avFormat);
    else
      av_close_input_file(m_avFormat);
    m_avFormat = NULL;
  }
  SAFE_CO_FREE(m_stOrigParser);
}

AVStream* CLAVFDemuxer::GetAVStreamByPID(int pid)
{
  if (!m_avFormat) return NULL;

  for (unsigned int idx = 0; idx < m_avFormat->nb_streams; ++idx) {
    if (m_avFormat->streams[idx]->id == pid && !(m_avFormat->streams[idx]->disposition & LAVF_DISPOSITION_SUB_STREAM))
      return m_avFormat->streams[idx];
  }

  return NULL;
}

void CLAVFDemuxer::UpdateSubStreams()
{
  for(unsigned int idx = 0; idx < m_avFormat->nb_streams; ++idx) {
    AVStream *st = m_avFormat->streams[idx];
    // Find and flag the AC-3 substream
    if (m_bMPEGTS && st->codec->codec_id == CODEC_ID_TRUEHD) {
      int id = st->id;
      AVStream *sub_st = NULL;

      for (unsigned int i = 0; i < m_avFormat->nb_streams; ++i) {
        AVStream *sst = m_avFormat->streams[i];
        if (idx != i && sst->id == id) {
          sub_st = sst;
          break;
        }
      }
      if (sub_st) {
        sub_st->disposition = st->disposition | LAVF_DISPOSITION_SUB_STREAM;
        av_metadata_copy(&sub_st->metadata, st->metadata, 0);
      }
    }
  }
}

void CLAVFDemuxer::SettingsChanged(ILAVFSettings *pSettings)
{
  int vc1Mode = pSettings->GetVC1TimestampMode();
  if (vc1Mode == 1) {
    m_bVC1Correction = true;
  } else if (vc1Mode == 2) {
    BOOL bReq = pSettings->IsVC1CorrectionRequired();
    m_bVC1Correction = m_bMatroska ? !bReq : bReq;
  } else {
    m_bVC1Correction = false;
  }

  for(unsigned int idx = 0; idx < m_avFormat->nb_streams; ++idx) {
    AVStream *st = m_avFormat->streams[idx];
    if (st->codec->codec_id == CODEC_ID_VC1) {
      st->need_parsing = m_bVC1Correction ? m_stOrigParser[idx] : AVSTREAM_PARSE_NONE;
    } else if (st->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      st->need_parsing = pSettings->GetVideoParsingEnabled() ? m_stOrigParser[idx] : AVSTREAM_PARSE_NONE;
    } else if (st->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
      st->need_parsing = pSettings->GetAudioParsingEnabled() ? m_stOrigParser[idx] : AVSTREAM_PARSE_NONE;
    }
  }

  m_bPGSNoParsing = !pSettings->GetPGSOnlyForced();
}

REFERENCE_TIME CLAVFDemuxer::GetDuration() const
{
  int64_t iLength = 0;
  if (m_avFormat->duration == (int64_t)AV_NOPTS_VALUE || m_avFormat->duration < 0LL) {
    // no duration is available for us
    // try to calculate it
    // TODO
    /*if (m_rtCurrent != Packet::INVALID_TIME && m_avFormat->file_size > 0 && m_avFormat->pb && m_avFormat->pb->pos > 0) {
    iLength = (((m_rtCurrent * m_avFormat->file_size) / m_avFormat->pb->pos) / 1000) & 0xFFFFFFFF;
    }*/
    DbgLog((LOG_ERROR, 1, TEXT("duration is not available")));
  } else {
    iLength = m_avFormat->duration;
  }
  return ConvertTimestampToRT(iLength, 1, AV_TIME_BASE, 0);
}

STDMETHODIMP CLAVFDemuxer::GetNextPacket(Packet **ppPacket)
{
  CheckPointer(ppPacket, E_POINTER);

  // If true, S_FALSE is returned, indicating a soft-failure
  bool bReturnEmpty = false;

  // Read packet
  AVPacket pkt;
  Packet *pPacket = NULL;

  // assume we are not eof
  if(m_avFormat->pb) {
    m_avFormat->pb->eof_reached = 0;
  }

  int result = 0;
  try {
    DBG_TIMING("av_read_frame", 30, result = av_read_frame(m_avFormat, &pkt))
  } catch(...) {
    // ignore..
  }

  if (result == AVERROR(EINTR) || result == AVERROR(EAGAIN))
  {
    // timeout, probably no real error, return empty packet
    bReturnEmpty = true;
  } else if (result < 0) {
    // meh, fail
  } else if (pkt.size < 0 || pkt.stream_index >= m_avFormat->nb_streams) {
    // XXX, in some cases ffmpeg returns a negative packet size
    if(m_avFormat->pb && !m_avFormat->pb->eof_reached) {
      bReturnEmpty = true;
    }
    av_free_packet(&pkt);
  } else {
    // Check right here if the stream is active, we can drop the package otherwise.
    BOOL streamActive = FALSE;
    for(int i = 0; i < unknown; ++i) {
      if(m_dActiveStreams[i] == pkt.stream_index) {
        streamActive = TRUE;
        break;
      }
    }
    if(!streamActive) {
      av_free_packet(&pkt);
      return S_FALSE;
    }

    AVStream *stream = m_avFormat->streams[pkt.stream_index];
    pPacket = new Packet();
    pPacket->bPosition = pkt.pos;

    // Only use PTS in Matroska for these formats
    if(m_bMatroska && stream->codec->codec_id == CODEC_ID_H264) {
      pkt.dts = AV_NOPTS_VALUE;
    }

    // we need to get duration slightly different for matroska embedded text subtitels
    if(m_bMatroska && stream->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
      pkt.duration = (int)pkt.convergence_duration;
    }

    if(m_bAVI && stream->codec && stream->codec->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      // AVI's always have borked pts, specially if m_pFormatContext->flags includes
      // AVFMT_FLAG_GENPTS so always use dts
      pkt.pts = AV_NOPTS_VALUE;
    }

    if(pkt.data) {
      pPacket->SetData(pkt.data, pkt.size);
    }

    pPacket->StreamId = (DWORD)pkt.stream_index;

    REFERENCE_TIME pts = (REFERENCE_TIME)ConvertTimestampToRT(pkt.pts, stream->time_base.num, stream->time_base.den);
    REFERENCE_TIME dts = (REFERENCE_TIME)ConvertTimestampToRT(pkt.dts, stream->time_base.num, stream->time_base.den);
    REFERENCE_TIME duration = (REFERENCE_TIME)ConvertTimestampToRT(pkt.duration, stream->time_base.num, stream->time_base.den, 0);

    REFERENCE_TIME rt = Packet::INVALID_TIME; // m_rtCurrent;
    // Try the different times set, pts first, dts when pts is not valid
    if (pts != Packet::INVALID_TIME) {
      rt = pts;
    } else if (dts != Packet::INVALID_TIME) {
      rt = dts;
    }

    if (stream->codec->codec_id == CODEC_ID_VC1) {
      if (m_bMatroska && m_bVC1Correction) {
        rt = pts;
        if (!m_bVC1SeenTimestamp) {
          if (rt == Packet::INVALID_TIME && dts != Packet::INVALID_TIME)
            rt = dts;
          m_bVC1SeenTimestamp = (rt != Packet::INVALID_TIME);
        }
      } else if (m_bMatroska) {
        pPacket->dwFlags |= LAV_PACKET_PARSED;
      } else if (m_bVC1Correction) {
        rt = dts;
        pPacket->dwFlags |= LAV_PACKET_PARSED;
      }
    }

    // Mark the packet as parsed, so the forced subtitle parser doesn't hit it
    if (stream->codec->codec_id == CODEC_ID_HDMV_PGS_SUBTITLE && m_bPGSNoParsing) {
      pPacket->dwFlags |= LAV_PACKET_PARSED;
    }

    pPacket->rtStart = pPacket->rtStop = rt;
    if (rt != Packet::INVALID_TIME) {
      pPacket->rtStop += (duration > 0 || stream->codec->codec_id == CODEC_ID_TRUEHD) ? duration : 1;
    }

    if (stream->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
      pPacket->bDiscontinuity = TRUE;
    }

    pPacket->bSyncPoint = (rt != Packet::INVALID_TIME && duration > 0) ? 1 : 0;
    pPacket->bAppendable = 0; //!pPacket->bSyncPoint;

    // Update current time
    if (rt != Packet::INVALID_TIME) {
      m_rtCurrent = rt;
    }

    av_free_packet(&pkt);
  }

  if (bReturnEmpty && !pPacket) {
    return S_FALSE;
  }
  if (!pPacket) {
    return E_FAIL;
  }

  *ppPacket = pPacket;
  return S_OK;
}

STDMETHODIMP CLAVFDemuxer::Seek(REFERENCE_TIME rTime)
{
  int videoStreamId = m_dActiveStreams[video];
  int64_t seek_pts = 0;
  // If we have a video stream, seek on that one. If we don't, well, then don't!
  if (videoStreamId != -1) {
    AVStream *stream = m_avFormat->streams[videoStreamId];
    seek_pts = ConvertRTToTimestamp(rTime, stream->time_base.num, stream->time_base.den);
  } else {
    seek_pts = ConvertRTToTimestamp(rTime, 1, AV_TIME_BASE);
  }

  int ret = avformat_seek_file(m_avFormat, videoStreamId, _I64_MIN, seek_pts, _I64_MAX, 0);
  //int ret = av_seek_frame(m_avFormat, -1, seek_pts, 0);
  if(ret < 0) {
    DbgLog((LOG_CUSTOM1, 1, L"::Seek() -- Key-Frame Seek failed"));
    ret = avformat_seek_file(m_avFormat, videoStreamId, _I64_MIN, seek_pts, _I64_MAX, AVSEEK_FLAG_ANY);
    if (ret < 0) {
      DbgLog((LOG_ERROR, 1, L"::Seek() -- Inaccurate Seek failed as well"));
    }
  }

  m_bVC1SeenTimestamp = FALSE;

  return S_OK;
}

STDMETHODIMP CLAVFDemuxer::SeekByte(int64_t pos, int flags)
{
  int ret = av_seek_frame(m_avFormat, -1, pos, flags | AVSEEK_FLAG_BYTE);
  if(ret < 0) {
    DbgLog((LOG_ERROR, 1, L"::SeekByte() -- Seek failed"));
  }
  return S_OK;
}

const char *CLAVFDemuxer::GetContainerFormat() const
{
  return m_pszInputFormat;
}

HRESULT CLAVFDemuxer::StreamInfo(DWORD streamId, LCID *plcid, WCHAR **ppszName) const
{
  if (streamId >= (DWORD)m_avFormat->nb_streams) { return E_FAIL; }

  if (plcid) {
    const char *lang = get_stream_language(m_avFormat->streams[streamId]);
    if (lang) {
      *plcid = ProbeLangForLCID(lang);
    } else {
      *plcid = 0;
    }
  }

  if(ppszName) {
    lavf_describe_stream(m_avFormat->streams[streamId], ppszName);
  }

  return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// IAMExtendedSeeking
STDMETHODIMP CLAVFDemuxer::get_ExSeekCapabilities(long* pExCapabilities)
{
  CheckPointer(pExCapabilities, E_POINTER);
  *pExCapabilities = AM_EXSEEK_CANSEEK;
  if(m_avFormat->nb_chapters > 0) *pExCapabilities |= AM_EXSEEK_MARKERSEEK;
  return S_OK;
}

STDMETHODIMP CLAVFDemuxer::get_MarkerCount(long* pMarkerCount)
{
  CheckPointer(pMarkerCount, E_POINTER);
  *pMarkerCount = (long)m_avFormat->nb_chapters;
  return S_OK;
}

STDMETHODIMP CLAVFDemuxer::get_CurrentMarker(long* pCurrentMarker)
{
  CheckPointer(pCurrentMarker, E_POINTER);
  // Can the time_base change in between chapters?
  // Anyhow, we do the calculation in the loop, just to be safe
  for(unsigned int i = 0; i < m_avFormat->nb_chapters; ++i) {
    int64_t pts = ConvertRTToTimestamp(m_rtCurrent, m_avFormat->chapters[i]->time_base.num, m_avFormat->chapters[i]->time_base.den);
    // Check if the pts is in between the bounds of the chapter
    if (pts >= m_avFormat->chapters[i]->start && pts <= m_avFormat->chapters[i]->end) {
      *pCurrentMarker = (i + 1);
      return S_OK;
    }
  }
  return E_FAIL;
}

STDMETHODIMP CLAVFDemuxer::GetMarkerTime(long MarkerNum, double* pMarkerTime)
{
  CheckPointer(pMarkerTime, E_POINTER);
  // Chapters go by a 1-based index, doh
  unsigned int index = MarkerNum - 1;
  if(index >= m_avFormat->nb_chapters) { return E_FAIL; }

  REFERENCE_TIME rt = ConvertTimestampToRT(m_avFormat->chapters[index]->start, m_avFormat->chapters[index]->time_base.num, m_avFormat->chapters[index]->time_base.den);
  *pMarkerTime = (double)rt / DSHOW_TIME_BASE;

  return S_OK;
}

STDMETHODIMP CLAVFDemuxer::GetMarkerName(long MarkerNum, BSTR* pbstrMarkerName)
{
  CheckPointer(pbstrMarkerName, E_POINTER);
  // Chapters go by a 1-based index, doh
  unsigned int index = MarkerNum - 1;
  if(index >= m_avFormat->nb_chapters) { return E_FAIL; }
  // Get the title, or generate one
  OLECHAR wTitle[128];
  if (av_metadata_get(m_avFormat->chapters[index]->metadata, "title", NULL, 0)) {
    char *title = av_metadata_get(m_avFormat->chapters[index]->metadata, "title", NULL, 0)->value;
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wTitle, 128);
  } else {
    swprintf_s(wTitle, L"Chapter %d", MarkerNum);
  }
  *pbstrMarkerName = SysAllocString(wTitle);
  return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// IKeyFrameInfo
STDMETHODIMP CLAVFDemuxer::GetKeyFrameCount(UINT& nKFs)
{
  if(m_dActiveStreams[video] < 0) { return E_NOTIMPL; }

  if (!m_bMatroska && !m_bAVI) {
    return E_FAIL;
  }

  AVStream *stream = m_avFormat->streams[m_dActiveStreams[video]];
  nKFs = stream->nb_index_entries;
  return (stream->nb_index_entries == stream->nb_frames) ? S_FALSE : S_OK;
}

STDMETHODIMP CLAVFDemuxer::GetKeyFrames(const GUID* pFormat, REFERENCE_TIME* pKFs, UINT& nKFs)
{
  CheckPointer(pFormat, E_POINTER);
  CheckPointer(pKFs, E_POINTER);

  if(m_dActiveStreams[video] < 0) { return E_NOTIMPL; }

  if (!m_bMatroska && !m_bAVI) {
    return E_FAIL;
  }

  if(*pFormat != TIME_FORMAT_MEDIA_TIME) return E_INVALIDARG;

  AVStream *stream = m_avFormat->streams[m_dActiveStreams[video]];
  nKFs = stream->nb_index_entries;
  for(unsigned int i = 0; i < nKFs; ++i) {
    pKFs[i] = ConvertTimestampToRT(stream->index_entries[i].timestamp, stream->time_base.num, stream->time_base.den);
  }
  return S_OK;
}

const AVStream* CLAVFDemuxer::GetAVStreamByIndex(UINT index)
{
  if(!m_avFormat)
    return NULL;

  UINT type = video;
  UINT count_v = m_streams[video].size();
  UINT count_a = m_streams[audio].size();
  UINT count_s = m_streams[subpic].size();
  if (index >= count_v) {
    index -= count_v;
    type = audio;
    if (index >= count_a) {
      index -= count_a;
      type = subpic;
      if (index >= count_s)
        return NULL;
    }
  }

  const stream st1 = m_streams[type][index];
  return  m_avFormat->streams[st1.pid];
}

/////////////////////////////////////////////////////////////////////////////
// ITrackInfo
STDMETHODIMP_(UINT) CLAVFDemuxer::GetTrackCount()
{
  if(!m_avFormat)
    return 0;

  UINT count = m_streams[video].size() + m_streams[audio].size() + m_streams[subpic].size();

  return count;
}

// \param aTrackIdx the track index (from 0 to GetTrackCount()-1)
STDMETHODIMP_(BOOL) CLAVFDemuxer::GetTrackInfo(UINT aTrackIdx, struct TrackElement* pStructureToFill)
{
  if(!m_avFormat || !pStructureToFill)
    return FALSE;

  const AVStream *st = GetAVStreamByIndex(aTrackIdx);
  if(!st)
    return FALSE;

  // Fill structure
  pStructureToFill->Size = sizeof(TrackElement);
  pStructureToFill->FlagDefault = (st->disposition & AV_DISPOSITION_DEFAULT);
  pStructureToFill->FlagForced = (st->disposition & AV_DISPOSITION_FORCED);
  const char *lang = get_stream_language(st);
  strncpy_s(pStructureToFill->Language, lang, 3);
  pStructureToFill->Language[3] = '\0';

  pStructureToFill->Type = (st->codec->codec_type == AVMEDIA_TYPE_VIDEO) ? TypeVideo :
                           (st->codec->codec_type == AVMEDIA_TYPE_AUDIO) ? TypeAudio :
                           (st->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) ? TypeSubtitle : 0;

  // The following flags are not exported via avformat
  pStructureToFill->FlagLacing = 0;
  pStructureToFill->MaxCache = 0;
  pStructureToFill->MinCache = 0;

  return TRUE;
}

// Get an extended information struct relative to the track type
STDMETHODIMP_(BOOL) CLAVFDemuxer::GetTrackExtendedInfo(UINT aTrackIdx, void* pStructureToFill)
{
  if(!m_avFormat || !pStructureToFill)
    return FALSE;

  const AVStream *st = GetAVStreamByIndex(aTrackIdx);
  if(!st)
    return FALSE;

  if (st->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
    TrackExtendedInfoVideo* pTEIV = (TrackExtendedInfoVideo*)pStructureToFill;

    pTEIV->DisplayUnit = 0; // always pixels
    pTEIV->DisplayWidth = st->codec->width;
    pTEIV->DisplayHeight = st->codec->height;

    pTEIV->PixelWidth = st->codec->coded_width ? st->codec->coded_width : st->codec->width;
    pTEIV->PixelHeight = st->codec->coded_height ? st->codec->coded_height : st->codec->height;

    pTEIV->AspectRatioType = 0;
    pTEIV->Interlaced = 0;
  } else if (st->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
    TrackExtendedInfoAudio* pTEIA = (TrackExtendedInfoAudio*)pStructureToFill;

    pTEIA->BitDepth = st->codec->bits_per_coded_sample;
    pTEIA->Channels = st->codec->channels;
    pTEIA->OutputSamplingFrequency = (FLOAT)st->codec->sample_rate;
    pTEIA->SamplingFreq = (FLOAT)st->codec->sample_rate;
  }

  return TRUE;
}

STDMETHODIMP_(BSTR) CLAVFDemuxer::GetTrackName(UINT aTrackIdx)
{
  if(!m_avFormat)
    return NULL;

  const AVStream *st = GetAVStreamByIndex(aTrackIdx);
  if(!st)
    return NULL;

  BSTR trackName = NULL;

  const char *title = NULL;
  if (av_metadata_get(st->metadata, "title", NULL, 0)) {
    title = av_metadata_get(st->metadata, "title", NULL, 0)->value;
  }
  if (title && title[0] != '\0') {
    trackName = ConvertCharToBSTR(title);
  }

  return trackName;
}

STDMETHODIMP_(BSTR) CLAVFDemuxer::GetTrackCodecName(UINT aTrackIdx)
{
  if(!m_avFormat)
    return NULL;

  const AVStream *st = GetAVStreamByIndex(aTrackIdx);
  if(!st)
    return NULL;

  BSTR codecName = NULL;

  std::string codec = get_codec_name(st->codec);
  if (!codec.empty()) {
    codecName = ConvertCharToBSTR(codec.c_str());
  }

  return codecName;
}

/////////////////////////////////////////////////////////////////////////////
// Internal Functions
STDMETHODIMP CLAVFDemuxer::AddStream(int streamId)
{
  HRESULT hr = S_OK;
  AVStream *pStream = m_avFormat->streams[streamId];

  if (pStream->discard == AVDISCARD_ALL || (pStream->codec->codec_id == CODEC_ID_NONE && pStream->codec->codec_tag == 0) || (!m_bSubStreams && (pStream->disposition & LAVF_DISPOSITION_SUB_STREAM)))
    return S_FALSE;

  stream s;
  s.pid = streamId;
  s.streamInfo = new CLAVFStreamInfo(pStream, m_pszInputFormat, hr);

  if(FAILED(hr)) {
    delete s.streamInfo;
    return hr;
  }

  switch(pStream->codec->codec_type)
  {
  case AVMEDIA_TYPE_VIDEO:
    m_streams[video].push_back(s);
    break;
  case AVMEDIA_TYPE_AUDIO:
    m_streams[audio].push_back(s);
    break;
  case AVMEDIA_TYPE_SUBTITLE:
    m_streams[subpic].push_back(s);
    break;
  default:
    // unsupported stream
    // Normally this should be caught while creating the stream info already.
    delete s.streamInfo;
    return E_FAIL;
    break;
  }
  return S_OK;
}

// Pin creation
STDMETHODIMP CLAVFDemuxer::CreateStreams()
{
  CAutoLock lock(m_pLock);  

  // try to use non-blocking methods
  m_avFormat->flags |= AVFMT_FLAG_NONBLOCK;

  for(int i = 0; i < countof(m_streams); ++i) {
    m_streams[i].Clear();
  }

  m_program = UINT_MAX;
  if (m_avFormat->nb_programs) {
    // look for first non empty stream and discard nonselected programs
    for (unsigned int i = 0; i < m_avFormat->nb_programs; ++i) {
      if(m_program == UINT_MAX && m_avFormat->programs[i]->nb_stream_indexes > 0) {
        m_program = i;
      }

      if(i != m_program) {
        m_avFormat->programs[i]->discard = AVDISCARD_ALL;
      }
    }
  }


  // Re-compute the overall file duration based on video and audio durations
  int64_t duration = INT64_MIN;
  int64_t st_duration = 0;
  int64_t start_time = INT64_MAX;
  int64_t st_start_time = 0;

  bool bProgram = (m_program < m_avFormat->nb_programs);
  unsigned int nbIndex = bProgram ? m_avFormat->programs[m_program]->nb_stream_indexes : m_avFormat->nb_streams;
  // add streams from selected program, or all streams if no program was selected
  for (unsigned int i = 0; i < nbIndex; ++i) {
    int streamIdx = bProgram ? m_avFormat->programs[m_program]->stream_index[i] : i;
    AddStream(streamIdx);

    AVStream *st = m_avFormat->streams[streamIdx];
    if (st->codec->codec_type == AVMEDIA_TYPE_VIDEO || st->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
      if (st->duration != AV_NOPTS_VALUE) {
        st_duration = av_rescale_q(st->duration, st->time_base, AV_RATIONAL_TIMEBASE);
        if (st_duration > duration)
          duration = st_duration;
      }

      st_start_time = av_rescale_q(st->start_time, st->time_base, AV_RATIONAL_TIMEBASE);
      if (st_start_time < start_time)
        start_time = st_start_time;
    }
  }

  if (duration != INT64_MIN) {
    m_avFormat->duration = duration;
  }
  if (start_time != INT64_MAX) {
    m_avFormat->start_time = start_time;
  }

  // Create fake subtitle pin
  if(!m_streams[subpic].empty()) {
    CreateNoSubtitleStream();
  }
  return S_OK;
}

REFERENCE_TIME CLAVFDemuxer::GetStartTime() const
{
  return av_rescale(m_avFormat->start_time, DSHOW_TIME_BASE, AV_TIME_BASE);
}

// Converts the lavf pts timestamp to a DShow REFERENCE_TIME
// Based on DVDDemuxFFMPEG
REFERENCE_TIME CLAVFDemuxer::ConvertTimestampToRT(int64_t pts, int num, int den, int64_t starttime) const
{
  if (pts == (int64_t)AV_NOPTS_VALUE) {
    return Packet::INVALID_TIME;
  }

  if(starttime == (int64_t)AV_NOPTS_VALUE) {
    starttime = av_rescale(m_avFormat->start_time, den, (int64_t)AV_TIME_BASE * num);
  }

  if(starttime != 0) {
    pts -= starttime;
  }

  // Let av_rescale do the work, its smart enough to not overflow
  REFERENCE_TIME timestamp = av_rescale(pts, (int64_t)num * DSHOW_TIME_BASE, den);

  return timestamp;
}

// Converts the lavf pts timestamp to a DShow REFERENCE_TIME
// Based on DVDDemuxFFMPEG
int64_t CLAVFDemuxer::ConvertRTToTimestamp(REFERENCE_TIME timestamp, int num, int den, int64_t starttime) const
{
  if (timestamp == Packet::INVALID_TIME) {
    return (int64_t)AV_NOPTS_VALUE;
  }

  if(starttime == (int64_t)AV_NOPTS_VALUE) {
    starttime = av_rescale(m_avFormat->start_time, den, (int64_t)AV_TIME_BASE * num);
  }

  int64_t pts = av_rescale(timestamp, den, (int64_t)num * DSHOW_TIME_BASE);
  if(starttime != 0) {
    pts += starttime;
  }

  return pts;
}

// Select the best video stream
const CBaseDemuxer::stream *CLAVFDemuxer::SelectVideoStream()
{
  const stream *best = NULL;
  CStreamList *streams = GetStreams(video);

  std::deque<stream>::iterator it;
  for ( it = streams->begin(); it != streams->end(); ++it ) {
    stream *check = &*it;
    if (!best) { best = check; continue; }
    uint64_t bestPixels = m_avFormat->streams[best->pid]->codec->width * m_avFormat->streams[best->pid]->codec->height;
    uint64_t checkPixels = m_avFormat->streams[check->pid]->codec->width * m_avFormat->streams[check->pid]->codec->height;

    if (checkPixels > bestPixels) {
      best = check;
    } else if (checkPixels == bestPixels) {
      int best_rate = m_avFormat->streams[best->pid]->codec->bit_rate;
      int check_rate = m_avFormat->streams[check->pid]->codec->bit_rate;
      if (best_rate && check_rate && check_rate > best_rate)
        best = check;
    }
  }

  return best;
}

static int audio_codec_priority(AVCodecContext *codec)
{
  int priority = 0;
  switch(codec->codec_id) {
  case CODEC_ID_FLAC:
  case CODEC_ID_TRUEHD:
  // All the PCM codecs
  case CODEC_ID_PCM_S16LE:
  case CODEC_ID_PCM_S16BE:
  case CODEC_ID_PCM_U16LE:
  case CODEC_ID_PCM_U16BE:
  case CODEC_ID_PCM_S32LE:
  case CODEC_ID_PCM_S32BE:
  case CODEC_ID_PCM_U32LE:
  case CODEC_ID_PCM_U32BE:
  case CODEC_ID_PCM_S24LE:
  case CODEC_ID_PCM_S24BE:
  case CODEC_ID_PCM_U24LE:
  case CODEC_ID_PCM_U24BE:
  case CODEC_ID_PCM_F32BE:
  case CODEC_ID_PCM_F32LE:
  case CODEC_ID_PCM_F64BE:
  case CODEC_ID_PCM_F64LE:
  case CODEC_ID_PCM_DVD:
  case CODEC_ID_PCM_BLURAY:
    priority = 10;
    break;
  case CODEC_ID_DTS:
    priority = 7;
    if (codec->profile >= FF_PROFILE_DTS_HD_HRA) {
      priority += 2;
    } else if (codec->profile >= FF_PROFILE_DTS_ES) {
      priority += 1;
    }
    break;
  case CODEC_ID_AC3:
  case CODEC_ID_EAC3:
  case CODEC_ID_AAC:
  case CODEC_ID_AAC_LATM:
    priority = 5;
    break;
  }

  // WAVE_FORMAT_EXTENSIBLE is multi-channel PCM, which doesn't have a proper tag otherwise
  if(codec->codec_tag == WAVE_FORMAT_EXTENSIBLE) {
    priority = 10;
  }

  return priority;
}

// Select the best audio stream
const CBaseDemuxer::stream *CLAVFDemuxer::SelectAudioStream(std::list<std::string> prefLanguages)
{
  const stream *best = NULL;
  CStreamList *streams = GetStreams(audio);

  std::deque<stream*> checkedStreams;

  // Filter for language
  if(!prefLanguages.empty()) {
    std::list<std::string>::iterator it;
    for ( it = prefLanguages.begin(); it != prefLanguages.end(); ++it ) {
      std::string checkLanguage = *it;
      // Convert 2-character code to 3-character
      if (checkLanguage.length() == 2) {
        checkLanguage = ISO6391To6392(checkLanguage.c_str());
      }
      std::deque<stream>::iterator sit;
      for ( sit = streams->begin(); sit != streams->end(); ++sit ) {
        const char *lang = get_stream_language(m_avFormat->streams[sit->pid]);
        if (lang) {
          std::string language = std::string(lang);
          // Convert 2-character code to 3-character
          if (language.length() == 2) {
            language = ISO6391To6392(lang);
          }
          // check if the language matches
          if (language == checkLanguage) {
            checkedStreams.push_back(&*sit);
          }
        }
      }
      // First language that has any streams is a match
      if (!checkedStreams.empty()) {
        break;
      }
    }
  }

  // If no language was set, or no matching streams were found
  // Put all streams in there
  if (checkedStreams.empty()) {
    std::deque<stream>::iterator sit;
    for ( sit = streams->begin(); sit != streams->end(); ++sit ) {
      checkedStreams.push_back(&*sit);
    }
  }

  // Check for a stream with a default flag
  // If in our current set is one, that one prevails
  std::deque<stream*>::iterator sit;
  for ( sit = checkedStreams.begin(); sit != checkedStreams.end(); ++sit ) {
    if (m_avFormat->streams[(*sit)->pid]->disposition & AV_DISPOSITION_DEFAULT) {
      best = *sit;
      break;
    }
  }

  if (!best && !checkedStreams.empty()) {
    // If only one stream is left, just use that one
    if (checkedStreams.size() == 1) {
      best = checkedStreams.at(0);
    } else {
      // Check for quality
      std::deque<stream*>::iterator sit;
      for ( sit = checkedStreams.begin(); sit != checkedStreams.end(); ++sit ) {
        if(!best) { best = *sit; continue; }
        AVStream *old_stream = m_avFormat->streams[best->pid];
        AVStream *new_stream = m_avFormat->streams[(*sit)->pid];
        // First, check number of channels
        int old_num_chans = old_stream->codec->channels;
        int new_num_chans = new_stream->codec->channels;
        if (new_num_chans > old_num_chans) {
          best = *sit;
        } else if (new_num_chans == old_num_chans) {
          // Same number of channels, check codec
          int old_priority = audio_codec_priority(old_stream->codec);
          int new_priority = audio_codec_priority(new_stream->codec);
          if (new_priority > old_priority) {
            best = *sit;
          }
        }
      }
    }
  }

  return best;
}

// Select the best subtitle stream
const CBaseDemuxer::stream *CLAVFDemuxer::SelectSubtitleStream(std::list<std::string> prefLanguages, int subtitleMode, BOOL bOnlyMatching)
{
  const stream *best = NULL;
  CStreamList *streams = GetStreams(subpic);

  if (subtitleMode == SUBMODE_NO_SUBS) {
    // Find the no-subs stream
    return streams->FindStream(NO_SUBTITLE_PID);
  }

  std::deque<stream*> checkedStreams;

  // Filter for language
  if(!prefLanguages.empty()) {
    std::list<std::string>::iterator it;
    for ( it = prefLanguages.begin(); it != prefLanguages.end(); ++it ) {
      std::string checkLanguage = *it;
      // Convert 2-character code to 3-character
      if (checkLanguage.length() == 2) {
        checkLanguage = ISO6391To6392(checkLanguage.c_str());
      }
      std::deque<stream>::iterator sit;
      for ( sit = streams->begin(); sit != streams->end(); ++sit ) {
        // Don't even try to check the no-subtitles stream
        if (sit->pid == NO_SUBTITLE_PID) { continue; }
        const char *lang = get_stream_language(m_avFormat->streams[sit->pid]);
        if (lang) {
          std::string language = std::string(lang);
          // Convert 2-character code to 3-character
          if (language.length() == 2) {
            language = ISO6391To6392(lang);
          }
          // check if the language matches
          if (language == checkLanguage) {
            checkedStreams.push_back(&*sit);
          }
        }
      }
      // First language that has any streams is a match
      if (!checkedStreams.empty()) {
        break;
      }
    }
  }

  // If no language was set, or no matching streams were found
  if (checkedStreams.empty() && (!bOnlyMatching || prefLanguages.empty())) {
    std::deque<stream>::iterator sit;
    for ( sit = streams->begin(); sit != streams->end(); ++sit ) {
      // Don't insert the no-subtitle stream in the list of possible candidates
      if ((*sit).pid == NO_SUBTITLE_PID) { continue; }
      checkedStreams.push_back(&*sit);
    }
  }

  // Check for a stream with a default flag
  // If in our current set is one, that one prevails
  std::deque<stream*>::iterator sit;
  for ( sit = checkedStreams.begin(); sit != checkedStreams.end(); ++sit ) {
    if (m_avFormat->streams[(*sit)->pid]->disposition & AV_DISPOSITION_DEFAULT) {
      if (!(subtitleMode == SUBMODE_FORCED_SUBS) == !(m_avFormat->streams[(*sit)->pid]->disposition & AV_DISPOSITION_FORCED)) {
        best = *sit;
        break;
      }
    }
  }

  // Select the best stream based on subtitle mode
  if (!best && !checkedStreams.empty()) {
    std::deque<stream*>::iterator sit;
    for ( sit = checkedStreams.begin(); sit != checkedStreams.end(); ++sit ) {
      AVStream *pStream = m_avFormat->streams[(*sit)->pid];

      // Check if the first stream qualifys for us. Forced if we want forced, not forced if we don't want forced.
      if (!(subtitleMode == SUBMODE_FORCED_SUBS) == !(m_avFormat->streams[(*sit)->pid]->disposition & AV_DISPOSITION_FORCED)) {
        best = *sit;
        break;
      // Special Check for "want forced subs" and "PGS only forced subs" - we just accept whatever PGS stream we find first.
      } else if (subtitleMode == SUBMODE_FORCED_SUBS && pStream->codec->codec_id == CODEC_ID_HDMV_PGS_SUBTITLE && m_pSettings->GetPGSOnlyForced() && !best) {
        best = *sit;
      } else if (subtitleMode == SUBMODE_ALWAYS_SUBS && !best) {
        // we found a forced track, but want full. Cycle through until we run out of tracks, or find a new full
        best = *sit;
      }
    }
  }

  if (!best) {
    best = streams->FindStream(NO_SUBTITLE_PID);
  }

  return best;
}
