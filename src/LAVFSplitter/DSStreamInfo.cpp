/*
 *      Copyright (C) 2005-2010 Team XBMC
 *      http://www.xbmc.org
 *
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
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 */

#include "stdafx.h"
#include "DSStreamInfo.h"
#include "DSGuidHelper.h"

CDSStreamInfo::CDSStreamInfo(AVStream *avstream, const char* containerFormat)
{
  m_containerFormat = std::string(containerFormat);

  mtype.InitMediaType();

  switch(avstream->codec->codec_type) {
  case AVMEDIA_TYPE_AUDIO:
    CreateAudioMediaType(avstream);
    break;
  case AVMEDIA_TYPE_VIDEO:
    CreateVideoMediaType(avstream);
    break;
  case AVMEDIA_TYPE_SUBTITLE:
    CreateSubtitleMediaType(avstream);
    break;
  default:
    break;
  }
}

CDSStreamInfo::~CDSStreamInfo()
{
}

STDMETHODIMP CDSStreamInfo::CreateAudioMediaType(AVStream *avstream)
{
  mtype = g_GuidHelper.initAudioType(avstream->codec->codec_id);
  WAVEFORMATEX* wvfmt = (WAVEFORMATEX*)mtype.AllocFormatBuffer(sizeof(WAVEFORMATEX) + avstream->codec->extradata_size);

  avstream->codec->codec_tag = av_codec_get_tag(mp_wav_taglists, avstream->codec->codec_id);

  // TODO: values for this are non-trivial, see <mmreg.h>
  wvfmt->wFormatTag = avstream->codec->codec_tag;

  wvfmt->nChannels = avstream->codec->channels;
  wvfmt->nSamplesPerSec = avstream->codec->sample_rate;
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

  wvfmt->nAvgBytesPerSec = avstream->codec->bit_rate / 8;

  wvfmt->cbSize = avstream->codec->extradata_size;
  if (avstream->codec->extradata_size > 0) {
    memcpy(wvfmt + 1, avstream->codec->extradata, avstream->codec->extradata_size);
  }

  //TODO Fix the sample size
  if (avstream->codec->bits_per_coded_sample == 0)
    mtype.SetSampleSize(256000);
  return S_OK;
}

STDMETHODIMP CDSStreamInfo::CreateVideoMediaType(AVStream *avstream)
{
  mtype = g_GuidHelper.initVideoType(avstream->codec->codec_id);
  mtype.bTemporalCompression = 1;
  mtype.bFixedSizeSamples = 0; // TODO

  avstream->codec->codec_tag = av_codec_get_tag(mp_bmp_taglists, avstream->codec->codec_id);

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

  return S_OK;
}

STDMETHODIMP CDSStreamInfo::CreateSubtitleMediaType(AVStream *avstream)
{
  mtype.InitMediaType();
  // TODO
  //mtype.majortype = MEDIATYPE_Subtitle;
  //mtype.formattype = FORMAT_SubtitleInfo;
  return S_OK;
}
