/*
*      Copyright (C) 2017 Hendrik Leppkes
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

#include "stdafx.h"
#include "d3d11va.h"
#include "d3d11/ID3DVideoMemoryConfiguration.h"
#include "dxva2/dxva_common.h"

ILAVDecoder *CreateDecoderD3D11()
{
  return new CDecD3D11();
}

////////////////////////////////////////////////////////////////////////////////
// D3D11 decoder implementation
////////////////////////////////////////////////////////////////////////////////

CDecD3D11::CDecD3D11(void)
  : CDecAvcodec()
{
}


CDecD3D11::~CDecD3D11(void)
{
  DestroyDecoder(true);

  if (m_pAllocator)
    m_pAllocator->DecoderDestruct();
  SafeRelease(&m_pAllocator);
}

STDMETHODIMP CDecD3D11::DestroyDecoder(bool bFull, bool bNoAVCodec)
{
  if (m_pOutputViews)
  {
    for (int i = 0; i < m_nOutputViews; i++)
    {
      SafeRelease(&m_pOutputViews[i]);
    }
    av_freep(&m_pOutputViews);
    m_nOutputViews = 0;
  }

  SafeRelease(&m_pDecoder);
  av_buffer_unref(&m_pFramesCtx);

  if (!bNoAVCodec) {
    CDecAvcodec::DestroyDecoder();
  }

  if (bFull) {
    av_buffer_unref(&m_pDevCtx);
  }

  return S_OK;
}

// ILAVDecoder
STDMETHODIMP CDecD3D11::InitAllocator(IMemAllocator **ppAlloc)
{
  HRESULT hr = S_OK;
  if (m_bReadBackFallback)
    return E_NOTIMPL;

  if (m_pAllocator == nullptr)
  {
    m_pAllocator = new CD3D11SurfaceAllocator(this, &hr);
    if (!m_pAllocator) {
      return E_OUTOFMEMORY;
    }
    if (FAILED(hr)) {
      SAFE_DELETE(m_pAllocator);
      return hr;
    }

    // Hold a reference on the allocator
    m_pAllocator->AddRef();
  }

  // return the proper interface
  return m_pAllocator->QueryInterface(__uuidof(IMemAllocator), (void **)ppAlloc);
}

STDMETHODIMP CDecD3D11::PostConnect(IPin *pPin)
{
  DbgLog((LOG_TRACE, 10, L"CDecD3D11::PostConnect()"));
  HRESULT hr = S_OK;

  ID3D11DecoderConfiguration *pD3D11DecoderConfiguration = nullptr;
  hr = pPin->QueryInterface(&pD3D11DecoderConfiguration);
  if (FAILED(hr)) {
    DbgLog((LOG_ERROR, 10, L"-> ID3D11DecoderConfiguration not available, using fallback mode"));
  }

  // Release old D3D resources, we're about to re-init
  m_pCallback->ReleaseAllDXVAResources();

  // free the decoder to force a re-init down the line
  SafeRelease(&m_pDecoder);

  // and the old device
  av_buffer_unref(&m_pDevCtx);

  // device id (hwcontext API wants a string)
  UINT nDevice = pD3D11DecoderConfiguration ? pD3D11DecoderConfiguration->GetD3D11AdapterIndex() : 0;

  for (;;)
  {
    char deviceId[34] = { 0 };
    _itoa_s(nDevice, deviceId, 10);

    // allocate device context
    int ret = av_hwdevice_ctx_create(&m_pDevCtx, AV_HWDEVICE_TYPE_D3D11VA, deviceId, nullptr, 0);
    if (ret < 0) {
      // if the device failed, try with the default device
      if (nDevice != 0)
      {
        nDevice = 0;
        continue;
      }

      DbgLog((LOG_ERROR, 10, L"-> Failed to create D3D11 hardware context"));
      goto fail;
    }

    break;
  }

  // check if the connection supports native mode
  {
    CMediaType mt = m_pCallback->GetOutputMediaType();
    if ((m_SurfaceFormat == DXGI_FORMAT_NV12 && mt.subtype != MEDIASUBTYPE_NV12)
      || (m_SurfaceFormat == DXGI_FORMAT_P010 && mt.subtype != MEDIASUBTYPE_P010)
      || (m_SurfaceFormat == DXGI_FORMAT_P016 && mt.subtype != MEDIASUBTYPE_P016)) {
      DbgLog((LOG_ERROR, 10, L"-> Connection is not the appropriate pixel format for D3D11 Native"));

      m_bReadBackFallback = false;
      SafeRelease(&pD3D11DecoderConfiguration);
    }
  }

  // verify hardware support
  {
    GUID guidConversion = GUID_NULL;
    hr = FindVideoServiceConversion(m_pAVCtx->codec_id, m_pAVCtx->profile, m_SurfaceFormat, &guidConversion);
    if (FAILED(hr))
    {
      goto fail;
    }

    // get decoder configuration
    D3D11_VIDEO_DECODER_DESC desc = { 0 };
    desc.Guid = guidConversion;
    desc.OutputFormat = m_SurfaceFormat;
    desc.SampleWidth = m_pAVCtx->coded_width;
    desc.SampleHeight = m_pAVCtx->coded_height;

    D3D11_VIDEO_DECODER_CONFIG decoder_config = { 0 };
    hr = FindDecoderConfiguration(&desc, &decoder_config);
    if (FAILED(hr))
    {
      goto fail;
    }
  }

  // Notice the connected pin that we're sending D3D11 textures
  if (pD3D11DecoderConfiguration)
  {
    AVD3D11VADeviceContext *pDeviceContext = (AVD3D11VADeviceContext *)((AVHWDeviceContext *)m_pDevCtx->data)->hwctx;

    hr = pD3D11DecoderConfiguration->ActivateD3D11Decoding(pDeviceContext->device, pDeviceContext->device_context, pDeviceContext->lock_ctx, 0);
    SafeRelease(&pD3D11DecoderConfiguration);

    m_bReadBackFallback = FAILED(hr);
  }
  else
  {
    m_bReadBackFallback = true;
  }

  return S_OK;

fail:
  SafeRelease(&pD3D11DecoderConfiguration);
  return E_FAIL;
}

STDMETHODIMP CDecD3D11::InitDecoder(AVCodecID codec, const CMediaType *pmt)
{
  HRESULT hr = S_OK;
  DbgLog((LOG_TRACE, 10, L"CDecD3D11::InitDecoder(): Initializing D3D11 decoder"));

  // Destroy old decoder
  DestroyDecoder(false);

  // reset stream compatibility
  m_bFailHWDecode = false;

  // Initialize ffmpeg
  hr = CDecAvcodec::InitDecoder(codec, pmt);
  if (FAILED(hr))
    return hr;

  if (check_dxva_codec_profile(m_pAVCtx->codec_id, m_pAVCtx->pix_fmt, m_pAVCtx->profile, AV_PIX_FMT_D3D11))
  {
    DbgLog((LOG_TRACE, 10, L"-> Incompatible profile detected, falling back to software decoding"));
    return E_FAIL;
  }

  // initialize surface format to ensure the default media type is set properly
  bool bHighBitdepth = (m_pAVCtx->codec_id == AV_CODEC_ID_HEVC && (m_pAVCtx->sw_pix_fmt == AV_PIX_FMT_YUV420P10 || m_pAVCtx->profile == FF_PROFILE_HEVC_MAIN_10))
                    || (m_pAVCtx->codec_id == AV_CODEC_ID_VP9  && (m_pAVCtx->sw_pix_fmt == AV_PIX_FMT_YUV420P10 || m_pAVCtx->profile == FF_PROFILE_VP9_2));
  if (bHighBitdepth)
    m_SurfaceFormat = DXGI_FORMAT_P010;
  else
    m_SurfaceFormat = DXGI_FORMAT_NV12;

  m_dwSurfaceWidth = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_width);
  m_dwSurfaceHeight = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_height);

  return S_OK;
}

HRESULT CDecD3D11::AdditionaDecoderInit()
{
  AVD3D11VAContext *ctx = av_d3d11va_alloc_context();

  if (m_pDecoder) {
    FillHWContext(ctx);
  }

  m_pAVCtx->thread_count = 1;
  m_pAVCtx->hwaccel_context = ctx;
  m_pAVCtx->get_format = get_d3d11_format;
  m_pAVCtx->get_buffer2 = get_d3d11_buffer;
  m_pAVCtx->opaque = this;

  m_pAVCtx->slice_flags |= SLICE_FLAG_ALLOW_FIELD;

  // disable error concealment in hwaccel mode, it doesn't work either way
  m_pAVCtx->error_concealment = 0;
  av_opt_set_int(m_pAVCtx, "enable_er", 0, AV_OPT_SEARCH_CHILDREN);

  return S_OK;
}

STDMETHODIMP CDecD3D11::FillHWContext(AVD3D11VAContext *ctx)
{
  AVD3D11VADeviceContext *pDeviceContext = (AVD3D11VADeviceContext *)((AVHWDeviceContext *)m_pDevCtx->data)->hwctx;

  ctx->decoder       = m_pDecoder;
  ctx->video_context = pDeviceContext->video_context;
  ctx->cfg           = &m_DecoderConfig;
  ctx->surface_count = m_nOutputViews;
  ctx->surface       = m_pOutputViews;

  ctx->context_mutex = pDeviceContext->lock_ctx;

  ctx->workaround    = 0;

  return S_OK;
}

STDMETHODIMP_(long) CDecD3D11::GetBufferCount()
{
  long buffers = 0;

  // Native decoding should use 16 buffers to enable seamless codec changes
  if (!m_bReadBackFallback)
    buffers = 16;
  else {
    // Buffers based on max ref frames
    if (m_nCodecId == AV_CODEC_ID_H264 || m_nCodecId == AV_CODEC_ID_HEVC)
      buffers = 16;
    else
      buffers = 2;
  }

  // 4 extra buffers for handling and safety
  buffers += 4;

  /*if (m_bReadBackFallback) {
    buffers += m_DisplayDelay;
  }*/
  if (m_pCallback->GetDecodeFlags() & LAV_VIDEO_DEC_FLAG_DVD) {
    buffers += 4;
  }
  return buffers;
}

