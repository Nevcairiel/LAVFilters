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

#include "stdafx.h"
#include "LAVVideo.h"
#include "VideoSettingsProp.h"
#include "Media.h"
#include <dvdmedia.h>

#include "moreuuids.h"
#include "registry.h"

#include <Shlwapi.h>

// static constructor
CUnknown* WINAPI CLAVVideo::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr)
{
  return new CLAVVideo(pUnk, phr);
}

CLAVVideo::CLAVVideo(LPUNKNOWN pUnk, HRESULT* phr)
  : CTransformFilter(NAME("LAV Video Decoder"), 0, __uuidof(CLAVVideo))
  , m_bDXVA(FALSE)
  , m_nCodecId(CODEC_ID_NONE)
  , m_pAVCodec(NULL)
  , m_pAVCtx(NULL)
  , m_pParser(NULL)
  , m_rtAvrTimePerFrame(0)
  , m_pFrame(NULL)
  , m_pFFBuffer(NULL)
  , m_nFFBufferSize(0)
  , m_pSwsContext(NULL)
  , m_bProcessExtradata(FALSE)
  , m_bFFReordering(FALSE)
  , m_bCalculateStopTime(FALSE)
  , m_bRVDropBFrameTimings(FALSE)
  , m_rtPrevStart(0)
  , m_rtPrevStop(0)
  , m_rtStartCache(AV_NOPTS_VALUE)
  , m_bDiscontinuity(FALSE)
  , m_nThreads(1)
  , m_bForceTypeNegotiation(FALSE)
  , m_bRuntimeConfig(FALSE)
  , m_CurrentThread(0)
{
  avcodec_init();
  avcodec_register_all();

  LoadSettings();

  m_PixFmtConverter.SetSettings(this);

#ifdef DEBUG
  DbgSetModuleLevel (LOG_TRACE, DWORD_MAX);
  DbgSetModuleLevel (LOG_ERROR, DWORD_MAX);
  DbgSetModuleLevel (LOG_CUSTOM1, DWORD_MAX); // FFMPEG messages use custom1
#endif
}

CLAVVideo::~CLAVVideo()
{
  av_free(m_pFFBuffer);
  ffmpeg_shutdown();
}

void CLAVVideo::ffmpeg_shutdown()
{
  m_pAVCodec	= NULL;

  if (m_pParser) {
    av_parser_close(m_pParser);
    m_pParser = NULL;
  }

  if (m_pAVCtx) {
    avcodec_close(m_pAVCtx);
    av_free(m_pAVCtx->extradata);
    av_free(m_pAVCtx);
    m_pAVCtx = NULL;
  }
  if (m_pFrame) {
    av_free(m_pFrame);
    m_pFrame = NULL;
  }

  m_nCodecId = CODEC_ID_NONE;
}

HRESULT CLAVVideo::LoadDefaults()
{
  m_settings.StreamAR = TRUE;
  m_settings.InterlacedFlags = TRUE;
  m_settings.NumThreads = 0;

  for (int i = 0; i < Codec_NB; ++i)
    m_settings.bFormats[i] = TRUE;

  m_settings.bFormats[Codec_RV123]    = FALSE;
  m_settings.bFormats[Codec_RV4]      = FALSE;
  m_settings.bFormats[Codec_Lagarith] = FALSE;
  m_settings.bFormats[Codec_Cinepak]  = FALSE;

  for (int i = 0; i < LAVPixFmt_NB; ++i)
    m_settings.bPixFmts[i] = TRUE;

  return S_OK;
}

static const WCHAR* pixFmtSettingsMap[LAVPixFmt_NB] = {
  L"yv12", L"nv12", L"yuy2", L"uyvy", L"ayuv", L"p010", L"p210", L"y410", L"p016", L"p216", L"y416", L"rgb32", L"rgb24"
};

HRESULT CLAVVideo::LoadSettings()
{
  LoadDefaults();
  if (m_bRuntimeConfig)
    return S_FALSE;

  HRESULT hr;
  BOOL bFlag;
  DWORD dwVal;

  CreateRegistryKey(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY);
  CRegistry reg = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY, hr);
  // We don't check if opening succeeded, because the read functions will set their hr accordingly anyway,
  // and we need to fill the settings with defaults.
  // ReadString returns an empty string in case of failure, so thats fine!

  bFlag = reg.ReadDWORD(L"StreamAR", hr);
  if (SUCCEEDED(hr)) m_settings.StreamAR = bFlag;

  bFlag = reg.ReadDWORD(L"InterlacedFlags", hr);
  if (SUCCEEDED(hr)) m_settings.InterlacedFlags = bFlag;

  dwVal = reg.ReadDWORD(L"NumThreads", hr);
  if (SUCCEEDED(hr)) m_settings.NumThreads = dwVal;

  CreateRegistryKey(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_FORMATS);
  CRegistry regF = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_FORMATS, hr);

  for (int i = 0; i < Codec_NB; ++i) {
    const codec_config_t *info = get_codec_config((LAVVideoCodec)i);
    bFlag = regF.ReadBOOL(info->name, hr);
    if (SUCCEEDED(hr)) m_settings.bFormats[i] = bFlag;
  }

  CreateRegistryKey(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_OUTPUT);
  CRegistry regP = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_OUTPUT, hr);

  for (int i = 0; i < LAVPixFmt_NB; ++i) {
    bFlag = regP.ReadBOOL(pixFmtSettingsMap[i], hr);
    if (SUCCEEDED(hr)) m_settings.bPixFmts[i] = bFlag;
  }

  return S_OK;
}

