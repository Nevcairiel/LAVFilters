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
#include "mkv/MatroskaParser.h"

typedef struct CodecMime{
    char str[32];
    enum CodecID id;
}CodecMime;

typedef struct CodecTags{
    char str[20];
    enum CodecID id;
}CodecTags;

extern "C" {
#include "libavformat/avio.h"

// Data arrays
__declspec(dllimport) extern const CodecTags ff_mkv_codec_tags[];
__declspec(dllimport) extern const CodecMime ff_mkv_mime_tags[];
__declspec(dllimport) extern const AVCodecTag ff_codec_bmp_tags[];
__declspec(dllimport) extern const AVCodecTag codec_movvideo_tags[];
__declspec(dllimport) extern const unsigned char ff_sipr_subpk_size[4];
__declspec(dllimport) extern const int avpriv_mpeg4audio_sample_rates[16];

// Helper functions
extern enum CodecID ff_codec_get_id(const AVCodecTag *tags, unsigned int tag);
extern int ff_get_wav_header(AVIOContext *pb, AVCodecContext *codec, int size);
extern AVChapter *avpriv_new_chapter(AVFormatContext *s, int id, AVRational time_base, int64_t start, int64_t end, const char *title);
extern void ff_update_cur_dts(AVFormatContext *s, AVStream *ref_st, int64_t timestamp);
};

#define IO_BUFFER_SIZE 32768
#define EBML_ID_HEADER 0x1A45DFA3

static const AVRational mkv_timebase = {1, 1000000000};

static const char *matroska_doctypes[] = { "matroska", "webm" };

typedef struct AVIOStream {
  InputStream base;
  AVFormatContext *ctx;
} AVIOStream;

typedef struct MatroskaTrack {
  TrackInfo *info;
  CompressedStream *cs;
  AVStream *stream;
  int ms_compat;
} MatroskaTrack;

typedef struct MatroskaDemuxContext {
  AVIOStream      *iostream;
  MatroskaFile    *matroska;
  AVFormatContext *ctx;

  int num_tracks;
  MatroskaTrack   *tracks;

  ulonglong mask;

  char CSBuffer[4096];
  char *Buffer;
  unsigned BufferSize;
} MatroskaDemuxContext;

static int aviostream_read(struct AVIOStream *cc,ulonglong pos,void *buffer,int count)
{
  int ret;
  int64_t ret64;

  /* Seek to the desired position */
  ret64 = avio_seek(cc->ctx->pb, pos, SEEK_SET);
  if(ret64 < 0) {
    DbgLog((LOG_ERROR, 10, L"aviostream_read(): Seek to %I64u failed with code %I64d", pos, ret64));
    return -1;
  }

  /* Read the requested number of bytes */
  ret = avio_read(cc->ctx->pb, (unsigned char *)buffer, count);
  if (ret == AVERROR_EOF) {
    return 0;
  } else if (ret < 0) {
    return -1;
  }
  return ret;
}

static longlong aviostream_scan(struct AVIOStream *cc,ulonglong start,unsigned signature)
{
  int64_t ret64;
  unsigned cmp = 0;

  /* Seek to the desired position */
  ret64 = avio_seek(cc->ctx->pb, start, SEEK_SET);
  if(ret64 < 0) {
    DbgLog((LOG_ERROR, 10, L"aviostream_scan(): Seek to %I64u failed with code %I64d", start, ret64));
    return -1;
  }

  /* Scan for the byte signature, until EOF was found */
  while(!cc->ctx->pb->eof_reached) {
    cmp = ((cmp << 8) | avio_r8(cc->ctx->pb)) & 0xffffffff;
    if (cmp == signature)
      return avio_tell(cc->ctx->pb) - 4;
  }

  return -1;
}

static unsigned aviostream_getcachesize(struct AVIOStream *cc)
{
  return cc->ctx->pb->max_packet_size ? cc->ctx->pb->max_packet_size : IO_BUFFER_SIZE;
}

static const char *aviostream_geterror(struct AVIOStream *cc)
{
  return "avio error";
}

static void *aviostream_memalloc(struct AVIOStream *cc, size_t size)
{
  return av_malloc(size);
}

static void *aviostream_memrealloc(struct AVIOStream *cc, void *mem, size_t newsize)
{
  return av_realloc(mem, newsize);
}

static void aviostream_memfree(struct AVIOStream *cc, void *mem)
{
  av_free(mem);
}

