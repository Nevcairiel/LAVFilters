/*
 *      Copyright (C) 2010-2012 Hendrik Leppkes
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

static void avfilter_free_lav_buffer(LAVFrame *pFrame)
{
  avfilter_unref_buffer((AVFilterBufferRef *)pFrame->priv_data);
}

HRESULT CLAVVideo::Filter(LAVFrame *pFrame, HRESULT (CLAVVideo::*deliverFunc)(LAVFrame *pFrame))
{
  if (m_Decoder.IsInterlaced() && m_settings.SWDeintMode == SWDeintMode_YADIF && (pFrame->format == LAVPixFmt_YUV420 || pFrame->format == LAVPixFmt_YUV422 || pFrame->format == LAVPixFmt_NV12)) {
    PixelFormat ff_pixfmt = (pFrame->format == LAVPixFmt_YUV420) ? PIX_FMT_YUV420P : (pFrame->format == LAVPixFmt_YUV422) ? PIX_FMT_YUV422P : PIX_FMT_NV12;

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
      enum PixelFormat pix_fmts[3];

      if (ff_pixfmt == PIX_FMT_NV12) {
        pix_fmts[0] = PIX_FMT_NV12;
        pix_fmts[1] = PIX_FMT_YUV420P;
      } else {
        pix_fmts[0] = ff_pixfmt;
        pix_fmts[1] = PIX_FMT_NONE;
      }
      pix_fmts[2] = PIX_FMT_NONE;

      AVFilter *buffersrc  = avfilter_get_by_name("buffer");
      AVFilter *buffersink = avfilter_get_by_name("buffersink");
      AVFilterInOut *outputs = avfilter_inout_alloc();
      AVFilterInOut *inputs  = avfilter_inout_alloc();

      m_pFilterGraph = avfilter_graph_alloc();

      _snprintf_s(args, sizeof(args), "%d:%d:%d:1:10000000:1:1", pFrame->width, pFrame->height, ff_pixfmt);
      ret = avfilter_graph_create_filter(&m_pFilterBufferSrc, buffersrc, "in", args, NULL, m_pFilterGraph);
      if (ret < 0) {
        DbgLog((LOG_TRACE, 10, L"::Filter()(init) Creating the input buffer filter failed with code %d", ret));
        avfilter_graph_free(&m_pFilterGraph);
        goto deliver;
      }

      ret = avfilter_graph_create_filter(&m_pFilterBufferSink, buffersink, "out", NULL, pix_fmts, m_pFilterGraph);
      if (ret < 0) {
        DbgLog((LOG_TRACE, 10, L"::Filter()(init) Creating the buffer sink filter failed with code %d", ret));
        avfilter_free(m_pFilterBufferSrc);
        m_pFilterBufferSrc = NULL;
        avfilter_graph_free(&m_pFilterGraph);
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

      _snprintf_s(args, sizeof(args), "yadif=%d:-1:1", (m_settings.SWDeintOutput == DeintOutput_FramePerField));
      if ((ret = avfilter_graph_parse(m_pFilterGraph, args, &inputs, &outputs, NULL)) < 0) {
        DbgLog((LOG_TRACE, 10, L"::Filter()(init) Parsing the graph failed with code %d", ret));
        avfilter_graph_free(&m_pFilterGraph);
        goto deliver;
      }

      if ((ret = avfilter_graph_config(m_pFilterGraph, NULL)) < 0) {
        DbgLog((LOG_TRACE, 10, L"::Filter()(init) Configuring the graph failed with code %d", ret));
        avfilter_graph_free(&m_pFilterGraph);
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
    in_picref->video->sample_aspect_ratio = pFrame->aspect_ratio;

    av_vsrc_buffer_add_video_buffer_ref(m_pFilterBufferSrc, in_picref, 0);
    HRESULT hrDeliver = S_OK;
    while (SUCCEEDED(hrDeliver) && av_buffersink_poll_frame(m_pFilterBufferSink)) {
      int ret = av_buffersink_get_buffer_ref(m_pFilterBufferSink, &out_picref, 0);
      if (ret >= 0 && out_picref) {
        LAVFrame *outFrame = NULL;
        AllocateFrame(&outFrame);

        REFERENCE_TIME rtDuration = pFrame->rtStop - pFrame->rtStart;
        if (m_settings.SWDeintOutput == DeintOutput_FramePerField)
          rtDuration >>= 1;

        // Copy most settings over
        outFrame->format       = (out_picref->format == PIX_FMT_YUV420P) ? LAVPixFmt_YUV420 : (out_picref->format == PIX_FMT_YUV422P) ? LAVPixFmt_YUV422 : LAVPixFmt_NV12;
        outFrame->bpp          = pFrame->bpp;
        outFrame->ext_format   = pFrame->ext_format;
        outFrame->avgFrameDuration = pFrame->avgFrameDuration;

        outFrame->width        = out_picref->video->w;
        outFrame->height       = out_picref->video->h;
        outFrame->aspect_ratio = out_picref->video->sample_aspect_ratio;
        outFrame->tff          = out_picref->video->top_field_first;
        outFrame->rtStart      = out_picref->pts;
        outFrame->rtStop       = out_picref->pts + rtDuration;
        memcpy(outFrame->data, out_picref->data, 4 * sizeof(uint8_t*));
        memcpy(outFrame->stride, out_picref->linesize, 4 * sizeof(int));

        outFrame->destruct = avfilter_free_lav_buffer;
        outFrame->priv_data = out_picref;

        hrDeliver = (*this.*deliverFunc)(outFrame);
      }
    }

    avfilter_unref_buffer(in_picref);
    ReleaseFrame(&pFrame);

    return S_OK;
  } else {
    m_filterPixFmt = LAVPixFmt_None;
    deliver:
    return (*this.*deliverFunc)(pFrame);
  }
}
