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
 */

#pragma once

#include "LAVPixFmtConverter.h"
#include "LAVVideoSettings.h"

#define MAX_THREADS 32

#define LAVC_VIDEO_REGISTRY_KEY L"Software\\LAV\\Video"

typedef struct {
  REFERENCE_TIME rtStart;
  REFERENCE_TIME rtStop;
} TimingCache;

[uuid("EE30215D-164F-4A92-A4EB-9D4C13390F9F")]
class CLAVVideo : public CTransformFilter, public ISpecifyPropertyPages, public ILAVVideoSettings
{
public:
  // constructor method used by class factory
  static CUnknown* WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT* phr);

  // IUnknown
  DECLARE_IUNKNOWN;
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  // ISpecifyPropertyPages
  STDMETHODIMP GetPages(CAUUID *pPages);

  // ILAVVideoSettings
  STDMETHODIMP SetRuntimeConfig(BOOL bRuntimeConfig);
  STDMETHODIMP SetNumThreads(DWORD dwNum);
  STDMETHODIMP_(DWORD) GetNumThreads();
  STDMETHODIMP SetStreamAR(BOOL bStreamAR);
  STDMETHODIMP_(BOOL) GetStreamAR();
  STDMETHODIMP SetReportInterlacedFlags(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetReportInterlacedFlags();

  // CTransformFilter
  HRESULT CheckInputType(const CMediaType* mtIn);
  HRESULT CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut);
  HRESULT DecideBufferSize(IMemAllocator * pAllocator, ALLOCATOR_PROPERTIES *pprop);
  HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);

  HRESULT SetMediaType(PIN_DIRECTION dir, const CMediaType *pmt);
  HRESULT EndOfStream();
  HRESULT BeginFlush();
  HRESULT EndFlush();
  HRESULT NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);
  HRESULT Receive(IMediaSample *pIn);

public:
  // Pin Configuration
  const static AMOVIESETUP_MEDIATYPE    sudPinTypesIn[];
  const static int                      sudPinTypesInCount;
  const static AMOVIESETUP_MEDIATYPE    sudPinTypesOut[];
  const static int                      sudPinTypesOutCount;

private:
  CLAVVideo(LPUNKNOWN pUnk, HRESULT* phr);
  ~CLAVVideo();

  HRESULT LoadDefaults();
  HRESULT LoadSettings();
  HRESULT SaveSettings();

  HRESULT ffmpeg_init(CodecID codec, const CMediaType *pmt);
  void ffmpeg_shutdown();

  HRESULT GetDeliveryBuffer(IMediaSample** ppOut, int width, int height, AVRational ar);
  HRESULT ReconnectOutput(int width, int height, AVRational ar);
  HRESULT Decode(const BYTE *pDataIn, int nSize, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);

  HRESULT SetTypeSpecificFlags(IMediaSample* pMS);

  HRESULT NegotiatePixelFormat(CMediaType &mt, int width, int height);

private:
  CodecID              m_nCodecId;       // FFMPEG Codec Id
  AVCodec              *m_pAVCodec;      // AVCodec reference
  AVCodecContext       *m_pAVCtx;        // AVCodecContext reference

  BOOL                 m_bDXVA;          // DXVA active?
  REFERENCE_TIME       m_rtAvrTimePerFrame;
  BOOL                 m_bProcessExtradata;

  AVFrame              *m_pFrame;
  BYTE                 *m_pFFBuffer;
  int                  m_nFFBufferSize;

  SwsContext           *m_pSwsContext;

  BOOL                 m_bFFReordering;
  BOOL                 m_bCalculateStopTime;
  REFERENCE_TIME       m_rtPrevStart;
  REFERENCE_TIME       m_rtPrevStop;

  TimingCache          m_tcThreadBuffer[MAX_THREADS];
  int                  m_CurrentThread;

  BOOL                 m_bDiscontinuity;
  BOOL                 m_bForceTypeNegotiation;

  int                  m_nThreads;

  CLAVPixFmtConverter  m_PixFmtConverter;

  BOOL                 m_bRuntimeConfig;
  struct VideoSettings {
    BOOL StreamAR;
    BOOL InterlacedFlags;
    DWORD NumThreads;
  } m_settings;
};
