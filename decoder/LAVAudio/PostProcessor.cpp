/*
 *      Copyright (C) 2010-2021 Hendrik Leppkes
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
#include "PostProcessor.h"
#include "LAVAudio.h"
#include "Media.h"

extern "C"
{
#include "libavutil/intreadwrite.h"
};

// PCM Volume Adjustment Factors, both for integer and float math
// entries start at 2 channel mixing, half volume
static int pcm_volume_adjust_integer[7] = {362, 443, 512, 572, 627, 677, 724};

static float pcm_volume_adjust_float[7] = {1.41421356f, 1.73205081f, 2.00000000f, 2.23606798f,
                                           2.44948974f, 2.64575131f, 2.82842712f};

// SCALE_CA helper macro for SampleCopyAdjust
#define SCALE_CA(sample, iFactor, factor) \
    {                                     \
        if (iFactor > 0)                  \
        {                                 \
            sample *= factor;             \
            sample >>= 8;                 \
        }                                 \
        else                              \
        {                                 \
            sample <<= 8;                 \
            sample /= factor;             \
        }                                 \
    }

//
// Helper Function that reads one sample from pIn, applys the scale specified by iFactor, and writes it to pOut
//
static inline void SampleCopyAdjust(BYTE *pOut, const BYTE *pIn, int iFactor, LAVAudioSampleFormat sfSampleFormat)
{
    ASSERT(abs(iFactor) > 1 && abs(iFactor) <= 8);
    const int factorIndex = abs(iFactor) - 2;

    switch (sfSampleFormat)
    {
    case SampleFormat_U8: {
        uint8_t *pOutSample = pOut;
        int32_t sample = *pIn + INT8_MIN;
        SCALE_CA(sample, iFactor, pcm_volume_adjust_integer[factorIndex]);
        *pOutSample = av_clip_uint8(sample - INT8_MIN);
    }
    break;
    case SampleFormat_16: {
        int16_t *pOutSample = (int16_t *)pOut;
        int32_t sample = *((int16_t *)pIn);
        SCALE_CA(sample, iFactor, pcm_volume_adjust_integer[factorIndex]);
        *pOutSample = av_clip_int16(sample);
    }
    break;
    case SampleFormat_24: {
        int32_t sample = (pIn[0] << 8) + (pIn[1] << 16) + (pIn[2] << 24);
        sample >>= 8;
        SCALE_CA(sample, iFactor, pcm_volume_adjust_integer[factorIndex]);
        sample = av_clip(sample, INT24_MIN, INT24_MAX);
        pOut[0] = sample & 0xff;
        pOut[1] = (sample >> 8) & 0xff;
        pOut[2] = (sample >> 16) & 0xff;
    }
    break;
    case SampleFormat_32: {
        int32_t *pOutSample = (int32_t *)pOut;
        int64_t sample = *((int32_t *)pIn);
        SCALE_CA(sample, iFactor, pcm_volume_adjust_integer[factorIndex]);
        *pOutSample = av_clipl_int32(sample);
    }
    break;
    case SampleFormat_FP32: {
        float *pOutSample = (float *)pOut;
        float sample = *((float *)pIn);
        if (iFactor > 0)
        {
            sample *= pcm_volume_adjust_float[factorIndex];
        }
        else
        {
            sample /= pcm_volume_adjust_float[factorIndex];
        }
        *pOutSample = av_clipf(sample, -1.0f, 1.0f);
    }
    break;
    default: ASSERT(0); break;
    }
}

//
// Writes one sample of silence into the buffer
//
static inline void Silence(BYTE *pBuffer, LAVAudioSampleFormat sfSampleFormat)
{
    switch (sfSampleFormat)
    {
    case SampleFormat_16:
    case SampleFormat_24:
    case SampleFormat_32:
    case SampleFormat_FP32: memset(pBuffer, 0, get_byte_per_sample(sfSampleFormat)); break;
    case SampleFormat_U8: *pBuffer = 128U; break;
    default: ASSERT(0);
    }
}

//
// Extended Channel Remapping Processor
//
// This function can process a PCM buffer of any sample format, and remap the channels
// into any arbitrary layout and channel count.
//
// The samples are copied byte-by-byte, without any conversion or loss.
//
// The ExtendedChannelMap is assumed to always have at least uOutChannels valid entries.
// Its layout is in output format:
//      Map[0] is the first output channel, and should contain the index in the source stream (or -1 for silence)
//      Map[1] is the second output channel
//
// Source channels can be applied multiple times to the Destination, but multiple Source channels cannot be merged into one channel.
// Note that when copying one source channel into multiple destinations, you always want to reduce its volume.
// You can either do this in a second step, or use the factor documented below
//
// Examples:
// 5.1 Input Buffer, following map will extract the Center channel, and return it as Mono:
// uOutChannels == 1; map = {2}
//
// Mono Input Buffer, Convert to Stereo
// uOutChannels == 2; map = {0, 0}
//
// Additionally, a factor can be applied to all PCM samples
//
// For optimization, the factor cannot be freely specified.
// Factors -1, 0, 1 are ignored.
// A Factor of 2 doubles the volume, 3 trippled, etc.
// A Factor of -2 will produce half volume, -3 one third, etc.
// The limit is a factor of 8/-8
//
// Otherwise, see ChannelMapping
HRESULT ExtendedChannelMapping(BufferDetails *pcm, const unsigned uOutChannels, const ExtendedChannelMap extMap)
{
#ifdef DEBUG
    ASSERT(pcm && pcm->bBuffer);
    ASSERT(uOutChannels > 0 && uOutChannels <= 8);
    for (unsigned idx = 0; idx < uOutChannels; ++idx)
    {
        ASSERT(extMap[idx].idx >= -1 && extMap[idx].idx < pcm->layout.nb_channels);
    }
#endif
    // Sample Size
    const unsigned uSampleSize = get_byte_per_sample(pcm->sfFormat);

    // New Output Buffer
    GrowableArray<BYTE> *out = new GrowableArray<BYTE>();
    out->SetSize(uOutChannels * pcm->nSamples * uSampleSize);

    const BYTE *pIn = pcm->bBuffer->Ptr();
    BYTE *pOut = out->Ptr();

    for (unsigned i = 0; i < pcm->nSamples; ++i)
    {
        for (unsigned ch = 0; ch < uOutChannels; ++ch)
        {
            if (extMap[ch].idx >= 0)
            {
                if (!extMap[ch].factor || abs(extMap[ch].factor) == 1)
                    memcpy(pOut, pIn + (extMap[ch].idx * uSampleSize), uSampleSize);
                else
                    SampleCopyAdjust(pOut, pIn + (extMap[ch].idx * uSampleSize), extMap[ch].factor, pcm->sfFormat);
            }
            else
                Silence(pOut, pcm->sfFormat);
            pOut += uSampleSize;
        }
        pIn += uSampleSize * pcm->layout.nb_channels;
    }

    // Apply changes to buffer
    delete pcm->bBuffer;
    pcm->bBuffer = out;
    av_channel_layout_uninit(&pcm->layout);
    pcm->layout.order = AV_CHANNEL_ORDER_UNSPEC;
    pcm->layout.nb_channels = uOutChannels;

    return S_OK;
}

#define CHL_CONTAINS_ALL(l, m) (((l) & (m)) == (m))
#define CHL_ALL_OR_NONE(l, m) (((l) & (m)) == (m) || ((l) & (m)) == 0)

HRESULT CLAVAudio::CheckChannelLayoutConformity(AVChannelLayout * layout)
{
    int channels = layout->nb_channels;

    if (layout->order != AV_CHANNEL_ORDER_NATIVE)
    {
        DbgLog((LOG_ERROR, 10,
                L"::CheckChannelLayoutConformity(): Only native channel orders are supported"));
        goto noprocessing;
    }

    if (layout->u.mask > INT32_MAX)
    {
        DbgLog((LOG_ERROR, 10,
                L"::CheckChannelLayoutConformity(): Layout channels above 32-bit are not handled (mask: 0x%llx)",
                layout->u.mask));
        goto noprocessing;
    }

    DWORD dwMask = (DWORD)layout->u.mask;

    // We require multi-channel and at least containing stereo
    if (!CHL_CONTAINS_ALL(dwMask, AV_CH_LAYOUT_STEREO) || channels == 2)
        goto noprocessing;

    // We do not know what to do with "top" channels
    if (dwMask & (AV_CH_TOP_CENTER | AV_CH_TOP_FRONT_LEFT | AV_CH_TOP_FRONT_CENTER | AV_CH_TOP_FRONT_RIGHT |
                    AV_CH_TOP_BACK_LEFT | AV_CH_TOP_BACK_CENTER | AV_CH_TOP_BACK_RIGHT))
    {
        DbgLog((LOG_ERROR, 10,
                L"::CheckChannelLayoutConformity(): Layout with top channels is not supported (mask: 0x%x)", dwMask));
        goto noprocessing;
    }

    // We need either both surround channels, or none. One of a type is not supported
    if (!CHL_ALL_OR_NONE(dwMask, AV_CH_BACK_LEFT | AV_CH_BACK_RIGHT) ||
        !CHL_ALL_OR_NONE(dwMask, AV_CH_SIDE_LEFT | AV_CH_SIDE_RIGHT) ||
        !CHL_ALL_OR_NONE(dwMask, AV_CH_FRONT_LEFT_OF_CENTER | AV_CH_FRONT_RIGHT_OF_CENTER))
    {
        DbgLog(
            (LOG_ERROR, 10,
             L"::CheckChannelLayoutConformity(): Layout with only one surround channel is not supported (mask: 0x%x)",
             dwMask));
        goto noprocessing;
    }

    // No need to process full 5.1/6.1 layouts, or any 8 channel layouts
    if (dwMask == AV_CH_LAYOUT_5POINT1 || dwMask == AV_CH_LAYOUT_5POINT1_BACK || dwMask == AV_CH_LAYOUT_6POINT1_BACK ||
        channels == 8)
    {
        DbgLog((LOG_TRACE, 10, L"::CheckChannelLayoutConformity(): Layout is already a default layout (mask: 0x%x)",
                dwMask));
        goto noprocessing;
    }

    // Check 5.1 channels
    if (CHL_CONTAINS_ALL(AV_CH_LAYOUT_5POINT1, dwMask)            /* 5.1 with side channels */
        || CHL_CONTAINS_ALL(AV_CH_LAYOUT_5POINT1_BACK, dwMask)    /* 5.1 with back channels */
        || CHL_CONTAINS_ALL(LAV_CH_LAYOUT_5POINT1_WIDE, dwMask)  /* 5.1 with side-front channels */
        || CHL_CONTAINS_ALL(LAV_CH_LAYOUT_5POINT1_BC, dwMask))  /* 3/1/x layouts, front channels with a back center */
        return Create51Conformity(dwMask);

    // Check 6.1 channels (5.1 layouts + Back Center)
    if (CHL_CONTAINS_ALL(AV_CH_LAYOUT_6POINT1, dwMask)           /* 6.1 with side channels */
        || CHL_CONTAINS_ALL(AV_CH_LAYOUT_6POINT1_BACK, dwMask) /* 6.1 with back channels */
        || CHL_CONTAINS_ALL(LAV_CH_LAYOUT_5POINT1_WIDE | AV_CH_BACK_CENTER, dwMask)) /* 6.1 with side-front channels */
        return Create61Conformity(dwMask);

    // Check 7.1 channels
    if (CHL_CONTAINS_ALL(AV_CH_LAYOUT_7POINT1, dwMask)                  /* 7.1 with side+back channels */
        || CHL_CONTAINS_ALL(AV_CH_LAYOUT_7POINT1_WIDE, dwMask)          /* 7.1 with front-side+back channels */
        || CHL_CONTAINS_ALL(LAV_CH_LAYOUT_7POINT1_EXTRAWIDE, dwMask)) /* 7.1 with front-side+side channels */
        return Create71Conformity(dwMask);