HRESULT CDecD3D11::PostDecode()
{
  if (m_bFailHWDecode) {
    DbgLog((LOG_TRACE, 10, L"::PostDecode(): HW Decoder failed, falling back to software decoding"));
    return E_FAIL;
  }
  return S_OK;
}

enum AVPixelFormat CDecD3D11::get_d3d11_format(struct AVCodecContext *s, const enum AVPixelFormat * pix_fmts)
{
  CDecD3D11 *pDec = (CDecD3D11 *)s->opaque;
  const enum AVPixelFormat *p;
  for (p = pix_fmts; *p != -1; p++) {
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*p);

    if (!desc || !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
      break;

    if (*p == AV_PIX_FMT_D3D11)
    {
      HRESULT hr = pDec->ReInitD3D11Decoder(s);
      if (FAILED(hr))
      {
        pDec->m_bFailHWDecode = TRUE;
        continue;
      }
      else
      {
        break;
      }
    }
  }
  return *p;
}

int CDecD3D11::get_d3d11_buffer(struct AVCodecContext *c, AVFrame *frame, int flags)
{
  CDecD3D11 *pDec = (CDecD3D11 *)c->opaque;
  HRESULT hr = S_OK;

  if (frame->format != AV_PIX_FMT_D3D11) {
    DbgLog((LOG_ERROR, 10, L"D3D11 buffer request, but not D3D11 pixfmt"));
    pDec->m_bFailHWDecode = TRUE;
    return -1;
  }

  hr = pDec->ReInitD3D11Decoder(c);
  if (FAILED(hr)) {
    pDec->m_bFailHWDecode = TRUE;
    return -1;
  }

  if (pDec->m_bReadBackFallback && pDec->m_pFramesCtx)
  {
    int ret = av_hwframe_get_buffer(pDec->m_pFramesCtx, frame, 0);
    frame->width = c->coded_width;
    frame->height = c->coded_height;
    return ret;
  }
  else if (pDec->m_bReadBackFallback == false && pDec->m_pAllocator)
  {
    IMediaSample *pSample = nullptr;
    hr = pDec->m_pAllocator->GetBuffer(&pSample, nullptr, nullptr, 0);
    if (SUCCEEDED(hr))
    {
      CD3D11MediaSample *pD3D11Sample = dynamic_cast<CD3D11MediaSample *>(pSample);

      // fill the frame from the sample, including a reference to the sample
      pD3D11Sample->GetAVFrameBuffer(frame);

      frame->width = c->coded_width;
      frame->height = c->coded_height;

      // the frame holds the sample now, can release the direct interface
      pD3D11Sample->Release();
      return 0;
    }
  }

  return -1;
}

