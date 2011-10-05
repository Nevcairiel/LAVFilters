/*
 *      Copyright (C) 2011 Hendrik Leppkes
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
 *
 *  Initial design and concept by Gabest and the MPC-HC Team, copyright under GPLv2
 *  Contributions by Ti-BEN from the XBMC DSPlayer Project, also under GPLv2
 */

#include "stdafx.h"
#include "LAVSplitter.h"
#include "OutputPin.h"
#include "InputPin.h"

#include "BaseDemuxer.h"
#include "LAVFDemuxer.h"
#include "BDDemuxer.h"

#include <Shlwapi.h>
#include <string>
#include <algorithm>

#include "registry.h"

#include "IGraphRebuildDelegate.h"

CLAVSplitter::CLAVSplitter(LPUNKNOWN pUnk, HRESULT* phr) 
  : CBaseFilter(NAME("lavf dshow source filter"), pUnk, this,  __uuidof(this), phr)
  , m_rtStart(0)
  , m_rtStop(0)
  , m_dRate(1.0)
  , m_rtLastStart(_I64_MIN)
  , m_rtLastStop(_I64_MIN)
  , m_rtCurrent(0)
  , m_bPlaybackStarted(FALSE)
  , m_pDemuxer(NULL)
  , m_bRuntimeConfig(FALSE)
  , m_pSite(NULL)
  , m_bFakeASFReader(FALSE)
{
  CLAVFDemuxer::ffmpeg_init();

  m_InputFormats.clear();

  std::set<FormatInfo> lavf_formats = CLAVFDemuxer::GetFormatList();
  m_InputFormats.insert(lavf_formats.begin(), lavf_formats.end());

  LoadSettings();

  m_pInput = new CLAVInputPin(NAME("LAV Input Pin"), this, this, phr);

#ifdef DEBUG
  DbgSetModuleLevel (LOG_TRACE, DWORD_MAX);
  DbgSetModuleLevel (LOG_ERROR, DWORD_MAX);

#if ENABLE_DEBUG_LOGFILE
  DbgSetLogFileDesktop(LAVF_LOG_FILE);
#endif
#endif
}

CLAVSplitter::~CLAVSplitter()
{
  SAFE_DELETE(m_pInput);
  Close();

  // delete old pins
  std::vector<CLAVOutputPin *>::iterator it;
  for(it = m_pRetiredPins.begin(); it != m_pRetiredPins.end(); ++it) {
    delete (*it);
  }
  m_pRetiredPins.clear();

  SafeRelease(&m_pSite);

#if defined(DEBUG) && ENABLE_DEBUG_LOGFILE
  DbgCloseLogFile();
#endif
}

STDMETHODIMP CLAVSplitter::Close()
{
  CAutoLock cAutoLock(this);

  CAMThread::CallWorker(CMD_EXIT);
  CAMThread::Close();

  m_State = State_Stopped;
  DeleteOutputs();

  SafeRelease(&m_pDemuxer);

  return S_OK;
}

// Default overrides for input formats
static BOOL get_iformat_default(std::string name)
{
  // Raw video formats lack timestamps..
  if (name == "rawvideo") {
    return FALSE;
  }

  return TRUE;
}

STDMETHODIMP CLAVSplitter::LoadDefaults()
{
  m_settings.prefAudioLangs = L"";
  m_settings.prefSubLangs   = L"";

  m_settings.subtitleMode     = SUBMODE_ALWAYS_SUBS;
  m_settings.subtitleMatching = TRUE;
  m_settings.PGSForcedStream  = TRUE;
  m_settings.PGSOnlyForced    = FALSE;

  m_settings.vc1Mode          = 2;
  m_settings.substreams       = TRUE;
  m_settings.videoParsing     = TRUE;
  m_settings.FixBrokenHDPVR   = TRUE;

  m_settings.StreamSwitchRemoveAudio = FALSE;

  std::set<FormatInfo>::iterator it;
  for (it = m_InputFormats.begin(); it != m_InputFormats.end(); ++it) {
    m_settings.formats[std::string(it->strName)] = get_iformat_default(it->strName);
  }

  return S_OK;
}

STDMETHODIMP CLAVSplitter::LoadSettings()
{
  LoadDefaults();
  if (m_bRuntimeConfig)
    return S_FALSE;

  HRESULT hr;
  DWORD dwVal;
  BOOL bFlag;

  CreateRegistryKey(HKEY_CURRENT_USER, LAVF_REGISTRY_KEY);
  CRegistry reg = CRegistry(HKEY_CURRENT_USER, LAVF_REGISTRY_KEY, hr);
  // We don't check if opening succeeded, because the read functions will set their hr accordingly anyway,
  // and we need to fill the settings with defaults.
  // ReadString returns an empty string in case of failure, so thats fine!

  // Language preferences
  m_settings.prefAudioLangs = reg.ReadString(L"prefAudioLangs", hr);
  m_settings.prefSubLangs = reg.ReadString(L"prefSubLangs", hr);

  // Subtitle mode, defaults to all subtitles
  dwVal = reg.ReadDWORD(L"subtitleMode", hr);
  if (SUCCEEDED(hr)) m_settings.subtitleMode = dwVal;

  bFlag = reg.ReadDWORD(L"subtitleMatching", hr);
  if (SUCCEEDED(hr)) m_settings.subtitleMatching = bFlag;

  bFlag = reg.ReadBOOL(L"PGSForcedStream", hr);
  if (SUCCEEDED(hr)) m_settings.PGSForcedStream = bFlag;

  bFlag = reg.ReadBOOL(L"PGSOnlyForced", hr);
  if (SUCCEEDED(hr)) m_settings.PGSOnlyForced = bFlag;

  dwVal = reg.ReadDWORD(L"vc1TimestampMode", hr);
  if (SUCCEEDED(hr)) m_settings.vc1Mode = dwVal;

  bFlag = reg.ReadDWORD(L"substreams", hr);
  if (SUCCEEDED(hr)) m_settings.substreams = bFlag;

  bFlag = reg.ReadDWORD(L"videoParsing", hr);
  if (SUCCEEDED(hr)) m_settings.videoParsing = bFlag;

  bFlag = reg.ReadDWORD(L"FixBrokenHDPVR", hr);
  if (SUCCEEDED(hr)) m_settings.FixBrokenHDPVR = bFlag;

  bFlag = reg.ReadDWORD(L"StreamSwitchRemoveAudio", hr);
  if (SUCCEEDED(hr)) m_settings.StreamSwitchRemoveAudio = bFlag;

  CreateRegistryKey(HKEY_CURRENT_USER, LAVF_REGISTRY_KEY_FORMATS);
  CRegistry regF = CRegistry(HKEY_CURRENT_USER, LAVF_REGISTRY_KEY_FORMATS, hr);

  WCHAR wBuffer[80];
  std::set<FormatInfo>::iterator it;
  for (it = m_InputFormats.begin(); it != m_InputFormats.end(); ++it) {
    MultiByteToWideChar(CP_UTF8, 0, it->strName, -1, wBuffer, 80);
    bFlag = regF.ReadBOOL(wBuffer, hr);
    if (SUCCEEDED(hr)) m_settings.formats[std::string(it->strName)] = bFlag;
  }

  return S_OK;
}

