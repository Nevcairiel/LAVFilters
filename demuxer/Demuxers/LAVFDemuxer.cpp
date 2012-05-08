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
#include "LAVFDemuxer.h"
#include "LAVFUtils.h"
#include "LAVFStreamInfo.h"
#include "ILAVPinInfo.h"

#include "LAVSplitterSettingsInternal.h"

#include "moreuuids.h"

extern "C" {
#include "libavformat/mpegts.h"
}

#ifdef DEBUG
#include "lavf_log.h"
extern "C" {
#include "libavutil/log.h"
}
#endif

#include "BDDemuxer.h"

#define AVFORMAT_GENPTS 0
#define AVFORMAT_OPEN_TIMEOUT 20

extern void lavf_get_iformat_infos(AVInputFormat *pFormat, const char **pszName, const char **pszDescription);

static const AVRational AV_RATIONAL_TIMEBASE = {1, AV_TIME_BASE};

void CLAVFDemuxer::ffmpeg_init()
{
#ifdef DEBUG
  DbgSetModuleLevel (LOG_CUSTOM1, DWORD_MAX); // FFMPEG messages use custom1
  av_log_set_callback(lavf_log_callback);
#endif

  av_register_all();
  avformat_network_init();
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

CLAVFDemuxer::CLAVFDemuxer(CCritSec *pLock, ILAVFSettingsInternal *settings)
  : CBaseDemuxer(L"lavf demuxer", pLock)
  , m_avFormat(NULL)
  , m_program(0)
  , m_rtCurrent(0)
  , m_bMatroska(FALSE)
  , m_bOgg(FALSE)
  , m_bAVI(FALSE)
  , m_bMPEGTS(FALSE)
  , m_bEVO(FALSE)
  , m_bRM(FALSE)
  , m_bBluRay(FALSE)
  , m_pBluRay(NULL)
  , m_bVC1Correction(FALSE)
  , m_bVC1SeenTimestamp(FALSE)
  , m_bPGSNoParsing(TRUE)
  , m_ForcedSubStream(-1)
  , m_pSettings(NULL)
  , m_stOrigParser(NULL)
  , m_pFontInstaller(NULL)
  , m_pszInputFormat(NULL)
  , m_bEnableTrackInfo(TRUE)
  , m_Abort(0), m_timeOpening(0)
{
  m_bSubStreams = settings->GetSubstreamsEnabled();

  m_pSettings = settings;

  WCHAR fileName[1024];
  GetModuleFileName(NULL, fileName, 1024);
  const WCHAR *file = PathFindFileName (fileName);

  if (_wcsicmp(file, L"zplayer.exe") == 0) {
    m_bEnableTrackInfo = FALSE;

    // TrackInfo is only properly handled in ZoomPlayer 8.0.0.74 and above
    DWORD dwVersionSize = GetFileVersionInfoSize(fileName, NULL);
    if (dwVersionSize > 0) {
      void *versionInfo = CoTaskMemAlloc(dwVersionSize);
      GetFileVersionInfo(fileName, 0, dwVersionSize, versionInfo);
      VS_FIXEDFILEINFO *info;
      unsigned cbInfo;
      BOOL bInfoPresent = VerQueryValue(versionInfo, TEXT("\\"), (LPVOID*)&info, &cbInfo);
      if (bInfoPresent) {
        bInfoPresent = bInfoPresent;
        uint64_t version = info->dwFileVersionMS;
        version <<= 32;
        version += info->dwFileVersionLS;
        if (version >= 0x000800000000004A)
          m_bEnableTrackInfo = TRUE;
      }
      CoTaskMemFree(versionInfo);
    }
  }
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
    m_bEnableTrackInfo && QI(ITrackInfo)
    QI2(IAMExtendedSeeking)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

/////////////////////////////////////////////////////////////////////////////
// Demuxer Functions
STDMETHODIMP CLAVFDemuxer::Open(LPCOLESTR pszFileName)
{
  return OpenInputStream(NULL, pszFileName);
}

STDMETHODIMP CLAVFDemuxer::AbortOpening(int mode)
{
  m_Abort = mode;
  return S_OK;
}

int CLAVFDemuxer::avio_interrupt_cb(void *opaque)
{
  CLAVFDemuxer *demux = (CLAVFDemuxer *)opaque;

  // Check for file opening timeout
  time_t now = time(NULL);
  if (demux->m_timeOpening && now > (demux->m_timeOpening + AVFORMAT_OPEN_TIMEOUT))
    return 1;

  return demux->m_Abort;
}

STDMETHODIMP CLAVFDemuxer::OpenInputStream(AVIOContext *byteContext, LPCOLESTR pszFileName)
{
  CAutoLock lock(m_pLock);
  HRESULT hr = S_OK;

  int ret; // return code from avformat functions

  // Convert the filename from wchar to char for avformat
  char fileName[4100] = {0};
  if (pszFileName) {
    ret = WideCharToMultiByte(CP_UTF8, 0, pszFileName, -1, fileName, 4096, NULL, NULL);
  }

  if (_strnicmp("mms:", fileName, 4) == 0) {
    memmove(fileName+1, fileName, strlen(fileName));
    memcpy(fileName, "mmsh", 4);
  }

  AVIOInterruptCB cb = {avio_interrupt_cb, this};

  // Create the avformat_context
  m_avFormat = avformat_alloc_context();
  m_avFormat->pb = byteContext;
  m_avFormat->interrupt_callback = cb;

  m_timeOpening = time(NULL);
  ret = avformat_open_input(&m_avFormat, fileName, NULL, NULL);
  if (ret < 0) {
    DbgLog((LOG_ERROR, 0, TEXT("::OpenInputStream(): avformat_open_input failed (%d)"), ret));
    goto done;
  }
  DbgLog((LOG_TRACE, 10, TEXT("::OpenInputStream(): avformat_open_input opened file of type '%S' (took %I64d seconds)"), m_avFormat->iformat->name, time(NULL) - m_timeOpening));
  m_timeOpening = 0;

  CHECK_HR(hr = InitAVFormat(pszFileName));

  return S_OK;
done:
  CleanupAVFormat();
  return E_FAIL;
}

void CLAVFDemuxer::AddMPEGTSStream(int pid, uint32_t stream_type)
{
  if (m_avFormat) {
    int program = -1;
    if (m_avFormat->nb_programs > 0) {
      unsigned nb_streams = 0;
      for (unsigned i = 0; i < m_avFormat->nb_programs; i++) {
        if (m_avFormat->programs[i]->nb_stream_indexes > nb_streams)
          program = i;
      }
    }
    avpriv_mpegts_add_stream(m_avFormat, pid, stream_type, program >= 0 ? m_avFormat->programs[program]->id : -1);
  }
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
  WideCharToMultiByte(CP_UTF8, 0, pszFileName, -1, path, (int)a_len, NULL, NULL);

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
          av_dict_set(&avstream->metadata, "language", (const char *)s->lang, 0);
      }
    }
  }
  // Free the clip
  clpi_free(cl);
  cl = NULL;

  return S_OK;
}

