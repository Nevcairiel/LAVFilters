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

#include "stdafx.h"
#include "LAVAudio.h"
#include "PostProcessor.h"
#include "Media.h"

#include <Shlwapi.h>

#pragma warning( push )
#pragma warning( disable : 4305 )
#include "libavcodec/dcadata.h"
#pragma warning( pop )

/** Taken from ffmpeg libavcodec/dca.c */
static const int64_t dca_core_channel_layout[] = {
  AV_CH_FRONT_CENTER,                                                      ///< 1, A
  AV_CH_LAYOUT_STEREO,                                                     ///< 2, A + B (dual mono)
  AV_CH_LAYOUT_STEREO,                                                     ///< 2, L + R (stereo)
  AV_CH_LAYOUT_STEREO,                                                     ///< 2, (L+R) + (L-R) (sum-difference)
  AV_CH_LAYOUT_STEREO,                                                     ///< 2, LT +RT (left and right total)
  AV_CH_LAYOUT_STEREO|AV_CH_FRONT_CENTER,                                  ///< 3, C+L+R
  AV_CH_LAYOUT_STEREO|AV_CH_BACK_CENTER,                                   ///< 3, L+R+S
  AV_CH_LAYOUT_STEREO|AV_CH_FRONT_CENTER|AV_CH_BACK_CENTER,                ///< 4, C + L + R+ S
  AV_CH_LAYOUT_STEREO|AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT,                    ///< 4, L + R +SL+ SR
  AV_CH_LAYOUT_STEREO|AV_CH_FRONT_CENTER|AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT, ///< 5, C + L + R+ SL+SR
  AV_CH_LAYOUT_STEREO|AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER,                    ///< 6, CL + CR + L + R + SL + SR
  AV_CH_LAYOUT_STEREO|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT|AV_CH_FRONT_CENTER|AV_CH_BACK_CENTER,                                      ///< 6, C + L + R+ LR + RR + OV
  AV_CH_FRONT_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_BACK_CENTER|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT,   ///< 6, CF+ CR+LF+ RF+LR + RR
  AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER|AV_CH_LAYOUT_STEREO|AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT, ///< 7, CL + C + CR + L + R + SL + SR
  AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER|AV_CH_LAYOUT_STEREO|AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT, ///< 8, CL + CR + L + R + SL1 + SL2+ SR1 + SR2
  AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER|AV_CH_LAYOUT_STEREO|AV_CH_SIDE_LEFT|AV_CH_BACK_CENTER|AV_CH_SIDE_RIGHT, ///< 8, CL + C+ CR + L + R + SL + S+ SR
};

typedef void* (*DtsOpen)();
typedef int (*DtsClose)(void *context);
typedef int (*DtsReset)(void *context);
typedef int (*DtsSetParam)(void *context, int channels, int bitdepth, int unk1, int unk2, int unk3);
typedef int (*DtsDecode)(void *context, BYTE *pInput, int len, BYTE *pOutput, int unk1, int unk2, int *pBitdepth, int *pChannels, int *pCoreSampleRate, int *pUnk4, int *pHDSampleRate, int *pUnk5, int *pProfile);

struct DTSDecoder {
  DtsOpen pDtsOpen;
  DtsClose pDtsClose;
  DtsReset pDtsReset;
  DtsSetParam pDtsSetParam;
  DtsDecode pDtsDecode;

  void *dtsContext;
  BYTE *dtsPCMBuffer;

  DTSDecoder() : pDtsOpen(NULL), pDtsClose(NULL), pDtsReset(NULL), pDtsSetParam(NULL), pDtsDecode(NULL), dtsContext(NULL), dtsPCMBuffer(NULL) {}
  ~DTSDecoder() {
    if (pDtsClose && dtsContext) {
      pDtsClose(dtsContext);
    }
  }
};

