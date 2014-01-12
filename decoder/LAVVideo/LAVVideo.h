/*
 *      Copyright (C) 2010-2014 Hendrik Leppkes
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

#include "decoders/ILAVDecoder.h"
#include "DecodeThread.h"
#include "ILAVPinInfo.h"

#include "LAVPixFmtConverter.h"
#include "LAVVideoSettings.h"
#include "FloatingAverage.h"

#include "ISpecifyPropertyPages2.h"
#include "SynchronizedQueue.h"

#include "subtitles/LAVSubtitleConsumer.h"
#include "subtitles/LAVVideoSubtitleInputPin.h"

#include "BaseTrayIcon.h"

#define LAVC_VIDEO_REGISTRY_KEY L"Software\\LAV\\Video"
#define LAVC_VIDEO_REGISTRY_KEY_FORMATS L"Software\\LAV\\Video\\Formats"
#define LAVC_VIDEO_REGISTRY_KEY_OUTPUT L"Software\\LAV\\Video\\Output"
#define LAVC_VIDEO_REGISTRY_KEY_HWACCEL L"Software\\LAV\\Video\\HWAccel"

#define LAVC_VIDEO_LOG_FILE     L"LAVVideo.txt"

#define DEBUG_FRAME_TIMINGS 0
#define DEBUG_PIXELCONV_TIMINGS 0

#define LAV_MT_FILTER_QUEUE_SIZE 4

typedef struct {
  REFERENCE_TIME rtStart;
  REFERENCE_TIME rtStop;
} TimingCache;

[uuid("EE30215D-164F-4A92-A4EB-9D4C13390F9F")]
class CLAVVideo : public CTransformFilter, public ISpecifyPropertyPages2, public ILAVVideoSettings, public ILAVVideoStatus, public ILAVVideoCallback
{
public:
  CLAVVideo(LPUNKNOWN pUnk, HRESULT* phr);
  ~CLAVVideo();

  static void CALLBACK StaticInit(BOOL bLoading, const CLSID *clsid);

  // IUnknown
  DECLARE_IUNKNOWN;
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  // ISpecifyPropertyPages2
  STDMETHODIMP GetPages(CAUUID *pPages);
  STDMETHODIMP CreatePage(const GUID& guid, IPropertyPage** ppPage);

  // ILAVVideoSettings
  STDMETHODIMP SetRuntimeConfig(BOOL bRuntimeConfig);
  STDMETHODIMP SetFormatConfiguration(LAVVideoCodec vCodec, BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetFormatConfiguration(LAVVideoCodec vCodec);
  STDMETHODIMP SetNumThreads(DWORD dwNum);
  STDMETHODIMP_(DWORD) GetNumThreads();
  STDMETHODIMP SetStreamAR(DWORD bStreamAR);
  STDMETHODIMP_(DWORD) GetStreamAR();
  STDMETHODIMP SetPixelFormat(LAVOutPixFmts pixFmt, BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetPixelFormat(LAVOutPixFmts pixFmt);
  STDMETHODIMP SetRGBOutputRange(DWORD dwRange);
  STDMETHODIMP_(DWORD) GetRGBOutputRange();

  STDMETHODIMP SetDeintFieldOrder(LAVDeintFieldOrder fieldOrder);
  STDMETHODIMP_(LAVDeintFieldOrder) GetDeintFieldOrder();
  STDMETHODIMP SetDeintForce(BOOL bForce);
  STDMETHODIMP_(BOOL) GetDeintForce();
  STDMETHODIMP SetDeintAggressive(BOOL bAggressive);
  STDMETHODIMP_(BOOL) GetDeintAggressive();

  STDMETHODIMP_(DWORD) CheckHWAccelSupport(LAVHWAccel hwAccel);
  STDMETHODIMP SetHWAccel(LAVHWAccel hwAccel);
  STDMETHODIMP_(LAVHWAccel) GetHWAccel();
  STDMETHODIMP SetHWAccelCodec(LAVVideoHWCodec hwAccelCodec, BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetHWAccelCodec(LAVVideoHWCodec hwAccelCodec);
  STDMETHODIMP SetHWAccelDeintMode(LAVHWDeintModes deintMode);
  STDMETHODIMP_(LAVHWDeintModes) GetHWAccelDeintMode();
  STDMETHODIMP SetHWAccelDeintOutput(LAVDeintOutput deintOutput);
  STDMETHODIMP_(LAVDeintOutput) GetHWAccelDeintOutput();
  STDMETHODIMP SetHWAccelDeintHQ(BOOL bHQ);
  STDMETHODIMP_(BOOL) GetHWAccelDeintHQ();
  STDMETHODIMP SetSWDeintMode(LAVSWDeintModes deintMode);
  STDMETHODIMP_(LAVSWDeintModes) GetSWDeintMode();
  STDMETHODIMP SetSWDeintOutput(LAVDeintOutput deintOutput);
  STDMETHODIMP_(LAVDeintOutput) GetSWDeintOutput();

  STDMETHODIMP SetDeintTreatAsProgressive(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetDeintTreatAsProgressive();

  STDMETHODIMP SetDitherMode(LAVDitherMode ditherMode);
  STDMETHODIMP_(LAVDitherMode) GetDitherMode();

  STDMETHODIMP SetUseMSWMV9Decoder(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetUseMSWMV9Decoder();

  STDMETHODIMP SetDVDVideoSupport(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetDVDVideoSupport();

  STDMETHODIMP SetHWAccelResolutionFlags(DWORD dwResFlags);
  STDMETHODIMP_(DWORD) GetHWAccelResolutionFlags();

  STDMETHODIMP SetTrayIcon(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetTrayIcon();

  STDMETHODIMP SetDeinterlacingMode(LAVDeintMode deintMode);
  STDMETHODIMP_(LAVDeintMode) GetDeinterlacingMode();

  STDMETHODIMP SetGPUDeviceIndex(DWORD dwDevice);

  // ILAVVideoStatus
  STDMETHODIMP_(const WCHAR *) GetActiveDecoderName() { return m_Decoder.GetDecoderName(); }

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

  HRESULT CheckConnect(PIN_DIRECTION dir, IPin *pPin);
  HRESULT BreakConnect(PIN_DIRECTION dir);
  HRESULT CompleteConnect(PIN_DIRECTION dir, IPin *pReceivePin);

  int GetPinCount();
  CBasePin* GetPin(int n);

  STDMETHODIMP JoinFilterGraph(IFilterGraph * pGraph, LPCWSTR pName);

  // ILAVVideoCallback
  STDMETHODIMP AllocateFrame(LAVFrame **ppFrame);
  STDMETHODIMP ReleaseFrame(LAVFrame **ppFrame);
  STDMETHODIMP Deliver(LAVFrame *pFrame);
  STDMETHODIMP_(LPWSTR) GetFileExtension();
  STDMETHODIMP_(BOOL) FilterInGraph(PIN_DIRECTION dir, const GUID &clsid) { if (dir == PINDIR_INPUT) return FilterInGraphSafe(m_pInput, clsid); else return FilterInGraphSafe(m_pOutput, clsid); }
  STDMETHODIMP_(DWORD) GetDecodeFlags() { return m_dwDecodeFlags; }
  STDMETHODIMP_(CMediaType&) GetInputMediaType() { return m_pInput->CurrentMediaType(); }
  STDMETHODIMP GetLAVPinInfo(LAVPinInfo &info) { if (m_LAVPinInfoValid) { info = m_LAVPinInfo; return S_OK; } return E_FAIL; }
  STDMETHODIMP_(CBasePin*) GetOutputPin() { return m_pOutput; }
  STDMETHODIMP_(CMediaType&) GetOutputMediaType() { return m_pOutput->CurrentMediaType(); }
  STDMETHODIMP DVDStripPacket(BYTE*& p, long& len) { static_cast<CDeCSSTransformInputPin*>(m_pInput)->StripPacket(p, len); return S_OK; }
  STDMETHODIMP_(LAVFrame*) GetFlushFrame();
  STDMETHODIMP ReleaseAllDXVAResources() { ReleaseLastSequenceFrame(); return S_OK; }
  STDMETHODIMP_(DWORD) GetGPUDeviceIndex() { return m_dwGPUDeviceIndex; }

public:
  // Pin Configuration
  const static AMOVIESETUP_MEDIATYPE    sudPinTypesIn[];
  const static int                      sudPinTypesInCount;
  const static AMOVIESETUP_MEDIATYPE    sudPinTypesOut[];
  const static int                      sudPinTypesOutCount;

private:
  HRESULT LoadDefaults();
  HRESULT ReadSettings(HKEY rootKey);
  HRESULT LoadSettings();
  HRESULT SaveSettings();

  HRESULT CreateTrayIcon();

  HRESULT CreateDecoder(const CMediaType *pmt);

  HRESULT GetDeliveryBuffer(IMediaSample** ppOut, int width, int height, AVRational ar, DXVA2_ExtendedFormat dxvaExtFormat, REFERENCE_TIME avgFrameDuration);
  HRESULT ReconnectOutput(int width, int height, AVRational ar, DXVA2_ExtendedFormat dxvaExtFlags, REFERENCE_TIME avgFrameDuration, BOOL bDXVA = FALSE);

  HRESULT SetFrameFlags(IMediaSample* pMS, LAVFrame *pFrame);

  HRESULT NegotiatePixelFormat(CMediaType &mt, int width, int height);
  BOOL IsInterlaced();


  HRESULT Filter(LAVFrame *pFrame);
  HRESULT DeliverToRenderer(LAVFrame *pFrame);

  HRESULT PerformFlush();
  HRESULT ReleaseLastSequenceFrame();

  HRESULT GetD3DBuffer(LAVFrame *pFrame);
  HRESULT RedrawStillImage();
  HRESULT SetInDVDMenu(bool menu) { m_bInDVDMenu = menu; return S_OK; }

  enum {CNTRL_EXIT, CNTRL_REDRAW};
  HRESULT ControlCmd(DWORD cmd) {
    return m_ControlThread->CallWorker(cmd);
  }

private:
  friend class CVideoOutputPin;
  friend class CDecodeThread;
  friend class CLAVControlThread;
  friend class CLAVSubtitleProvider;
  friend class CLAVSubtitleConsumer;

  CDecodeThread        m_Decoder;
  CAMThread            *m_ControlThread = nullptr;

  REFERENCE_TIME       m_rtPrevStart = 0;
  REFERENCE_TIME       m_rtPrevStop  = 0;

  BOOL                 m_bForceInputAR  = FALSE;
  BOOL                 m_bSendMediaType = FALSE;
  BOOL                 m_bFlushing      = FALSE;

  HRESULT              m_hrDeliver      = S_OK;

  CLAVPixFmtConverter  m_PixFmtConverter;
  std::wstring         m_strExtension;

  BOOL                 m_bDXVAExtFormatSupport = TRUE;
  DWORD                m_bMadVR                = -1;
  DWORD                m_bOverlayMixer         = -1;
  DWORD                m_dwDecodeFlags         = 0;

  BOOL                 m_bInDVDMenu            = FALSE;

  AVFilterGraph        *m_pFilterGraph         = nullptr;
  AVFilterContext      *m_pFilterBufferSrc     = nullptr;
  AVFilterContext      *m_pFilterBufferSink    = nullptr;

  LAVPixelFormat       m_filterPixFmt          = LAVPixFmt_None;
  int                  m_filterWidth           = 0;
  int                  m_filterHeight          = 0;
  LAVFrame             m_FilterPrevFrame;

  BOOL                 m_LAVPinInfoValid       = FALSE;
  LAVPinInfo           m_LAVPinInfo;

  CLAVVideoSubtitleInputPin *m_pSubtitleInput  = nullptr;
  CLAVSubtitleConsumer *m_SubtitleConsumer     = nullptr;

  LAVFrame             *m_pLastSequenceFrame   = nullptr;

  AM_SimpleRateChange  m_DVDRate = AM_SimpleRateChange{AV_NOPTS_VALUE, 10000};

  BOOL                 m_bRuntimeConfig = FALSE;
  struct VideoSettings {
    BOOL TrayIcon;
    DWORD StreamAR;
    DWORD NumThreads;
    BOOL bFormats[Codec_VideoNB];
    BOOL bMSWMV9DMO;
    BOOL bPixFmts[LAVOutPixFmt_NB];
    DWORD RGBRange;
    DWORD HWAccel;
    BOOL bHWFormats[HWCodec_NB];
    DWORD HWAccelResFlags;
    DWORD HWDeintMode;
    DWORD HWDeintOutput;
    BOOL HWDeintHQ;
    DWORD DeintFieldOrder;
    LAVDeintMode DeintMode;
    DWORD SWDeintMode;
    DWORD SWDeintOutput;
    DWORD DitherMode;
    BOOL bDVDVideo;
  } m_settings;

  DWORD m_dwGPUDeviceIndex = DWORD_MAX;

  CBaseTrayIcon *m_pTrayIcon = nullptr;

#ifdef DEBUG
  FloatingAverage<double> m_pixFmtTimingAvg;
#endif
};
