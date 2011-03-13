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

static const AVRational AV_RATIONAL_TIMEBASE = {1, AV_TIME_BASE};

CLAVFDemuxer::CLAVFDemuxer(CCritSec *pLock)
  : CBaseDemuxer(L"lavf demuxer", pLock), m_avFormat(NULL), m_rtCurrent(0), m_bIsStream(false), m_bMatroska(false), m_bAVI(false), m_bMPEGTS(false), m_program(0), m_bVC1Correction(true)
{
  av_register_all();
  register_protocol(&ufile_protocol);
#ifdef DEBUG
  DbgSetModuleLevel (LOG_CUSTOM1, DWORD_MAX); // FFMPEG messages use custom1
  av_log_set_callback(lavf_log_callback);
#endif
}

CLAVFDemuxer::~CLAVFDemuxer()
{
  CleanupAVFormat();
}

STDMETHODIMP CLAVFDemuxer::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = NULL;

  return
    QI(IKeyFrameInfo)
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
  // The "ufile" protocol then converts it back to wchar_t to pass it to windows APIs
  // Isn't it great?
  char fileName[4096] = "ufile:";
  ret = WideCharToMultiByte(CP_UTF8, 0, pszFileName, -1, fileName+6, 4090, NULL, NULL);

  ret = av_open_input_file(&m_avFormat, fileName, NULL, FFMPEG_FILE_BUFFER_SIZE, NULL);
  if (ret < 0) {
    DbgLog((LOG_ERROR, 0, TEXT("av_open_input_file failed (%d)"), ret));
    goto done;
  }

  CHECK_HR(hr = InitAVFormat());

  return S_OK;
done:
  CleanupAVFormat();
  return E_FAIL;
}

STDMETHODIMP CLAVFDemuxer::OpenInputStream(AVIOContext *byteContext)
{
  CAutoLock lock(m_pLock);
  HRESULT hr = S_OK;

  int ret; // return code from avformat functions

  AVInputFormat *fmt = NULL;

  m_bIsStream = true;

  ret = av_probe_input_buffer(byteContext, &fmt, "", NULL, 0, FFMPEG_FILE_BUFFER_SIZE);
  if (ret < 0) {
    DbgLog((LOG_ERROR, 0, TEXT("av_probe_input_buffer failed (%d)"), ret));
    goto done;
  }
  ret = av_open_input_stream(&m_avFormat, byteContext, "", fmt, NULL);
  if (ret < 0) {
    DbgLog((LOG_ERROR, 0, TEXT("av_open_input_stream failed (%d)"), ret));
    goto done;
  }

  CHECK_HR(hr = InitAVFormat());

  return S_OK;
done:
  CleanupAVFormat();
  return E_FAIL;
}

STDMETHODIMP CLAVFDemuxer::InitAVFormat()
{
  HRESULT hr = S_OK;

  unsigned int idx = 0;

  int ret = av_find_stream_info(m_avFormat);
  if (ret < 0) {
    DbgLog((LOG_ERROR, 0, TEXT("av_find_stream_info failed (%d)"), ret));
    goto done;
  }

  // Re-compute the overall file duration based on video and audio durations
  int64_t duration = INT64_MIN;
  int64_t st_duration = 0;

  for(idx = 0; idx < m_avFormat->nb_streams; ++idx) {
    AVStream *st = m_avFormat->streams[idx];
    if (st->duration != AV_NOPTS_VALUE && (st->codec->codec_type == AVMEDIA_TYPE_VIDEO || st->codec->codec_type == AVMEDIA_TYPE_AUDIO)) {
      st_duration = av_rescale_q(st->duration, st->time_base, AV_RATIONAL_TIMEBASE);
      if (st_duration > duration)
        duration = st_duration;
    }
  }
  if (duration != INT64_MIN) {
    m_avFormat->duration = duration;
  }

  m_bMatroska = (_strnicmp(m_avFormat->iformat->name, "matroska", 8) == 0);
  m_bAVI = (_strnicmp(m_avFormat->iformat->name, "avi", 3) == 0);
  m_bMPEGTS = (_strnicmp(m_avFormat->iformat->name, "mpegts", 6) == 0);

  CHECK_HR(hr = CreateStreams());

  return S_OK;
done:
  CleanupAVFormat();
  return E_FAIL;
}

void CLAVFDemuxer::CleanupAVFormat()
{
  if (m_avFormat) {
    for (unsigned int idx = 0; idx < m_avFormat->nb_streams; ++idx)
      free(m_avFormat->streams[idx]->codec->opaque);
    if (m_bIsStream)
      av_close_input_stream(m_avFormat);
    else
      av_close_input_file(m_avFormat);
    m_avFormat = NULL;
  }
}

