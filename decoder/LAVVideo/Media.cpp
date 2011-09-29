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
#include "Media.h"

#include <MMReg.h>

#include "moreuuids.h"

typedef struct {
  const CLSID*        clsMinorType;
  const enum CodecID  nFFCodec;
} FFMPEG_SUBTYPE_MAP;

// Map Media Subtype <> FFMPEG Codec Id
static const FFMPEG_SUBTYPE_MAP lavc_video_codecs[] = {
  // H264
  { &MEDIASUBTYPE_H264, CODEC_ID_H264 },
  { &MEDIASUBTYPE_h264, CODEC_ID_H264 },
  { &MEDIASUBTYPE_X264, CODEC_ID_H264 },
  { &MEDIASUBTYPE_x264, CODEC_ID_H264 },
  { &MEDIASUBTYPE_AVC1, CODEC_ID_H264 },
  { &MEDIASUBTYPE_avc1, CODEC_ID_H264 },
  { &MEDIASUBTYPE_CCV1, CODEC_ID_H264 }, // Used by Haali Splitter

  // MPEG1/2
  { &MEDIASUBTYPE_MPEG1Payload, CODEC_ID_MPEG1VIDEO },
  { &MEDIASUBTYPE_MPEG1Video,   CODEC_ID_MPEG1VIDEO },
  { &MEDIASUBTYPE_MPEG2_VIDEO,  CODEC_ID_MPEG2VIDEO },

  // MJPEG
  { &MEDIASUBTYPE_MJPG,  CODEC_ID_MJPEG  },
  { &MEDIASUBTYPE_MJPGB, CODEC_ID_MJPEGB },

  // VC-1
  { &MEDIASUBTYPE_WVC1, CODEC_ID_VC1 },
  { &MEDIASUBTYPE_wvc1, CODEC_ID_VC1 },
  { &MEDIASUBTYPE_WMVA, CODEC_ID_VC1 },
  { &MEDIASUBTYPE_wmva, CODEC_ID_VC1 },

  // WMV
  { &MEDIASUBTYPE_WMV1, CODEC_ID_WMV1 },
  { &MEDIASUBTYPE_wmv1, CODEC_ID_WMV1 },
  { &MEDIASUBTYPE_WMV2, CODEC_ID_WMV2 },
  { &MEDIASUBTYPE_wmv2, CODEC_ID_WMV2 },
  { &MEDIASUBTYPE_WMV3, CODEC_ID_WMV3 },
  { &MEDIASUBTYPE_wmv3, CODEC_ID_WMV3 },

  // VP8
  { &MEDIASUBTYPE_VP80, CODEC_ID_VP8 },

  // MPEG4 ASP
  { &MEDIASUBTYPE_XVID, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_xvid, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_DIVX, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_divx, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_DX50, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_dx50, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_MP4V, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_mp4v, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_M4S2, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_m4s2, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_MP4S, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_mp4s, CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_FMP4, CODEC_ID_MPEG4 },

  // MS-MPEG4
  { &MEDIASUBTYPE_MPG4, CODEC_ID_MSMPEG4V1 },
  { &MEDIASUBTYPE_mpg4, CODEC_ID_MSMPEG4V1 },
  { &MEDIASUBTYPE_MP41, CODEC_ID_MSMPEG4V1 },
  { &MEDIASUBTYPE_mp41, CODEC_ID_MSMPEG4V1 },
  { &MEDIASUBTYPE_DIV1, CODEC_ID_MSMPEG4V1 },
  { &MEDIASUBTYPE_div1, CODEC_ID_MSMPEG4V1 },

  { &MEDIASUBTYPE_MP42, CODEC_ID_MSMPEG4V2 },
  { &MEDIASUBTYPE_mp42, CODEC_ID_MSMPEG4V2 },
  { &MEDIASUBTYPE_DIV2, CODEC_ID_MSMPEG4V2 },
  { &MEDIASUBTYPE_div2, CODEC_ID_MSMPEG4V2 },

  { &MEDIASUBTYPE_MP43, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_mp43, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_DIV3, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_div3, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_MPG3, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_mpg3, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_DIV4, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_div4, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_DIV5, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_div5, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_DIV6, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_div6, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_DVX3, CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_dvx3, CODEC_ID_MSMPEG4V3 },

  // Flash
  { &MEDIASUBTYPE_FLV1, CODEC_ID_FLV1 },
  { &MEDIASUBTYPE_flv1, CODEC_ID_FLV1 },
  { &MEDIASUBTYPE_VP60, CODEC_ID_VP6  },
  { &MEDIASUBTYPE_vp60, CODEC_ID_VP6  },
  { &MEDIASUBTYPE_VP61, CODEC_ID_VP6  },
  { &MEDIASUBTYPE_vp61, CODEC_ID_VP6  },
  { &MEDIASUBTYPE_VP62, CODEC_ID_VP6  },
  { &MEDIASUBTYPE_vp62, CODEC_ID_VP6  },
  { &MEDIASUBTYPE_VP6A, CODEC_ID_VP6A },
  { &MEDIASUBTYPE_vp6a, CODEC_ID_VP6A },
  { &MEDIASUBTYPE_VP6F, CODEC_ID_VP6F },
  { &MEDIASUBTYPE_vp6f, CODEC_ID_VP6F },
  { &MEDIASUBTYPE_FLV4, CODEC_ID_VP6F },
  { &MEDIASUBTYPE_flv4, CODEC_ID_VP6F },
  { &MEDIASUBTYPE_FSV1, CODEC_ID_FLASHSV },

  // Real
  { &MEDIASUBTYPE_RV10, CODEC_ID_RV10 },
  { &MEDIASUBTYPE_RV20, CODEC_ID_RV20 },
  { &MEDIASUBTYPE_RV30, CODEC_ID_RV30 },
  { &MEDIASUBTYPE_RV40, CODEC_ID_RV40 },

  // DV Video
  { &MEDIASUBTYPE_dvsd, CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DVSD, CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_CDVH, CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_dv25, CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DV25, CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_dv50, CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DV50, CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DVCP, CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DVC,  CODEC_ID_DVVIDEO },

  // Misc Formats
  { &MEDIASUBTYPE_SVQ1, CODEC_ID_SVQ1 },
  { &MEDIASUBTYPE_SVQ3, CODEC_ID_SVQ3 },
  { &MEDIASUBTYPE_H261, CODEC_ID_H261 },
  { &MEDIASUBTYPE_h261, CODEC_ID_H261 },
  { &MEDIASUBTYPE_H263, CODEC_ID_H263 },
  { &MEDIASUBTYPE_h263, CODEC_ID_H263 },
  { &MEDIASUBTYPE_S263, CODEC_ID_H263 },
  { &MEDIASUBTYPE_s263, CODEC_ID_H263 },
  { &MEDIASUBTYPE_THEORA, CODEC_ID_THEORA },
  { &MEDIASUBTYPE_theora, CODEC_ID_THEORA },
  { &MEDIASUBTYPE_TSCC, CODEC_ID_TSCC },
  { &MEDIASUBTYPE_IV50, CODEC_ID_INDEO5 },
  { &MEDIASUBTYPE_IV31, CODEC_ID_INDEO3 },
  { &MEDIASUBTYPE_IV32, CODEC_ID_INDEO3 },
  { &MEDIASUBTYPE_FPS1, CODEC_ID_FRAPS },
  { &MEDIASUBTYPE_HuffYUV,  CODEC_ID_HUFFYUV },
  { &MEDIASUBTYPE_Lagarith, CODEC_ID_LAGARITH },
  { &MEDIASUBTYPE_CVID,  CODEC_ID_CINEPAK },
  { &MEDIASUBTYPE_QTRle, CODEC_ID_QTRLE },
  { &MEDIASUBTYPE_VP30, CODEC_ID_VP3  },
  { &MEDIASUBTYPE_VP31, CODEC_ID_VP3  },
  { &MEDIASUBTYPE_CSCD, CODEC_ID_CSCD },
  { &MEDIASUBTYPE_QPEG, CODEC_ID_QPEG },
  { &MEDIASUBTYPE_QP10, CODEC_ID_QPEG },
  { &MEDIASUBTYPE_QP11, CODEC_ID_QPEG },
  { &MEDIASUBTYPE_MSZH, CODEC_ID_MSZH },
  { &MEDIASUBTYPE_ZLIB, CODEC_ID_ZLIB },
  { &MEDIASUBTYPE_QTRpza, CODEC_ID_RPZA },
  { &MEDIASUBTYPE_PNG, CODEC_ID_PNG   },
  { &MEDIASUBTYPE_PCM, CODEC_ID_MSRLE }, // Yeah, PCM. Its the same FourCC as used by MS-RLE
  { &MEDIASUBTYPE_apch, CODEC_ID_PRORES },
  { &MEDIASUBTYPE_apcn, CODEC_ID_PRORES },
  { &MEDIASUBTYPE_apcs, CODEC_ID_PRORES },
  { &MEDIASUBTYPE_apco, CODEC_ID_PRORES },
  { &MEDIASUBTYPE_ap4h, CODEC_ID_PRORES },

  // Game Formats
  { &MEDIASUBTYPE_BIKI, CODEC_ID_BINKVIDEO  },
  { &MEDIASUBTYPE_BIKB, CODEC_ID_BINKVIDEO  },
  { &MEDIASUBTYPE_SMK2, CODEC_ID_SMACKVIDEO },
  { &MEDIASUBTYPE_SMK4, CODEC_ID_SMACKVIDEO },
};