STDMETHODIMP CLAVSplitter::SaveSettings()
{
  if (m_bRuntimeConfig) {
    if (m_pDemuxer)
      m_pDemuxer->SettingsChanged(static_cast<ILAVFSettingsInternal *>(this));
    return S_FALSE;
  }

  HRESULT hr;
  CRegistry reg = CRegistry(HKEY_CURRENT_USER, LAVF_REGISTRY_KEY, hr);
  if (SUCCEEDED(hr)) {
    reg.WriteString(L"prefAudioLangs", m_settings.prefAudioLangs.c_str());
    reg.WriteString(L"prefSubLangs", m_settings.prefSubLangs.c_str());
    reg.WriteDWORD(L"subtitleMode", m_settings.subtitleMode);
    reg.WriteBOOL(L"subtitleMatching", m_settings.subtitleMatching);
    reg.WriteBOOL(L"PGSForcedStream", m_settings.PGSForcedStream);
    reg.WriteBOOL(L"PGSOnlyForced", m_settings.PGSOnlyForced);
    reg.WriteDWORD(L"vc1TimestampMode", m_settings.vc1Mode);
    reg.WriteBOOL(L"substreams", m_settings.substreams);
    reg.WriteBOOL(L"videoParsing", m_settings.videoParsing);
    reg.WriteBOOL(L"FixBrokenHDPVR", m_settings.FixBrokenHDPVR);
    reg.WriteBOOL(L"StreamSwitchRemoveAudio", m_settings.StreamSwitchRemoveAudio);
  }

  CRegistry regF = CRegistry(HKEY_CURRENT_USER, LAVF_REGISTRY_KEY_FORMATS, hr);
  if (SUCCEEDED(hr)) {
    WCHAR wBuffer[80];
    std::set<FormatInfo>::iterator it;
    for (it = m_InputFormats.begin(); it != m_InputFormats.end(); ++it) {
      MultiByteToWideChar(CP_UTF8, 0, it->strName, -1, wBuffer, 80);
      regF.WriteBOOL(wBuffer, m_settings.formats[std::string(it->strName)]);
    }
  }

  if (m_pDemuxer) {
    m_pDemuxer->SettingsChanged(static_cast<ILAVFSettingsInternal *>(this));
  }
  return S_OK;
}

STDMETHODIMP CLAVSplitter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = NULL;

  if (m_pDemuxer && (riid == __uuidof(IKeyFrameInfo) || riid == __uuidof(ITrackInfo) || riid == IID_IAMExtendedSeeking)) {
    return m_pDemuxer->QueryInterface(riid, ppv);
  }

  return
    QI(IMediaSeeking)
    QI(IAMStreamSelect)
    QI2(ISpecifyPropertyPages)
    QI2(ILAVFSettings)
    QI2(ILAVFSettingsInternal)
    QI(IObjectWithSite)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

// ISpecifyPropertyPages
STDMETHODIMP CLAVSplitter::GetPages(CAUUID *pPages)
{
  CheckPointer(pPages, E_POINTER);
  pPages->cElems = 2;
  pPages->pElems = (GUID *)CoTaskMemAlloc(sizeof(GUID) * pPages->cElems);
  if (pPages->pElems == NULL) {
    return E_OUTOFMEMORY;
  }
  pPages->pElems[0] = CLSID_LAVSplitterSettingsProp;
  pPages->pElems[1] = CLSID_LAVSplitterFormatsProp;
  return S_OK;
}

// IObjectWithSite
STDMETHODIMP CLAVSplitter::SetSite(IUnknown *pUnkSite)
{
  // AddRef to store it for later
  pUnkSite->AddRef();

  // Release the old one
  SafeRelease(&m_pSite);

  // Store the new one
  m_pSite = pUnkSite;

  return S_OK;
}

STDMETHODIMP CLAVSplitter::GetSite(REFIID riid, void **ppvSite)
{
  CheckPointer(ppvSite, E_POINTER);
  *ppvSite = NULL;
  if (!m_pSite) {
    return E_FAIL;
  }

  IUnknown *pSite = NULL;
  HRESULT hr = m_pSite->QueryInterface(riid, (void **)&pSite);
  if (SUCCEEDED(hr) && pSite) {
    pSite->AddRef();
    *ppvSite = pSite;
    return S_OK;
  }
  return E_NOINTERFACE;
}

// CBaseSplitter
int CLAVSplitter::GetPinCount()
{
  CAutoLock lock(&m_csPins);

  int count = (int)m_pPins.size();
  if (m_pInput)
    count++;

  return count;
}

CBasePin *CLAVSplitter::GetPin(int n)
{
  CAutoLock lock(&m_csPins);

  if (n < 0 ||n >= GetPinCount()) return NULL;

  if (m_pInput) {
    if(n == 0)
      return m_pInput;
    else
      n--;
  }

  return m_pPins[n];
}

