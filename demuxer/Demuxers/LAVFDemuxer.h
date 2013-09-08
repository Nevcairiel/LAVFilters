/*
 *      Copyright (C) 2010-2015 Hendrik Leppkes
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
#include <algorithm>
#include <sstream>

#include "BaseDemuxer.h"
#include "IKeyFrameInfo.h"
#include "ITrackInfo.h"
#include "DSMResourceBag.h"
#include "FontInstaller.h"

#define SUBMODE_FORCED_PGS_ONLY 0xFF

class FormatInfo;
class CBDDemuxer;

#define FFMPEG_FILE_BUFFER_SIZE   32768 // default reading size for ffmpeg
class CLAVFDemuxer : public CBaseDemuxer, public IAMExtendedSeeking, public IKeyFrameInfo, public ITrackInfo, public IAMMediaContent, public IPropertyBag,
                     public IDSMResourceBagImpl
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
  STDMETHODIMP Reset();
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

  STDMETHODIMP_(BSTR) GetTrackCodecID(UINT aTrackIdx) { return nullptr; }
  STDMETHODIMP_(BSTR) GetTrackName(UINT aTrackIdx);
  STDMETHODIMP_(BSTR) GetTrackCodecName(UINT aTrackIdx);
  STDMETHODIMP_(BSTR) GetTrackCodecInfoURL(UINT aTrackIdx) { return nullptr; }
  STDMETHODIMP_(BSTR) GetTrackCodecDownloadURL(UINT aTrackIdx) { return nullptr; }

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

  // IPropertyBag
  STDMETHODIMP Read(LPCOLESTR pszPropName, VARIANT *pVar, IErrorLog *pErrorLog);
  STDMETHODIMP Write(LPCOLESTR pszPropName, VARIANT *pVar);

  STDMETHODIMP OpenInputStream(AVIOContext *byteContext, LPCOLESTR pszFileName = nullptr, const char *format = nullptr, BOOL bForce = FALSE, BOOL bFileSource = FALSE);
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
  STDMETHODIMP InitAVFormat(LPCOLESTR pszFileName, BOOL bForce);
  void CleanupAVFormat();
  void UpdateParserFlags(AVStream *st);

  REFERENCE_TIME ConvertTimestampToRT(int64_t pts, int den, int num, int64_t starttime = (int64_t)AV_NOPTS_VALUE) const;
  int64_t ConvertRTToTimestamp(REFERENCE_TIME timestamp, int den, int num, int64_t starttime = (int64_t)AV_NOPTS_VALUE) const;

  int GetStreamIdxFromTotalIdx(size_t index) const;
  const CBaseDemuxer::stream* GetStreamFromTotalIdx(size_t index) const;
  HRESULT CheckBDM2TSCPLI(LPCOLESTR pszFileName);

  HRESULT UpdateForcedSubtitleStream(unsigned audio_pid);

  static int avio_interrupt_cb(void *opaque);

  STDMETHODIMP GetBSTRMetadata(const char *key, BSTR *pbstrValue, int stream = -1);
  STDMETHODIMP CreatePacketMediaType(Packet *pPacket, enum AVCodecID codec_id, BYTE *extradata, int extradata_size, BYTE *paramchange, int paramchange_size);
  STDMETHODIMP ParseICYMetadataPacket();

private:
  AVFormatContext *m_avFormat        = nullptr;
  const char      *m_pszInputFormat  = nullptr;

  BOOL m_bMatroska                   = FALSE;
  BOOL m_bOgg                        = FALSE;
  BOOL m_bAVI                        = FALSE;
  BOOL m_bMPEGTS                     = FALSE;
  BOOL m_bMPEGPS                     = FALSE;
  BOOL m_bRM                         = FALSE;
  BOOL m_bPMP                        = FALSE;
  BOOL m_bMP4                        = FALSE;

  BOOL m_bTSDiscont                  = FALSE;
  BOOL m_bSubStreams                 = FALSE;
  BOOL m_bVC1Correction              = FALSE;
  BOOL m_bVC1SeenTimestamp           = FALSE;
  BOOL m_bPGSNoParsing               = FALSE;

  int m_ForcedSubStream              = -1;
  unsigned int m_program             = 0;

  REFERENCE_TIME m_rtCurrent         = 0;

  AVStreamParseType *m_stOrigParser  = nullptr;

  CFontInstaller *m_pFontInstaller   = nullptr;
  ILAVFSettingsInternal *m_pSettings = nullptr;

  BOOL m_bEnableTrackInfo            = TRUE;

  CBDDemuxer *m_pBluRay              = nullptr;

  int m_Abort                        = 0;
  time_t m_timeOpening               = 0;
};
