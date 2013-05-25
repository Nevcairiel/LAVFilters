/*
 *      Copyright (C) 2010-2013 Hendrik Leppkes
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

#pragma once

#include <string>
#include <list>
#include <set>
#include <vector>
#include <map>
#include "PacketQueue.h"

#include "BaseDemuxer.h"

#include "LAVSplitterSettingsInternal.h"
#include "SettingsProp.h"
#include "IBufferInfo.h"

#include "ISpecifyPropertyPages2.h"

#include "LAVSplitterTrayIcon.h"

#define LAVF_REGISTRY_KEY L"Software\\LAV\\Splitter"
#define LAVF_REGISTRY_KEY_FORMATS LAVF_REGISTRY_KEY L"\\Formats"
#define LAVF_LOG_FILE     L"LAVSplitter.txt"

#define MAX_PTS_SHIFT 50000000i64

class CLAVOutputPin;
class CLAVInputPin;

#ifdef	_MSC_VER
#pragma warning(disable: 4355)
#endif

[uuid("171252A0-8820-4AFE-9DF8-5C92B2D66B04")]
class CLAVSplitter 
  : public CBaseFilter
  , public CCritSec
  , protected CAMThread
  , public IFileSourceFilter
  , public IMediaSeeking
  , public IAMStreamSelect
  , public IAMOpenProgress
  , public ILAVFSettingsInternal
  , public ISpecifyPropertyPages2
  , public IObjectWithSite
  , public IBufferInfo
{
public:
  CLAVSplitter(LPUNKNOWN pUnk, HRESULT* phr);
  virtual ~CLAVSplitter();

  static void CALLBACK StaticInit(BOOL bLoading, const CLSID *clsid);

  // IUnknown
  DECLARE_IUNKNOWN;
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  // CBaseFilter methods
  int GetPinCount();
  CBasePin *GetPin(int n);
  STDMETHODIMP GetClassID(CLSID* pClsID);

  STDMETHODIMP Stop();
  STDMETHODIMP Pause();
  STDMETHODIMP Run(REFERENCE_TIME tStart);

  STDMETHODIMP JoinFilterGraph(IFilterGraph * pGraph, LPCWSTR pName);

  // IFileSourceFilter
  STDMETHODIMP Load(LPCOLESTR pszFileName, const AM_MEDIA_TYPE * pmt);
  STDMETHODIMP GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt);

  // IMediaSeeking
  STDMETHODIMP GetCapabilities(DWORD* pCapabilities);
  STDMETHODIMP CheckCapabilities(DWORD* pCapabilities);
  STDMETHODIMP IsFormatSupported(const GUID* pFormat);
  STDMETHODIMP QueryPreferredFormat(GUID* pFormat);
  STDMETHODIMP GetTimeFormat(GUID* pFormat);
  STDMETHODIMP IsUsingTimeFormat(const GUID* pFormat);
  STDMETHODIMP SetTimeFormat(const GUID* pFormat);
  STDMETHODIMP GetDuration(LONGLONG* pDuration);
  STDMETHODIMP GetStopPosition(LONGLONG* pStop);
  STDMETHODIMP GetCurrentPosition(LONGLONG* pCurrent);
  STDMETHODIMP ConvertTimeFormat(LONGLONG* pTarget, const GUID* pTargetFormat, LONGLONG Source, const GUID* pSourceFormat);
  STDMETHODIMP SetPositions(LONGLONG* pCurrent, DWORD dwCurrentFlags, LONGLONG* pStop, DWORD dwStopFlags);
  STDMETHODIMP GetPositions(LONGLONG* pCurrent, LONGLONG* pStop);
  STDMETHODIMP GetAvailable(LONGLONG* pEarliest, LONGLONG* pLatest);
  STDMETHODIMP SetRate(double dRate);
  STDMETHODIMP GetRate(double* pdRate);
  STDMETHODIMP GetPreroll(LONGLONG* pllPreroll);

  // IAMStreamSelect
  STDMETHODIMP Count(DWORD *pcStreams);
  STDMETHODIMP Enable(long lIndex, DWORD dwFlags);
  STDMETHODIMP Info(long lIndex, AM_MEDIA_TYPE **ppmt, DWORD *pdwFlags, LCID *plcid, DWORD *pdwGroup, WCHAR **ppszName, IUnknown **ppObject, IUnknown **ppUnk);

  // IAMOpenProgress
  STDMETHODIMP QueryProgress(LONGLONG *pllTotal, LONGLONG *pllCurrent);
  STDMETHODIMP AbortOperation();

  // ISpecifyPropertyPages2
  STDMETHODIMP GetPages(CAUUID *pPages);
  STDMETHODIMP CreatePage(const GUID& guid, IPropertyPage** ppPage);

  // IObjectWithSite
  STDMETHODIMP SetSite(IUnknown *pUnkSite);
  STDMETHODIMP GetSite(REFIID riid, void **ppvSite);

  // IBufferInfo
  STDMETHODIMP_(int) GetCount();
  STDMETHODIMP GetStatus(int i, int& samples, int& size);
  STDMETHODIMP_(DWORD) GetPriority();

  // ILAVFSettings
  STDMETHODIMP SetRuntimeConfig(BOOL bRuntimeConfig);
  STDMETHODIMP GetPreferredLanguages(LPWSTR *ppLanguages);
  STDMETHODIMP SetPreferredLanguages(LPCWSTR pLanguages);
  STDMETHODIMP GetPreferredSubtitleLanguages(LPWSTR *ppLanguages);
  STDMETHODIMP SetPreferredSubtitleLanguages(LPCWSTR pLanguages);
  STDMETHODIMP_(LAVSubtitleMode) GetSubtitleMode();
  STDMETHODIMP SetSubtitleMode(LAVSubtitleMode mode);
  STDMETHODIMP_(BOOL) GetSubtitleMatchingLanguage();
  STDMETHODIMP SetSubtitleMatchingLanguage(BOOL dwMode);
  STDMETHODIMP_(BOOL) GetPGSForcedStream();
  STDMETHODIMP SetPGSForcedStream(BOOL bFlag);
  STDMETHODIMP_(BOOL) GetPGSOnlyForced();
  STDMETHODIMP SetPGSOnlyForced(BOOL bForced);
  STDMETHODIMP_(int) GetVC1TimestampMode();
  STDMETHODIMP SetVC1TimestampMode(int iMode);
  STDMETHODIMP SetSubstreamsEnabled(BOOL bSubStreams);
  STDMETHODIMP_(BOOL) GetSubstreamsEnabled();
  STDMETHODIMP SetVideoParsingEnabled(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetVideoParsingEnabled();
  STDMETHODIMP SetFixBrokenHDPVR(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetFixBrokenHDPVR();
  STDMETHODIMP_(HRESULT) SetFormatEnabled(LPCSTR strFormat, BOOL bEnabled);
  STDMETHODIMP_(BOOL) IsFormatEnabled(LPCSTR strFormat);
  STDMETHODIMP SetStreamSwitchRemoveAudio(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetStreamSwitchRemoveAudio();
  STDMETHODIMP GetAdvancedSubtitleConfig(LPWSTR *ppAdvancedConfig);
  STDMETHODIMP SetAdvancedSubtitleConfig(LPCWSTR pAdvancedConfig);
  STDMETHODIMP SetUseAudioForHearingVisuallyImpaired(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetUseAudioForHearingVisuallyImpaired();
  STDMETHODIMP SetMaxQueueMemSize(DWORD dwMaxSize);
  STDMETHODIMP_(DWORD) GetMaxQueueMemSize();
  STDMETHODIMP SetTrayIcon(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetTrayIcon();
  STDMETHODIMP SetPreferHighQualityAudioStreams(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetPreferHighQualityAudioStreams();
  STDMETHODIMP SetLoadMatroskaExternalSegments(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetLoadMatroskaExternalSegments();
  STDMETHODIMP GetFormats(LPSTR** formats, UINT* nFormats);

  // ILAVSplitterSettingsInternal
  STDMETHODIMP_(LPCSTR) GetInputFormat() { if (m_pDemuxer) return m_pDemuxer->GetContainerFormat(); return NULL; }
  STDMETHODIMP_(std::set<FormatInfo>&) GetInputFormats();
  STDMETHODIMP_(BOOL) IsVC1CorrectionRequired();
  STDMETHODIMP_(CMediaType *) GetOutputMediatype(int stream);

  STDMETHODIMP_(DWORD) GetStreamFlags(DWORD dwStream) { if (m_pDemuxer) return m_pDemuxer->GetStreamFlags(dwStream); return 0; }
  STDMETHODIMP_(int) GetPixelFormat(DWORD dwStream) { if (m_pDemuxer) return m_pDemuxer->GetPixelFormat(dwStream); return AV_PIX_FMT_NONE; }
  STDMETHODIMP_(int) GetHasBFrames(DWORD dwStream){ if (m_pDemuxer) return m_pDemuxer->GetHasBFrames(dwStream); return -1; }

  // Settings helper
  std::list<std::string> GetPreferredAudioLanguageList();
  std::list<CSubtitleSelector> GetSubtitleSelectors();

  bool IsAnyPinDrying();
  void SetFakeASFReader(BOOL bFlag) { m_bFakeASFReader = bFlag; }
protected:
  // CAMThread
  enum {CMD_EXIT, CMD_SEEK};
  DWORD ThreadProc();

  HRESULT DemuxSeek(REFERENCE_TIME rtStart);
  HRESULT DemuxNextPacket();
  HRESULT DeliverPacket(Packet *pPacket);

  void DeliverBeginFlush();
  void DeliverEndFlush();

  STDMETHODIMP Close();
  STDMETHODIMP DeleteOutputs();

  STDMETHODIMP InitDemuxer();

  friend class CLAVOutputPin;
  STDMETHODIMP SetPositionsInternal(void *caller, LONGLONG* pCurrent, DWORD dwCurrentFlags, LONGLONG* pStop, DWORD dwStopFlags);

public:
  CLAVOutputPin *GetOutputPin(DWORD streamId, BOOL bActiveOnly = FALSE);
  STDMETHODIMP RenameOutputPin(DWORD TrackNumSrc, DWORD TrackNumDst, std::vector<CMediaType> pmts);
  STDMETHODIMP UpdateForcedSubtitleMediaType();

  STDMETHODIMP CompleteInputConnection();
  STDMETHODIMP BreakInputConnection();

protected:
  STDMETHODIMP LoadDefaults();
  STDMETHODIMP ReadSettings(HKEY rootKey);
  STDMETHODIMP LoadSettings();
  STDMETHODIMP SaveSettings();

  STDMETHODIMP CreateTrayIcon();

protected:
  CLAVInputPin *m_pInput;

private:
  CCritSec m_csPins;
  std::vector<CLAVOutputPin *> m_pPins;
  std::vector<CLAVOutputPin *> m_pActivePins;
  std::vector<CLAVOutputPin *> m_pRetiredPins;
  std::set<DWORD> m_bDiscontinuitySent;

  std::wstring m_fileName;
  std::wstring m_processName;

  CBaseDemuxer *m_pDemuxer;

  BOOL m_bPlaybackStarted;
  BOOL m_bFakeASFReader;

  // Times
  REFERENCE_TIME m_rtStart, m_rtStop, m_rtCurrent, m_rtNewStart, m_rtNewStop;
  REFERENCE_TIME m_rtOffset;
  double m_dRate;
  BOOL m_bStopValid;

  // Seeking
  REFERENCE_TIME m_rtLastStart, m_rtLastStop;
  std::set<void *> m_LastSeekers;

  // flushing
  bool m_fFlushing;
  CAMEvent m_eEndFlush;

  std::set<FormatInfo> m_InputFormats;

  // Settings
  struct Settings {
    BOOL TrayIcon;
    std::wstring prefAudioLangs;
    std::wstring prefSubLangs;
    std::wstring subtitleAdvanced;
    LAVSubtitleMode subtitleMode;
    BOOL PGSForcedStream;
    BOOL PGSOnlyForced;
    int vc1Mode;
    BOOL substreams;

    BOOL MatroskaExternalSegments;

    BOOL StreamSwitchRemoveAudio;
    BOOL ImpairedAudio;
    BOOL PreferHighQualityAudio;
    DWORD QueueMaxSize;

    std::map<std::string, BOOL> formats;
  } m_settings;

  BOOL m_bRuntimeConfig;

  IUnknown *m_pSite;

  CBaseTrayIcon *m_pTrayIcon;
};

[uuid("B98D13E7-55DB-4385-A33D-09FD1BA26338")]
class CLAVSplitterSource : public CLAVSplitter
{
public:
  // construct only via class factory
  CLAVSplitterSource(LPUNKNOWN pUnk, HRESULT* phr);
  virtual ~CLAVSplitterSource();

  // IUnknown
  DECLARE_IUNKNOWN;
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);
};