HRESULT CLAVVideo::SaveSettings()
{
  if (m_bRuntimeConfig)
    return S_FALSE;

  HRESULT hr;
  CRegistry reg = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY, hr);
  if (SUCCEEDED(hr)) {
    reg.WriteBOOL(L"StreamAR", m_settings.StreamAR);
    reg.WriteBOOL(L"InterlacedFlags", m_settings.InterlacedFlags);
    reg.WriteDWORD(L"NumThreads", m_settings.NumThreads);

    CRegistry regF = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_FORMATS, hr);
    for (int i = 0; i < Codec_NB; ++i) {
      const codec_config_t *info = get_codec_config((LAVVideoCodec)i);
      regF.WriteBOOL(info->name, m_settings.bFormats[i]);
    }

    CRegistry regP = CRegistry(HKEY_CURRENT_USER, LAVC_VIDEO_REGISTRY_KEY_OUTPUT, hr);
    for (int i = 0; i < LAVPixFmt_NB; ++i) {
      regP.WriteBOOL(pixFmtSettingsMap[i], m_settings.bPixFmts[i]);
    }
  }
  return S_OK;
}

// IUnknown
STDMETHODIMP CLAVVideo::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  CheckPointer(ppv, E_POINTER);

  *ppv = NULL;

  return
    QI2(ISpecifyPropertyPages)
    QI2(ILAVVideoSettings)
    __super::NonDelegatingQueryInterface(riid, ppv);
}

// ISpecifyPropertyPages
STDMETHODIMP CLAVVideo::GetPages(CAUUID *pPages)
{
  CheckPointer(pPages, E_POINTER);
  pPages->cElems = 2;
  pPages->pElems = (GUID *)CoTaskMemAlloc(sizeof(GUID) * pPages->cElems);
  if (pPages->pElems == NULL) {
    return E_OUTOFMEMORY;
  }
  pPages->pElems[0] = CLSID_LAVVideoSettingsProp;
  pPages->pElems[1] = CLSID_LAVVideoFormatsProp;
  return S_OK;
}

// CTransformFilter
HRESULT CLAVVideo::CheckInputType(const CMediaType *mtIn)
{
  for(int i = 0; i < sudPinTypesInCount; i++) {
    if(*sudPinTypesIn[i].clsMajorType == mtIn->majortype
      && *sudPinTypesIn[i].clsMinorType == mtIn->subtype && (mtIn->formattype == FORMAT_VideoInfo || mtIn->formattype == FORMAT_VideoInfo2 || mtIn->formattype == FORMAT_MPEGVideo || mtIn->formattype == FORMAT_MPEG2Video)) {
        return S_OK;
    }
  }
  return VFW_E_TYPE_NOT_ACCEPTED;
}

