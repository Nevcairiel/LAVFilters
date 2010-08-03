#include "stdafx.h"
#include "LAVFSplitter.h"
#include "DSStreamInfo.h"
#include "LAVFOutputPin.h"
#include "utils.h"

#include <string>

// static constructor
CUnknown* WINAPI CLAVFSplitter::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr)
{
  return new CLAVFSplitter(pUnk, phr);
}

CLAVFSplitter::CLAVFSplitter(LPUNKNOWN pUnk, HRESULT* phr) 
  : CBaseFilter(NAME("lavf dshow source filter"), pUnk, this,  __uuidof(this))
  , m_rtDuration(0), m_rtStart(0), m_rtStop(0), m_rtCurrent(0)
  , m_dRate(1.0)
  , m_avFormat(NULL)
{
  av_register_all();

  if(phr) { *phr = S_OK; }
}

CLAVFSplitter::~CLAVFSplitter()
{
  CAutoLock cAutoLock(this);

  CAMThread::CallWorker(CMD_EXIT);
  CAMThread::Close();

  DeleteOutputs();

  CoTaskMemFree(m_fileName);
}

STDMETHODIMP CLAVFSplitter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = NULL;

  return 
    QI(IFileSourceFilter)
    QI(IMediaSeeking)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

// CBaseSplitter
int CLAVFSplitter::GetPinCount()
{
  CAutoLock lock(this);

  int count = m_pPins.size();
  return count;
}

CBasePin *CLAVFSplitter::GetPin(int n)
{
  CAutoLock lock(this);

  if (n < 0 ||n >= GetPinCount()) return NULL;
  return m_pPins[n];
}

CLAVFOutputPin *CLAVFSplitter::GetOutputPin(DWORD streamId)
{
  std::vector<CLAVFOutputPin *>::iterator it;
  for(it = m_pPins.begin(); it != m_pPins.end(); it++) {
    if ((*it)->GetStreamId() == streamId) {
      return *it;
    }
  }
  return NULL;
}

// IFileSourceFilter
STDMETHODIMP CLAVFSplitter::Load(LPCOLESTR pszFileName, const AM_MEDIA_TYPE * pmt)
{
  CheckPointer(pszFileName, E_POINTER);

  // Copy the filename, just in case
  int strlen = wcslen(pszFileName) + 1;
  m_fileName = (WCHAR *)CoTaskMemAlloc(sizeof(WCHAR) * (strlen + 1));
  wcsncpy_s(m_fileName, strlen, pszFileName, _TRUNCATE);

  HRESULT hr = S_OK;

  if(FAILED(hr = DeleteOutputs()) || FAILED(hr = CreateOutputs()))
  {
    m_fileName = L"";
  }

  return hr;
}

// Get the currently loaded file
STDMETHODIMP CLAVFSplitter::GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt)
{
  CheckPointer(ppszFileName, E_POINTER);

  int strlen = wcslen(m_fileName) + 1;
  *ppszFileName = (LPOLESTR)CoTaskMemAlloc(sizeof(WCHAR) * (strlen + 1));

  if(!(*ppszFileName))
    return E_OUTOFMEMORY;

  wcsncpy_s(*ppszFileName, strlen, m_fileName, _TRUNCATE);
  return S_OK;
}

int CLAVFSplitter::GetStreamLength()
{
  if (m_avFormat->duration == (int64_t)AV_NOPTS_VALUE || m_avFormat->duration < 0LL) {
    // no duration is available for us
    // try to calculate it
    int iLength = 0;
    if (m_rtCurrent != Packet::INVALID_TIME && m_avFormat->file_size > 0 && m_avFormat->pb && m_avFormat->pb->pos > 0) {
      iLength = (int)(((m_rtCurrent * m_avFormat->file_size) / m_avFormat->pb->pos) / 1000) & 0xFFFFFFFF;
    }
    return iLength;
  }
  return (int)(m_avFormat->duration / (AV_TIME_BASE / 1000));
}

