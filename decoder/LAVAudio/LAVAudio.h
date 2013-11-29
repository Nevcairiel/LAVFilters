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

#include "LAVAudioSettings.h"
#include "FloatingAverage.h"
#include "Media.h"
#include "BitstreamParser.h"
#include "PostProcessor.h"

#include "ISpecifyPropertyPages2.h"
#include "BaseTrayIcon.h"

//////////////////// Configuration //////////////////////////

// Buffer Size for decoded PCM: 1s of 192kHz 32-bit with 8 channels
// 192000 (Samples) * 4 (Bytes per Sample) * 8 (channels)
#define LAV_AUDIO_BUFFER_SIZE 6144000

// Maximum Durations (in reference time)
// 10ms (DTS has 10.6667 ms samples, don't want to queue them up)
#define PCM_BUFFER_MAX_DURATION 100000
// 4ms
#define PCM_BUFFER_MIN_DURATION 40000

// Maximum desync that we attribute to jitter before re-syncing (10ms)
#define MAX_JITTER_DESYNC 100000i64

//////////////////// End Configuration //////////////////////

#define AV_CODEC_ID_PCM_SxxBE (AVCodecID)0x19001
#define AV_CODEC_ID_PCM_SxxLE (AVCodecID)0x19002
#define AV_CODEC_ID_PCM_UxxBE (AVCodecID)0x19003
#define AV_CODEC_ID_PCM_UxxLE (AVCodecID)0x19004
#define AV_CODEC_ID_PCM_QTRAW (AVCodecID)0x19005

#define LAVC_AUDIO_REGISTRY_KEY L"Software\\LAV\\Audio"
#define LAVC_AUDIO_REGISTRY_KEY_FORMATS L"Software\\LAV\\Audio\\Formats"
#define LAVC_AUDIO_LOG_FILE     L"LAVAudio.txt"

struct WAVEFORMATEX_HDMV_LPCM;

struct BufferDetails {
  GrowableArray<BYTE>   *bBuffer;         // PCM Buffer
  LAVAudioSampleFormat  sfFormat;         // Sample Format
  WORD                  wBitsPerSample;   // Bits per sample
  DWORD                 dwSamplesPerSec;  // Samples per second
  unsigned              nSamples;         // Samples in the buffer (every sample is sizeof(sfFormat) * nChannels in the buffer)
  WORD                  wChannels;        // Number of channels
  DWORD                 dwChannelMask;    // channel mask
  REFERENCE_TIME        rtStart;          // Start Time of the buffer
  BOOL                  bPlanar;          // Planar (not used)


  BufferDetails() : bBuffer(NULL), sfFormat(SampleFormat_16), wBitsPerSample(0), dwSamplesPerSec(0), wChannels(0), dwChannelMask(0), nSamples(0), rtStart(AV_NOPTS_VALUE), bPlanar(FALSE) {
    bBuffer = new GrowableArray<BYTE>();
  };
  ~BufferDetails() {
    delete bBuffer;
  }
};

struct DTSDecoder;

[uuid("E8E73B6B-4CB3-44A4-BE99-4F7BCB96E491")]
class CLAVAudio : public CTransformFilter, public ISpecifyPropertyPages2, public ILAVAudioSettings, public ILAVAudioStatus
{
public:
  CLAVAudio(LPUNKNOWN pUnk, HRESULT* phr);
  ~CLAVAudio();

  static void CALLBACK StaticInit(BOOL bLoading, const CLSID *clsid);

  // IUnknown
  DECLARE_IUNKNOWN;
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

  // ISpecifyPropertyPages2
  STDMETHODIMP GetPages(CAUUID *pPages);
  STDMETHODIMP CreatePage(const GUID& guid, IPropertyPage** ppPage);