// Check if the types are compatible
HRESULT CLAVVideo::CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut)
{
  if (SUCCEEDED(CheckInputType(mtIn)) && mtOut->majortype == MEDIATYPE_Video) {
    if (m_PixFmtConverter.IsAllowedSubtype(&mtOut->subtype)) {
      return S_OK;
    }
  }
  return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CLAVVideo::DecideBufferSize(IMemAllocator* pAllocator, ALLOCATOR_PROPERTIES* pProperties)
{
  if(m_pInput->IsConnected() == FALSE) {
    return E_UNEXPECTED;
  }

  BITMAPINFOHEADER *pBIH = NULL;
  CMediaType mtOut = m_pOutput->CurrentMediaType();
  formatTypeHandler(mtOut.Format(), mtOut.FormatType(), &pBIH, NULL);

  pProperties->cBuffers = 1;
  pProperties->cbBuffer = pBIH->biSizeImage;
  pProperties->cbAlign  = 1;
  pProperties->cbPrefix = 0;

  HRESULT hr;
  ALLOCATOR_PROPERTIES Actual;
  if(FAILED(hr = pAllocator->SetProperties(pProperties, &Actual))) {
    return hr;
  }

  return pProperties->cBuffers > Actual.cBuffers || pProperties->cbBuffer > Actual.cbBuffer
    ? E_FAIL : S_OK;
}

HRESULT CLAVVideo::GetMediaType(int iPosition, CMediaType *pMediaType)
{
  if(m_pInput->IsConnected() == FALSE) {
    return E_UNEXPECTED;
  }
  if(iPosition < 0) {
    return E_INVALIDARG;
  }

  if(iPosition >= m_PixFmtConverter.GetNumMediaTypes()) {
    return VFW_S_NO_MORE_ITEMS;
  }

  CMediaType mtIn = m_pInput->CurrentMediaType();

  BITMAPINFOHEADER *pBIH = NULL;
  REFERENCE_TIME rtAvgTime;
  DWORD dwAspectX = 0, dwAspectY = 0;
  formatTypeHandler(mtIn.Format(), mtIn.FormatType(), &pBIH, &rtAvgTime, &dwAspectX, &dwAspectY);

  *pMediaType = m_PixFmtConverter.GetMediaType(iPosition, pBIH->biWidth, pBIH->biHeight, dwAspectX, dwAspectY, rtAvgTime);

  return S_OK;
}

#define MPEG12_CODEC(codec) ((codec == CODEC_ID_MPEG1VIDEO) || (codec == CODEC_ID_MPEG2VIDEO))

HRESULT CLAVVideo::ffmpeg_init(CodecID codec, const CMediaType *pmt)
{
  ffmpeg_shutdown();

  for(int i = 0; i < Codec_NB; ++i) {
    const codec_config_t *config = get_codec_config((LAVVideoCodec)i);
    bool bMatched = false;
    for (int k = 0; k < config->nCodecs; ++k) {
      if (config->codecs[k] == codec) {
        bMatched = true;
        break;
      }
    }
    if (bMatched && !m_settings.bFormats[i]) {
      return VFW_E_UNSUPPORTED_VIDEO;
    }
  }

  BITMAPINFOHEADER *pBMI = NULL;
  formatTypeHandler((const BYTE *)pmt->Format(), pmt->FormatType(), &pBMI, &m_rtAvrTimePerFrame);

  m_strExtension = L"";

  IFileSourceFilter *pSource = NULL;
  if (SUCCEEDED(FindIntefaceInGraph(m_pInput, IID_IFileSourceFilter, (void **)&pSource))) {
    LPOLESTR pwszFile = NULL;
    if (SUCCEEDED(pSource->GetCurFile(&pwszFile, NULL)) && pwszFile) {
      LPWSTR pwszExtension = PathFindExtensionW(pwszFile);
      m_strExtension = std::wstring(pwszExtension);
      CoTaskMemFree(pwszFile);
    }
    SafeRelease(&pSource);
  }

  m_pAVCodec    = avcodec_find_decoder(codec);
  CheckPointer(m_pAVCodec, VFW_E_UNSUPPORTED_VIDEO);

  m_pAVCtx = avcodec_alloc_context3(m_pAVCodec);
  CheckPointer(m_pAVCtx, E_POINTER);

  m_pAVCtx->codec_type            = AVMEDIA_TYPE_VIDEO;
  m_pAVCtx->codec_id              = (CodecID)codec;
  m_pAVCtx->codec_tag             = pBMI->biCompression;
  
  if(MPEG12_CODEC(codec)) {
    m_pParser = av_parser_init(codec);
  }

  m_pAVCtx->width = pBMI->biWidth;
  m_pAVCtx->height = pBMI->biHeight;
  m_pAVCtx->codec_tag = pBMI->biCompression;
  m_pAVCtx->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  m_pAVCtx->error_recognition = FF_ER_CAREFUL;
  m_pAVCtx->workaround_bugs = FF_BUG_AUTODETECT;
  m_pAVCtx->bits_per_coded_sample = pBMI->biBitCount;

  int thread_type = getThreadFlags(codec);
  if (thread_type) {
    // Thread Count. 0 = auto detect
    int thread_count = m_settings.NumThreads;
    if (thread_count == 0) {
      SYSTEM_INFO systemInfo;
      GetNativeSystemInfo(&systemInfo);
      thread_count = systemInfo.dwNumberOfProcessors * 3 / 2;
    }

    m_pAVCtx->thread_count = min(thread_count, MAX_THREADS);
    m_pAVCtx->thread_type = thread_type;
  } else {
    m_pAVCtx->thread_count = 1;
  }

  m_pFrame = avcodec_alloc_frame();
  CheckPointer(m_pFrame, E_POINTER);

  BYTE *extra = NULL;
  unsigned int extralen = 0;

  getExtraData((const BYTE *)pmt->Format(), pmt->FormatType(), NULL, &extralen);

  m_h264RandomAccess.SetAVCNALSize(0);
  m_h264RandomAccess.flush(m_pAVCtx->thread_count);

  if (extralen > 0) {
    // Reconstruct AVC1 extradata format
    if (pmt->formattype == FORMAT_MPEG2Video && (m_pAVCtx->codec_tag == MAKEFOURCC('a','v','c','1') || m_pAVCtx->codec_tag == MAKEFOURCC('A','V','C','1') || m_pAVCtx->codec_tag == MAKEFOURCC('C','C','V','1'))) {
      MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)pmt->Format();
      extralen += 7;
      extra = (uint8_t *)av_mallocz(extralen + FF_INPUT_BUFFER_PADDING_SIZE);
      extra[0] = 1;
      extra[1] = (BYTE)mp2vi->dwProfile;
      extra[2] = 0;
      extra[3] = (BYTE)mp2vi->dwLevel;
      extra[4] = (BYTE)(mp2vi->dwFlags ? mp2vi->dwFlags : 2) - 1;

      // Actually copy the metadata into our new buffer
      unsigned int actual_len;
      getExtraData((const BYTE *)pmt->Format(), pmt->FormatType(), extra+6, &actual_len);

      // Count the number of SPS/PPS in them and set the length
      // We'll put them all into one block and add a second block with 0 elements afterwards
      // The parsing logic does not care what type they are, it just expects 2 blocks.
      BYTE *p = extra+6, *end = extra+6+actual_len;
      int count = 0;
      while (p+1 < end) {
        unsigned len = (((unsigned)p[0] << 8) | p[1]) + 2;
        if (p + len > end) {
          break;
        }
        count++;
        p += len;
      }
      extra[5] = count;
      extra[extralen-1] = 0;

      m_h264RandomAccess.SetAVCNALSize(mp2vi->dwFlags);
    } else {
      // Just copy extradata for other formats
      extra = (uint8_t *)av_mallocz(extralen + FF_INPUT_BUFFER_PADDING_SIZE);
      getExtraData((const BYTE *)pmt->Format(), pmt->FormatType(), extra, NULL);
    }
    m_pAVCtx->extradata = extra;
    m_pAVCtx->extradata_size = extralen;
  }

  m_nCodecId                      = codec;

  BOOL bVC1OnMPC = (codec == CODEC_ID_VC1 && (FilterInGraph(CLSID_MPCHCMPEGSplitter, m_pGraph) || FilterInGraph(CLSID_MPCHCMPEGSplitterSource, m_pGraph)));

  m_bFFReordering      = ((codec == CODEC_ID_H264 && m_strExtension != L".avi") || codec == CODEC_ID_VP8 || codec == CODEC_ID_VP3 || codec == CODEC_ID_THEORA || codec == CODEC_ID_HUFFYUV || bVC1OnMPC);
  m_bCalculateStopTime = (codec == CODEC_ID_H264 || bVC1OnMPC);

  m_bRVDropBFrameTimings = (m_nCodecId == CODEC_ID_RV10 || m_nCodecId == CODEC_ID_RV20 || m_strExtension == L".mkv" || !(FilterInGraph(CLSID_LAVSplitter, m_pGraph) || FilterInGraph(CLSID_LAVSplitterSource, m_pGraph)));

  int ret = avcodec_open2(m_pAVCtx, m_pAVCodec, NULL);
  if (ret >= 0) {

  } else {
    return VFW_E_UNSUPPORTED_VIDEO;
  }

  // Detect MPEG-2 chroma format
  if (codec == CODEC_ID_MPEG2VIDEO && m_pAVCtx->extradata && m_pAVCtx->extradata_size) {
    uint32_t state = -1;
    uint8_t *data  = m_pAVCtx->extradata;
    int size       = m_pAVCtx->extradata_size;
    while(size > 2 && state != 0x000001b5) {
      state = (state << 8) | *data++;
      size--;
    }
    if (state == 0x000001b5) {
      int start_code = (*data) >> 4;
      if (start_code == 1) {
        data++;
        int chroma = ((*data) & 0x6) >> 1;
        if (chroma < 2) {
          m_pAVCtx->pix_fmt = PIX_FMT_YUV420P;
        } else if (chroma == 2) {
          m_pAVCtx->pix_fmt = PIX_FMT_YUV422P;
        } else {
          m_pAVCtx->pix_fmt = PIX_FMT_YUV444P;
        }
      }
    }
  }

  m_PixFmtConverter.SetInputPixFmt(m_pAVCtx->pix_fmt);
  m_bForceTypeNegotiation = TRUE;

  return S_OK;
}

