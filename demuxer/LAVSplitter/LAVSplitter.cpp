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
#include <regex>
#include <algorithm>

#include "registry.h"

#include "IGraphRebuildDelegate.h"

CLAVSplitter::CLAVSplitter(LPUNKNOWN pUnk, HRESULT* phr) 
  : CBaseFilter(NAME("lavf dshow source filter"), pUnk, this,  __uuidof(this), phr)
{
  WCHAR fileName[1024];
  GetModuleFileName(nullptr, fileName, 1024);
  m_processName = PathFindFileName (fileName);

  m_InputFormats.clear();

  std::set<FormatInfo> lavf_formats = CLAVFDemuxer::GetFormatList();
  m_InputFormats.insert(lavf_formats.begin(), lavf_formats.end());

  LoadSettings();

  m_pInput = new CLAVInputPin(NAME("LAV Input Pin"), this, this, phr);

  m_ePlaybackInit.Set();

#ifdef DEBUG
  DbgSetModuleLevel (LOG_TRACE, DWORD_MAX);
  DbgSetModuleLevel (LOG_ERROR, DWORD_MAX);

#ifdef LAV_DEBUG_RELEASE
  DbgSetLogFileDesktop(LAVF_LOG_FILE);
#endif
#endif
}

CLAVSplitter::~CLAVSplitter()
{
  SAFE_DELETE(m_pInput);
  SAFE_DELETE(m_pTrayIcon);
  Close();

  // delete old pins
  for(CLAVOutputPin *pPin : m_pRetiredPins) {
    delete pPin;
  }
  m_pRetiredPins.clear();

  SafeRelease(&m_pSite);

#if defined(DEBUG) && defined(LAV_DEBUG_RELEASE)
  DbgCloseLogFile();
#endif
}

STDMETHODIMP CLAVSplitter::Close()
{
  CAutoLock cAutoLock(this);

  AbortOperation();
  CAMThread::CallWorker(CMD_EXIT);
  CAMThread::Close();

  m_State = State_Stopped;
  DeleteOutputs();

  SafeRelease(&m_pDemuxer);

  return S_OK;
}

STDMETHODIMP CLAVSplitter::CreateTrayIcon()
{
  CAutoLock cObjectLock(m_pLock);
  if (m_pTrayIcon)
    return E_UNEXPECTED;
  if (CBaseTrayIcon::ProcessBlackList())
    return S_FALSE;
  m_pTrayIcon = new CLAVSplitterTrayIcon(this, TEXT(LAV_SPLITTER), IDI_ICON1);
  return S_OK;
}

STDMETHODIMP CLAVSplitter::JoinFilterGraph(IFilterGraph * pGraph, LPCWSTR pName)
{
  CAutoLock cObjectLock(m_pLock);
  HRESULT hr = __super::JoinFilterGraph(pGraph, pName);
  if (pGraph && !m_pTrayIcon && m_settings.TrayIcon) {
    CreateTrayIcon();
  } else if (!pGraph && m_pTrayIcon) {
    SAFE_DELETE(m_pTrayIcon);
  }
  return hr;
}

// Default overrides for input formats
static BOOL get_iformat_default(std::string name)
{
  return TRUE;
}

STDMETHODIMP CLAVSplitter::LoadDefaults()
{
  m_settings.TrayIcon         = FALSE;

  m_settings.prefAudioLangs   = L"";
  m_settings.prefSubLangs     = L"";
  m_settings.subtitleAdvanced = L"";

  m_settings.subtitleMode     = LAVSubtitleMode_Default;
  m_settings.PGSForcedStream  = TRUE;
  m_settings.PGSOnlyForced    = FALSE;

  m_settings.vc1Mode          = 2;
  m_settings.substreams       = TRUE;

  m_settings.MatroskaExternalSegments = TRUE;

  m_settings.StreamSwitchRemoveAudio = FALSE;
  m_settings.ImpairedAudio    = FALSE;
  m_settings.PreferHighQualityAudio = TRUE;
  m_settings.QueueMaxPackets  = 350;
  m_settings.QueueMaxMemSize  = 256;
  m_settings.NetworkAnalysisDuration = 1000;

  for (const FormatInfo& fmt : m_InputFormats) {
    m_settings.formats[std::string(fmt.strName)] = get_iformat_default(fmt.strName);
  }

  return S_OK;
}

STDMETHODIMP CLAVSplitter::LoadSettings()
{
  LoadDefaults();
  if (m_bRuntimeConfig)
    return S_FALSE;

  ReadSettings(HKEY_LOCAL_MACHINE);
  return ReadSettings(HKEY_CURRENT_USER);
}