// Pin creation
STDMETHODIMP CLAVFSplitter::CreateOutputs()
{
  CAutoLock lock(this);

  int ret; // return code from avformat functions

  // Conver the filename from wchar to char for avformat
  char fileName[1024];
  wcstombs_s(NULL, fileName, 1024, m_fileName, _TRUNCATE);

  ret = av_open_input_file(&m_avFormat, fileName, NULL, FFMPEG_FILE_BUFFER_SIZE, NULL);
  if (ret < 0) {
    DbgLog((LOG_ERROR, 0, TEXT("av_open_input_file failed")));
    goto fail;
  }

  ret = av_find_stream_info(m_avFormat);
  if (ret < 0) {
    DbgLog((LOG_ERROR, 0, TEXT("av_find_stream_info failed")));
    goto fail;
  }

  m_bMatroska = (_stricmp(m_avFormat->iformat->name, "matroska") == 0);
  m_bAVI = (_stricmp(m_avFormat->iformat->name, "avi") == 0);

  // try to use non-blocking methods
  m_avFormat->flags |= AVFMT_FLAG_NONBLOCK;

  m_rtNewStart = m_rtStart = m_rtCurrent = 0;
  m_rtNewStop = m_rtStop = m_rtDuration = (REFERENCE_TIME)DVD_MSEC_TO_TIME(GetStreamLength());

  // TODO Programms support
  ASSERT(m_avFormat->nb_programs == 0);

  for(int i = 0; i < countof(m_streams); i++) {
    m_streams[i].Clear();
  }

  const char* container = m_avFormat->iformat->name;
  for(unsigned int streamId = 0; streamId < m_avFormat->nb_streams; streamId++)
  {
    AVStream* pStream = m_avFormat->streams[streamId];
    stream s;
    s.pid = streamId;
    s.streamInfo = new CDSStreamInfo(pStream, container);

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
      delete s.streamInfo;
      break;
    }
  }

  HRESULT hr = S_OK;

  // Try to create pins
  for(int i = 0; i < countof(m_streams); i++) {
    std::list<stream>::iterator it;
    for ( it = m_streams[i].begin(); it != m_streams[i].end(); it++ ) {
      const WCHAR* name = CStreamList::ToString(i);

      std::vector<CMediaType> mts;
      mts.push_back(it->streamInfo->mtype);

      CLAVFOutputPin* pPin = new CLAVFOutputPin(mts, name, this, this, &hr);
      // We need to addref this so IUnknown can do proper ref counting here
      pPin->AddRef();
      pPin->SetStreamId(it->pid);
      m_pPins.push_back(pPin);
    }
  }

  return S_OK;
fail:
  // Cleanup
  if (m_avFormat) {
    av_close_input_file(m_avFormat);
    m_avFormat = NULL;
  }
  return E_FAIL;
}

STDMETHODIMP CLAVFSplitter::DeleteOutputs()
{
  CAutoLock lock(this);
  if(m_State != State_Stopped) return VFW_E_NOT_STOPPED;

  if(m_avFormat) {
    av_close_input_file(m_avFormat);
    m_avFormat = NULL;
  }

  // Release pins
  std::vector<CLAVFOutputPin *>::iterator it;
  for(it = m_pPins.begin(); it != m_pPins.end(); it++) {
    if(IPin* pPinTo = (*it)->GetConnected()) pPinTo->Disconnect();
    (*it)->Disconnect();
    (*it)->Release();
  }
  m_pPins.clear();

  return S_OK;
}

bool CLAVFSplitter::IsAnyPinDrying()
{
  // TODO
  return true;
}