// Define Input Media Types
const AMOVIESETUP_MEDIATYPE CLAVVideo::sudPinTypesIn[] = {
  // H264
  { &MEDIATYPE_Video, &MEDIASUBTYPE_H264 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_h264 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_X264 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_x264 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_AVC1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_avc1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_CCV1 },

  // MPEG1/2
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MPEG1Payload },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MPEG1Video   },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MPEG2_VIDEO  },

  // MJPEG
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MJPG  },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MJPGB },

  // VC-1
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WVC1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wvc1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WMVA },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wmva },

  // WMV
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WMV1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wmv1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WMV2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wmv2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WMV3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wmv3 },

  // VP8
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP80 },

  // MPEG4 ASP
  { &MEDIATYPE_Video, &MEDIASUBTYPE_XVID },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_xvid },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DIVX },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_divx },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DX50 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_dx50 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MP4V },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mp4v },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_M4S2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_m4s2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MP4S },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mp4s },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_FMP4 },

  // MS-MPEG4
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MPG4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mpg4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MP41 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mp41 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DIV1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_div1 },

  { &MEDIATYPE_Video, &MEDIASUBTYPE_MP42 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mp42 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DIV2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_div2 },

  { &MEDIATYPE_Video, &MEDIASUBTYPE_MP43 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mp43 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DIV3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_div3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MPG3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mpg3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DIV4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_div4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DIV5 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_div5 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DIV6 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_div6 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVX3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_dvx3 },

  // Flash
  { &MEDIATYPE_Video, &MEDIASUBTYPE_FLV1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_flv1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP60 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_vp60 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP61 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_vp61 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP62 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_vp62 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP6A },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_vp6a },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP6F },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_vp6f },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_FLV4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_flv4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_FSV1 },

  // Real
  { &MEDIATYPE_Video, &MEDIASUBTYPE_RV10 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_RV20 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_RV30 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_RV40 },

  // DV Video
  { &MEDIATYPE_Video, &MEDIASUBTYPE_dvsd },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVSD },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_CDVH },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_dv25 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DV25 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_dv50 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DV50 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVCP },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVC  },

  // Misc Formats
  { &MEDIATYPE_Video, &MEDIASUBTYPE_SVQ1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_SVQ3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_H261 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_h261 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_H263 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_h263 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_S263 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_s263 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_THEORA },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_theora },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_TSCC },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_IV50 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_IV31 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_IV32 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_FPS1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_HuffYUV },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_Lagarith },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_CVID },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_QTRle },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP30 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP31 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_CSCD },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_QPEG },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_QP10 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_QP11 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MSZH },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_ZLIB },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_QTRpza },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_PNG  },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_PCM  }, // Yeah, PCM. Its the same FourCC as used by MS-RLE
  { &MEDIATYPE_Video, &MEDIASUBTYPE_apch },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_apcn },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_apcs },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_apco },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_ap4h },

  // Game Formats
  { &MEDIATYPE_Video, &MEDIASUBTYPE_BIKI },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_BIKB },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_SMK2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_SMK4 },
};
const int CLAVVideo::sudPinTypesInCount = countof(CLAVVideo::sudPinTypesIn);