HRESULT CLAVAudio::InitDTSDecoder()
{
  if (!m_hDllExtraDecoder) {
    // Add path of LAVAudio.ax into the Dll search path
    WCHAR wModuleFile[1024];
    GetModuleFileName(g_hInst, wModuleFile, 1024);
    PathRemoveFileSpecW(wModuleFile);
    SetDllDirectory(wModuleFile);

    HMODULE hDll = LoadLibrary(TEXT("dtsdecoderdll.dll"));
    CheckPointer(hDll, E_FAIL);

    m_hDllExtraDecoder = hDll;
  }

  DTSDecoder *context = new DTSDecoder();

  context->pDtsOpen = (DtsOpen)GetProcAddress(m_hDllExtraDecoder, "DtsApiDecOpen");
  if(!context->pDtsOpen) goto fail;

  context->pDtsClose = (DtsClose)GetProcAddress(m_hDllExtraDecoder, "DtsApiDecClose");
  if(!context->pDtsClose) goto fail;

  context->pDtsReset = (DtsReset)GetProcAddress(m_hDllExtraDecoder, "DtsApiDecReset");
  if(!context->pDtsReset) goto fail;

  context->pDtsSetParam = (DtsSetParam)GetProcAddress(m_hDllExtraDecoder, "DtsApiDecSetParam");
  if(!context->pDtsSetParam) goto fail;

  context->pDtsDecode = (DtsDecode)GetProcAddress(m_hDllExtraDecoder, "DtsApiDecodeData");
  if(!context->pDtsDecode) goto fail;

  context->dtsContext = context->pDtsOpen();
  if(!context->dtsContext) goto fail;

  context->dtsPCMBuffer = (BYTE *)av_mallocz(LAV_AUDIO_BUFFER_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);

  m_DTSBitDepth = 24;
  m_DTSDecodeChannels = 8;

  m_pDTSDecoderContext = context;

  FlushDTSDecoder();

  return S_OK;
fail:
  SAFE_DELETE(context);
  FreeLibrary(m_hDllExtraDecoder);
  m_hDllExtraDecoder = NULL;
  return E_FAIL;
}

HRESULT CLAVAudio::FreeDTSDecoder()
{
  if (m_pDTSDecoderContext)
    av_freep(&m_pDTSDecoderContext->dtsPCMBuffer);
  SAFE_DELETE(m_pDTSDecoderContext);
  return S_OK;
}

HRESULT CLAVAudio::FlushDTSDecoder(BOOL bReopen)
{
  if (m_pDTSDecoderContext) {
    if(bReopen) {
      m_pDTSDecoderContext->pDtsClose(m_pDTSDecoderContext->dtsContext);
      m_pDTSDecoderContext->dtsContext = m_pDTSDecoderContext->pDtsOpen();
    }
    m_pDTSDecoderContext->pDtsReset(m_pDTSDecoderContext->dtsContext);
    m_pDTSDecoderContext->pDtsSetParam(m_pDTSDecoderContext->dtsContext, m_DTSDecodeChannels, m_DTSBitDepth, 0, 0, 0);
  }

  return S_OK;
}

static unsigned dts_header_get_channels(DTSHeader header)
{
  if (header.IsHD && header.HDTotalChannels)
    return header.HDTotalChannels;

  if (header.ChannelLayout > 15) // user-definied layouts
    return 8;

  unsigned channels = dca_channels[header.ChannelLayout];
  if (header.XChChannelLayout)
    channels++;
  if (header.LFE)
    channels++;
  return channels;
}

static unsigned dts_determine_decode_channels(DTSHeader header)
{
  unsigned coded_channels = dts_header_get_channels(header);
  unsigned decode_channels;
  switch(coded_channels) {
  case 2:
  case 7:
  case 8:
    decode_channels = coded_channels;
    break;
  case 1:
  case 3:
  case 4:
  case 5:
    decode_channels = 6;
    break;
  case 6:
    if ((header.ChannelLayout == 9 || header.ChannelLayout == 63) && header.LFE)  // Layout 9 is 5.0, with LFE makes default 5.1, nothing special
      decode_channels = 6;
    else                                          // Other possibility for 6 channels is 6.0
      decode_channels = 7;
    break;
  default:
    DbgLog((LOG_TRACE, 10, L"DTSDecoder(): Unknown number of channels - %d!", coded_channels));
    decode_channels = 8;
    break;
  }
  return decode_channels;
}

