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

#pragma once

#include "parser.h"

/** DCA syncwords */
#define DCA_MARKER_RAW_BE 0x7FFE8001
#define DCA_MARKER_RAW_LE 0xFE7F0180
#define DCA_MARKER_14B_BE 0x1FFFE800
#define DCA_MARKER_14B_LE 0xFF1F00E8

/** DCA-HD specific block starts with this marker. */
#define DCA_HD_MARKER     0x64582025

/** DCA extension blocks */
#define DCA_XCH_MARKER    0x5a5a5a5a
#define DCA_XXCH_MARKER   0x47004A03

struct DTSParserContext;

struct DTSHeader {
  unsigned HasCore;
  unsigned CRCPresent;
  unsigned SamplesPerBlock;
  unsigned Blocks;
  unsigned FrameSize;
  unsigned ChannelLayout;
  unsigned SampleRate;
  unsigned LFE;

  // Extensions
  unsigned ES;
  unsigned XChChannelLayout;

  unsigned IsHD;
  unsigned HDTotalChannels;
  unsigned HDSpeakerMask;
};

int init_dts_parser(DTSParserContext **pContext);
int close_dts_parser(DTSParserContext **pContext);

int parse_dts_header(DTSParserContext *pContext, DTSHeader *pHeader, uint8_t *pBuffer, unsigned uSize);
