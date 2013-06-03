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
 */

#pragma once

#include <Qnetwork.h>
#include <set>

#include "BaseDemuxer.h"
#include "IKeyFrameInfo.h"
#include "ITrackInfo.h"
#include "FontInstaller.h"

#define SUBMODE_FORCED_PGS_ONLY 0xFF

class FormatInfo;
class CBDDemuxer;

#define FFMPEG_FILE_BUFFER_SIZE   32768 // default reading size for ffmpeg
class CLAVFDemuxer : public CBaseDemuxer, public IAMExtendedSeeking, public IKeyFrameInfo, public ITrackInfo, public IAMMediaContent
{
public:
  CLAVFDemuxer(CCritSec *pLock, ILAVFSettingsInternal *settings);
  ~CLAVFDemuxer();

  static void ffmpeg_init(bool network);
  static std::set<FormatInfo> GetFormatList();

  // IUnknown
  DECLARE_IUNKNOWN
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  // IDispatch
  STDMETHODIMP GetTypeInfoCount(UINT* pctinfo) {return E_NOTIMPL;}
  STDMETHODIMP GetTypeInfo(UINT itinfo, LCID lcid, ITypeInfo** pptinfo) {return E_NOTIMPL;}
  STDMETHODIMP GetIDsOfNames(REFIID riid, OLECHAR** rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid) {return E_NOTIMPL;}
  STDMETHODIMP Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo, UINT* puArgErr) {return E_NOTIMPL;}

  // CBaseDemuxer
  STDMETHODIMP Open(LPCOLESTR pszFileName);
  STDMETHODIMP Start() { if (m_avFormat) av_read_play(m_avFormat); return S_OK; }
  STDMETHODIMP AbortOpening(int mode = 1);
  REFERENCE_TIME GetDuration() const;
  STDMETHODIMP GetNextPacket(Packet **ppPacket);
  STDMETHODIMP Seek(REFERENCE_TIME rTime);
  const char *GetContainerFormat() const;
  virtual DWORD GetContainerFlags() { return m_bTSDiscont ? LAVFMT_TS_DISCONT : 0; }

  STDMETHODIMP SetTitle(int idx);
  STDMETHODIMP_(int) GetTitle();
  STDMETHODIMP GetTitleInfo(int idx, REFERENCE_TIME *rtDuration, WCHAR **ppszName);
  STDMETHODIMP_(int) GetNumTitles();

  void SettingsChanged(ILAVFSettingsInternal *pSettings);

  // Select the best video stream
  const stream* SelectVideoStream();
  // Select the best audio stream
  const stream* SelectAudioStream(std::list<std::string> prefLanguages);
  // Select the best subtitle stream
  const stream* SelectSubtitleStream(std::list<CSubtitleSelector> subtitleSelectors, std::string audioLanguage);

  HRESULT SetActiveStream(StreamType type, int pid);

  STDMETHODIMP_(DWORD) GetStreamFlags(DWORD dwStream);
  STDMETHODIMP_(int) GetPixelFormat(DWORD dwStream);
  STDMETHODIMP_(int) GetHasBFrames(DWORD dwStream);

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

  // ITrackInfo
  STDMETHODIMP_(UINT) GetTrackCount();

  // \param aTrackIdx the track index (from 0 to GetTrackCount()-1)
  STDMETHODIMP_(BOOL) GetTrackInfo(UINT aTrackIdx, struct TrackElement* pStructureToFill);

  // Get an extended information struct relative to the track type
  STDMETHODIMP_(BOOL) GetTrackExtendedInfo(UINT aTrackIdx, void* pStructureToFill);

  STDMETHODIMP_(BSTR) GetTrackCodecID(UINT aTrackIdx) { return NULL; }
  STDMETHODIMP_(BSTR) GetTrackName(UINT aTrackIdx);
  STDMETHODIMP_(BSTR) GetTrackCodecName(UINT aTrackIdx);
  STDMETHODIMP_(BSTR) GetTrackCodecInfoURL(UINT aTrackIdx) { return NULL; }
  STDMETHODIMP_(BSTR) GetTrackCodecDownloadURL(UINT aTrackIdx) { return NULL; }

  // IAMMediaContent
  STDMETHODIMP get_AuthorName(BSTR *pbstrAuthorName) { return GetBSTRMetadata("artist", pbstrAuthorName); }
  STDMETHODIMP get_Title(BSTR *pbstrTitle) { return GetBSTRMetadata("title", pbstrTitle); }
  STDMETHODIMP get_Rating(BSTR *pbstrRating) { return E_NOTIMPL; }
  STDMETHODIMP get_Description(BSTR *pbstrDescription) { return GetBSTRMetadata("comment", pbstrDescription); }
  STDMETHODIMP get_Copyright(BSTR *pbstrCopyright) { return GetBSTRMetadata("copyright", pbstrCopyright); }
  STDMETHODIMP get_BaseURL(BSTR *pbstrBaseURL) { return E_NOTIMPL; }
  STDMETHODIMP get_LogoURL(BSTR *pbstrLogoURL) { return E_NOTIMPL; }
  STDMETHODIMP get_LogoIconURL(BSTR *pbstrLogoURL) { return E_NOTIMPL; }
  STDMETHODIMP get_WatermarkURL(BSTR *pbstrWatermarkURL) { return E_NOTIMPL; }
  STDMETHODIMP get_MoreInfoURL(BSTR *pbstrMoreInfoURL) { return E_NOTIMPL; }
  STDMETHODIMP get_MoreInfoBannerImage(BSTR *pbstrMoreInfoBannerImage) { return E_NOTIMPL; }
  STDMETHODIMP get_MoreInfoBannerURL(BSTR *pbstrMoreInfoBannerURL) { return E_NOTIMPL; }
  STDMETHODIMP get_MoreInfoText(BSTR *pbstrMoreInfoText) { return E_NOTIMPL; }

  STDMETHODIMP OpenInputStream(AVIOContext *byteContext, LPCOLESTR pszFileName = NULL, const char *format = NULL);
  STDMETHODIMP SeekByte(int64_t pos, int flags);

  AVStream* GetAVStreamByPID(int pid);
  void UpdateSubStreams();
  unsigned int GetNumStreams() const { return m_avFormat->nb_streams; }

  REFERENCE_TIME GetStartTime() const;
  void SetBluRay(CBDDemuxer *pBluRay) { m_pBluRay = pBluRay; }

  void AddMPEGTSStream(int pid, uint32_t stream_type);

