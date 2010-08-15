/*
 *      Copyright (C) 2010 Hendrik Leppkes
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
#include <Qnetwork.h>
#include "PacketQueue.h"

#include "IKeyFrameInfo.h"

#define FFMPEG_FILE_BUFFER_SIZE   32768 // default reading size for ffmpeg

#define DVD_TIME_BASE 10000000        // DirectShow times are in 100ns units
#define DVD_SEC_TO_TIME(x)  ((double)(x) * DVD_TIME_BASE)
#define DVD_TIME_TO_MSEC(x) ((int)((double)(x) * 1000 / DVD_TIME_BASE))
#define DVD_MSEC_TO_TIME(x) ((double)(x) * DVD_TIME_BASE / 1000)

class CDSStreamInfo;
class CLAVFOutputPin;

[uuid("B98D13E7-55DB-4385-A33D-09FD1BA26338")]
class CLAVFSplitter 
  : public CBaseFilter
  , public CCritSec
  , protected CAMThread
  , public IFileSourceFilter
  , public IMediaSeeking
  , public IAMStreamSelect
  , public IAMExtendedSeeking
  , public IKeyFrameInfo
{
public:
  // constructor method used by class factory
  static CUnknown* WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT* phr);

  // IUnknown
  DECLARE_IUNKNOWN;
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  // IDispatch
  STDMETHODIMP GetTypeInfoCount(UINT* pctinfo) {return E_NOTIMPL;}
  STDMETHODIMP GetTypeInfo(UINT itinfo, LCID lcid, ITypeInfo** pptinfo) {return E_NOTIMPL;}
  STDMETHODIMP GetIDsOfNames(REFIID riid, OLECHAR** rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid) {return E_NOTIMPL;}
  STDMETHODIMP Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo, UINT* puArgErr) {return E_NOTIMPL;}

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

  // IAMExtendedSeeking
  STDMETHODIMP get_ExSeekCapabilities(long* pExCapabilities);
  STDMETHODIMP get_MarkerCount(long* pMarkerCount);
  STDMETHODIMP get_CurrentMarker(long* pCurrentMarker);
  STDMETHODIMP GetMarkerTime(long MarkerNum, double* pMarkerTime);
  STDMETHODIMP GetMarkerName(long MarkerNum, BSTR* pbstrMarkerName);
  STDMETHODIMP put_PlaybackSpeed(double Speed) {return E_NOTIMPL;}
  STDMETHODIMP get_PlaybackSpeed(double* pSpeed) {return E_NOTIMPL;}

  // IKeyFrameInfo
  STDMETHODIMP GetKeyFrameCount(UINT& nKFs);
  STDMETHODIMP GetKeyFrames(const GUID* pFormat, REFERENCE_TIME* pKFs, UINT& nKFs);

  bool IsAnyPinDrying();
protected:
  // CAMThread
  enum {CMD_EXIT, CMD_SEEK};
  DWORD ThreadProc();

  REFERENCE_TIME GetStreamLength();
  HRESULT DemuxSeek(REFERENCE_TIME rtStart);
  HRESULT DemuxNextPacket();
  REFERENCE_TIME ConvertTimestampToRT(int64_t pts, int den, int num);
  int64_t ConvertRTToTimestamp(REFERENCE_TIME timestamp, int den, int num);

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
  class CStreamList : public std::vector<stream>
  {
  public:
    static const WCHAR* ToString(int type);
    const stream* FindStream(DWORD pid);
    void Clear();
  } m_streams[unknown];

  CLAVFOutputPin *GetOutputPin(DWORD streamId);
  STDMETHODIMP RenameOutputPin(DWORD TrackNumSrc, DWORD TrackNumDst, const AM_MEDIA_TYPE* pmt);
  DWORD GetVideoStreamId();

private:
  // construct only via class factory
  CLAVFSplitter(LPUNKNOWN pUnk, HRESULT* phr);
  ~CLAVFSplitter();

private:
  CCritSec m_csPins;
  std::vector<CLAVFOutputPin *> m_pPins;
  std::set<DWORD> m_bDiscontinuitySent;

  std::wstring m_fileName;

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