STDMETHODIMP CLAVSplitter::GetClassID(CLSID* pClsID)
{
  CheckPointer (pClsID, E_POINTER);

  if (m_bFakeASFReader) {
    *pClsID = CLSID_WMAsfReader;
    return S_OK;
  } else {
    return __super::GetClassID(pClsID);
  }
}

CLAVOutputPin *CLAVSplitter::GetOutputPin(DWORD streamId, BOOL bActiveOnly)
{
  CAutoLock lock(&m_csPins);

  std::vector<CLAVOutputPin *> &vec = bActiveOnly ? m_pActivePins : m_pPins;

  std::vector<CLAVOutputPin *>::iterator it;
  for(it = vec.begin(); it != vec.end(); ++it) {
    if ((*it)->GetStreamId() == streamId) {
      return *it;
    }
  }
  return NULL;
}

STDMETHODIMP CLAVSplitter::CompleteInputConnection()
{
  HRESULT hr = S_OK;

  SAFE_DELETE(m_pDemuxer);
  CLAVFDemuxer *pDemux = new CLAVFDemuxer(this, this);

  AVIOContext *pContext = NULL;

  if (FAILED(hr = m_pInput->GetAVIOContext(&pContext))) {
    return hr;
  }

  LPOLESTR pszFileName = NULL;

  PIN_INFO info;
  hr = m_pInput->GetConnected()->QueryPinInfo(&info);
  if (SUCCEEDED(hr) && info.pFilter) {
    IFileSourceFilter *pSource = NULL;
    if (SUCCEEDED(info.pFilter->QueryInterface(&pSource)) && pSource) {
      pSource->GetCurFile(&pszFileName, NULL);
      SafeRelease(&pSource);
    }
    SafeRelease(&info.pFilter);
  }

  if(FAILED(hr = pDemux->OpenInputStream(pContext, pszFileName))) {
    SAFE_DELETE(pDemux);
    return hr;
  }
  m_pDemuxer = pDemux;
  m_pDemuxer->AddRef();

  return InitDemuxer();
}

STDMETHODIMP CLAVSplitter::BreakInputConnection()
{
  return Close();
}

// IFileSourceFilter
STDMETHODIMP CLAVSplitter::Load(LPCOLESTR pszFileName, const AM_MEDIA_TYPE * pmt)
{
  CheckPointer(pszFileName, E_POINTER);

  m_bPlaybackStarted = FALSE;

  m_fileName = std::wstring(pszFileName);

  HRESULT hr = S_OK;
  SAFE_DELETE(m_pDemuxer);
  LPWSTR extension = PathFindExtensionW(pszFileName);
  // BDMV uses the BD demuxer, everything else LAVF
  if (_wcsicmp(extension, L".bdmv") == 0 || _wcsicmp(extension, L".mpls") == 0) {
    m_pDemuxer = new CBDDemuxer(this, this);
  } else {
    m_pDemuxer = new CLAVFDemuxer(this, this);
  }
  if(FAILED(hr = m_pDemuxer->Open(pszFileName))) {
    SAFE_DELETE(m_pDemuxer);
    return hr;
  }
  m_pDemuxer->AddRef();

  return InitDemuxer();
}

// Get the currently loaded file
STDMETHODIMP CLAVSplitter::GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt)
{
  CheckPointer(ppszFileName, E_POINTER);

  size_t strlen = m_fileName.length() + 1;
  *ppszFileName = (LPOLESTR)CoTaskMemAlloc(sizeof(wchar_t) * strlen);

  if(!(*ppszFileName))
    return E_OUTOFMEMORY;

  wcsncpy_s(*ppszFileName, strlen, m_fileName.c_str(), _TRUNCATE);
  return S_OK;
}

STDMETHODIMP CLAVSplitter::InitDemuxer()
{
  HRESULT hr = S_OK;

  m_rtStart = m_rtNewStart = m_rtCurrent = 0;
  m_rtStop = m_rtNewStop = m_pDemuxer->GetDuration();

  const CBaseDemuxer::stream *videoStream = m_pDemuxer->SelectVideoStream();
  if (videoStream) {
    CLAVOutputPin* pPin = new CLAVOutputPin(videoStream->streamInfo->mtypes, CBaseDemuxer::CStreamList::ToStringW(CBaseDemuxer::video), this, this, &hr, CBaseDemuxer::video, m_pDemuxer->GetContainerFormat());
    if(SUCCEEDED(hr)) {
      pPin->SetStreamId(videoStream->pid);
      m_pPins.push_back(pPin);
      m_pDemuxer->SetActiveStream(CBaseDemuxer::video, videoStream->pid);
    } else {
      delete pPin;
    }
  }

  std::list<std::string> audioLangs = GetPreferredAudioLanguageList();
  const CBaseDemuxer::stream *audioStream = m_pDemuxer->SelectAudioStream(audioLangs);
  if (audioStream) {
    CLAVOutputPin* pPin = new CLAVOutputPin(audioStream->streamInfo->mtypes, CBaseDemuxer::CStreamList::ToStringW(CBaseDemuxer::audio), this, this, &hr, CBaseDemuxer::audio, m_pDemuxer->GetContainerFormat());
    if(SUCCEEDED(hr)) {
      pPin->SetStreamId(audioStream->pid);
      m_pPins.push_back(pPin);
      m_pDemuxer->SetActiveStream(CBaseDemuxer::audio, audioStream->pid);
    } else {
      delete pPin;
    }
  }

  std::list<std::string> subtitleLangs = GetPreferredSubtitleLanguageList();
  if (subtitleLangs.empty() && !audioLangs.empty()) {
    subtitleLangs = audioLangs;
  }
  const CBaseDemuxer::stream *subtitleStream = m_pDemuxer->SelectSubtitleStream(subtitleLangs, m_settings.subtitleMode, m_settings.subtitleMatching);
  if (subtitleStream) {
    CLAVOutputPin* pPin = new CLAVOutputPin(subtitleStream->streamInfo->mtypes, CBaseDemuxer::CStreamList::ToStringW(CBaseDemuxer::subpic), this, this, &hr, CBaseDemuxer::subpic, m_pDemuxer->GetContainerFormat());
    if(SUCCEEDED(hr)) {
      pPin->SetStreamId(subtitleStream->pid);
      m_pPins.push_back(pPin);
      m_pDemuxer->SetActiveStream(CBaseDemuxer::subpic, subtitleStream->pid);
    } else {
      delete pPin;
    }
  }

  if(SUCCEEDED(hr)) {
    // If there are no pins, what good are we?
    return !m_pPins.empty() ? S_OK : E_FAIL;
  } else {
    return hr;
  }
}

