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
 *
 *  Contributions by Ti-BEN from the XBMC DSPlayer Project, also under GPLv2
 */

#include "stdafx.h"
#include "LAVFStreamInfo.h"
#include "LAVFVideoHelper.h"
#include "LAVFAudioHelper.h"
#include "moreuuids.h"

#include <vector>

CLAVFStreamInfo::CLAVFStreamInfo(AVStream *avstream, const char* containerFormat, HRESULT &hr)
  : CStreamInfo(), m_containerFormat(containerFormat)
{
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
  if (avstream->codec->codec_tag == 0) {
    avstream->codec->codec_tag = av_codec_get_tag(mp_wav_taglists, avstream->codec->codec_id);
  }

  CMediaType mtype = g_AudioHelper.initAudioType(avstream->codec->codec_id, avstream->codec->codec_tag);

  if(mtype.formattype == FORMAT_WaveFormatEx) {
    // Special Logic for the MPEG1 Audio Formats (MP1, MP2)
    if(mtype.subtype == MEDIASUBTYPE_MPEG1AudioPayload) {
      mtype.pbFormat = (BYTE *)g_AudioHelper.CreateMP1WVFMT(avstream, &mtype.cbFormat);
    } else if (mtype.subtype == MEDIASUBTYPE_BD_LPCM_AUDIO) {
      mtype.pbFormat = (BYTE *)g_AudioHelper.CreateWVFMTEX_LPCM(avstream, &mtype.cbFormat);
      mtypes.push_back(mtype);
      mtype.subtype = MEDIASUBTYPE_HDMV_LPCM_AUDIO;
    } else if ((mtype.subtype == MEDIASUBTYPE_PCM || mtype.subtype == MEDIASUBTYPE_IEEE_FLOAT) && avstream->codec->codec_tag != WAVE_FORMAT_EXTENSIBLE) {
      // Create raw PCM media type
      mtype.pbFormat = (BYTE *)g_AudioHelper.CreateWFMTEX_RAW_PCM(avstream, &mtype.cbFormat, mtype.subtype);
    } else {
      WAVEFORMATEX *wvfmt = g_AudioHelper.CreateWVFMTEX(avstream, &mtype.cbFormat);

      if (avstream->codec->codec_tag == WAVE_FORMAT_EXTENSIBLE && avstream->codec->extradata_size >= 22) {
        // The WAVEFORMATEXTENSIBLE GUID is not recognized by the audio renderers
        // Set the actual subtype as GUID
        WAVEFORMATEXTENSIBLE *wvfmtex = (WAVEFORMATEXTENSIBLE *)wvfmt;
        mtype.subtype = wvfmtex->SubFormat;
      }
      mtype.pbFormat = (BYTE *)wvfmt;

      if (avstream->codec->codec_id == CODEC_ID_FLAC) {
        mtype.subtype = MEDIASUBTYPE_FLAC_FRAMED;
        mtypes.push_back(mtype);
        mtype.subtype = MEDIASUBTYPE_FLAC;
      } else if (avstream->codec->codec_id == CODEC_ID_DTS) {
        mtypes.push_back(mtype);
        mtype.subtype = FOURCCMap(WAVE_FORMAT_DTS2);
        wvfmt->wFormatTag = WAVE_FORMAT_DTS2;
      } else if (avstream->codec->codec_id == CODEC_ID_TRUEHD) {
        //wvfmt->wFormatTag = (WORD)WAVE_FORMAT_TRUEHD;
        mtypes.push_back(mtype);
        mtype.subtype = MEDIASUBTYPE_DOLBY_TRUEHD_ARCSOFT;
      }
    }
  } else if (mtype.formattype == FORMAT_VorbisFormat2 && mtype.subtype == MEDIASUBTYPE_Vorbis2) {
    // With Matroska and Ogg we know how to split up the extradata
    // and put it into a VorbisFormat2
    if (m_containerFormat == "matroska" || m_containerFormat == "ogg") {
      BYTE *vorbis2 = (BYTE *)g_AudioHelper.CreateVorbis2(avstream, &mtype.cbFormat);
      if (vorbis2) {
        mtype.pbFormat = vorbis2;
        mtypes.push_back(mtype);
      }
    }
    // Old vorbis header without extradata
    mtype.subtype = MEDIASUBTYPE_Vorbis;
    mtype.formattype = FORMAT_VorbisFormat;
    mtype.pbFormat = (BYTE *)g_AudioHelper.CreateVorbis(avstream, &mtype.cbFormat);
  }

  mtypes.push_back(mtype);

  // Create our special media type
  CMediaType ff_mtype;
  ff_mtype.InitMediaType();
  ff_mtype.SetSampleSize(256000);
  ff_mtype.majortype = MEDIATYPE_Audio;
  ff_mtype.subtype = MEDIASUBTYPE_FFMPEG_AUDIO;
  ff_mtype.formattype = FORMAT_WaveFormatExFFMPEG;
  ff_mtype.pbFormat = (BYTE *)g_AudioHelper.CreateWVFMTEX_FF(avstream, &ff_mtype.cbFormat);
  mtypes.push_back(ff_mtype);

  return S_OK;
}