// Worker Thread
DWORD CLAVFSplitter::ThreadProc()
{
  m_fFlushing = false;
  m_eEndFlush.Set();

  for(DWORD cmd = (DWORD)-1; ; cmd = GetRequest())
  {
    if(cmd == CMD_EXIT)
    {
      m_hThread = NULL;
      Reply(S_OK);
      return 0;
    }

    SetThreadPriority(m_hThread, THREAD_PRIORITY_NORMAL);

    m_rtStart = m_rtNewStart;
    m_rtStop = m_rtNewStop;

    DemuxSeek(m_rtStart);

    if(cmd != (DWORD)-1)
      Reply(S_OK);

    // Wait for the end of any flush
    m_eEndFlush.Wait();

    if(!m_fFlushing) {
      std::vector<CLAVFOutputPin *>::iterator it;
      for(it = m_pPins.begin(); it != m_pPins.end(); it++) {
        if ((*it)->IsConnected()) {
          (*it)->DeliverNewSegment(m_rtStart, m_rtStop, m_dRate);
        }
      }
    }

    m_bDiscontinuitySent.clear();

    HRESULT hr = S_OK;
    while(SUCCEEDED(hr) && !CheckRequest(&cmd)) {
      hr = DemuxNextPacket();
    }

    // If we didnt exit by request, deliver end-of-stream
    if(!CheckRequest(&cmd)) {
      std::vector<CLAVFOutputPin *>::iterator it;
      for(it = m_pPins.begin(); it != m_pPins.end(); it++) {
        (*it)->DeliverEndOfStream();
      }
    }

  }
  ASSERT(0); // we should only exit via CMD_EXIT

  m_hThread = NULL;
  return 0;
}

// Converts the lavf pts timestamp to a DShow REFERENCE_TIME
REFERENCE_TIME CLAVFSplitter::ConvertTimestamp(int64_t pts, int den, int num)
{
  if (pts == (int64_t)AV_NOPTS_VALUE) {
    return Packet::INVALID_TIME;
  }

  // do calculations in double-precision floats as they can easily overflow otherwise
  // we don't care for having a completly exact timestamp anyway
  double timestamp = (double)pts * num  / den;
  double starttime = 0.0f;

  if (m_avFormat->start_time != (int64_t)AV_NOPTS_VALUE) {
    starttime = (double)m_avFormat->start_time / AV_TIME_BASE;
  }

  if(timestamp > starttime) {
    timestamp -= starttime;
  } else if( timestamp + 0.1f > starttime ) {
    timestamp = 0;
  }

  return (REFERENCE_TIME)(timestamp * DVD_TIME_BASE);
}

HRESULT CLAVFSplitter::DemuxSeek(REFERENCE_TIME rtStart)
{
  int time = DVD_TIME_TO_MSEC(rtStart);
  if(time < 0) { time = 0; }

  __int64 seek_pts = (__int64)time * (AV_TIME_BASE / 1000);
  if (m_avFormat->start_time != (int64_t)AV_NOPTS_VALUE) {
    seek_pts += m_avFormat->start_time;
  }

  int ret = avformat_seek_file(m_avFormat, -1, _I64_MIN, seek_pts, _I64_MAX, 0);
  //int ret = av_seek_frame(m_avFormat, -1, seek_pts, 0);

  return S_OK;
}