static int aviostream_progress(struct AVIOStream *cc, ulonglong cur, ulonglong max)
{
  return 1;
}

static longlong aviostream_getfilesize(struct AVIOStream *cc)
{
  return avio_size(cc->ctx->pb);
}

/* Taken vanilla from ffmpeg */
static int mkv_probe(AVProbeData *p)
{
  uint64_t total = 0;
  int len_mask = 0x80, size = 1, n = 1, i;

  /* EBML header? */
  if (AV_RB32(p->buf) != EBML_ID_HEADER)
    return 0;

  /* length of header */
  total = p->buf[4];
  while (size <= 8 && !(total & len_mask)) {
    size++;
    len_mask >>= 1;
  }
  if (size > 8)
    return 0;
  total &= (len_mask - 1);
  while (n < size)
    total = (total << 8) | p->buf[4 + n++];

  /* Does the probe data contain the whole header? */
  if (p->buf_size < 4 + size + total)
    return 0;

  /* The header should contain a known document type. For now,
  * we don't parse the whole header but simply check for the
  * availability of that array of characters inside the header.
  * Not fully fool-proof, but good enough. */
  for (i = 0; i < FF_ARRAY_ELEMS(matroska_doctypes); i++) {
    int probelen = strlen(matroska_doctypes[i]);
    if (total < probelen)
      continue;
    for (n = 4+size; n <= 4+size+total-probelen; n++)
      if (!memcmp(p->buf+n, matroska_doctypes[i], probelen))
        return AVPROBE_SCORE_MAX*2;
  }

  // probably valid EBML header but no recognized doctype
  return AVPROBE_SCORE_MAX;
}

static int matroska_aac_profile(char *codec_id)
{
  static const char * const aac_profiles[] = { "MAIN", "LC", "SSR" };
  int profile;

  for (profile=0; profile<FF_ARRAY_ELEMS(aac_profiles); profile++)
    if (strstr(codec_id, aac_profiles[profile]))
      break;
  return profile + 1;
}

static int matroska_aac_sri(int samplerate)
{
  int sri;

  for (sri=0; sri<FF_ARRAY_ELEMS(avpriv_mpeg4audio_sample_rates); sri++)
    if (avpriv_mpeg4audio_sample_rates[sri] == samplerate)
      break;
  return sri;
}

static void mkv_process_chapter(AVFormatContext *s, Chapter *chapter, int level)
{
  unsigned i;
  if (chapter->UID && chapter->Enabled) {
    AVChapter *avchap = avpriv_new_chapter(s, (int)chapter->UID, mkv_timebase, chapter->Start, chapter->End, chapter->Display ? chapter->Display->String : NULL);

    if (level > 0 && chapter->Display && chapter->Display->String) {
      char *title = (char *)av_mallocz(level + strlen(chapter->Display->String) + 2);
      memset(title, '+', level);
      title[level] = ' ';
      memcpy(&title[level+1], chapter->Display->String, strlen(chapter->Display->String));
      av_dict_set(&avchap->metadata, "title", title, 0);
    }
  }
  if (chapter->nChildren > 0) {
    for (i = 0; i < chapter->nChildren; i++) {
      mkv_process_chapter(s, chapter->Children + i, level + 1);
    }
  }
}

static ulonglong mkv_get_track_mask(MatroskaDemuxContext *ctx)
{
  int i;
  ulonglong mask = 0;
  for (i = 0; i < ctx->num_tracks; i++) {
    if (!ctx->tracks[i].stream || ctx->tracks[i].stream->discard == AVDISCARD_ALL)
      mask |= (1ui64 << i);
  }
  return mask;
}