STDMETHODIMP CDecD3D11::ReInitD3D11Decoder(AVCodecContext *c)
{
  HRESULT hr = S_OK;

  // Don't allow decoder creation during first init
  if (m_bInInit)
    return S_FALSE;

  // sanity check that we have a device
  if (m_pDevCtx == nullptr)
    return E_FAIL;

  // we need an allocator at this point
  if (m_bReadBackFallback == false && m_pAllocator == nullptr)
    return E_FAIL;

  if (m_pDecoder == nullptr || m_dwSurfaceWidth != dxva_align_dimensions(c->codec_id, c->coded_width) || m_dwSurfaceHeight != dxva_align_dimensions(c->codec_id, c->coded_height) || m_DecodePixelFormat != c->sw_pix_fmt)
  {
    AVD3D11VADeviceContext *pDeviceContext = (AVD3D11VADeviceContext *)((AVHWDeviceContext *)m_pDevCtx->data)->hwctx;
    DbgLog((LOG_TRACE, 10, L"No D3D11 Decoder or image dimensions changed -> Re-Allocating resources"));

    // if we're not in readback mode, we need to flush all the frames
    if (m_bReadBackFallback == false)
      avcodec_flush_buffers(c);

    pDeviceContext->lock(pDeviceContext->lock_ctx);
    hr = CreateD3D11Decoder();
    pDeviceContext->unlock(pDeviceContext->lock_ctx);
    if (FAILED(hr))
      return hr;

    // Update the frames context in the allocator
    if (m_bReadBackFallback == false)
    {
      // decommit the allocator
      m_pAllocator->Decommit();

      // verify we were able to decommit all its resources
      if (m_pAllocator->DecommitInProgress()) {
        DbgLog((LOG_TRACE, 10, L"WARNING! D3D11 Allocator is still busy, trying to flush downstream"));
        m_pCallback->ReleaseAllDXVAResources();
        m_pCallback->GetOutputPin()->GetConnected()->BeginFlush();
        m_pCallback->GetOutputPin()->GetConnected()->EndFlush();
        if (m_pAllocator->DecommitInProgress()) {
          DbgLog((LOG_TRACE, 10, L"WARNING! Flush had no effect, decommit of the allocator still not complete"));
        }
        else {
          DbgLog((LOG_TRACE, 10, L"Flush was successfull, decommit completed!"));
        }
      }

      // re-commit it to update its frame reference
      m_pAllocator->Commit();
    }
  }

  return S_OK;
}