private:
  STDMETHODIMP AddStream(int streamId);
  STDMETHODIMP CreateStreams();
  STDMETHODIMP InitAVFormat(LPCOLESTR pszFileName);
  void CleanupAVFormat();
  void UpdateParserFlags(AVStream *st);

  REFERENCE_TIME ConvertTimestampToRT(int64_t pts, int den, int num, int64_t starttime = (int64_t)AV_NOPTS_VALUE) const;
  int64_t ConvertRTToTimestamp(REFERENCE_TIME timestamp, int den, int num, int64_t starttime = (int64_t)AV_NOPTS_VALUE) const;

  int GetStreamIdxFromTotalIdx(size_t index) const;
  const CBaseDemuxer::stream* GetStreamFromTotalIdx(size_t index) const;
  HRESULT CheckBDM2TSCPLI(LPCOLESTR pszFileName);

  HRESULT UpdateForcedSubtitleStream(unsigned audio_pid);

  static int avio_interrupt_cb(void *opaque);

  STDMETHODIMP GetBSTRMetadata(const char *key, BSTR *pbstrValue);
  STDMETHODIMP CreatePacketMediaType(Packet *pPacket, enum AVCodecID codec_id, BYTE *extradata, int extradata_size, BYTE *paramchange, int paramchange_size);

private:
  AVFormatContext *m_avFormat;
  const char *m_pszInputFormat;

  BOOL m_bMatroska;
  BOOL m_bOgg;
  BOOL m_bAVI;
  BOOL m_bMPEGTS;
  BOOL m_bMPEGPS;
  BOOL m_bRM;
  BOOL m_bPMP;
  BOOL m_bMP4;
  BOOL m_bTSDiscont;
  BOOL m_bVC1Correction;

  BOOL m_bSubStreams;

  BOOL m_bVC1SeenTimestamp;

  BOOL m_bPGSNoParsing;
  int m_ForcedSubStream;

  unsigned int m_program;

  REFERENCE_TIME m_rtCurrent;

  enum AVStreamParseType *m_stOrigParser;

  CFontInstaller *m_pFontInstaller;
  ILAVFSettingsInternal *m_pSettings;

  BOOL m_bEnableTrackInfo;

  CBDDemuxer *m_pBluRay;

  int m_Abort;
  time_t m_timeOpening;
};
