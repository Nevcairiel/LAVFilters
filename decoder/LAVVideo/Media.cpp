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
#include "Media.h"

#include <MMReg.h>
#include <Mfidl.h>

#include "moreuuids.h"

typedef struct {
  const CLSID*        clsMinorType;
  const enum AVCodecID  nFFCodec;
} FFMPEG_SUBTYPE_MAP;

// Map Media Subtype <> FFMPEG Codec Id
static const FFMPEG_SUBTYPE_MAP lavc_video_codecs[] = {
  // H264
  { &MEDIASUBTYPE_H264, AV_CODEC_ID_H264 },
  { &MEDIASUBTYPE_h264, AV_CODEC_ID_H264 },
  { &MEDIASUBTYPE_X264, AV_CODEC_ID_H264 },
  { &MEDIASUBTYPE_x264, AV_CODEC_ID_H264 },
  { &MEDIASUBTYPE_AVC1, AV_CODEC_ID_H264 },
  { &MEDIASUBTYPE_avc1, AV_CODEC_ID_H264 },
  { &MEDIASUBTYPE_CCV1, AV_CODEC_ID_H264 }, // Used by Haali Splitter
  { &MEDIASUBTYPE_H264_bis, AV_CODEC_ID_H264}, // MainConcept specific

  { &MEDIASUBTYPE_AMVC, AV_CODEC_ID_H264_MVC },
  { &MEDIASUBTYPE_MVC1, AV_CODEC_ID_H264_MVC },

  // HEVC
  { &MEDIASUBTYPE_HEVC, AV_CODEC_ID_HEVC },
  { &MEDIASUBTYPE_HVC1, AV_CODEC_ID_HEVC },
  { &MEDIASUBTYPE_HM10, AV_CODEC_ID_HEVC },

  // MPEG1/2
  { &MEDIASUBTYPE_MPEG1Payload, AV_CODEC_ID_MPEG1VIDEO },
  { &MEDIASUBTYPE_MPEG1Video,   AV_CODEC_ID_MPEG1VIDEO },
  { &MEDIASUBTYPE_MPEG2_VIDEO,  AV_CODEC_ID_MPEG2VIDEO },

  // MJPEG
  { &MEDIASUBTYPE_MJPG,   AV_CODEC_ID_MJPEG  },
  { &MEDIASUBTYPE_QTJpeg, AV_CODEC_ID_MJPEG  },
  { &MEDIASUBTYPE_MJPGB,  AV_CODEC_ID_MJPEGB },

  // VC-1
  { &MEDIASUBTYPE_WVC1, AV_CODEC_ID_VC1 },
  { &MEDIASUBTYPE_wvc1, AV_CODEC_ID_VC1 },
  { &MEDIASUBTYPE_WMVA, AV_CODEC_ID_VC1 },
  { &MEDIASUBTYPE_wmva, AV_CODEC_ID_VC1 },
  { &MEDIASUBTYPE_WVP2, AV_CODEC_ID_VC1IMAGE },
  { &MEDIASUBTYPE_wvp2, AV_CODEC_ID_VC1IMAGE },

  // WMV
  { &MEDIASUBTYPE_WMV1, AV_CODEC_ID_WMV1 },
  { &MEDIASUBTYPE_wmv1, AV_CODEC_ID_WMV1 },
  { &MEDIASUBTYPE_WMV2, AV_CODEC_ID_WMV2 },
  { &MEDIASUBTYPE_wmv2, AV_CODEC_ID_WMV2 },
  { &MEDIASUBTYPE_WMV3, AV_CODEC_ID_WMV3 },
  { &MEDIASUBTYPE_wmv3, AV_CODEC_ID_WMV3 },
  { &MEDIASUBTYPE_WMVP, AV_CODEC_ID_WMV3IMAGE },
  { &MEDIASUBTYPE_wmvp, AV_CODEC_ID_WMV3IMAGE },

  // VP7/8/9
  { &MEDIASUBTYPE_VP70, AV_CODEC_ID_VP7 },
  { &MEDIASUBTYPE_VP80, AV_CODEC_ID_VP8 },
  { &MEDIASUBTYPE_VP90, AV_CODEC_ID_VP9 },

  // AV1
  { &MEDIASUBTYPE_AV01, AV_CODEC_ID_AV1 },

  // MPEG4 ASP
  { &MEDIASUBTYPE_XVID, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_xvid, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_DIVX, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_divx, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_Divx, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_DX50, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_dx50, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_MP4V, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_mp4v, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_M4S2, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_m4s2, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_MP4S, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_mp4s, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_FMP4, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_3IVX, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_3ivx, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_3IV1, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_3iv1, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_3IV2, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_3iv2, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_BLZ0, AV_CODEC_ID_MPEG4 },
  { &MEDIASUBTYPE_GEOV, AV_CODEC_ID_MPEG4 },

  // MS-MPEG4
  { &MEDIASUBTYPE_MPG4, AV_CODEC_ID_MSMPEG4V1 },
  { &MEDIASUBTYPE_mpg4, AV_CODEC_ID_MSMPEG4V1 },
  { &MEDIASUBTYPE_MP41, AV_CODEC_ID_MSMPEG4V1 },
  { &MEDIASUBTYPE_mp41, AV_CODEC_ID_MSMPEG4V1 },
  { &MEDIASUBTYPE_DIV1, AV_CODEC_ID_MSMPEG4V1 },
  { &MEDIASUBTYPE_div1, AV_CODEC_ID_MSMPEG4V1 },

  { &MEDIASUBTYPE_MP42, AV_CODEC_ID_MSMPEG4V2 },
  { &MEDIASUBTYPE_mp42, AV_CODEC_ID_MSMPEG4V2 },
  { &MEDIASUBTYPE_DIV2, AV_CODEC_ID_MSMPEG4V2 },
  { &MEDIASUBTYPE_div2, AV_CODEC_ID_MSMPEG4V2 },

  { &MEDIASUBTYPE_MP43, AV_CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_mp43, AV_CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_DIV3, AV_CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_div3, AV_CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_MPG3, AV_CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_mpg3, AV_CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_DIV4, AV_CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_div4, AV_CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_DIV5, AV_CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_div5, AV_CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_DIV6, AV_CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_div6, AV_CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_DVX3, AV_CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_dvx3, AV_CODEC_ID_MSMPEG4V3 },
  { &MEDIASUBTYPE_3IVD, AV_CODEC_ID_MSMPEG4V3 },

  // Flash
  { &MEDIASUBTYPE_FLV1, AV_CODEC_ID_FLV1 },
  { &MEDIASUBTYPE_flv1, AV_CODEC_ID_FLV1 },
  { &MEDIASUBTYPE_VP60, AV_CODEC_ID_VP6  },
  { &MEDIASUBTYPE_vp60, AV_CODEC_ID_VP6  },
  { &MEDIASUBTYPE_VP61, AV_CODEC_ID_VP6  },
  { &MEDIASUBTYPE_vp61, AV_CODEC_ID_VP6  },
  { &MEDIASUBTYPE_VP62, AV_CODEC_ID_VP6  },
  { &MEDIASUBTYPE_vp62, AV_CODEC_ID_VP6  },
  { &MEDIASUBTYPE_VP6A, AV_CODEC_ID_VP6A },
  { &MEDIASUBTYPE_vp6a, AV_CODEC_ID_VP6A },
  { &MEDIASUBTYPE_VP6F, AV_CODEC_ID_VP6F },
  { &MEDIASUBTYPE_vp6f, AV_CODEC_ID_VP6F },
  { &MEDIASUBTYPE_FLV4, AV_CODEC_ID_VP6F },
  { &MEDIASUBTYPE_flv4, AV_CODEC_ID_VP6F },
  { &MEDIASUBTYPE_FSV1, AV_CODEC_ID_FLASHSV },

  // Real
  { &MEDIASUBTYPE_RV10, AV_CODEC_ID_RV10 },
  { &MEDIASUBTYPE_RV20, AV_CODEC_ID_RV20 },
  { &MEDIASUBTYPE_RV30, AV_CODEC_ID_RV30 },
  { &MEDIASUBTYPE_RV40, AV_CODEC_ID_RV40 },

  // DV Video
  { &MEDIASUBTYPE_dvsd, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DVSD, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_CDVH, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_CDVC, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_CDV5, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_dv25, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DV25, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_dv50, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DV50, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DVCP, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DV5P, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DV5N, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DVPP, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DVC,  AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DVH1, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DVH2, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DVH3, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DVH4, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DVH5, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DVH6, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DVHQ, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_DVHP, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_AVdv, AV_CODEC_ID_DVVIDEO },
  { &MEDIASUBTYPE_AVd1, AV_CODEC_ID_DVVIDEO },

  // JPEG 2000
  { &MEDIASUBTYPE_mjp2, AV_CODEC_ID_JPEG2000 },
  { &MEDIASUBTYPE_MJ2C, AV_CODEC_ID_JPEG2000 },
  { &MEDIASUBTYPE_LJ2C, AV_CODEC_ID_JPEG2000 },
  { &MEDIASUBTYPE_LJ2K, AV_CODEC_ID_JPEG2000 },
  { &MEDIASUBTYPE_IPJ2, AV_CODEC_ID_JPEG2000 },

  // Misc Formats
  { &MEDIASUBTYPE_SVQ1, AV_CODEC_ID_SVQ1 },
  { &MEDIASUBTYPE_SVQ3, AV_CODEC_ID_SVQ3 },
  { &MEDIASUBTYPE_H261, AV_CODEC_ID_H261 },
  { &MEDIASUBTYPE_h261, AV_CODEC_ID_H261 },
  { &MEDIASUBTYPE_H263, AV_CODEC_ID_H263 },
  { &MEDIASUBTYPE_h263, AV_CODEC_ID_H263 },
  { &MEDIASUBTYPE_S263, AV_CODEC_ID_H263 },
  { &MEDIASUBTYPE_s263, AV_CODEC_ID_H263 },
  { &MEDIASUBTYPE_I263, AV_CODEC_ID_H263I },
  { &MEDIASUBTYPE_i263, AV_CODEC_ID_H263I },
  { &MEDIASUBTYPE_THEORA, AV_CODEC_ID_THEORA },
  { &MEDIASUBTYPE_theora, AV_CODEC_ID_THEORA },
  { &MEDIASUBTYPE_TSCC, AV_CODEC_ID_TSCC },
  { &MEDIASUBTYPE_TSC2, AV_CODEC_ID_TSCC2 },
  { &MEDIASUBTYPE_IV50, AV_CODEC_ID_INDEO5 },
  { &MEDIASUBTYPE_IV41, AV_CODEC_ID_INDEO4 },
  { &MEDIASUBTYPE_IV31, AV_CODEC_ID_INDEO3 },
  { &MEDIASUBTYPE_IV32, AV_CODEC_ID_INDEO3 },
  { &MEDIASUBTYPE_FPS1, AV_CODEC_ID_FRAPS },
  { &MEDIASUBTYPE_HuffYUV,  AV_CODEC_ID_HUFFYUV },
  { &MEDIASUBTYPE_Lagarith, AV_CODEC_ID_LAGARITH },
  { &MEDIASUBTYPE_CVID,  AV_CODEC_ID_CINEPAK },
  { &MEDIASUBTYPE_QTRle, AV_CODEC_ID_QTRLE },
  { &MEDIASUBTYPE_VP30, AV_CODEC_ID_VP3  },
  { &MEDIASUBTYPE_VP31, AV_CODEC_ID_VP3  },
  { &MEDIASUBTYPE_CSCD, AV_CODEC_ID_CSCD },
  { &MEDIASUBTYPE_QPEG, AV_CODEC_ID_QPEG },
  { &MEDIASUBTYPE_QP10, AV_CODEC_ID_QPEG },
  { &MEDIASUBTYPE_QP11, AV_CODEC_ID_QPEG },
  { &MEDIASUBTYPE_MSZH, AV_CODEC_ID_MSZH },
  { &MEDIASUBTYPE_ZLIB, AV_CODEC_ID_ZLIB },
  { &MEDIASUBTYPE_QTRpza, AV_CODEC_ID_RPZA },
  { &MEDIASUBTYPE_PCM, AV_CODEC_ID_MSRLE }, // Yeah, PCM. Its the same FourCC as used by MS-RLE
  { &MEDIASUBTYPE_apch, AV_CODEC_ID_PRORES },
  { &MEDIASUBTYPE_apcn, AV_CODEC_ID_PRORES },
  { &MEDIASUBTYPE_apcs, AV_CODEC_ID_PRORES },
  { &MEDIASUBTYPE_apco, AV_CODEC_ID_PRORES },
  { &MEDIASUBTYPE_ap4h, AV_CODEC_ID_PRORES },
  { &MEDIASUBTYPE_ULRA, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_ULRG, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_ULY0, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_ULY2, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_ULY4, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_UQY2, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_UQRG, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_UQRA, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_ULH0, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_ULH2, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_ULH4, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_UMY2, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_UMH2, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_UMY4, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_UMH4, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_UMRG, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_UMRA, AV_CODEC_ID_UTVIDEO },
  { &MEDIASUBTYPE_AMVV, AV_CODEC_ID_AMV     },
  { &MEDIASUBTYPE_AMVF, AV_CODEC_ID_AMV     },
  { &MEDIASUBTYPE_DiracVideo, AV_CODEC_ID_DIRAC },
  { &MEDIASUBTYPE_DRAC, AV_CODEC_ID_DIRAC },
  { &MEDIASUBTYPE_AVdn, AV_CODEC_ID_DNXHD },
  { &MEDIASUBTYPE_AVdh, AV_CODEC_ID_DNXHD },
  { &MEDIASUBTYPE_CRAM, AV_CODEC_ID_MSVIDEO1 },
  { &MEDIASUBTYPE_MSVC, AV_CODEC_ID_MSVIDEO1 },
  { &MEDIASUBTYPE_WHAM, AV_CODEC_ID_MSVIDEO1 },
  { &MEDIASUBTYPE_8BPS, AV_CODEC_ID_8BPS  },
  { &MEDIASUBTYPE_LOCO, AV_CODEC_ID_LOCO  },
  { &MEDIASUBTYPE_ZMBV, AV_CODEC_ID_ZMBV  },
  { &MEDIASUBTYPE_VCR1, AV_CODEC_ID_VCR1 },
  { &MEDIASUBTYPE_AASC, AV_CODEC_ID_AASC },
  { &MEDIASUBTYPE_SNOW, AV_CODEC_ID_SNOW },
  { &MEDIASUBTYPE_FFV1, AV_CODEC_ID_FFV1 },
  { &MEDIASUBTYPE_FFVH, AV_CODEC_ID_FFVHUFF },
  { &MEDIASUBTYPE_VMNC, AV_CODEC_ID_VMNC },
  { &MEDIASUBTYPE_FLIC, AV_CODEC_ID_FLIC },
  //{ &MEDIASUBTYPE_G2M2, AV_CODEC_ID_G2M },
  //{ &MEDIASUBTYPE_G2M3, AV_CODEC_ID_G2M },
  { &MEDIASUBTYPE_G2M4, AV_CODEC_ID_G2M },
  { &MEDIASUBTYPE_icod, AV_CODEC_ID_AIC },
  { &MEDIASUBTYPE_DUCK, AV_CODEC_ID_TRUEMOTION1 },
  { &MEDIASUBTYPE_TM20, AV_CODEC_ID_TRUEMOTION2 },
  { &MEDIASUBTYPE_CFHD, AV_CODEC_ID_CFHD },
  { &MEDIASUBTYPE_MAGY, AV_CODEC_ID_MAGICYUV },

  // Game Formats
  { &MEDIASUBTYPE_BIKI, AV_CODEC_ID_BINKVIDEO  },
  { &MEDIASUBTYPE_BIKB, AV_CODEC_ID_BINKVIDEO  },
  { &MEDIASUBTYPE_SMK2, AV_CODEC_ID_SMACKVIDEO },
  { &MEDIASUBTYPE_SMK4, AV_CODEC_ID_SMACKVIDEO },
  { &MEDIASUBTYPE_THPV, AV_CODEC_ID_THP },
  { &MEDIASUBTYPE_ROQV, AV_CODEC_ID_ROQ },

  // Image Formats
  { &MEDIASUBTYPE_PNG,  AV_CODEC_ID_PNG   },
  { &MEDIASUBTYPE_TIFF, AV_CODEC_ID_TIFF  },
  { &MEDIASUBTYPE_BMP,  AV_CODEC_ID_BMP   },
  { &MEDIASUBTYPE_GIF,  AV_CODEC_ID_GIF   },
  { &MEDIASUBTYPE_TGA,  AV_CODEC_ID_TARGA },

  // Special raw formats
  { &MEDIASUBTYPE_v210, AV_CODEC_ID_V210 },
  { &MEDIASUBTYPE_v410, AV_CODEC_ID_V410 },
  { &MEDIASUBTYPE_LAV_RAWVIDEO, AV_CODEC_ID_RAWVIDEO },
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
  { &MEDIATYPE_Video, &MEDIASUBTYPE_H264_bis },

  { &MEDIATYPE_Video, &MEDIASUBTYPE_AMVC },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MVC1 },

  // HEVC
  { &MEDIATYPE_Video, &MEDIASUBTYPE_HEVC },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_HVC1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_HM10 },

  // MPEG1/2
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MPEG1Payload },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MPEG1Video   },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MPEG2_VIDEO  },

  { &MEDIATYPE_DVD_ENCRYPTED_PACK, &MEDIASUBTYPE_MPEG2_VIDEO },
  { &MEDIATYPE_MPEG2_PACK,         &MEDIASUBTYPE_MPEG2_VIDEO },
  { &MEDIATYPE_MPEG2_PES,          &MEDIASUBTYPE_MPEG2_VIDEO },

  // MJPEG
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MJPG   },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_QTJpeg },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MJPGB  },

  // VC-1
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WVC1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wvc1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WMVA },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wmva },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WVP2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wvp2 },

  // WMV
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WMV1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wmv1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WMV2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wmv2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WMV3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wmv3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WMVP },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_wmvp },

  // VP7/8/9
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP70 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP80 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VP90 },

  // AV1
  { &MEDIATYPE_Video, &MEDIASUBTYPE_AV01 },

  // MPEG4 ASP
  { &MEDIATYPE_Video, &MEDIASUBTYPE_XVID },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_xvid },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DIVX },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_divx },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_Divx },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DX50 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_dx50 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MP4V },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mp4v },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_M4S2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_m4s2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MP4S },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mp4s },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_FMP4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_3IVX },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_3ivx },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_3IV1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_3iv1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_3IV2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_3iv2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_BLZ0 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_GEOV },

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
  { &MEDIATYPE_Video, &MEDIASUBTYPE_3IVD },

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
  { &MEDIATYPE_Video, &MEDIASUBTYPE_CDVC },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_CDV5 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_dv25 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DV25 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_dv50 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DV50 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVCP },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DV5P },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DV5N },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVPP },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVC  },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVH1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVH2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVH3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVH4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVH5 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVH6 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVHQ },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DVHP },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_AVdv },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_AVd1 },

  // JPEG 2000
  { &MEDIATYPE_Video, &MEDIASUBTYPE_mjp2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MJ2C },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_LJ2C },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_LJ2K },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_IPJ2 },

  // Misc Formats
  { &MEDIATYPE_Video, &MEDIASUBTYPE_SVQ1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_SVQ3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_H261 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_h261 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_H263 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_h263 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_S263 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_s263 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_I263 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_i263 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_THEORA },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_theora },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_TSCC },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_TSC2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_IV50 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_IV41 },
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
  { &MEDIATYPE_Video, &MEDIASUBTYPE_PCM  }, // Yeah, PCM. Its the same FourCC as used by MS-RLE
  { &MEDIATYPE_Video, &MEDIASUBTYPE_apch },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_apcn },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_apcs },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_apco },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_ap4h },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_ULRA },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_ULRG },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_ULY0 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_ULY2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_ULY4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_UQY2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_UQRG },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_UQRA },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_ULH0 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_ULH2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_ULH4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_UMY2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_UMH2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_UMY4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_UMH4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_UMRG },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_UMRA },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_AMVV },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_AMVF },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DiracVideo },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DRAC },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_AVdn },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_AVdh },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_CRAM },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MSVC },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_WHAM },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_8BPS },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_LOCO },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_ZMBV },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VCR1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_AASC },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_SNOW },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_FFV1 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_FFVH },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_VMNC },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_FLIC },
  //{ &MEDIATYPE_Video, &MEDIASUBTYPE_G2M2 },
  //{ &MEDIATYPE_Video, &MEDIASUBTYPE_G2M3 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_G2M4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_icod },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_DUCK },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_TM20 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_CFHD },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_MAGY },

  // Game Formats
  { &MEDIATYPE_Video, &MEDIASUBTYPE_BIKI },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_BIKB },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_SMK2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_SMK4 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_THPV },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_ROQV },

  // Image Formats
  { &MEDIATYPE_Video, &MEDIASUBTYPE_PNG  },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_TIFF },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_BMP  },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_GIF  },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_TGA  },

  // Special raw formats
  { &MEDIATYPE_Video, &MEDIASUBTYPE_v210 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_v410 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_LAV_RAWVIDEO },
};
const UINT CLAVVideo::sudPinTypesInCount = countof(CLAVVideo::sudPinTypesIn);

