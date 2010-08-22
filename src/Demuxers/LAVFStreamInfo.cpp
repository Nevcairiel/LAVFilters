/*
 *      Copyright (C) 2010 Hendrik Leppkes
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
 *
 *  Contributions by Ti-BEN from the XBMC DSPlayer Project, also under GPLv2
 */

#include "stdafx.h"
#include "LAVFStreamInfo.h"
#include "LAVFGuidHelper.h"
#include "moreuuids.h"

CLAVFStreamInfo::CLAVFStreamInfo(AVStream *avstream, const char* containerFormat, HRESULT &hr)
  : CStreamInfo(), m_containerFormat(containerFormat)
{
  m_containerFormat = std::string(containerFormat);

  switch(avstream->codec->codec_type) {
  case AVMEDIA_TYPE_AUDIO:
    hr = CreateAudioMediaType(avstream);
    break;
  case AVMEDIA_TYPE_VIDEO:
    hr = CreateVideoMediaType(avstream);
    break;
  case AVMEDIA_TYPE_SUBTITLE:
    hr = CreateSubtitleMediaType(avstream);
    break;
  default:
    hr = E_FAIL;
    break;
  }
}

CLAVFStreamInfo::~CLAVFStreamInfo()
{
}

STDMETHODIMP CLAVFStreamInfo::CreateAudioMediaType(AVStream *avstream)
{
  mtype = g_GuidHelper.initAudioType(avstream->codec->codec_id);
  WAVEFORMATEX* wvfmt = (WAVEFORMATEX*)mtype.AllocFormatBuffer(sizeof(WAVEFORMATEX) + avstream->codec->extradata_size);
  memset(wvfmt, 0, sizeof(WAVEFORMATEX));

  avstream->codec->codec_tag = av_codec_get_tag(mp_wav_taglists, avstream->codec->codec_id);

  // Check if its LATM by any chance
  // This doesn't seem to work with any decoders i tested, but at least we won't connect to any wrong decoder
  if(m_containerFormat == "mpegts" && avstream->codec->codec_id == CODEC_ID_AAC) {
    // PESContext in mpegts.c
    int *pes = (int *)avstream->priv_data;
    if(pes[2] == 0x11) {
      avstream->codec->codec_tag = WAVE_FORMAT_LATM_AAC;
      mtype.subtype = MEDIASUBTYPE_LATM_AAC;
    }
  }

  // TODO: values for this are non-trivial, see <mmreg.h>
  wvfmt->wFormatTag = avstream->codec->codec_tag;

  wvfmt->nChannels = avstream->codec->channels;
  wvfmt->nSamplesPerSec = avstream->codec->sample_rate;
  wvfmt->nAvgBytesPerSec = avstream->codec->bit_rate / 8;

  if(avstream->codec->codec_id == CODEC_ID_AAC) {
    wvfmt->wBitsPerSample = 0;
    wvfmt->nBlockAlign = 1;
  } else {
    wvfmt->wBitsPerSample = avstream->codec->bits_per_coded_sample;
    if (wvfmt->wBitsPerSample == 0) {
      wvfmt->wBitsPerSample = av_get_bits_per_sample_format(avstream->codec->sample_fmt);
    }

    if ( avstream->codec->block_align > 0 ) {
      wvfmt->nBlockAlign = avstream->codec->block_align;
    } else {
      if ( wvfmt->wBitsPerSample == 0 ) {
        DbgOutString(L"BitsPerSample is 0, no good!");
      }
      wvfmt->nBlockAlign = (WORD)((wvfmt->nChannels * wvfmt->wBitsPerSample) / 8);
    }
  }

  wvfmt->cbSize = avstream->codec->extradata_size;
  if (avstream->codec->extradata_size > 0) {
    memcpy(wvfmt + 1, avstream->codec->extradata, avstream->codec->extradata_size);
  }

  //TODO Fix the sample size
  //if (avstream->codec->bits_per_coded_sample == 0)
  mtype.SetSampleSize(256000);

  return S_OK;
}