// Define Output Media Types
const AMOVIESETUP_MEDIATYPE CLAVVideo::sudPinTypesOut[] = {
  { &MEDIATYPE_Video, &MEDIASUBTYPE_YV12 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_NV12 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_YUY2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_UYVY },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_RGB32 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_RGB24 },
};
const int CLAVVideo::sudPinTypesOutCount = countof(CLAVVideo::sudPinTypesOut);

// Crawl the lavc_video_codecs array for the proper codec
CodecID FindCodecId(const CMediaType *mt)
{
  for (int i=0; i<countof(lavc_video_codecs); ++i) {
    if (mt->subtype == *lavc_video_codecs[i].clsMinorType) {
      return lavc_video_codecs[i].nFFCodec;
    }
  }
  return CODEC_ID_NONE;
}

// Strings will be filled in eventually.
// CODEC_ID_NONE means there is some special handling going on.
// Order is Important, has to be the same as the CC Enum
// Also, the order is used for storage in the Registry
static codec_config_t m_codec_config[] = {
  { 1, { CODEC_ID_H264 }},                                                // Codec_H264
  { 1, { CODEC_ID_VC1 }},                                                 // Codec_VC1
  { 1, { CODEC_ID_MPEG1VIDEO }, L"mpeg1"},                                // Codec_MPEG1
  { 1, { CODEC_ID_MPEG2VIDEO }, L"mpeg2"},                                // Codec_MPEG2
  { 1, { CODEC_ID_MPEG4 }},                                               // Codec_MPEG4
  { 3, { CODEC_ID_MSMPEG4V1, CODEC_ID_MSMPEG4V2, CODEC_ID_MSMPEG4V3 }, L"msmpeg4", L"MS-MPEG-4 (DIVX3)" },   // Codec_MSMPEG4
  { 1, { CODEC_ID_VP8 }},                                                 // Codec_VP8
  { 1, { CODEC_ID_WMV3 }},                                                // Codec_WMV3
  { 2, { CODEC_ID_WMV1, CODEC_ID_WMV2 }, L"wmv12", L"Windows Media Video 7/8" },  // Codec_WMV12
  { 2, { CODEC_ID_MJPEG, CODEC_ID_MJPEGB }},                              // Codec_MJPEG
  { 2, { CODEC_ID_THEORA, CODEC_ID_VP3 }},                                // Codec_Theora
  { 2, { CODEC_ID_FLV1, CODEC_ID_FLASHSV }, L"flash", L"Flash Video (FLV1, FSV1)"}, // Codec_FLV1
  { 3, { CODEC_ID_VP6, CODEC_ID_VP6A, CODEC_ID_VP6F }},                   // Codec_VP6
  { 2, { CODEC_ID_SVQ1, CODEC_ID_SVQ3 }, L"svq", L"SVQ 1 / SVQ 3"},       // Codec_SVQ
  { 1, { CODEC_ID_H261 }},                                                // Codec_H261
  { 1, { CODEC_ID_H263 }},                                                // Codec_H263
  { 2, { CODEC_ID_INDEO3, CODEC_ID_INDEO5 }, L"indeo", L"Intel Indeo 3/5"}, // Codec_Indeo
  { 1, { CODEC_ID_TSCC }},                                                // Codec_TSCC
  { 1, { CODEC_ID_FRAPS }},                                               // Codec_Fraps
  { 1, { CODEC_ID_HUFFYUV }},                                             // Codec_HuffYUV
  { 1, { CODEC_ID_QTRLE }},                                               // Codec_QTRle
  { 1, { CODEC_ID_DVVIDEO }},                                             // Codec_DV
  { 1, { CODEC_ID_BINKVIDEO }, L"bink"},                                  // Codec_Bink
  { 1, { CODEC_ID_SMACKVIDEO }},                                          // Codec_Smacker
  { 2, { CODEC_ID_RV10, CODEC_ID_RV20 }, L"rv12", L"RealVideo 1/2" },     // Codev_RV12
  { 2, { CODEC_ID_RV30, CODEC_ID_RV40 }, L"rv34", L"RealVideo 3/4" },     // Codec_RV34
  { 1, { CODEC_ID_LAGARITH }},                                            // Codec_Lagarith
  { 1, { CODEC_ID_CINEPAK }},                                             // Codec_Cinepak
  { 1, { CODEC_ID_CSCD }},                                                // Codec_Camstudio
  { 1, { CODEC_ID_QPEG }},                                                // Codec_QPEG
  { 2, { CODEC_ID_ZLIB, CODEC_ID_MSZH }, L"zlib", L"ZLIB/MSZH lossless" },// Codec_ZLIB
  { 1, { CODEC_ID_RPZA }},                                                // Codec_QTRpza
  { 1, { CODEC_ID_PNG }},                                                 // Codec_PNG
  { 1, { CODEC_ID_MSRLE }},                                               // Codec_MSRLE
  { 1, { CODEC_ID_PRORES }, L"prores", L"ProRes" },                       // Codec_ProRes
};

const codec_config_t *get_codec_config(LAVVideoCodec codec)
{
  codec_config_t *config = &m_codec_config[codec];

  AVCodec *avcodec = avcodec_find_decoder(config->codecs[0]);
  if (avcodec) {
    if (!config->name) {
      size_t name_len = strlen(avcodec->name) + 1;
      wchar_t *name = (wchar_t *)calloc(name_len, sizeof(wchar_t));
      MultiByteToWideChar(CP_UTF8, 0, avcodec->name, -1, name, (int)name_len);

      config->name = name;
    }

    if (!config->description) {
      size_t desc_len = strlen(avcodec->long_name) + 1;
      wchar_t *desc = (wchar_t *)calloc(desc_len, sizeof(wchar_t));
      MultiByteToWideChar(CP_UTF8, 0, avcodec->long_name, -1, desc, (int)desc_len);

      config->description = desc;
    }
  }

  return &m_codec_config[codec];
}
