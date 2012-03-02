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

#define __STDC_CONSTANT_MACROS

// Exclude inline asm from being included
#define AVCODEC_X86_MATHOPS_H

extern "C" {
#pragma warning( push )
#pragma warning( disable : 4018 )
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#include "libavcodec/avcodec.h"
#include "libavcodec/get_bits.h"
#include "libavcodec/dcadata.h"
#include "libavcodec/dca_parser.h"
#pragma warning( pop )
};

#include "dts.h"
#include "parser.h"

#include <vector>

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

struct DTSParserContext {
  GetBitContext *gb;
};

int init_dts_parser(DTSParserContext **pContext)
{
  if(!pContext) return -1;

  *pContext = (DTSParserContext *)malloc(sizeof(DTSParserContext));
  memset(*pContext, 0, sizeof(DTSParserContext));
  if(!*pContext) return -1;

  (*pContext)->gb = new GetBitContext();

  return 0;
}

int close_dts_parser(DTSParserContext **pContext)
{
  if(!pContext) return -1;

  delete (*pContext)->gb;
  free(*pContext);
  *pContext = NULL;

  return 0;
}

static int parse_dts_xch_header(DTSParserContext *pContext, DTSHeader *pHeader, const uint8_t *pBuffer, unsigned uSize)
{
  GetBitContext *gb = pContext->gb;
  init_get_bits(gb, pBuffer, uSize << 3);

  get_bits(gb, 32);                                   /* DCA_XCH_MARKER */
  get_bits(gb, 10);                                   /* XCh Frame Size */
  pHeader->XChChannelLayout = get_bits(gb, 4);        /* Extension Channel Arrangement */

  return 0;
}


static int parse_dts_xch_hd_header(DTSParserContext *pContext, DTSHeader *pHeader, const uint8_t *pBuffer, unsigned uSize)
{
  GetBitContext *gb = pContext->gb;
  init_get_bits(gb, pBuffer, uSize << 3);

  /* Huh? */
  pHeader->XChChannelLayout = 1;

  return 0;
}

static int parse_dts_xxch_header(DTSParserContext *pContext, DTSHeader *pHeader, const uint8_t *pBuffer, unsigned uSize)
{
  GetBitContext *gb = pContext->gb;
  init_get_bits(gb, pBuffer, uSize << 3);

  get_bits(gb, 32);                                   /* DCA_XXCH_MARKER */
  get_bits(gb, 8);                                    /* ? */
  pHeader->XChChannelLayout = get_bits(gb, 2);        /* Channels Added? */
  get_bits(gb, 6);                                    /* ? */

  return 0;
}

static int parse_dts_xxch_hd_header(DTSParserContext *pContext, DTSHeader *pHeader, const uint8_t *pBuffer, unsigned uSize)
{
  GetBitContext *gb = pContext->gb;
  init_get_bits(gb, pBuffer, uSize << 3);

  get_bits(gb, 32);                                   /* DCA_XXCH_MARKER */
  get_bits(gb, 8);                                    /* ? */
  pHeader->XChChannelLayout = get_bits(gb, 2);        /* Channels Added? */
  get_bits(gb, 6);                                    /* ? */

  return 0;
}