STDMETHODIMP CDecD3D11::FindVideoServiceConversion(AVCodecID codec, int profile, DXGI_FORMAT surface_format, GUID *input )
{
  AVD3D11VADeviceContext *pDeviceContext = (AVD3D11VADeviceContext *)((AVHWDeviceContext *)m_pDevCtx->data)->hwctx;
  HRESULT hr = S_OK;

  UINT nProfiles = pDeviceContext->video_device->GetVideoDecoderProfileCount();
  GUID *guid_list = (GUID *)av_malloc_array(nProfiles, sizeof(*guid_list));

  DbgLog((LOG_TRACE, 10, L"-> Enumerating supported D3D11 modes (count: %d)", nProfiles));
  for (UINT i = 0; i < nProfiles; i++)
  {
    hr = pDeviceContext->video_device->GetVideoDecoderProfile(i, &guid_list[i]);
    if (FAILED(hr)) {
      DbgLog((LOG_ERROR, 10, L"Error retrieving decoder profile"));
      av_free(guid_list);
      return hr;
    }

#ifdef DEBUG
    const dxva_mode_t *mode = get_dxva_mode_from_guid(&guid_list[i]);
    if (mode) {
      DbgLog((LOG_TRACE, 10, L"  -> %S", mode->name));
    }
    else {
      DbgLog((LOG_TRACE, 10, L"  -> Unknown GUID (%s)", WStringFromGUID(guid_list[i]).c_str()));
    }
#endif
  }

  /* Iterate over our priority list */
  for (unsigned i = 0; dxva_modes[i].name; i++) {
    const dxva_mode_t *mode = &dxva_modes[i];
    if (!check_dxva_mode_compatibility(mode, codec, profile))
      continue;

    BOOL supported = FALSE;
    for (UINT g = 0; !supported && g < nProfiles; g++) {
      supported = IsEqualGUID(*mode->guid, guid_list[g]);
    }
    if (!supported)
      continue;

    DbgLog((LOG_TRACE, 10, L"-> Trying to use '%S'", mode->name));
    hr = pDeviceContext->video_device->CheckVideoDecoderFormat(mode->guid, surface_format, &supported);
    if (SUCCEEDED(hr) && supported) {
      *input = *mode->guid;

      av_free(guid_list);
      return S_OK;
    }
  }

  av_free(guid_list);
  return E_FAIL;
}