STDMETHODIMP CLAVSplitter::ReadSettings(HKEY rootKey)
{
  HRESULT hr;
  DWORD dwVal;
  BOOL bFlag;
  std::wstring strVal;

  CRegistry reg = CRegistry(rootKey, LAVF_REGISTRY_KEY, hr, TRUE);
  if (SUCCEEDED(hr)) {
    bFlag = reg.ReadBOOL(L"TrayIcon", hr);
    if (SUCCEEDED(hr)) m_settings.TrayIcon = bFlag;

    // Language preferences
    strVal = reg.ReadString(L"prefAudioLangs", hr);
    if (SUCCEEDED(hr)) m_settings.prefAudioLangs = strVal;

    strVal = reg.ReadString(L"prefSubLangs", hr);
    if (SUCCEEDED(hr)) m_settings.prefSubLangs = strVal;

    strVal = reg.ReadString(L"subtitleAdvanced", hr);
    if (SUCCEEDED(hr)) m_settings.subtitleAdvanced = strVal;

    // Subtitle mode, defaults to all subtitles
    dwVal = reg.ReadDWORD(L"subtitleMode", hr);
    if (SUCCEEDED(hr)) m_settings.subtitleMode = (LAVSubtitleMode)dwVal;

    bFlag = reg.ReadBOOL(L"PGSForcedStream", hr);
    if (SUCCEEDED(hr)) m_settings.PGSForcedStream = bFlag;

    bFlag = reg.ReadBOOL(L"PGSOnlyForced", hr);
    if (SUCCEEDED(hr)) m_settings.PGSOnlyForced = bFlag;

    dwVal = reg.ReadDWORD(L"vc1TimestampMode", hr);
    if (SUCCEEDED(hr)) m_settings.vc1Mode = dwVal;

    bFlag = reg.ReadDWORD(L"substreams", hr);
    if (SUCCEEDED(hr)) m_settings.substreams = bFlag;

    bFlag = reg.ReadDWORD(L"MatroskaExternalSegments", hr);
    if (SUCCEEDED(hr)) m_settings.MatroskaExternalSegments = bFlag;

    bFlag = reg.ReadDWORD(L"StreamSwitchRemoveAudio", hr);
    if (SUCCEEDED(hr)) m_settings.StreamSwitchRemoveAudio = bFlag;

    bFlag = reg.ReadDWORD(L"PreferHighQualityAudio", hr);
    if (SUCCEEDED(hr)) m_settings.PreferHighQualityAudio = bFlag;

    bFlag = reg.ReadDWORD(L"ImpairedAudio", hr);
    if (SUCCEEDED(hr)) m_settings.ImpairedAudio = bFlag;

    dwVal = reg.ReadDWORD(L"QueueMaxSize", hr);
    if (SUCCEEDED(hr)) m_settings.QueueMaxMemSize = dwVal;

    dwVal = reg.ReadDWORD(L"NetworkAnalysisDuration", hr);
    if (SUCCEEDED(hr)) m_settings.NetworkAnalysisDuration = dwVal;

    dwVal = reg.ReadDWORD(L"QueueMaxPackets", hr);
    if (SUCCEEDED(hr)) m_settings.QueueMaxPackets = dwVal;
  }

  CRegistry regF = CRegistry(rootKey, LAVF_REGISTRY_KEY_FORMATS, hr, TRUE);
  if (SUCCEEDED(hr)) {
    WCHAR wBuffer[80];
    for (const FormatInfo& fmt : m_InputFormats) {
      SafeMultiByteToWideChar(CP_UTF8, 0, fmt.strName, -1, wBuffer, 80);
      bFlag = regF.ReadBOOL(wBuffer, hr);
      if (SUCCEEDED(hr)) m_settings.formats[std::string(fmt.strName)] = bFlag;
    }
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
  CreateRegistryKey(HKEY_CURRENT_USER, LAVF_REGISTRY_KEY);
  CRegistry reg = CRegistry(HKEY_CURRENT_USER, LAVF_REGISTRY_KEY, hr);
  if (SUCCEEDED(hr)) {
    reg.WriteBOOL(L"TrayIcon", m_settings.TrayIcon);
    reg.WriteString(L"prefAudioLangs", m_settings.prefAudioLangs.c_str());
    reg.WriteString(L"prefSubLangs", m_settings.prefSubLangs.c_str());
    reg.WriteString(L"subtitleAdvanced", m_settings.subtitleAdvanced.c_str());
    reg.WriteDWORD(L"subtitleMode", m_settings.subtitleMode);
    reg.WriteBOOL(L"PGSForcedStream", m_settings.PGSForcedStream);
    reg.WriteBOOL(L"PGSOnlyForced", m_settings.PGSOnlyForced);
    reg.WriteDWORD(L"vc1TimestampMode", m_settings.vc1Mode);
    reg.WriteBOOL(L"substreams", m_settings.substreams);
    reg.WriteBOOL(L"MatroskaExternalSegments", m_settings.MatroskaExternalSegments);
    reg.WriteBOOL(L"StreamSwitchRemoveAudio", m_settings.StreamSwitchRemoveAudio);
    reg.WriteBOOL(L"PreferHighQualityAudio", m_settings.PreferHighQualityAudio);
    reg.WriteBOOL(L"ImpairedAudio", m_settings.ImpairedAudio);
    reg.WriteDWORD(L"QueueMaxSize", m_settings.QueueMaxMemSize);
    reg.WriteDWORD(L"NetworkAnalysisDuration", m_settings.NetworkAnalysisDuration);
    reg.WriteDWORD(L"QueueMaxPackets", m_settings.QueueMaxPackets);
  }

  CreateRegistryKey(HKEY_CURRENT_USER, LAVF_REGISTRY_KEY_FORMATS);
  CRegistry regF = CRegistry(HKEY_CURRENT_USER, LAVF_REGISTRY_KEY_FORMATS, hr);
  if (SUCCEEDED(hr)) {
    WCHAR wBuffer[80];
    for (const FormatInfo& fmt : m_InputFormats) {
      SafeMultiByteToWideChar(CP_UTF8, 0, fmt.strName, -1, wBuffer, 80);
      regF.WriteBOOL(wBuffer, m_settings.formats[std::string(fmt.strName)]);
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

  *ppv = nullptr;

  if (m_pDemuxer && (riid == __uuidof(IKeyFrameInfo) || riid == __uuidof(ITrackInfo) || riid == IID_IAMExtendedSeeking || riid == IID_IAMMediaContent || riid == IID_IPropertyBag || riid == __uuidof(IDSMResourceBag))) {
    return m_pDemuxer->QueryInterface(riid, ppv);
  }

  return
    QI(IMediaSeeking)
    QI(IAMStreamSelect)
    QI(ISpecifyPropertyPages)
    QI(ISpecifyPropertyPages2)
    QI2(ILAVFSettings)
    QI2(ILAVFSettingsInternal)
    QI(IObjectWithSite)
    QI(IBufferInfo)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

// ISpecifyPropertyPages2
STDMETHODIMP CLAVSplitter::GetPages(CAUUID *pPages)
{
  CheckPointer(pPages, E_POINTER);
  pPages->cElems = 2;
  pPages->pElems = (GUID *)CoTaskMemAlloc(sizeof(GUID) * pPages->cElems);
  if (pPages->pElems == nullptr) {
    return E_OUTOFMEMORY;
  }
  pPages->pElems[0] = CLSID_LAVSplitterSettingsProp;
  pPages->pElems[1] = CLSID_LAVSplitterFormatsProp;
  return S_OK;
}

STDMETHODIMP CLAVSplitter::CreatePage(const GUID& guid, IPropertyPage** ppPage)
{
  CheckPointer(ppPage, E_POINTER);
  HRESULT hr = S_OK;

  if (*ppPage != nullptr)
    return E_INVALIDARG;

  if (guid == CLSID_LAVSplitterSettingsProp)
    *ppPage = new CLAVSplitterSettingsProp(nullptr, &hr);
  else if (guid == CLSID_LAVSplitterFormatsProp)
    *ppPage = new CLAVSplitterFormatsProp(nullptr, &hr);

  if (SUCCEEDED(hr) && *ppPage) {
    (*ppPage)->AddRef();
    return S_OK;
  } else {
    SAFE_DELETE(*ppPage);
    return E_FAIL;
  }
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
  *ppvSite = nullptr;
  if (!m_pSite) {
    return E_FAIL;
  }

  IUnknown *pSite = nullptr;
  HRESULT hr = m_pSite->QueryInterface(riid, (void **)&pSite);
  if (SUCCEEDED(hr) && pSite) {
    pSite->AddRef();
    *ppvSite = pSite;
    return S_OK;
  }
  return E_NOINTERFACE;
}

// IBufferInfo
STDMETHODIMP_(int) CLAVSplitter::GetCount()
{
  CAutoLock pinLock(&m_csPins);
  return (int)m_pPins.size();
}

STDMETHODIMP CLAVSplitter::GetStatus(int i, int& samples, int& size)
{
  CAutoLock pinLock(&m_csPins);
  if ((size_t)i >= m_pPins.size())
    return E_FAIL;

  CLAVOutputPin *pPin = m_pPins.at(i);
  if (!pPin)
    return E_FAIL;
  return pPin->GetQueueSize(samples, size);
}

STDMETHODIMP_(DWORD) CLAVSplitter::GetPriority()
{
  return 0;
}

// IAMOpenProgress

STDMETHODIMP CLAVSplitter::QueryProgress(LONGLONG *pllTotal, LONGLONG *pllCurrent)
{
  return E_NOTIMPL;
}

STDMETHODIMP CLAVSplitter::AbortOperation()
{
  if (m_pDemuxer)
    return m_pDemuxer->AbortOpening();
  else
    return E_UNEXPECTED;
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

  if (n < 0 ||n >= GetPinCount()) return nullptr;

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

  auto &vec = bActiveOnly ? m_pActivePins : m_pPins;
  for(CLAVOutputPin *pPin : vec) {
    if (pPin->GetStreamId() == streamId) {
      return pPin;
    }
  }
  return nullptr;
}

STDMETHODIMP CLAVSplitter::CompleteInputConnection()
{
  HRESULT hr = S_OK;
  BOOL bFileInput = FALSE;

  // Check if blacklisted
  if (!m_bRuntimeConfig && CheckApplicationBlackList(LAVF_REGISTRY_KEY L"\\Blacklist"))
    return E_FAIL;

  SAFE_DELETE(m_pDemuxer);

  AVIOContext *pContext = nullptr;

  if (FAILED(hr = m_pInput->GetAVIOContext(&pContext))) {
    return hr;
  }

  LPOLESTR pszFileName = nullptr;

  PIN_INFO info;
  hr = m_pInput->GetConnected()->QueryPinInfo(&info);
  if (SUCCEEDED(hr) && info.pFilter) {
    IFileSourceFilter *pSource = nullptr;
    if (SUCCEEDED(info.pFilter->QueryInterface(&pSource)) && pSource) {
      pSource->GetCurFile(&pszFileName, nullptr);
      SafeRelease(&pSource);
    }
    CLSID inputCLSID;
    if (SUCCEEDED(info.pFilter->GetClassID(&inputCLSID))) {
      bFileInput = (inputCLSID == CLSID_AsyncReader);
    }
    SafeRelease(&info.pFilter);
  }

  const char *format = nullptr;
  if (m_pInput->CurrentMediaType().subtype == MEDIASUBTYPE_MPEG2_TRANSPORT) {
    format = "mpegts";
  }

  CLAVFDemuxer *pDemux = new CLAVFDemuxer(this, this);
  if(FAILED(hr = pDemux->OpenInputStream(pContext, pszFileName, format, FALSE, bFileInput))) {
    SAFE_DELETE(pDemux);
    return hr;
  }
  m_pDemuxer = pDemux;
  m_pDemuxer->AddRef();

  SAFE_CO_FREE(pszFileName);

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
  if (m_State != State_Stopped) return E_UNEXPECTED;

  // Check if blacklisted
  if (!m_bRuntimeConfig && CheckApplicationBlackList(LAVF_REGISTRY_KEY L"\\Blacklist"))
    return E_FAIL;

  // Close, just in case we're being re-used
  Close();

  m_fileName = std::wstring(pszFileName);

  HRESULT hr = S_OK;
  SAFE_DELETE(m_pDemuxer);
  LPWSTR extension = PathFindExtensionW(pszFileName);

  DbgLog((LOG_TRACE, 10, L"::Load(): Opening file '%s' (extension: %s)", pszFileName, extension));

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

  // Disable subtitles in applications known to fail with them (explorer thumbnail generator, power point, basically all applications using MCI)
  bool bNoSubtitles = _wcsicmp(m_processName.c_str(), L"dllhost.exe") == 0 || _wcsicmp(m_processName.c_str(), L"explorer.exe") == 0 || _wcsicmp(m_processName.c_str(), L"powerpnt.exe") == 0 || _wcsicmp(m_processName.c_str(), L"pptview.exe") == 0;

  m_rtStart = m_rtNewStart = m_rtCurrent = 0;
  m_rtStop = m_rtNewStop = m_pDemuxer->GetDuration();
  m_bPlaybackStarted = FALSE;

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

  std::string audioLanguage = audioStream ? audioStream->language : std::string();

  std::list<CSubtitleSelector> subtitleSelectors = GetSubtitleSelectors();
  const CBaseDemuxer::stream *subtitleStream = m_pDemuxer->SelectSubtitleStream(subtitleSelectors, audioLanguage);
  if (subtitleStream && !bNoSubtitles) {
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
  for(CLAVOutputPin *pPin : m_pPins) {
    if(IPin* pPinTo = pPin->GetConnected()) pPinTo->Disconnect();
    pPin->Disconnect();
    m_pRetiredPins.push_back(pPin);
  }
  m_pPins.clear();

  return S_OK;
}

bool CLAVSplitter::IsAnyPinDrying()
{
  // MPC changes thread priority here
  // TODO: Investigate if that is needed
  for(CLAVOutputPin *pPin : m_pActivePins) {
    if(pPin->IsConnected() && !pPin->IsDiscontinuous() && pPin->QueueCount() < pPin->GetQueueLowLimit()) {
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

  m_pDemuxer->Start();

  m_fFlushing = false;
  m_eEndFlush.Set();
  for(DWORD cmd = (DWORD)-1; ; cmd = GetRequest())
  {
    if(cmd == CMD_EXIT)
    {
      Reply(S_OK);
      m_ePlaybackInit.Set();
      return 0;
    }

    m_ePlaybackInit.Reset();

    m_rtStart = m_rtNewStart;
    m_rtStop = m_rtNewStop;

    if(m_bPlaybackStarted || m_rtStart != 0 || cmd == CMD_SEEK) {
      HRESULT hr = S_FALSE;
      if (m_pInput) {
        hr = m_pInput->SeekStream(m_rtStart);
        if (SUCCEEDED(hr))
          m_pDemuxer->Reset();
      }
      if (hr != S_OK)
        DemuxSeek(m_rtStart);
    }

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
    m_rtOffset = AV_NOPTS_VALUE;

    m_bDiscontinuitySent.clear();

    m_bPlaybackStarted = TRUE;
    m_ePlaybackInit.Set();

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
    m_rtCurrent = pPacket->rtStop;

    if (m_bStopValid && m_rtStop && pPacket->rtStart > m_rtStop) {
      DbgLog((LOG_TRACE, 10, L"::DeliverPacket(): Reached the designated stop time of %I64d at %I64d", m_rtStop, pPacket->rtStart));
      delete pPacket;
      return E_FAIL;
    }

    pPacket->rtStart -= m_rtStart;
    pPacket->rtStop -= m_rtStart;

    ASSERT(pPacket->rtStart <= pPacket->rtStop);

    // Filter PTS values
    // This will try to compensate for timestamp discontinuities in the stream
    if (m_pDemuxer->GetContainerFlags() & LAVFMT_TS_DISCONT) {
      if (!pPin->IsSubtitlePin()) {
        // Initialize on the first stream coming in
        if (pPin->m_rtPrev == AV_NOPTS_VALUE && m_rtOffset == AV_NOPTS_VALUE) {
          pPin->m_rtPrev = 0;
          m_rtOffset = 0;
        }

        REFERENCE_TIME rt = pPacket->rtStart + m_rtOffset;

        if(pPin->m_rtPrev != AV_NOPTS_VALUE && _abs64(rt - pPin->m_rtPrev) > MAX_PTS_SHIFT) {
          m_rtOffset += pPin->m_rtPrev - rt;
          if (!(m_pDemuxer->GetContainerFlags() & LAVFMT_TS_DISCONT_NO_DOWNSTREAM))
            m_bDiscontinuitySent.clear();
          DbgLog((LOG_TRACE, 10, L"::DeliverPacket(): MPEG-TS/PS discontinuity detected, adjusting offset to %I64d (stream: %d, prev: %I64d, now: %I64d)", m_rtOffset, pPacket->StreamId, pPin->m_rtPrev, rt));
        }
      }
      pPacket->rtStart += m_rtOffset;
      pPacket->rtStop += m_rtOffset;

      pPin->m_rtPrev = pPacket->rtStart;
    }

    pPacket->rtStart = (REFERENCE_TIME)(pPacket->rtStart / m_dRate);
    pPacket->rtStop = (REFERENCE_TIME)(pPacket->rtStop / m_dRate);
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

STDMETHODIMP_(CMediaType *) CLAVSplitter::GetOutputMediatype(int stream)
{
  CLAVOutputPin* pPin = GetOutputPin(stream, FALSE);
  if (!pPin || !pPin->IsConnected())
    return nullptr;

  CMediaType *pmt = new CMediaType(pPin->GetActiveMediaType());
  return pmt;
}

// State Control
STDMETHODIMP CLAVSplitter::Stop()
{
  CAutoLock cAutoLock(this);

  // Wait for playback to finish initializing
  m_ePlaybackInit.Wait();

  // Ask network operations to exit
  if (m_pDemuxer)
    m_pDemuxer->AbortOpening(1);

  DeliverBeginFlush();
  CAMThread::CallWorker(CMD_EXIT);
  CAMThread::Close();
  DeliverEndFlush();

  if (m_pDemuxer)
    m_pDemuxer->AbortOpening(0);

  HRESULT hr;
  if(FAILED(hr = __super::Stop())) {
    return hr;
  }

  return S_OK;
}

STDMETHODIMP CLAVSplitter::Pause()
{
  CAutoLock cAutoLock(this);
  CheckPointer(m_pDemuxer, E_UNEXPECTED);

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
    if (!ThreadExists())
      m_ePlaybackInit.Reset();
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
  for(CLAVOutputPin *pPin : m_pPins) {
    pPin->DeliverBeginFlush();
  }
}

void CLAVSplitter::DeliverEndFlush()
{
  // flush all pins
  for(CLAVOutputPin *pPin : m_pPins) {
    pPin->DeliverEndFlush();
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

STDMETHODIMP CLAVSplitter::GetDuration(LONGLONG* pDuration) {
  REFERENCE_TIME rtDuration = -1;
  CheckPointer(pDuration, E_POINTER);
  CheckPointer(m_pDemuxer, E_UNEXPECTED);

  if (m_pInput) {
    if (FAILED(m_pInput->GetStreamDuration(&rtDuration)))
      rtDuration = -1;
  }

  if (rtDuration < 0)
    rtDuration = m_pDemuxer->GetDuration();

  if (rtDuration < 0)
      return E_FAIL;

  *pDuration = rtDuration;
  return S_OK;
}

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
    case AM_SEEKING_AbsolutePositioning: rtStop = *pStop; m_bStopValid = TRUE; break;
    case AM_SEEKING_RelativePositioning: rtStop += *pStop; m_bStopValid = TRUE; break;
    case AM_SEEKING_IncrementalPositioning: rtStop = rtCurrent + *pStop; m_bStopValid = TRUE; break;
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
    CBaseDemuxer::CStreamList *streams = m_pDemuxer->GetStreams(CBaseDemuxer::subpic);
    const CBaseDemuxer::stream *s = streams->FindStream(FORCED_SUBTITLE_PID);
    CMediaType *mt = new CMediaType(s->streamInfo->mtypes.back());
    pPin->SendMediaType(mt);
  }

  return S_OK;
}

static int QueryAcceptMediaTypes(IPin *pPin, std::deque<CMediaType> pmts)
{
  for(unsigned int i = 0; i < pmts.size(); i++) {
    if (S_OK == pPin->QueryAccept(&pmts[i])) {
      DbgLog((LOG_TRACE, 20, L"QueryAcceptMediaTypes() - IPin:QueryAccept succeeded on index %d", i));
      return i;
    }
  }
  return -1;
}

STDMETHODIMP CLAVSplitter::RenameOutputPin(DWORD TrackNumSrc, DWORD TrackNumDst, std::deque<CMediaType> pmts)
{
  CheckPointer(m_pDemuxer, E_UNEXPECTED);
  if (TrackNumSrc == TrackNumDst) return S_OK;

  // WMP/WMC like to always enable the first track, overwriting any initial stream choice
  // So instead block it from doing anything here.
  if (!m_bPlaybackStarted && (m_processName == L"wmplayer.exe" || m_processName == L"ehshell.exe"))
    return S_OK;

  CLAVOutputPin* pPin = GetOutputPin(TrackNumSrc);

  DbgLog((LOG_TRACE, 20, L"::RenameOutputPin() - Switching %s Stream %d to %d", CBaseDemuxer::CStreamList::ToStringW(pPin->GetPinType()), TrackNumSrc, TrackNumDst));
  // Output Pin was found
  // Stop the Graph, remove the old filter, render the graph again, start it up again
  // This only works on pins that were connected before, or the filter graph could .. well, break
  if (pPin && pPin->IsConnected()) {
    HRESULT hr = S_OK;

    IMediaControl *pControl = nullptr;
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
    IGraphRebuildDelegate *pDelegate = nullptr;
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
        IGraphBuilder *pGraphBuilder = nullptr;
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

  int num_titles = m_pDemuxer->GetNumTitles();
  if (num_titles > 1)
    *pcStreams += num_titles;

  return S_OK;
}

STDMETHODIMP CLAVSplitter::Enable(long lIndex, DWORD dwFlags)
{
  CheckPointer(m_pDemuxer, E_UNEXPECTED);
  if(!(dwFlags & AMSTREAMSELECTENABLE_ENABLE)) {
    return E_NOTIMPL;
  }

  int i, j;
  for(i = 0, j = 0; i < CBaseDemuxer::unknown; i++) {
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
  int idx = (lIndex - j);
  int num_titles = m_pDemuxer->GetNumTitles();
  if (num_titles > 1 && idx >= 0 && idx < num_titles) {
    DbgLog((LOG_TRACE, 10, L"Setting title to %d", idx));
    HRESULT hr = m_pDemuxer->SetTitle(idx);
    if (SUCCEEDED(hr)) {
      // Perform a seek to the start of the new title
      IMediaSeeking *pSeek = nullptr;
      hr = m_pGraph->QueryInterface(&pSeek);
      if (SUCCEEDED(hr)) {
        LONGLONG current = 0;
        pSeek->SetPositions(&current, AM_SEEKING_AbsolutePositioning, nullptr, AM_SEEKING_NoPositioning);
        SafeRelease(&pSeek);
      }
      // Notify the player about the length change
      DbgLog((LOG_TRACE, 10, L"Title change complete, signaling player"));
      NotifyEvent(EC_LENGTH_CHANGED, 0, 0);
    }
  }
  return S_FALSE;
}

STDMETHODIMP CLAVSplitter::Info(long lIndex, AM_MEDIA_TYPE **ppmt, DWORD *pdwFlags, LCID *plcid, DWORD *pdwGroup, WCHAR **ppszName, IUnknown **ppObject, IUnknown **ppUnk)
{
  CheckPointer(m_pDemuxer, E_UNEXPECTED);
  HRESULT hr = S_FALSE;
  int i, j;
  for(i = 0, j = 0; i < CBaseDemuxer::unknown; i++) {
    CBaseDemuxer::CStreamList *streams = m_pDemuxer->GetStreams((CBaseDemuxer::StreamType)i);
    int cnt = (int)streams->size();

    if(lIndex >= j && lIndex < j+cnt) {
      long idx = (lIndex - j);

      CBaseDemuxer::stream& s = streams->at(idx);

      if(ppmt) *ppmt = CreateMediaType(&s.streamInfo->mtypes[0]);
      if(pdwFlags) *pdwFlags = GetOutputPin(s) ? (AMSTREAMSELECTINFO_ENABLED|AMSTREAMSELECTINFO_EXCLUSIVE) : 0;
      if(pdwGroup) *pdwGroup = i;
      if(ppObject) *ppObject = nullptr;
      if(ppUnk) *ppUnk = nullptr;

      // Special case for the "no subtitles" pin
      if(s.pid == NO_SUBTITLE_PID) {
        if (plcid) *plcid = LCID_NOSUBTITLES;
        if (ppszName) {
          WCHAR str[] = L"S: " NO_SUB_STRING;
          size_t len = wcslen(str) + 1;
          *ppszName = (WCHAR*)CoTaskMemAlloc(len * sizeof(WCHAR));
          if (*ppszName)
            wcsncpy_s(*ppszName, len, str, _TRUNCATE);
        }
      } else if (s.pid == FORCED_SUBTITLE_PID) {
        if (plcid) {
          *plcid = s.lcid;
        }
        if (ppszName) {
          WCHAR str[] = L"S: " FORCED_SUB_STRING;
          size_t len = wcslen(str) + 1;
          *ppszName = (WCHAR*)CoTaskMemAlloc(len * sizeof(WCHAR));
          if (*ppszName)
            wcsncpy_s(*ppszName, len, str, _TRUNCATE);
        }
      } else {
        if (plcid) {
          *plcid = s.lcid;
        }
        if (ppszName) {
          std::string info = s.streamInfo->codecInfo;
          *ppszName = CoTaskGetWideCharFromMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS, info.c_str(), -1);
        }
      }
      hr = S_OK;
      break;
    }
    j += cnt;
  }

  if (hr == S_FALSE) {
    int idx = (lIndex - j);
    int num_titles = m_pDemuxer->GetNumTitles();
    if (num_titles > 1 && idx >= 0 && idx < num_titles) {
      if(ppmt) *ppmt = nullptr;
      if(pdwFlags) *pdwFlags = m_pDemuxer->GetTitle() == idx ? (AMSTREAMSELECTINFO_ENABLED|AMSTREAMSELECTINFO_EXCLUSIVE) : 0;
      if(pdwGroup) *pdwGroup = 18;
      if(ppObject) *ppObject = nullptr;
      if(ppUnk) *ppUnk = nullptr;
      m_pDemuxer->GetTitleInfo(idx, nullptr, ppszName);

      hr = S_OK;
    }
  }

  return hr;
}

// setting helpers
std::list<std::string> CLAVSplitter::GetPreferredAudioLanguageList()
{
  std::list<std::string> list;

  char *buffer = CoTaskGetMultiByteFromWideChar(CP_UTF8, 0, m_settings.prefAudioLangs.c_str(), -1);
  if (!buffer)
    return list;

  split(std::string(buffer), std::string(",; "), list);
  SAFE_CO_FREE(buffer);

  return list;
}

std::list<CSubtitleSelector> CLAVSplitter::GetSubtitleSelectors()
{
  std::list<CSubtitleSelector> selectorList;

  std::string separators = ",; ";
  std::list<std::string> tokenList;

  if (m_settings.subtitleMode == LAVSubtitleMode_NoSubs) {
    // Do nothing
  } else if (m_settings.subtitleMode == LAVSubtitleMode_Default || m_settings.subtitleMode == LAVSubtitleMode_ForcedOnly) {
    // Convert to wide-char to utf8
    char *buffer = CoTaskGetMultiByteFromWideChar(CP_UTF8, 0, m_settings.prefSubLangs.c_str(), -1);
    if (!buffer)
      return selectorList;

    std::list<std::string> langList;
    split(std::string(buffer), separators, langList);
    SAFE_CO_FREE(buffer);

    // If no languages have been set, prefer the forced/default streams as specified by the audio languages
    bool bNoLanguage = false;
    if (langList.empty()) {
      langList = GetPreferredAudioLanguageList();
      bNoLanguage = true;
    }

    for (const std::string& lang : langList) {
      std::string token = "*:" + lang;
      if (m_settings.subtitleMode == LAVSubtitleMode_ForcedOnly || bNoLanguage) {
        tokenList.push_back(token + "|f");
        if (m_settings.subtitleMode == LAVSubtitleMode_Default)
          tokenList.push_back(token + "|d");
      } else {
        tokenList.push_back(token + "|d");
        tokenList.push_back(token + "|!h");
      }
    }

    // Add fallbacks (forced/default)
    tokenList.push_back("*:*|f");
    if (m_settings.subtitleMode == LAVSubtitleMode_Default)
      tokenList.push_back("*:*|d");
  } else if (m_settings.subtitleMode == LAVSubtitleMode_Advanced) {
    // Convert to wide-char to utf8
    char *buffer = CoTaskGetMultiByteFromWideChar(CP_UTF8, 0, m_settings.subtitleAdvanced.c_str(), -1);
    if (!buffer)
      return selectorList;

    split(std::string(buffer), separators, tokenList);
    SAFE_CO_FREE(buffer);
  }

  // Add the "off" termination element
  tokenList.push_back("*:off");

  std::regex advRegex(
                      "(?:(\\*|[[:alpha:]]+):)?"        // audio language
                      "(\\*|[[:alpha:]]+)"              // subtitle language
                      "(?:\\|(!?)([fdnh]+))?"           // flags
                      "(?:@([^" + separators + "]+))?"  // subtitle track name substring
                    );
  for (const std::string& token : tokenList) {
    std::cmatch res;
    bool found = std::regex_search(token.c_str(), res, advRegex);
    if (found) {
      CSubtitleSelector selector;
      selector.audioLanguage = res[1].str().empty() ? "*" : ProbeForISO6392(res[1].str().c_str());
      selector.subtitleLanguage = ProbeForISO6392(res[2].str().c_str());
      selector.dwFlags = 0;

      // Parse flags
      std::string flags = res[4];
      if (flags.length() > 0) {
        if (flags.find('d') != flags.npos)
          selector.dwFlags |= SUBTITLE_FLAG_DEFAULT;
        if (flags.find('f') != flags.npos)
          selector.dwFlags |= SUBTITLE_FLAG_FORCED;
        if (flags.find('n') != flags.npos)
          selector.dwFlags |= SUBTITLE_FLAG_NORMAL;
        if (flags.find('h') != flags.npos)
          selector.dwFlags |= SUBTITLE_FLAG_IMPAIRED;

        if (m_settings.subtitleMode == LAVSubtitleMode_Default) {
          if (selector.subtitleLanguage == "*" && (selector.dwFlags & SUBTITLE_FLAG_DEFAULT))
            selector.dwFlags |= SUBTITLE_FLAG_VIRTUAL;
        } else {
          if (selector.dwFlags & SUBTITLE_FLAG_FORCED)
            selector.dwFlags |= SUBTITLE_FLAG_VIRTUAL;
        }

        // Check for flag negation
        std::string not = res[3];
        if (not.length() == 1 && not == "!") {
          selector.dwFlags = (~selector.dwFlags) & 0xFF;
        }
      }

      selector.subtitleTrackName = res[5];

      selectorList.push_back(selector);
      DbgLog((LOG_TRACE, 10, L"::GetSubtitleSelectors(): Parsed selector \"%S\" to: %S -> %S (flags: 0x%x, match: %S)", token.c_str(), selector.audioLanguage.c_str(), selector.subtitleLanguage.c_str(), selector.dwFlags, selector.subtitleTrackName.c_str()));
    } else {
      DbgLog((LOG_ERROR, 10, L"::GetSubtitleSelectors(): Selector string \"%S\" could not be parsed", token.c_str()));
    }
  }

  return selectorList;
}

// Settings
// ILAVAudioSettings
HRESULT CLAVSplitter::SetRuntimeConfig(BOOL bRuntimeConfig)
{
  m_bRuntimeConfig = bRuntimeConfig;
  LoadSettings();

  // Tray Icon is disabled by default
  SAFE_DELETE(m_pTrayIcon);

  return S_OK;
}


STDMETHODIMP CLAVSplitter::GetPreferredLanguages(LPWSTR *ppLanguages)
{
  CheckPointer(ppLanguages, E_POINTER);
  size_t len = m_settings.prefAudioLangs.length() + 1;
  if (len > 1) {
    *ppLanguages = (WCHAR *)CoTaskMemAlloc(sizeof(WCHAR) * len);
    if (*ppLanguages)
      wcsncpy_s(*ppLanguages, len,  m_settings.prefAudioLangs.c_str(), _TRUNCATE);
  } else {
    *ppLanguages = nullptr;
  }
  return S_OK;
}

STDMETHODIMP CLAVSplitter::SetPreferredLanguages(LPCWSTR pLanguages)
{
  m_settings.prefAudioLangs = std::wstring(pLanguages);
  return SaveSettings();
}

STDMETHODIMP CLAVSplitter::GetPreferredSubtitleLanguages(LPWSTR *ppLanguages)
{
  CheckPointer(ppLanguages, E_POINTER);
  size_t len = m_settings.prefSubLangs.length() + 1;
  if (len > 1) {
    *ppLanguages = (WCHAR *)CoTaskMemAlloc(sizeof(WCHAR) * len);
    if (*ppLanguages)
      wcsncpy_s(*ppLanguages, len,  m_settings.prefSubLangs.c_str(), _TRUNCATE);
  } else {
    *ppLanguages = nullptr;
  }
  return S_OK;
}

STDMETHODIMP CLAVSplitter::SetPreferredSubtitleLanguages(LPCWSTR pLanguages)
{
  m_settings.prefSubLangs = std::wstring(pLanguages);
  return SaveSettings();
}

STDMETHODIMP_(LAVSubtitleMode) CLAVSplitter::GetSubtitleMode()
{
  return m_settings.subtitleMode;
}

STDMETHODIMP CLAVSplitter::SetSubtitleMode(LAVSubtitleMode mode)
{
  m_settings.subtitleMode = mode;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVSplitter::GetSubtitleMatchingLanguage()
{
  return FALSE;
}

STDMETHODIMP CLAVSplitter::SetSubtitleMatchingLanguage(BOOL dwMode)
{
  return E_FAIL;
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
  return E_FAIL;
}

STDMETHODIMP_(BOOL) CLAVSplitter::GetVideoParsingEnabled()
{
  return TRUE;
}

STDMETHODIMP CLAVSplitter::SetFixBrokenHDPVR(BOOL bEnabled)
{
  return E_FAIL;
}

STDMETHODIMP_(BOOL) CLAVSplitter::GetFixBrokenHDPVR()
{
  return TRUE;
}

STDMETHODIMP_(BOOL) CLAVSplitter::IsFormatEnabled(LPCSTR strFormat)
{
  std::string format(strFormat);
  if (m_settings.formats.find(format) != m_settings.formats.end()) {
    return m_settings.formats[format];
  }
  return FALSE;
}

STDMETHODIMP_(HRESULT) CLAVSplitter::SetFormatEnabled(LPCSTR strFormat, BOOL bEnabled)
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

STDMETHODIMP CLAVSplitter::GetAdvancedSubtitleConfig(LPWSTR *ppAdvancedConfig)
{
  CheckPointer(ppAdvancedConfig, E_POINTER);
  size_t len = m_settings.subtitleAdvanced.length() + 1;
  if (len > 1) {
    *ppAdvancedConfig = (WCHAR *)CoTaskMemAlloc(sizeof(WCHAR) * len);
    if (*ppAdvancedConfig)
      wcsncpy_s(*ppAdvancedConfig, len,  m_settings.subtitleAdvanced.c_str(), _TRUNCATE);
  } else {
    *ppAdvancedConfig = nullptr;
  }
  return S_OK;
}

STDMETHODIMP CLAVSplitter::SetAdvancedSubtitleConfig(LPCWSTR pAdvancedConfig)
{
  m_settings.subtitleAdvanced = std::wstring(pAdvancedConfig);
  return SaveSettings();
}

STDMETHODIMP CLAVSplitter::SetUseAudioForHearingVisuallyImpaired(BOOL bEnabled)
{
  m_settings.ImpairedAudio = bEnabled;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVSplitter::GetUseAudioForHearingVisuallyImpaired()
{
  return m_settings.ImpairedAudio;
}

STDMETHODIMP CLAVSplitter::SetMaxQueueMemSize(DWORD dwMaxSize)
{
  m_settings.QueueMaxMemSize = dwMaxSize;
  for(auto it = m_pPins.begin(); it != m_pPins.end(); it++) {
    (*it)->SetQueueSizes();
  }
  return SaveSettings();
}

STDMETHODIMP_(DWORD) CLAVSplitter::GetMaxQueueMemSize()
{
  return m_settings.QueueMaxMemSize;
}

STDMETHODIMP CLAVSplitter::SetTrayIcon(BOOL bEnabled)
{
  m_settings.TrayIcon = bEnabled;
  // The tray icon is created if not yet done so, however its not removed on the fly
  // Removing the icon on the fly can cause deadlocks if the config is changed from the icons thread
  if (bEnabled && m_pGraph && !m_pTrayIcon) {
    CreateTrayIcon();
  }
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVSplitter::GetTrayIcon()
{
  return m_settings.TrayIcon;
}

STDMETHODIMP CLAVSplitter::SetPreferHighQualityAudioStreams(BOOL bEnabled)
{
  m_settings.PreferHighQualityAudio = bEnabled;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVSplitter::GetPreferHighQualityAudioStreams()
{
  return m_settings.PreferHighQualityAudio;
}

STDMETHODIMP CLAVSplitter::SetLoadMatroskaExternalSegments(BOOL bEnabled)
{
  m_settings.MatroskaExternalSegments = bEnabled;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVSplitter::GetLoadMatroskaExternalSegments()
{
  return m_settings.MatroskaExternalSegments;
}

STDMETHODIMP CLAVSplitter::GetFormats(LPSTR** formats, UINT* nFormats)
{
  CheckPointer(formats, E_POINTER);
  CheckPointer(nFormats, E_POINTER);

  *nFormats = (UINT)m_InputFormats.size();

  *formats = (LPSTR*)CoTaskMemAlloc(sizeof(LPSTR) * *nFormats);
  CheckPointer(*formats, E_OUTOFMEMORY);

  size_t i = 0;
  for (auto it = m_InputFormats.begin(); it != m_InputFormats.end(); it++, i++) {
    size_t len = strlen(it->strName) + 1;
    (*formats)[i] = (LPSTR)CoTaskMemAlloc(sizeof(CHAR) * len);
    if (!(*formats)[i]) {
      break;
    }
    strcpy_s((*formats)[i], len, it->strName);
  }

  if (i != *nFormats) {
    for (size_t j = 0; j < i; j++) {
      CoTaskMemFree((*formats)[i]);
    }
    CoTaskMemFree(*formats);
    *formats = nullptr;

    return E_OUTOFMEMORY;
  }

  return S_OK;
}

STDMETHODIMP CLAVSplitter::SetNetworkStreamAnalysisDuration(DWORD dwDuration)
{
  m_settings.NetworkAnalysisDuration = dwDuration;
  return SaveSettings();
}

STDMETHODIMP_(DWORD) CLAVSplitter::GetNetworkStreamAnalysisDuration()
{
  return m_settings.NetworkAnalysisDuration;
}

STDMETHODIMP CLAVSplitter::SetMaxQueueSize(DWORD dwMaxSize)
{
  m_settings.QueueMaxPackets = dwMaxSize;
  for (auto it = m_pPins.begin(); it != m_pPins.end(); it++) {
    (*it)->SetQueueSizes();
  }
  return SaveSettings();
}

STDMETHODIMP_(DWORD) CLAVSplitter::GetMaxQueueSize()
{
  return m_settings.QueueMaxPackets;
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

  *ppv = nullptr;

  return
    QI(IFileSourceFilter)
    QI(IAMOpenProgress)
    __super::NonDelegatingQueryInterface(riid, ppv);
}