STDMETHODIMP CLAVFStreamInfo::CreateVideoMediaType(AVStream *avstream)
{
  mtype = g_GuidHelper.initVideoType(avstream->codec->codec_id);
  mtype.bTemporalCompression = 1;
  mtype.bFixedSizeSamples = 0; // TODO

  avstream->codec->codec_tag = av_codec_get_tag(mp_bmp_taglists, avstream->codec->codec_id);

  // If we need aspect info, we switch to VIH2
  AVRational r = avstream->sample_aspect_ratio;
  if (mtype.formattype == FORMAT_VideoInfo && (r.den > 0 && r.num > 0 && (r.den > 1 || r.num > 1))) {
    mtype.formattype = FORMAT_VideoInfo2;
  }

  if (mtype.formattype == FORMAT_VideoInfo) {
    mtype.pbFormat = (BYTE *)g_GuidHelper.CreateVIH(avstream, &mtype.cbFormat);
  } else if (mtype.formattype == FORMAT_VideoInfo2) {
    mtype.pbFormat = (BYTE *)g_GuidHelper.CreateVIH2(avstream, &mtype.cbFormat, (m_containerFormat == "mpegts"));
  } else if (mtype.formattype == FORMAT_MPEGVideo) {
    mtype.pbFormat = (BYTE *)g_GuidHelper.CreateMPEG1VI(avstream, &mtype.cbFormat);
  } else if (mtype.formattype == FORMAT_MPEG2Video) {
    mtype.pbFormat = (BYTE *)g_GuidHelper.CreateMPEG2VI(avstream, &mtype.cbFormat, (m_containerFormat == "mpegts"));
    // mpeg-ts ships its stuff in annexb form, and MS defines annexb to go with H264 instead of AVC1
    // sadly, ffdshow doesnt connect to H264 (and doesnt work on annexb in general)
    if (m_containerFormat == "mpegts" && avstream->codec->codec_id == CODEC_ID_H264) {
      mtype.subtype = MEDIASUBTYPE_H264;
      ((MPEG2VIDEOINFO *)mtype.pbFormat)->hdr.bmiHeader.biCompression = FOURCCMap(&MEDIASUBTYPE_H264).Data1;
    }
  }

  // TODO fix sample size
  mtype.SetSampleSize(256000);

  return S_OK;
}

STDMETHODIMP CLAVFStreamInfo::CreateSubtitleMediaType(AVStream *avstream)
{
  // Skip teletext
  if (avstream->codec->codec_id == CODEC_ID_DVB_TELETEXT) {
    return E_FAIL;
  }
  mtype.InitMediaType();
  mtype.majortype = MEDIATYPE_Subtitle;
  mtype.formattype = FORMAT_SubtitleInfo;
  // create format info
  SUBTITLEINFO *subInfo = (SUBTITLEINFO *)mtype.AllocFormatBuffer(sizeof(SUBTITLEINFO) + avstream->codec->extradata_size);
  memset(subInfo, 0, mtype.FormatLength());

  if (av_metadata_get(avstream->metadata, "language", NULL, 0))
  {
    char *lang = av_metadata_get(avstream->metadata, "language", NULL, 0)->value;
    strncpy_s(subInfo->IsoLang, 4, lang, _TRUNCATE);
  }

  if (av_metadata_get(avstream->metadata, "title", NULL, 0))
  {
    // read metadata
    char *title = av_metadata_get(avstream->metadata, "title", NULL, 0)->value;
    // convert to wchar
    MultiByteToWideChar(CP_UTF8, 0, title, -1, subInfo->TrackName, 256);
  }

  // Extradata
  memcpy(mtype.pbFormat + (subInfo->dwOffset = sizeof(SUBTITLEINFO)), avstream->codec->extradata, avstream->codec->extradata_size);

  // TODO CODEC_ID_MOV_TEXT
  mtype.subtype = avstream->codec->codec_id == CODEC_ID_TEXT ? MEDIASUBTYPE_UTF8 :
                  avstream->codec->codec_id == CODEC_ID_SSA ? MEDIASUBTYPE_ASS :
                  avstream->codec->codec_id == CODEC_ID_HDMV_PGS_SUBTITLE ? MEDIASUBTYPE_HDMVSUB :
                  avstream->codec->codec_id == CODEC_ID_DVD_SUBTITLE ? MEDIASUBTYPE_VOBSUB :
                  avstream->codec->codec_id == CODEC_ID_DVB_SUBTITLE ? MEDIASUBTYPE_DVB_SUBTITLES :
                  MEDIASUBTYPE_NULL;

  return S_OK;
}
