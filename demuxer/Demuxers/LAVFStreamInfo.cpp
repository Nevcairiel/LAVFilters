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
 *
 *  Contributions by Ti-BEN from the XBMC DSPlayer Project, also under GPLv2
 */

#include "stdafx.h"
#include "LAVFStreamInfo.h"
#include "LAVFVideoHelper.h"
#include "LAVFAudioHelper.h"
#include "LAVFUtils.h"
#include "moreuuids.h"

#include <vector>
#include <sstream>

CLAVFStreamInfo::CLAVFStreamInfo(AVFormatContext *avctx, AVStream *avstream, const char* containerFormat, HRESULT &hr)
  : CStreamInfo(), m_containerFormat(containerFormat)
{
  switch(avstream->codec->codec_type) {
  case AVMEDIA_TYPE_AUDIO:
    hr = CreateAudioMediaType(avctx, avstream);
    break;
  case AVMEDIA_TYPE_VIDEO:
    hr = CreateVideoMediaType(avctx, avstream);
    break;
  case AVMEDIA_TYPE_SUBTITLE:
    hr = CreateSubtitleMediaType(avctx, avstream);
    break;
  default:
    hr = E_FAIL;
    break;
  }

  codecInfo = lavf_get_stream_description(avstream);
}

CLAVFStreamInfo::~CLAVFStreamInfo()
{
}