HRESULT CLAVFSplitter::DemuxNextPacket()
{
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
  } else if (pkt.size < 0 || pkt.stream_index >= MAX_STREAMS) {
    // XXX, in some cases ffmpeg returns a negative packet size
    if(m_avFormat->pb && !m_avFormat->pb->eof_reached) {
      bReturnEmpty = true;
    }
    av_free_packet(&pkt);
  } else {
    AVStream *stream = m_avFormat->streams[pkt.stream_index];
    pPacket = new Packet();

    // libavformat sometimes bugs and sends dts/pts 0 instead of invalid.. correct this
    if(pkt.dts == 0) {
      pkt.dts = AV_NOPTS_VALUE;
    }
    if(pkt.pts == 0) {
      pkt.pts = AV_NOPTS_VALUE;
    }

    // we need to get duration slightly different for matroska embedded text subtitels
    if(m_bMatroska && stream->codec->codec_id == CODEC_ID_TEXT && pkt.convergence_duration != 0) {
      pkt.duration = (int)pkt.convergence_duration;
    }

    if(m_bAVI && stream->codec && stream->codec->codec_type == CODEC_TYPE_VIDEO)
    {
      // AVI's always have borked pts, specially if m_pFormatContext->flags includes
      // AVFMT_FLAG_GENPTS so always use dts
      pkt.pts = AV_NOPTS_VALUE;
    }

    if(pkt.data) {
      pPacket->SetData(pkt.data, pkt.size);
    }

    pPacket->StreamId = (DWORD)pkt.stream_index;

    REFERENCE_TIME pts = (REFERENCE_TIME)ConvertTimestamp(pkt.pts, stream->time_base.den, stream->time_base.num);
    REFERENCE_TIME dts = (REFERENCE_TIME)ConvertTimestamp(pkt.dts, stream->time_base.den, stream->time_base.num);
    REFERENCE_TIME duration =  (REFERENCE_TIME)DVD_SEC_TO_TIME((double)pkt.duration * stream->time_base.num / stream->time_base.den);

    REFERENCE_TIME rt = m_rtCurrent;
    // Try the different times set, pts first, dts when pts is not valid
    if (pts != Packet::INVALID_TIME) {
      rt = pts;
    } else if (dts != Packet::INVALID_TIME) {
      rt = dts;
    }

    pPacket->rtStart = rt;
    pPacket->rtStop = rt + ((duration > 0) ? duration : 1);

    pPacket->bSyncPoint = (duration > 0) ? 1 : 0;
    pPacket->bAppendable = !pPacket->bSyncPoint;

    av_free_packet(&pkt);
  }

  if (bReturnEmpty && !pPacket) {
    return S_FALSE;
  }
  if (!pPacket) {
    return E_FAIL;
  }

  return DeliverPacket(pPacket);
}

HRESULT CLAVFSplitter::DeliverPacket(Packet *pPacket)
{
  HRESULT hr = S_FALSE;

  CLAVFOutputPin* pPin = GetOutputPin(pPacket->StreamId);
  if(!pPin || !pPin->IsConnected()) {
    delete pPacket;
    return S_FALSE;
  }

  if(pPacket->rtStart != Packet::INVALID_TIME) {
    m_rtCurrent = pPacket->rtStart;

    pPacket->rtStart -= m_rtStart;
    pPacket->rtStop -= m_rtStart;

    ASSERT(pPacket->rtStart <= pPacket->rtStop);
  }

  if(m_bDiscontinuitySent.find(pPacket->StreamId) == m_bDiscontinuitySent.end()) {
    pPacket->bDiscontinuity = true;
  }

  BOOL bDiscontinuity = pPacket->bDiscontinuity; 

  hr = pPin->QueuePacket(pPacket);

  // TODO track active pins

  if(bDiscontinuity) {
    m_bDiscontinuitySent.insert(pPacket->StreamId);
  }

  return hr;
}

// State Control
STDMETHODIMP CLAVFSplitter::Stop()
{
  CAutoLock cAutoLock(this);

  DeliverBeginFlush();
  CallWorker(CMD_EXIT);
  DeliverEndFlush();

  HRESULT hr;
  if(FAILED(hr = __super::Stop())) {
    return hr;
  }

  return S_OK;
}

STDMETHODIMP CLAVFSplitter::Pause()
{
  CAutoLock cAutoLock(this);

  FILTER_STATE fs = m_State;

  HRESULT hr;
  if(FAILED(hr = __super::Pause())) {
    return hr;
  }

  // The filter graph will set us to pause before running
  // So if we were stopped before, create the thread
  // Note that the splitter will always be running,
  // and even in pause mode fill up the buffers
  if(fs == State_Stopped) {
    Create();
  }

  return S_OK;
}