STDMETHODIMP CLAVSplitter::DeleteOutputs()
{
  CAutoLock lock(this);
  if(m_State != State_Stopped) return VFW_E_NOT_STOPPED;

  CAutoLock pinLock(&m_csPins);
  // Release pins
  std::vector<CLAVOutputPin *>::iterator it;
  for(it = m_pPins.begin(); it != m_pPins.end(); ++it) {
    if(IPin* pPinTo = (*it)->GetConnected()) pPinTo->Disconnect();
    (*it)->Disconnect();
    m_pRetiredPins.push_back(*it);
  }
  m_pPins.clear();

  return S_OK;
}

bool CLAVSplitter::IsAnyPinDrying()
{
  // MPC changes thread priority here
  // TODO: Investigate if that is needed
  std::vector<CLAVOutputPin *>::iterator it;
  for(it = m_pActivePins.begin(); it != m_pActivePins.end(); ++it) {
    if((*it)->IsConnected() && !(*it)->IsDiscontinuous() && (*it)->QueueCount() < MIN_PACKETS_IN_QUEUE) {
      return true;
    }
  }
  return false;
}

// Worker Thread
DWORD CLAVSplitter::ThreadProc()
{
  std::vector<CLAVOutputPin *>::iterator pinIter;

  CheckPointer(m_pDemuxer, 0);

  SetThreadName(-1, "CLAVSplitter Demux");

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

    if(m_bPlaybackStarted || m_rtStart != 0 || cmd == CMD_SEEK)
      DemuxSeek(m_rtStart);

    if(cmd != (DWORD)-1)
      Reply(S_OK);

    // Wait for the end of any flush
    m_eEndFlush.Wait();

    m_pActivePins.clear();

    for(pinIter = m_pPins.begin(); pinIter != m_pPins.end() && !m_fFlushing; ++pinIter) {
      if ((*pinIter)->IsConnected()) {
        (*pinIter)->DeliverNewSegment(m_rtStart, m_rtStop, m_dRate);
        m_pActivePins.push_back(*pinIter);
      }
    }

    m_bDiscontinuitySent.clear();

    m_bPlaybackStarted = TRUE;

    HRESULT hr = S_OK;
    while(SUCCEEDED(hr) && !CheckRequest(&cmd)) {
      hr = DemuxNextPacket();
    }

    // If we didnt exit by request, deliver end-of-stream
    if(!CheckRequest(&cmd)) {
      for(pinIter = m_pActivePins.begin(); pinIter != m_pActivePins.end(); ++pinIter) {
        (*pinIter)->QueueEndOfStream();
      }
    }

  }
  ASSERT(0); // we should only exit via CMD_EXIT

  m_hThread = NULL;
  return 0;
}

// Seek to the specified time stamp
// Based on DVDDemuxFFMPEG
HRESULT CLAVSplitter::DemuxSeek(REFERENCE_TIME rtStart)
{
  if(rtStart < 0) { rtStart = 0; }
  
  return m_pDemuxer->Seek(rtStart);
}

// Demux the next packet and deliver it to the output pins
// Based on DVDDemuxFFMPEG
HRESULT CLAVSplitter::DemuxNextPacket()
{
  Packet *pPacket;
  HRESULT hr = S_OK;
  hr = m_pDemuxer->GetNextPacket(&pPacket);
  // Only S_OK indicates we have a proper packet
  // S_FALSE is a "soft error", don't deliver the packet
  if (hr != S_OK) {
    return hr;
  }
  return DeliverPacket(pPacket);
}

HRESULT CLAVSplitter::DeliverPacket(Packet *pPacket)
{
  HRESULT hr = S_FALSE;

  if (pPacket->dwFlags & LAV_PACKET_FORCED_SUBTITLE)
    pPacket->StreamId = FORCED_SUBTITLE_PID;

  CLAVOutputPin* pPin = GetOutputPin(pPacket->StreamId, TRUE);
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
    pPacket->bDiscontinuity = TRUE;
  }

  BOOL bDiscontinuity = pPacket->bDiscontinuity; 
  DWORD streamId = pPacket->StreamId;

  hr = pPin->QueuePacket(pPacket);

  if (hr != S_OK) {
    // Find a iterator pointing to the pin
    std::vector<CLAVOutputPin *>::iterator it = std::find(m_pActivePins.begin(), m_pActivePins.end(), pPin);
    // Remove it from the vector
    m_pActivePins.erase(it);

    // Fail if no active pins remain, otherwise resume demuxing
    return m_pActivePins.empty() ? E_FAIL : S_OK;
  }

  if(bDiscontinuity) {
    m_bDiscontinuitySent.insert(streamId);
  }

  return hr;
}

// State Control
STDMETHODIMP CLAVSplitter::Stop()
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

STDMETHODIMP CLAVSplitter::Pause()
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
    // At this point, the graph is hopefully finished, tell the demuxer about all the cool things
    m_pDemuxer->SettingsChanged(static_cast<ILAVFSettingsInternal *>(this));

    // Create demuxing thread
    Create();
  }

  return S_OK;
}

STDMETHODIMP CLAVSplitter::Run(REFERENCE_TIME tStart)
{
  CAutoLock cAutoLock(this);

  HRESULT hr;
  if(FAILED(hr = __super::Run(tStart))) {
    return hr;
  }

  return S_OK;
}

// Flushing
void CLAVSplitter::DeliverBeginFlush()
{
  m_fFlushing = true;

  // flush all pins
  std::vector<CLAVOutputPin *>::iterator it;
  for(it = m_pPins.begin(); it != m_pPins.end(); ++it) {
    (*it)->DeliverBeginFlush();
  }
}