HRESULT CLAVVideo::SetMediaType(PIN_DIRECTION dir, const CMediaType *pmt)
{
  HRESULT hr = S_OK;
  DbgLog((LOG_TRACE, 5, L"SetMediaType -- %S", dir == PINDIR_INPUT ? "in" : "out"));
  if (dir == PINDIR_INPUT) {
    CodecID codec = CODEC_ID_NONE;
    const void *format = pmt->Format();
    GUID format_type = pmt->formattype;

    codec = FindCodecId(pmt);

    if (codec == CODEC_ID_NONE) {
      return VFW_E_TYPE_NOT_ACCEPTED;
    }

    hr = ffmpeg_init(codec, pmt);
    if (FAILED(hr)) {
      return hr;
    }
  } else if (dir == PINDIR_OUTPUT) {
    m_PixFmtConverter.SetOutputPixFmt(m_PixFmtConverter.GetOutputBySubtype(pmt->Subtype()));
  }
  return __super::SetMediaType(dir, pmt);
}

HRESULT CLAVVideo::EndOfStream()
{
  DbgLog((LOG_TRACE, 1, L"EndOfStream"));
  CAutoLock cAutoLock(&m_csReceive);

  // Run decode without data until all frames are out
  REFERENCE_TIME rtStart = AV_NOPTS_VALUE, rtStop = AV_NOPTS_VALUE;
  Decode(NULL, 0, rtStart, rtStop);

  return __super::EndOfStream();
}

HRESULT CLAVVideo::BeginFlush()
{
  DbgLog((LOG_TRACE, 1, L"BeginFlush"));
  return __super::BeginFlush();
}

HRESULT CLAVVideo::EndFlush()
{
  DbgLog((LOG_TRACE, 1, L"EndFlush"));
  m_rtPrevStop = 0;
  return __super::EndFlush();
}

HRESULT CLAVVideo::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
  DbgLog((LOG_TRACE, 1, L"NewSegment - %d / %d", tStart, tStop));
  CAutoLock cAutoLock(&m_csReceive);

  if (m_pAVCtx) {
    avcodec_flush_buffers (m_pAVCtx);
    m_h264RandomAccess.flush(m_pAVCtx->thread_count);
  }

  m_CurrentThread = 0;
  m_rtStartCache = AV_NOPTS_VALUE;

  return __super::NewSegment(tStart, tStop, dRate);
}