inline static int init_parser(AVFormatContext *s, AVStream *st) {
  if (!st->parser && st->need_parsing && !(s->flags & AVFMT_FLAG_NOPARSE)) {
    st->parser = av_parser_init(st->codec->codec_id);
    if (st->parser) {
      if(st->need_parsing == AVSTREAM_PARSE_HEADERS){
        st->parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;
      }else if(st->need_parsing == AVSTREAM_PARSE_FULL_ONCE){
        st->parser->flags |= PARSER_FLAG_ONCE;
      }
    } else {
      return -1;
    }
  }
  return 0;
}

void CLAVFDemuxer::UpdateParserFlags(AVStream *st) {
  if (st->parser) {
    if ((st->codec->codec_id == CODEC_ID_MPEG2VIDEO || st->codec->codec_id == CODEC_ID_MPEG1VIDEO) && _stricmp(m_pszInputFormat, "mpegvideo") != 0) {
      st->parser->flags |= PARSER_FLAG_NO_TIMESTAMP_MANGLING;
    } else if (st->codec->codec_id == CODEC_ID_VC1) {
      if (m_bVC1Correction) {
        st->parser->flags &= ~PARSER_FLAG_NO_TIMESTAMP_MANGLING;
      } else {
        st->parser->flags |= PARSER_FLAG_NO_TIMESTAMP_MANGLING;
      }
    }
  }
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

  LPWSTR extension = pszFileName ? PathFindExtensionW(pszFileName) : NULL;

  m_bMatroska = (_strnicmp(m_pszInputFormat, "matroska", 8) == 0);
  m_bOgg = (_strnicmp(m_pszInputFormat, "ogg", 3) == 0);
  m_bAVI = (_strnicmp(m_pszInputFormat, "avi", 3) == 0);
  m_bMPEGTS = (_strnicmp(m_pszInputFormat, "mpegts", 6) == 0);
  m_bEVO = ((extension ? _wcsicmp(extension, L".evo") == 0 : TRUE) && _stricmp(m_pszInputFormat, "mpeg") == 0);
  m_bRM = (_stricmp(m_pszInputFormat, "rm") == 0);

  if (AVFORMAT_GENPTS) {
    m_avFormat->flags |= AVFMT_FLAG_GENPTS;
  }

  m_avFormat->flags |= AVFMT_FLAG_IGNPARSERSYNC;

  if (pszFileName) {
    WCHAR szOut[24];
    DWORD dwNumChars = 24;
    hr = UrlGetPart(pszFileName, szOut, &dwNumChars, URL_PART_SCHEME, 0);
    if (SUCCEEDED(hr) && dwNumChars && (_wcsicmp(szOut, L"file") != 0)) {
      m_avFormat->flags |= AVFMT_FLAG_NETWORK;
    }
  }

  // Increase default probe sizes
  //m_avFormat->probesize            = 5 * 5000000;
  if (m_bMPEGTS)
    m_avFormat->max_analyze_duration = (m_avFormat->max_analyze_duration * 3) / 2;

  m_timeOpening = time(NULL);
  int ret = avformat_find_stream_info(m_avFormat, NULL);
  if (ret < 0) {
    DbgLog((LOG_ERROR, 0, TEXT("::InitAVFormat(): av_find_stream_info failed (%d)"), ret));
    goto done;
  }
  DbgLog((LOG_TRACE, 10, TEXT("::InitAVFormat(): avformat_find_stream_info finished, took %I64d seconds"), time(NULL) - m_timeOpening));
  m_timeOpening = 0;

  // Check if this is a m2ts in a BD structure, and if it is, read some extra stream properties out of the CLPI files
  if (m_bBluRay && m_pBluRay) {
    m_pBluRay->ProcessClipLanguages();
  } else if (pszFileName && m_bMPEGTS) {
    CheckBDM2TSCPLI(pszFileName);
  }

  SAFE_CO_FREE(m_stOrigParser);
  m_stOrigParser = (enum AVStreamParseType *)CoTaskMemAlloc(m_avFormat->nb_streams * sizeof(enum AVStreamParseType));

  for(unsigned int idx = 0; idx < m_avFormat->nb_streams; ++idx) {
    AVStream *st = m_avFormat->streams[idx];

    // Disable full stream parsing for these formats
    if (st->need_parsing == AVSTREAM_PARSE_FULL) {
      if (st->codec->codec_id == CODEC_ID_DVB_SUBTITLE) {
        st->need_parsing = AVSTREAM_PARSE_NONE;
      }
    }

    if (m_bOgg && st->codec->codec_id == CODEC_ID_H264) {
      st->need_parsing = AVSTREAM_PARSE_FULL;
    }

    // Create the parsers with the appropriate flags
    init_parser(m_avFormat, st);
    UpdateParserFlags(st);

#ifdef DEBUG
    DbgLog((LOG_TRACE, 30, L"Stream %d (pid %d) - codec: %d; parsing: %S;", idx, st->id, st->codec->codec_id, lavf_get_parsing_string(st->need_parsing)));
#endif
    m_stOrigParser[idx] = st->need_parsing;

    if ((st->codec->codec_id == CODEC_ID_DTS && st->codec->codec_tag == 0xA2)
     || (st->codec->codec_id == CODEC_ID_EAC3 && st->codec->codec_tag == 0xA1))
      st->disposition |= LAVF_DISPOSITION_SECONDARY_AUDIO;

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

HRESULT CLAVFDemuxer::SetActiveStream(StreamType type, int pid)
{
  HRESULT hr = S_OK;

  if (type == audio)
    UpdateForcedSubtitleStream(pid);

  hr = __super::SetActiveStream(type, pid);

  for(unsigned int idx = 0; idx < m_avFormat->nb_streams; ++idx) {
    AVStream *st = m_avFormat->streams[idx];
    if (st->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      st->discard = (m_dActiveStreams[video] == idx) ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
    } else if (st->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
      st->discard = (m_dActiveStreams[audio] == idx) ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
      // If the stream is a sub stream, make sure to activate the main stream as well
      if (m_bMPEGTS && (st->disposition & LAVF_DISPOSITION_SUB_STREAM) && st->discard == AVDISCARD_DEFAULT) {
        for(unsigned int idx2 = 0; idx2 < m_avFormat->nb_streams; ++idx2) {
          AVStream *mst = m_avFormat->streams[idx2];
          if (mst->id == st->id) {
            mst->discard = AVDISCARD_DEFAULT;
            break;
          }
        }
      }
    } else if (st->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
      st->discard = (m_dActiveStreams[subpic] == idx || (m_dActiveStreams[subpic] == FORCED_SUBTITLE_PID && m_ForcedSubStream == idx)) ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
    } else {
      st->discard = AVDISCARD_ALL;
    }
  }

  return hr;
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
        av_dict_copy(&sub_st->metadata, st->metadata, 0);
      }
    }
  }
}

