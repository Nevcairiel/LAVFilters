#include "stdafx.h"
#include "FFSplitter.h"
#include "utils.h"

#include <string>

// static constructor
CUnknown* WINAPI CFFSplitter::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr)
{
  return new CFFSplitter(pUnk, phr);
}

CFFSplitter::CFFSplitter(LPUNKNOWN pUnk, HRESULT* phr) 
  : CBaseFilter(NAME("FFSplitter Source Filter"), pUnk, this,  __uuidof(this))
  , m_avFormat(NULL)
{
  av_register_all();
}

CFFSplitter::~CFFSplitter()
{
  CAutoLock cAutoLock(this);

  CAMThread::CallWorker(CMD_EXIT);
  CAMThread::Close();

  CoTaskMemFree(m_fileName);
}

STDMETHODIMP CFFSplitter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = NULL;

  return 
    QI(IFileSourceFilter)
    //QI(IMediaSeeking)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

// CBaseSplitter
int CFFSplitter::GetPinCount()
{
  return 0;
}

CBasePin *CFFSplitter::GetPin(int n)
{
  return NULL;
}

// IFileSourceFilter
STDMETHODIMP CFFSplitter::Load(LPCOLESTR pszFileName, const AM_MEDIA_TYPE * pmt)
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
STDMETHODIMP CFFSplitter::GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt)
{
  CheckPointer(ppszFileName, E_POINTER);
  
  int strlen = wcslen(m_fileName) + 1;
  *ppszFileName = (LPOLESTR)CoTaskMemAlloc(sizeof(WCHAR) * (strlen + 1));
  
  if(!(*ppszFileName))
    return E_OUTOFMEMORY;
  
  wcsncpy_s(*ppszFileName, strlen, m_fileName, _TRUNCATE);
  return S_OK;
}

// Pin creation
STDMETHODIMP CFFSplitter::CreateOutputs()
{
  CAutoLock lock(this);

  // Conver the filename from wchar to char for avformat
  char fileName[1024];
  wcstombs_s(NULL, fileName, 1024, m_fileName, _TRUNCATE);

  int ret = av_open_input_file(&m_avFormat, fileName, NULL, FFMPEG_FILE_BUFFER_SIZE, NULL);
  if (ret < 0) {
    DbgOutString(L"av_open_input_file failed\n");
    goto fail;
  }

  ret = av_find_stream_info(m_avFormat);
  if (ret < 0) {
    DbgOutString(L"av_find_stream_info failed\n");
    goto fail;
  }

  // try to use non-blocking methods
  m_avFormat->flags |= AVFMT_FLAG_NONBLOCK;

  // TODO
  // For all streams, get stream info, create format info, create output pins
  
  // TODO Programms support
  ASSERT(m_avFormat->nb_programs == 0);

  for(unsigned int streamId = 0; streamId < m_avFormat->nb_streams; streamId++)
  {
    AVStream* pStream = m_avFormat->streams[streamId];
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

STDMETHODIMP CFFSplitter::DeleteOutputs()
{
  CAutoLock lock(this);
  if(m_State != State_Stopped) return VFW_E_NOT_STOPPED;

  if(m_avFormat) {
    av_close_input_file(m_avFormat);
    m_avFormat = NULL;
  }

  return S_OK;
}

// Worker Thread
DWORD CFFSplitter::ThreadProc()
{
  m_fFlushing = false;

  for(DWORD cmd = (DWORD)-1; ; cmd = GetRequest())
  {
    if(cmd == CMD_EXIT)
    {
      m_hThread = NULL;
      Reply(S_OK);
      return 0;
    }

    SetThreadPriority(m_hThread, THREAD_PRIORITY_NORMAL);

    if(cmd != (DWORD)-1)
      Reply(S_OK);

    // Wait for the end of any flush
    m_eEndFlush.Wait();

    // TODO: Demuxing
  }
  ASSERT(0); // we should only exit via CMD_EXIT

  m_hThread = NULL;
  return 0;
}

// State Control
STDMETHODIMP CFFSplitter::Stop()
{
  CAutoLock cAutoLock(this);

  DeliverBeginFlush();
  CallWorker(CMD_EXIT);
  DeliverEndFlush();

  HRESULT hr;
  if(FAILED(hr = __super::Stop()))
    return hr;

  return S_OK;
}

STDMETHODIMP CFFSplitter::Pause()
{
  CAutoLock cAutoLock(this);

  FILTER_STATE fs = m_State;

  HRESULT hr;
  if(FAILED(hr = __super::Pause()))
    return hr;

  if(fs == State_Stopped)
  {
    Create();
  }

  return S_OK;
}

STDMETHODIMP CFFSplitter::Run(REFERENCE_TIME tStart)
{
  CAutoLock cAutoLock(this);

  HRESULT hr;
  if(FAILED(hr = __super::Run(tStart)))
    return hr;

  return S_OK;
}

// Flushing
void CFFSplitter::DeliverBeginFlush()
{
  m_fFlushing = true;
  // TODO: flush pins
}

void CFFSplitter::DeliverEndFlush()
{
  // TODO: flush pins
  m_fFlushing = false;
  m_eEndFlush.Set();
}
