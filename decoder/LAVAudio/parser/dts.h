/*
 *      Copyright (C) 2010-2012 Hendrik Leppkes
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

struct DTSParserContext;

struct DTSHeader {
  unsigned HasCore;
  unsigned CRCPresent;
  unsigned SamplesPerBlock;
  unsigned Blocks;
  unsigned FrameSize;
  unsigned ChannelLayout;
  unsigned SampleRate;
  unsigned Bitrate;
  unsigned LFE;

  // Extensions
  unsigned XChChannelLayout;

  unsigned IsHD;
  unsigned HDTotalChannels;
  unsigned HDSpeakerMask;
};

int init_dts_parser(DTSParserContext **pContext);
int close_dts_parser(DTSParserContext **pContext);

int parse_dts_header(DTSParserContext *pContext, DTSHeader *pHeader, uint8_t *pBuffer, unsigned uSize);