void CLAVFDemuxer::SettingsChanged(ILAVFSettings *pSettings)
{
  int vc1Mode = pSettings->GetVC1TimestampMode();
  if (vc1Mode == 1 || (vc1Mode == 2 && !pSettings->IsVC1CompatModeRequired())) {
    m_bVC1Correction = true;
  } else {
    m_bVC1Correction = false;
  }

  for(unsigned int idx = 0; idx < m_avFormat->nb_streams; ++idx) {
    AVStream *st = m_avFormat->streams[idx];
    if (st->codec->codec_id == CODEC_ID_VC1) {
      st->need_parsing = m_bVC1Correction ? AVSTREAM_PARSE_TIMESTAMPS : AVSTREAM_PARSE_NONE;
    }
  }
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
    result = av_read_frame(m_avFormat, &pkt);
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

    // we need to get duration slightly different for matroska embedded text subtitels
    if(m_bMatroska && stream->codec->codec_id == CODEC_ID_TEXT && pkt.convergence_duration != 0) {
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

    // stupid VC1
    if (m_bVC1Correction && stream->codec->codec_id == CODEC_ID_VC1 && dts != Packet::INVALID_TIME) {
      rt = dts;
    }

    pPacket->rtStart = pPacket->rtStop = rt;
    if (rt != Packet::INVALID_TIME) {
      pPacket->rtStop += (duration > 0) ? duration : 1;
    }

    if (stream->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
      pPacket->bDiscontinuity = TRUE;
    } else {
      pPacket->bSyncPoint = (rt != Packet::INVALID_TIME && duration > 0) ? 1 : 0;
      pPacket->bAppendable = 0; //!pPacket->bSyncPoint;
    }

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

  return S_OK;
}

const char *CLAVFDemuxer::GetContainerFormat() const
{
  return m_avFormat->iformat->name;
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
  AVStream *stream = m_avFormat->streams[m_dActiveStreams[video]];
  nKFs = stream->nb_index_entries;
  return (stream->nb_index_entries == stream->nb_frames) ? S_FALSE : S_OK;
}

STDMETHODIMP CLAVFDemuxer::GetKeyFrames(const GUID* pFormat, REFERENCE_TIME* pKFs, UINT& nKFs)
{
  CheckPointer(pFormat, E_POINTER);
  CheckPointer(pKFs, E_POINTER);

  if(m_dActiveStreams[video] < 0) { return E_NOTIMPL; }

  if(*pFormat != TIME_FORMAT_MEDIA_TIME) return E_INVALIDARG;

  AVStream *stream = m_avFormat->streams[m_dActiveStreams[video]];
  nKFs = stream->nb_index_entries;
  for(unsigned int i = 0; i < nKFs; ++i) {
    pKFs[i] = ConvertTimestampToRT(stream->index_entries[i].timestamp, stream->time_base.num, stream->time_base.den);
  }
  return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Internal Functions
STDMETHODIMP CLAVFDemuxer::AddStream(int streamId)
{
  HRESULT hr = S_OK;
  AVStream *pStream = m_avFormat->streams[streamId];
  stream s;
  s.pid = streamId;
  s.streamInfo = new CLAVFStreamInfo(pStream, m_avFormat->iformat->name, hr);

  if(FAILED(hr)) {
    delete s.streamInfo;
    return hr;
  }

  pStream->codec->opaque = NULL;
  // HACK: Change codec_id to TEXT for SSA to prevent some evil doings
  if (pStream->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
    pStream->codec->opaque = malloc(sizeof(CodecID));
    *((CodecID *)pStream->codec->opaque) = pStream->codec->codec_id;
    pStream->codec->codec_id = CODEC_ID_TEXT;
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

  if (m_avFormat->nb_programs) {
    m_program = UINT_MAX;
    // look for first non empty stream and discard nonselected programs
    for (unsigned int i = 0; i < m_avFormat->nb_programs; ++i) {
      if(m_program == UINT_MAX && m_avFormat->programs[i]->nb_stream_indexes > 0) {
        m_program = i;
      }

      if(i != m_program) {
        m_avFormat->programs[i]->discard = AVDISCARD_ALL;
      }
    }
    if(m_program == UINT_MAX) {
      m_program = 0;
    }
    // add streams from selected program
    for (unsigned int i = 0; i < m_avFormat->programs[m_program]->nb_stream_indexes; ++i) {
      AddStream(m_avFormat->programs[m_program]->stream_index[i]);
    }
  } else {
    for(unsigned int streamId = 0; streamId < m_avFormat->nb_streams; ++streamId) {
      AddStream(streamId);
    }
  }

  // Create fake subtitle pin
  if(!m_streams[subpic].empty()) {
    CreateNoSubtitleStream();
  }
  return S_OK;
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
    priority = 8;
    break;
  case CODEC_ID_AC3:
  case CODEC_ID_EAC3:
  case CODEC_ID_AAC:
  case CODEC_ID_AAC_LATM:
    priority = 7;
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
            if (subtitleMode == SUBMODE_ALWAYS_SUBS || ((subtitleMode == SUBMODE_FORCED_SUBS) && (m_avFormat->streams[sit->pid]->disposition & AV_DISPOSITION_FORCED))) {
              checkedStreams.push_back(&*sit);
            }
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
      if (subtitleMode != SUBMODE_FORCED_SUBS || (m_avFormat->streams[(*sit)->pid]->disposition & AV_DISPOSITION_FORCED)) {
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
      if (subtitleMode == SUBMODE_FORCED_SUBS && (pStream->disposition & AV_DISPOSITION_FORCED)
        || subtitleMode == SUBMODE_ALWAYS_SUBS && !(pStream->disposition & AV_DISPOSITION_FORCED) ) {
          best = *sit;
          break;
      } else if (subtitleMode == SUBMODE_ALWAYS_SUBS) {
        // we found a forced track, but want full. Cycle through until we run out of tracks, or find a new full
        if(!best)
          best = *sit;
      }
    }
  }

  if (!best) {
    best = streams->FindStream(NO_SUBTITLE_PID);
  }

  return best;
}