noprocessing:
    m_bChannelMappingRequired = FALSE;
    return S_FALSE;
}

HRESULT CLAVAudio::Create51Conformity(DWORD dwLayout)
{
    DbgLog((LOG_TRACE, 10, L"::Create51Conformity(): Creating 5.1 default layout (mask: 0x%x)", dwLayout));
    int ch = 0;
    ExtChMapClear(&m_ChannelMap);
    // All layouts we support have to contain L/R
    ExtChMapSet(&m_ChannelMap, 0, ch++, 0);
    ExtChMapSet(&m_ChannelMap, 1, ch++, 0);
    // Center channel
    if (dwLayout & AV_CH_FRONT_CENTER)
        ExtChMapSet(&m_ChannelMap, 2, ch++, 0);
    // LFE
    if (dwLayout & AV_CH_LOW_FREQUENCY)
        ExtChMapSet(&m_ChannelMap, 3, ch++, 0);
    // Back/Side
    if (dwLayout & (AV_CH_SIDE_LEFT | AV_CH_BACK_LEFT | AV_CH_FRONT_LEFT_OF_CENTER))
    {
        ExtChMapSet(&m_ChannelMap, 4, ch++, 0);
        ExtChMapSet(&m_ChannelMap, 5, ch++, 0);
        // Back Center
    }
    else if (dwLayout & AV_CH_BACK_CENTER)
    {
        ExtChMapSet(&m_ChannelMap, 4, ch, -2);
        ExtChMapSet(&m_ChannelMap, 5, ch++, -2);
    }
    m_bChannelMappingRequired = TRUE;
    av_channel_layout_uninit(&m_ChannelMapOutputLayout);
    av_channel_layout_from_mask(&m_ChannelMapOutputLayout, AV_CH_LAYOUT_5POINT1);
    return S_OK;
}