void CLAVSplitter::DeliverEndFlush()
{
  // flush all pins
  std::vector<CLAVOutputPin *>::iterator it;
  for(it = m_pPins.begin(); it != m_pPins.end(); ++it) {
    (*it)->DeliverEndFlush();
  }

  m_fFlushing = false;
  m_eEndFlush.Set();
}

// IMediaSeeking
STDMETHODIMP CLAVSplitter::GetCapabilities(DWORD* pCapabilities)
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

STDMETHODIMP CLAVSplitter::CheckCapabilities(DWORD* pCapabilities)
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

STDMETHODIMP CLAVSplitter::IsFormatSupported(const GUID* pFormat) {return !pFormat ? E_POINTER : *pFormat == TIME_FORMAT_MEDIA_TIME ? S_OK : S_FALSE;}
STDMETHODIMP CLAVSplitter::QueryPreferredFormat(GUID* pFormat) {return GetTimeFormat(pFormat);}
STDMETHODIMP CLAVSplitter::GetTimeFormat(GUID* pFormat) {return pFormat ? *pFormat = TIME_FORMAT_MEDIA_TIME, S_OK : E_POINTER;}
STDMETHODIMP CLAVSplitter::IsUsingTimeFormat(const GUID* pFormat) {return IsFormatSupported(pFormat);}
STDMETHODIMP CLAVSplitter::SetTimeFormat(const GUID* pFormat) {return S_OK == IsFormatSupported(pFormat) ? S_OK : E_INVALIDARG;}
STDMETHODIMP CLAVSplitter::GetDuration(LONGLONG* pDuration) {CheckPointer(pDuration, E_POINTER); CheckPointer(m_pDemuxer, E_UNEXPECTED); *pDuration = m_pDemuxer->GetDuration(); return S_OK;}
STDMETHODIMP CLAVSplitter::GetStopPosition(LONGLONG* pStop) {return GetDuration(pStop);}
STDMETHODIMP CLAVSplitter::GetCurrentPosition(LONGLONG* pCurrent) {return E_NOTIMPL;}
STDMETHODIMP CLAVSplitter::ConvertTimeFormat(LONGLONG* pTarget, const GUID* pTargetFormat, LONGLONG Source, const GUID* pSourceFormat) {return E_NOTIMPL;}
STDMETHODIMP CLAVSplitter::SetPositions(LONGLONG* pCurrent, DWORD dwCurrentFlags, LONGLONG* pStop, DWORD dwStopFlags)
{
  return SetPositionsInternal(this, pCurrent, dwCurrentFlags, pStop, dwStopFlags);
}
STDMETHODIMP CLAVSplitter::SetPositionsInternal(void *caller, LONGLONG* pCurrent, DWORD dwCurrentFlags, LONGLONG* pStop, DWORD dwStopFlags)
{
  DbgLog((LOG_TRACE, 20, "::SetPositions() - seek request; caller: %p, current: %I64d; start: %I64d; flags: 0x%x, stop: %I64d; flags: 0x%x", caller, m_rtCurrent, pCurrent ? *pCurrent : -1, dwCurrentFlags, pStop ? *pStop : -1, dwStopFlags));
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

  if(m_rtLastStart == rtCurrent && m_rtLastStop == rtStop && m_LastSeekers.find(caller) == m_LastSeekers.end()) {
    m_LastSeekers.insert(caller);
    return S_OK;
  }

  m_rtLastStart = rtCurrent;
  m_rtLastStop = rtStop;
  m_LastSeekers.clear();
  m_LastSeekers.insert(caller);

  m_rtNewStart = m_rtCurrent = rtCurrent;
  m_rtNewStop = rtStop;

  DbgLog((LOG_TRACE, 20, " -> Performing seek to %I64d", m_rtNewStart));
  if(ThreadExists())
  {
    DeliverBeginFlush();
    CallWorker(CMD_SEEK);
    DeliverEndFlush();
  }
  DbgLog((LOG_TRACE, 20, " -> Seek finished", m_rtNewStart));

  return S_OK;
}
STDMETHODIMP CLAVSplitter::GetPositions(LONGLONG* pCurrent, LONGLONG* pStop)
{
  if(pCurrent) *pCurrent = m_rtCurrent;
  if(pStop) *pStop = m_rtStop;
  return S_OK;
}
STDMETHODIMP CLAVSplitter::GetAvailable(LONGLONG* pEarliest, LONGLONG* pLatest)
{
  if(pEarliest) *pEarliest = 0;
  return GetDuration(pLatest);
}
STDMETHODIMP CLAVSplitter::SetRate(double dRate) {return dRate > 0 ? m_dRate = dRate, S_OK : E_INVALIDARG;}
STDMETHODIMP CLAVSplitter::GetRate(double* pdRate) {return pdRate ? *pdRate = m_dRate, S_OK : E_POINTER;}
STDMETHODIMP CLAVSplitter::GetPreroll(LONGLONG* pllPreroll) {return pllPreroll ? *pllPreroll = 0, S_OK : E_POINTER;}

STDMETHODIMP CLAVSplitter::UpdateForcedSubtitleMediaType()
{
  CheckPointer(m_pDemuxer, E_UNEXPECTED);

  CLAVOutputPin* pPin = GetOutputPin(FORCED_SUBTITLE_PID);
  if (pPin) {
    const CBaseDemuxer::CStreamList *streams = m_pDemuxer->GetStreams(CBaseDemuxer::subpic);
    const CBaseDemuxer::stream *s = streams->FindStream(FORCED_SUBTITLE_PID);
    CMediaType *mt = new CMediaType(s->streamInfo->mtypes.back());
    pPin->SendMediaType(mt);
  }

  return S_OK;
}

static int QueryAcceptMediaTypes(IPin *pPin, std::vector<CMediaType> pmts)
{
  for(unsigned int i = 0; i < pmts.size(); i++) {
    if (S_OK == pPin->QueryAccept(&pmts[i])) {
      DbgLog((LOG_TRACE, 20, L"QueryAcceptMediaTypes() - IPin:QueryAccept succeeded on index %d", i));
      return i;
    }
  }
  return -1;
}

