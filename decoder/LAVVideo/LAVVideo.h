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

#include "decoders/ILAVDecoder.h"

#include "LAVPixFmtConverter.h"
#include "LAVVideoSettings.h"
#include "H264RandomAccess.h"
#include "FloatingAverage.h"

#define LAVC_VIDEO_REGISTRY_KEY L"Software\\LAV\\Video"
#define LAVC_VIDEO_REGISTRY_KEY_FORMATS L"Software\\LAV\\Video\\Formats"
#define LAVC_VIDEO_REGISTRY_KEY_OUTPUT L"Software\\LAV\\Video\\Output"

#define DEBUG_FRAME_TIMINGS 0
#define DEBUG_PIXELCONV_TIMINGS 0

typedef struct {
  REFERENCE_TIME rtStart;
  REFERENCE_TIME rtStop;
} TimingCache;

[uuid("EE30215D-164F-4A92-A4EB-9D4C13390F9F")]
class CLAVVideo : public CTransformFilter, public ISpecifyPropertyPages, public ILAVVideoSettings, public ILAVVideoCallback
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
  STDMETHODIMP_(BOOL) GetPixelFormat(LAVOutPixFmts pixFmt);
  STDMETHODIMP SetPixelFormat(LAVOutPixFmts pixFmt, BOOL bEnabled);
  STDMETHODIMP SetHighQualityPixelFormatConversion(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetHighQualityPixelFormatConversion();
  STDMETHODIMP SetRGBOutputRange(DWORD dwRange);
  STDMETHODIMP_(DWORD) GetRGBOutputRange();

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

  // ILAVVideoCallback
  STDMETHODIMP AllocateFrame(LAVFrame **ppFrame);
  STDMETHODIMP ReleaseFrame(LAVFrame **ppFrame);
  STDMETHODIMP Deliver(LAVFrame *pFrame);
  STDMETHODIMP_(LPWSTR) GetFileExtension();
  STDMETHODIMP_(BOOL) FilterInGraph(const GUID &clsid) { return ::FilterInGraph(clsid, m_pGraph); }

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

  HRESULT CreateDecoder(const CMediaType *pmt);

  HRESULT GetDeliveryBuffer(IMediaSample** ppOut, int width, int height, AVRational ar, DXVA2_ExtendedFormat dxvaExtFormat);
  HRESULT ReconnectOutput(int width, int height, AVRational ar, DXVA2_ExtendedFormat dxvaExtFlags);

  HRESULT SetTypeSpecificFlags(IMediaSample* pMS, LAVFrame *pFrame);

  HRESULT NegotiatePixelFormat(CMediaType &mt, int width, int height);

private:
  ILAVDecoder          *m_pDecoder;

  REFERENCE_TIME       m_rtPrevStart;
  REFERENCE_TIME       m_rtPrevStop;

  BOOL                 m_bForceInputAR;
  BOOL                 m_bSendMediaType;

  CLAVPixFmtConverter  m_PixFmtConverter;
  std::wstring         m_strExtension;

  CH264RandomAccess    m_h264RandomAccess;

  BOOL                 m_bRuntimeConfig;
  struct VideoSettings {
    BOOL StreamAR;
    BOOL InterlacedFlags;
    DWORD NumThreads;
    BOOL bFormats[Codec_NB];
    BOOL bPixFmts[LAVOutPixFmt_NB];
    BOOL HighQualityPixConv;
    DWORD RGBRange;
  } m_settings;

#ifdef DEBUG
  FloatingAverage<double> m_pixFmtTimingAvg;
#endif
};