HRESULT CLAVAudio::Create61Conformity(DWORD dwLayout)
{
    if (m_settings.Expand61)
    {
        DbgLog((LOG_TRACE, 10, L"::Create61Conformity(): Expanding to 7.1"));
        return Create71Conformity(dwLayout);
    }

    DbgLog((LOG_TRACE, 10, L"::Create61Conformity(): Creating 6.1 default layout (mask: 0x%x)", dwLayout));
    int ch = 0;
    ExtChMapClear(&m_ChannelMap);
    // All layouts we support have to contain L/R
    ExtChMapSet(&m_ChannelMap, 0, ch++, 0);
    ExtChMapSet(&m_ChannelMap, 1, ch++, 0);
    // Center channel
    if (dwLayout & AV_CH_FRONT_CENTER)
        ExtChMapSet(&m_ChannelMap, 2, ch++, 0);
    // LFE
    if (dwLayout & AV_CH_LOW_FREQUENCY)
        ExtChMapSet(&m_ChannelMap, 3, ch++, 0);
    // Back channels, if before BC
    if (dwLayout & (AV_CH_BACK_LEFT | AV_CH_FRONT_LEFT_OF_CENTER))
    {
        DbgLog((LOG_TRACE, 10, L"::Create61Conformity(): Using surround channels *before* BC"));
        ExtChMapSet(&m_ChannelMap, 4, ch++, 0);
        ExtChMapSet(&m_ChannelMap, 5, ch++, 0);
    }
    // Back Center
    if (dwLayout & AV_CH_BACK_CENTER)
        ExtChMapSet(&m_ChannelMap, 6, ch++, 0);
    // Back channels, if after BC
    if (dwLayout & AV_CH_SIDE_LEFT)
    {
        DbgLog((LOG_TRACE, 10, L"::Create61Conformity(): Using surround channels *after* BC"));
        ExtChMapSet(&m_ChannelMap, 4, ch++, 0);
        ExtChMapSet(&m_ChannelMap, 5, ch++, 0);
    }

    m_bChannelMappingRequired = TRUE;
    av_channel_layout_uninit(&m_ChannelMapOutputLayout);
    av_channel_layout_from_mask(&m_ChannelMapOutputLayout, AV_CH_LAYOUT_6POINT1_BACK);
    return S_OK;
}

