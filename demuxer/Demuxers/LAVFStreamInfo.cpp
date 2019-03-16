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
 *
 *  Contributions by Ti-BEN from the XBMC DSPlayer Project, also under GPLv2
 */

#include "stdafx.h"
#include "LAVFStreamInfo.h"
#include "LAVFVideoHelper.h"
#include "LAVFAudioHelper.h"
#include "LAVFUtils.h"
#include "moreuuids.h"
#include "H264Nalu.h"

#include <vector>
#include <sstream>

CLAVFStreamInfo::CLAVFStreamInfo(AVFormatContext *avctx, AVStream *avstream, const char* containerFormat, HRESULT &hr)
  : CStreamInfo(), m_containerFormat(containerFormat)
{
  switch(avstream->codecpar->codec_type) {
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
  // if no codec tag is set, or the tag is set to PCM (which is often wrong), lookup a new tag
  if (avstream->codecpar->codec_tag == 0 || avstream->codecpar->codec_tag == 1) {
    avstream->codecpar->codec_tag = av_codec_get_tag(mp_wav_taglists, avstream->codecpar->codec_id);
  }

  if (avstream->codecpar->channels == 0 || avstream->codecpar->sample_rate == 0) {
    if (avstream->codecpar->codec_id == AV_CODEC_ID_AAC && avstream->codecpar->bit_rate) {
      if (!avstream->codecpar->channels) avstream->codecpar->channels = 2;
      if (!avstream->codecpar->sample_rate) avstream->codecpar->sample_rate = 48000;
    } else
      return E_FAIL;
  }

  // use the variant bitrate for the stream if only one stream is available (applies to some radio streams)
  if (avstream->codecpar->bit_rate == 0 && avctx->nb_streams == 1) {
    AVDictionaryEntry *dict = av_dict_get(avstream->metadata, "variant_bitrate", nullptr, 0);
    if (dict && dict->value)
      avstream->codecpar->bit_rate = atoi(dict->value);
  }

  CMediaType mtype = g_AudioHelper.initAudioType(avstream->codecpar, avstream->codecpar->codec_tag, m_containerFormat);

  if(mtype.formattype == FORMAT_WaveFormatEx) {
    // Special Logic for the MPEG1 Audio Formats (MP1, MP2)
    if(mtype.subtype == MEDIASUBTYPE_MPEG1AudioPayload) {
      mtype.pbFormat = (BYTE *)g_AudioHelper.CreateMP1WVFMT(avstream, &mtype.cbFormat);
    } else if (mtype.subtype == MEDIASUBTYPE_BD_LPCM_AUDIO) {
      mtype.pbFormat = (BYTE *)g_AudioHelper.CreateWVFMTEX_LPCM(avstream, &mtype.cbFormat);
      mtypes.push_back(mtype);
      mtype.subtype = MEDIASUBTYPE_HDMV_LPCM_AUDIO;
    } else if ((mtype.subtype == MEDIASUBTYPE_PCM || mtype.subtype == MEDIASUBTYPE_IEEE_FLOAT) && avstream->codecpar->codec_tag != WAVE_FORMAT_EXTENSIBLE) {
      // Create raw PCM media type
      mtype.pbFormat = (BYTE *)g_AudioHelper.CreateWFMTEX_RAW_PCM(avstream, &mtype.cbFormat, mtype.subtype, &mtype.lSampleSize);
    } else {
      WAVEFORMATEX *wvfmt = g_AudioHelper.CreateWVFMTEX(avstream, &mtype.cbFormat);

      if (avstream->codecpar->codec_tag == WAVE_FORMAT_EXTENSIBLE && avstream->codecpar->extradata_size >= 22) {
        // The WAVEFORMATEXTENSIBLE GUID is not recognized by the audio renderers
        // Set the actual subtype as GUID
        WAVEFORMATEXTENSIBLE *wvfmtex = (WAVEFORMATEXTENSIBLE *)wvfmt;
        mtype.subtype = wvfmtex->SubFormat;
      }
      mtype.pbFormat = (BYTE *)wvfmt;

      if (avstream->codecpar->codec_id == AV_CODEC_ID_FLAC) {
        // These are required to block accidental connection to ReClock
        wvfmt->nAvgBytesPerSec = (wvfmt->nSamplesPerSec * wvfmt->nChannels * wvfmt->wBitsPerSample) >> 3;
        wvfmt->nBlockAlign = 1;
        mtype.subtype = MEDIASUBTYPE_FLAC_FRAMED;
        mtypes.push_back(mtype);
        mtype.subtype = MEDIASUBTYPE_FLAC;
      } else if (avstream->codecpar->codec_id == AV_CODEC_ID_EAC3) {
        mtypes.push_back(mtype);
        mtype.subtype = MEDIASUBTYPE_DOLBY_DDPLUS_ARCSOFT;
      } else if (avstream->codecpar->codec_id == AV_CODEC_ID_DTS) {
        wvfmt->wFormatTag = WAVE_FORMAT_DTS2;
        if (avstream->codecpar->profile >= FF_PROFILE_DTS_HD_HRA) {
          mtype.subtype = MEDIASUBTYPE_DTS_HD;
          mtypes.push_back(mtype);
        }
        mtype.subtype = MEDIASUBTYPE_WAVE_DTS;
      } else if (avstream->codecpar->codec_id == AV_CODEC_ID_TRUEHD) {
        //wvfmt->wFormatTag = (WORD)WAVE_FORMAT_TRUEHD;
        mtypes.push_back(mtype);
        mtype.subtype = MEDIASUBTYPE_DOLBY_TRUEHD_ARCSOFT;
      } else if (avstream->codecpar->codec_id == AV_CODEC_ID_AAC) {
        wvfmt->wFormatTag = (WORD)MEDIASUBTYPE_AAC_ADTS.Data1;

        CMediaType adtsMtype = mtype;
        adtsMtype.subtype = MEDIASUBTYPE_AAC_ADTS;
        mtypes.push_back(adtsMtype);

        // if we have extradata, use ordinary AAC type
        if (avstream->codecpar->extradata_size) {
          mtype.subtype = MEDIASUBTYPE_AAC;
        } else {
          // else, use ADTS type
          mtype.subtype = MEDIASUBTYPE_MPEG_ADTS_AAC;
        }
        wvfmt->wFormatTag = (WORD)mtype.subtype.Data1;
      } else if (avstream->codecpar->codec_id == AV_CODEC_ID_AAC_LATM) {
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

static bool h264_is_annexb(std::string format, AVStream *avstream)
{
  ASSERT(avstream->codecpar->codec_id == AV_CODEC_ID_H264 || avstream->codecpar->codec_id == AV_CODEC_ID_H264_MVC);
  if (avstream->codecpar->extradata_size < 4)
    return true;
  if (avstream->codecpar->extradata[0] == 1)
    return false;
  if (format == "avi") {
    BYTE *src = avstream->codecpar->extradata;
    unsigned startcode = AV_RB32(src);
    if (startcode == 0x00000001 || (startcode & 0xffffff00) == 0x00000100)
      return true;
    if (avstream->codecpar->codec_tag == MKTAG('A','V','C','1') || avstream->codecpar->codec_tag == MKTAG('a','v','c','1'))
      return false;
  }
  return true;
}

static bool hevc_is_annexb(std::string format, AVStream *avstream)
{
  ASSERT(avstream->codecpar->codec_id == AV_CODEC_ID_HEVC);
  if (avstream->codecpar->extradata_size < 23)
    return true;
  if (avstream->codecpar->extradata[0] || avstream->codecpar->extradata[1] || avstream->codecpar->extradata[2] > 1)
    return false;
  /*if (format == "avi") {
    BYTE *src = avstream->codec->extradata;
    unsigned startcode = AV_RB32(src);
    if (startcode == 0x00000001 || (startcode & 0xffffff00) == 0x00000100)
      return true;
  }*/
  return true;
}

static int get_vpcC_chroma(AVCodecParameters *codecpar)
{
  int chroma_w = 0, chroma_h = 0;
  if (av_pix_fmt_get_chroma_sub_sample((AVPixelFormat)codecpar->format, &chroma_w, &chroma_h) == 0) {
    if (chroma_w == 1 && chroma_h == 1) {
      return (codecpar->chroma_location == AVCHROMA_LOC_LEFT)
        ? 0
        : 1;
    }
    else if (chroma_w == 1 && chroma_h == 0) {
      return 2;
    }
    else if (chroma_w == 0 && chroma_h == 0) {
      return 3;
    }
  }
  return 0;
}

static int get_pixel_bitdepth(AVPixelFormat pix_fmt)
{
  const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(pix_fmt);
  if (desc == NULL) {
    DbgLog((LOG_TRACE, 10, "Unknown pixel format"));
    return 8;
  }
  return desc->comp[0].depth;
}

STDMETHODIMP CLAVFStreamInfo::CreateVideoMediaType(AVFormatContext *avctx, AVStream *avstream)
{
  unsigned int origCodecTag = avstream->codecpar->codec_tag;
  if (avstream->codecpar->codec_tag == 0 && avstream->codecpar->codec_id != AV_CODEC_ID_DVVIDEO) {
    avstream->codecpar->codec_tag = av_codec_get_tag(mp_bmp_taglists, avstream->codecpar->codec_id);
  }

  if (avstream->codecpar->codec_id == AV_CODEC_ID_H264_MVC) {
    // Don't create media types for MVC extension streams, they are handled specially
    return S_FALSE;
  }

  if (avstream->codecpar->width == 0 || avstream->codecpar->height == 0)
    return E_FAIL;

  CMediaType mtype = g_VideoHelper.initVideoType(avstream->codecpar->codec_id, avstream->codecpar->codec_tag, m_containerFormat);

  mtype.SetTemporalCompression(TRUE);
  mtype.SetVariableSize();

  // Somewhat hackish to force VIH for AVI content.
  // TODO: Figure out why exactly this is required
  if (m_containerFormat == "avi" && avstream->codecpar->codec_id != AV_CODEC_ID_H264) {
    mtype.formattype = FORMAT_VideoInfo;
  }

  // Native MPEG4 in Matroska needs a special formattype
  if (m_containerFormat == "matroska" && avstream->codecpar->codec_id == AV_CODEC_ID_MPEG4) {
    if (AVDictionaryEntry *mkvCodecId = av_dict_get(avstream->metadata, "mkv-codec-id", nullptr, 0)) {
      if (strcmp(mkvCodecId->value, "V_MS/VFW/FOURCC") != 0)
        mtype.formattype = FORMAT_MPEG2Video;
    }
  }

  // If we need aspect info, we switch to VIH2
  AVRational r = avstream->sample_aspect_ratio;
  AVRational rc = avstream->codecpar->sample_aspect_ratio;
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
    else if (mtype.subtype == MEDIASUBTYPE_VP90) {
      // generate extradata for VP9 streams
      mtype.ReallocFormatBuffer(sizeof(VIDEOINFOHEADER2) + 16);
      VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)mtype.pbFormat;
      vih2->bmiHeader.biSize = sizeof(BITMAPINFOHEADER) + 16;

      BYTE *extra = mtype.pbFormat + sizeof(VIDEOINFOHEADER2);
      AV_WB32(extra, MKBETAG('v', 'p', 'c', 'C'));
      AV_WB8 (extra +  4, 1); // version
      AV_WB24(extra +  5, 0); // flags
      AV_WB8 (extra +  8, avstream->codecpar->profile);
      AV_WB8 (extra +  9, avstream->codecpar->level == FF_LEVEL_UNKNOWN ? 0 : avstream->codecpar->level);
      AV_WB8 (extra + 10, get_pixel_bitdepth((AVPixelFormat)avstream->codecpar->format) << 4 | get_vpcC_chroma(avstream->codecpar) << 1 | (avstream->codecpar->color_range == AVCOL_RANGE_JPEG));
      AV_WB8 (extra + 11, avstream->codecpar->color_primaries);
      AV_WB8 (extra + 12, avstream->codecpar->color_trc);
      AV_WB8 (extra + 13, avstream->codecpar->color_space);
      AV_WB16(extra + 14, 0); // no codec init data
    }
  } else if (mtype.formattype == FORMAT_MPEGVideo) {
    mtype.pbFormat = (BYTE *)g_VideoHelper.CreateMPEG1VI(avstream, &mtype.cbFormat, m_containerFormat);
  } else if (mtype.formattype == FORMAT_MPEG2Video) {
    mtype.pbFormat = (BYTE *)g_VideoHelper.CreateMPEG2VI(avstream, &mtype.cbFormat, m_containerFormat, FALSE);
    MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)mtype.pbFormat;
    if (avstream->codecpar->codec_id == AV_CODEC_ID_H264) {
      if (h264_is_annexb(m_containerFormat, avstream)) {
        mtype.subtype = MEDIASUBTYPE_H264;
      } else {
        mtype.subtype = MEDIASUBTYPE_AVC1;
        if (mp2vi->dwFlags == 0)
          mp2vi->dwFlags = 4;
      }
      mp2vi->hdr.bmiHeader.biCompression = mtype.subtype.Data1;

      // Create a second AVC1 compat type for mpegts
      // This should only ever be used by "legacy" decoders
      if (mtype.subtype == MEDIASUBTYPE_H264 && m_containerFormat == "mpegts") {
        mtypes.push_back(mtype);
        mtype.ResetFormatBuffer();
        mtype.subtype = MEDIASUBTYPE_AVC1;
        mtype.pbFormat = (BYTE *)g_VideoHelper.CreateMPEG2VI(avstream, &mtype.cbFormat, m_containerFormat, TRUE);
        MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)mtype.pbFormat;
        mp2vi->hdr.bmiHeader.biCompression = mtype.subtype.Data1;
      }
    } else if (avstream->codecpar->codec_id == AV_CODEC_ID_HEVC) {
      if (hevc_is_annexb(m_containerFormat, avstream)) {
        mtype.subtype = MEDIASUBTYPE_HEVC;
      } else {
        mtype.subtype = MEDIASUBTYPE_HVC1;
      }
      mp2vi->hdr.bmiHeader.biCompression = mtype.subtype.Data1;
    }
  }

  // Detect MVC extensions and adjust the type appropriately
  if (avstream->codecpar->codec_id == AV_CODEC_ID_H264) {
    if (h264_is_annexb(m_containerFormat, avstream)) {
      if (m_containerFormat == "mpegts") {
        int nBaseStream = -1, nExtensionStream = -1;
        if (GetH264MVCStreamIndices(avctx, &nBaseStream, &nExtensionStream) && nBaseStream == avstream->index) {
          CMediaType mvcType = mtypes.front();
          AVStream *mvcStream = avctx->streams[nExtensionStream];

          MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)mvcType.ReallocFormatBuffer(sizeof(MPEG2VIDEOINFO) + avstream->codecpar->extradata_size + mvcStream->codecpar->extradata_size);

          size_t ExtradataSize = 0;

          // clear the extradata storage
          memset(&mp2vi->dwSequenceHeader[0], 0, avstream->codecpar->extradata_size + mvcStream->codecpar->extradata_size);

          // assemble the extradata in the correct order, first the SPS+SPS Subset, then any PPS
          CH264Nalu nalParser;

          // main SPS from the base stream
          nalParser.SetBuffer(avstream->codecpar->extradata, avstream->codecpar->extradata_size, 0);
          while (nalParser.ReadNext()) {
            if (nalParser.GetType() == NALU_TYPE_SPS) {
              memcpy((BYTE *)&mp2vi->dwSequenceHeader[0] + ExtradataSize, nalParser.GetNALBuffer(), nalParser.GetLength());
              ExtradataSize += nalParser.GetLength();
            }
          }

          // subset SPS from the extension stream
          nalParser.SetBuffer(mvcStream->codecpar->extradata, mvcStream->codecpar->extradata_size, 0);
          while (nalParser.ReadNext()) {
            if (nalParser.GetType() == NALU_TYPE_SPS || nalParser.GetType() == NALU_TYPE_SPS_SUB) {
              memcpy((BYTE *)&mp2vi->dwSequenceHeader[0] + ExtradataSize, nalParser.GetNALBuffer(), nalParser.GetLength());
              ExtradataSize += nalParser.GetLength();
            }

            // extract profile info from the subset SPS
            if (nalParser.GetType() == NALU_TYPE_SPS_SUB) {
              const BYTE *pData = nalParser.GetDataBuffer();
              mp2vi->dwProfile = avstream->codecpar->profile = pData[1];
              mp2vi->dwLevel = avstream->codecpar->level = pData[3];
            }
          }

          // PPS from the main stream
          nalParser.SetBuffer(avstream->codecpar->extradata, avstream->codecpar->extradata_size, 0);
          while (nalParser.ReadNext()) {
            if (nalParser.GetType() == NALU_TYPE_PPS) {
              memcpy((BYTE *)&mp2vi->dwSequenceHeader[0] + ExtradataSize, nalParser.GetNALBuffer(), nalParser.GetLength());
              ExtradataSize += nalParser.GetLength();
            }
          }

          // PPS from the extension stream
          nalParser.SetBuffer(mvcStream->codecpar->extradata, mvcStream->codecpar->extradata_size, 0);
          while (nalParser.ReadNext()) {
            if (nalParser.GetType() == NALU_TYPE_PPS) {
              memcpy((BYTE *)&mp2vi->dwSequenceHeader[0] + ExtradataSize, nalParser.GetNALBuffer(), nalParser.GetLength());
              ExtradataSize += nalParser.GetLength();
            }
          }

          // update mediatype
          mp2vi->cbSequenceHeader = (DWORD)ExtradataSize;
          mvcType.cbFormat = SIZE_MPEG2VIDEOINFO(mp2vi);
          mvcType.subtype = MEDIASUBTYPE_AMVC;
          mp2vi->hdr.bmiHeader.biCompression = mvcType.subtype.Data1;

          mtypes.push_front(mvcType);
        }
      }
    } else {
      CMediaType mvcType = mtype;

      MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)mvcType.ReallocFormatBuffer(sizeof(MPEG2VIDEOINFO) + avstream->codecpar->extradata_size);
      HRESULT hr = g_VideoHelper.ProcessH264MVCExtradata(avstream->codecpar->extradata, avstream->codecpar->extradata_size, mp2vi);
      if (hr == S_OK) {
        mvcType.cbFormat = SIZE_MPEG2VIDEOINFO(mp2vi);
        mvcType.subtype = MEDIASUBTYPE_MVC1;
        mp2vi->hdr.bmiHeader.biCompression = mvcType.subtype.Data1;
        mtypes.push_front(mvcType);

        // update stream profile/level appropriately
        if (mp2vi->dwProfile != 0)
          avstream->codecpar->profile = mp2vi->dwProfile;
        if (mp2vi->dwLevel != 0)
          avstream->codecpar->level = mp2vi->dwLevel;
      }
    }
  }

  if (avstream->codecpar->codec_id == AV_CODEC_ID_RAWVIDEO) {
    BITMAPINFOHEADER *pBMI = nullptr;
    videoFormatTypeHandler(mtype.pbFormat, &mtype.formattype, &pBMI, nullptr, nullptr, nullptr);
    mtype.bFixedSizeSamples = TRUE;
    mtype.bTemporalCompression = FALSE;
    mtype.lSampleSize = pBMI->biSizeImage;
    if (!avstream->codecpar->codec_tag || avstream->codecpar->codec_tag == MKTAG('r','a','w',' ')) {
      switch (avstream->codecpar->format) {
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
      switch (avstream->codecpar->codec_tag) {
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
    mtype.ReallocFormatBuffer((ULONG)(hdrsize + extrasize + sizeof(avstream->codecpar->format)));
    if (extrasize) {
      memmove(mtype.pbFormat + hdrsize + sizeof(avstream->codecpar->format), mtype.pbFormat + hdrsize, extrasize);
    }
    *(int *)(mtype.pbFormat + hdrsize) = avstream->codecpar->format;
    videoFormatTypeHandler(mtype.pbFormat, &mtype.formattype, &pBMI, nullptr, nullptr, nullptr);
    pBMI->biSize = (DWORD)(sizeof(BITMAPINFOHEADER) + sizeof(avstream->codecpar->format) + extrasize);
    mtypes.push_back(mtype);
  }

  if (avstream->codecpar->codec_id == AV_CODEC_ID_MJPEG) {
    BITMAPINFOHEADER *pBMI = nullptr;
    videoFormatTypeHandler(mtype.pbFormat, &mtype.formattype, &pBMI, nullptr, nullptr, nullptr);

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

enum AVCodecID ff_get_pcm_codec_id(int bps, int flt, int be, int sflags);
#include "libavformat/isom.h"

static std::string CreateVOBSubHeaderFromMP4(int vidW, int vidH, MOVStreamContext *context, const char *buffer, int buf_size)
{
  std::ostringstream header;
  if (buffer && buf_size) {
    int w = context && context->width ? context->width : vidW;
    int h = context && context->height ? context->height : vidH;

    header << "# VobSub index file, v7 (do not modify this line!)\n";
    // ffmpeg might provide us with the size already
    if (strncmp(buffer, "size:", 5) != 0) {
      header << "size: " << w << "x" << h << "\n";
    }
    header.write(buffer, buf_size);
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
  if (avstream->codecpar->codec_id == AV_CODEC_ID_DVB_TELETEXT) {
    return E_FAIL;
  }
  CMediaType mtype;
  mtype.majortype = MEDIATYPE_Subtitle;
  mtype.formattype = FORMAT_SubtitleInfo;

  int extra = avstream->codecpar->extradata_size;

  // parse flags from mov tx3g atom
  if (avstream->codecpar->codec_id == AV_CODEC_ID_MOV_TEXT && avstream->codecpar->codec_tag == MKTAG('t', 'x', '3', 'g') && extra >= 4)
  {
    uint32_t flags = AV_RB32(avstream->codecpar->extradata);
    if (flags & 0x80000000)
      avstream->disposition |= AV_DISPOSITION_FORCED;
  }

  if (avstream->codecpar->codec_id == AV_CODEC_ID_MOV_TEXT || avstream->codecpar->codec_id == AV_CODEC_ID_TEXT || avstream->codecpar->codec_id == AV_CODEC_ID_SUBRIP) {
    extra = 0;
  }

  // create format info
  SUBTITLEINFO *subInfo = (SUBTITLEINFO *)mtype.AllocFormatBuffer(sizeof(SUBTITLEINFO) + extra);
  memset(subInfo, 0, mtype.FormatLength());

  if (AVDictionaryEntry *dictEntry = av_dict_get(avstream->metadata, "language", nullptr, 0))
  {
    char *lang = dictEntry->value;
    strncpy_s(subInfo->IsoLang, 4, lang, _TRUNCATE);
  } else {
    strncpy_s(subInfo->IsoLang, 4, "und", _TRUNCATE);
  }

  if (AVDictionaryEntry *dictEntry = av_dict_get(avstream->metadata, "title", nullptr, 0))
  {
    // read metadata
    char *title = dictEntry->value;
    // convert to wchar
    SafeMultiByteToWideChar(CP_UTF8, 0, title, -1, subInfo->TrackName, 256);
  }

  subInfo->dwOffset = sizeof(SUBTITLEINFO);

  // Find first video stream
  AVStream *vidStream = nullptr;
  for (unsigned i = 0; i < avctx->nb_streams; i++) {
    if (avctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      vidStream = avctx->streams[i];
      break;
    }
  }

  // Extradata
  if (m_containerFormat == "mp4" && avstream->codecpar->codec_id == AV_CODEC_ID_DVD_SUBTITLE) {
    std::string strVobSubHeader = CreateVOBSubHeaderFromMP4(vidStream ? vidStream->codecpar->width : 720, vidStream ? vidStream->codecpar->height : 576, (MOVStreamContext *)avstream->priv_data, (char*)avstream->codecpar->extradata, extra);
    size_t len = strVobSubHeader.length();
    mtype.ReallocFormatBuffer((ULONG)(sizeof(SUBTITLEINFO) + len));
    memcpy(mtype.pbFormat + sizeof(SUBTITLEINFO), strVobSubHeader.c_str(), len);
  } else if (m_containerFormat == "mpeg" && avstream->codecpar->codec_id == AV_CODEC_ID_DVD_SUBTITLE) {
    // And a VobSub type
    std::string strVobSubHeader = GetDefaultVOBSubHeader(vidStream ? vidStream->codecpar->width : 720, vidStream ? vidStream->codecpar->height : 576);
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
    mp2vi->hdr.bmiHeader.biWidth = vidStream ? vidStream->codecpar->width : 720;
    mp2vi->hdr.bmiHeader.biHeight = vidStream ? vidStream->codecpar->height : 576;
    mtypes.push_back(dvdmtype);

    return S_OK;
  } else {
    memcpy(mtype.pbFormat + sizeof(SUBTITLEINFO), avstream->codecpar->extradata, extra);
  }

  mtype.subtype = avstream->codecpar->codec_id == AV_CODEC_ID_TEXT ? MEDIASUBTYPE_UTF8 :
                  avstream->codecpar->codec_id == AV_CODEC_ID_SRT ? MEDIASUBTYPE_UTF8 : /* SRT is essentially SUBRIP with inband timing information, parsing needed */
                  avstream->codecpar->codec_id == AV_CODEC_ID_SUBRIP ? MEDIASUBTYPE_UTF8 :
                  avstream->codecpar->codec_id == AV_CODEC_ID_MOV_TEXT ? MEDIASUBTYPE_UTF8 :
                  avstream->codecpar->codec_id == AV_CODEC_ID_ASS ? MEDIASUBTYPE_ASS :
                  avstream->codecpar->codec_id == AV_CODEC_ID_SSA ? MEDIASUBTYPE_ASS :
                  avstream->codecpar->codec_id == AV_CODEC_ID_HDMV_PGS_SUBTITLE ? MEDIASUBTYPE_HDMVSUB :
                  avstream->codecpar->codec_id == AV_CODEC_ID_DVD_SUBTITLE ? MEDIASUBTYPE_VOBSUB :
                  avstream->codecpar->codec_id == AV_CODEC_ID_DVB_SUBTITLE ? MEDIASUBTYPE_DVB_SUBTITLES :
                  MEDIASUBTYPE_NULL;

  mtypes.push_back(mtype);
  return S_OK;
}
