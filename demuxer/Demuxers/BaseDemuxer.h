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

#pragma once

#include <deque>

#include "StreamInfo.h"

#define DSHOW_TIME_BASE 10000000        // DirectShow times are in 100ns units
#define NO_SUBTITLE_PID DWORD_MAX
#define FORCED_SUBTITLE_PID (NO_SUBTITLE_PID - 1)

#define FORCED_SUB_STRING L"Forced Subtitles (auto)"

struct ILAVFSettingsInternal;

// Data Packet for queue storage
class Packet
{
public:
  static const REFERENCE_TIME INVALID_TIME = _I64_MIN;

  DWORD StreamId;
  BOOL bDiscontinuity, bSyncPoint, bAppendable;
  REFERENCE_TIME rtStart, rtStop;
  LONGLONG bPosition;
  AM_MEDIA_TYPE* pmt;

#define LAV_PACKET_PARSED           0x0001
#define LAV_PACKET_MOV_TEXT         0x0002
#define LAV_PACKET_FORCED_SUBTITLE  0x0004
#define LAV_PACKET_H264_ANNEXB      0x0008
  DWORD dwFlags;

  Packet() { pmt = NULL; m_pbData = NULL; bDiscontinuity = bSyncPoint = bAppendable = FALSE; rtStart = rtStop = INVALID_TIME; m_sSize = 0; m_sBlockSize = 0; bPosition = -1; dwFlags = 0; }
  ~Packet() { DeleteMediaType(pmt); SAFE_CO_FREE(m_pbData); }

  // Getter
  size_t GetDataSize() const { return m_sSize; }
  BYTE *GetData() { return m_pbData; }
  BYTE GetAt(DWORD pos) const { return m_pbData[pos]; }
  bool IsEmpty() const { return m_sSize == 0; }

  // Setter
  void SetDataSize(size_t len) { m_sSize = len; if (m_sSize > m_sBlockSize || !m_pbData) { m_pbData = (BYTE *)CoTaskMemRealloc(m_pbData, m_sSize + FF_INPUT_BUFFER_PADDING_SIZE); m_sBlockSize = m_sSize; } memset(m_pbData+m_sSize, 0, FF_INPUT_BUFFER_PADDING_SIZE);}
  void SetData(const void* ptr, size_t len) { SetDataSize(len); memcpy(m_pbData, ptr, len); }
  void Clear() { m_sSize = m_sBlockSize = 0; SAFE_CO_FREE(m_pbData); }

  // Append the data of the package to our data buffer
  void Append(Packet *ptr) {
    AppendData(ptr->GetData(), ptr->GetDataSize());
  }

  void AppendData(const void* ptr, size_t len) {
    size_t prevSize = m_sSize;
    SetDataSize(m_sSize + len);
    memcpy(m_pbData+prevSize, ptr, len);
  }

  // Remove count bytes from position index
  void RemoveHead(size_t count) {
    count = min(count, m_sSize);
    memmove(m_pbData, m_pbData+count, m_sSize-count);
    SetDataSize(m_sSize - count);
  }

private:
  size_t m_sSize;
  size_t m_sBlockSize;
  BYTE *m_pbData;
};

typedef struct CSubtitleSelector {
  std::string audioLanguage;
  std::string subtitleLanguage;

#define SUBTITLE_FLAG_DEFAULT 0x1
#define SUBTITLE_FLAG_FORCED  0x2
#define SUBTITLE_FLAG_PGS     0x4
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
    struct stream() { streamInfo = NULL; pid = 0; }
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
  virtual STDMETHODIMP AbortOpening(int mode = 1) { return E_NOTIMPL; }
  // Get Duration
  virtual REFERENCE_TIME GetDuration() const = 0;
  // Get the next packet from the file
  virtual STDMETHODIMP GetNextPacket(Packet **ppPacket) = 0;
  // Seek to the given position
  virtual STDMETHODIMP Seek(REFERENCE_TIME rTime) = 0;
  // Get the container format
  virtual const char *GetContainerFormat() const = 0;
  // Create Stream Description
  virtual HRESULT StreamInfo(const CBaseDemuxer::stream &s, LCID *plcid, WCHAR **ppszName) const = 0;

  // Select the active title
  virtual STDMETHODIMP SetTitle(uint32_t idx) { return E_NOTIMPL; }
  // Get Title Info
  virtual STDMETHODIMP GetTitleInfo(uint32_t idx, REFERENCE_TIME *rtDuration, WCHAR **ppszName) { return E_NOTIMPL; }
  // Title count
  virtual STDMETHODIMP GetNumTitles(uint32_t *count) { return E_NOTIMPL; }
  
  // Set the currently active stream of one type
  // The demuxers can use this to filter packets before returning back to the caller on GetNextPacket
  // This functionality is optional however, so the caller should not rely on only receiving packets
  // for active streams.
  virtual HRESULT SetActiveStream(StreamType type, int pid) { m_dActiveStreams[type] = pid; return S_OK; }

  // Called when the settings of the splitter change
  virtual void SettingsChanged(ILAVFSettingsInternal *pSettings) {};

  virtual STDMETHODIMP_(DWORD) GetStreamFlags(DWORD dwStream) { return 0; }
  virtual STDMETHODIMP_(int) GetPixelFormat(DWORD dwStream) { return PIX_FMT_NONE; }

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


protected:
  CBaseDemuxer(LPCTSTR pName, CCritSec *pLock);
  void CreateNoSubtitleStream();
  void CreatePGSForcedSubtitleStream();

public:
  // Get the StreamList of the correponding type
  virtual CStreamList *GetStreams(StreamType type) { return &m_streams[type]; }

  
  // Select the best video stream
  virtual const stream* SelectVideoStream() = 0;
  
  // Select the best audio stream
  virtual const stream* SelectAudioStream(std::list<std::string> prefLanguages) = 0;

  // Select the best subtitle stream
  virtual const stream* SelectSubtitleStream(std::list<CSubtitleSelector> subtitleSelectors, std::string audioLanguage) = 0;

protected:
  CCritSec *m_pLock;
  CStreamList m_streams[unknown];
  int m_dActiveStreams[unknown];
};
