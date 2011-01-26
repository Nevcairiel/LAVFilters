
#include "stdafx.h"
#include "LAVCAudio.h"

#include <MMReg.h>

#include "moreuuids.h"

typedef struct {
  const CLSID*        clsMinorType;
  const enum CodecID  nFFCodec;
} FFMPEG_SUBTYPE_MAP;

// Map Media Subtype <> FFMPEG Codec Id
FFMPEG_SUBTYPE_MAP lavc_audio_codecs[] = {
  { &MEDIASUBTYPE_AAC,          CODEC_ID_AAC },
  { &MEDIASUBTYPE_LATM_AAC,     CODEC_ID_AAC_LATM },
  { &MEDIASUBTYPE_DOLBY_TRUEHD, CODEC_ID_TRUEHD },
  { &MEDIASUBTYPE_MP3,          CODEC_ID_MP3 },
};

// Define Input Media Types
const AMOVIESETUP_MEDIATYPE CLAVCAudio::sudPinTypesIn[] = {
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_AAC },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_LATM_AAC },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_DOLBY_TRUEHD },
  { &MEDIATYPE_Audio, &MEDIASUBTYPE_MP3 },
};
const int CLAVCAudio::sudPinTypesInCount = countof(CLAVCAudio::sudPinTypesIn);

// Define Output Media Types
const AMOVIESETUP_MEDIATYPE CLAVCAudio::sudPinTypesOut[] = {
	{ &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM },
};
const int CLAVCAudio::sudPinTypesOutCount = countof(CLAVCAudio::sudPinTypesOut);

// Crawl the lavc_audio_codecs array for the proper codec
CodecID FindCodecId(const CMediaType *mt)
{
  for (int i=0; i<countof(lavc_audio_codecs); ++i) {
    if (mt->subtype == *lavc_audio_codecs[i].clsMinorType) {
      return lavc_audio_codecs[i].nFFCodec;
    }
  }
  return CODEC_ID_NONE;
}

// Default Channel to Speaker Map
scmap_t m_scmap_default[] = {
  //    FL  FR  FC  LFe BL  BR  FLC FRC
  {1, { 0,-1,-1,-1,-1,-1,-1,-1 }, 0},		// Mono			M1, 0
  {2, { 0, 1,-1,-1,-1,-1,-1,-1 }, 0},		// Stereo		FL, FR
  {3, { 0, 1, 2,-1,-1,-1,-1,-1 }, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER},															// 3/0			FL, FR, FC
  {4, { 0, 1, 2, 3,-1,-1,-1,-1 }, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY},										// 3/1			FL, FR, FC, Surround
  {5, { 0, 1, 2, 3, 4,-1,-1,-1 }, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT},						// 3/2			FL, FR, FC, BL, BR
  {6, { 0, 1, 2, 3, 4, 5,-1,-1 }, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT},// 3/2+LFe		FL, FR, FC, BL, BR, LFe
  {7, { 0, 1, 2, 3, 4, 5, 6,-1 }, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_SIDE_LEFT|SPEAKER_SIDE_RIGHT|SPEAKER_BACK_CENTER},	// 3/4			FL, FR, FC, BL, Bls, Brs, BR
  {8, { 0, 1, 2, 3, 6, 7, 4, 5 }, SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_SIDE_LEFT|SPEAKER_SIDE_RIGHT|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT},// 3/4+LFe		FL, FR, FC, BL, Bls, Brs, BR, LFe
};