HRESULT CLAVVideo::GetDeliveryBuffer(IMediaSample** ppOut, int width, int height, AVRational ar)
{
  CheckPointer(ppOut, E_POINTER);

  HRESULT hr;

  if(FAILED(hr = ReconnectOutput(width, height, ar))) {
    return hr;
  }

  if(FAILED(hr = m_pOutput->GetDeliveryBuffer(ppOut, NULL, NULL, 0))) {
    return hr;
  }

  AM_MEDIA_TYPE* pmt = NULL;
  if(SUCCEEDED((*ppOut)->GetMediaType(&pmt)) && pmt) {
    CMediaType mt = *pmt;
    m_pOutput->SetMediaType(&mt);
    DeleteMediaType(pmt);
    pmt = NULL;
  }

  (*ppOut)->SetDiscontinuity(FALSE);
  (*ppOut)->SetSyncPoint(TRUE);

  return S_OK;
}

HRESULT CLAVVideo::ReconnectOutput(int width, int height, AVRational ar)
{
  CMediaType& mt = m_pOutput->CurrentMediaType();
  VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)mt.Format();

  HRESULT hr = S_FALSE;

  int num = width, den = height;
  av_reduce(&num, &den, (int64_t)ar.num * num, (int64_t)ar.den * den, 255);
  if (!m_settings.StreamAR || num == 0 || den == 0) {
    num = vih2->dwPictAspectRatioX;
    den = vih2->dwPictAspectRatioY;
  }

  if (vih2->rcTarget.right != width
    || vih2->rcTarget.bottom != height
    || vih2->dwPictAspectRatioX != num
    || vih2->dwPictAspectRatioY != den
    || m_bForceTypeNegotiation) {

    m_bForceTypeNegotiation = FALSE;
    
    vih2->bmiHeader.biWidth = width;
    vih2->bmiHeader.biHeight = height;
    vih2->dwPictAspectRatioX = num;
    vih2->dwPictAspectRatioY = den;

    SetRect(&vih2->rcSource, 0, 0, width, height);
    SetRect(&vih2->rcTarget, 0, 0, width, height);

    vih2->bmiHeader.biSizeImage = width * height * vih2->bmiHeader.biBitCount >> 3;

    if (mt.subtype == MEDIASUBTYPE_RGB32 || mt.subtype == MEDIASUBTYPE_RGB24) {
      vih2->bmiHeader.biHeight = -vih2->bmiHeader.biHeight;
    }

    hr = m_pOutput->GetConnected()->QueryAccept(&mt);
    if(SUCCEEDED(hr = m_pOutput->GetConnected()->ReceiveConnection(m_pOutput, &mt))) {
      IMediaSample *pOut = NULL;
      if (SUCCEEDED(hr = m_pOutput->GetDeliveryBuffer(&pOut, NULL, NULL, 0))) {
        AM_MEDIA_TYPE *pmt = NULL;
        if(SUCCEEDED(pOut->GetMediaType(&pmt)) && pmt) {
          CMediaType mt = *pmt;
          m_pOutput->SetMediaType(&mt);
#ifdef DEBUG
          VIDEOINFOHEADER2 *vih2new = (VIDEOINFOHEADER2 *)mt.Format();
          DbgLog((LOG_TRACE, 10, L"New MediaType negotiated; actual width: %d - renderer requests: %ld", width, vih2new->bmiHeader.biWidth));
#endif
          DeleteMediaType(pmt);
        } else { // No Stride Request? We're ok with that, too!
          DbgLog((LOG_TRACE, 10, L"We did not get a stride request, sending width no stride; width: %d", width));
        }
        pOut->Release();
      }
    }
    NotifyEvent(EC_VIDEO_SIZE_CHANGED, MAKELPARAM(width, height), 0);

    hr = S_OK;
  }

  return hr;
}

HRESULT CLAVVideo::NegotiatePixelFormat(CMediaType &outMt, int width, int height)
{
  HRESULT hr = S_OK;
  int i = 0;

  VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)outMt.Format();

  for (i = 0; i < m_PixFmtConverter.GetNumMediaTypes(); ++i) {
    CMediaType &mt = m_PixFmtConverter.GetMediaType(i, width, height, vih2->dwPictAspectRatioX, vih2->dwPictAspectRatioY, vih2->AvgTimePerFrame);
    //hr = m_pOutput->GetConnected()->QueryAccept(&mt);
    hr = m_pOutput->GetConnected()->ReceiveConnection(m_pOutput, &mt);
    if (hr == S_OK) {
      DbgLog((LOG_TRACE, 10, L"::NegotiatePixelFormat(): Filter accepted format with index %d", i));
      m_bForceTypeNegotiation = TRUE;
      m_pOutput->SetMediaType(&mt);
      return S_OK;
    }
  }

  DbgLog((LOG_ERROR, 10, L"::NegotiatePixelFormat(): Unable to agree on a pixel format", i));
  return E_FAIL;
}

