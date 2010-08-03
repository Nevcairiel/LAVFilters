#ifndef __FF_SPLITTER_H__
#define __FF_SPLITTER_H__

#include <string>
#include <list>
#include <set>
#include <vector>
#include "PacketQueue.h"

#define FFMPEG_FILE_BUFFER_SIZE   32768 // default reading size for ffmpeg

#define DVD_TIME_BASE 10000000        // DirectShow times are in 100ns units
#define DVD_SEC_TO_TIME(x)  ((double)(x) * DVD_TIME_BASE)
#define DVD_TIME_TO_MSEC(x) ((int)((double)(x) * 1000 / DVD_TIME_BASE))

class CDSStreamInfo;
class CLAVFOutputPin;

[uuid("B98D13E7-55DB-4385-A33D-09FD1BA26338")]
class CLAVFSplitter 
  : public CBaseFilter
  , public CCritSec
  , protected CAMThread
  , public IFileSourceFilter
{
public:
  // constructor method used by class factory
  static CUnknown* WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT* phr);

  // IUnknown
  DECLARE_IUNKNOWN;
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  // CBaseFilter methods
  int GetPinCount();
  CBasePin *GetPin(int n);

  STDMETHODIMP Stop();
  STDMETHODIMP Pause();
  STDMETHODIMP Run(REFERENCE_TIME tStart);

  // IFileSourceFilter
  STDMETHODIMP Load(LPCOLESTR pszFileName, const AM_MEDIA_TYPE * pmt);
  STDMETHODIMP GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt);

  bool IsAnyPinDrying();
protected:
  // CAMThread
  enum {CMD_EXIT, CMD_SEEK};
  DWORD ThreadProc();

  HRESULT DemuxSeek(REFERENCE_TIME rtStart);
  HRESULT DemuxNextPacket();
  REFERENCE_TIME ConvertTimestamp(int64_t pts, int den, int num);

  HRESULT DeliverPacket(Packet *pPacket);

  void DeliverBeginFlush();
  void DeliverEndFlush();

  STDMETHODIMP CreateOutputs();
  STDMETHODIMP DeleteOutputs();

public:
  struct stream {
    CDSStreamInfo *streamInfo;
    DWORD pid;
    struct stream() { streamInfo = NULL; pid = 0; }
    operator DWORD() const { return pid; }
    bool operator == (const struct stream& s) const { return (DWORD)*this == (DWORD)s; }
  };

  enum StreamType {video, audio, subpic, unknown};
  class CStreamList : public std::list<stream> 
  {
  public:
    static const WCHAR* ToString(int type);
    const stream* FindStream(DWORD pid);
    void Clear();
  } m_streams[unknown];

  CLAVFOutputPin *GetOutputPin(DWORD streamId);

private:
  // construct only via class factory
  CLAVFSplitter(LPUNKNOWN pUnk, HRESULT* phr);
  ~CLAVFSplitter();

private:
  std::vector<CLAVFOutputPin *> m_pPins;
  std::set<DWORD> m_bDiscontinuitySent;

  WCHAR *m_fileName;

  AVFormatContext *m_avFormat;

  bool m_bMatroska;
  bool m_bAVI;

  // Times
  REFERENCE_TIME m_rtDuration; // derived filter should set this at the end of CreateOutputs
  REFERENCE_TIME m_rtStart, m_rtStop, m_rtCurrent, m_rtNewStart, m_rtNewStop;
  double m_dRate;

  // flushing
  bool m_fFlushing;
  CAMEvent m_eEndFlush;
};

#endif