static void DTSRemapOutputChannels(BufferDetails *buffer, DTSHeader header)
{
  const unsigned channels = dts_header_get_channels(header);
  if (channels == 1 && buffer->wChannels == 6) {                  /* DTS 1.1.0.0 produces 6 channels, with Mono in the center */
    ChannelMap map = {2};
    ChannelMapping(buffer, 1, map);
  } else if (channels == 1 && buffer->wChannels == 2) {           /* DTS 1.1.0.8 produces 2 channels, with Mono in both L/R */
    // Take the left channel, and increase volume (reduction from 2 channels)
    ExtendedChannelMap map = {{0, 2}};
    ExtendedChannelMapping(buffer, 1, map);
  } else if (channels == 3) {                                     /* --- 3 Channel Formats --- */
    if (header.ChannelLayout == 6) {                              /* 2/1/0 Layout, L+R and BC mixed into BL/BR */
      ExtendedChannelMap map = {{0, 0}, {1, 0}, {4, 2}};
      ExtendedChannelMapping(buffer, 3, map);
    } else {
      ChannelMap map = {0, 1, 2};
      ChannelMapping(buffer, 3, map);
    }
  } else if (channels == 4) {                                     /* --- 4 Channel Formats --- */
    if (header.ChannelLayout == 6) {                              /* 2/1/1 Layout, L+R+LFE and BC mixed into BL/BR */
      ExtendedChannelMap map = {{0, 0}, {1, 0}, {3, 0}, {4, 2}};
      ExtendedChannelMapping(buffer, 4, map);
    } else if (header.ChannelLayout == 8) {                       /* 2/2/0 Layout, L+R+BL+BR */
      ChannelMap map = {0, 1, 4, 5};
      ChannelMapping(buffer, 4, map);
    } else if (header.ChannelLayout == 7) {                       /* 3/1/0 Layout, L+R+C and BC mixed into BL/BR */
      ExtendedChannelMap map = {{0, 0}, {1, 0}, {2, 0}, {4, 2}};
      ExtendedChannelMapping(buffer, 4, map);
    } else {
      ChannelMap map = {0, 1, 2, 3};
      ChannelMapping(buffer, 4, map);
    }
  } else if (channels == 5) {                                     /* --- 5 Channel Formats --- */
    if (header.ChannelLayout == 8) {                              /* 2/2/1 Layout, L+R+LFE+BL+BR */
      ChannelMap map = {0, 1, 3, 4, 5};
      ChannelMapping(buffer, 5, map);
    } else if (header.ChannelLayout == 7) {                       /* 3/1/1 Layout, L+R+C+LFE and BC mixed into BL/BR */
      ExtendedChannelMap map = {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 2}};
      ExtendedChannelMapping(buffer, 5, map);
    } else if (header.ChannelLayout == 9) {                       /* 3/2/0 Layout, L+R+C+BL+BR */
      ChannelMap map = {0, 1, 2, 4, 5};
      ChannelMapping(buffer, 5, map);
    } else {
      ChannelMap map = {0, 1, 2, 3, 4};
      ChannelMapping(buffer, 5, map);
    }
  } else if (channels == 6) {
    if (buffer->wChannels == 7) {                                 /* 3/3/0 Layout, DTS 1.1.0.0 - packed into 7 channels, empty LFE */
      ChannelMap map = {0, 1, 2, 4, 5, 6};
      ChannelMapping(buffer, 6, map);
    } else if (header.ChannelLayout == 9 && !header.LFE && header.XChChannelLayout) {
      ChannelMap map = {0, 1, 2, 4, 5};
      ChannelMapping(buffer, 5, map);
    }
  } else if (channels == 7 && buffer->wChannels == 8 && header.LFE) {       /* 3/3/1 Layout, DTS 1.1.0.8 - packed into 8 channels, BC in BL */
    ChannelMap map = {0, 1, 2, 3, 6, 7, 4};
    ChannelMapping(buffer, 7, map);
  }

  // Assign appropriate channel mask
  if (buffer->wChannels > 6) {
    if (buffer->wChannels == 7 && header.LFE)                               /* 3/3/1 (6.1) Layout */
      buffer->dwChannelMask = AV_CH_LAYOUT_5POINT1_BACK|AV_CH_BACK_CENTER;
    else if (buffer->wChannels == 7)                                        /* 3/4/0 (7.0) Layout */
      buffer->dwChannelMask = AV_CH_LAYOUT_7POINT0;
    else                                                                    /* 3/4/1 (7.1) Layout */
      buffer->dwChannelMask = AV_CH_LAYOUT_7POINT1;
  } else if (buffer->wChannels == 6 && header.ChannelLayout == 9 && !header.LFE) { /* 3/3/0 (6.0) Layout */
    buffer->dwChannelMask = AV_CH_LAYOUT_5POINT0_BACK|AV_CH_BACK_CENTER;
  } else {
    buffer->dwChannelMask = (DWORD)dca_core_channel_layout[header.ChannelLayout];
    if(header.LFE)
      buffer->dwChannelMask |= AV_CH_LOW_FREQUENCY;
  }
}