HRESULT CLAVVideo::Receive(IMediaSample *pIn)
{
  CAutoLock cAutoLock(&m_csReceive);
  HRESULT        hr = S_OK;
  BYTE           *pDataIn = NULL;
  int            nSize;
  REFERENCE_TIME rtStart = AV_NOPTS_VALUE;
  REFERENCE_TIME rtStop = AV_NOPTS_VALUE;

  AM_SAMPLE2_PROPERTIES const *pProps = m_pInput->SampleProps();
  if(pProps->dwStreamId != AM_STREAM_MEDIA) {
    return m_pOutput->Deliver(pIn);
  }

  AM_MEDIA_TYPE *pmt = NULL;
  if (SUCCEEDED(pIn->GetMediaType(&pmt)) && pmt) {
    CMediaType mt = *pmt;
    m_pInput->SetMediaType(&mt);
    DeleteMediaType(pmt);
  }

  if (FAILED(hr = pIn->GetPointer(&pDataIn))) {
    return hr;
  }

  nSize = pIn->GetActualDataLength();
  hr = pIn->GetTime(&rtStart, &rtStop);

  if (FAILED(hr)) {
    rtStart = rtStop = AV_NOPTS_VALUE;
  }

  if (rtStop-1 <= rtStart) {
    rtStop = AV_NOPTS_VALUE;
  }

  if (pIn->IsDiscontinuity() == S_OK) {
    m_bDiscontinuity = TRUE;
  }

  hr = Decode(pDataIn, nSize, rtStart, rtStop);

  return hr;
}

