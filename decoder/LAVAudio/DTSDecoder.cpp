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

typedef void* (*DtsOpen)();
typedef int (*DtsClose)(void *context);
typedef int (*DtsReset)();
typedef int (*DtsSetParam)(void *context, int channels, int bitdepth, int unk1, int unk2, int unk3);
typedef int (*DtsDecode)(void *context, BYTE *pInput, int len, BYTE *pOutput, int unk1, int unk2, int *pBitdepth, int *pChannels, int *pCoreSampleRate, int *pUnk4, int *pHDSampleRate, int *pUnk5, int *pProfile);

struct DTSDecoder {
  DtsOpen pDtsOpen;
  DtsClose pDtsClose;
  DtsReset pDtsReset;
  DtsSetParam pDtsSetParam;
  DtsDecode pDtsDecode;

  void *dtsContext;

  DTSDecoder() : pDtsOpen(NULL), pDtsClose(NULL), pDtsReset(NULL), pDtsSetParam(NULL), pDtsDecode(NULL), dtsContext(NULL) {}
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

  m_DTSBitDepth = 24;
  m_DTSDecodeChannels = 8;

  m_pExtraDecoderContext = context;

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
  if (m_pExtraDecoderContext) {
    DTSDecoder *context = (DTSDecoder *)m_pExtraDecoderContext;
    delete context;
    m_pExtraDecoderContext = NULL;
  }

  return S_OK;
}

HRESULT CLAVAudio::FlushDTSDecoder()
{
  if (m_nCodecId == CODEC_ID_DTS && m_pExtraDecoderContext) {
    DTSDecoder *context = (DTSDecoder *)m_pExtraDecoderContext;

    context->pDtsReset();
    context->pDtsSetParam(context->dtsContext, m_DTSDecodeChannels, m_DTSBitDepth, 0, 0, 0);
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
  channels += header.XChChannelLayout;
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
    decode_channels = 2;
    break;
  case 1:
  case 3:
  case 4:
  case 5:
    decode_channels = 6;
    break;
  case 6:
    if (header.ChannelLayout == 9 && !header.XChChannelLayout)
      decode_channels = 6;
    else
      decode_channels = 7;
    break;
  case 7:
    decode_channels = 7;
    break;
  case 8:
    decode_channels = 8;
    break;
  }
  return decode_channels;
}

HRESULT CLAVAudio::DecodeDTS(const BYTE * const p, int buffsize, int &consumed, BufferDetails *out)
{
  CheckPointer(out, E_POINTER);
  int nPCMLength	= 0;
  const BYTE *pDataInBuff = p;

  BOOL bEOF = (buffsize == -1);
  if (buffsize == -1) buffsize = 1;

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
      m_bUpdateTimeCache = FALSE;
    }

    if (!bEOF && used_bytes > 0) {
      buffsize -= used_bytes;
      pDataInBuff += used_bytes;
      consumed += used_bytes;
    }

    if (pOut_size > 0) {
      COPY_TO_BUFFER(pOut, pOut_size);

      DTSDecoder *dtsDec = (DTSDecoder *)m_pExtraDecoderContext;

      // Parse DTS headers
      m_bsParser.Parse(CODEC_ID_DTS, m_pFFBuffer, pOut_size, NULL);
      unsigned decode_channels = dts_determine_decode_channels(m_bsParser.m_DTSHeader);

      // Init Decoder with new Parameters, if required
      if (m_DTSDecodeChannels != decode_channels) {
        DbgLog((LOG_TRACE, 20, L"::Decode(): Switching to %d channel decoding", decode_channels));
        dtsDec->pDtsSetParam(dtsDec->dtsContext, decode_channels, m_DTSBitDepth, 0, 0, 0);
        m_DTSDecodeChannels = decode_channels;
      }

      int bitdepth, channels, CoreSampleRate, HDSampleRate, profile;
      int unk4 = 0, unk5 = 0;
      nPCMLength = dtsDec->pDtsDecode(dtsDec->dtsContext, m_pFFBuffer, pOut_size, m_pPCMData, 0, 8, &bitdepth, &channels, &CoreSampleRate, &unk4, &HDSampleRate, &unk5, &profile);
      if (nPCMLength > 0 && bitdepth != m_DTSBitDepth) {
        int decodeBits = bitdepth > 16 ? 24 : 16;

        // If the bit-depth changed, instruct the DTS Decoder to decode to the new bit depth, and decode the previous sample again.
        if (decodeBits != m_DTSBitDepth && out->bBuffer->GetCount() == 0) {
          DbgLog((LOG_TRACE, 20, L"::Decode(): The DTS decoder indicated that it outputs %d bits, changing config to %d bits to compensate", bitdepth, decodeBits));
          m_DTSBitDepth = decodeBits;

          dtsDec->pDtsSetParam(dtsDec->dtsContext, 8, m_DTSBitDepth, 0, 0, 0);
          nPCMLength = dtsDec->pDtsDecode(dtsDec->dtsContext, m_pFFBuffer, pOut_size, m_pPCMData, 0, 8, &bitdepth, &channels, &CoreSampleRate, &unk4, &HDSampleRate, &unk5, &profile);
        }
      }
      if (nPCMLength <= 0) {
        DbgLog((LOG_TRACE, 50, L"::Decode() - DTS decoding failed"));
        m_bQueueResync = TRUE;
        continue;
      }

      m_bUpdateTimeCache = TRUE;

      out->wChannels        = channels;
      out->dwSamplesPerSec  = HDSampleRate;
      out->sfFormat         = m_DTSBitDepth == 24 ? SampleFormat_24 : SampleFormat_16;
      out->dwChannelMask    = get_channel_mask(channels); // TODO
      out->wBitsPerSample   = bitdepth;

      // TODO: get rid of these
      m_pAVCtx->channels    = channels;
      m_pAVCtx->sample_rate = HDSampleRate;
      m_pAVCtx->profile     = profile;

      // Set long-time cache to the first timestamp encountered, used on MPEG-TS containers
      // If the current timestamp is not valid, use the last delivery timestamp in m_rtStart
      if (m_rtStartCacheLT == AV_NOPTS_VALUE) {
        if (m_rtStartInputCache == AV_NOPTS_VALUE) {
          DbgLog((LOG_CUSTOM5, 20, L"WARNING: m_rtStartInputCache is invalid, using calculated rtStart"));
        }
        m_rtStartCacheLT = m_rtStartInputCache != AV_NOPTS_VALUE ? m_rtStartInputCache : m_rtStart;
      }
    }

    // Append to output buffer
    if (nPCMLength > 0) {
      out->bBuffer->Append(m_pPCMData, nPCMLength);
    }
  }

  if (out->bBuffer->GetCount() <= 0) {
    return S_FALSE;
  }

  out->nSamples = out->bBuffer->GetCount() / get_byte_per_sample(out->sfFormat) / out->wChannels;
  m_DecodeFormat = out->sfFormat;

  return S_OK;
}