int CLAVAudio::SafeDTSDecode(BYTE *pInput, int len, BYTE *pOutput, int unk1, int unk2, int *pBitdepth, int *pChannels, int *pCoreSampleRate, int *pUnk4, int *pHDSampleRate, int *pUnk5, int *pProfile)
{
  int nPCMLen = 0;
  __try {
    nPCMLen = m_pDTSDecoderContext->pDtsDecode(m_pDTSDecoderContext->dtsContext, pInput, len, pOutput, unk1, unk2, pBitdepth, pChannels, pCoreSampleRate, pUnk4, pHDSampleRate, pUnk5, pProfile);
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    DbgLog((LOG_TRACE, 50, L"::Decode() - DTS Decoder threw an exception"));
    nPCMLen = 0;
  }
  return nPCMLen;
};

HRESULT CLAVAudio::DecodeDTS(const BYTE * const p, int buffsize, int &consumed, HRESULT *hrDeliver)
{
  HRESULT hr = S_FALSE;
  int nPCMLength	= 0;
  const BYTE *pDataInBuff = p;

  BOOL bEOF = (buffsize == -1);
  if (buffsize == -1) buffsize = 1;

  BufferDetails out;

  consumed = 0;
  while (buffsize > 0) {
    nPCMLength = 0;
    if (bEOF) buffsize = 0;
    else {
      COPY_TO_BUFFER(pDataInBuff, buffsize);
    }

    ASSERT(m_pParser);

    BYTE *pOut = NULL;
    int pOut_size = 0;
    int used_bytes = av_parser_parse2(m_pParser, m_pAVCtx, &pOut, &pOut_size, m_pFFBuffer, buffsize, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    if (used_bytes < 0) {
      return E_FAIL;
    } else if(used_bytes == 0 && pOut_size == 0) {
      DbgLog((LOG_TRACE, 50, L"::Decode() - could not process buffer, starving?"));
      break;
    }

    // Timestamp cache to compensate for one frame delay the parser might introduce, in case the frames were already perfectly sliced apart
    // If we used more (or equal) bytes then was output again, we encountered a new frame, update timestamps
    if (used_bytes >= pOut_size && m_bUpdateTimeCache) {
      m_rtStartInputCache = m_rtStartInput;
      m_rtStopInputCache = m_rtStopInput;
      m_rtStartInput = m_rtStopInput = AV_NOPTS_VALUE;
      m_bUpdateTimeCache = FALSE;
    }

    if (!bEOF && used_bytes > 0) {
      buffsize -= used_bytes;
      pDataInBuff += used_bytes;
      consumed += used_bytes;
    }

    if (pOut_size > 0) {
      COPY_TO_BUFFER(pOut, pOut_size);

      // Parse DTS headers
      m_bsParser.Parse(CODEC_ID_DTS, m_pFFBuffer, pOut_size, NULL);
      unsigned decode_channels = dts_determine_decode_channels(m_bsParser.m_DTSHeader);

      // Init Decoder with new Parameters, if required
      if (m_DTSDecodeChannels != decode_channels) {
        DbgLog((LOG_TRACE, 20, L"::Decode(): Switching to %d channel decoding", decode_channels));
        m_DTSDecodeChannels = decode_channels;

        FlushDTSDecoder();
      }

      int bitdepth = 0, channels = 0, CoreSampleRate = 0, HDSampleRate = 0, profile = 0;
      int unk4 = 0, unk5 = 0;
      nPCMLength = SafeDTSDecode(m_pFFBuffer, pOut_size, m_pDTSDecoderContext->dtsPCMBuffer, 0, 8, &bitdepth, &channels, &CoreSampleRate, &unk4, &HDSampleRate, &unk5, &profile);
      if (nPCMLength > 0 && bitdepth != m_DTSBitDepth) {
        int decodeBits = bitdepth > 16 ? 24 : 16;

        // If the bit-depth changed, instruct the DTS Decoder to decode to the new bit depth, and decode the previous sample again.
        if (decodeBits != m_DTSBitDepth && out.bBuffer->GetCount() == 0) {
          DbgLog((LOG_TRACE, 20, L"::Decode(): The DTS decoder indicated that it outputs %d bits, changing config to %d bits to compensate", bitdepth, decodeBits));
          m_DTSBitDepth = decodeBits;

          FlushDTSDecoder();
          nPCMLength = SafeDTSDecode(m_pFFBuffer, pOut_size, m_pDTSDecoderContext->dtsPCMBuffer, 0, 2, &bitdepth, &channels, &CoreSampleRate, &unk4, &HDSampleRate, &unk5, &profile);
        }
      }
      if (nPCMLength <= 0) {
        DbgLog((LOG_TRACE, 50, L"::Decode() - DTS decoding failed"));
        FlushDTSDecoder(TRUE);
        m_bQueueResync = TRUE;
        continue;
      }

      out.wChannels        = channels;
      out.dwSamplesPerSec  = HDSampleRate;
      out.sfFormat         = m_DTSBitDepth == 24 ? SampleFormat_24 : SampleFormat_16;
      out.dwChannelMask    = get_channel_mask(channels); // TODO
      out.wBitsPerSample   = bitdepth;

      // DTS Express
      if (profile == 0 && !m_bsParser.m_DTSHeader.HasCore) {
        profile = 1 << 7;
      }

      // TODO: get rid of these
      m_pAVCtx->sample_rate = HDSampleRate;
      m_pAVCtx->profile     = profile;

      // Send current input time to the delivery function
      out.rtStart = m_rtStartInputCache;
      m_rtStartInputCache = AV_NOPTS_VALUE;
      m_bUpdateTimeCache = TRUE;
    }

    // Send to Output
    if (nPCMLength > 0) {
      hr = S_OK;
      out.bBuffer->Append(m_pDTSDecoderContext->dtsPCMBuffer, nPCMLength);
      out.nSamples = out.bBuffer->GetCount() / get_byte_per_sample(out.sfFormat) / out.wChannels;

      if (m_pAVCtx->profile != (1 << 7)) {
        DTSRemapOutputChannels(&out, m_bsParser.m_DTSHeader);
      }

      m_pAVCtx->channels = out.wChannels;
      m_DecodeFormat = out.sfFormat;
      m_DecodeLayout = out.dwChannelMask;

      if (SUCCEEDED(PostProcess(&out))) {
        *hrDeliver = QueueOutput(out);
        if (FAILED(*hrDeliver)) {
          return S_FALSE;
        }
      }
    }
  }

  return hr;
}