// Define Output Media Types
const AMOVIESETUP_MEDIATYPE CLAVVideo::sudPinTypesOut[] = {
  { &MEDIATYPE_Video, &MEDIASUBTYPE_YV12 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_NV12 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_YUY2 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_UYVY },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_RGB32 },
  { &MEDIATYPE_Video, &MEDIASUBTYPE_RGB24 },
};
const UINT CLAVVideo::sudPinTypesOutCount = countof(CLAVVideo::sudPinTypesOut);

// Crawl the lavc_video_codecs array for the proper codec
AVCodecID FindCodecId(const CMediaType *mt)
{
  for (int i=0; i<countof(lavc_video_codecs); ++i) {
    if (mt->subtype == *lavc_video_codecs[i].clsMinorType) {
      return lavc_video_codecs[i].nFFCodec;
    }
  }
  return AV_CODEC_ID_NONE;
}

// Strings will be filled in eventually.
// AV_CODEC_ID_NONE means there is some special handling going on.
// Order is Important, has to be the same as the CC Enum
// Also, the order is used for storage in the Registry
static codec_config_t m_codec_config[] = {
  { 1, { AV_CODEC_ID_H264 }},                                                // Codec_H264
  { 2, { AV_CODEC_ID_VC1, AV_CODEC_ID_VC1IMAGE }},                           // Codec_VC1
  { 1, { AV_CODEC_ID_MPEG1VIDEO }, "mpeg1"},                                 // Codec_MPEG1
  { 1, { AV_CODEC_ID_MPEG2VIDEO }, "mpeg2"},                                 // Codec_MPEG2
  { 1, { AV_CODEC_ID_MPEG4 }},                                               // Codec_MPEG4
  { 3, { AV_CODEC_ID_MSMPEG4V1, AV_CODEC_ID_MSMPEG4V2, AV_CODEC_ID_MSMPEG4V3 }, "msmpeg4", "MS-MPEG-4 (DIVX3)" },   // Codec_MSMPEG4
  { 1, { AV_CODEC_ID_VP8 }},                                                 // Codec_VP8
  { 2, { AV_CODEC_ID_WMV3, AV_CODEC_ID_WMV3IMAGE }},                         // Codec_WMV3
  { 2, { AV_CODEC_ID_WMV1, AV_CODEC_ID_WMV2 }, "wmv12", "Windows Media Video 7/8" },  // Codec_WMV12
  { 3, { AV_CODEC_ID_MJPEG, AV_CODEC_ID_MJPEGB, AV_CODEC_ID_AMV }},                // Codec_MJPEG
  { 2, { AV_CODEC_ID_THEORA, AV_CODEC_ID_VP3 }},                                // Codec_Theora
  { 2, { AV_CODEC_ID_FLV1, AV_CODEC_ID_FLASHSV }, "flash", "Flash Video (FLV1, FSV1)"}, // Codec_FLV1
  { 3, { AV_CODEC_ID_VP6, AV_CODEC_ID_VP6A, AV_CODEC_ID_VP6F }},                   // Codec_VP6
  { 2, { AV_CODEC_ID_SVQ1, AV_CODEC_ID_SVQ3 }, "svq", "SVQ 1 / SVQ 3"},         // Codec_SVQ
  { 1, { AV_CODEC_ID_H261 }},                                                // Codec_H261
  { 2, { AV_CODEC_ID_H263, AV_CODEC_ID_H263I }},                             // Codec_H263
  { 3, { AV_CODEC_ID_INDEO3, AV_CODEC_ID_INDEO4, AV_CODEC_ID_INDEO5 }, "indeo", "Intel Indeo 3/4/5"}, // Codec_Indeo
  { 2, { AV_CODEC_ID_TSCC, AV_CODEC_ID_TSCC2 }},                             // Codec_TSCC
  { 1, { AV_CODEC_ID_FRAPS }},                                               // Codec_Fraps
  { 2, { AV_CODEC_ID_HUFFYUV, AV_CODEC_ID_FFVHUFF }},                           // Codec_HuffYUV
  { 1, { AV_CODEC_ID_QTRLE }},                                               // Codec_QTRle
  { 1, { AV_CODEC_ID_DVVIDEO }},                                             // Codec_DV
  { 1, { AV_CODEC_ID_BINKVIDEO }, "bink"},                                   // Codec_Bink
  { 1, { AV_CODEC_ID_SMACKVIDEO }},                                          // Codec_Smacker
  { 2, { AV_CODEC_ID_RV10, AV_CODEC_ID_RV20 }, "rv12", "RealVideo 1/2" },       // Codev_RV12
  { 2, { AV_CODEC_ID_RV30, AV_CODEC_ID_RV40 }, "rv34", "RealVideo 3/4" },       // Codec_RV34
  { 1, { AV_CODEC_ID_LAGARITH }},                                            // Codec_Lagarith
  { 1, { AV_CODEC_ID_CINEPAK }},                                             // Codec_Cinepak
  { 1, { AV_CODEC_ID_CSCD }},                                                // Codec_Camstudio
  { 1, { AV_CODEC_ID_QPEG }},                                                // Codec_QPEG
  { 2, { AV_CODEC_ID_ZLIB, AV_CODEC_ID_MSZH }, "zlib", "ZLIB/MSZH lossless" },  // Codec_ZLIB
  { 1, { AV_CODEC_ID_RPZA }},                                                // Codec_QTRpza
  { 1, { AV_CODEC_ID_PNG }},                                                 // Codec_PNG
  { 2, { AV_CODEC_ID_MSRLE, AV_CODEC_ID_AASC }},                                // Codec_MSRLE
  { 1, { AV_CODEC_ID_PRORES }},                                              // Codec_ProRes
  { 1, { AV_CODEC_ID_UTVIDEO }},                                             // Codec_UtVideo
  { 1, { AV_CODEC_ID_DIRAC }},                                               // Codec_Dirac
  { 1, { AV_CODEC_ID_DNXHD }},                                               // Codec_DNxHD
  { 1, { AV_CODEC_ID_MSVIDEO1 }},                                            // Codec_MSVideo1
  { 1, { AV_CODEC_ID_8BPS }},                                                // Codec_8BPS
  { 1, { AV_CODEC_ID_LOCO }},                                                // Codec_LOCO
  { 1, { AV_CODEC_ID_ZMBV }},                                                // Codec_ZMBV
  { 1, { AV_CODEC_ID_VCR1 }},                                                // Codec_VCR1
  { 1, { AV_CODEC_ID_SNOW }},                                                // Codec_Snow
  { 1, { AV_CODEC_ID_FFV1 }},                                                // Codec_FFV1
  { 2, { AV_CODEC_ID_V210, AV_CODEC_ID_V410 }, "v210/v410", "v210/v410 uncompressed"}, // Codec_v210
  { 1, { AV_CODEC_ID_JPEG2000 }},                                            // Codec_JPEG2000
  { 1, { AV_CODEC_ID_VMNC }},                                                // Codec_VMNC
  { 1, { AV_CODEC_ID_FLIC }},                                                // Codec_FLIC
  { 1, { AV_CODEC_ID_G2M }},                                                 // Codec_G2M
  { 1, { AV_CODEC_ID_AIC }, "icod", "Apple Intermediate Codec (ICOD)"},      // Codec_ICOD
  { 1, { AV_CODEC_ID_THP }},                                                 // Codec_THP
  { 1, { AV_CODEC_ID_HEVC }},                                                // Codec_HEVC
  { 1, { AV_CODEC_ID_VP9 }},                                                 // Codec_VP9
  { 2, { AV_CODEC_ID_TRUEMOTION1, AV_CODEC_ID_TRUEMOTION2 }, "truemotion", "Duck TrueMotion 1/2"}, // Codec_TrueMotion
  { 1, { AV_CODEC_ID_VP7 }},                                                 // Codec_VP7
  { 1, { AV_CODEC_ID_H264_MVC }, "h264mvc", "H.264 MVC 3D" },                // Codec_H264MVC
  { 1, { AV_CODEC_ID_CFHD }},                                                // Codec_CineformHD
  { 1, { AV_CODEC_ID_MAGICYUV }},                                            // Codec_MagicYUV
  { 1, { AV_CODEC_ID_AV1 }},                                                 // Codec_AV1
};