static int parse_dts_hd_header(DTSParserContext *pContext, DTSHeader *pHeader, const uint8_t *pBuffer, unsigned uSize)
{
  GetBitContext *gb = pContext->gb;
  init_get_bits(gb, pBuffer, uSize << 3);

  unsigned NumAudio = 1;
  unsigned NumAssets = 1;

  get_bits(gb, 32);                                   /* DCA_HD_MARKER */
  get_bits(gb, 8);                                    /* Unknown */
  unsigned SubIdx = get_bits(gb, 2);                  /* Substream Index */
  unsigned blownUp = get_bits(gb, 1);                 /* Blown Up Header */
  if (blownUp) {
    get_bits(gb, 12);                                 /* Header Size */
    get_bits(gb, 20);                                 /* HD Size */
  } else {
    get_bits(gb, 8);                                  /* Header Size */
    get_bits(gb, 16);                                 /* HD Size */
  }
  unsigned staticFields = get_bits(gb, 1);            /* Static fields present */
  if (staticFields) {
    std::vector<uint32_t> ActiveExSSMasks;

    get_bits(gb, 2);                                  /* Reference clock code */
    get_bits(gb, 3);                                  /* ExSS frame duration code */           
    if (get_bits(gb, 1))                              /* Timestamp flag */
      get_bits(gb, 36);                               /* Timestamp */
    NumAudio = get_bits(gb, 3) + 1;                       /* Num audio present */
    NumAssets = get_bits(gb, 3) + 1;                      /* Num assets */
    for (uint8_t Pos = 0; Pos < NumAudio; Pos++) {
      uint32_t mask = get_bits(gb, SubIdx+1);         /* Active ExSS masks */
      ActiveExSSMasks.push_back(mask);
    }
    for (uint8_t Pos = 0; Pos < NumAudio; Pos++)
      for (uint8_t Pos2 = 0; Pos2<SubIdx+1; Pos2+=2)
        if (ActiveExSSMasks[Pos]%2)
          get_bits(gb, 8);                            /* Active ExSS masks 2 */
    if (get_bits(gb, 1)) {                            /* Mix Metadata Flag */
      get_bits(gb, 2);                                /* Mix Metadata Adjustment Level */
      unsigned bits = get_bits(gb, 2);                /* Bits4Mix Mask */
      bits = 4 + bits * 4;
      unsigned num  = get_bits(gb, 2) + 1;            /* Num Configs */
      for (uint8_t Pos = 0; Pos < num; Pos++)
        get_bits(gb, bits);                           /* Mix Out Ch Mask */
    }
  }                                                   /* END: Static Fields */
  // Sizes
  for (uint8_t Pos = 0; Pos < NumAssets; Pos++) {
    if (blownUp) {
      get_bits(gb, 20);                               /* Size */
    } else {
      get_bits(gb, 16);                               /* Size */
    }
  }

  for (uint8_t Pos = 0; Pos < NumAssets; Pos++) {
    get_bits(gb, 9);                                  /* Asset Size */
    get_bits(gb, 3);                                  /* Asset Descriptor Data */
    if (staticFields) {
      if (get_bits(gb, 1))                            /* Asset type descriptor present */
        get_bits(gb, 4);                              /* Asset type descriptor */
      if (get_bits(gb, 1))                            /* Language descriptor present */
        get_bits(gb, 24);                             /* Language descriptor */
      if (get_bits(gb, 1)) {                          /* Info text present */
        unsigned bytes = get_bits(gb, 10) + 1;        /* Info text size */
        for (unsigned Pos = 0; Pos < bytes; Pos++)
          get_bits(gb, 8);                            /* Info Text */
      }
      get_bits(gb, 5);                                /* Bit Resolution */
      get_bits(gb, 4);                                /* Maximum Sample Rate */
      pHeader->HDTotalChannels = get_bits(gb, 8) + 1; /* Total Number of Channels */
      if (get_bits(gb, 1)) {                          /* 1 to 1 speaker map present */
        unsigned SpeakerActivityMaskBits = 0, SpeakerRemapSetsCount;
        if (pHeader->HDTotalChannels > 2)
          get_bits(gb, 1);                            /* Embedded stereo flag */
        if (pHeader->HDTotalChannels > 6)
          get_bits(gb, 1);                            /* Embedded 6 channels flag */
        if (get_bits(gb, 1)) {                        /* Speaker Mask Flag */
          SpeakerActivityMaskBits = get_bits(gb, 2);  /* Speaker mask bits */
          SpeakerActivityMaskBits = 4 + SpeakerActivityMaskBits * 4;
          pHeader->HDSpeakerMask = get_bits(gb, SpeakerActivityMaskBits);  /* Speaker activity Mask */
        }
        SpeakerRemapSetsCount = get_bits(gb, 3);      /* Speaker remap sets count */
        for (uint8_t Pos = 0; Pos < SpeakerRemapSetsCount; Pos++)
          get_bits(gb, SpeakerActivityMaskBits);      /* Standard Speaker Layout mask */
        /* unfinished
        for (uint8_t Pos = 0; Pos < SpeakerRemapSetsCount; Pos++)
        get_bits(gb, 5);                            /* NumDecCh4Remap
        */
      }
    }
  }

  return 0;
}