HRESULT CLAVAudio::Create71Conformity(DWORD dwLayout)
{
    DbgLog((LOG_TRACE, 10, L"::Create71Conformity(): Creating 7.1 default layout (mask: 0x%x)", dwLayout));
    int ch = 0;
    ExtChMapClear(&m_ChannelMap);
    // All layouts we support have to contain L/R
    ExtChMapSet(&m_ChannelMap, 0, ch++, 0);
    ExtChMapSet(&m_ChannelMap, 1, ch++, 0);
    // Center channel
    if (dwLayout & AV_CH_FRONT_CENTER)
        ExtChMapSet(&m_ChannelMap, 2, ch++, 0);
    // LFE
    if (dwLayout & AV_CH_LOW_FREQUENCY)
        ExtChMapSet(&m_ChannelMap, 3, ch++, 0);
    // Back channels
    if (dwLayout & AV_CH_BACK_CENTER)
    {
        DbgLog((LOG_TRACE, 10, L"::Create71Conformity(): Usign BC to fill back channels"));
        if (dwLayout & AV_CH_SIDE_LEFT)
        {
            DbgLog((LOG_TRACE, 10, L"::Create71Conformity(): Using BC before side-surround channels"));
            ExtChMapSet(&m_ChannelMap, 4, ch, -2);
            ExtChMapSet(&m_ChannelMap, 5, ch++, -2);
            ExtChMapSet(&m_ChannelMap, 6, ch++, 0);
            ExtChMapSet(&m_ChannelMap, 7, ch++, 0);
        }
        else
        {
            DbgLog((LOG_TRACE, 10, L"::Create71Conformity(): Using BC after side-surround channels"));
            ExtChMapSet(&m_ChannelMap, 6, ch++, 0);
            ExtChMapSet(&m_ChannelMap, 7, ch++, 0);
            ExtChMapSet(&m_ChannelMap, 4, ch, -2);
            ExtChMapSet(&m_ChannelMap, 5, ch++, -2);
        }
    }
    else
    {
        DbgLog((LOG_TRACE, 10, L"::Create71Conformity(): Using original 4 surround channels"));
        ExtChMapSet(&m_ChannelMap, 4, ch++, 0);
        ExtChMapSet(&m_ChannelMap, 5, ch++, 0);
        ExtChMapSet(&m_ChannelMap, 6, ch++, 0);
        ExtChMapSet(&m_ChannelMap, 7, ch++, 0);
    }

    m_bChannelMappingRequired = TRUE;
    av_channel_layout_uninit(&m_ChannelMapOutputLayout);
    av_channel_layout_from_mask(&m_ChannelMapOutputLayout, AV_CH_LAYOUT_7POINT1);
    return S_OK;
}