const codec_config_t *get_codec_config(LAVVideoCodec codec)
{
  codec_config_t *config = &m_codec_config[codec];

  const AVCodecDescriptor *desc = avcodec_descriptor_get(config->codecs[0]);
  if (desc) {
    if (!config->name) {
      config->name = desc->name;
    }

    if (!config->description) {
      config->description = desc->long_name;
    }
  }

  return &m_codec_config[codec];
}

int flip_plane(BYTE *buffer, int stride, int height)
{
  BYTE *line_buffer = (BYTE *)av_malloc(stride);
  BYTE *cur_front   = buffer;
  BYTE *cur_back    = buffer + (stride * (height - 1));
  height /= 2;
  for (int i = 0; i < height; i++) {
    memcpy(line_buffer, cur_front, stride);
    memcpy(cur_front, cur_back, stride);
    memcpy(cur_back, line_buffer, stride);
    cur_front += stride;
    cur_back -= stride;
  }
  av_freep(&line_buffer);
  return 0;
}

void fillDXVAExtFormat(DXVA2_ExtendedFormat &fmt, int range, int primaries, int matrix, int transfer, int chroma_sample_location, bool bClear)
{
  if (bClear)
    fmt.value = 0;

  if (range != -1)
      fmt.NominalRange = range ? DXVA2_NominalRange_0_255 : DXVA2_NominalRange_16_235;

  // Color Primaries
  switch(primaries) {
  case AVCOL_PRI_BT709:
    fmt.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
    break;
  case AVCOL_PRI_BT470M:
    fmt.VideoPrimaries = DXVA2_VideoPrimaries_BT470_2_SysM;
    break;
  case AVCOL_PRI_BT470BG:
    fmt.VideoPrimaries = DXVA2_VideoPrimaries_BT470_2_SysBG;
    break;
  case AVCOL_PRI_SMPTE170M:
    fmt.VideoPrimaries = DXVA2_VideoPrimaries_SMPTE170M;
    break;
  case AVCOL_PRI_SMPTE240M:
    fmt.VideoPrimaries = DXVA2_VideoPrimaries_SMPTE240M;
    break;
  // Values from newer Windows SDK (MediaFoundation)
  case AVCOL_PRI_BT2020:
    fmt.VideoPrimaries = (DXVA2_VideoPrimaries)9;
    break;
  case AVCOL_PRI_SMPTE428:
    // XYZ
    fmt.VideoPrimaries = (DXVA2_VideoPrimaries)10;
    break;
  case AVCOL_PRI_SMPTE431:
    // DCI-P3
    fmt.VideoPrimaries = (DXVA2_VideoPrimaries)11;
    break;
  }

  // Color Space / Transfer Matrix
  switch (matrix) {
  case AVCOL_SPC_BT709:
    fmt.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT709;
    break;
  case AVCOL_SPC_BT470BG:
  case AVCOL_SPC_SMPTE170M:
    fmt.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT601;
    break;
  case AVCOL_SPC_SMPTE240M:
    fmt.VideoTransferMatrix = DXVA2_VideoTransferMatrix_SMPTE240M;
    break;
  // Values from newer Windows SDK (MediaFoundation)
  case AVCOL_SPC_BT2020_CL:
  case AVCOL_SPC_BT2020_NCL:
    fmt.VideoTransferMatrix = (DXVA2_VideoTransferMatrix)4;
    break;
  // Custom values, not official standard, but understood by madVR
  case AVCOL_SPC_FCC:
    fmt.VideoTransferMatrix = (DXVA2_VideoTransferMatrix)6;
    break;
  case AVCOL_SPC_YCGCO:
    fmt.VideoTransferMatrix = (DXVA2_VideoTransferMatrix)7;
    break;
  }

  // Color Transfer Function
  switch(transfer) {
  case AVCOL_TRC_BT709:
  case AVCOL_TRC_SMPTE170M:
  case AVCOL_TRC_BT2020_10:
  case AVCOL_TRC_BT2020_12:
    fmt.VideoTransferFunction = DXVA2_VideoTransFunc_709;
    break;
  case AVCOL_TRC_GAMMA22:
    fmt.VideoTransferFunction = DXVA2_VideoTransFunc_22;
    break;
  case AVCOL_TRC_GAMMA28:
    fmt.VideoTransferFunction = DXVA2_VideoTransFunc_28;
    break;
  case AVCOL_TRC_SMPTE240M:
    fmt.VideoTransferFunction = DXVA2_VideoTransFunc_240M;
    break;
  case AVCOL_TRC_LOG:
    fmt.VideoTransferFunction = MFVideoTransFunc_Log_100;
    break;
  case AVCOL_TRC_LOG_SQRT:
    fmt.VideoTransferFunction = MFVideoTransFunc_Log_316;
    break;
  // Values from newer Windows SDK (MediaFoundation)
  case AVCOL_TRC_SMPTEST2084:
    fmt.VideoTransferFunction = 15;
    break;
  case AVCOL_TRC_ARIB_STD_B67: // HLG
    fmt.VideoTransferFunction = 16;
    break;
  }

  // Chroma location
  switch (chroma_sample_location) {
  case AVCHROMA_LOC_LEFT:
    fmt.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG2;
    break;
  case AVCHROMA_LOC_CENTER:
    fmt.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG1;
    break;
  case AVCHROMA_LOC_TOPLEFT:
    fmt.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Cosited;
    break;
  }
}

