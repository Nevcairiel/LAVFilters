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
 *
 *  Initial design and concept by Gabest and the MPC-HC Team, copyright under GPLv2
 *  Contributions by Ti-BEN from the XBMC DSPlayer Project, also under GPLv2
 */

#pragma once

#include <string>
#include <list>
#include <set>
#include <vector>
#include "PacketQueue.h"

#include "BaseDemuxer.h"

#include "LAVSplitterSettings.h"
#include "SettingsProp.h"

#define LAVF_REGISTRY_KEY L"Software\\LAV\\Splitter"

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
  , public ILAVFSettings
  , public ISpecifyPropertyPages
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

  // ISpecifyPropertyPages
  STDMETHODIMP GetPages(CAUUID *pPages);

  // ILAVFSettings
  STDMETHODIMP GetPreferredLanguages(WCHAR **ppLanguages);
  STDMETHODIMP SetPreferredLanguages(WCHAR *pLanguages);
  STDMETHODIMP GetPreferredSubtitleLanguages(WCHAR **ppLanguages);
  STDMETHODIMP SetPreferredSubtitleLanguages(WCHAR *pLanguages);
  STDMETHODIMP_(DWORD) GetSubtitleMode();
  STDMETHODIMP SetSubtitleMode(DWORD dwMode);
  STDMETHODIMP_(BOOL) GetSubtitleMatchingLanguage();
  STDMETHODIMP SetSubtitleMatchingLanguage(BOOL dwMode);
  STDMETHODIMP_(int) GetVC1TimestampMode();
  STDMETHODIMP SetVC1TimestampMode(int iMode);
  STDMETHODIMP_(BOOL) IsVC1CorrectionRequired();
  STDMETHODIMP SetSubstreamsEnabled(BOOL bSubStreams);
  STDMETHODIMP_(BOOL) GetSubstreamsEnabled();
  STDMETHODIMP SetVideoParsingEnabled(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetVideoParsingEnabled();
  STDMETHODIMP SetAudioParsingEnabled(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetAudioParsingEnabled();
  STDMETHODIMP SetGeneratePTS(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetGeneratePTS();

  // Settings helper
  std::list<std::string> GetPreferredAudioLanguageList();
  std::list<std::string> GetPreferredSubtitleLanguageList();

  bool IsAnyPinDrying();
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
  CLAVOutputPin *GetOutputPin(DWORD streamId);
  STDMETHODIMP RenameOutputPin(DWORD TrackNumSrc, DWORD TrackNumDst, std::vector<CMediaType> pmts);

  STDMETHODIMP CompleteInputConnection();
  STDMETHODIMP BreakInputConnection();

protected:
  // construct only via class factory
  CLAVSplitter(LPUNKNOWN pUnk, HRESULT* phr);
  virtual ~CLAVSplitter();

  STDMETHODIMP LoadSettings();
  STDMETHODIMP SaveSettings();

protected:
  CLAVInputPin *m_pInput;

private:
  CCritSec m_csPins;
  std::vector<CLAVOutputPin *> m_pPins;
  std::vector<CLAVOutputPin *> m_pRetiredPins;
  std::set<DWORD> m_bDiscontinuitySent;

  std::wstring m_fileName;

  CBaseDemuxer *m_pDemuxer;

  BOOL m_bPlaybackStarted;

  // Times
  REFERENCE_TIME m_rtStart, m_rtStop, m_rtCurrent, m_rtNewStart, m_rtNewStop;
  double m_dRate;

  // Seeking
  REFERENCE_TIME m_rtLastStart, m_rtLastStop;
  std::set<void *> m_LastSeekers;

  // flushing
  bool m_fFlushing;
  CAMEvent m_eEndFlush;

  // Settings
  struct Settings {
    std::wstring prefAudioLangs;
    std::wstring prefSubLangs;
    DWORD subtitleMode;
    BOOL subtitleMatching;
    int vc1Mode;
    BOOL substreams;
    BOOL videoParsing;
    BOOL audioParsing;
    BOOL generatePTS;
  } m_settings;
};

[uuid("B98D13E7-55DB-4385-A33D-09FD1BA26338")]
class CLAVSplitterSource : public CLAVSplitter
{
public:
  // constructor method used by class factory
  static CUnknown* WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT* phr);

  // IUnknown
  DECLARE_IUNKNOWN;
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

private:
  // construct only via class factory
  CLAVSplitterSource(LPUNKNOWN pUnk, HRESULT* phr);
  virtual ~CLAVSplitterSource();
};