HRESULT CLAVVideo::Decode(BYTE *pDataIn, int nSize, const REFERENCE_TIME rtStartIn, REFERENCE_TIME rtStopIn)
{
  HRESULT hr = S_OK;
  int     got_picture = 0;
  int     used_bytes  = 0;

  IMediaSample *pOut = NULL;
  BYTE         *pDataOut = NULL;
  BOOL         bFlush = FALSE;

  AVPacket avpkt;
  av_init_packet(&avpkt);

  if (m_bProcessExtradata && m_pAVCtx->extradata && m_pAVCtx->extradata_size > 0) {
    avpkt.data = m_pAVCtx->extradata;
    avpkt.size = m_pAVCtx->extradata_size;
    used_bytes = avcodec_decode_video2 (m_pAVCtx, m_pFrame, &got_picture, &avpkt);
    av_free(m_pAVCtx->extradata);
    m_pAVCtx->extradata = NULL;
    m_pAVCtx->extradata_size = 0;
  }

  if (pDataIn == NULL) {
    bFlush = TRUE;
  }

  if (m_pAVCtx->active_thread_type & FF_THREAD_FRAME) {
    if (!m_bFFReordering) {
      m_tcThreadBuffer[m_CurrentThread].rtStart = rtStartIn;
      m_tcThreadBuffer[m_CurrentThread].rtStop  = rtStopIn;
    }

    m_CurrentThread++;
    if (m_CurrentThread >= m_pAVCtx->thread_count) {
      m_CurrentThread = 0;
    }
  }

  if (m_nCodecId == CODEC_ID_H264 && !bFlush) {
    BOOL bRecovered = m_h264RandomAccess.searchRecoveryPoint(pDataIn, nSize);
    if (!bRecovered) {
      return S_OK;
    }
  }

  while (nSize > 0 || bFlush) {

    REFERENCE_TIME rtStart = rtStartIn, rtStop = rtStopIn;

    if (!bFlush) {
      if (nSize+FF_INPUT_BUFFER_PADDING_SIZE > m_nFFBufferSize) {
        m_nFFBufferSize	= nSize + FF_INPUT_BUFFER_PADDING_SIZE;
        m_pFFBuffer = (BYTE *)av_realloc(m_pFFBuffer, m_nFFBufferSize);
      }

      memcpy(m_pFFBuffer, pDataIn, nSize);
      memset(m_pFFBuffer+nSize, 0, FF_INPUT_BUFFER_PADDING_SIZE);

      avpkt.data = m_pFFBuffer;
      avpkt.size = nSize;
      avpkt.pts = rtStartIn;
      avpkt.dts = rtStopIn;
      //avpkt.duration = (int)(rtStart != _I64_MIN && rtStop != _I64_MIN ? rtStop - rtStart : 0);
      avpkt.flags = AV_PKT_FLAG_KEY;
    } else {
      avpkt.data = NULL;
      avpkt.size = 0;
      //DbgLog((LOG_TRACE, 10, L"Flushing Frame", rtStart));
    }

    if (m_pParser) {
      BYTE *pOut = NULL;
      int pOut_size = 0;

      used_bytes = av_parser_parse2(m_pParser, m_pAVCtx, &pOut, &pOut_size, avpkt.data, avpkt.size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

      if (used_bytes == 0 && pOut_size == 0 && !bFlush) {
        DbgLog((LOG_TRACE, 50, L"::Decode() - could not process buffer, starving?"));
        break;
      }

      if (used_bytes >= pOut_size) {
        if (rtStartIn != AV_NOPTS_VALUE)
          m_rtStartCache = rtStartIn;
      } else if (pOut_size > used_bytes) {
        if (used_bytes != avpkt.size || rtStart == AV_NOPTS_VALUE)
          rtStart = m_rtStartCache;
        m_rtStartCache = rtStartIn;
      }

      if (pOut_size > 0 || bFlush) {

        if (pOut_size > 0) {
          // Copy output data into the work buffer
          if (pOut_size+FF_INPUT_BUFFER_PADDING_SIZE > m_nFFBufferSize) {
            m_nFFBufferSize	= pOut_size + FF_INPUT_BUFFER_PADDING_SIZE;
            m_pFFBuffer = (BYTE *)av_realloc(m_pFFBuffer, m_nFFBufferSize);
          }

          memcpy(m_pFFBuffer, pOut, pOut_size);
          memset(m_pFFBuffer+pOut_size, 0, FF_INPUT_BUFFER_PADDING_SIZE);

          avpkt.data = m_pFFBuffer;
          avpkt.size = pOut_size;
          avpkt.pts = rtStart;
          avpkt.dts = AV_NOPTS_VALUE;
        }

        int ret2 = avcodec_decode_video2 (m_pAVCtx, m_pFrame, &got_picture, &avpkt);
        if (ret2 < 0) {
          DbgLog((LOG_TRACE, 50, L"::Decode() - decoding failed despite successfull parsing"));
          continue;
        }
      } else {
        got_picture = 0;
      }
    } else {
      used_bytes = avcodec_decode_video2 (m_pAVCtx, m_pFrame, &got_picture, &avpkt);
    }

    if (used_bytes < 0) {
      return S_OK;
    }

    // When Frame Threading, we won't know how much data has been consumed, so it by default eats everything.
    if (bFlush || (m_pAVCtx->active_thread_type & FF_THREAD_FRAME && !m_pParser) || m_nCodecId == CODEC_ID_MJPEGB) {
      nSize = 0;
    } else {
      nSize -= used_bytes;
      pDataIn += used_bytes;
    }

    if (m_nCodecId == CODEC_ID_H264) {
      m_h264RandomAccess.judgeFrameUsability(m_pFrame, &got_picture);
    }

    if (!got_picture || !m_pFrame->data[0]) {
      bFlush = FALSE;
      //DbgLog((LOG_TRACE, 10, L"No picture"));
      continue;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // The next big block computes the proper timestamps
    ///////////////////////////////////////////////////////////////////////////////////////////////
    if (MPEG12_CODEC(m_nCodecId)) {
      if (m_bDiscontinuity && m_pFrame->pict_type == AV_PICTURE_TYPE_I && m_pFrame->pkt_pts != AV_NOPTS_VALUE) {
        rtStart = m_pFrame->pkt_pts;
        m_bDiscontinuity = FALSE;
        DbgLog((LOG_TRACE, 20, L"::Decode() - Synced MPEG-2 timestamps to %I64d", rtStart));
      } else {
        rtStart = m_rtPrevStop;
      }

      REFERENCE_TIME duration = (REF_SECOND_MULT * m_pAVCtx->time_base.num / m_pAVCtx->time_base.den) * m_pAVCtx->ticks_per_frame;
      rtStop = rtStart + (duration * (m_pFrame->repeat_pict ? 3 : 2)  / 2);
    } else if (m_bFFReordering) {
      rtStart = m_pFrame->pkt_pts;
      rtStop = m_pFrame->pkt_dts;
    } else if (m_nCodecId == CODEC_ID_RV10 || m_nCodecId == CODEC_ID_RV20 || m_nCodecId == CODEC_ID_RV30 || m_nCodecId == CODEC_ID_RV40) {
      if (m_bRVDropBFrameTimings && m_pFrame->pict_type == AV_PICTURE_TYPE_B)
        rtStart = AV_NOPTS_VALUE;
    } else if (m_pAVCtx->active_thread_type & FF_THREAD_FRAME) {
      unsigned index = m_CurrentThread;
      rtStart = m_tcThreadBuffer[index].rtStart;
      rtStop  = m_tcThreadBuffer[index].rtStop;
    }

    if (rtStart == AV_NOPTS_VALUE) {
      rtStart = m_rtPrevStop;
      rtStop = AV_NOPTS_VALUE;
    }

    if (m_bCalculateStopTime || rtStop == AV_NOPTS_VALUE) {
      REFERENCE_TIME duration = 0;

      CMediaType mt = m_pInput->CurrentMediaType();
      formatTypeHandler(mt.Format(), mt.FormatType(), NULL, &duration, NULL, NULL);
      if (!duration && m_pAVCtx->time_base.num && m_pAVCtx->time_base.den) {
        duration = (REF_SECOND_MULT * m_pAVCtx->time_base.num / m_pAVCtx->time_base.den) * m_pAVCtx->ticks_per_frame;
      } else if(!duration) {
        duration = 1;
      }
      
      rtStop = rtStart + (duration * (m_pFrame->repeat_pict ? 3 : 2)  / 2);
    }

    DbgLog((LOG_TRACE, 10, L"Frame, rtStart: %I64d, diff: %I64d, key: %d, ref: %d, type: %c, repeat: %d, tidx: %d", rtStart, rtStart-m_rtPrevStart, m_pFrame->key_frame, m_pFrame->reference, av_get_picture_type_char(m_pFrame->pict_type), m_pFrame->repeat_pict, m_CurrentThread));

    m_rtPrevStart = rtStart;
    m_rtPrevStop = rtStop;

    if (rtStart < 0) {
      continue;
    }

    // Collect width/height
    int width  = m_pAVCtx->width;
    int height = m_pAVCtx->height;

    if (width == 1920 && height == 1088) {
      height = 1080;
    }

    if (m_pAVCtx->pix_fmt != m_PixFmtConverter.GetInputPixFmt()) {
      DbgLog((LOG_TRACE, 10, L"::Decode(): Changing pixel format from %d to %d",  m_PixFmtConverter.GetInputPixFmt(), m_pAVCtx->pix_fmt));
      m_PixFmtConverter.SetInputPixFmt(m_pAVCtx->pix_fmt);

      CMediaType& mt = m_pOutput->CurrentMediaType();

      if (m_PixFmtConverter.GetOutputBySubtype(mt.Subtype()) != m_PixFmtConverter.GetPreferredOutput()) {
        NegotiatePixelFormat(mt, width, height);
      }
    }

    if(FAILED(hr = GetDeliveryBuffer(&pOut, width, height, m_pAVCtx->sample_aspect_ratio)) || FAILED(hr = pOut->GetPointer(&pDataOut))) {
      return hr;
    }

    pOut->SetTime(&rtStart, &rtStop);
    pOut->SetMediaTime(NULL, NULL);

    CMediaType& mt = m_pOutput->CurrentMediaType();
    VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)mt.Format();

    m_PixFmtConverter.Convert(m_pFrame, pDataOut, width, height, vih2->bmiHeader.biWidth);

    SetTypeSpecificFlags (pOut);
    hr = m_pOutput->Deliver(pOut);
    SafeRelease(&pOut);

    if (bFlush) {
      m_CurrentThread++;
      if (m_CurrentThread >= m_pAVCtx->thread_count)
        m_CurrentThread = 0;
    }
  }
  return hr;
}

HRESULT CLAVVideo::SetTypeSpecificFlags(IMediaSample* pMS)
{
  HRESULT hr = S_OK;
  IMediaSample2 *pMS2 = NULL;
  if (SUCCEEDED(hr = pMS->QueryInterface(&pMS2))) {
    AM_SAMPLE2_PROPERTIES props;
    if(SUCCEEDED(pMS2->GetProperties(sizeof(props), (BYTE*)&props))) {
      props.dwTypeSpecificFlags &= ~0x7f;

      if(!m_pFrame->interlaced_frame) {
        props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_WEAVE;
      } else {
        if(m_pFrame->top_field_first) {
          props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_FIELD1FIRST;
        }
      }

      switch (m_pFrame->pict_type) {
      case AV_PICTURE_TYPE_I:
      case AV_PICTURE_TYPE_SI:
        props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_I_SAMPLE;
        break;
      case AV_PICTURE_TYPE_P:
      case AV_PICTURE_TYPE_SP:
        props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_P_SAMPLE;
        break;
      case AV_PICTURE_TYPE_B:
      case AV_PICTURE_TYPE_BI:
        props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_B_SAMPLE;
        break;
      }

      pMS2->SetProperties(sizeof(props), (BYTE*)&props);
    }
  }
  SafeRelease(&pMS2);
  return hr;
}

// H264 Random Access Helpers

// ILAVVideoSettings
STDMETHODIMP CLAVVideo::SetRuntimeConfig(BOOL bRuntimeConfig)
{
  m_bRuntimeConfig = bRuntimeConfig;
  LoadSettings();

  return S_OK;
}

STDMETHODIMP CLAVVideo::SetFormatConfiguration(LAVVideoCodec vCodec, BOOL bEnabled)
{
  if (vCodec < 0 || vCodec >= Codec_NB)
    return E_FAIL;

  m_settings.bFormats[vCodec] = bEnabled;

  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetFormatConfiguration(LAVVideoCodec vCodec)
{
  if (vCodec < 0 || vCodec >= Codec_NB)
    return FALSE;

  return m_settings.bFormats[vCodec];
}

STDMETHODIMP CLAVVideo::SetNumThreads(DWORD dwNum)
{
  m_settings.NumThreads = dwNum;
  return SaveSettings();
}

STDMETHODIMP_(DWORD) CLAVVideo::GetNumThreads()
{
  return m_settings.NumThreads;
}

STDMETHODIMP CLAVVideo::SetStreamAR(BOOL bStreamAR)
{
  m_settings.StreamAR = bStreamAR;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetStreamAR()
{
  return m_settings.StreamAR;
}

STDMETHODIMP CLAVVideo::SetReportInterlacedFlags(BOOL bEnabled)
{
  m_settings.InterlacedFlags = bEnabled;
  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetReportInterlacedFlags()
{
  return m_settings.InterlacedFlags;
}

STDMETHODIMP CLAVVideo::SetPixelFormat(LAVVideoPixFmts pixFmt, BOOL bEnabled)
{
  if (pixFmt < 0 || pixFmt >= LAVPixFmt_NB)
    return E_FAIL;

  m_settings.bPixFmts[pixFmt] = bEnabled;

  return SaveSettings();
}

STDMETHODIMP_(BOOL) CLAVVideo::GetPixelFormat(LAVVideoPixFmts pixFmt)
{
  if (pixFmt < 0 || pixFmt >= LAVPixFmt_NB)
    return FALSE;

  return m_settings.bPixFmts[pixFmt];
}