STDMETHODIMP CLAVFStreamInfo::CreateAudioMediaType(AVFormatContext *avctx, AVStream *avstream)
{
  // Make sure DTS Express has valid settings
  if (avstream->codec->codec_id == AV_CODEC_ID_DTS && avstream->codec->codec_tag == 0xA2) {
    avstream->codec->channels = avstream->codec->channels ? avstream->codec->channels : 2;
    avstream->codec->sample_rate = avstream->codec->sample_rate ? avstream->codec->sample_rate : 48000;
  }

  if (avstream->codec->codec_tag == 0) {
    avstream->codec->codec_tag = av_codec_get_tag(mp_wav_taglists, avstream->codec->codec_id);
  }

  if (avstream->codec->channels == 0 || avstream->codec->sample_rate == 0) {
    if (avstream->codec->codec_id == AV_CODEC_ID_AAC && avstream->codec->bit_rate) {
      if (!avstream->codec->channels) avstream->codec->channels = 2;
      if (!avstream->codec->sample_rate) avstream->codec->sample_rate = 48000;
    } else
      return E_FAIL;
  }

  CMediaType mtype = g_AudioHelper.initAudioType(avstream->codec->codec_id, avstream->codec->codec_tag, m_containerFormat);

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
      mtype.pbFormat = (BYTE *)g_AudioHelper.CreateWFMTEX_RAW_PCM(avstream, &mtype.cbFormat, mtype.subtype, &mtype.lSampleSize);
    } else {
      WAVEFORMATEX *wvfmt = g_AudioHelper.CreateWVFMTEX(avstream, &mtype.cbFormat);

      if (avstream->codec->codec_tag == WAVE_FORMAT_EXTENSIBLE && avstream->codec->extradata_size >= 22) {
        // The WAVEFORMATEXTENSIBLE GUID is not recognized by the audio renderers
        // Set the actual subtype as GUID
        WAVEFORMATEXTENSIBLE *wvfmtex = (WAVEFORMATEXTENSIBLE *)wvfmt;
        mtype.subtype = wvfmtex->SubFormat;
      }
      mtype.pbFormat = (BYTE *)wvfmt;

      if (avstream->codec->codec_id == AV_CODEC_ID_FLAC) {
        // These are required to block accidental connection to ReClock
        wvfmt->nAvgBytesPerSec = (wvfmt->nSamplesPerSec * wvfmt->nChannels * wvfmt->wBitsPerSample) >> 3;
        wvfmt->nBlockAlign = 1;
        mtype.subtype = MEDIASUBTYPE_FLAC_FRAMED;
        mtypes.push_back(mtype);
        mtype.subtype = MEDIASUBTYPE_FLAC;
      } else if (avstream->codec->codec_id == AV_CODEC_ID_EAC3) {
        mtypes.push_back(mtype);
        mtype.subtype = MEDIASUBTYPE_DOLBY_DDPLUS_ARCSOFT;
      } else if (avstream->codec->codec_id == AV_CODEC_ID_DTS) {
        wvfmt->wFormatTag = WAVE_FORMAT_DTS2;
      } else if (avstream->codec->codec_id == AV_CODEC_ID_TRUEHD) {
        //wvfmt->wFormatTag = (WORD)WAVE_FORMAT_TRUEHD;
        mtypes.push_back(mtype);
        mtype.subtype = MEDIASUBTYPE_DOLBY_TRUEHD_ARCSOFT;
      } else if (avstream->codec->codec_id == AV_CODEC_ID_AAC) {
        wvfmt->wFormatTag = (WORD)MEDIASUBTYPE_AAC_ADTS.Data1;

        CMediaType adtsMtype = mtype;
        adtsMtype.subtype = MEDIASUBTYPE_AAC_ADTS;
        // Clear artificially generated extradata so we don't confuse LAV Audio
        if (m_containerFormat == "mpeg" || m_containerFormat == "mpegts") {
          adtsMtype.cbFormat = sizeof(WAVEFORMATEX);
          ((WAVEFORMATEX *)adtsMtype.pbFormat)->cbSize = 0;
        }
        mtypes.push_back(adtsMtype);

        mtype.subtype = MEDIASUBTYPE_AAC;
        wvfmt->wFormatTag = (WORD)mtype.subtype.Data1;
      } else if (avstream->codec->codec_id == AV_CODEC_ID_AAC_LATM) {
        mtypes.push_back(mtype);
        mtype.subtype = MEDIASUBTYPE_MPEG_LOAS;
        wvfmt->wFormatTag = (WORD)mtype.subtype.Data1;
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

STDMETHODIMP CLAVFStreamInfo::CreateVideoMediaType(AVFormatContext *avctx, AVStream *avstream)
{
  unsigned int origCodecTag = avstream->codec->codec_tag;
  if (avstream->codec->codec_tag == 0 && avstream->codec->codec_id != AV_CODEC_ID_DVVIDEO) {
    avstream->codec->codec_tag = av_codec_get_tag(mp_bmp_taglists, avstream->codec->codec_id);
  }

  if (avstream->codec->width == 0 || avstream->codec->height == 0
    || ((avstream->codec->codec_id == AV_CODEC_ID_MPEG1VIDEO || avstream->codec->codec_id == AV_CODEC_ID_MPEG2VIDEO) && (avstream->codec->time_base.den == 0 || avstream->codec->time_base.num == 0)))
    return E_FAIL;

  CMediaType mtype = g_VideoHelper.initVideoType(avstream->codec->codec_id, avstream->codec->codec_tag, m_containerFormat);

  mtype.SetTemporalCompression(TRUE);
  mtype.SetVariableSize();

  // Somewhat hackish to force VIH for AVI content.
  // TODO: Figure out why exactly this is required
  if (m_containerFormat == "avi" && avstream->codec->codec_id != AV_CODEC_ID_H264) {
    mtype.formattype = FORMAT_VideoInfo;
  }

  // If we need aspect info, we switch to VIH2
  AVRational r = avstream->sample_aspect_ratio;
  AVRational rc = avstream->codec->sample_aspect_ratio;
  if (mtype.formattype == FORMAT_VideoInfo && ((r.den > 0 && r.num > 0 && (r.den > 1 || r.num > 1)) || (rc.den > 0 && rc.num > 0 && (rc.den > 1 || rc.num > 1)))) {
    mtype.formattype = FORMAT_VideoInfo2;
  }

  if (mtype.formattype == FORMAT_VideoInfo) {
    mtype.pbFormat = (BYTE *)g_VideoHelper.CreateVIH(avstream, &mtype.cbFormat, m_containerFormat);
  } else if (mtype.formattype == FORMAT_VideoInfo2) {
    mtype.pbFormat = (BYTE *)g_VideoHelper.CreateVIH2(avstream, &mtype.cbFormat, m_containerFormat);
    if (mtype.subtype == MEDIASUBTYPE_WVC1) {
      // If we send the cyberlink subtype first, it'll work with it, and with ffdshow, dmo and mpc-hc internal
      VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)mtype.pbFormat;
      if (*((BYTE*)vih2 + sizeof(VIDEOINFOHEADER2)) == 0) {
        mtype.subtype = MEDIASUBTYPE_WVC1_CYBERLINK;
        mtypes.push_back(mtype);
        mtype.subtype = MEDIASUBTYPE_WVC1_ARCSOFT;
        mtypes.push_back(mtype);
      }
      mtype.subtype = MEDIASUBTYPE_WVC1;
    } else if (mtype.subtype == MEDIASUBTYPE_WMVA) {
      mtypes.push_back(mtype);

      mtype.subtype = MEDIASUBTYPE_WVC1;
      VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)mtype.pbFormat;
      vih2->bmiHeader.biCompression = mtype.subtype.Data1;
    }
  } else if (mtype.formattype == FORMAT_MPEGVideo) {
    mtype.pbFormat = (BYTE *)g_VideoHelper.CreateMPEG1VI(avstream, &mtype.cbFormat, m_containerFormat);
  } else if (mtype.formattype == FORMAT_MPEG2Video) {
    BOOL bConvertToAVC1 = (m_containerFormat == "mpegts");
    mtype.pbFormat = (BYTE *)g_VideoHelper.CreateMPEG2VI(avstream, &mtype.cbFormat, m_containerFormat, bConvertToAVC1);
    if (avstream->codec->codec_id == AV_CODEC_ID_H264 && !bConvertToAVC1 && (!avstream->codec->extradata_size || avstream->codec->extradata[0] != 1)) {
      mtype.subtype = MEDIASUBTYPE_H264;
      MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)mtype.pbFormat;
      mp2vi->hdr.bmiHeader.biCompression = mtype.subtype.Data1;
    }
  }

  if (avstream->codec->codec_id == AV_CODEC_ID_RAWVIDEO) {
    BITMAPINFOHEADER *pBMI = NULL;
    videoFormatTypeHandler(mtype.pbFormat, &mtype.formattype, &pBMI, NULL, NULL, NULL);
    mtype.bFixedSizeSamples = TRUE;
    mtype.bTemporalCompression = FALSE;
    mtype.lSampleSize = pBMI->biSizeImage;
    if (!avstream->codec->codec_tag || avstream->codec->codec_tag == MKTAG('r','a','w',' ')) {
      switch (avstream->codec->pix_fmt) {
      case AV_PIX_FMT_BGRA:
        mtype.subtype = MEDIASUBTYPE_ARGB32;
        mtypes.push_back(mtype);
        mtype.subtype = MEDIASUBTYPE_RGB32;
        break;
      case AV_PIX_FMT_BGR24:
        mtype.subtype = MEDIASUBTYPE_RGB24;
        break;
      default:
        DbgLog((LOG_TRACE, 10, L"::CreateVideoMediaType(): Unsupported raw video pixel format"));
      }
    } else {
      switch (avstream->codec->codec_tag) {
      case MKTAG('B','G','R','A'):
        {
          pBMI->biHeight = -pBMI->biHeight;
          mtype.subtype = MEDIASUBTYPE_ARGB32;
          mtypes.push_back(mtype);
          mtype.subtype = MEDIASUBTYPE_RGB32;
        }
        break;
      default:
        DbgLog((LOG_TRACE, 10, L"::CreateVideoMediaType(): Unsupported raw video codec tag format"));
      }
    }
    mtypes.push_back(mtype);
    mtype.subtype = MEDIASUBTYPE_LAV_RAWVIDEO;
    size_t hdrsize = 0, extrasize = 0;
    if (mtype.formattype == FORMAT_VideoInfo) {
      hdrsize = sizeof(VIDEOINFOHEADER);
    } else if (mtype.formattype == FORMAT_VideoInfo2) {
      hdrsize = sizeof(VIDEOINFOHEADER2);
    } else {
      ASSERT(0);
    }
    extrasize = mtype.cbFormat - hdrsize;
    mtype.ReallocFormatBuffer(hdrsize + extrasize + sizeof(avstream->codec->pix_fmt));
    if (extrasize) {
      memmove(mtype.pbFormat + hdrsize + sizeof(avstream->codec->pix_fmt), mtype.pbFormat + hdrsize, extrasize);
    }
    *(int *)(mtype.pbFormat + hdrsize) = avstream->codec->pix_fmt;
    videoFormatTypeHandler(mtype.pbFormat, &mtype.formattype, &pBMI, NULL, NULL, NULL);
    pBMI->biSize = sizeof(BITMAPINFOHEADER) + sizeof(avstream->codec->pix_fmt) + extrasize;
    mtypes.push_back(mtype);
  }

  if (avstream->codec->codec_id == AV_CODEC_ID_MJPEG) {
    BITMAPINFOHEADER *pBMI = NULL;
    videoFormatTypeHandler(mtype.pbFormat, &mtype.formattype, &pBMI, NULL, NULL, NULL);

    DWORD fourCC = MKTAG('M','J','P','G');

    // If the original fourcc is different to MJPG, add this one
    if (fourCC != pBMI->biCompression) {
      mtypes.push_back(mtype);

      mtype.subtype = FOURCCMap(fourCC);
      pBMI->biCompression = fourCC;
    }
  }

  mtypes.push_back(mtype);
  return S_OK;
}

