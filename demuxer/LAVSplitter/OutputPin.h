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

#pragma once

#include <vector>
#include <string>
#include "PacketQueue.h"
#include "StreamParser.h"

#include "moreuuids.h"

#include "LAVSplitter.h"
#include "ILAVPinInfo.h"
#include "IBitRateInfo.h"
#include "IMediaSideData.h"

class CLAVOutputPin
  : public CBaseOutputPin
  , public ILAVPinInfo
  , public IBitRateInfo
  , public IMediaSideData
  , IMediaSeeking
  , protected CAMThread
{
public:
  CLAVOutputPin(std::deque<CMediaType>& mts, LPCWSTR pName, CBaseFilter *pFilter, CCritSec *pLock, HRESULT *phr, CBaseDemuxer::StreamType pinType = CBaseDemuxer::unknown,const char* container = "");
  virtual ~CLAVOutputPin();

  DECLARE_IUNKNOWN;
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  // IQualityControl
  STDMETHODIMP Notify(IBaseFilter* pSender, Quality q) { return E_NOTIMPL; }

  // CBaseOutputPin
  HRESULT DecideAllocator(IMemInputPin * pPin, IMemAllocator ** pAlloc);
  HRESULT DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProperties);
  HRESULT CheckMediaType(const CMediaType* pmt);
  HRESULT GetMediaType(int iPosition, CMediaType* pmt);
  HRESULT Active();
  HRESULT Inactive();

  STDMETHODIMP Connect(IPin * pReceivePin, const AM_MEDIA_TYPE *pmt);
  HRESULT CompleteConnect(IPin *pReceivePin);

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

  // ILAVPinInfo
  STDMETHODIMP_(DWORD) GetStreamFlags();
  STDMETHODIMP_(int) GetPixelFormat();
  STDMETHODIMP_(int) GetVersion() { return 1; }
  STDMETHODIMP_(int) GetHasBFrames();

  // IBitRateInfo
  STDMETHODIMP_(DWORD) GetCurrentBitRate() { return m_BitRate.nCurrentBitRate; }
  STDMETHODIMP_(DWORD) GetAverageBitRate() { return m_BitRate.nAverageBitRate; }

  // IMediaSideData
  STDMETHODIMP SetSideData(GUID guidType, const BYTE *pData, size_t size) { return E_NOTIMPL; }
  STDMETHODIMP GetSideData(GUID guidType, const BYTE **pData, size_t *pSize);

  size_t QueueCount();
  HRESULT QueuePacket(Packet *pPacket);
  HRESULT QueueEndOfStream();
  bool IsDiscontinuous();
  size_t GetQueueLowLimit() const { return m_nQueueLow; }

  DWORD GetStreamId() { return m_streamId; };
  void SetStreamId(DWORD newStreamId) { m_streamId = newStreamId; };

  void SetNewMediaTypes(std::deque<CMediaType> pmts) { CAutoLock lock(&m_csMT); m_mts = pmts; SetQueueSizes(); }
  void SendMediaType(CMediaType *mt) { CAutoLock lock(&m_csMT); m_newMT = mt;}
  void SetStreamMediaType(CMediaType *mt) { CAutoLock lock(&m_csMT); m_StreamMT = *mt; }

  CMediaType& GetActiveMediaType() { return m_mt; }

  BOOL IsVideoPin() { return m_pinType == CBaseDemuxer::video; }
  BOOL IsAudioPin() { return m_pinType == CBaseDemuxer::audio; }
  BOOL IsSubtitlePin(){ return m_pinType == CBaseDemuxer::subpic; }
  CBaseDemuxer::StreamType GetPinType() { return m_pinType; }

  HRESULT QueueFromParser(Packet *pPacket) { m_queue.Queue(pPacket); return S_OK; }

  HRESULT GetQueueSize(int& samples, int& size);

public:
  // Packet handling functions
  virtual HRESULT DeliverBeginFlush();
  virtual HRESULT DeliverEndFlush();

  virtual HRESULT DeliverNewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

  void SetQueueSizes();

  REFERENCE_TIME m_rtPrev = AV_NOPTS_VALUE;

protected:
  virtual HRESULT DeliverPacket(Packet *pPacket);

private:
  enum {CMD_EXIT};
  DWORD ThreadProc();

  void MakeISCRHappy();

private:
  CCritSec m_csMT;
  std::deque<CMediaType> m_mts;
  CPacketQueue m_queue;
  CMediaType m_StreamMT;

  std::string m_containerFormat;

  // Flush control
  bool m_fFlushing = false;
  bool m_fFlushed  = false;
  CAMEvent m_eEndFlush{TRUE};

  HRESULT m_hrDeliver = S_OK;

  int m_nBuffers        = 1;
  size_t m_nQueueLow    = MIN_PACKETS_IN_QUEUE;
  size_t m_nQueueHigh   = 350;
  size_t m_nQueueMaxMem = 256 * 1024 * 1024;

  DWORD m_streamId      = 0;
  CMediaType *m_newMT   = nullptr;

  CBaseDemuxer::StreamType m_pinType;

  CStreamParser m_Parser;
  BOOL m_bPacketAllocator = FALSE;

  // IBitRateInfo
  struct BitRateInfo {
    UINT64 nTotalBytesDelivered         = 0;
    REFERENCE_TIME rtTotalTimeDelivered = 0;
    UINT64 nBytesSinceLastDeliverTime   = 0;
    REFERENCE_TIME rtLastDeliverTime    = Packet::INVALID_TIME;
    DWORD nCurrentBitRate               = 0;
    DWORD nAverageBitRate               = 0;
  } m_BitRate;
};