STDMETHODIMP CDecD3D11::FindDecoderConfiguration(const D3D11_VIDEO_DECODER_DESC *desc, D3D11_VIDEO_DECODER_CONFIG *pConfig)
{
  AVD3D11VADeviceContext *pDeviceContext = (AVD3D11VADeviceContext *)((AVHWDeviceContext *)m_pDevCtx->data)->hwctx;
  HRESULT hr = S_OK;

  UINT nConfig = 0;
  hr = pDeviceContext->video_device->GetVideoDecoderConfigCount(desc, &nConfig);
  if (FAILED(hr)) {
    DbgLog((LOG_ERROR, 10, L"Unable to retreive decoder configuration count"));
    return E_FAIL;
  }

  int best_score = 0;
  D3D11_VIDEO_DECODER_CONFIG best_config;
  for (UINT i = 0; i < nConfig; i++)
  {
    D3D11_VIDEO_DECODER_CONFIG config = { 0 };
    hr = pDeviceContext->video_device->GetVideoDecoderConfig(desc, i, &config);
    if (FAILED(hr))
      continue;

    int score;
    if (config.ConfigBitstreamRaw == 1)
      score = 1;
    else if (m_pAVCtx->codec_id == AV_CODEC_ID_H264 && config.ConfigBitstreamRaw == 2)
      score = 2;
    else
      continue;
    if (IsEqualGUID(config.guidConfigBitstreamEncryption, DXVA2_NoEncrypt))
      score += 16;
    if (score > best_score) {
      best_score = score;
      best_config = config;
    }
  }

  if (best_score <= 0) {
    DbgLog((LOG_TRACE, 10, L"-> No matching configuration available"));
    return E_FAIL;
  }

  *pConfig = best_config;
  return S_OK;
}

static DXGI_FORMAT d3d11va_map_sw_to_hw_format(enum AVPixelFormat pix_fmt)
{
  switch (pix_fmt) {
  case AV_PIX_FMT_YUV420P10:
  case AV_PIX_FMT_P010:       return DXGI_FORMAT_P010;
  case AV_PIX_FMT_NV12:
  default:                    return DXGI_FORMAT_NV12;
  }
}