#include "libavformat/isom.h"

static std::string CreateVOBSubHeaderFromMP4(int vidW, int vidH, MOVStreamContext *context, const BYTE *buffer, int buf_size)
{
  std::ostringstream header;
  if (buf_size >= 16*4) {
    int w = context && context->width ? context->width : vidW;
    int h = context && context->height ? context->height : vidH;

    header << "# VobSub index file, v7 (do not modify this line!)\n";
    header << "size: " << w << "x" << h << "\n";
    header << "palette: ";

    const BYTE *pal = buffer;
    char rgb[7];
    for(int i = 0; i < 16*4; i += 4) {
      BYTE y = (pal[i+1]-16)*255/219;
      BYTE u = pal[i+2];
      BYTE v = pal[i+3];
      BYTE r = (BYTE)min(max(1.0*y + 1.4022*(v-128), 0), 255);
      BYTE g = (BYTE)min(max(1.0*y - 0.3456*(u-128) - 0.7145*(v-128), 0), 255);
      BYTE b = (BYTE)min(max(1.0*y + 1.7710*(u-128), 0) , 255);
      sprintf_s(rgb, "%02x%02x%02x", r, g, b);
      if (i)
        header << ",";
      header << rgb;
    }
    header << "\n";
  }
  return header.str();
}

