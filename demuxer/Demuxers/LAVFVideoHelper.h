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
 *
 *  Contributions by Ti-BEN from the XBMC DSPlayer Project, also under GPLv2
 */

#pragma once

#include "dvdmedia.h"

extern "C" {
#include "libavformat/riff.h"
}

const AVCodecTag mp_bmp_tags[] = {
  { CODEC_ID_AMV,               MKTAG('A', 'M', 'V', 'V')},
  { CODEC_ID_BETHSOFTVID,       MKTAG('B', 'E', 'T', 'H')},
  { CODEC_ID_BFI,               MKTAG('B', 'F', 'I', 'V')},
  { CODEC_ID_C93,               MKTAG('C', '9', '3', 'V')},
  { CODEC_ID_CDGRAPHICS,        MKTAG('C', 'D', 'G', 'R')},
  { CODEC_ID_DNXHD,             MKTAG('A', 'V', 'd', 'n')},
  { CODEC_ID_DSICINVIDEO,       MKTAG('D', 'C', 'I', 'V')},
  { CODEC_ID_DXA,               MKTAG('D', 'X', 'A', '1')},
  { CODEC_ID_FLIC,              MKTAG('F', 'L', 'I', 'C')},
  { CODEC_ID_IDCIN,             MKTAG('I', 'D', 'C', 'I')},
  { CODEC_ID_INTERPLAY_VIDEO,   MKTAG('I', 'N', 'P', 'V')},
  { CODEC_ID_MDEC,              MKTAG('M', 'D', 'E', 'C')},
  { CODEC_ID_MOTIONPIXELS,      MKTAG('M', 'V', 'I', '1')},
  { CODEC_ID_NUV,               MKTAG('N', 'U', 'V', '1')},
  { CODEC_ID_RL2,               MKTAG('R', 'L', '2', 'V')},
  { CODEC_ID_ROQ,               MKTAG('R', 'o', 'Q', 'V')},
  { CODEC_ID_RV10,              MKTAG('R', 'V', '1', '0')},
  { CODEC_ID_RV20,              MKTAG('R', 'V', '2', '0')},
  { CODEC_ID_RV30,              MKTAG('R', 'V', '3', '0')},
  { CODEC_ID_RV40,              MKTAG('R', 'V', '4', '0')},
  { CODEC_ID_TGV,               MKTAG('f', 'V', 'G', 'T')},
  { CODEC_ID_THP,               MKTAG('T', 'H', 'P', 'V')},
  { CODEC_ID_TIERTEXSEQVIDEO,   MKTAG('T', 'S', 'E', 'Q')},
  { CODEC_ID_TXD,               MKTAG('T', 'X', 'D', 'V')},
  { CODEC_ID_VP6A,              MKTAG('V', 'P', '6', 'A')},
  { CODEC_ID_VMDVIDEO,          MKTAG('V', 'M', 'D', 'V')},
  { CODEC_ID_WS_VQA,            MKTAG('V', 'Q', 'A', 'V')},
  { CODEC_ID_XAN_WC3,           MKTAG('W', 'C', '3', 'V')},
};
const AVCodecTag ff2_codec_bmp_tags[] = {
  //{ CODEC_ID_H264,         MKTAG('H', '2', '6', '4') },
  //{ CODEC_ID_H264,         MKTAG('h', '2', '6', '4') },
  //{ CODEC_ID_H264,         MKTAG('X', '2', '6', '4') },
  //{ CODEC_ID_H264,         MKTAG('x', '2', '6', '4') },
  { CODEC_ID_H264,         MKTAG('A', 'V', 'C', '1') },
  { CODEC_ID_H264,         MKTAG('a', 'v', 'c', '1') },
  //{ CODEC_ID_H264,         MKTAG('V', 'S', 'S', 'H') },
  { CODEC_ID_H263,         MKTAG('H', '2', '6', '3') },
  { CODEC_ID_H263,         MKTAG('X', '2', '6', '3') },
  { CODEC_ID_H263,         MKTAG('T', '2', '6', '3') },
  { CODEC_ID_H263,         MKTAG('L', '2', '6', '3') },
  { CODEC_ID_H263,         MKTAG('V', 'X', '1', 'K') },
  { CODEC_ID_H263,         MKTAG('Z', 'y', 'G', 'o') },
  { CODEC_ID_H263P,        MKTAG('H', '2', '6', '3') },
  { CODEC_ID_H263I,        MKTAG('I', '2', '6', '3') }, /* intel h263 */
  { CODEC_ID_H261,         MKTAG('H', '2', '6', '1') },
  { CODEC_ID_H263P,        MKTAG('U', '2', '6', '3') },
  { CODEC_ID_H263P,        MKTAG('v', 'i', 'v', '1') },
  { CODEC_ID_MPEG4,        MKTAG('F', 'M', 'P', '4') },
  { CODEC_ID_MPEG4,        MKTAG('D', 'I', 'V', 'X') },
  { CODEC_ID_MPEG4,        MKTAG('D', 'X', '5', '0') },
  { CODEC_ID_MPEG4,        MKTAG('X', 'V', 'I', 'D') },
  { CODEC_ID_MPEG4,        MKTAG('M', 'P', '4', 'S') },
  { CODEC_ID_MPEG4,        MKTAG('M', '4', 'S', '2') },
  { CODEC_ID_MPEG4,        MKTAG( 4 ,  0 ,  0 ,  0 ) }, /* some broken avi use this */
  { CODEC_ID_MPEG4,        MKTAG('D', 'I', 'V', '1') },
  { CODEC_ID_MPEG4,        MKTAG('B', 'L', 'Z', '0') },
  { CODEC_ID_MPEG4,        MKTAG('m', 'p', '4', 'v') },
  { CODEC_ID_MPEG4,        MKTAG('U', 'M', 'P', '4') },
  { CODEC_ID_MPEG4,        MKTAG('W', 'V', '1', 'F') },
  { CODEC_ID_MPEG4,        MKTAG('S', 'E', 'D', 'G') },
  { CODEC_ID_MPEG4,        MKTAG('R', 'M', 'P', '4') },
  { CODEC_ID_MPEG4,        MKTAG('3', 'I', 'V', '2') },
  { CODEC_ID_MPEG4,        MKTAG('F', 'F', 'D', 'S') },
  { CODEC_ID_MPEG4,        MKTAG('F', 'V', 'F', 'W') },
  { CODEC_ID_MPEG4,        MKTAG('D', 'C', 'O', 'D') },
  { CODEC_ID_MPEG4,        MKTAG('M', 'V', 'X', 'M') },
  { CODEC_ID_MPEG4,        MKTAG('P', 'M', '4', 'V') },
  { CODEC_ID_MPEG4,        MKTAG('S', 'M', 'P', '4') },
  { CODEC_ID_MPEG4,        MKTAG('D', 'X', 'G', 'M') },
  { CODEC_ID_MPEG4,        MKTAG('V', 'I', 'D', 'M') },
  { CODEC_ID_MPEG4,        MKTAG('M', '4', 'T', '3') },
  { CODEC_ID_MPEG4,        MKTAG('G', 'E', 'O', 'X') },
  { CODEC_ID_MPEG4,        MKTAG('H', 'D', 'X', '4') }, /* flipped video */
  { CODEC_ID_MPEG4,        MKTAG('D', 'M', 'K', '2') },
  { CODEC_ID_MPEG4,        MKTAG('D', 'I', 'G', 'I') },
  { CODEC_ID_MPEG4,        MKTAG('I', 'N', 'M', 'C') },
  { CODEC_ID_MPEG4,        MKTAG('E', 'P', 'H', 'V') }, /* Ephv MPEG-4 */
  { CODEC_ID_MPEG4,        MKTAG('E', 'M', '4', 'A') },
  { CODEC_ID_MPEG4,        MKTAG('M', '4', 'C', 'C') }, /* Divio MPEG-4 */
  { CODEC_ID_MPEG4,        MKTAG('S', 'N', '4', '0') },
  { CODEC_ID_MPEG4,        MKTAG('V', 'S', 'P', 'X') },
  { CODEC_ID_MPEG4,        MKTAG('U', 'L', 'D', 'X') },
  { CODEC_ID_MPEG4,        MKTAG('G', 'E', 'O', 'V') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '3') }, /* default signature when using MSMPEG4 */
  { CODEC_ID_MSMPEG4V3,    MKTAG('M', 'P', '4', '3') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('M', 'P', 'G', '3') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '5') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '6') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '4') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'V', 'X', '3') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('A', 'P', '4', '1') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('C', 'O', 'L', '1') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('C', 'O', 'L', '0') },
  { CODEC_ID_MSMPEG4V2,    MKTAG('M', 'P', '4', '2') },
  { CODEC_ID_MSMPEG4V2,    MKTAG('D', 'I', 'V', '2') },
  { CODEC_ID_MSMPEG4V1,    MKTAG('M', 'P', 'G', '4') },
  { CODEC_ID_MSMPEG4V1,    MKTAG('M', 'P', '4', '1') },
  { CODEC_ID_WMV1,         MKTAG('W', 'M', 'V', '1') },
  { CODEC_ID_WMV2,         MKTAG('W', 'M', 'V', '2') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 's', 'd') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', 'd') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', '1') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 's', 'l') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', '2', '5') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', '5', '0') },
  { CODEC_ID_DVVIDEO,      MKTAG('c', 'd', 'v', 'c') }, /* Canopus DV */
  { CODEC_ID_DVVIDEO,      MKTAG('C', 'D', 'V', 'H') }, /* Canopus DV */
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'c', ' ') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'c', 's') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', '1') },
  { CODEC_ID_MPEG1VIDEO,   MKTAG('m', 'p', 'g', '1') },
  { CODEC_ID_MPEG1VIDEO,   MKTAG('m', 'p', 'g', '2') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('m', 'p', 'g', '2') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('M', 'P', 'E', 'G') },
  { CODEC_ID_MPEG1VIDEO,   MKTAG('P', 'I', 'M', '1') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('P', 'I', 'M', '2') },
  { CODEC_ID_MPEG1VIDEO,   MKTAG('V', 'C', 'R', '2') },
  { CODEC_ID_MPEG1VIDEO,   MKTAG( 1 ,  0 ,  0 ,  16) },
  { CODEC_ID_MPEG2VIDEO,   MKTAG( 2 ,  0 ,  0 ,  16) },
  { CODEC_ID_MPEG4,        MKTAG( 4 ,  0 ,  0 ,  16) },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('D', 'V', 'R', ' ') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('M', 'M', 'E', 'S') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('L', 'M', 'P', '2') }, /* Lead MPEG2 in avi */
  { CODEC_ID_MPEG2VIDEO,   MKTAG('s', 'l', 'i', 'f') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('E', 'M', '2', 'V') },
  { CODEC_ID_MJPEG,        MKTAG('M', 'J', 'P', 'G') },
  { CODEC_ID_MJPEG,        MKTAG('L', 'J', 'P', 'G') },
  { CODEC_ID_MJPEG,        MKTAG('d', 'm', 'b', '1') },
  { CODEC_ID_MJPEG,        MKTAG('m', 'j', 'p', 'a') },
  { CODEC_ID_LJPEG,        MKTAG('L', 'J', 'P', 'G') },
  { CODEC_ID_MJPEG,        MKTAG('J', 'P', 'G', 'L') }, /* Pegasus lossless JPEG */
  { CODEC_ID_JPEGLS,       MKTAG('M', 'J', 'L', 'S') }, /* JPEG-LS custom FOURCC for avi - encoder */
  { CODEC_ID_JPEGLS,       MKTAG('M', 'J', 'P', 'G') },
  { CODEC_ID_MJPEG,        MKTAG('M', 'J', 'L', 'S') }, /* JPEG-LS custom FOURCC for avi - decoder */
  { CODEC_ID_MJPEG,        MKTAG('j', 'p', 'e', 'g') },
  { CODEC_ID_MJPEG,        MKTAG('I', 'J', 'P', 'G') },
  { CODEC_ID_MJPEG,        MKTAG('A', 'V', 'R', 'n') },
  { CODEC_ID_MJPEG,        MKTAG('A', 'C', 'D', 'V') },
  { CODEC_ID_MJPEG,        MKTAG('Q', 'I', 'V', 'G') },
  { CODEC_ID_MJPEG,        MKTAG('S', 'L', 'M', 'J') }, /* SL M-JPEG */
  { CODEC_ID_MJPEG,        MKTAG('C', 'J', 'P', 'G') }, /* Creative Webcam JPEG */
  { CODEC_ID_MJPEG,        MKTAG('I', 'J', 'L', 'V') }, /* Intel JPEG Library Video Codec */
  { CODEC_ID_MJPEG,        MKTAG('M', 'V', 'J', 'P') }, /* Midvid JPEG Video Codec */
  { CODEC_ID_MJPEG,        MKTAG('A', 'V', 'I', '1') },
  { CODEC_ID_MJPEG,        MKTAG('A', 'V', 'I', '2') },
  { CODEC_ID_MJPEG,        MKTAG('M', 'T', 'S', 'J') },
  { CODEC_ID_MJPEG,        MKTAG('Z', 'J', 'P', 'G') }, /* Paradigm Matrix M-JPEG Codec */
  { CODEC_ID_HUFFYUV,      MKTAG('H', 'F', 'Y', 'U') },
  { CODEC_ID_FFVHUFF,      MKTAG('F', 'F', 'V', 'H') },
  { CODEC_ID_CYUV,         MKTAG('C', 'Y', 'U', 'V') },
  { CODEC_ID_RAWVIDEO,     MKTAG( 0 ,  0 ,  0 ,  0 ) },
  { CODEC_ID_RAWVIDEO,     MKTAG( 3 ,  0 ,  0 ,  0 ) },
  { CODEC_ID_RAWVIDEO,     MKTAG('I', '4', '2', '0') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'U', 'Y', '2') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', '4', '2', '2') },
  { CODEC_ID_RAWVIDEO,     MKTAG('V', '4', '2', '2') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'U', 'N', 'V') },
  { CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'N', 'V') },
  { CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'N', 'Y') },
  { CODEC_ID_RAWVIDEO,     MKTAG('u', 'y', 'v', '1') },
  { CODEC_ID_RAWVIDEO,     MKTAG('2', 'V', 'u', '1') },
  { CODEC_ID_RAWVIDEO,     MKTAG('2', 'v', 'u', 'y') },
  { CODEC_ID_RAWVIDEO,     MKTAG('P', '4', '2', '2') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'V', '1', '2') },
  { CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'V', 'Y') },
  { CODEC_ID_RAWVIDEO,     MKTAG('V', 'Y', 'U', 'Y') },
  { CODEC_ID_RAWVIDEO,     MKTAG('I', 'Y', 'U', 'V') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', '8', '0', '0') },
  { CODEC_ID_RAWVIDEO,     MKTAG('H', 'D', 'Y', 'C') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'V', 'U', '9') },
  { CODEC_ID_RAWVIDEO,     MKTAG('V', 'D', 'T', 'Z') }, /* SoftLab-NSK VideoTizer */
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', '4', '1', '1') },
  { CODEC_ID_RAWVIDEO,     MKTAG('N', 'V', '1', '2') },
  { CODEC_ID_RAWVIDEO,     MKTAG('N', 'V', '2', '1') },
  { CODEC_ID_FRWU,         MKTAG('F', 'R', 'W', 'U') },
  { CODEC_ID_R210,         MKTAG('r', '2', '1', '0') },
  { CODEC_ID_V210,         MKTAG('v', '2', '1', '0') },
  { CODEC_ID_INDEO3,       MKTAG('I', 'V', '3', '1') },
  { CODEC_ID_INDEO3,       MKTAG('I', 'V', '3', '2') },
  { CODEC_ID_INDEO4,       MKTAG('I', 'V', '4', '1') },
  { CODEC_ID_INDEO5,       MKTAG('I', 'V', '5', '0') },
  { CODEC_ID_VP3,          MKTAG('V', 'P', '3', '1') },
  { CODEC_ID_VP3,          MKTAG('V', 'P', '3', '0') },
  { CODEC_ID_VP5,          MKTAG('V', 'P', '5', '0') },
  { CODEC_ID_VP6,          MKTAG('V', 'P', '6', '0') },
  { CODEC_ID_VP6,          MKTAG('V', 'P', '6', '1') },
  { CODEC_ID_VP6,          MKTAG('V', 'P', '6', '2') },
  { CODEC_ID_VP6F,         MKTAG('V', 'P', '6', 'F') },
  { CODEC_ID_VP6F,         MKTAG('F', 'L', 'V', '4') },
  { CODEC_ID_VP8,          MKTAG('V', 'P', '8', '0') },
  { CODEC_ID_ASV1,         MKTAG('A', 'S', 'V', '1') },
  { CODEC_ID_ASV2,         MKTAG('A', 'S', 'V', '2') },
  { CODEC_ID_VCR1,         MKTAG('V', 'C', 'R', '1') },
  { CODEC_ID_FFV1,         MKTAG('F', 'F', 'V', '1') },
  { CODEC_ID_XAN_WC4,      MKTAG('X', 'x', 'a', 'n') },
  { CODEC_ID_MIMIC,        MKTAG('L', 'M', '2', '0') },
  { CODEC_ID_MSRLE,        MKTAG('m', 'r', 'l', 'e') },
  { CODEC_ID_MSRLE,        MKTAG( 1 ,  0 ,  0 ,  0 ) },
  { CODEC_ID_MSRLE,        MKTAG( 2 ,  0 ,  0 ,  0 ) },
  { CODEC_ID_MSVIDEO1,     MKTAG('M', 'S', 'V', 'C') },
  { CODEC_ID_MSVIDEO1,     MKTAG('m', 's', 'v', 'c') },
  { CODEC_ID_MSVIDEO1,     MKTAG('C', 'R', 'A', 'M') },
  { CODEC_ID_MSVIDEO1,     MKTAG('c', 'r', 'a', 'm') },
  { CODEC_ID_MSVIDEO1,     MKTAG('W', 'H', 'A', 'M') },
  { CODEC_ID_MSVIDEO1,     MKTAG('w', 'h', 'a', 'm') },
  { CODEC_ID_CINEPAK,      MKTAG('c', 'v', 'i', 'd') },
  { CODEC_ID_TRUEMOTION1,  MKTAG('D', 'U', 'C', 'K') },
  { CODEC_ID_TRUEMOTION1,  MKTAG('P', 'V', 'E', 'Z') },
  { CODEC_ID_MSZH,         MKTAG('M', 'S', 'Z', 'H') },
  { CODEC_ID_ZLIB,         MKTAG('Z', 'L', 'I', 'B') },
  { CODEC_ID_SNOW,         MKTAG('S', 'N', 'O', 'W') },
  { CODEC_ID_4XM,          MKTAG('4', 'X', 'M', 'V') },
  { CODEC_ID_FLV1,         MKTAG('F', 'L', 'V', '1') },
  { CODEC_ID_FLASHSV,      MKTAG('F', 'S', 'V', '1') },
  { CODEC_ID_SVQ1,         MKTAG('s', 'v', 'q', '1') },
  { CODEC_ID_TSCC,         MKTAG('t', 's', 'c', 'c') },
  { CODEC_ID_ULTI,         MKTAG('U', 'L', 'T', 'I') },
  { CODEC_ID_VIXL,         MKTAG('V', 'I', 'X', 'L') },
  { CODEC_ID_QPEG,         MKTAG('Q', 'P', 'E', 'G') },
  { CODEC_ID_QPEG,         MKTAG('Q', '1', '.', '0') },
  { CODEC_ID_QPEG,         MKTAG('Q', '1', '.', '1') },
  { CODEC_ID_WMV3,         MKTAG('W', 'M', 'V', '3') },
  { CODEC_ID_VC1,          MKTAG('W', 'V', 'C', '1') },
  { CODEC_ID_VC1,          MKTAG('W', 'M', 'V', 'A') },
  { CODEC_ID_LOCO,         MKTAG('L', 'O', 'C', 'O') },
  { CODEC_ID_WNV1,         MKTAG('W', 'N', 'V', '1') },
  { CODEC_ID_AASC,         MKTAG('A', 'A', 'S', 'C') },
  { CODEC_ID_INDEO2,       MKTAG('R', 'T', '2', '1') },
  { CODEC_ID_FRAPS,        MKTAG('F', 'P', 'S', '1') },
  { CODEC_ID_THEORA,       MKTAG('t', 'h', 'e', 'o') },
  { CODEC_ID_TRUEMOTION2,  MKTAG('T', 'M', '2', '0') },
  { CODEC_ID_CSCD,         MKTAG('C', 'S', 'C', 'D') },
  { CODEC_ID_ZMBV,         MKTAG('Z', 'M', 'B', 'V') },
  { CODEC_ID_KMVC,         MKTAG('K', 'M', 'V', 'C') },
  { CODEC_ID_CAVS,         MKTAG('C', 'A', 'V', 'S') },
  { CODEC_ID_JPEG2000,     MKTAG('M', 'J', '2', 'C') },
  { CODEC_ID_VMNC,         MKTAG('V', 'M', 'n', 'c') },
  { CODEC_ID_TARGA,        MKTAG('t', 'g', 'a', ' ') },
  { CODEC_ID_PNG,          MKTAG('M', 'P', 'N', 'G') },
  { CODEC_ID_PNG,          MKTAG('P', 'N', 'G', '1') },
  { CODEC_ID_CLJR,         MKTAG('c', 'l', 'j', 'r') },
  { CODEC_ID_DIRAC,        MKTAG('d', 'r', 'a', 'c') },
  { CODEC_ID_RPZA,         MKTAG('a', 'z', 'p', 'r') },
  { CODEC_ID_RPZA,         MKTAG('R', 'P', 'Z', 'A') },
  { CODEC_ID_RPZA,         MKTAG('r', 'p', 'z', 'a') },
  { CODEC_ID_SP5X,         MKTAG('S', 'P', '5', '4') },
  { CODEC_ID_AURA,         MKTAG('A', 'U', 'R', 'A') },
  { CODEC_ID_AURA2,        MKTAG('A', 'U', 'R', '2') },
  { CODEC_ID_DPX,          MKTAG('d', 'p', 'x', ' ') },
  { CODEC_ID_KGV1,         MKTAG('K', 'G', 'V', '1') },
  { CODEC_ID_NONE,         0 }
};
const struct AVCodecTag * const mp_bmp_taglists[] = {ff2_codec_bmp_tags, mp_bmp_tags, 0};

class CLAVFVideoHelper
{
public:
  CLAVFVideoHelper() {};
  CMediaType initVideoType(CodecID codecId, unsigned int &codecTag, std::string container);

  VIDEOINFOHEADER *CreateVIH(const AVStream *avstream, ULONG *size);
  VIDEOINFOHEADER2 *CreateVIH2(const AVStream *avstream, ULONG *size, std::string container = "");
  MPEG1VIDEOINFO *CreateMPEG1VI(const AVStream *avstream, ULONG *size);
  MPEG2VIDEOINFO *CreateMPEG2VI(const AVStream *avstream, ULONG *size, std::string container = "");
};

extern CLAVFVideoHelper g_VideoHelper;