void CLAVFDemuxer::SettingsChanged(ILAVFSettingsInternal *pSettings)
{
  int vc1Mode = pSettings->GetVC1TimestampMode();
  if (vc1Mode == 1 || strcmp(m_pszInputFormat, "rawvideo") == 0) {
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
      UpdateParserFlags(st);
    } else if (st->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      st->need_parsing = pSettings->GetVideoParsingEnabled() ? m_stOrigParser[idx] : AVSTREAM_PARSE_NONE;
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
    //DbgLog((LOG_ERROR, 1, TEXT("duration is not available")));
    return -1;
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
  } else if (result == AVERROR_EOF) {
    DbgLog((LOG_TRACE, 10, L"::GetNextPacket(): End of File reached"));
  } else if (result < 0) {
    // meh, fail
  } else if (pkt.size <= 0 || pkt.stream_index < 0 || (unsigned)pkt.stream_index >= m_avFormat->nb_streams) {
    // XXX, in some cases ffmpeg returns a zero or negative packet size
    if(m_avFormat->pb && !m_avFormat->pb->eof_reached) {
      bReturnEmpty = true;
    }
    av_free_packet(&pkt);
  } else {
    // Check right here if the stream is active, we can drop the package otherwise.
    BOOL streamActive = FALSE;
    BOOL forcedSubStream = FALSE;
    for(int i = 0; i < unknown; ++i) {
      if(m_dActiveStreams[i] == pkt.stream_index) {
        streamActive = TRUE;
        break;
      }
    }

    // Accept it if its the forced subpic stream
    if (m_dActiveStreams[subpic] == FORCED_SUBTITLE_PID && pkt.stream_index == m_ForcedSubStream) {
      forcedSubStream = streamActive = TRUE;
    }

    if(!streamActive) {
      av_free_packet(&pkt);
      return S_FALSE;
    }

    AVStream *stream = m_avFormat->streams[pkt.stream_index];
    pPacket = new Packet();
    pPacket->bPosition = pkt.pos;

    if ((m_bMatroska || m_bOgg) && stream->codec->codec_id == CODEC_ID_H264) {
      if (!stream->codec->extradata_size || stream->codec->extradata[0] != 1 || AV_RB32(pkt.data) == 0x00000001) {
        pPacket->dwFlags |= LAV_PACKET_H264_ANNEXB;
      } else { // No DTS for H264 in native format
        pkt.dts = AV_NOPTS_VALUE;
      }
    }

    if(m_bAVI && stream->codec && stream->codec->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      // AVI's always have borked pts, specially if m_pFormatContext->flags includes
      // AVFMT_FLAG_GENPTS so always use dts
      pkt.pts = AV_NOPTS_VALUE;
    }

    if (stream->codec->codec_id == CODEC_ID_RV10 || stream->codec->codec_id == CODEC_ID_RV20 || stream->codec->codec_id == CODEC_ID_RV30 || stream->codec->codec_id == CODEC_ID_RV40) {
      pkt.pts = AV_NOPTS_VALUE;
    }

    // Never use DTS for these formats
    if (!m_bAVI && (stream->codec->codec_id == CODEC_ID_MPEG2VIDEO || stream->codec->codec_id == CODEC_ID_MPEG1VIDEO || (stream->codec->codec_id == CODEC_ID_H264 && !m_bMatroska)))
      pkt.dts = AV_NOPTS_VALUE;

    if(pkt.data) {
      pPacket->SetData(pkt.data, pkt.size);
    }

    pPacket->StreamId = (DWORD)pkt.stream_index;

    if (m_bMPEGTS && !m_bBluRay) {
      int64_t start_time = av_rescale_q(m_avFormat->start_time, AV_RATIONAL_TIMEBASE, stream->time_base);
      const int64_t pts_diff = pkt.pts - start_time;
      const int64_t dts_diff = pkt.dts - start_time;
      if ((pkt.pts == AV_NOPTS_VALUE || pts_diff < -stream->time_base.den) && (pkt.dts == AV_NOPTS_VALUE || dts_diff < -stream->time_base.den) && stream->pts_wrap_bits < 63) {
        if (pkt.pts != AV_NOPTS_VALUE)
          pkt.pts += 1LL << stream->pts_wrap_bits;
        if (pkt.dts != AV_NOPTS_VALUE)
        pkt.dts += 1LL << stream->pts_wrap_bits;
      }
    }

    REFERENCE_TIME pts = (REFERENCE_TIME)ConvertTimestampToRT(pkt.pts, stream->time_base.num, stream->time_base.den);
    REFERENCE_TIME dts = (REFERENCE_TIME)ConvertTimestampToRT(pkt.dts, stream->time_base.num, stream->time_base.den);
    REFERENCE_TIME duration = (REFERENCE_TIME)ConvertTimestampToRT((m_bMatroska && stream->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) ? pkt.convergence_duration : pkt.duration, stream->time_base.num, stream->time_base.den, 0);

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
          m_bVC1SeenTimestamp = (pts != Packet::INVALID_TIME);
        }
      } else if (m_bVC1Correction) {
        rt = dts;
        pPacket->dwFlags |= LAV_PACKET_PARSED;
      }
    } else if (stream->codec->codec_id == CODEC_ID_MOV_TEXT) {
      pPacket->dwFlags |= LAV_PACKET_MOV_TEXT;
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

      if (forcedSubStream) {
        pPacket->dwFlags |= LAV_PACKET_FORCED_SUBTITLE;
        pPacket->dwFlags &= ~LAV_PACKET_PARSED;
      }
    }

    pPacket->bSyncPoint = pkt.flags & AV_PKT_FLAG_KEY;
    pPacket->bAppendable = 0; //!pPacket->bSyncPoint;
    pPacket->bDiscontinuity = (pkt.flags & AV_PKT_FLAG_CORRUPT);