void processFFHDRData(MediaSideDataHDR *sd, AVMasteringDisplayMetadata *ff)
{
  if (!sd || !ff)
    return;

  // avcodec exports the display primaries in RGB order, we export them in GBR
  if (ff->has_primaries) {
    sd->display_primaries_x[0] = av_q2d(ff->display_primaries[1][0]);
    sd->display_primaries_y[0] = av_q2d(ff->display_primaries[1][1]);
    sd->display_primaries_x[1] = av_q2d(ff->display_primaries[2][0]);
    sd->display_primaries_y[1] = av_q2d(ff->display_primaries[2][1]);
    sd->display_primaries_x[2] = av_q2d(ff->display_primaries[0][0]);
    sd->display_primaries_y[2] = av_q2d(ff->display_primaries[0][1]);

    sd->white_point_x = av_q2d(ff->white_point[0]);
    sd->white_point_y = av_q2d(ff->white_point[1]);
  }

  if (ff->has_luminance) {
    sd->max_display_mastering_luminance = av_q2d(ff->max_luminance);
    sd->min_display_mastering_luminance = av_q2d(ff->min_luminance);
  }
}

void processFFHDR10PlusData(MediaSideDataHDR10Plus *sd, AVDynamicHDRPlus *ff, int width, int height)
{
  if (!sd || !ff)
    return;

  if (ff->num_windows > 3)
    return;

  sd->num_windows = ff->num_windows;
  for (int i = 0; i < ff->num_windows; i++)
  {
    sd->windows[i].upper_left_corner_x = (int)av_q2d(ff->params[i].window_upper_left_corner_x) * (width - 1);
    sd->windows[i].upper_left_corner_y = (int)av_q2d(ff->params[i].window_upper_left_corner_y) * (height - 1);

    sd->windows[i].lower_right_corner_x = (int)av_q2d(ff->params[i].window_lower_right_corner_x) * (width - 1);
    sd->windows[i].lower_right_corner_y = (int)av_q2d(ff->params[i].window_lower_right_corner_y) * (height - 1);

    sd->windows[i].center_of_ellipse_x = ff->params[i].center_of_ellipse_x;
    sd->windows[i].center_of_ellipse_y = ff->params[i].center_of_ellipse_y;

    sd->windows[i].rotation_angle = ff->params[i].rotation_angle;
    sd->windows[i].semimajor_axis_internal_ellipse = ff->params[i].semimajor_axis_internal_ellipse;
    sd->windows[i].semimajor_axis_external_ellipse = ff->params[i].semimajor_axis_external_ellipse;
    sd->windows[i].semiminor_axis_external_ellipse = ff->params[i].semiminor_axis_external_ellipse;

    sd->windows[i].overlap_process_option = ff->params[i].overlap_process_option;

    for (int k = 0; k < 3; k++)
      sd->windows[i].maxscl[k] = av_q2d(ff->params[i].maxscl[k]);

    sd->windows[i].average_maxrgb = av_q2d(ff->params[i].average_maxrgb);

    sd->windows[i].num_distribution_maxrgb_percentiles = ff->params[i].num_distribution_maxrgb_percentiles;
    for (int k = 0; k < ff->params[i].num_distribution_maxrgb_percentiles; k++)
    {
      sd->windows[i].distribution_maxrgb_percentiles[k].percentage = ff->params[i].distribution_maxrgb[k].percentage;
      sd->windows[i].distribution_maxrgb_percentiles[k].percentile = av_q2d(ff->params[i].distribution_maxrgb[k].percentile);
    }

    sd->windows[i].fraction_bright_pixels = av_q2d(ff->params[i].fraction_bright_pixels);
    sd->windows[i].tone_mapping_flag = ff->params[i].tone_mapping_flag;

    if (sd->windows[i].tone_mapping_flag)
    {
      sd->windows[i].knee_point_x = av_q2d(ff->params[i].knee_point_x);
      sd->windows[i].knee_point_y = av_q2d(ff->params[i].knee_point_y);

      sd->windows[i].num_bezier_curve_anchors = ff->params[i].num_bezier_curve_anchors;
      for (int k = 0; k < ff->params[i].num_bezier_curve_anchors; k++)
        sd->windows[i].bezier_curve_anchors[k] = av_q2d(ff->params[i].bezier_curve_anchors[k]);
    }

    sd->windows[i].color_saturation_mapping_flag = ff->params[i].color_saturation_mapping_flag;
    if (sd->windows[i].color_saturation_mapping_flag)
      sd->windows[i].color_saturation_weight = av_q2d(ff->params[i].color_saturation_weight);
  }

  sd->targeted_system_display_maximum_luminance = av_q2d(ff->targeted_system_display_maximum_luminance);

  sd->targeted_system_display_actual_peak_luminance_flag = ff->targeted_system_display_actual_peak_luminance_flag;
  sd->num_rows_targeted_system_display_actual_peak_luminance = ff->num_rows_targeted_system_display_actual_peak_luminance;
  sd->num_cols_targeted_system_display_actual_peak_luminance = ff->num_cols_targeted_system_display_actual_peak_luminance;
  for (int i = 0; i < ff->num_rows_targeted_system_display_actual_peak_luminance; i++)
    for (int j = 0; j < ff->num_cols_targeted_system_display_actual_peak_luminance; j++)
      sd->targeted_system_display_actual_peak_luminance[i][j] = av_q2d(ff->targeted_system_display_actual_peak_luminance[i][j]);

  sd->mastering_display_actual_peak_luminance_flag = ff->mastering_display_actual_peak_luminance_flag;
  sd->num_rows_mastering_display_actual_peak_luminance = ff->num_rows_mastering_display_actual_peak_luminance;
  sd->num_cols_mastering_display_actual_peak_luminance = ff->num_cols_mastering_display_actual_peak_luminance;
  for (int i = 0; i < ff->num_rows_mastering_display_actual_peak_luminance; i++)
    for (int j = 0; j < ff->num_cols_mastering_display_actual_peak_luminance; j++)
      sd->mastering_display_actual_peak_luminance[i][j] = av_q2d(ff->mastering_display_actual_peak_luminance[i][j]);
}