STDMETHODIMP CLAVSplitter::RenameOutputPin(DWORD TrackNumSrc, DWORD TrackNumDst, std::vector<CMediaType> pmts)
{
  CheckPointer(m_pDemuxer, E_UNEXPECTED);

#ifndef DEBUG
  if (TrackNumSrc == TrackNumDst) return S_OK;
#endif

  CLAVOutputPin* pPin = GetOutputPin(TrackNumSrc);

  DbgLog((LOG_TRACE, 20, L"::RenameOutputPin() - Switching %s Stream %d to %d", CBaseDemuxer::CStreamList::ToStringW(pPin->GetPinType()), TrackNumSrc, TrackNumDst));
  // Output Pin was found
  // Stop the Graph, remove the old filter, render the graph again, start it up again
  // This only works on pins that were connected before, or the filter graph could .. well, break
  if (pPin && pPin->IsConnected()) {
    HRESULT hr = S_OK;

    IMediaControl *pControl = NULL;
    hr = m_pGraph->QueryInterface(IID_IMediaControl, (void **)&pControl);

    FILTER_STATE oldState;
    // Get the graph state
    // If the graph is in transition, we'll get the next state, not the previous
    hr = pControl->GetState(10, (OAFilterState *)&oldState);
    DbgLog((LOG_TRACE, 20, L"::RenameOutputPin() - IMediaControl::GetState returned %d (hr %x)", oldState, hr));

    // Stop the filter graph
    hr = pControl->Stop();
    DbgLog((LOG_TRACE, 20, L"::RenameOutputPin() - IMediaControl::Stop (hr %x)", hr));

    Lock();

    // Update Output Pin
    pPin->SetStreamId(TrackNumDst);
    m_pDemuxer->SetActiveStream(pPin->GetPinType(), TrackNumDst);
    pPin->SetNewMediaTypes(pmts);

    // IGraphRebuildDelegate support
    // Query our Site for the appropriate interface, and if its present, delegate graph building there
    IGraphRebuildDelegate *pDelegate = NULL;
    if (SUCCEEDED(GetSite(IID_IGraphRebuildDelegate, (void **)&pDelegate)) && pDelegate) {
      hr = pDelegate->RebuildPin(m_pGraph, pPin);
      if (hr == S_FALSE) {
        int mtIdx = QueryAcceptMediaTypes(pPin->GetConnected(), pmts);
        if (mtIdx == -1) {
          DbgLog((LOG_ERROR, 10, L"::RenameOutputPin(): No matching media type after rebuild delegation"));
          mtIdx = 0;
        }
        CMediaType *mt = new CMediaType(pmts[mtIdx]);
        pPin->SendMediaType(mt);
      }
      SafeRelease(&pDelegate);

      if (SUCCEEDED(hr)) {
        goto resumegraph;
      }
      DbgLog((LOG_TRACE, 10, L"::RenameOutputPin(): IGraphRebuildDelegate::RebuildPin failed"));
    }

    // Audio Filters get their connected filter removed
    // This way we make sure we reconnect to the proper filter
    // Other filters just disconnect and try to reconnect later on
    PIN_INFO pInfo;
    hr = pPin->GetConnected()->QueryPinInfo(&pInfo);
    if (FAILED(hr)) {
      DbgLog((LOG_ERROR, 10, L"::RenameOutputPin(): QueryPinInfo failed (hr %x)", hr));
    }

    int mtIdx = QueryAcceptMediaTypes(pPin->GetConnected(), pmts);
    BOOL bMediaTypeFound = (mtIdx >= 0);

    if (!bMediaTypeFound) {
      DbgLog((LOG_TRACE, 10, L"::RenameOutputPin() - Filter does not accept our media types!"));
      mtIdx = 0; // Fallback type
    }
    CMediaType *pmt = &pmts[mtIdx];

    if(!pPin->IsVideoPin() && SUCCEEDED(hr) && pInfo.pFilter) {
      BOOL bRemoveFilter = m_settings.StreamSwitchRemoveAudio || !bMediaTypeFound;
      if (bRemoveFilter && pPin->IsAudioPin()) {
        hr = m_pGraph->RemoveFilter(pInfo.pFilter);
  #ifdef DEBUG
        CLSID guidFilter;
        pInfo.pFilter->GetClassID(&guidFilter);
        DbgLog((LOG_TRACE, 20, L"::RenameOutputPin() - IFilterGraph::RemoveFilter - %s (hr %x)", WStringFromGUID(guidFilter).c_str(), hr));
  #endif
        // Use IGraphBuilder to rebuild the graph
        IGraphBuilder *pGraphBuilder = NULL;
        if(SUCCEEDED(hr = m_pGraph->QueryInterface(__uuidof(IGraphBuilder), (void **)&pGraphBuilder))) {
          // Instruct the GraphBuilder to connect us again
          hr = pGraphBuilder->Render(pPin);
          DbgLog((LOG_TRACE, 20, L"::RenameOutputPin() - IGraphBuilder::Render (hr %x)", hr));
          pGraphBuilder->Release();
        }
      } else {
        hr = ReconnectPin(pPin, pmt);
        DbgLog((LOG_TRACE, 20, L"::RenameOutputPin() - ReconnectPin (hr %x)", hr));
      }

      if (pPin->IsAudioPin() && m_settings.PGSForcedStream)
        UpdateForcedSubtitleMediaType();
    } else {
      CMediaType *mt = new CMediaType(*pmt);
      pPin->SendMediaType(mt);
      DbgLog((LOG_TRACE, 20, L"::RenameOutputPin() - Sending new Media Type"));
    }
    if(SUCCEEDED(hr) && pInfo.pFilter) { pInfo.pFilter->Release(); }

resumegraph:
    Unlock();

    // Re-start the graph
    if(oldState == State_Paused) {
      hr = pControl->Pause();
      DbgLog((LOG_TRACE, 20, L"::RenameOutputPin() - IMediaControl::Pause (hr %x)", hr));
    } else if (oldState == State_Running) {
      hr = pControl->Run();
      DbgLog((LOG_TRACE, 20, L"::RenameOutputPin() - IMediaControl::Run (hr %x)", hr));
    }
    pControl->Release();

    return hr;
  } else if (pPin) {
    CAutoLock lock(this);
    // In normal operations, this won't make much sense
    // However, in graphstudio it is now possible to change the stream before connecting
    pPin->SetStreamId(TrackNumDst);
    m_pDemuxer->SetActiveStream(pPin->GetPinType(), TrackNumDst);
    pPin->SetNewMediaTypes(pmts);

    return S_OK;
  }

  return E_FAIL;
}