  // ILAVAudioSettings
  STDMETHODIMP SetRuntimeConfig(BOOL bRuntimeConfig);
  STDMETHODIMP GetDRC(BOOL *pbDRCEnabled, int *piDRCLevel);
  STDMETHODIMP SetDRC(BOOL bDRCEnabled, int iDRCLevel);
  STDMETHODIMP_(BOOL) GetFormatConfiguration(LAVAudioCodec aCodec);
  STDMETHODIMP SetFormatConfiguration(LAVAudioCodec aCodec, BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetBitstreamConfig(LAVBitstreamCodec bsCodec);
  STDMETHODIMP SetBitstreamConfig(LAVBitstreamCodec bsCodec, BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetDTSHDFraming();
  STDMETHODIMP SetDTSHDFraming(BOOL bHDFraming);
  STDMETHODIMP_(BOOL) GetAutoAVSync();
  STDMETHODIMP SetAutoAVSync(BOOL bAutoSync);
  STDMETHODIMP_(BOOL) GetOutputStandardLayout();
  STDMETHODIMP SetOutputStandardLayout(BOOL bStdLayout);
  STDMETHODIMP_(BOOL) GetExpandMono();
  STDMETHODIMP SetExpandMono(BOOL bExpandMono);
  STDMETHODIMP_(BOOL) GetExpand61();
  STDMETHODIMP SetExpand61(BOOL bExpand61);
  STDMETHODIMP_(BOOL) GetAllowRawSPDIFInput();
  STDMETHODIMP SetAllowRawSPDIFInput(BOOL bAllow);
  STDMETHODIMP_(BOOL) GetSampleFormat(LAVAudioSampleFormat format);
  STDMETHODIMP SetSampleFormat(LAVAudioSampleFormat format, BOOL bEnabled);
  STDMETHODIMP GetAudioDelay(BOOL *pbEnabled, int *pDelay);
  STDMETHODIMP SetAudioDelay(BOOL bEnabled, int delay);
  STDMETHODIMP SetMixingEnabled(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetMixingEnabled();
  STDMETHODIMP SetMixingLayout(DWORD dwLayout);
  STDMETHODIMP_(DWORD) GetMixingLayout();
  STDMETHODIMP SetMixingFlags(DWORD dwFlags);
  STDMETHODIMP_(DWORD) GetMixingFlags();
  STDMETHODIMP SetMixingMode(LAVAudioMixingMode mixingMode);
  STDMETHODIMP_(LAVAudioMixingMode) GetMixingMode();
  STDMETHODIMP SetMixingLevels(DWORD dwCenterLevel, DWORD dwSurroundLevel, DWORD dwLFELevel);
  STDMETHODIMP GetMixingLevels(DWORD *dwCenterLevel, DWORD *dwSurroundLevel, DWORD *dwLFELevel);
  STDMETHODIMP SetTrayIcon(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetTrayIcon();
  STDMETHODIMP SetSampleConvertDithering(BOOL bEnabled);
  STDMETHODIMP_(BOOL) GetSampleConvertDithering();

  // ILAVAudioStatus
  STDMETHODIMP_(BOOL) IsSampleFormatSupported(LAVAudioSampleFormat sfCheck);
  STDMETHODIMP GetDecodeDetails(const char **pCodec, const char **pDecodeFormat, int *pnChannels, int *pSampleRate, DWORD *pChannelMask);
  STDMETHODIMP GetOutputDetails(const char **pOutputFormat, int *pnChannels, int *pSampleRate, DWORD *pChannelMask);
  STDMETHODIMP EnableVolumeStats();
  STDMETHODIMP DisableVolumeStats();
  STDMETHODIMP GetChannelVolumeAverage(WORD nChannel, float *pfDb);

  // CTransformFilter
  HRESULT CheckInputType(const CMediaType* mtIn);
  HRESULT CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut);
  HRESULT DecideBufferSize(IMemAllocator * pAllocator, ALLOCATOR_PROPERTIES *pprop);
  HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);

  HRESULT Receive(IMediaSample *pIn);

  STDMETHODIMP JoinFilterGraph(IFilterGraph * pGraph, LPCWSTR pName);

  // Optional Overrides
  HRESULT CheckConnect(PIN_DIRECTION dir, IPin *pPin);
  HRESULT SetMediaType(PIN_DIRECTION dir, const CMediaType *pmt);

  HRESULT EndOfStream();
  HRESULT BeginFlush();
  HRESULT EndFlush();
  HRESULT NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

  HRESULT BreakConnect(PIN_DIRECTION Dir);

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

  STDMETHODIMP CreateTrayIcon();

  HRESULT ffmpeg_init(AVCodecID codec, const void *format, GUID format_type, DWORD formatlen);
  void ffmpeg_shutdown();

  CMediaType CreateMediaType(LAVAudioSampleFormat outputFormat, DWORD nSamplesPerSec, WORD nChannels, DWORD dwChannelMask, WORD wBitsPerSample = 0) const;
  HRESULT ReconnectOutput(long cbBuffer, CMediaType& mt);
  HRESULT ProcessBuffer(BOOL bEOF = FALSE);
  HRESULT Decode(const BYTE *p, int buffsize, int &consumed, HRESULT *hrDeliver);
  HRESULT PostProcess(BufferDetails *buffer);
  HRESULT GetDeliveryBuffer(IMediaSample **pSample, BYTE **pData);

  HRESULT QueueOutput(BufferDetails &buffer);
  HRESULT FlushOutput(BOOL bDeliver = TRUE);
  HRESULT FlushDecoder();

  HRESULT Deliver(BufferDetails &buffer);

  void CreateBDLPCMHeader(BYTE *pBuf, const WAVEFORMATEX_HDMV_LPCM *wfex_lpcm) const;
  void CreateDVDLPCMHeader(BYTE *pBuf, const WAVEFORMATEX *wfex) const;
  HRESULT ParseRealAudioHeader(const BYTE *extra, const size_t extralen);

  void UpdateVolumeStats(const BufferDetails &buffer);

  BOOL IsBitstreaming(AVCodecID codec);
  HRESULT InitBitstreaming();
  HRESULT ShutdownBitstreaming();
  static int BSWriteBuffer(void *opaque, uint8_t *buf, int buf_size);

  HRESULT CreateBitstreamContext(AVCodecID codec, WAVEFORMATEX *wfe);
  HRESULT UpdateBitstreamContext();
  HRESULT FreeBitstreamContext();

  HRESULT Bitstream(const BYTE *p, int buffsize, int &consumed, HRESULT *hrDeliver);
  HRESULT DeliverBitstream(AVCodecID codec, const BYTE *buffer, DWORD dwSize, DWORD dwFrameSize, REFERENCE_TIME rtStartInput, REFERENCE_TIME rtStopInput);

  CMediaType CreateBitstreamMediaType(AVCodecID codec, DWORD dwSampleRate, BOOL bDTSHDOverride = FALSE);
  void ActivateDTSHDMuxing();

  HRESULT InitDTSDecoder();
  HRESULT FreeDTSDecoder();
  HRESULT FlushDTSDecoder(BOOL bReopen = FALSE);
  HRESULT DecodeDTS(const BYTE * const p, int buffsize, int &consumed, HRESULT *hrDeliver);
  int SafeDTSDecode(BYTE *pInput, int len, BYTE *pOutput, int unk1, int unk2, int *pBitdepth, int *pChannels, int *pCoreSampleRate, int *pUnk4, int *pHDSampleRate, int *pUnk5, int *pProfile);

  HRESULT CheckChannelLayoutConformity(DWORD dwLayout);
  HRESULT Create51Conformity(DWORD dwLayout);
  HRESULT Create61Conformity(DWORD dwLayout);
  HRESULT Create71Conformity(DWORD dwLayout);

  LAVAudioSampleFormat GetBestAvailableSampleFormat(LAVAudioSampleFormat inFormat, BOOL bNoFallback = FALSE);
  HRESULT Truncate32Buffer(BufferDetails *buffer);
  HRESULT PadTo32(BufferDetails *buffer);

  HRESULT PerformAVRProcessing(BufferDetails *buffer);

private:
  AVCodecID            m_nCodecId;       // FFMPEG Codec Id
  AVCodec              *m_pAVCodec;      // AVCodec reference
  AVCodecContext       *m_pAVCtx;        // AVCodecContext reference
  AVCodecParserContext *m_pParser;       // AVCodecParserContext reference

  AVFrame              *m_pFrame;        // AVFrame used for decoding

  BOOL                 m_bDiscontinuity; // Discontinuity
  REFERENCE_TIME       m_rtStart;        // Start time
  double               m_dStartOffset;   // Start time offset (extra precision)
  double               m_dRate;

  REFERENCE_TIME       m_rtStartInput;   // rtStart of the last input package
  REFERENCE_TIME       m_rtStopInput;    // rtStop of the last input package

  BOOL                 m_bUpdateTimeCache;
  REFERENCE_TIME       m_rtStartInputCache;   // rtStart of the last input package
  REFERENCE_TIME       m_rtStopInputCache;    // rtStop of the last input package
  REFERENCE_TIME       m_rtBitstreamCache;    // Bitstreaming time cache

  GrowableArray<BYTE>  m_buff;           // Input Buffer
  LAVAudioSampleFormat m_DecodeFormat;   // Decode Format
  LAVAudioSampleFormat m_MixingInputFormat;
  LAVAudioSampleFormat m_FallbackFormat;
  DWORD                m_dwOverrideMixer;

  BOOL                 m_bSampleSupport[SampleFormat_NB];

  BOOL                 m_bHasVideo;

  AVAudioResampleContext *m_avrContext;
  LAVAudioSampleFormat m_sfRemixFormat;
  DWORD                m_dwRemixLayout;
  BOOL                 m_bAVResampleFailed;
  BOOL                 m_bMixingSettingsChanged;
  float                m_fMixingClipThreshold;

  // Settings
  struct AudioSettings {
    BOOL TrayIcon;
    BOOL DRCEnabled;
    int DRCLevel;
    BOOL bFormats[Codec_AudioNB];
    BOOL bBitstream[Bitstream_NB];
    BOOL DTSHDFraming;
    BOOL AutoAVSync;
    BOOL ExpandMono;
    BOOL Expand61;
    BOOL OutputStandardLayout;
    BOOL AllowRawSPDIF;
    BOOL bSampleFormats[SampleFormat_NB];
    BOOL SampleConvertDither;
    BOOL AudioDelayEnabled;
    int  AudioDelay;

    BOOL MixingEnabled;
    DWORD MixingLayout;
    DWORD MixingFlags;
    DWORD MixingMode;
    DWORD MixingCenterLevel;
    DWORD MixingSurroundLevel;
    DWORD MixingLFELevel;
  } m_settings;
  BOOL                m_bRuntimeConfig;

  BOOL                m_bVolumeStats;    // Volume Stats gathering enabled
  FloatingAverage<float> m_faVolume[8];     // Floating Average for volume (8 channels)

  BOOL                m_bQueueResync;
  BOOL                m_bResyncTimestamp;
  BOOL                m_bNeedSyncpoint;
  BufferDetails       m_OutputQueue;

  AVIOContext *m_avioBitstream;
  GrowableArray<BYTE> m_bsOutput;

  AVFormatContext     *m_avBSContext;

  BOOL                m_bDTSHD;
  BOOL                m_bForceDTSCore;
  CBitstreamParser    m_bsParser;
  BOOL                m_bFindDTSInPCM;
  BOOL                m_bBitStreamingSettingsChanged;

  FloatingAverage<REFERENCE_TIME> m_faJitter;
  REFERENCE_TIME      m_JitterLimit;

  HMODULE             m_hDllExtraDecoder;

  DTSDecoder          *m_pDTSDecoderContext;
  unsigned            m_DTSBitDepth;
  unsigned            m_DTSDecodeChannels;

  DWORD               m_DecodeLayout;
  DWORD               m_DecodeLayoutSanified;
  DWORD               m_MixingInputLayout;
  BOOL                m_bChannelMappingRequired;

  ExtendedChannelMap  m_ChannelMap;
  int                 m_ChannelMapOutputChannels;
  DWORD               m_ChannelMapOutputLayout;

  struct {
    int flavor;
    int coded_frame_size;
    int audio_framesize;
    int sub_packet_h;
    int sub_packet_size;
    unsigned int deint_id;
  } m_raData;

  CBaseTrayIcon *m_pTrayIcon;
};