#ifdef DEBUG
    if (pkt.flags & AV_PKT_FLAG_CORRUPT)
      DbgLog((LOG_TRACE, 10, L"::GetNextPacket() - Signaling Discontinuinty because of corrupt package"));
#endif

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
  if (rTime > 0) {
    if (videoStreamId != -1) {
      AVStream *stream = m_avFormat->streams[videoStreamId];
      int64_t start_time = AV_NOPTS_VALUE;

      // MPEG-TS needs a protection against a wrapped around start time
      // It is possible for the start_time in m_avFormat to be wrapped around, but the start_time in the current stream not to be.
      // In this case, ConvertRTToTimestamp would produce timestamps not valid for seeking.
      //
      // Compensate for this by creating a negative start_time, resembling the actual value in m_avFormat->start_time without wrapping.
      if (m_bMPEGTS && stream->start_time != AV_NOPTS_VALUE) {
        int64_t start = av_rescale_q(stream->start_time, stream->time_base, AV_RATIONAL_TIMEBASE);

        if (start < m_avFormat->start_time
          && m_avFormat->start_time > av_rescale_q(3LL << (stream->pts_wrap_bits - 2), stream->time_base, AV_RATIONAL_TIMEBASE)
          && start < av_rescale_q(3LL << (stream->pts_wrap_bits - 3), stream->time_base, AV_RATIONAL_TIMEBASE)) {
          start_time = m_avFormat->start_time - av_rescale_q(1LL << stream->pts_wrap_bits, stream->time_base, AV_RATIONAL_TIMEBASE);
        }
      }
      seek_pts = ConvertRTToTimestamp(rTime, stream->time_base.num, stream->time_base.den, start_time);
    } else {
      seek_pts = ConvertRTToTimestamp(rTime, 1, AV_TIME_BASE);
    }
  }

  if (seek_pts < 0)
    seek_pts = 0;

  if (strcmp(m_pszInputFormat, "rawvideo") == 0 && seek_pts == 0)
    return SeekByte(0, AVSEEK_FLAG_BACKWARD);

  int flags = AVSEEK_FLAG_BACKWARD;

  int ret = av_seek_frame(m_avFormat, videoStreamId, seek_pts, flags);
  if(ret < 0) {
    DbgLog((LOG_CUSTOM1, 1, L"::Seek() -- Key-Frame Seek failed"));
    ret = av_seek_frame(m_avFormat, videoStreamId, seek_pts, flags | AVSEEK_FLAG_ANY);
    if (ret < 0) {
      DbgLog((LOG_ERROR, 1, L"::Seek() -- Inaccurate Seek failed as well"));
    }
  }

  for (unsigned i = 0; i < m_avFormat->nb_streams; i++) {
    init_parser(m_avFormat, m_avFormat->streams[i]);
    UpdateParserFlags(m_avFormat->streams[i]);
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

  for (unsigned i = 0; i < m_avFormat->nb_streams; i++) {
    init_parser(m_avFormat, m_avFormat->streams[i]);
    UpdateParserFlags(m_avFormat->streams[i]);
  }

  m_bVC1SeenTimestamp = FALSE;

  return S_OK;
}