int parse_dts_header(DTSParserContext *pContext, DTSHeader *pHeader, uint8_t *pBuffer, unsigned uSize)
{
  if(!pContext) return -1;
  if(!pHeader) return -1;

  unsigned ExtDescriptor = 0, ExtCoding = 0;

  uint8_t dts_buffer[32 + FF_INPUT_BUFFER_PADDING_SIZE] = {0};
  int ret = ff_dca_convert_bitstream(pBuffer, uSize, dts_buffer, 32);

  bool is16be = (AV_RB32(pBuffer) == DCA_MARKER_RAW_BE);

  /* Parse Core Header */
  if (ret >= 0) {
    pHeader->HasCore = 1;

    GetBitContext *gb = pContext->gb;
    init_get_bits(gb, dts_buffer, 32 << 3);

    skip_bits_long(gb, 32);                             /* Sync code */
    skip_bits1(gb);                                     /* Frame type */
    pHeader->SamplesPerBlock  = get_bits(gb, 5) + 1;    /* Samples deficit */
    pHeader->CRCPresent       = get_bits1(gb);          /* CRC present */
    pHeader->Blocks           = get_bits(gb, 7) + 1;    /* Number of Blocks */
    pHeader->FrameSize        = get_bits(gb, 14) + 1;   /* Primary (core) Frame Size */
    pHeader->ChannelLayout    = get_bits(gb, 6);        /* Channel configuration */
    unsigned sample_index     = get_bits(gb, 4);        /* Sample frequency index */
    pHeader->SampleRate       = dca_sample_rates[sample_index];
    unsigned bitrate_index    = get_bits(gb, 5);        /* Bitrate index */
    pHeader->Bitrate          = dca_bit_rates[bitrate_index];
    skip_bits1(gb);                                     /* Down mix */
    skip_bits1(gb);                                     /* Dynamic range */
    skip_bits1(gb);                                     /* Time stamp */
    skip_bits1(gb);                                     /* Auxiliary data */
    skip_bits1(gb);                                     /* HDCD */
    ExtDescriptor             = get_bits(gb, 3);        /* External descriptor  */
    ExtCoding                 = get_bits1(gb);          /* Extended coding */
    skip_bits1(gb);                                     /* ASPF */
    pHeader->LFE              = get_bits(gb, 2);        /* LFE */
    skip_bits1(gb);                                     /* Predictor History */
    if(pHeader->CRCPresent)
      skip_bits(gb, 16);                                /* CRC */
    skip_bits1(gb);                                     /* Multirate Interpolator */
    skip_bits(gb, 4);                                   /* Encoder Software Revision */
    skip_bits(gb, 2);                                   /* Copy history */
    skip_bits1(gb);                                     /* ES */
    skip_bits(gb, 4);                                   /* Dialog Normalization Parameter */
    skip_bits(gb, 4);                                   /* Unknown or Dialog Normalization Parameter */
  } else {
    pHeader->HasCore = 0;
  }

  if (pHeader->HasCore && !is16be)
    return 0;

  // DTS-HD parsing
  const uint8_t *pHD = NULL;
  if (pHeader->HasCore) { // If we have a core, only search after the normal buffer
    if (uSize > (pHeader->FrameSize + 4)) { // at least 4 bytes extra, could probably insert a minimal size of a HD header, but so what
      pHD = find_marker32_position(pBuffer + pHeader->FrameSize, uSize - pHeader->FrameSize, DCA_HD_MARKER);
    }
  } else {
    pHD = find_marker32_position(pBuffer, uSize, DCA_HD_MARKER);
  }
  if (pHD) {
    pHeader->IsHD = 1;
    size_t remaining = uSize - (pHD - pBuffer);
    parse_dts_hd_header(pContext, pHeader, pHD, (unsigned)remaining);

    const uint8_t *pXChHD = find_marker32_position(pHD, remaining, DCA_XCH_MARKER);
    if (pXChHD) {
      size_t remaining = uSize - (pXChHD - pBuffer);
      parse_dts_xch_hd_header(pContext, pHeader, pXChHD, (unsigned)remaining);
    }

    const uint8_t *pXXChHD = find_marker32_position(pHD, remaining, DCA_XXCH_MARKER);
    if (pXXChHD) {
      size_t remaining = uSize - (pXXChHD - pBuffer);
      parse_dts_xxch_hd_header(pContext, pHeader, pXXChHD, (unsigned)remaining);
    }
  }

  // Handle DTS extensions
  if (ExtCoding) {
    size_t coreSize = pHD ? (pHD - pBuffer) : uSize;
    if (ExtDescriptor == 0 || ExtDescriptor == 3) {
      const uint8_t *pXCh = find_marker32_position(pBuffer, coreSize, DCA_XCH_MARKER);
      if (pXCh) {
        size_t remaining = coreSize - (pXCh - pBuffer);
        parse_dts_xch_header(pContext, pHeader, pXCh, (unsigned)remaining);
      }
    }
    if (ExtDescriptor == 6) {
      const uint8_t *pXXCh = find_marker32_position(pBuffer, coreSize, DCA_XXCH_MARKER);
      if (pXXCh) {
        size_t remaining = coreSize - (pXXCh - pBuffer);
        parse_dts_xxch_header(pContext, pHeader, pXXCh, (unsigned)remaining);
      }
    }
  }

  return 0;
}