HRESULT CLAVAudio::PadTo32(BufferDetails *buffer)
{
    ASSERT(buffer->sfFormat == SampleFormat_24);

    const DWORD size = (buffer->nSamples * buffer->layout.nb_channels) * 4;
    GrowableArray<BYTE> *pcmOut = new GrowableArray<BYTE>();
    pcmOut->SetSize(size);

    const BYTE *pDataIn = buffer->bBuffer->Ptr();
    BYTE *pDataOut = pcmOut->Ptr();

    for (unsigned int i = 0; i < buffer->nSamples; ++i)
    {
        for (int ch = 0; ch < buffer->layout.nb_channels; ++ch)
        {
            AV_WL32(pDataOut, AV_RL24(pDataIn) << 8);
            pDataOut += 4;
            pDataIn += 3;
        }
    }
    delete buffer->bBuffer;
    buffer->bBuffer = pcmOut;
    buffer->sfFormat = SampleFormat_32;
    buffer->wBitsPerSample = 24;

    return S_OK;
}

HRESULT CLAVAudio::Truncate32Buffer(BufferDetails *buffer)
{
    ASSERT(buffer->sfFormat == SampleFormat_32 && buffer->wBitsPerSample <= 24);

    // Determine the bytes per sample to keep. Cut a 16-bit sample to 24 if 16-bit is disabled for some reason
    const int bytes_per_sample = buffer->wBitsPerSample <= 16 && GetSampleFormat(SampleFormat_16) ? 2 : 3;
    if (bytes_per_sample == 3 && !GetSampleFormat(SampleFormat_24))
        return S_FALSE;

    const int skip = 4 - bytes_per_sample;
    const DWORD size = (buffer->nSamples * buffer->layout.nb_channels) * bytes_per_sample;
    GrowableArray<BYTE> *pcmOut = new GrowableArray<BYTE>();
    pcmOut->SetSize(size);

    const BYTE *pDataIn = buffer->bBuffer->Ptr();
    BYTE *pDataOut = pcmOut->Ptr();

    pDataIn += skip;
    for (unsigned int i = 0; i < buffer->nSamples; ++i)
    {
        for (int ch = 0; ch < buffer->layout.nb_channels; ++ch)
        {
            memcpy(pDataOut, pDataIn, bytes_per_sample);
            pDataOut += bytes_per_sample;
            pDataIn += 4;
        }
    }

    delete buffer->bBuffer;
    buffer->bBuffer = pcmOut;
    buffer->sfFormat = bytes_per_sample == 3 ? SampleFormat_24 : SampleFormat_16;

    return S_OK;
}

