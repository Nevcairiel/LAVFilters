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

  context->pDtsSetParam(context->dtsContext, 8, 24, 0, 0, 0);
  m_iDTSBitDepth = 24;

  m_pExtraDecoderContext = context;

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
    context->pDtsSetParam(context->dtsContext, 8, m_iDTSBitDepth, 0, 0, 0);
  }

  return S_OK;
}

HRESULT CLAVAudio::DecodeDTS(BYTE *pPCMData, int *nPCMLength, AVPacket *pkt, const BufferDetails *out)
{
  m_bsParser.Parse(CODEC_ID_DTS, pkt->data, pkt->size, NULL);

  DTSDecoder *dtsDec = (DTSDecoder *)m_pExtraDecoderContext;
  int bitdepth, channels, CoreSampleRate, HDSampleRate, profile;
  int unk4 = 0, unk5 = 0;
  *nPCMLength = dtsDec->pDtsDecode(dtsDec->dtsContext, pkt->data, pkt->size, pPCMData, 0, 8, &bitdepth, &channels, &CoreSampleRate, &unk4, &HDSampleRate, &unk5, &profile);
  if (*nPCMLength > 0 && bitdepth != m_iDTSBitDepth) {
    int decodeBits = bitdepth > 16 ? 24 : 16;

    // If the bit-depth changed, instruct the DTS Decoder to decode to the new bit depth, and decode the previous sample again.
    if (decodeBits != m_iDTSBitDepth && out->bBuffer->GetCount() == 0) {
      DbgLog((LOG_TRACE, 20, L"::Decode(): The DTS decoder indicated that it outputs %d bits, change config to %d bits to compensate", bitdepth, decodeBits));
      m_iDTSBitDepth = decodeBits;

      dtsDec->pDtsSetParam(dtsDec->dtsContext, 8, m_iDTSBitDepth, 0, 0, 0);
      *nPCMLength = dtsDec->pDtsDecode(dtsDec->dtsContext, pkt->data, pkt->size, pPCMData, 0, 8, &bitdepth, &channels, &CoreSampleRate, &unk4, &HDSampleRate, &unk5, &profile);
    }
  }

  if (*nPCMLength == 0)
    return E_FAIL;

  m_pAVCtx->sample_rate = HDSampleRate;
  m_pAVCtx->channels = channels;
  m_pAVCtx->profile = profile;

  return S_OK;
}