// IAMStreamSelect
STDMETHODIMP CLAVSplitter::Count(DWORD *pcStreams)
{
  CheckPointer(pcStreams, E_POINTER);
  CheckPointer(m_pDemuxer, E_UNEXPECTED);

  *pcStreams = 0;
  for(int i = 0; i < CBaseDemuxer::unknown; i++) {
    *pcStreams += (DWORD)m_pDemuxer->GetStreams((CBaseDemuxer::StreamType)i)->size();
  }

  return S_OK;
}

STDMETHODIMP CLAVSplitter::Enable(long lIndex, DWORD dwFlags)
{
  CheckPointer(m_pDemuxer, E_UNEXPECTED);
  if(!(dwFlags & AMSTREAMSELECTENABLE_ENABLE)) {
    return E_NOTIMPL;
  }

  for(int i = 0, j = 0; i < CBaseDemuxer::unknown; i++) {
    CBaseDemuxer::CStreamList *streams = m_pDemuxer->GetStreams((CBaseDemuxer::StreamType)i);
    int cnt = (int)streams->size();

    if(lIndex >= j && lIndex < j+cnt) {
      long idx = (lIndex - j);

      CBaseDemuxer::stream& to = streams->at(idx);

      std::deque<CBaseDemuxer::stream>::iterator it;
      for(it = streams->begin(); it != streams->end(); ++it) {
        if(!GetOutputPin(it->pid)) {
          continue;
        }

        HRESULT hr;
        if(FAILED(hr = RenameOutputPin(*it, to, to.streamInfo->mtypes))) {
          return hr;
        }
        return S_OK;
      }
      break;
    }
    j += cnt;
  }
  return S_FALSE;
}

STDMETHODIMP CLAVSplitter::Info(long lIndex, AM_MEDIA_TYPE **ppmt, DWORD *pdwFlags, LCID *plcid, DWORD *pdwGroup, WCHAR **ppszName, IUnknown **ppObject, IUnknown **ppUnk)
{
  CheckPointer(m_pDemuxer, E_UNEXPECTED);
  HRESULT hr = S_FALSE;
  for(int i = 0, j = 0; i < CBaseDemuxer::unknown; i++) {
    CBaseDemuxer::CStreamList *streams = m_pDemuxer->GetStreams((CBaseDemuxer::StreamType)i);
    int cnt = (int)streams->size();

    if(lIndex >= j && lIndex < j+cnt) {
      long idx = (lIndex - j);

      CBaseDemuxer::stream& s = streams->at(idx);

      if(ppmt) *ppmt = CreateMediaType(&s.streamInfo->mtypes[0]);
      if(pdwFlags) *pdwFlags = GetOutputPin(s) ? (AMSTREAMSELECTINFO_ENABLED|AMSTREAMSELECTINFO_EXCLUSIVE) : 0;
      if(pdwGroup) *pdwGroup = i;
      if(ppObject) *ppObject = NULL;
      if(ppUnk) *ppUnk = NULL;

      // Special case for the "no subtitles" pin
      if(s.pid == NO_SUBTITLE_PID) {
        if (plcid) *plcid = LCID_NOSUBTITLES;
        if (ppszName) {
          WCHAR str[] = L"S: No subtitles";
          size_t len = wcslen(str) + 1;
          *ppszName = (WCHAR*)CoTaskMemAlloc(len * sizeof(WCHAR));
          wcsncpy_s(*ppszName, len, str, _TRUNCATE);
        }
      } else if (s.pid == FORCED_SUBTITLE_PID) {
        if (plcid) {
          SUBTITLEINFO *subinfo = (SUBTITLEINFO *)s.streamInfo->mtypes[0].Format();
          *plcid = ProbeLangForLCID(subinfo->IsoLang);
        }
        if (ppszName) {
          WCHAR str[] = L"S: " FORCED_SUB_STRING;
          size_t len = wcslen(str) + 1;
          *ppszName = (WCHAR*)CoTaskMemAlloc(len * sizeof(WCHAR));
          wcsncpy_s(*ppszName, len, str, _TRUNCATE);
        }
      } else {
        // Populate stream name and language code
        m_pDemuxer->StreamInfo(s.pid, plcid, ppszName);
      }
      hr = S_OK;
      break;
    }
    j += cnt;
  }

  return hr;
}

// setting helpers
std::list<std::string> CLAVSplitter::GetPreferredAudioLanguageList()
{
  // Convert to multi-byte ascii
  int bufSize = (int)(sizeof(WCHAR) * (m_settings.prefAudioLangs.length() + 1));
  char *buffer = (char *)CoTaskMemAlloc(bufSize);
  WideCharToMultiByte(CP_UTF8, 0, m_settings.prefAudioLangs.c_str(), -1, buffer, bufSize, NULL, NULL);

  std::list<std::string> list;

  split(std::string(buffer), std::string(",; "), list);

  return list;
}

std::list<std::string> CLAVSplitter::GetPreferredSubtitleLanguageList()
{
  // Convert to multi-byte ascii
  int bufSize = (int)(sizeof(WCHAR) * (m_settings.prefSubLangs.length() + 1));
  char *buffer = (char *)CoTaskMemAlloc(bufSize);
  WideCharToMultiByte(CP_UTF8, 0, m_settings.prefSubLangs.c_str(), -1, buffer, bufSize, NULL, NULL);

  std::list<std::string> list;

  split(std::string(buffer), std::string(",; "), list);

  return list;
}

// Settings
// ILAVAudioSettings
HRESULT CLAVSplitter::SetRuntimeConfig(BOOL bRuntimeConfig)
{
  m_bRuntimeConfig = bRuntimeConfig;
  LoadSettings();

  return S_OK;
}