STDMETHODIMP CLAVFSplitter::Run(REFERENCE_TIME tStart)
{
  CAutoLock cAutoLock(this);

  HRESULT hr;
  if(FAILED(hr = __super::Run(tStart))) {
    return hr;
  }

  return S_OK;
}

// Flushing
void CLAVFSplitter::DeliverBeginFlush()
{
  m_fFlushing = true;

  // flush all pins
  std::vector<CLAVFOutputPin *>::iterator it;
  for(it = m_pPins.begin(); it != m_pPins.end(); it++) {
    (*it)->DeliverBeginFlush();
  }
}

void CLAVFSplitter::DeliverEndFlush()
{
  // flush all pins
  std::vector<CLAVFOutputPin *>::iterator it;
  for(it = m_pPins.begin(); it != m_pPins.end(); it++) {
    (*it)->DeliverEndFlush();
  }

  m_fFlushing = false;
  m_eEndFlush.Set();
}

// IMediaSeeking
STDMETHODIMP CLAVFSplitter::GetCapabilities(DWORD* pCapabilities)
{
  CheckPointer(pCapabilities, E_POINTER);

  *pCapabilities =
    AM_SEEKING_CanGetStopPos   |
    AM_SEEKING_CanGetDuration  |
    AM_SEEKING_CanSeekAbsolute |
    AM_SEEKING_CanSeekForwards |
    AM_SEEKING_CanSeekBackwards;

  return S_OK;
}

STDMETHODIMP CLAVFSplitter::CheckCapabilities(DWORD* pCapabilities)
{
  CheckPointer(pCapabilities, E_POINTER);
  // capabilities is empty, all is good
  if(*pCapabilities == 0) return S_OK;
  // read caps
  DWORD caps;
  GetCapabilities(&caps);

  // Store the caps that we wanted
  DWORD wantCaps = *pCapabilities;
  // Update pCapabilities with what we have
  *pCapabilities = caps & wantCaps;

  // if nothing matches, its a disaster!
  if(*pCapabilities == 0) return E_FAIL;
  // if all matches, its all good
  if(*pCapabilities == wantCaps) return S_OK;
  // otherwise, a partial match
  return S_FALSE;
}