extern "C" const uint8_t *avpriv_find_start_code(const uint8_t *p, const uint8_t *end, uint32_t *state);

int CheckForSequenceMarkers(AVCodecID codec, const uint8_t *buf, long len, uint32_t *state, const uint8_t **pos)
{
  int status = 0;
  if (buf && len > 0) {
    const uint8_t *p = buf, *end = buf + len;
    if (codec == AV_CODEC_ID_MPEG2VIDEO) {
      while (p < end) {
        p = avpriv_find_start_code(p, end, state);
        if (*state == 0x000001b7) {
          DbgLog((LOG_TRACE, 50, L"Found SEQ_END_CODE at %p (end: %p)", p, end));
          status |= STATE_EOS_FOUND;
          if (pos)
            *pos = p;
          return status;
        } else if (*state == 0x000001b8) {
          status |= STATE_GOP_FOUND;
        }
      }
    }
    return status;
  }
  return status;
}

int sws_scale2(struct SwsContext *c, const uint8_t *const srcSlice[], const ptrdiff_t srcStride[], int srcSliceY, int srcSliceH, uint8_t *const dst[], const ptrdiff_t dstStride[])
{
  if (!c)
    return -1;

  int srcStride2[4];
  int dstStride2[4];

  for (int i = 0; i < 4; i++) {
    srcStride2[i] = (int)srcStride[i];
    dstStride2[i] = (int)dstStride[i];
  }
  return sws_scale(c, srcSlice, srcStride2, srcSliceY, srcSliceH, dst, dstStride2);
}