HRESULT CLAVAudio::PerformAVRProcessing(BufferDetails *buffer)
{
    int ret = 0;

    AVChannelLayout chMixingLayout{};
    if (av_channel_layout_check(&m_chOverrideMixer) != 0)
    {
        av_channel_layout_copy(&chMixingLayout, &m_chOverrideMixer);
    }
    else if (m_settings.MixingEnabled)
    {
        av_channel_layout_from_mask(&chMixingLayout, m_settings.MixingLayout);
    }
    else
    {
        av_channel_layout_copy(&chMixingLayout, &buffer->layout);
    }

    // No mixing stereo, if the user doesn't want it
    if (buffer->layout.nb_channels <= 2 && (m_settings.MixingFlags & LAV_MIXING_FLAG_UNTOUCHED_STEREO))
    {
        av_channel_layout_uninit(&chMixingLayout);
        av_channel_layout_copy(&chMixingLayout, &buffer->layout);
    }

    LAVAudioSampleFormat outputFormat = av_channel_layout_compare(&buffer->layout, &chMixingLayout) != 0
                                            ? GetBestAvailableSampleFormat(SampleFormat_FP32)
                                            : GetBestAvailableSampleFormat(buffer->sfFormat);

    // Short Circuit some processing
    if (av_channel_layout_compare(&buffer->layout, &chMixingLayout) == 0 && !buffer->bPlanar)
    {
        if (buffer->sfFormat == SampleFormat_24 && outputFormat == SampleFormat_32)
        {
            PadTo32(buffer);
            return S_OK;
        }
        else if (buffer->sfFormat == SampleFormat_32 && outputFormat == SampleFormat_24)
        {
            buffer->wBitsPerSample = 24;
            Truncate32Buffer(buffer);
            return S_OK;
        }
    }

    // Sadly, we need to convert this, avresample has no 24-bit mode
    if (buffer->sfFormat == SampleFormat_24)
    {
        PadTo32(buffer);
    }

    if (av_channel_layout_compare(&buffer->layout, &m_MixingInputLayout) != 0 || (!m_swrContext && !m_bAVResampleFailed) || m_bMixingSettingsChanged ||
        av_channel_layout_compare(&m_chRemixLayout, &chMixingLayout) != 0 || outputFormat != m_sfRemixFormat ||
        buffer->sfFormat != m_MixingInputFormat)
    {
        m_bAVResampleFailed = FALSE;
        m_bMixingSettingsChanged = FALSE;
        if (m_swrContext)
        {
            swr_free(&m_swrContext);
        }

        av_channel_layout_copy(&m_MixingInputLayout, &buffer->layout);
        av_channel_layout_copy(&m_chRemixLayout, &chMixingLayout);

        m_MixingInputFormat = buffer->sfFormat;
        m_sfRemixFormat = outputFormat;

        swr_alloc_set_opts2(&m_swrContext, &chMixingLayout, get_ff_sample_fmt(m_sfRemixFormat), buffer->dwSamplesPerSec,
                               &buffer->layout, get_ff_sample_fmt(buffer->sfFormat), buffer->dwSamplesPerSec, 0, NULL);

        av_opt_set_int(m_swrContext, "dither_method",
                       m_settings.SampleConvertDither ? SWR_DITHER_TRIANGULAR_HIGHPASS : SWR_DITHER_NONE, 0);

        // Setup mixing properties, if needed
        if (av_channel_layout_compare(&buffer->layout, &chMixingLayout) != 0)
        {
            ASSERT(chMixingLayout.order == AV_CHANNEL_ORDER_NATIVE);

            BOOL bNormalize = !!(m_settings.MixingFlags & LAV_MIXING_FLAG_NORMALIZE_MATRIX);
            av_opt_set_int(m_swrContext, "clip_protection",
                           !bNormalize && (m_settings.MixingFlags & LAV_MIXING_FLAG_CLIP_PROTECTION), 0);
            av_opt_set_int(m_swrContext, "internal_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);

            // setup matrix parameters
            const double center_mix_level = (double)m_settings.MixingCenterLevel / 10000.0;
            const double surround_mix_level = (double)m_settings.MixingSurroundLevel / 10000.0;
            const double lfe_mix_level =
                (double)m_settings.MixingLFELevel / 10000.0 / (chMixingLayout.u.mask == AV_CH_LAYOUT_MONO ? 1.0 : M_SQRT1_2);

            av_opt_set_double(m_swrContext, "center_mix_level", center_mix_level, 0);
            av_opt_set_double(m_swrContext, "surround_mix_level", surround_mix_level, 0);
            av_opt_set_double(m_swrContext, "lfe_mix_level", lfe_mix_level, 0);
            av_opt_set_double(m_swrContext, "rematrix_maxval", bNormalize ? 1.0 : 0.0, 0);
            av_opt_set_int(m_swrContext, "matrix_encoding", (AVMatrixEncoding)m_settings.MixingMode, 0);
        }

        // Open Resample Context
        ret = swr_init(m_swrContext);
        if (ret < 0)
        {
            DbgLog((LOG_ERROR, 10, L"swr_init failed"));
            goto setuperr;
        }
    }

    if (!m_swrContext)
    {
        DbgLog((LOG_ERROR, 10, L"swresample context missing?"));
        return E_FAIL;
    }

    LAVAudioSampleFormat bufferFormat =
        (m_sfRemixFormat == SampleFormat_24) ? SampleFormat_32 : m_sfRemixFormat; // avresample always outputs 32-bit

    GrowableArray<BYTE> *pcmOut = new GrowableArray<BYTE>();
    pcmOut->Allocate(FFALIGN(buffer->nSamples, 32) * m_chRemixLayout.nb_channels *
                     get_byte_per_sample(bufferFormat));
    BYTE *pOut = pcmOut->Ptr();

    BYTE *pIn = buffer->bBuffer->Ptr();
    ret = swr_convert(m_swrContext, &pOut, pcmOut->GetAllocated(), (const uint8_t **)&pIn, buffer->nSamples);
    if (ret < 0)
    {
        DbgLog((LOG_ERROR, 10, L"swr_convert failed"));
        delete pcmOut;
        return S_FALSE;
    }

    delete buffer->bBuffer;
    buffer->bBuffer = pcmOut;
    av_channel_layout_copy(&buffer->layout, &m_chRemixLayout);
    buffer->sfFormat = bufferFormat;
    buffer->wBitsPerSample = get_byte_per_sample(m_sfRemixFormat) << 3;
    buffer->bBuffer->SetSize(buffer->layout.nb_channels * buffer->nSamples * get_byte_per_sample(buffer->sfFormat));

    return S_OK;
setuperr:
    swr_free(&m_swrContext);
    m_bAVResampleFailed = TRUE;
    return E_FAIL;
}

HRESULT CLAVAudio::PostProcess(BufferDetails *buffer)
{
    // Validate channel mask
    if (buffer->layout.order == AV_CHANNEL_ORDER_UNSPEC || (buffer->layout.order == AV_CHANNEL_ORDER_NATIVE && buffer->layout.u.mask == 0) || (buffer->layout.order != AV_CHANNEL_ORDER_UNSPEC && buffer->layout.order != AV_CHANNEL_ORDER_NATIVE))
    {
        int layout_channels = buffer->layout.nb_channels;

        av_channel_layout_uninit(&buffer->layout);
        DWORD dwMask = get_channel_mask(layout_channels);
        if (dwMask)
        {
            av_channel_layout_from_mask(&buffer->layout, dwMask);
        }
        else
        {
            buffer->layout.order = AV_CHANNEL_ORDER_UNSPEC;
            buffer->layout.nb_channels = layout_channels;
        }
    }

    if (m_settings.SuppressFormatChanges)
    {
        if (av_channel_layout_check(&m_SuppressLayout) == 0)
        {
            av_channel_layout_copy(&m_SuppressLayout, &buffer->layout);
        }
        else
        {
            if (av_channel_layout_compare(&buffer->layout, &m_SuppressLayout) != 0 && buffer->layout.nb_channels <= m_SuppressLayout.nb_channels)
            {
                // only warn once
                if (av_channel_layout_compare(&m_chOverrideMixer, &m_SuppressLayout) != 0)
                {
                    DbgLog((LOG_TRACE, 10, L"Channel Format change suppressed"));
                    av_channel_layout_copy(&m_chOverrideMixer, &m_SuppressLayout);
                }
            }
            else if (buffer->layout.nb_channels > m_SuppressLayout.nb_channels)
            {
                DbgLog((LOG_TRACE, 10, L"Channel count increased, allowing change"));
                av_channel_layout_uninit(&m_chOverrideMixer);
                av_channel_layout_copy(&m_SuppressLayout, &buffer->layout);
            }
        }
    }

    ASSERT(buffer->layout.order == AV_CHANNEL_ORDER_NATIVE || buffer->layout.order == AV_CHANNEL_ORDER_UNSPEC);

    BOOL bOverrideMixing = av_channel_layout_check(&m_chOverrideMixer) != 0;

    // determine the layout we're mixing to
    AVChannelLayout chMixingLayout{};
    if (bOverrideMixing)
        av_channel_layout_copy(&chMixingLayout, &m_chOverrideMixer);
    else if (m_settings.MixingEnabled)
        av_channel_layout_from_mask(&chMixingLayout, m_settings.MixingLayout);

    BOOL bMixing = (m_settings.MixingEnabled || bOverrideMixing) &&
                   av_channel_layout_compare(&buffer->layout, &chMixingLayout) != 0;
    LAVAudioSampleFormat outputFormat = GetBestAvailableSampleFormat(buffer->sfFormat);
    // Perform conversion to layout and sample format, if required
    if (bMixing || outputFormat != buffer->sfFormat)
    {
        PerformAVRProcessing(buffer);
    }

    // Remap to standard configurations, if requested (not in combination with mixing)
    if (!bMixing && m_settings.OutputStandardLayout)
    {
        if (av_channel_layout_compare(&buffer->layout, &m_DecodeLayoutSanified) != 0)
        {
            av_channel_layout_copy(&m_DecodeLayoutSanified, &buffer->layout);
            CheckChannelLayoutConformity(&buffer->layout);
        }
        if (m_bChannelMappingRequired)
        {
            ExtendedChannelMapping(buffer, m_ChannelMapOutputLayout.nb_channels, m_ChannelMap);
            av_channel_layout_copy(&buffer->layout, &m_ChannelMapOutputLayout);
        }
    }

    // Map to the requested 5.1 layout
    if (m_settings.Output51Legacy && buffer->layout.u.mask == AV_CH_LAYOUT_5POINT1)
        buffer->layout.u.mask = AV_CH_LAYOUT_5POINT1_BACK;
    else if (!m_settings.Output51Legacy && buffer->layout.u.mask == AV_CH_LAYOUT_5POINT1_BACK)
        buffer->layout.u.mask = AV_CH_LAYOUT_5POINT1;

    // Check if current output uses back layout, and keep it active in that case
    if (buffer->layout.u.mask == AV_CH_LAYOUT_5POINT1)
    {
        WAVEFORMATEX *wfe = (WAVEFORMATEX *)m_pOutput->CurrentMediaType().Format();
        if (wfe->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        {
            WAVEFORMATEXTENSIBLE *wfex = (WAVEFORMATEXTENSIBLE *)wfe;
            if (wfex->dwChannelMask == AV_CH_LAYOUT_5POINT1_BACK)
                buffer->layout.u.mask = AV_CH_LAYOUT_5POINT1_BACK;
        }
    }

    // Mono -> Stereo expansion
    if (buffer->layout.nb_channels == 1 && m_settings.ExpandMono)
    {
        ExtendedChannelMap map = {{0, -2}, {0, -2}};
        ExtendedChannelMapping(buffer, 2, map);
        av_channel_layout_uninit(&buffer->layout);
        av_channel_layout_default(&buffer->layout, 2);
    }

    // 6.1 -> 7.1 expansion
    if (m_settings.Expand61)
    {
        if (buffer->layout.u.mask == AV_CH_LAYOUT_6POINT1_BACK)
        {
            ExtendedChannelMap map = {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {6, -2}, {6, -2}, {4, 0}, {5, 0}};
            ExtendedChannelMapping(buffer, 8, map);
            buffer->layout.u.mask = AV_CH_LAYOUT_7POINT1;
        }
        else if (buffer->layout.u.mask == AV_CH_LAYOUT_6POINT1)
        {
            ExtendedChannelMap map = {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, -2}, {4, -2}, {5, 0}, {6, 0}};
            ExtendedChannelMapping(buffer, 8, map);
            buffer->layout.u.mask = AV_CH_LAYOUT_7POINT1;
        }
    }

    if (m_bVolumeStats)
    {
        UpdateVolumeStats(*buffer);
    }

    // Truncate 24-in-32 to real 24
    if (buffer->sfFormat == SampleFormat_32 && buffer->wBitsPerSample && buffer->wBitsPerSample <= 24)
    {
        Truncate32Buffer(buffer);
    }

    return S_OK;
}