STDMETHODIMP CDecD3D11::CreateD3D11Decoder()
{
  HRESULT hr = S_OK;
  AVD3D11VADeviceContext *pDeviceContext = (AVD3D11VADeviceContext *)((AVHWDeviceContext *)m_pDevCtx->data)->hwctx;

  // release the old decoder, it needs to be re-created
  SafeRelease(&m_pDecoder);

  // find a decoder configuration
  GUID profileGUID = GUID_NULL;
  DXGI_FORMAT surface_format = d3d11va_map_sw_to_hw_format(m_pAVCtx->sw_pix_fmt);
  hr = FindVideoServiceConversion(m_pAVCtx->codec_id, m_pAVCtx->profile, surface_format, &profileGUID);
  if (FAILED(hr))
  {
    DbgLog((LOG_ERROR, 10, L"-> No video service profile found"));
    return hr;
  }

  // get decoder configuration
  D3D11_VIDEO_DECODER_DESC desc = { 0 };
  desc.Guid = profileGUID;
  desc.OutputFormat = surface_format;
  desc.SampleWidth = m_pAVCtx->coded_width;
  desc.SampleHeight = m_pAVCtx->coded_height;

  D3D11_VIDEO_DECODER_CONFIG decoder_config = { 0 };
  hr = FindDecoderConfiguration(&desc, &decoder_config);
  if (FAILED(hr))
  {
    DbgLog((LOG_ERROR, 10, L"-> No valid video decoder configuration found"));
    return hr;
  }

  m_DecoderConfig = decoder_config;

  // update surface properties
  m_dwSurfaceWidth = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_width);
  m_dwSurfaceHeight = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_height);
  m_DecodePixelFormat = m_pAVCtx->sw_pix_fmt;
  m_SurfaceFormat = surface_format;

  if (m_bReadBackFallback == false && m_pAllocator)
  {
    ALLOCATOR_PROPERTIES properties;
    hr = m_pAllocator->GetProperties(&properties);
    if (FAILED(hr))
      return hr;

    m_dwSurfaceCount = properties.cBuffers;
  }
  else
  {
    m_dwSurfaceCount = GetBufferCount();
  }

  // allocate a new frames context for the dimensions and format
  hr = AllocateFramesContext(m_dwSurfaceWidth, m_dwSurfaceHeight, m_DecodePixelFormat, m_dwSurfaceCount, &m_pFramesCtx);
  if (FAILED(hr))
  {
    DbgLog((LOG_ERROR, 10, L"-> Error allocating frames context"));
    return hr;
  }

  // release any old output views and allocate memory for the new ones
  if (m_pOutputViews)
  {
    for (int i = 0; i < m_nOutputViews; i++)
    {
      SafeRelease(&m_pOutputViews[i]);
    }
    av_freep(&m_pOutputViews);
  }

  m_pOutputViews = (ID3D11VideoDecoderOutputView **)av_mallocz_array(m_dwSurfaceCount, sizeof(*m_pOutputViews));
  m_nOutputViews = m_dwSurfaceCount;

  // allocate output views for the frames
  AVD3D11VAFramesContext *pFramesContext = (AVD3D11VAFramesContext *)((AVHWFramesContext *)m_pFramesCtx->data)->hwctx;
  for (int i = 0; i < m_nOutputViews; i++)
  {
    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc = { 0 };
    viewDesc.DecodeProfile = profileGUID;
    viewDesc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2D.ArraySlice = i;

    hr = pDeviceContext->video_device->CreateVideoDecoderOutputView(pFramesContext->texture, &viewDesc, &m_pOutputViews[i]);
    if (FAILED(hr))
    {
      DbgLog((LOG_ERROR, 10, L"-> Failed to create video decoder output views"));
      return E_FAIL;
    }
  }

  // create the decoder
  hr = pDeviceContext->video_device->CreateVideoDecoder(&desc, &decoder_config, &m_pDecoder);
  if (FAILED(hr))
  {
    DbgLog((LOG_ERROR, 10, L"-> Failed to create video decoder object"));
    return E_FAIL;
  }

  FillHWContext((AVD3D11VAContext *)m_pAVCtx->hwaccel_context);

  return S_OK;
}