const char *CLAVFDemuxer::GetContainerFormat() const
{
  return m_pszInputFormat;
}

HRESULT CLAVFDemuxer::StreamInfo(const CBaseDemuxer::stream &s, LCID *plcid, WCHAR **ppszName) const
{
  if (s.pid >= (DWORD)m_avFormat->nb_streams) { return E_FAIL; }

  if (plcid) {
    const char *lang = get_stream_language(m_avFormat->streams[s.pid]);
    if (lang) {
      *plcid = ProbeLangForLCID(lang);
    } else {
      *plcid = 0;
    }
  }

  if(ppszName) {
    std::string info = s.streamInfo->codecInfo;
    size_t len = info.size() + 1;
    *ppszName = (WCHAR *)CoTaskMemAlloc(len * sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, info.c_str(), -1, *ppszName, (int)len);
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
  if (av_dict_get(m_avFormat->chapters[index]->metadata, "title", NULL, 0)) {
    char *title = av_dict_get(m_avFormat->chapters[index]->metadata, "title", NULL, 0)->value;
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

int CLAVFDemuxer::GetStreamIdxFromTotalIdx(size_t index) const
{
  const stream* st = GetStreamFromTotalIdx(index);
  if (st)
    return st->pid;
  return -1;
}

const CBaseDemuxer::stream* CLAVFDemuxer::GetStreamFromTotalIdx(size_t index) const
{
  int type = video;
  size_t count_v = m_streams[video].size();
  size_t count_a = m_streams[audio].size();
  size_t count_s = m_streams[subpic].size();
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

  return &m_streams[type][index];
}


/////////////////////////////////////////////////////////////////////////////
// ITrackInfo
STDMETHODIMP_(UINT) CLAVFDemuxer::GetTrackCount()
{
  if(!m_avFormat)
    return 0;

  size_t count = m_streams[video].size() + m_streams[audio].size() + m_streams[subpic].size();

  return (UINT)count;
}

// \param aTrackIdx the track index (from 0 to GetTrackCount()-1)
STDMETHODIMP_(BOOL) CLAVFDemuxer::GetTrackInfo(UINT aTrackIdx, struct TrackElement* pStructureToFill)
{
  DbgLog((LOG_TRACE, 20, L"ITrackInfo::GetTrackInfo(): index %d, struct: %p", aTrackIdx, pStructureToFill));

  if(!m_avFormat || !pStructureToFill)
    return FALSE;

  ZeroMemory(pStructureToFill, sizeof(*pStructureToFill));
  pStructureToFill->Size = sizeof(*pStructureToFill);

  const stream *st = GetStreamFromTotalIdx(aTrackIdx);
  if (!st || st->pid < 0 || st->pid == NO_SUBTITLE_PID)
    return FALSE;

  if (st->pid == FORCED_SUBTITLE_PID) {
    pStructureToFill->FlagDefault = 0;
    pStructureToFill->FlagForced  = 1;
    pStructureToFill->Type        = TypeSubtitle;
    strcpy_s(pStructureToFill->Language, "und");
  } else {
    const AVStream *avst = m_avFormat->streams[st->pid];

    // Fill structure
    pStructureToFill->FlagDefault = (avst->disposition & AV_DISPOSITION_DEFAULT);
    pStructureToFill->FlagForced = (avst->disposition & AV_DISPOSITION_FORCED);
    strncpy_s(pStructureToFill->Language, st->language.c_str(), _TRUNCATE);
    pStructureToFill->Language[3] = '\0';

    pStructureToFill->Type = (avst->codec->codec_type == AVMEDIA_TYPE_VIDEO) ? TypeVideo :
                             (avst->codec->codec_type == AVMEDIA_TYPE_AUDIO) ? TypeAudio :
                             (avst->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) ? TypeSubtitle : 0;
  }

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

  int id = GetStreamIdxFromTotalIdx(aTrackIdx);
  if (id < 0 || (unsigned)id >= m_avFormat->nb_streams)
    return FALSE;

  const AVStream *st = m_avFormat->streams[id];

  if (st->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
    TrackExtendedInfoVideo* pTEIV = (TrackExtendedInfoVideo*)pStructureToFill;

    ZeroMemory(pTEIV, sizeof(*pTEIV));

    pTEIV->Size = sizeof(*pTEIV);
    pTEIV->DisplayUnit = 0; // always pixels
    pTEIV->DisplayWidth = st->codec->width;
    pTEIV->DisplayHeight = st->codec->height;

    pTEIV->PixelWidth = st->codec->coded_width ? st->codec->coded_width : st->codec->width;
    pTEIV->PixelHeight = st->codec->coded_height ? st->codec->coded_height : st->codec->height;

    pTEIV->AspectRatioType = 0;
    pTEIV->Interlaced = 0;
  } else if (st->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
    TrackExtendedInfoAudio* pTEIA = (TrackExtendedInfoAudio*)pStructureToFill;

    ZeroMemory(pTEIA, sizeof(*pTEIA));

    pTEIA->Size = sizeof(*pTEIA);
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

  int id = GetStreamIdxFromTotalIdx(aTrackIdx);
  if (id < 0 || (unsigned)id >= m_avFormat->nb_streams)
    return FALSE;

  const AVStream *st = m_avFormat->streams[id];

  BSTR trackName = NULL;

  const char *title = NULL;
  if (av_dict_get(st->metadata, "title", NULL, 0)) {
    title = av_dict_get(st->metadata, "title", NULL, 0)->value;
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

  int id = GetStreamIdxFromTotalIdx(aTrackIdx);
  if (id < 0 || (unsigned)id >= m_avFormat->nb_streams)
    return FALSE;

  const AVStream *st = m_avFormat->streams[id];

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

  if (pStream->discard == AVDISCARD_ALL || (pStream->codec->codec_id == CODEC_ID_NONE && pStream->codec->codec_tag == 0) || (!m_bSubStreams && (pStream->disposition & LAVF_DISPOSITION_SUB_STREAM)) || (pStream->disposition & AV_DISPOSITION_ATTACHED_PIC))
    return S_FALSE;

  if (pStream->codec->codec_type == AVMEDIA_TYPE_VIDEO && (!pStream->codec->width || !pStream->codec->height))
    return S_FALSE;

  stream s;
  s.pid = streamId;

  // Extract language
  const char *lang = NULL;
  if (av_dict_get(pStream->metadata, "language", NULL, 0)) {
    lang = av_dict_get(pStream->metadata, "language", NULL, 0)->value;
  }
  s.language = lang ? ProbeForISO6392(lang) : "und";
  s.streamInfo = new CLAVFStreamInfo(m_avFormat, pStream, m_pszInputFormat, hr);

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
    // Use a scoring system to select the best available program
    // A "good" program at least has a valid video and audio stream
    // We'll try here to detect these streams and decide on the best program
    // Every present stream gets one point, if it appears to be valid, it gets 4
    // Valid video streams have a width and height, valid audio streams have a channel count.
    // If one program was found with both streams valid, we'll stop looking.
    DWORD dwScore = 0; // Stream found: 1, stream valid: 4
    for (unsigned int i = 0; i < m_avFormat->nb_programs; ++i) {
      if(m_avFormat->programs[i]->nb_stream_indexes > 0) {
        DWORD dwVideoScore = 0;
        DWORD dwAudioScore = 0;
        for(unsigned k = 0; k < m_avFormat->programs[i]->nb_stream_indexes; ++k) {
          unsigned streamIdx = m_avFormat->programs[i]->stream_index[k];
          AVStream *st = m_avFormat->streams[streamIdx];
          if (st->codec->codec_type == AVMEDIA_TYPE_VIDEO && dwVideoScore < 4) {
            if (st->codec->width != 0 && st->codec->height != 0)
              dwVideoScore = 4;
            else
              dwVideoScore = 1;
          } else if (st->codec->codec_type == AVMEDIA_TYPE_AUDIO && dwAudioScore < 4) {
            if (st->codec->channels != 0)
              dwAudioScore = 4;
            else
              dwAudioScore = 1;
          }
        }

        // Check the score of the previously found stream
        // In addition, we always require a valid video stream (or none), a invalid one is not allowed.
        if (dwVideoScore != 1 && (dwVideoScore+dwAudioScore) > dwScore) {
          dwScore = dwVideoScore+dwAudioScore;
          m_program = i;
          if (dwScore == 8)
            break;
        }
      }
    }
  }

  // File has programs
  bool bProgram = (m_program < m_avFormat->nb_programs);

  // Discard unwanted programs
  if (bProgram) {
    for (unsigned int i = 0; i < m_avFormat->nb_programs; ++i) {
      if (i != m_program)
        m_avFormat->programs[i]->discard = AVDISCARD_ALL;
    }
  }

  // Re-compute the overall file duration based on video and audio durations
  int64_t duration = INT64_MIN;
  int64_t st_duration = 0;
  int64_t start_time = INT64_MAX;
  int64_t st_start_time = 0;

  // Number of streams (either in file or in program)
  unsigned int nbIndex = bProgram ? m_avFormat->programs[m_program]->nb_stream_indexes : m_avFormat->nb_streams;

  // File has PGS streams
  bool bHasPGS = false;

  // add streams from selected program, or all streams if no program was selected
  for (unsigned int i = 0; i < nbIndex; ++i) {
    int streamIdx = bProgram ? m_avFormat->programs[m_program]->stream_index[i] : i;
    if (S_OK != AddStream(streamIdx))
      continue;

    AVStream *st = m_avFormat->streams[streamIdx];
    if (st->codec->codec_type == AVMEDIA_TYPE_VIDEO || st->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
      if (st->duration != AV_NOPTS_VALUE) {
        st_duration = av_rescale_q(st->duration, st->time_base, AV_RATIONAL_TIMEBASE);
        if (st_duration > duration)
          duration = st_duration;
      }

      if (st->start_time != AV_NOPTS_VALUE) {
        st_start_time = av_rescale_q(st->start_time, st->time_base, AV_RATIONAL_TIMEBASE);
        if (start_time != INT64_MAX && m_bMPEGTS && st->pts_wrap_bits < 60) {
          int64_t start = av_rescale_q(start_time, AV_RATIONAL_TIMEBASE, st->time_base);
          if (start < (3LL << (st->pts_wrap_bits - 3)) && st->start_time > (3LL << (st->pts_wrap_bits - 2))) {
            start_time = av_rescale_q(start + (1LL << st->pts_wrap_bits), st->time_base, AV_RATIONAL_TIMEBASE);
          } else if (st->start_time < (3LL << (st->pts_wrap_bits - 3)) && start > (3LL << (st->pts_wrap_bits - 2))) {
            st_start_time = av_rescale_q(st->start_time + (1LL << st->pts_wrap_bits), st->time_base, AV_RATIONAL_TIMEBASE);
          }
        }
        if (st_start_time < start_time)
          start_time = st_start_time;
      }
    }
    if (st->codec->codec_id == CODEC_ID_HDMV_PGS_SUBTITLE)
      bHasPGS = true;
  }

  if (duration != INT64_MIN) {
    m_avFormat->duration = duration;
  }
  if (start_time != INT64_MAX) {
    m_avFormat->start_time = start_time;
  }

  if(bHasPGS && m_pSettings->GetPGSForcedStream()) {
    CreatePGSForcedSubtitleStream();
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

  if(starttime == AV_NOPTS_VALUE) {
    if (m_avFormat->start_time != AV_NOPTS_VALUE) {
      starttime = av_rescale(m_avFormat->start_time, den, (int64_t)AV_TIME_BASE * num);
    } else {
      starttime = 0;
    }
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

  if(starttime == AV_NOPTS_VALUE) {
    if (m_avFormat->start_time != AV_NOPTS_VALUE) {
      starttime = av_rescale(m_avFormat->start_time, den, (int64_t)AV_TIME_BASE * num);
    } else {
      starttime = 0;
    }
  }

  int64_t pts = av_rescale(timestamp, den, (int64_t)num * DSHOW_TIME_BASE);
  if(starttime != 0) {
    pts += starttime;
  }

  return pts;
}

HRESULT CLAVFDemuxer::UpdateForcedSubtitleStream(unsigned audio_pid)
{
  if (!m_avFormat || audio_pid >= m_avFormat->nb_streams)
    return E_UNEXPECTED;

  stream *audiost = GetStreams(audio)->FindStream(audio_pid);
  if (!audiost)
    return E_FAIL;

  // Build CSubtitleSelector for this special case
  std::list<CSubtitleSelector> selectors;
  CSubtitleSelector selector;
  selector.audioLanguage = "*";
  selector.subtitleLanguage = audiost->language;
  selector.dwFlags = SUBTITLE_FLAG_PGS;

  selectors.push_back(selector);

  const stream *subst = SelectSubtitleStream(selectors, audiost->language);
  if (subst) {
    m_ForcedSubStream = subst->pid;

    CStreamList *streams = GetStreams(subpic);
    stream *forced = streams->FindStream(FORCED_SUBTITLE_PID);
    if (forced) {
      CMediaType mtype = forced->streamInfo->mtypes.back();
      forced->streamInfo->mtypes.pop_back();
      forced->language = audiost->language;

      SUBTITLEINFO *subInfo = (SUBTITLEINFO *)mtype.Format();
      strncpy_s(subInfo->IsoLang, audiost->language.c_str(), 3);
      subInfo->IsoLang[3] = 0;

      forced->streamInfo->mtypes.push_back(mtype);
    }
  }

  return subst ? S_OK : S_FALSE;
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

    int check_nb_f = m_avFormat->streams[check->pid]->codec_info_nb_frames;
    int best_nb_f  = m_avFormat->streams[best->pid]->codec_info_nb_frames;
    if (m_bRM && (check_nb_f > 0 && best_nb_f <= 0)) {
      best = check;
    } else if (!m_bRM || check_nb_f > 0) {
      if (checkPixels > bestPixels) {
        best = check;
      } else if (checkPixels == bestPixels) {
        int best_rate = m_avFormat->streams[best->pid]->codec->bit_rate;
        int check_rate = m_avFormat->streams[check->pid]->codec->bit_rate;
        if (best_rate && check_rate && check_rate > best_rate)
          best = check;
      }
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
  case CODEC_ID_MLP:
  case CODEC_ID_TTA:
  case CODEC_ID_MP4ALS:
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
  case CODEC_ID_WAVPACK:
  case CODEC_ID_EAC3:
    priority = 8;
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
      std::string checkLanguage = ProbeForISO6392(it->c_str());
      std::deque<stream>::iterator sit;
      for ( sit = streams->begin(); sit != streams->end(); ++sit ) {
        std::string language = sit->language;
        // check if the language matches
        if (language == checkLanguage) {
          checkedStreams.push_back(&*sit);
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

        int check_nb_f = new_stream->codec_info_nb_frames;
        int best_nb_f  = old_stream->codec_info_nb_frames;
        if (m_bRM && (check_nb_f > 0 && best_nb_f <= 0)) {
          best = *sit;
        } else if (!m_bRM || check_nb_f > 0) {
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
            } else if (new_priority == old_priority) {
              int best_rate = old_stream->codec->bit_rate;
              int check_rate = new_stream->codec->bit_rate;
              if (best_rate && check_rate && check_rate > best_rate)
                best = *sit;
            }
          }
        }
      }
    }
  }

  return best;
}

static inline bool does_language_match(std::string selector, std::string selectee)
{
  return (selector == "*" || selector == selectee);
}

// Select the best subtitle stream
const CBaseDemuxer::stream *CLAVFDemuxer::SelectSubtitleStream(std::list<CSubtitleSelector> subtitleSelectors, std::string audioLanguage)
{
  const stream *best = NULL;
  CStreamList *streams = GetStreams(subpic);

  std::deque<stream*> checkedStreams;

  std::list<CSubtitleSelector>::iterator it = subtitleSelectors.begin();
  for (it = subtitleSelectors.begin(); it != subtitleSelectors.end() && checkedStreams.empty(); it++) {

    if (!does_language_match(it->audioLanguage, audioLanguage))
      continue;

    if (it->subtitleLanguage == "off")
      break;

    std::deque<stream>::iterator sit;
    for (sit = streams->begin(); sit != streams->end(); sit++) {
      if (sit->pid == NO_SUBTITLE_PID)
        continue;
      if (sit->pid == FORCED_SUBTITLE_PID) {
        if ((it->dwFlags == 0 || it->dwFlags & SUBTITLE_FLAG_FORCED) && does_language_match(it->subtitleLanguage, audioLanguage))
          checkedStreams.push_back(&*sit);
        continue;
      }

      if (it->dwFlags == 0
        || ((it->dwFlags & SUBTITLE_FLAG_DEFAULT) && (m_avFormat->streams[sit->pid]->disposition & AV_DISPOSITION_DEFAULT))
        || ((it->dwFlags & SUBTITLE_FLAG_FORCED) && (m_avFormat->streams[sit->pid]->disposition & AV_DISPOSITION_FORCED))
        || ((it->dwFlags & SUBTITLE_FLAG_PGS) && (m_avFormat->streams[sit->pid]->codec->codec_id == CODEC_ID_HDMV_PGS_SUBTITLE))) {
        std::string streamLanguage = sit->language;
        if (does_language_match(it->subtitleLanguage, streamLanguage))
          checkedStreams.push_back(&*sit);
      }
    }
  }

  if (!checkedStreams.empty())
    best = streams->FindStream(checkedStreams.front()->pid);
  else
    best = streams->FindStream(NO_SUBTITLE_PID);

  return best;
}

STDMETHODIMP_(DWORD) CLAVFDemuxer::GetStreamFlags(DWORD dwStream)
{
  if (!m_avFormat || dwStream >= m_avFormat->nb_streams)
    return 0;

  DWORD dwFlags = 0;
  AVStream *st = m_avFormat->streams[dwStream];

  if (st->codec->codec_id == CODEC_ID_H264 && (m_bAVI || (m_bMatroska && (!st->codec->extradata_size || st->codec->extradata[0] != 1))))
    dwFlags |= LAV_STREAM_FLAG_H264_DTS;

  if (m_bMatroska && (st->codec->codec_id == CODEC_ID_RV30 || st->codec->codec_id == CODEC_ID_RV40))
    dwFlags |= LAV_STREAM_FLAG_RV34_MKV;

  return dwFlags;
}

STDMETHODIMP_(int) CLAVFDemuxer::GetPixelFormat(DWORD dwStream)
{
  if (!m_avFormat || dwStream >= m_avFormat->nb_streams)
    return PIX_FMT_NONE;

  return m_avFormat->streams[dwStream]->codec->pix_fmt;
}