static int mkv_read_header(AVFormatContext *s)
{
  MatroskaDemuxContext *ctx = (MatroskaDemuxContext *)s->priv_data;
  char ErrorMessage[256];
  int i, j, num_tracks;
  SegmentInfo *segment;
  Chapter *chapters = NULL;
  Attachment *attachments = NULL;
  Cue *cues = NULL;
  unsigned int count, u;

  AVIOStream *iostream = (AVIOStream *)av_mallocz(sizeof(AVIOStream));
  iostream->base.read = (int (*)(InputStream *,ulonglong,void *,int))aviostream_read;
  iostream->base.scan = (longlong (*)(InputStream *,ulonglong,unsigned int))aviostream_scan;
  iostream->base.getcachesize = (unsigned (*)(InputStream *cc))aviostream_getcachesize;
  iostream->base.geterror = (const char *(*)(InputStream *))aviostream_geterror;
  iostream->base.memalloc = (void *(*)(InputStream *,size_t))aviostream_memalloc;
  iostream->base.memrealloc = (void *(*)(InputStream *,void *,size_t))aviostream_memrealloc;
  iostream->base.memfree = (void (*)(InputStream *,void *))aviostream_memfree;
  iostream->base.progress = (int (*)(InputStream *,ulonglong,ulonglong))aviostream_progress;
  iostream->base.getfilesize = (longlong (*)(InputStream *))aviostream_getfilesize;
  iostream->ctx = s;
  ctx->iostream = iostream;

  DbgLog((LOG_TRACE, 10, L"mkv_read_header(): Opening MKV file"));
  ctx->matroska = mkv_OpenEx(&iostream->base, 0, 0, ErrorMessage, sizeof(ErrorMessage));
  if (!ctx->matroska) {
    DbgLog((LOG_ERROR, 10, L" -> mkv_OpenEx returned error: %S", ErrorMessage));
    return -1;
  }

  /* Gather information */
  segment = mkv_GetFileInfo(ctx->matroska);

  if (segment->Duration)
    s->duration = segment->Duration / (1000000000 / AV_TIME_BASE);
  av_dict_set(&s->metadata, "title", segment->Title, 0);

  /* Tracks */
  ctx->num_tracks = num_tracks = mkv_GetNumTracks(ctx->matroska);
  ctx->tracks = (MatroskaTrack *)av_mallocz(sizeof(MatroskaTrack) * num_tracks);
  for(i = 0; i < num_tracks; i++) {
    MatroskaTrack *track = &ctx->tracks[i];
    TrackInfo *info = mkv_GetTrackInfo(ctx->matroska, i);
    enum CodecID codec_id = CODEC_ID_NONE;
    AVStream *st;
    uint8_t *extradata = NULL;
    int extradata_size = 0;
    int extradata_offset = 0;
    uint32_t fourcc = 0;

    track->info = info;

    if (info->Type != TT_VIDEO && info->Type != TT_AUDIO && info->Type != TT_SUB) {
      DbgLog((LOG_ERROR, 10, L" -> Unknown or unsupported track type: %d", info->Type));
      continue;
    }

    if (info->CodecID == NULL)
      continue;

    DbgLog((LOG_TRACE, 10, L" -> Track %d, type: %d, codec_id: %S", i, info->Type, info->CodecID));

    if (info->Type == TT_VIDEO) {
      if (!info->AV.Video.DisplayWidth)
        info->AV.Video.DisplayWidth = info->AV.Video.PixelWidth;
      if (!info->AV.Video.DisplayHeight)
        info->AV.Video.DisplayHeight = info->AV.Video.PixelHeight;
    } else if (info->Type == TT_AUDIO) {
      if (!mkv_TruncFloat(info->AV.Audio.OutputSamplingFreq))
        info->AV.Audio.OutputSamplingFreq = info->AV.Audio.SamplingFreq;
    }

    if (info->CompEnabled && info->CompMethod == COMP_ZLIB) {
      DbgLog((LOG_TRACE, 10, L" -> Track is ZLIB compressed"));
      track->cs = cs_Create(ctx->matroska, i, ErrorMessage, sizeof(ErrorMessage));
      if (!track->cs) {
        DbgLog((LOG_TRACE, 10, L" -> Creating compressed stream failed: %S", ErrorMessage));
        continue;
      }
    }

    for(j=0; ff_mkv_codec_tags[j].id != CODEC_ID_NONE; j++){
      if(!strncmp(ff_mkv_codec_tags[j].str, info->CodecID, strlen(ff_mkv_codec_tags[j].str))){
        codec_id = ff_mkv_codec_tags[j].id;
        break;
      }
    }

    st = track->stream = avformat_new_stream(s, NULL);
    if (st == NULL)
      return AVERROR(ENOMEM);

    if (!strcmp(info->CodecID, "V_MS/VFW/FOURCC") && info->CodecPrivateSize >= 40 && info->CodecPrivate != NULL) {
      track->ms_compat = 1;
      fourcc = AV_RL32((uint8_t *)info->CodecPrivate + 16);
      codec_id = ff_codec_get_id(ff_codec_bmp_tags, fourcc);
      extradata_offset = 40;
    } else if (!strcmp(info->CodecID, "A_MS/ACM") && info->CodecPrivateSize >= 14 && info->CodecPrivate != NULL) {
      int ret;
      AVIOContext *pb = avio_alloc_context((uint8_t *)info->CodecPrivate, info->CodecPrivateSize, AVIO_FLAG_READ, NULL, NULL, NULL, NULL);
      ret = ff_get_wav_header(pb, st->codec, info->CodecPrivateSize);
      av_free(pb);
      if (ret < 0)
        return ret;
      codec_id = st->codec->codec_id;
      extradata_offset = FFMIN(info->CodecPrivateSize, 18);
    } else if (!strcmp(info->CodecID, "V_QUICKTIME") && (info->CodecPrivateSize >= 86) && (info->CodecPrivate != NULL)) {
      fourcc = AV_RL32(info->CodecPrivate);
      codec_id = ff_codec_get_id(codec_movvideo_tags, fourcc);
    } else if (codec_id == CODEC_ID_PCM_S16BE) {
      switch (info->AV.Audio.BitDepth) {
      case  8:  codec_id = CODEC_ID_PCM_U8;     break;
      case 24:  codec_id = CODEC_ID_PCM_S24BE;  break;
      case 32:  codec_id = CODEC_ID_PCM_S32BE;  break;
      }
    } else if (codec_id == CODEC_ID_PCM_S16LE) {
      switch (info->AV.Audio.BitDepth) {
      case  8:  codec_id = CODEC_ID_PCM_U8;     break;
      case 24:  codec_id = CODEC_ID_PCM_S24LE;  break;
      case 32:  codec_id = CODEC_ID_PCM_S32LE;  break;
      }
    } else if (codec_id==CODEC_ID_PCM_F32LE && info->AV.Audio.BitDepth == 64) {
      codec_id = CODEC_ID_PCM_F64LE;
    } else if (codec_id == CODEC_ID_AAC && !info->CodecPrivateSize) {
      int profile = matroska_aac_profile(info->CodecID);
      int sri = matroska_aac_sri(mkv_TruncFloat(info->AV.Audio.SamplingFreq));
      extradata = (uint8_t *)av_malloc(5);
      if (extradata == NULL)
        return AVERROR(ENOMEM);
      extradata[0] = (profile << 3) | ((sri&0x0E) >> 1);
      extradata[1] = ((sri&0x01) << 7) | (info->AV.Audio.Channels<<3);
      if (strstr(info->CodecID, "SBR")) {
        sri = matroska_aac_sri(mkv_TruncFloat(info->AV.Audio.OutputSamplingFreq));
        extradata[2] = 0x56;
        extradata[3] = 0xE5;
        extradata[4] = 0x80 | (sri<<3);
        extradata_size = 5;
      } else
        extradata_size = 2;
    } else if (codec_id == CODEC_ID_TTA) {
      extradata_size = 30;
      extradata = (uint8_t *)av_mallocz(extradata_size);
      if (extradata == NULL)
        return AVERROR(ENOMEM);
      AVIOContext *pb = avio_alloc_context(extradata, extradata_size, AVIO_FLAG_READ, NULL, NULL, NULL, NULL);
      avio_write(pb, (const unsigned char *)"TTA1", 4);
      avio_wl16(pb, 1);
      avio_wl16(pb, info->AV.Audio.Channels);
      avio_wl16(pb, info->AV.Audio.BitDepth);
      avio_wl32(pb, mkv_TruncFloat(info->AV.Audio.OutputSamplingFreq));
      avio_wl32(pb, (unsigned int)(s->duration * info->AV.Audio.OutputSamplingFreq));
      av_free(pb);
    } else if (codec_id == CODEC_ID_RA_144) {
      info->AV.Audio.OutputSamplingFreq = 8000;
      info->AV.Audio.Channels = 1;
    } else if (codec_id == CODEC_ID_RA_288 || codec_id == CODEC_ID_COOK ||
      codec_id == CODEC_ID_ATRAC3 || codec_id == CODEC_ID_SIPR) {
        int flavor;
        AVIOContext *pb = avio_alloc_context((uint8_t *)info->CodecPrivate, info->CodecPrivateSize, AVIO_FLAG_READ, NULL, NULL, NULL, NULL);
        avio_skip(pb, 22);
        flavor              = avio_rb16(pb);
        int coded_framesize = avio_rb32(pb);
        avio_skip(pb, 12);
        int sub_packet_h    = avio_rb16(pb);
        int frame_size      = avio_rb16(pb);
        int sub_packet_size = avio_rb16(pb);
        uint8_t *buf        = (uint8_t *)av_malloc(frame_size * sub_packet_h);
        if (codec_id == CODEC_ID_RA_288) {
          st->codec->block_align = coded_framesize;
          info->CodecPrivateSize = 0;
        } else {
          if (codec_id == CODEC_ID_SIPR && flavor < 4) {
            const int sipr_bit_rate[4] = { 6504, 8496, 5000, 16000 };
            sub_packet_size = ff_sipr_subpk_size[flavor];
            st->codec->bit_rate = sipr_bit_rate[flavor];
          }
          st->codec->block_align = sub_packet_size;
          extradata_offset = 78;
        }
    }
    info->CodecPrivateSize -= extradata_offset;

    if (codec_id == CODEC_ID_NONE)
      DbgLog((LOG_TRACE, 10, L" -> Unknown/unsupported CodecID: %S", info->CodecID));

    av_set_pts_info(st, 64, 1, 1000*1000*1000); /* 64 bit pts in ns */

    st->codec->codec_id = codec_id;
    st->start_time = 0;
    if (strlen(info->Language) == 0) /* default english language if none is set */
      av_dict_set(&st->metadata, "language", "eng", 0);
    else if (strcmp(info->Language, "und"))
      av_dict_set(&st->metadata, "language", info->Language, 0);
    av_dict_set(&st->metadata, "title", info->Name, 0);

    if (info->Default)
      st->disposition |= AV_DISPOSITION_DEFAULT;
    if (info->Forced)
      st->disposition |= AV_DISPOSITION_FORCED;

    if (info->DefaultDuration)
      av_reduce(&st->codec->time_base.num, &st->codec->time_base.den, info->DefaultDuration, 1000000000, 30000);

    if (!st->codec->extradata) {
      if(extradata){
        st->codec->extradata = extradata;
        st->codec->extradata_size = extradata_size;
      } else if(info->CodecPrivate && info->CodecPrivateSize > 0){
        st->codec->extradata = (uint8_t *)av_mallocz(info->CodecPrivateSize + FF_INPUT_BUFFER_PADDING_SIZE);
        if(st->codec->extradata == NULL)
          return AVERROR(ENOMEM);
        st->codec->extradata_size = info->CodecPrivateSize;
        memcpy(st->codec->extradata, (uint8_t *)info->CodecPrivate + extradata_offset, info->CodecPrivateSize);
      }
    }

    if (info->Type == TT_VIDEO) {
      st->codec->codec_type = AVMEDIA_TYPE_VIDEO;
      st->codec->codec_tag  = fourcc;
      st->codec->width  = info->AV.Video.PixelWidth;
      st->codec->height = info->AV.Video.PixelHeight;
      av_reduce(&st->sample_aspect_ratio.num,
        &st->sample_aspect_ratio.den,
        st->codec->height * info->AV.Video.DisplayWidth,
        st->codec-> width * info->AV.Video.DisplayHeight,
        255);
      if (st->codec->codec_id != CODEC_ID_H264)
        st->need_parsing = AVSTREAM_PARSE_HEADERS;
      if (info->DefaultDuration)
        st->avg_frame_rate = av_d2q(1000000000.0/info->DefaultDuration, INT_MAX);

      /* export stereo mode flag as metadata tag */
      /* if (track->video.stereo_mode && track->video.stereo_mode < MATROSKA_VIDEO_STEREO_MODE_COUNT)
      av_dict_set(&st->metadata, "stereo_mode", matroska_video_stereo_mode[track->video.stereo_mode], 0);

      /* if we have virtual track, mark the real tracks
      for (j=0; j < track->operation.combine_planes.nb_elem; j++) {
        char buf[32];
        if (planes[j].type >= MATROSKA_VIDEO_STEREO_PLANE_COUNT)
          continue;
        snprintf(buf, sizeof(buf), "%s_%d", matroska_video_stereo_plane[planes[j].type], i);
        for (k=0; k < matroska->tracks.nb_elem; k++)
          if (planes[j].uid == tracks[k].uid) {
            av_dict_set(&s->streams[k]->metadata, "stereo_mode", buf, 0);
            break;
          }
      } */
    } else if (info->Type == TT_AUDIO) {
      st->codec->codec_type = AVMEDIA_TYPE_AUDIO;
      st->codec->sample_rate = (unsigned int)info->AV.Audio.OutputSamplingFreq;
      st->codec->channels = info->AV.Audio.Channels;
      if (st->codec->codec_id != CODEC_ID_AAC && st->codec->codec_id != CODEC_ID_MLP && st->codec->codec_id != CODEC_ID_TRUEHD)
        st->need_parsing = AVSTREAM_PARSE_HEADERS;
    } else if (info->Type == TT_SUB) {
      st->codec->codec_type = AVMEDIA_TYPE_SUBTITLE;
    }
  }

  /* chapter start at level -1 because they are always wrapped in a edition entry */
  mkv_GetChapters(ctx->matroska, &chapters, &count);
  for (u = 0; u < count; u++) {
    mkv_process_chapter(s, chapters + u, -1);
  }

  mkv_GetAttachments(ctx->matroska, &attachments, &count);
  for (u = 0; u < count; u++) {
    Attachment *attach = &attachments[u];

    if (!(attach->Name && attach->MimeType && attach->Length > 0)) {
      DbgLog((LOG_TRACE, 10, L" -> Incomplete attachment, skipping"));
    } else {
      AVStream *st = avformat_new_stream(s, NULL);
      if (st == NULL)
        break;
      av_dict_set(&st->metadata, "filename", attach->Name, 0);
      av_dict_set(&st->metadata, "mimetype", attach->MimeType, 0);
      st->codec->codec_id = CODEC_ID_NONE;
      st->codec->codec_type = AVMEDIA_TYPE_ATTACHMENT;

      st->codec->extradata = (uint8_t *)av_malloc((size_t)attach->Length + FF_INPUT_BUFFER_PADDING_SIZE);
      if(st->codec->extradata == NULL)
        break;
      st->codec->extradata_size = (int)attach->Length;
      aviostream_read(ctx->iostream, attach->Position, st->codec->extradata, st->codec->extradata_size);

      for (i=0; ff_mkv_mime_tags[i].id != CODEC_ID_NONE; i++) {
        if (!strncmp(ff_mkv_mime_tags[i].str, attach->MimeType, strlen(ff_mkv_mime_tags[i].str))) {
          st->codec->codec_id = ff_mkv_mime_tags[i].id;
          break;
        }
      }
    }
  }

  /* convert Cue entries into av index entries */
  mkv_GetCues(ctx->matroska, &cues, &count);
  for (u = 0; u < count; u++) {
    ulonglong pos = mkv_GetSegmentTop(ctx->matroska) + cues[u].Position;
    for(i = 0; i < num_tracks; i++) {
      av_add_index_entry(ctx->tracks[i].stream, -1, cues[u].Time, 0, 0, AVINDEX_KEYFRAME);
    }
  }

  return 0;
}

