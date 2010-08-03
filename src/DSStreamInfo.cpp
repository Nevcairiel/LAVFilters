#include "stdafx.h"
#include "DSStreamInfo.h"
#include "DSGuidHelper.h"

CDSStreamInfo::CDSStreamInfo(AVStream *avstream, const char* containerFormat)
{
  int len = strlen(containerFormat) + 1;
  m_containerFormat = (char *)malloc(sizeof(char) * len);
  strncpy_s(m_containerFormat, len, containerFormat, _TRUNCATE);

  mtype.InitMediaType();

  switch(avstream->codec->codec_type) {
  case AVMEDIA_TYPE_AUDIO:
    CreateAudioMediaType(avstream);
    break;
  case AVMEDIA_TYPE_VIDEO:
    CreateVideoMediaType(avstream);
    break;
  case AVMEDIA_TYPE_SUBTITLE:
    CreateSubtitleMediaType(avstream);
    break;
  default:
    break;
  }
}

CDSStreamInfo::~CDSStreamInfo()
{
  free(m_containerFormat);
}

STDMETHODIMP CDSStreamInfo::CreateAudioMediaType(AVStream *avstream)
{
  mtype = g_GuidHelper.initAudioType(avstream->codec->codec_id);
  WAVEFORMATEX* wvfmt = (WAVEFORMATEX*)mtype.AllocFormatBuffer(sizeof(WAVEFORMATEX) + avstream->codec->extradata_size);

  avstream->codec->codec_tag = av_codec_get_tag(mp_wav_taglists, avstream->codec->codec_id);

  // TODO: values for this are non-trivial, see <mmreg.h>
  wvfmt->wFormatTag = avstream->codec->codec_tag;

  wvfmt->nChannels = avstream->codec->channels;
  wvfmt->nSamplesPerSec = avstream->codec->sample_rate;
  wvfmt->wBitsPerSample = avstream->codec->bits_per_coded_sample;
  if (wvfmt->wBitsPerSample == 0) {
    wvfmt->wBitsPerSample = av_get_bits_per_sample_format(avstream->codec->sample_fmt);
  }

  if ( avstream->codec->block_align > 0 ) {
    wvfmt->nBlockAlign = avstream->codec->block_align;
  } else {
    if ( wvfmt->wBitsPerSample == 0 ) {
      DbgOutString(L"BitsPerSample is 0, no good!");
    }
    wvfmt->nBlockAlign = (WORD)((wvfmt->nChannels * wvfmt->wBitsPerSample) / 8);
  }

  wvfmt->nAvgBytesPerSec = avstream->codec->bit_rate / 8;

  wvfmt->cbSize = avstream->codec->extradata_size;
  if (avstream->codec->extradata_size > 0) {
    memcpy(wvfmt + 1, avstream->codec->extradata, avstream->codec->extradata_size);
  }

  //TODO Fix the sample size
  if (avstream->codec->bits_per_coded_sample == 0)
    mtype.SetSampleSize(256000);
  return S_OK;
}

STDMETHODIMP CDSStreamInfo::CreateVideoMediaType(AVStream *avstream)
{
  mtype = g_GuidHelper.initVideoType(avstream->codec->codec_id);
  mtype.bTemporalCompression = 0;
  mtype.bFixedSizeSamples = 1; // TODO

  avstream->codec->codec_tag = av_codec_get_tag(mp_bmp_taglists, avstream->codec->codec_id);

  if (mtype.formattype == FORMAT_VideoInfo) {
    mtype.pbFormat = (BYTE *)g_GuidHelper.CreateVIH(avstream, &mtype.cbFormat);
  } else if (mtype.formattype == FORMAT_VideoInfo2) {
    mtype.pbFormat = (BYTE *)g_GuidHelper.CreateVIH2(avstream, &mtype.cbFormat);
  } else if (mtype.formattype == FORMAT_MPEGVideo) {
    mtype.pbFormat = (BYTE *)g_GuidHelper.CreateMPEG1VI(avstream, &mtype.cbFormat);
  } else if (mtype.formattype == FORMAT_MPEG2Video) {
    mtype.pbFormat = (BYTE *)g_GuidHelper.CreateMPEG2VI(avstream, &mtype.cbFormat, (strcmp(m_containerFormat, "mpegts") == 0));
  }

  return S_OK;
}

STDMETHODIMP CDSStreamInfo::CreateSubtitleMediaType(AVStream *avstream)
{
  mtype.InitMediaType();
  // TODO
  //mtype.majortype = MEDIATYPE_Subtitle;
  //mtype.formattype = FORMAT_SubtitleInfo;
  return S_OK;
}