STDMETHODIMP CLAVFSplitter::IsFormatSupported(const GUID* pFormat) {return !pFormat ? E_POINTER : *pFormat == TIME_FORMAT_MEDIA_TIME ? S_OK : S_FALSE;}
STDMETHODIMP CLAVFSplitter::QueryPreferredFormat(GUID* pFormat) {return GetTimeFormat(pFormat);}
STDMETHODIMP CLAVFSplitter::GetTimeFormat(GUID* pFormat) {return pFormat ? *pFormat = TIME_FORMAT_MEDIA_TIME, S_OK : E_POINTER;}
STDMETHODIMP CLAVFSplitter::IsUsingTimeFormat(const GUID* pFormat) {return IsFormatSupported(pFormat);}
STDMETHODIMP CLAVFSplitter::SetTimeFormat(const GUID* pFormat) {return S_OK == IsFormatSupported(pFormat) ? S_OK : E_INVALIDARG;}
STDMETHODIMP CLAVFSplitter::GetDuration(LONGLONG* pDuration) {CheckPointer(pDuration, E_POINTER); *pDuration = m_rtDuration; return S_OK;}
STDMETHODIMP CLAVFSplitter::GetStopPosition(LONGLONG* pStop) {return GetDuration(pStop);}
STDMETHODIMP CLAVFSplitter::GetCurrentPosition(LONGLONG* pCurrent) {return E_NOTIMPL;}
STDMETHODIMP CLAVFSplitter::ConvertTimeFormat(LONGLONG* pTarget, const GUID* pTargetFormat, LONGLONG Source, const GUID* pSourceFormat) {return E_NOTIMPL;}
STDMETHODIMP CLAVFSplitter::SetPositions(LONGLONG* pCurrent, DWORD dwCurrentFlags, LONGLONG* pStop, DWORD dwStopFlags)
{
  CAutoLock cAutoLock(this);

  if(!pCurrent && !pStop
    || (dwCurrentFlags&AM_SEEKING_PositioningBitsMask) == AM_SEEKING_NoPositioning 
    && (dwStopFlags&AM_SEEKING_PositioningBitsMask) == AM_SEEKING_NoPositioning) {
      return S_OK;
  }

  REFERENCE_TIME
    rtCurrent = m_rtCurrent,
    rtStop = m_rtStop;

  if(pCurrent) {
    switch(dwCurrentFlags&AM_SEEKING_PositioningBitsMask)
    {
    case AM_SEEKING_NoPositioning: break;
    case AM_SEEKING_AbsolutePositioning: rtCurrent = *pCurrent; break;
    case AM_SEEKING_RelativePositioning: rtCurrent = rtCurrent + *pCurrent; break;
    case AM_SEEKING_IncrementalPositioning: rtCurrent = rtCurrent + *pCurrent; break;
    }
  }

  if(pStop){
    switch(dwStopFlags&AM_SEEKING_PositioningBitsMask)
    {
    case AM_SEEKING_NoPositioning: break;
    case AM_SEEKING_AbsolutePositioning: rtStop = *pStop; break;
    case AM_SEEKING_RelativePositioning: rtStop += *pStop; break;
    case AM_SEEKING_IncrementalPositioning: rtStop = rtCurrent + *pStop; break;
    }
  }

  if(m_rtCurrent == rtCurrent && m_rtStop == rtStop) {
    return S_OK;
  }

  m_rtNewStart = m_rtCurrent = rtCurrent;
  m_rtNewStop = rtStop;

  if(ThreadExists())
  {
    DeliverBeginFlush();
    CallWorker(CMD_SEEK);
    DeliverEndFlush();
  }

  return S_OK;
}
STDMETHODIMP CLAVFSplitter::GetPositions(LONGLONG* pCurrent, LONGLONG* pStop)
{
  if(pCurrent) *pCurrent = m_rtCurrent;
  if(pStop) *pStop = m_rtStop;
  return S_OK;
}
STDMETHODIMP CLAVFSplitter::GetAvailable(LONGLONG* pEarliest, LONGLONG* pLatest)
{
  if(pEarliest) *pEarliest = 0;
  return GetDuration(pLatest);
}
STDMETHODIMP CLAVFSplitter::SetRate(double dRate) {return dRate > 0 ? m_dRate = dRate, S_OK : E_INVALIDARG;}
STDMETHODIMP CLAVFSplitter::GetRate(double* pdRate) {return pdRate ? *pdRate = m_dRate, S_OK : E_POINTER;}
STDMETHODIMP CLAVFSplitter::GetPreroll(LONGLONG* pllPreroll) {return pllPreroll ? *pllPreroll = 0, S_OK : E_POINTER;}

// CStreamList
const WCHAR* CLAVFSplitter::CStreamList::ToString(int type)
{
  return 
    type == video ? L"Video" :
    type == audio ? L"Audio" :
    type == subpic ? L"Subtitle" :
    L"Unknown";
}

const CLAVFSplitter::stream* CLAVFSplitter::CStreamList::FindStream(DWORD pid)
{
  std::list<stream>::iterator it;
  for ( it = begin(); it != end(); it++ ) {
    if ((*it).pid == pid) {
      return &(*it);
    }
  }

  return NULL;
}

void CLAVFSplitter::CStreamList::Clear()
{
  std::list<stream>::iterator it;
  for ( it = begin(); it != end(); it++ ) {
    delete (*it).streamInfo;
  }
  __super::clear();
}