STDMETHODIMP CLAVFStreamInfo::CreateVideoMediaType(AVStream *avstream)
{
  if (avstream->codec->codec_tag == 0) {
    avstream->codec->codec_tag = av_codec_get_tag(mp_bmp_taglists, avstream->codec->codec_id);
  }
  CMediaType mtype = g_VideoHelper.initVideoType(avstream->codec->codec_id, avstream->codec->codec_tag, m_containerFormat);

  mtype.bTemporalCompression = 1;
  mtype.bFixedSizeSamples = 0; // TODO

  // Somewhat hackish to force VIH for AVI content.
  // TODO: Figure out why exactly this is required
  if (m_containerFormat == "avi") {
    mtype.formattype = FORMAT_VideoInfo;
  }

  // If we need aspect info, we switch to VIH2
  AVRational r = avstream->sample_aspect_ratio;
  if (mtype.formattype == FORMAT_VideoInfo && (r.den > 0 && r.num > 0 && (r.den > 1 || r.num > 1))) {
    mtype.formattype = FORMAT_VideoInfo2;
  }

  if (mtype.formattype == FORMAT_VideoInfo) {
    mtype.pbFormat = (BYTE *)g_VideoHelper.CreateVIH(avstream, &mtype.cbFormat);
  } else if (mtype.formattype == FORMAT_VideoInfo2) {
    mtype.pbFormat = (BYTE *)g_VideoHelper.CreateVIH2(avstream, &mtype.cbFormat, m_containerFormat);
    if (avstream->codec->codec_id == CODEC_ID_VC1) {
      // If we send the cyberlink subtype first, it'll work with it, and with ffdshow, dmo and mpc-hc internal
      mtype.subtype = MEDIASUBTYPE_WVC1_CYBERLINK;
      mtypes.push_back(mtype);
      mtype.subtype = MEDIASUBTYPE_WVC1_ARCSOFT;
      mtypes.push_back(mtype);
      mtype.subtype = MEDIASUBTYPE_WVC1;
    }
  } else if (mtype.formattype == FORMAT_MPEGVideo) {
    mtype.pbFormat = (BYTE *)g_VideoHelper.CreateMPEG1VI(avstream, &mtype.cbFormat);
  } else if (mtype.formattype == FORMAT_MPEG2Video) {
    mtype.pbFormat = (BYTE *)g_VideoHelper.CreateMPEG2VI(avstream, &mtype.cbFormat, m_containerFormat);
  }

  mtypes.push_back(mtype);
  return S_OK;
}

STDMETHODIMP CLAVFStreamInfo::CreateSubtitleMediaType(AVStream *avstream)
{
  // Skip teletext
  if (avstream->codec->codec_id == CODEC_ID_DVB_TELETEXT) {
    return E_FAIL;
  }
  CMediaType mtype;
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

  mtypes.push_back(mtype);
  return S_OK;
}
