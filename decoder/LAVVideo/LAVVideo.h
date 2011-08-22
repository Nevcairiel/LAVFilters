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
#include "H264RandomAccess.h"

#define MAX_THREADS 16

#define LAVC_VIDEO_REGISTRY_KEY L"Software\\LAV\\Video"
#define LAVC_VIDEO_REGISTRY_KEY_FORMATS L"Software\\LAV\\Video\\Formats"
#define LAVC_VIDEO_REGISTRY_KEY_OUTPUT L"Software\\LAV\\Video\\Output"

typedef struct {
  REFERENCE_TIME rtStart;
  REFERENCE_TIME rtStop;
} TimingCache;

[uuid("EE30215D-164F-4A92-A4EB-9D4C13390F9F")]
class CLAVVideo : public CTransformFilter, public ISpecifyPropertyPages, public ILAVVideoSettings
{
public:
  CLAVVideo(LPUNKNOWN pUnk, HRESULT* phr);
  ~CLAVVideo();

  // IUnknown
  DECLARE_IUNKNOWN;
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  // ISpecifyPropertyPages
  STDMETHODIMP GetPages(CAUUID *pPages);

  // ILAVVideoSettings
  STDMETHODIMP SetRuntimeConfig(BOOL bRuntimeConfig);
  STDMETHODIMP_(BOOL) GetFormatConfiguration(LAVVideoCodec vCodec);
  STDMETHODIMP SetFormatConfiguration(LAVVideoCodec vCodec, BOOL bEnabled);
  STDMETHODIMP SetNumThreads(DWORD dwNum);
  STDMETHODIMP_(DWORD) GetNumThreads();
  STDMETHODIMP SetStreamAR(BOOL bStreamAR);
  STDMETHODIMP_(BOOL) GetStreamAR();
  STDMETHODIMP SetReportInterlacedFlags(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetReportInterlacedFlags();
  STDMETHODIMP_(BOOL) GetPixelFormat(LAVVideoPixFmts pixFmt);
  STDMETHODIMP SetPixelFormat(LAVVideoPixFmts pixFmt, BOOL bEnabled);
  STDMETHODIMP SetHighQualityPixelFormatConversion(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetHighQualityPixelFormatConversion();

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

  HRESULT BreakConnect(PIN_DIRECTION dir);

public:
  // Pin Configuration
  const static AMOVIESETUP_MEDIATYPE    sudPinTypesIn[];
  const static int                      sudPinTypesInCount;
  const static AMOVIESETUP_MEDIATYPE    sudPinTypesOut[];
  const static int                      sudPinTypesOutCount;

private:
  HRESULT LoadDefaults();
  HRESULT LoadSettings();
  HRESULT SaveSettings();

  HRESULT ffmpeg_init(CodecID codec, const CMediaType *pmt);
  void ffmpeg_shutdown();

  HRESULT GetDeliveryBuffer(IMediaSample** ppOut, int width, int height, AVRational ar);
  HRESULT ReconnectOutput(int width, int height, AVRational ar);
  HRESULT Decode(BYTE *pDataIn, int nSize, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop);

  HRESULT SetTypeSpecificFlags(IMediaSample* pMS);

  HRESULT NegotiatePixelFormat(CMediaType &mt, int width, int height);

private:
  CodecID              m_nCodecId;       // FFMPEG Codec Id
  AVCodec              *m_pAVCodec;      // AVCodec reference
  AVCodecContext       *m_pAVCtx;        // AVCodecContext reference
  AVCodecParserContext *m_pParser;       // AVCodecParserContext reference

  BOOL                 m_bDXVA;          // DXVA active?
  REFERENCE_TIME       m_rtAvrTimePerFrame;
  BOOL                 m_bProcessExtradata;

  AVFrame              *m_pFrame;
  BYTE                 *m_pFFBuffer;
  int                  m_nFFBufferSize;

  SwsContext           *m_pSwsContext;

  BOOL                 m_bFFReordering;
  BOOL                 m_bCalculateStopTime;
  BOOL                 m_bRVDropBFrameTimings;
  REFERENCE_TIME       m_rtPrevStart;
  REFERENCE_TIME       m_rtPrevStop;
  REFERENCE_TIME       m_rtStartCache;

  TimingCache          m_tcThreadBuffer[MAX_THREADS];
  int                  m_CurrentThread;

  BOOL                 m_bDiscontinuity;
  BOOL                 m_bWaitingForKeyFrame;
  BOOL                 m_bForceTypeNegotiation;
  BOOL                 m_bForceInputAR;
  BOOL                 m_bSendMediaType;

  int                  m_nThreads;

  CLAVPixFmtConverter  m_PixFmtConverter;
  std::wstring         m_strExtension;

  CH264RandomAccess    m_h264RandomAccess;

  BOOL                 m_bRuntimeConfig;
  struct VideoSettings {
    BOOL StreamAR;
    BOOL InterlacedFlags;
    DWORD NumThreads;
    BOOL bFormats[Codec_NB];
    BOOL bPixFmts[LAVPixFmt_NB];
    BOOL HighQualityPixConv;
  } m_settings;
};