static int mkv_read_packet(AVFormatContext *s, AVPacket *pkt)
{
  MatroskaDemuxContext *ctx = (MatroskaDemuxContext *)s->priv_data;

  int ret;
  unsigned int size, offset = 0, flags, track_num;
  ulonglong start_time, end_time, pos;
  MatroskaTrack *track;

  ulonglong mask = mkv_get_track_mask(ctx);
  if (mask != ctx->mask) {
    mkv_SetTrackMask(ctx->matroska, mask);
    ctx->mask = mask;
  }

again:
  ret = mkv_ReadFrame(ctx->matroska, mask, &track_num, &start_time, &end_time, &pos, &size, &flags);
  if (ret < 0)
    return AVERROR_EOF;

  track = &ctx->tracks[track_num];
  if (!track->stream || track->stream->discard == AVDISCARD_ALL)
    goto again;

  /* zlib compression */
  if (track->cs) {
    unsigned int frame_size = 0;
    cs_NextFrame(track->cs, pos, size);
    for(;;) {
      ret = cs_ReadData(track->cs, ctx->CSBuffer, sizeof(ctx->CSBuffer));
      if (ret < 0) {
        DbgLog((LOG_ERROR, 10, L"cs_ReadData failed"));
        return AVERROR(EIO);
      } else if (ret == 0) {
        size = frame_size;
        break;
      }
      if (ctx->BufferSize < (frame_size + ret)) {
        ctx->BufferSize = (frame_size + ret) * 2;
        ctx->Buffer = (char *)av_realloc(ctx->Buffer, ctx->BufferSize);
      }
      memcpy(ctx->Buffer + frame_size, ctx->CSBuffer, ret);
      frame_size += ret;
    }
    av_new_packet(pkt, frame_size);
    memcpy(pkt->data, ctx->Buffer, frame_size);
  } else {
    /* header removal compression */
    if (track->info->CompEnabled && track->info->CompMethod == COMP_PREPEND && track->info->CompMethodPrivateSize > 0) {
      offset = track->info->CompMethodPrivateSize;
    }

    av_new_packet(pkt, size+offset);
    ret = aviostream_read(ctx->iostream, pos, pkt->data+offset, size);
    if (ret < (int)size) {
      av_free_packet(pkt);
      return AVERROR(EIO);
    }

    if (offset > 0)
      memcpy(pkt->data, track->info->CompMethodPrivate, offset);
  }

  if (!(flags & FRAME_UNKNOWN_START)) {
    if (track->ms_compat)
      pkt->dts = start_time;
    else
      pkt->pts = start_time;

    if (track->info->Type == TT_SUB)
      pkt->convergence_duration = end_time - start_time;
    else
      pkt->duration = (int)(end_time - start_time);
  }

  pkt->flags = (flags & FRAME_KF) ? AV_PKT_FLAG_KEY : 0;
  pkt->pos = pos;
  pkt->size = size+offset;
  pkt->stream_index = track->stream->index;

  return 0;
}