STDMETHODIMP CLAVSplitter::GetPreferredLanguages(WCHAR **ppLanguages)
{
  CheckPointer(ppLanguages, E_POINTER);
  size_t len = m_settings.prefAudioLangs.length() + 1;
  if (len > 1) {
    *ppLanguages = (WCHAR *)CoTaskMemAlloc(sizeof(WCHAR) * len);
    wcsncpy_s(*ppLanguages, len,  m_settings.prefAudioLangs.c_str(), _TRUNCATE);
  } else {
    *ppLanguages = NULL;
  }
  return S_OK;
}

STDMETHODIMP CLAVSplitter::SetPreferredLanguages(WCHAR *pLanguages)
{
  m_settings.prefAudioLangs = std::wstring(pLanguages);
  return SaveSettings();
}

STDMETHODIMP CLAVSplitter::GetPreferredSubtitleLanguages(WCHAR **ppLanguages)
{
  CheckPointer(ppLanguages, E_POINTER);
  size_t len = m_settings.prefSubLangs.length() + 1;
  if (len > 1) {
    *ppLanguages = (WCHAR *)CoTaskMemAlloc(sizeof(WCHAR) * len);
    wcsncpy_s(*ppLanguages, len,  m_settings.prefSubLangs.c_str(), _TRUNCATE);
  } else {
    *ppLanguages = NULL;
  }
  return S_OK;
}

STDMETHODIMP CLAVSplitter::SetPreferredSubtitleLanguages(WCHAR *pLanguages)
{
  m_settings.prefSubLangs = std::wstring(pLanguages);
  return SaveSettings();
}

STDMETHODIMP_(DWORD) CLAVSplitter::GetSubtitleMode()
{
  return m_settings.subtitleMode;
}

STDMETHODIMP CLAVSplitter::SetSubtitleMode(DWORD dwMode)
{
  m_settings.subtitleMode = dwMode;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVSplitter::GetSubtitleMatchingLanguage()
{
  return m_settings.subtitleMatching;
}

STDMETHODIMP CLAVSplitter::SetSubtitleMatchingLanguage(BOOL dwMode)
{
  m_settings.subtitleMatching = dwMode;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVSplitter::GetPGSForcedStream()
{
  return m_settings.PGSForcedStream;
}

STDMETHODIMP CLAVSplitter::SetPGSForcedStream(BOOL bFlag)
{
  m_settings.PGSForcedStream = bFlag;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVSplitter::GetPGSOnlyForced()
{
  return m_settings.PGSOnlyForced;
}

STDMETHODIMP CLAVSplitter::SetPGSOnlyForced(BOOL bForced)
{
  m_settings.PGSOnlyForced = bForced;
  return SaveSettings();
}

STDMETHODIMP_(int) CLAVSplitter::GetVC1TimestampMode()
{
  return m_settings.vc1Mode;
}

STDMETHODIMP CLAVSplitter::SetVC1TimestampMode(int iMode)
{
  m_settings.vc1Mode = iMode;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVSplitter::IsVC1CorrectionRequired()
{
  return
      FilterInGraph(CLSID_LAVVideo, m_pGraph)
   || (FilterInGraph(CLSID_LAVCUVID, m_pGraph) && (_strnicmp(m_pDemuxer->GetContainerFormat(), "matroska", 8) == 0))
   || FilterInGraph(CLSID_MPCVideoDec, m_pGraph)
   || FilterInGraph(CLSID_ffdshowDXVA, m_pGraph)
   || FilterInGraphWithInputSubtype(CLSID_madVR, m_pGraph, MEDIASUBTYPE_WVC1)
   || FilterInGraphWithInputSubtype(CLSID_DMOWrapperFilter, m_pGraph, MEDIASUBTYPE_WVC1);
}

STDMETHODIMP CLAVSplitter::SetSubstreamsEnabled(BOOL bSubStreams)
{
  m_settings.substreams = bSubStreams;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVSplitter::GetSubstreamsEnabled()
{
  return m_settings.substreams;
}

STDMETHODIMP CLAVSplitter::SetVideoParsingEnabled(BOOL bEnabled)
{
  m_settings.videoParsing = bEnabled;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVSplitter::GetVideoParsingEnabled()
{
  return m_settings.videoParsing;
}

STDMETHODIMP CLAVSplitter::SetFixBrokenHDPVR(BOOL bEnabled)
{
  m_settings.FixBrokenHDPVR = bEnabled;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVSplitter::GetFixBrokenHDPVR()
{
  return m_settings.FixBrokenHDPVR;
}

STDMETHODIMP_(BOOL) CLAVSplitter::IsFormatEnabled(const char *strFormat)
{
  std::string format(strFormat);
  if (m_settings.formats.find(format) != m_settings.formats.end()) {
    return m_settings.formats[format];
  }
  return FALSE;
}

STDMETHODIMP_(HRESULT) CLAVSplitter::SetFormatEnabled(const char *strFormat, BOOL bEnabled)
{
  std::string format(strFormat);
  if (m_settings.formats.find(format) != m_settings.formats.end()) {
    m_settings.formats[format] = bEnabled;
    return SaveSettings();
  }
  return E_FAIL;
}

STDMETHODIMP CLAVSplitter::SetStreamSwitchRemoveAudio(BOOL bEnabled)
{
  m_settings.StreamSwitchRemoveAudio = bEnabled;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVSplitter::GetStreamSwitchRemoveAudio()
{
  return m_settings.StreamSwitchRemoveAudio;
}

STDMETHODIMP_(std::set<FormatInfo>&) CLAVSplitter::GetInputFormats()
{
  return m_InputFormats;
}

CLAVSplitterSource::CLAVSplitterSource(LPUNKNOWN pUnk, HRESULT* phr)
  : CLAVSplitter(pUnk, phr)
{
  m_clsid = __uuidof(CLAVSplitterSource);
  SAFE_DELETE(m_pInput);
}

CLAVSplitterSource::~CLAVSplitterSource()
{
}

STDMETHODIMP CLAVSplitterSource::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = NULL;

  return
    QI(IFileSourceFilter)
    __super::NonDelegatingQueryInterface(riid, ppv);
}
