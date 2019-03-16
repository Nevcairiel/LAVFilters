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

#include <deque>

#include "StreamInfo.h"
#include "Packet.h"
#include "IMediaSideDataFFmpeg.h"

#define DSHOW_TIME_BASE 10000000        // DirectShow times are in 100ns units
#define NO_SUBTITLE_PID DWORD_MAX
#define FORCED_SUBTITLE_PID (NO_SUBTITLE_PID - 1)

#define NO_SUB_STRING L"No subtitles"
#define FORCED_SUB_STRING L"Forced Subtitles (auto)"

struct ILAVFSettingsInternal;

typedef struct CSubtitleSelector {
  std::string audioLanguage;
  std::string subtitleLanguage;
  std::string subtitleTrackName;

#define SUBTITLE_FLAG_DEFAULT  0x0001
#define SUBTITLE_FLAG_FORCED   0x0002
#define SUBTITLE_FLAG_NORMAL   0x0004
#define SUBTITLE_FLAG_IMPAIRED 0x0008
#define SUBTITLE_FLAG_VIRTUAL  0x0080

// Values above 0xFF are special
#define SUBTITLE_FLAG_PGS      0x8000
  DWORD dwFlags;
} CSubtitleSelector;

class CBaseDemuxer : public CUnknown
{
public:
  enum StreamType {video, audio, subpic, unknown};

  typedef struct stream {
    CStreamInfo *streamInfo;
    DWORD pid;
    std::string language;
    std::string trackName;
    LCID lcid;
    MediaSideDataFFMpeg SideData;
    struct stream() { streamInfo = nullptr; pid = 0; lcid = 0; memset(&SideData, 0, sizeof(SideData)); }
    operator DWORD() const { return pid; }
    bool operator == (const struct stream& s) const { return (DWORD)*this == (DWORD)s; }
  } stream;

  DECLARE_IUNKNOWN

  // Demuxing Methods (pure virtual)

  // Open the file
  virtual STDMETHODIMP Open(LPCOLESTR pszFileName) = 0;
  // Start the demuxer
  virtual STDMETHODIMP Start() { return E_NOTIMPL; }
  // Abort opening the file
  virtual STDMETHODIMP AbortOpening(int mode = 1, int timeout = 0) { return E_NOTIMPL; }
  // Get Duration
  virtual REFERENCE_TIME GetDuration() const = 0;
  // Get the next packet from the file
  virtual STDMETHODIMP GetNextPacket(Packet **ppPacket) = 0;
  // Seek to the given position
  virtual STDMETHODIMP Seek(REFERENCE_TIME rTime) = 0;
  // Reset the demuxer, start reading at position 0
  virtual STDMETHODIMP Reset() = 0;
  // Get the container format
  virtual const char *GetContainerFormat() const = 0;
  // Get Container Flags
#define LAVFMT_TS_DISCONT               0x0001
#define LAVFMT_TS_DISCONT_NO_DOWNSTREAM 0x0002
  virtual DWORD GetContainerFlags() { return 0; }
  // Select the active title
  virtual STDMETHODIMP SetTitle(int idx) { return E_NOTIMPL; }
  // Query the active title
  virtual STDMETHODIMP_(int) GetTitle() { return 0; }
  // Get Title Info
  virtual STDMETHODIMP GetTitleInfo(int idx, REFERENCE_TIME *rtDuration, WCHAR **ppszName) { return E_NOTIMPL; }
  // Title count
  virtual STDMETHODIMP_(int) GetNumTitles() { return 0; }
  
  // Set the currently active stream of one type
  // The demuxers can use this to filter packets before returning back to the caller on GetNextPacket
  // This functionality is optional however, so the caller should not rely on only receiving packets
  // for active streams.
  virtual HRESULT SetActiveStream(StreamType type, int pid) { m_dActiveStreams[type] = pid; return S_OK; }

  // Called when the settings of the splitter change
  virtual void SettingsChanged(ILAVFSettingsInternal *pSettings) {};

  virtual STDMETHODIMP_(DWORD) GetStreamFlags(DWORD dwStream) { return 0; }
  virtual STDMETHODIMP_(int) GetPixelFormat(DWORD dwStream) { return AV_PIX_FMT_NONE; }
  virtual STDMETHODIMP_(int) GetHasBFrames(DWORD dwStream) { return -1; }
  virtual STDMETHODIMP GetSideData(DWORD dwStream, GUID guidType, const BYTE **pData, size_t *pSize) { return E_NOTIMPL; }

public:
  class CStreamList : public std::deque<stream>
  {
  public:
    ~CStreamList() { Clear(); }
    static const WCHAR* ToStringW(int type);
    static const CHAR* ToString(int type);
    stream* FindStream(DWORD pid);
    void Clear();
  };

  stream* FindStream(DWORD pid);

protected:
  CBaseDemuxer(LPCTSTR pName, CCritSec *pLock);
  void CreateNoSubtitleStream();
  void CreatePGSForcedSubtitleStream();

public:
  // Get the StreamList of the corresponding type
  virtual CStreamList *GetStreams(StreamType type) { return &m_streams[type]; }

  
  // Select the best video stream
  virtual const stream* SelectVideoStream() = 0;
  
  // Select the best audio stream
  virtual const stream* SelectAudioStream(std::list<std::string> prefLanguages) = 0;

  // Select the best subtitle stream
  virtual const stream* SelectSubtitleStream(std::list<CSubtitleSelector> subtitleSelectors, std::string audioLanguage) = 0;

protected:
  CCritSec *m_pLock = nullptr;
  CStreamList m_streams[unknown];
  int m_dActiveStreams[unknown];
};