static int mkv_read_close(AVFormatContext *s)
{
  MatroskaDemuxContext *ctx = (MatroskaDemuxContext *)s->priv_data;
  int i;

  mkv_Close(ctx->matroska);
  av_freep(&ctx->iostream);

  for (i = 0; i < ctx->num_tracks; i++) {
    av_freep(&ctx->tracks[i].cs);
  }
  av_freep(&ctx->tracks);


  return 0;
}

static int mkv_read_seek(AVFormatContext *s, int stream_index, int64_t timestamp, int flags)
{
  MatroskaDemuxContext *ctx = (MatroskaDemuxContext *)s->priv_data;
  int mkvflags = !(flags & AVSEEK_FLAG_ANY) ? MKVF_SEEK_TO_PREV_KEYFRAME : 0;
  int i;
  AVStream *st = ctx->tracks[stream_index].stream;

  /* check if we're seeking to a index entry directly, and if so, disable the keyframe logic */
  for (i = 0; i < st->nb_index_entries; i++) {
    if (st->index_entries[i].timestamp == timestamp) {
      mkvflags &= ~MKVF_SEEK_TO_PREV_KEYFRAME;
      break;
    }
  }

  /* update track mask */
  mkv_SetTrackMask(ctx->matroska, mkv_get_track_mask(ctx));

  /* perform seek */
  DBG_TIMING("mkv_Seek", 0, mkv_Seek(ctx->matroska, timestamp, mkvflags))

  /* Update current timestamp */
  int64_t cur_dts = mkv_GetLowestQTimecode(ctx->matroska);
  DbgLog((LOG_TRACE, 10, "mkv_read_seek: requested: %I64d, achieved: %I64d", timestamp, cur_dts));
  if (cur_dts == -1)
    cur_dts = timestamp;

  ff_update_cur_dts(s, ctx->tracks[stream_index].stream, cur_dts);

  return 0;
}

AVInputFormat lav_mkv_demuxer = {
  "matroska",                   /* name */
  "Matroska/WebM",              /* long name */
  0,                            /* flags */
  NULL,                         /* extensions */
  NULL,                         /* codec_tag */
  NULL,                         /* priv_class */
  NULL,                         /* next */
  0,                            /* raw_codec_id */
  sizeof(MatroskaDemuxContext), /* priv_data_size */
  mkv_probe,                    /* read_probe */
  mkv_read_header,              /* read_header */
  mkv_read_packet,              /* read_packet */
  mkv_read_close,               /* read_close */
  mkv_read_seek,                /* read_seek */
  NULL,                         /* read_timestamp */
  NULL,                         /* read_play */
  NULL,                         /* read_pause */
  NULL,                         /* read_seek2 */
};