STDMETHODIMP CDecD3D11::AllocateFramesContext(int width, int height, AVPixelFormat format, int nSurfaces, AVBufferRef **ppFramesCtx)
{
  ASSERT(m_pAVCtx);
  ASSERT(m_pDevCtx);
  ASSERT(ppFramesCtx);

  // unref any old buffer
  av_buffer_unref(ppFramesCtx);

  // allocate a new frames context for the device context
  *ppFramesCtx = av_hwframe_ctx_alloc(m_pDevCtx);
  if (*ppFramesCtx == nullptr)
    return E_OUTOFMEMORY;

  AVHWFramesContext *pFrames = (AVHWFramesContext *)(*ppFramesCtx)->data;
  pFrames->format = AV_PIX_FMT_D3D11;
  pFrames->sw_format = (format == AV_PIX_FMT_YUV420P10) ? AV_PIX_FMT_P010 : AV_PIX_FMT_NV12;
  pFrames->width = width;
  pFrames->height = height;
  pFrames->initial_pool_size = nSurfaces;

  AVD3D11VAFramesContext *pFramesHWContext = (AVD3D11VAFramesContext *)pFrames->hwctx;
  pFramesHWContext->BindFlags |= D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;
  pFramesHWContext->MiscFlags |= D3D11_RESOURCE_MISC_SHARED;

  int ret = av_hwframe_ctx_init(*ppFramesCtx);
  if (ret < 0)
  {
    av_buffer_unref(ppFramesCtx);
    return E_FAIL;
  }

  return S_OK;
}

HRESULT CDecD3D11::HandleDXVA2Frame(LAVFrame *pFrame)
{
  ASSERT(pFrame->format == LAVPixFmt_D3D11);

  if (pFrame->flags & LAV_FRAME_FLAG_FLUSH) {
    /*if (m_bReadBackFallback) {
      FlushDisplayQueue(TRUE);
    }*/
    Deliver(pFrame);
    return S_OK;
  }

  if (m_bReadBackFallback)
  {
    AVFrame *src = (AVFrame *)pFrame->priv_data;
    AVFrame *dst = av_frame_alloc();

    int ret = av_hwframe_transfer_data(dst, src, 0);
    if (ret < 0)
    {
      ReleaseFrame(&pFrame);
      av_frame_free(&dst);
      return E_FAIL;
    }

    // free the source frame
    av_frame_free(&src);

    // and store the dst frame in LAVFrame
    pFrame->priv_data = dst;
    GetPixelFormat(&pFrame->format, &pFrame->bpp);

    ASSERT((dst->format == AV_PIX_FMT_NV12 && pFrame->format == LAVPixFmt_NV12) || (dst->format == AV_PIX_FMT_P010 && pFrame->format == LAVPixFmt_P016));

    for (int i = 0; i < 4; i++) {
      pFrame->data[i] = dst->data[i];
      pFrame->stride[i] = dst->linesize[i];
    }

    Deliver(pFrame);
  }
  else
  {
    AVFrame *pAVFrame = (AVFrame *)pFrame->priv_data;
    pFrame->data[0] = pAVFrame->data[3];
    pFrame->data[1] = pFrame->data[2] = pFrame->data[3] = nullptr;

    GetPixelFormat(&pFrame->format, &pFrame->bpp);

    Deliver(pFrame);
  }

  return S_OK;
}

STDMETHODIMP CDecD3D11::GetPixelFormat(LAVPixelFormat *pPix, int *pBpp)
{
  // Output is always NV12 or P010
  if (pPix)
    *pPix = m_bReadBackFallback == false ? LAVPixFmt_D3D11 : ((m_SurfaceFormat == DXGI_FORMAT_P010 || m_SurfaceFormat == DXGI_FORMAT_P016) ? LAVPixFmt_P016 : LAVPixFmt_NV12);

  if (pBpp)
    *pBpp = (m_SurfaceFormat == DXGI_FORMAT_P016) ? 16 : (m_SurfaceFormat == DXGI_FORMAT_P010 ? 10 : 8);

  return S_OK;
}
