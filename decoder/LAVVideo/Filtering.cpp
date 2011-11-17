/*
 *      Copyright (C) 2011 Hendrik Leppkes
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

void lav_avfilter_default_free_buffer(AVFilterBuffer *ptr)
{
  // Nothing
}

STDMETHODIMP CLAVVideo::Filter(LAVFrame *pFrame)
{
  if ((m_bInterlaced || m_settings.DeintForce) && m_settings.HWDeintMode == SWDeintMode_YADIF && (pFrame->format == LAVPixFmt_YUV420 || pFrame->format == LAVPixFmt_YUV422)) {
    PixelFormat ff_pixfmt = (pFrame->format == LAVPixFmt_YUV420) ? PIX_FMT_YUV420P : PIX_FMT_YUV422P;

    if (!m_pFilterGraph || pFrame->format != m_filterPixFmt || pFrame->width != m_filterWidth || pFrame->height != m_filterHeight) {
      DbgLog((LOG_TRACE, 10, L":Filter()(init) Initializing YADIF deinterlacing filter..."));
      if (m_pFilterGraph) {
        avfilter_graph_free(&m_pFilterGraph);
        m_pFilterBufferSrc = NULL;
        m_pFilterBufferSink = NULL;
      }

      m_filterPixFmt = pFrame->format;
      m_filterWidth  = pFrame->width;
      m_filterHeight = pFrame->height;

      char args[512];
      int ret = 0;
      enum PixelFormat pix_fmts[2];
      pix_fmts[0] = ff_pixfmt;
      pix_fmts[1] = PIX_FMT_NONE;

      AVFilter *buffersrc  = avfilter_get_by_name("buffer");
      AVFilter *buffersink = avfilter_get_by_name("buffersink");
      AVFilterInOut *outputs = avfilter_inout_alloc();
      AVFilterInOut *inputs  = avfilter_inout_alloc();

      m_pFilterGraph = avfilter_graph_alloc();

      _snprintf_s(args, sizeof(args), "%d:%d:%d:%d:%d:%d:%d", pFrame->width, pFrame->height, ff_pixfmt, 1, 10000000, 1, 1);
      ret = avfilter_graph_create_filter(&m_pFilterBufferSrc, buffersrc, "in", args, NULL, m_pFilterGraph);
      if (ret < 0) {
        DbgLog((LOG_TRACE, 10, L"::Filter()(init) Creating the input buffer filter failed with code %d", ret));
        goto deliver;
      }

      ret = avfilter_graph_create_filter(&m_pFilterBufferSink, buffersink, "out", NULL, pix_fmts, m_pFilterGraph);
      if (ret < 0) {
        DbgLog((LOG_TRACE, 10, L"::Filter()(init) Creating the buffer sink filter failed with code %d", ret));
        avfilter_free(m_pFilterBufferSrc);
        m_pFilterBufferSrc = NULL;
        goto deliver;
      }

      /* Endpoints for the filter graph. */
      outputs->name       = av_strdup("in");
      outputs->filter_ctx = m_pFilterBufferSrc;
      outputs->pad_idx    = 0;
      outputs->next       = NULL;

      inputs->name       = av_strdup("out");
      inputs->filter_ctx = m_pFilterBufferSink;
      inputs->pad_idx    = 0;
      inputs->next       = NULL;

      BOOL bForced = (m_settings.DeintAggressive && m_bInterlaced) || m_settings.DeintForce;
      _snprintf_s(args, sizeof(args), "yadif=%d:%d:%d", (m_settings.SWDeintOutput == DeintOutput_FramePerField), -1, !bForced);
      if ((ret = avfilter_graph_parse(m_pFilterGraph, args, &inputs, &outputs, NULL)) < 0) {
        DbgLog((LOG_TRACE, 10, L"::Filter()(init) Parsing the graph failed with code %d", ret));
        goto deliver;
      }

      if ((ret = avfilter_graph_config(m_pFilterGraph, NULL)) < 0) {
        DbgLog((LOG_TRACE, 10, L"::Filter()(init) Configuring the graph failed with code %d", ret));
        goto deliver;
      }

      DbgLog((LOG_TRACE, 10, L":Filter()(init) YADIF Initialization complete"));
    }

    AVFilterBufferRef *out_picref = NULL;
    AVFilterBufferRef *in_picref = NULL;

    in_picref = avfilter_get_video_buffer_ref_from_arrays(pFrame->data, pFrame->stride, AV_PERM_READ, pFrame->width, pFrame->height, ff_pixfmt);

    in_picref->pts                    = pFrame->rtStart;
    in_picref->video->interlaced      = pFrame->interlaced;
    in_picref->video->top_field_first = pFrame->tff;
    in_picref->buf->free              = lav_avfilter_default_free_buffer;

    av_vsrc_buffer_add_video_buffer_ref(m_pFilterBufferSrc, in_picref, 0);
    while (avfilter_poll_frame(m_pFilterBufferSink->inputs[0])) {
      av_vsink_buffer_get_video_buffer_ref(m_pFilterBufferSink, &out_picref, 0);
      if (out_picref) {
        LAVFrame *outFrame = NULL;
        AllocateFrame(&outFrame);

        // Copy most settings over
        *outFrame = *pFrame;
        outFrame->priv_data = NULL;
        outFrame->destruct = NULL;
        outFrame->interlaced = 0;
        outFrame->rtStart = out_picref->pts;
        outFrame->rtStop  = outFrame->rtStart + ((pFrame->rtStop - pFrame->rtStart) >> 1);
        memcpy(outFrame->data, out_picref->data, 4 * sizeof(uint8_t*));
        memcpy(outFrame->stride, out_picref->linesize, 4 * sizeof(int));

        DeliverToRenderer(outFrame);

        avfilter_unref_buffer(out_picref);
      }
    }

    avfilter_unref_buffer(in_picref);
    ReleaseFrame(&pFrame);

    return S_OK;
  } else {
    deliver:
    return DeliverToRenderer(pFrame);
  }
}