static std::string GetDefaultVOBSubHeader(int w, int h)
{
  std::ostringstream header;
  header << "# VobSub index file, v7 (do not modify this line!)\n";
  header << "size: " << w << "x" << h << "\n";
  header << "palette: ";
  //header << "000000,f0f0f0,cccccc,999999,";
  header << "000000,e0e0e0,808080,202020,";
  header << "3333fa,1111bb,fa3333,bb1111,";
  header << "33fa33,11bb11,fafa33,bbbb11,";
  header << "fa33fa,bb11bb,33fafa,11bbbb\n";
  return header.str();
}


STDMETHODIMP CLAVFStreamInfo::CreateSubtitleMediaType(AVFormatContext *avctx, AVStream *avstream)
{
  // Skip teletext
  if (avstream->codec->codec_id == AV_CODEC_ID_DVB_TELETEXT) {
    return E_FAIL;
  }
  CMediaType mtype;
  mtype.majortype = MEDIATYPE_Subtitle;
  mtype.formattype = FORMAT_SubtitleInfo;

  int extra = avstream->codec->extradata_size;
  if (avstream->codec->codec_id == AV_CODEC_ID_MOV_TEXT || avstream->codec->codec_id == AV_CODEC_ID_TEXT || avstream->codec->codec_id == AV_CODEC_ID_SUBRIP) {
    extra = 0;
  }

  // create format info
  SUBTITLEINFO *subInfo = (SUBTITLEINFO *)mtype.AllocFormatBuffer(sizeof(SUBTITLEINFO) + extra);
  memset(subInfo, 0, mtype.FormatLength());

  if (AVDictionaryEntry *dictEntry = av_dict_get(avstream->metadata, "language", NULL, 0))
  {
    char *lang = dictEntry->value;
    strncpy_s(subInfo->IsoLang, 4, lang, _TRUNCATE);
  } else {
    strncpy_s(subInfo->IsoLang, 4, "und", _TRUNCATE);
  }

  if (AVDictionaryEntry *dictEntry = av_dict_get(avstream->metadata, "title", NULL, 0))
  {
    // read metadata
    char *title = dictEntry->value;
    // convert to wchar
    MultiByteToWideChar(CP_UTF8, 0, title, -1, subInfo->TrackName, 256);
  }

  subInfo->dwOffset = sizeof(SUBTITLEINFO);

  // Find first video stream
  AVStream *vidStream = NULL;
  for (unsigned i = 0; i < avctx->nb_streams; i++) {
    if (avctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      vidStream = avctx->streams[i];
      break;
    }
  }

  // Extradata
  if (m_containerFormat == "mp4" && avstream->codec->codec_id == AV_CODEC_ID_DVD_SUBTITLE) {
    std::string strVobSubHeader = CreateVOBSubHeaderFromMP4(vidStream ? vidStream->codec->width : 720, vidStream ? vidStream->codec->height : 576, (MOVStreamContext *)avstream->priv_data, avstream->codec->extradata, extra);
    size_t len = strVobSubHeader.length();
    mtype.ReallocFormatBuffer((ULONG)(sizeof(SUBTITLEINFO) + len));
    memcpy(mtype.pbFormat + sizeof(SUBTITLEINFO), strVobSubHeader.c_str(), len);
  } else if (m_containerFormat == "mpeg" && avstream->codec->codec_id == AV_CODEC_ID_DVD_SUBTITLE) {
    // And a VobSub type
    std::string strVobSubHeader = GetDefaultVOBSubHeader(vidStream ? vidStream->codec->width : 720, vidStream ? vidStream->codec->height : 576);
    size_t len = strVobSubHeader.length();
    mtype.ReallocFormatBuffer((ULONG)(sizeof(SUBTITLEINFO) + len));
    memcpy(mtype.pbFormat + sizeof(SUBTITLEINFO), strVobSubHeader.c_str(), len);
    mtype.subtype = MEDIASUBTYPE_VOBSUB;
    mtypes.push_back(mtype);

    // Offer the DVD subtype
    CMediaType dvdmtype;
    dvdmtype.majortype = MEDIATYPE_Video;
    dvdmtype.subtype = MEDIASUBTYPE_DVD_SUBPICTURE;
    dvdmtype.formattype = FORMAT_MPEG2_VIDEO;
    MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)dvdmtype.AllocFormatBuffer(sizeof(MPEG2VIDEOINFO));
    ZeroMemory(mp2vi, sizeof(MPEG2VIDEOINFO));
    mp2vi->hdr.bmiHeader.biWidth = vidStream ? vidStream->codec->width : 720;
    mp2vi->hdr.bmiHeader.biHeight = vidStream ? vidStream->codec->height : 576;
    mtypes.push_back(dvdmtype);

    return S_OK;
  } else {
    memcpy(mtype.pbFormat + sizeof(SUBTITLEINFO), avstream->codec->extradata, extra);
  }

  mtype.subtype = avstream->codec->codec_id == AV_CODEC_ID_TEXT ? MEDIASUBTYPE_UTF8 :
                  avstream->codec->codec_id == AV_CODEC_ID_SRT ? MEDIASUBTYPE_UTF8 : /* SRT is essentially SUBRIP with inband timing information, parsing needed */
                  avstream->codec->codec_id == AV_CODEC_ID_SUBRIP ? MEDIASUBTYPE_UTF8 :
                  avstream->codec->codec_id == AV_CODEC_ID_MOV_TEXT ? MEDIASUBTYPE_UTF8 :
                  avstream->codec->codec_id == AV_CODEC_ID_SSA ? MEDIASUBTYPE_ASS :
                  avstream->codec->codec_id == AV_CODEC_ID_HDMV_PGS_SUBTITLE ? MEDIASUBTYPE_HDMVSUB :
                  avstream->codec->codec_id == AV_CODEC_ID_DVD_SUBTITLE ? MEDIASUBTYPE_VOBSUB :
                  avstream->codec->codec_id == AV_CODEC_ID_DVB_SUBTITLE ? MEDIASUBTYPE_DVB_SUBTITLES :
                  MEDIASUBTYPE_NULL;

  mtypes.push_back(mtype);
  return S_OK;
}
