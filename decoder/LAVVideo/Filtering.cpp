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
 */

#include "stdafx.h"
#include "LAVVideo.h"

static void lav_free_lavframe(void *opaque, uint8_t *data)
{
  LAVFrame *frame = (LAVFrame *)opaque;
  FreeLAVFrameBuffers(frame);
  SAFE_CO_FREE(opaque);
}

static void lav_unref_frame(void *opaque, uint8_t *data)
{
  AVBufferRef *buf = (AVBufferRef *)opaque;
  av_buffer_unref(&buf);
}

static void avfilter_free_lav_buffer(LAVFrame *pFrame)
{
  av_frame_free((AVFrame **)&pFrame->priv_data);
}

HRESULT CLAVVideo::Filter(LAVFrame *pFrame)
{
  int ret = 0;
  BOOL bFlush = pFrame->flags & LAV_FRAME_FLAG_FLUSH;
  if (m_Decoder.IsInterlaced(FALSE) && m_settings.DeintMode != DeintMode_Disable
    && (m_settings.SWDeintMode == SWDeintMode_YADIF || m_settings.SWDeintMode == SWDeintMode_W3FDIF_Simple || m_settings.SWDeintMode == SWDeintMode_W3FDIF_Complex)
    && ((bFlush && m_pFilterGraph) || pFrame->format == LAVPixFmt_YUV420 || pFrame->format == LAVPixFmt_YUV422 || pFrame->format == LAVPixFmt_NV12)) {
    AVPixelFormat ff_pixfmt = (pFrame->format == LAVPixFmt_YUV420) ? AV_PIX_FMT_YUV420P : (pFrame->format == LAVPixFmt_YUV422) ? AV_PIX_FMT_YUV422P : AV_PIX_FMT_NV12;

    if (!bFlush && (!m_pFilterGraph || pFrame->format != m_filterPixFmt || pFrame->width != m_filterWidth || pFrame->height != m_filterHeight)) {
      DbgLog((LOG_TRACE, 10, L":Filter()(init) Initializing YADIF deinterlacing filter..."));
      if (m_pFilterGraph) {
        avfilter_graph_free(&m_pFilterGraph);
        m_pFilterBufferSrc = nullptr;
        m_pFilterBufferSink = nullptr;
      }

      m_filterPixFmt = pFrame->format;
      m_filterWidth  = pFrame->width;
      m_filterHeight = pFrame->height;

      char args[512];
      enum AVPixelFormat pix_fmts[3];

      if (ff_pixfmt == AV_PIX_FMT_NV12) {
        pix_fmts[0] = AV_PIX_FMT_NV12;
        pix_fmts[1] = AV_PIX_FMT_YUV420P;
      } else {
        pix_fmts[0] = ff_pixfmt;
        pix_fmts[1] = AV_PIX_FMT_NONE;
      }
      pix_fmts[2] = AV_PIX_FMT_NONE;

      const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
      const AVFilter *buffersink = avfilter_get_by_name("buffersink");
      AVFilterInOut *outputs = avfilter_inout_alloc();
      AVFilterInOut *inputs  = avfilter_inout_alloc();

      m_pFilterGraph = avfilter_graph_alloc();

      av_opt_set(m_pFilterGraph, "thread_type", "slice", AV_OPT_SEARCH_CHILDREN);
      av_opt_set_int(m_pFilterGraph, "threads", FFMAX(1, av_cpu_count() / 2), AV_OPT_SEARCH_CHILDREN);

      // 0/0 is not a valid value for avfilter, make sure it doesn't happen
      AVRational aspect_ratio = pFrame->aspect_ratio;
      if (aspect_ratio.num == 0 || aspect_ratio.den == 0)
        aspect_ratio = { 0, 1 };

      _snprintf_s(args, sizeof(args), "video_size=%dx%d:pix_fmt=%s:time_base=1/10000000:pixel_aspect=%d/%d", pFrame->width, pFrame->height, av_get_pix_fmt_name(ff_pixfmt), pFrame->aspect_ratio.num, pFrame->aspect_ratio.den);
      ret = avfilter_graph_create_filter(&m_pFilterBufferSrc, buffersrc, "in", args, nullptr, m_pFilterGraph);
      if (ret < 0) {
        DbgLog((LOG_TRACE, 10, L"::Filter()(init) Creating the input buffer filter failed with code %d", ret));
        avfilter_graph_free(&m_pFilterGraph);
        goto deliver;
      }

      ret = avfilter_graph_create_filter(&m_pFilterBufferSink, buffersink, "out", nullptr, nullptr, m_pFilterGraph);
      if (ret < 0) {
        DbgLog((LOG_TRACE, 10, L"::Filter()(init) Creating the buffer sink filter failed with code %d", ret));
        avfilter_free(m_pFilterBufferSrc);
        m_pFilterBufferSrc = nullptr;
        avfilter_graph_free(&m_pFilterGraph);
        goto deliver;
      }

      /* set allowed pixfmts on the output */
      av_opt_set_int_list(m_pFilterBufferSink->priv, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, 0);

      /* Endpoints for the filter graph. */
      outputs->name       = av_strdup("in");
      outputs->filter_ctx = m_pFilterBufferSrc;
      outputs->pad_idx    = 0;
      outputs->next       = nullptr;

      inputs->name       = av_strdup("out");
      inputs->filter_ctx = m_pFilterBufferSink;
      inputs->pad_idx    = 0;
      inputs->next       = nullptr;

      if (m_settings.SWDeintMode == SWDeintMode_YADIF)
        _snprintf_s(args, sizeof(args), "yadif=mode=%s:parity=auto:deint=interlaced", (m_settings.SWDeintOutput == DeintOutput_FramePerField) ? "send_field" : "send_frame");
      else if (m_settings.SWDeintMode == SWDeintMode_W3FDIF_Simple)
        _snprintf_s(args, sizeof(args), "w3fdif=filter=simple:deint=interlaced");
      else if (m_settings.SWDeintMode == SWDeintMode_W3FDIF_Complex)
        _snprintf_s(args, sizeof(args), "w3fdif=filter=complex:deint=interlaced");
      else
        ASSERT(0);

      if ((ret = avfilter_graph_parse_ptr(m_pFilterGraph, args, &inputs, &outputs, nullptr)) < 0) {
        DbgLog((LOG_TRACE, 10, L"::Filter()(init) Parsing the graph failed with code %d", ret));
        avfilter_graph_free(&m_pFilterGraph);
        goto deliver;
      }

      if ((ret = avfilter_graph_config(m_pFilterGraph, nullptr)) < 0) {
        DbgLog((LOG_TRACE, 10, L"::Filter()(init) Configuring the graph failed with code %d", ret));
        avfilter_graph_free(&m_pFilterGraph);
        goto deliver;
      }

      DbgLog((LOG_TRACE, 10, L":Filter()(init) avfilter Initialization complete"));
    }

    if (!m_pFilterGraph)
      goto deliver;

    if (pFrame->direct) {
      HRESULT hr = DeDirectFrame(pFrame, true);
      if (FAILED(hr)) {
        ReleaseFrame(&pFrame);
        return hr;
      }
    }

    AVFrame *in_frame = nullptr;
    BOOL refcountedFrame = (m_Decoder.HasThreadSafeBuffers() == S_OK);
    // When flushing, we feed a NULL frame
    if (!bFlush) {
      in_frame = av_frame_alloc();

      for (int i = 0; i < 4; i++) {
        in_frame->data[i] = pFrame->data[i];
        in_frame->linesize[i] = (int)pFrame->stride[i];
      }

      in_frame->width               = pFrame->width;
      in_frame->height              = pFrame->height;
      in_frame->format              = ff_pixfmt;
      in_frame->pts                 = pFrame->rtStart;
      in_frame->interlaced_frame    = pFrame->interlaced;
      in_frame->top_field_first     = pFrame->tff;
      in_frame->sample_aspect_ratio = pFrame->aspect_ratio;

      if (refcountedFrame) {
        AVBufferRef *pFrameBuf = av_buffer_create(nullptr, 0, lav_free_lavframe, pFrame, 0);
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get((AVPixelFormat)in_frame->format);
        int planes = (in_frame->format == AV_PIX_FMT_NV12) ? 2 : desc->nb_components;

        for (int i = 0; i < planes; i++) {
          int h_shift    = (i == 1 || i == 2) ? desc->log2_chroma_h : 0;
          int plane_size = (in_frame->height >> h_shift) * in_frame->linesize[i];

          AVBufferRef *planeRef = av_buffer_ref(pFrameBuf);
          in_frame->buf[i] = av_buffer_create(in_frame->data[i], plane_size, lav_unref_frame, planeRef, AV_BUFFER_FLAG_READONLY);
        }
        av_buffer_unref(&pFrameBuf);
      }

      m_FilterPrevFrame = *pFrame;
      memset(m_FilterPrevFrame.data, 0, sizeof(m_FilterPrevFrame.data));
      m_FilterPrevFrame.destruct = nullptr;
    } else {
      if (!m_FilterPrevFrame.height) // if height is not set, the frame is most likely not valid
        return S_OK;
      *pFrame = m_FilterPrevFrame;
    }

    if ((ret = av_buffersrc_write_frame(m_pFilterBufferSrc, in_frame)) < 0) {
      av_frame_free(&in_frame);
      goto deliver;
    }

    BOOL bFramePerField = (m_settings.SWDeintMode == SWDeintMode_YADIF && m_settings.SWDeintOutput == DeintOutput_FramePerField)
                        || m_settings.SWDeintMode == SWDeintMode_W3FDIF_Simple || m_settings.SWDeintMode == SWDeintMode_W3FDIF_Complex;

    AVFrame *out_frame = av_frame_alloc();
    HRESULT hrDeliver = S_OK;
    while (SUCCEEDED(hrDeliver) && (av_buffersink_get_frame(m_pFilterBufferSink, out_frame) >= 0)) {
      LAVFrame *outFrame = nullptr;
      AllocateFrame(&outFrame);

      REFERENCE_TIME rtDuration = pFrame->rtStop - pFrame->rtStart;
      if (bFramePerField)
        rtDuration >>= 1;

      // Copy most settings over
      outFrame->format       = (out_frame->format == AV_PIX_FMT_YUV420P) ? LAVPixFmt_YUV420 : (out_frame->format == AV_PIX_FMT_YUV422P) ? LAVPixFmt_YUV422 : LAVPixFmt_NV12;
      outFrame->bpp          = pFrame->bpp;
      outFrame->ext_format   = pFrame->ext_format;
      outFrame->avgFrameDuration = pFrame->avgFrameDuration;
      outFrame->flags        = pFrame->flags;

      outFrame->width        = out_frame->width;
      outFrame->height       = out_frame->height;
      outFrame->aspect_ratio = out_frame->sample_aspect_ratio;
      outFrame->tff          = out_frame->top_field_first;
        
      REFERENCE_TIME pts     = av_rescale(out_frame->pts, m_pFilterBufferSink->inputs[0]->time_base.num * 10000000LL, m_pFilterBufferSink->inputs[0]->time_base.den);
      outFrame->rtStart      = pts;
      outFrame->rtStop       = pts + rtDuration;

      if (bFramePerField) {
        if (outFrame->avgFrameDuration != AV_NOPTS_VALUE)
          outFrame->avgFrameDuration /= 2;
      }

      for (int i = 0; i < 4; i++) {
        outFrame->data[i] = out_frame->data[i];
        outFrame->stride[i] = out_frame->linesize[i];
      }

      outFrame->destruct = avfilter_free_lav_buffer;
      outFrame->priv_data = av_frame_alloc();
      av_frame_move_ref((AVFrame *)outFrame->priv_data, out_frame);

      hrDeliver = DeliverToRenderer(outFrame);
    }
    if (!refcountedFrame)
      ReleaseFrame(&pFrame);
    av_frame_free(&in_frame);
    av_frame_free(&out_frame);

    // We EOF'ed the graph, need to close it
    if (bFlush) {
      if (m_pFilterGraph) {
        avfilter_graph_free(&m_pFilterGraph);
        m_pFilterBufferSrc = nullptr;
        m_pFilterBufferSink = nullptr;
      }
    }

    return S_OK;
  } else {
    m_filterPixFmt = LAVPixFmt_None;
    deliver:
    return DeliverToRenderer(pFrame);
  }
}
