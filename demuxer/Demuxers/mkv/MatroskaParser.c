/*
 * Copyright (c) 2004-2008 Mike Matsnev.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    Mike Matsnev.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: MatroskaParser.c,v 1.75 2010/08/14 08:38:41 mike Exp $
 *
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#ifdef _WIN32
// MS names some functions differently
#define        alloca          _alloca
#define        inline          __inline

#include <tchar.h>
#endif

#ifndef EVCBUG
#define        EVCBUG
#endif

#include "MatroskaParser.h"

#ifdef MATROSKA_COMPRESSION_SUPPORT
#include <zlib.h>
#endif

#define        EBML_VERSION              1
#define        EBML_MAX_ID_LENGTH    4
#define        EBML_MAX_SIZE_LENGTH  8
#define        MATROSKA_VERSION      2
#define        MATROSKA_DOCTYPE      "matroska"
#define WEBM_DOCTYPE          "webm"

#define        MAX_STRING_LEN              1023
#define        QSEGSIZE              512
#define        MAX_TRACKS              32
#define        MAX_READAHEAD              (256*1024)

#define        MAXCLUSTER              (64*1048576)
#define        MAXFRAME              (4*1048576)

#ifdef WIN32
#define        LL(x)        x##i64
#define        ULL(x)        x##ui64
#else
#define        LL(x)        x##ll
#define        ULL(x)        x##ull
#endif

#define        MAXU64                      ULL(0xffffffffffffffff)
#define        ONE                      ULL(1)

// compatibility
static char  *mystrdup(struct InputStream *is,const char *src) {
  size_t  len;
  char          *dst;

  if (src==NULL)
    return NULL;

  len = strlen(src);
  dst = is->memalloc(is,len+1);
  if (dst==NULL)
    return NULL;

  memcpy(dst,src,len+1);

  return dst;
}

static void  mystrlcpy(char *dst,const char *src,unsigned size) {
  unsigned  i;

  for (i=0;i+1<size && src[i];++i)
    dst[i] = src[i];
  if (i<size)
    dst[i] = 0;
}

struct Cue {
  ulonglong        Time;
  ulonglong        Position;
  ulonglong        Block;
  unsigned char        Track;
};

struct QueueEntry {
  struct QueueEntry *next;
  unsigned int            Length;

  ulonglong            Start;
  ulonglong            End;
  ulonglong            Position;

  unsigned int            flags;
};

struct Queue {
  struct QueueEntry *head;
  struct QueueEntry *tail;
};

#define        MPF_ERROR 0x10000
#define        IBSZ          1024

#define        RBRESYNC  1

struct MatroskaFile {
  // parser config
  unsigned    flags;

  // input
  InputStream *cache;

  // internal buffering
  char              inbuf[IBSZ];
  ulonglong   bufbase; // file offset of the first byte in buffer
  int              bufpos; // current read position in buffer
  int              buflen; // valid bytes in buffer

  // error reporting
  char              errmsg[128];
  jmp_buf     jb;

  // pointers to key elements
  ulonglong   pSegment;
  ulonglong   pSeekHead;
  ulonglong   pSegmentInfo;
  ulonglong   pCluster;
  ulonglong   pTracks;
  ulonglong   pCues;
  ulonglong   pAttachments;
  ulonglong   pChapters;
  ulonglong   pTags;

  // flags for key elements
  struct {
    unsigned int  SegmentInfo:1;
    unsigned int  Cluster:1;
    unsigned int  Tracks:1;
    unsigned int  Cues:1;
    unsigned int  Attachments:1;
    unsigned int  Chapters:1;
    unsigned int  Tags:1;
  } seen;

  // file info
  ulonglong   firstTimecode;

  // SegmentInfo
  struct SegmentInfo  Seg;

  // Tracks
  unsigned int            nTracks,nTracksSize;
  struct TrackInfo  **Tracks;

  // Queues
  struct QueueEntry *QFreeList;
  unsigned int            nQBlocks,nQBlocksSize;
  struct QueueEntry **QBlocks;
  struct Queue            *Queues;
  ulonglong            readPosition;
  unsigned int            trackMask;
  ulonglong            pSegmentTop;  // offset of next byte after the segment
  ulonglong            tcCluster;    // current cluster timecode

  // Cues
  unsigned int            nCues,nCuesSize;
  struct Cue            *Cues;

  // Attachments
  unsigned int            nAttachments,nAttachmentsSize;
  struct Attachment *Attachments;

  // Chapters
  unsigned int            nChapters,nChaptersSize;
  struct Chapter    *Chapters;

  // Tags
  unsigned int            nTags,nTagsSize;
  struct Tag            *Tags;
};

///////////////////////////////////////////////////////////////////////////
// error reporting
static void   myvsnprintf_string(char **pdest,char *de,const char *str) {
  char        *dest = *pdest;

  while (dest < de && *str)
    *dest++ = *str++;

  *pdest = dest;
}

static void   myvsnprintf_uint_impl(char **pdest,char *de,int width,int zero,
                                    int neg,unsigned base,int letter,
                                    int ms,ulonglong val)
{
  char        *dest = *pdest;
  char        tmp[21]; /* enough for 64 bit ints */
  char        *np = tmp + sizeof(tmp);
  int        rw,pad,trail;
  char        pc = zero ? '0' : ' ';

  *--np = '\0';
  if (val == 0)
    *--np = '0';
  else
    while (val != 0) {
      int          rem = (int)(val % base);
      val = val / base;

      *--np = (char)(rem < 10 ? rem + '0' : rem - 10 + letter);
    }

  rw = (int)(tmp - np + sizeof(tmp) - 1);
  if (ms)
    ++rw;

  pad = trail = 0;

  if (rw < width)
    pad = width - rw;

  if (neg)
    trail = pad, pad = 0;

  if (dest < de && ms)
    *dest++ = '-';

  while (dest < de && pad--)
    *dest++ = pc;

  while (dest < de && *np)
    *dest++ = *np++;

  while (dest < de && trail--)
    *dest++ = ' ';

  *pdest = dest;
}

static void   myvsnprintf_uint(char **pdest,char *de,int width,int zero,
                               int neg,unsigned base,int letter,
                               ulonglong val)
{
  myvsnprintf_uint_impl(pdest,de,width,zero,neg,base,letter,0,val);
}

static void   myvsnprintf_int(char **pdest,char *de,int width,int zero,
                              int neg,unsigned base,int letter,
                              longlong val)
{
  if (val < 0)
    myvsnprintf_uint_impl(pdest,de,width,zero,neg,base,letter,1,-val);
  else
    myvsnprintf_uint_impl(pdest,de,width,zero,neg,base,letter,0,val);
}

static void   myvsnprintf(char *dest,unsigned dsize,const char *fmt,va_list ap) {
  // s,d,x,u,ll
  char            *de = dest + dsize - 1;
  int            state = 0, width, zero, neg, ll;

  if (dsize <= 1) {
    if (dsize > 0)
      *dest = '\0';
    return;
  }

  while (*fmt && dest < de)
    switch (state) {
      case 0:
        if (*fmt == '%') {
          ++fmt;
          state = 1;
          width = zero = neg = ll = 0;
        } else
          *dest++ = *fmt++;
        break;
      case 1:
        if (*fmt == '-') {
          neg = 1;
          ++fmt;
          state = 2;
          break;
        }
        if (*fmt == '0')
          zero = 1;
        state = 2;
      case 2:
        if (*fmt >= '0' && *fmt <= '9') {
          width = width * 10 + *fmt++ - '0';
          break;
        }
        state = 3;
      case 3:
        if (*fmt == 'l') {
          ++ll;
          ++fmt;
          break;
        }
        state = 4;
      case 4:
        switch (*fmt) {
          case 's':
            myvsnprintf_string(&dest,de,va_arg(ap,const char *));
            break;
          case 'd':
            switch (ll) {
              case 0:
                myvsnprintf_int(&dest,de,width,zero,neg,10,'a',va_arg(ap,int));
                break;
              case 1:
                myvsnprintf_int(&dest,de,width,zero,neg,10,'a',va_arg(ap,long));
                break;
              case 2:
                myvsnprintf_int(&dest,de,width,zero,neg,10,'a',va_arg(ap,longlong));
                break;
            }
            break;
          case 'u':
            switch (ll) {
              case 0:
                myvsnprintf_uint(&dest,de,width,zero,neg,10,'a',va_arg(ap,unsigned int));
                break;
              case 1:
                myvsnprintf_uint(&dest,de,width,zero,neg,10,'a',va_arg(ap,unsigned long));
                break;
              case 2:
                myvsnprintf_uint(&dest,de,width,zero,neg,10,'a',va_arg(ap,ulonglong));
                break;
            }
            break;
          case 'x':
            switch (ll) {
              case 0:
                myvsnprintf_uint(&dest,de,width,zero,neg,16,'a',va_arg(ap,unsigned int));
                break;
              case 1:
                myvsnprintf_uint(&dest,de,width,zero,neg,16,'a',va_arg(ap,unsigned long));
                break;
              case 2:
                myvsnprintf_uint(&dest,de,width,zero,neg,16,'a',va_arg(ap,ulonglong));
                break;
            }
            break;
          case 'X':
            switch (ll) {
              case 0:
                myvsnprintf_uint(&dest,de,width,zero,neg,16,'A',va_arg(ap,unsigned int));
                break;
              case 1:
                myvsnprintf_uint(&dest,de,width,zero,neg,16,'A',va_arg(ap,unsigned long));
                break;
              case 2:
                myvsnprintf_uint(&dest,de,width,zero,neg,16,'A',va_arg(ap,ulonglong));
                break;
            }
            break;
          default:
            break;
        }
        ++fmt;
        state = 0;
        break;
      default:
        state = 0;
        break;
    }
  *dest = '\0';
}

static void   errorjmp(MatroskaFile *mf,const char *fmt, ...) {
  va_list   ap;

  va_start(ap, fmt);
  myvsnprintf(mf->errmsg,sizeof(mf->errmsg),fmt,ap);
  va_end(ap);

  mf->flags |= MPF_ERROR;

  longjmp(mf->jb,1);
}

///////////////////////////////////////////////////////////////////////////
// arrays
static void *ArrayAlloc(MatroskaFile *mf,void **base,
                        unsigned *cur,unsigned *max,unsigned elem_size)
{
  if (*cur>=*max) {
    void      *np;
    unsigned  newsize = *max * 2;
    if (newsize==0)
      newsize = 1;

    np = mf->cache->memrealloc(mf->cache,*base,newsize*elem_size);
    if (np==NULL)
      errorjmp(mf,"Out of memory in ArrayAlloc");

    *base = np;
    *max = newsize;
  }

  return (char*)*base + elem_size * (*cur)++;
}

static void ArrayReleaseMemory(MatroskaFile *mf,void **base,
                               unsigned cur,unsigned *max,unsigned elem_size)
{
  if (cur<*max) {
    void  *np = mf->cache->memrealloc(mf->cache,*base,cur*elem_size);
    *base = np;
    *max = cur;
  }
}


#define        ASGET(f,s,name)          ArrayAlloc((f),(void**)&(s)->name,&(s)->n##name,&(s)->n##name##Size,sizeof(*((s)->name)))
#define        AGET(f,name)          ArrayAlloc((f),(void**)&(f)->name,&(f)->n##name,&(f)->n##name##Size,sizeof(*((f)->name)))
#define        ARELEASE(f,s,name)  ArrayReleaseMemory((f),(void**)&(s)->name,(s)->n##name,&(s)->n##name##Size,sizeof(*((s)->name)))

///////////////////////////////////////////////////////////////////////////
// queues
static struct QueueEntry *QPut(struct Queue *q,struct QueueEntry *qe) {
  if (q->tail)
    q->tail->next = qe;
  qe->next = NULL;
  q->tail = qe;
  if (q->head==NULL)
    q->head = qe;

  return qe;
}

static struct QueueEntry  *QGet(struct Queue *q) {
  struct QueueEntry   *qe = q->head;
  if (qe == NULL)
    return NULL;
  q->head = qe->next;
  if (q->tail == qe)
    q->tail = NULL;
  return qe;
}

static struct QueueEntry  *QAlloc(MatroskaFile *mf) {
  struct QueueEntry   *qe,**qep;
  if (mf->QFreeList == NULL) {
    unsigned              i;

    qep = AGET(mf,QBlocks);

    *qep = mf->cache->memalloc(mf->cache,QSEGSIZE * sizeof(*qe));
    if (*qep == NULL)
      errorjmp(mf,"Ouf of memory");

    qe = *qep;

    for (i=0;i<QSEGSIZE-1;++i)
      qe[i].next = qe+i+1;
    qe[QSEGSIZE-1].next = NULL;

    mf->QFreeList = qe;
  }

  qe = mf->QFreeList;
  mf->QFreeList = qe->next;

  return qe;
}

static inline void QFree(MatroskaFile *mf,struct QueueEntry *qe) {
  qe->next = mf->QFreeList;
  mf->QFreeList = qe;
}

// fill the buffer at current position
static void fillbuf(MatroskaFile *mf) {
  int            rd;

  // advance buffer pointers
  mf->bufbase += mf->buflen;
  mf->buflen = mf->bufpos = 0;

  // get the relevant page
  rd = mf->cache->read(mf->cache, mf->bufbase, mf->inbuf, IBSZ);
  if (rd<0)
    errorjmp(mf,"I/O Error: %s",mf->cache->geterror(mf->cache));

  mf->buflen = rd;
}

// fill the buffer and return next char
static int  nextbuf(MatroskaFile *mf) {
  fillbuf(mf);

  if (mf->bufpos < mf->buflen)
    return (unsigned char)(mf->inbuf[mf->bufpos++]);

  return EOF;
}

static inline int readch(MatroskaFile *mf) {
  return mf->bufpos < mf->buflen ? (unsigned char)(mf->inbuf[mf->bufpos++]) : nextbuf(mf);
}

static inline ulonglong        filepos(MatroskaFile *mf) {
  return mf->bufbase + mf->bufpos;
}

static void   readbytes(MatroskaFile *mf,void *buffer,int len) {
  char        *cp = buffer;
  int        nb = mf->buflen - mf->bufpos;

  if (nb > len)
    nb = len;

  memcpy(cp, mf->inbuf + mf->bufpos, nb);
  mf->bufpos += nb;
  len -= nb;
  cp += nb;

  if (len>0) {
    mf->bufbase += mf->buflen;
    mf->bufpos = mf->buflen = 0;

    nb = mf->cache->read(mf->cache, mf->bufbase, cp, len);
    if (nb<0)
      errorjmp(mf,"I/O Error: %s",mf->cache->geterror(mf->cache));
    if (nb != len)
      errorjmp(mf,"Short read: got %d bytes of %d",nb,len);
    mf->bufbase += len;
  }
}

static void   skipbytes(MatroskaFile *mf,ulonglong len) {
  int            nb = mf->buflen - mf->bufpos;

  if (nb > len)
    nb = (int)len;

  mf->bufpos += nb;
  len -= nb;

  if (len>0) {
    mf->bufbase += mf->buflen;
    mf->bufpos = mf->buflen = 0;

    mf->bufbase += len;
  }
}

static void seek(MatroskaFile *mf,ulonglong pos) {
  // see if pos is inside buffer
  if (pos>=mf->bufbase && pos<mf->bufbase+mf->buflen)
    mf->bufpos = (unsigned)(pos - mf->bufbase);
  else {
    // invalidate buffer and set pointer
    mf->bufbase = pos;
    mf->buflen = mf->bufpos = 0;
  }
}

///////////////////////////////////////////////////////////////////////////
// floating point
static inline MKFLOAT        mkfi(int i) {
#ifdef MATROSKA_INTEGER_ONLY
  MKFLOAT  f;
  f.v = (longlong)i << 32;
  return f;
#else
  return i;
#endif
}

static inline longlong mul3(MKFLOAT scale,longlong tc) {
#ifdef MATROSKA_INTEGER_ONLY
  //             x1 x0
  //             y1 y0
  //    --------------
  //             x0*y0
  //          x1*y0
  //          x0*y1
  //       x1*y1
  //    --------------
  //       .. r1 r0 ..
  //
  //    r = ((x0*y0) >> 32) + (x1*y0) + (x0*y1) + ((x1*y1) << 32)
  unsigned    x0,x1,y0,y1;
  ulonglong   p;
  char              sign = 0;

  if (scale.v < 0)
    sign = !sign, scale.v = -scale.v;
  if (tc < 0)
    sign = !sign, tc = -tc;

  x0 = (unsigned)scale.v;
  x1 = (unsigned)((ulonglong)scale.v >> 32);
  y0 = (unsigned)tc;
  y1 = (unsigned)((ulonglong)tc >> 32);

  p = (ulonglong)x0*y0 >> 32;
  p += (ulonglong)x0*y1;
  p += (ulonglong)x1*y0;
  p += (ulonglong)(x1*y1) << 32;

  return p;
#else
  return (longlong)(scale * tc);
#endif
}

///////////////////////////////////////////////////////////////////////////
// EBML support
static int   readID(MatroskaFile *mf) {
  int        c1,c2,c3,c4;

  c1 = readch(mf);
  if (c1 == EOF)
    return EOF;

  if (c1 & 0x80)
    return c1;

  if ((c1 & 0xf0) == 0)
    errorjmp(mf,"Invalid first byte of EBML ID: %02X",c1);

  c2 = readch(mf);
  if (c2 == EOF)
fail:
    errorjmp(mf,"Got EOF while reading EBML ID");

  if ((c1 & 0xc0) == 0x40)
    return (c1<<8) | c2;

  c3 = readch(mf);
  if (c3 == EOF)
    goto fail;

  if ((c1 & 0xe0) == 0x20)
    return (c1<<16) | (c2<<8) | c3;

  c4 = readch(mf);
  if (c4 == EOF)
    goto fail;

  if ((c1 & 0xf0) == 0x10)
    return (c1<<24) | (c2<<16) | (c3<<8) | c4;

  return 0; // NOT REACHED
}

static ulonglong readVLUIntImp(MatroskaFile *mf,int *mask) {
  int                c,d,m;
  ulonglong        v = 0;

  c = readch(mf);
  if (c == EOF)
    return 0; // XXX should errorjmp()?

  if (c == 0)
    errorjmp(mf,"Invalid first byte of EBML integer: 0");

  for (m=0;;++m) {
    if (c & (0x80 >> m)) {
      c &= 0x7f >> m;
      if (mask)
        *mask = m;
      return v | ((ulonglong)c << m*8);
    }
    d = readch(mf);
    if (d == EOF)
      errorjmp(mf,"Got EOF while reading EBML unsigned integer");
    v = (v<<8) | d;
  }
  // NOT REACHED
}

static inline ulonglong        readVLUInt(MatroskaFile *mf) {
  return readVLUIntImp(mf,NULL);
}

static ulonglong        readSizeUnspec(MatroskaFile *mf) {
  int            m;
  ulonglong v = readVLUIntImp(mf,&m);

  // see if it's unspecified
  if (v == (MAXU64 >> (57-m*7)))
    return MAXU64;

  return v;
}

static ulonglong        readSize(MatroskaFile *mf) {
  ulonglong v = readSizeUnspec(mf);

  // see if it's unspecified
  if (v == MAXU64)
    errorjmp(mf,"Unspecified element size is not supported here.");

  return v;
}

static inline longlong        readVLSInt(MatroskaFile *mf) {
  static longlong bias[8] = { (ONE<<6)-1, (ONE<<13)-1, (ONE<<20)-1, (ONE<<27)-1,
                              (ONE<<34)-1, (ONE<<41)-1, (ONE<<48)-1, (ONE<<55)-1 };

  int            m;
  longlong  v = readVLUIntImp(mf,&m);

  return v - bias[m];
}

static ulonglong  readUInt(MatroskaFile *mf,unsigned int len) {
  int                c;
  unsigned int        m = len;
  ulonglong        v = 0;

  if (len==0)
    return v;
  if (len>8)
    errorjmp(mf,"Unsupported integer size in readUInt: %u",len);

  do {
    c = readch(mf);
    if (c == EOF)
      errorjmp(mf,"Got EOF while reading EBML unsigned integer");
    v = (v<<8) | c;
  } while (--m);

  return v;
}

static inline longlong        readSInt(MatroskaFile *mf,unsigned int len) {
  longlong        v = readUInt(mf,(unsigned)len);
  int                s = 64 - (len<<3);
  return (v << s) >> s;
}

static MKFLOAT readFloat(MatroskaFile *mf,unsigned int len) {
#ifdef MATROSKA_INTEGER_ONLY
  MKFLOAT          f;
  int                  shift;
#else
  union {
    unsigned int  ui;
    ulonglong          ull;
    float          f;
    double          d;
  } u;
#endif

  if (len!=4 && len!=8)
    errorjmp(mf,"Invalid float size in readFloat: %u",len);

#ifdef MATROSKA_INTEGER_ONLY
  if (len == 4) {
    unsigned  ui = (unsigned)readUInt(mf,(unsigned)len);
    f.v = (ui & 0x7fffff) | 0x800000;
    if (ui & 0x80000000)
      f.v = -f.v;
    shift = (ui >> 23) & 0xff;
    if (shift == 0) // assume 0
zero:
      shift = 0, f.v = 0;
    else if (shift == 255)
inf:
      if (ui & 0x80000000)
        f.v = LL(0x8000000000000000);
      else
        f.v = LL(0x7fffffffffffffff);
    else {
      shift += -127 + 9;
      if (shift > 39)
        goto inf;
shift:
      if (shift < 0)
        f.v = f.v >> -shift;
      else if (shift > 0)
        f.v = f.v << shift;
    }
  } else if (len == 8) {
    ulonglong  ui = readUInt(mf,(unsigned)len);
    f.v = (ui & LL(0xfffffffffffff)) | LL(0x10000000000000);
    if (ui & 0x80000000)
      f.v = -f.v;
    shift = (int)((ui >> 52) & 0x7ff);
    if (shift == 0) // assume 0
      goto zero;
    else if (shift == 2047)
      goto inf;
    else {
      shift += -1023 - 20;
      if (shift > 10)
        goto inf;
      goto shift;
    }
  }

  return f;
#else
  if (len==4) {
    u.ui = (unsigned int)readUInt(mf,(unsigned)len);
    return u.f;
  }

  if (len==8) {
    u.ull = readUInt(mf,(unsigned)len);
    return u.d;
  }

  return 0;
#endif
}

static void readString(MatroskaFile *mf,ulonglong len,char *buffer,int buflen) {
  int          nread;

  if (buflen<1)
    errorjmp(mf,"Invalid buffer size in readString: %d",buflen);

  nread = buflen - 1;

  if (nread > len)
    nread = (int)len;

  readbytes(mf,buffer,nread);
  len -= nread;

  if (len>0)
    skipbytes(mf,len);

  buffer[nread] = '\0';
}

static void readLangCC(MatroskaFile *mf, ulonglong len, char lcc[4]) {
  unsigned  todo = len > 3 ? 3 : (int)len;

  lcc[0] = lcc[1] = lcc[2] = lcc[3] = 0;
  readbytes(mf, lcc, todo);
  skipbytes(mf, len - todo);
}

///////////////////////////////////////////////////////////////////////////
// file parser
#define        FOREACH2(f,tl,clid) \
  { \
    ulonglong        tmplen = (tl); \
    { \
      ulonglong   start = filepos(f); \
      ulonglong   cur,len; \
      int              id; \
      for (;;) { \
        cur = filepos(mf); \
        if (tmplen != MAXU64 && cur == start + tmplen) \
          break; \
        id = readID(f); \
        if (id==EOF) \
          errorjmp(mf,"Unexpected EOF while reading EBML container"); \
        len = id == clid ? readSizeUnspec(mf) : readSize(mf); \
        switch (id) {

#define FOREACH(f,tl) FOREACH2(f,tl,EOF)

#define RESTART()     (tmplen=len),(start=cur)

#define        ENDFOR1(f) \
        default: \
          skipbytes(f,len); \
          break; \
        }
#define        ENDFOR2() \
      } \
    } \
  }

#define        ENDFOR(f) ENDFOR1(f) ENDFOR2()

#define        myalloca(f,c) alloca(c)
#define        STRGETF(f,v,len,func) \
  { \
    char *TmpVal; \
    unsigned TmpLen = (len)>MAX_STRING_LEN ? MAX_STRING_LEN : (unsigned)(len); \
    TmpVal = func(f->cache,TmpLen+1); \
    if (TmpVal == NULL) \
      errorjmp(mf,"Out of memory"); \
    readString(f,len,TmpVal,TmpLen+1); \
    (v) = TmpVal; \
  }

#define        STRGETA(f,v,len)  STRGETF(f,v,len,myalloca)
#define        STRGETM(f,v,len)  STRGETF(f,v,len,f->cache->memalloc)

static int  IsWritingApp(MatroskaFile *mf,const char *str) {
  const char  *cp = mf->Seg.WritingApp;
  if (!cp)
    return 0;

  while (*str && *str++==*cp++) ;

  return !*str;
}

static void parseEBML(MatroskaFile *mf,ulonglong toplen) {
  ulonglong v;
  char            buf[32];

  FOREACH(mf,toplen)
    case 0x4286: // Version
      v = readUInt(mf,(unsigned)len);
      break;
    case 0x42f7: // ReadVersion
      v = readUInt(mf,(unsigned)len);
      if (v > EBML_VERSION)
        errorjmp(mf,"File requires version %d EBML parser",(int)v);
      break;
    case 0x42f2: // MaxIDLength
      v = readUInt(mf,(unsigned)len);
      if (v > EBML_MAX_ID_LENGTH)
        errorjmp(mf,"File has identifiers longer than %d",(int)v);
      break;
    case 0x42f3: // MaxSizeLength
      v = readUInt(mf,(unsigned)len);
      if (v > EBML_MAX_SIZE_LENGTH)
        errorjmp(mf,"File has integers longer than %d",(int)v);
      break;
    case 0x4282: // DocType
      readString(mf,len,buf,sizeof(buf));
      if (strcmp(buf,MATROSKA_DOCTYPE) != 0 && strcmp(buf,WEBM_DOCTYPE) != 0)
        errorjmp(mf,"Unsupported DocType: %s",buf);
      break;
    case 0x4287: // DocTypeVersion
      v = readUInt(mf,(unsigned)len);
      break;
    case 0x4285: // DocTypeReadVersion
      v = readUInt(mf,(unsigned)len);
      if (v > MATROSKA_VERSION)
        errorjmp(mf,"File requires version %d Matroska parser",(int)v);
      break;
  ENDFOR(mf);
}

static void parseSeekEntry(MatroskaFile *mf,ulonglong toplen) {
  int            seekid = 0;
  ulonglong pos = (ulonglong)-1;

  FOREACH(mf,toplen)
    case 0x53ab: // SeekID
      if (len>EBML_MAX_ID_LENGTH)
        errorjmp(mf,"Invalid ID size in parseSeekEntry: %d\n",(int)len);
      seekid = (int)readUInt(mf,(unsigned)len);
      break;
    case 0x53ac: // SeekPos
      pos = readUInt(mf,(unsigned)len);
      break;
  ENDFOR(mf);

  if (pos == (ulonglong)-1)
    errorjmp(mf,"Invalid element position in parseSeekEntry");

  pos += mf->pSegment;
  switch (seekid) {
    case 0x114d9b74: // next SeekHead
      if (mf->pSeekHead)
        errorjmp(mf,"SeekHead contains more than one SeekHead pointer");
      mf->pSeekHead = pos;
      break;
    case 0x1549a966: // SegmentInfo
      mf->pSegmentInfo = pos;
      break;
    case 0x1f43b675: // Cluster
      if (!mf->pCluster)
        mf->pCluster = pos;
      break;
    case 0x1654ae6b: // Tracks
      mf->pTracks = pos;
      break;
    case 0x1c53bb6b: // Cues
      mf->pCues = pos;
      break;
    case 0x1941a469: // Attachments
      mf->pAttachments = pos;
      break;
    case 0x1043a770: // Chapters
      mf->pChapters = pos;
      break;
    case 0x1254c367: // tags
      mf->pTags = pos;
      break;
  }
}

static void parseSeekHead(MatroskaFile *mf,ulonglong toplen) {
  FOREACH(mf,toplen)
    case 0x4dbb:
      parseSeekEntry(mf,len);
      break;
  ENDFOR(mf);
}

static void parseSegmentInfo(MatroskaFile *mf,ulonglong toplen) {
  MKFLOAT     duration = mkfi(0);

  if (mf->seen.SegmentInfo) {
    skipbytes(mf,toplen);
    return;
  }

  mf->seen.SegmentInfo = 1;
  mf->Seg.TimecodeScale = 1000000; // Default value

  FOREACH(mf,toplen)
    case 0x73a4: // SegmentUID
      if (len!=sizeof(mf->Seg.UID))
        errorjmp(mf,"SegmentUID size is not %d bytes",mf->Seg.UID);
      readbytes(mf,mf->Seg.UID,sizeof(mf->Seg.UID));
      break;
    case 0x7384: // SegmentFilename
      STRGETM(mf,mf->Seg.Filename,len);
      break;
    case 0x3cb923: // PrevUID
      if (len!=sizeof(mf->Seg.PrevUID))
        errorjmp(mf,"PrevUID size is not %d bytes",mf->Seg.PrevUID);
      readbytes(mf,mf->Seg.PrevUID,sizeof(mf->Seg.PrevUID));
      break;
    case 0x3c83ab: // PrevFilename
      STRGETM(mf,mf->Seg.PrevFilename,len);
      break;
    case 0x3eb923: // NextUID
      if (len!=sizeof(mf->Seg.NextUID))
        errorjmp(mf,"NextUID size is not %d bytes",mf->Seg.NextUID);
      readbytes(mf,mf->Seg.NextUID,sizeof(mf->Seg.NextUID));
      break;
    case 0x3e83bb: // NextFilename
      STRGETM(mf,mf->Seg.NextFilename,len);
      break;
    case 0x2ad7b1: // TimecodeScale
      mf->Seg.TimecodeScale = readUInt(mf,(unsigned)len);
      if (mf->Seg.TimecodeScale == 0)
        errorjmp(mf,"Segment timecode scale is zero");
      break;
    case 0x4489: // Duration
      duration = readFloat(mf,(unsigned)len);
      break;
    case 0x4461: // DateUTC
      mf->Seg.DateUTC = readUInt(mf,(unsigned)len);
      mf->Seg.DateUTCValid = 1;
      break;
    case 0x7ba9: // Title
      STRGETM(mf,mf->Seg.Title,len);
      break;
    case 0x4d80: // MuxingApp
      STRGETM(mf,mf->Seg.MuxingApp,len);
      break;
    case 0x5741: // WritingApp
      STRGETM(mf,mf->Seg.WritingApp,len);
      break;
  ENDFOR(mf);

  mf->Seg.Duration = mul3(duration,mf->Seg.TimecodeScale);
}

static void parseFirstCluster(MatroskaFile *mf,ulonglong toplen) {
  int       seenTimecode = 0, seenBlock = 0;
  longlong  tc;
  ulonglong clstart = filepos(mf);

  mf->seen.Cluster = 1;
  mf->firstTimecode = 0;

  FOREACH2(mf,toplen,0x1f43b675)
    case 0xe7: // Timecode
      tc = readUInt(mf,(unsigned)len);
      if (!seenTimecode) {
        seenTimecode = 1;
        mf->firstTimecode += tc;
      }

      if (seenBlock) {
out:
        if (toplen != MAXU64)
          skipbytes(mf,clstart + toplen - filepos(mf));
        else if (len != MAXU64)
          skipbytes(mf,cur + len - filepos(mf));
        return;
      }
      break;
    case 0xa3: // BlockEx
      readVLUInt(mf); // track number
      tc = readSInt(mf, 2);
      if (!seenBlock) {
        seenBlock = 1;
        mf->firstTimecode += tc;
      }

      if (seenTimecode)
        goto out;
      break;
    case 0xa0: // BlockGroup
      FOREACH(mf,len)
        case 0xa1: // Block
          readVLUInt(mf); // track number
          tc = readSInt(mf,2);
          if (!seenBlock) {
            seenBlock = 1;
            mf->firstTimecode += tc;
          }

          if (seenTimecode)
            goto out;
      ENDFOR(mf);
      break;
    case 0x1f43b675:
      return;
  ENDFOR(mf);
}

static void parseVideoInfo(MatroskaFile *mf,ulonglong toplen,struct TrackInfo *ti) {
  ulonglong v;
  char            dW = 0, dH = 0;

  FOREACH(mf,toplen)
    case 0x9a: // FlagInterlaced
      ti->AV.Video.Interlaced = readUInt(mf,(unsigned)len)!=0;
      break;
    case 0x53b8: // StereoMode
      v = readUInt(mf,(unsigned)len);
      if (v>3)
        errorjmp(mf,"Invalid stereo mode");
      ti->AV.Video.StereoMode = (unsigned char)v;
      break;
    case 0xb0: // PixelWidth
      v = readUInt(mf,(unsigned)len);
      if (v>0xffffffff)
        errorjmp(mf,"PixelWidth is too large");
      ti->AV.Video.PixelWidth = (unsigned)v;
      if (!dW)
        ti->AV.Video.DisplayWidth = ti->AV.Video.PixelWidth;
      break;
    case 0xba: // PixelHeight
      v = readUInt(mf,(unsigned)len);
      if (v>0xffffffff)
        errorjmp(mf,"PixelHeight is too large");
      ti->AV.Video.PixelHeight = (unsigned)v;
      if (!dH)
        ti->AV.Video.DisplayHeight = ti->AV.Video.PixelHeight;
      break;
    case 0x54b0: // DisplayWidth
      v = readUInt(mf,(unsigned)len);
      if (v>0xffffffff)
        errorjmp(mf,"DisplayWidth is too large");
      ti->AV.Video.DisplayWidth = (unsigned)v;
      dW = 1;
      break;
    case 0x54ba: // DisplayHeight
      v = readUInt(mf,(unsigned)len);
      if (v>0xffffffff)
        errorjmp(mf,"DisplayHeight is too large");
      ti->AV.Video.DisplayHeight = (unsigned)v;
      dH = 1;
      break;
    case 0x54b2: // DisplayUnit
      v = readUInt(mf,(unsigned)len);
      ti->AV.Video.DisplayUnit = (unsigned char)v;
      break;
    case 0x54b3: // AspectRatioType
      v = readUInt(mf,(unsigned)len);
      ti->AV.Video.AspectRatioType = (unsigned char)v;
      break;
    case 0x54aa: // PixelCropBottom
      v = readUInt(mf,(unsigned)len);
      if (v>0xffffffff)
        errorjmp(mf,"PixelCropBottom is too large");
      ti->AV.Video.CropB = (unsigned)v;
      break;
    case 0x54bb: // PixelCropTop
      v = readUInt(mf,(unsigned)len);
      if (v>0xffffffff)
        errorjmp(mf,"PixelCropTop is too large");
      ti->AV.Video.CropT = (unsigned)v;
      break;
    case 0x54cc: // PixelCropLeft
      v = readUInt(mf,(unsigned)len);
      if (v>0xffffffff)
        errorjmp(mf,"PixelCropLeft is too large");
      ti->AV.Video.CropL = (unsigned)v;
      break;
    case 0x54dd: // PixelCropRight
      v = readUInt(mf,(unsigned)len);
      if (v>0xffffffff)
        errorjmp(mf,"PixelCropRight is too large");
      ti->AV.Video.CropR = (unsigned)v;
      break;
    case 0x2eb524: // ColourSpace
      ti->AV.Video.ColourSpace = (unsigned)readUInt(mf,4);
      break;
    case 0x2fb523: // GammaValue
      ti->AV.Video.GammaValue = readFloat(mf,(unsigned)len);
      break;
  ENDFOR(mf);
}

static void parseAudioInfo(MatroskaFile *mf,ulonglong toplen,struct TrackInfo *ti) {
  ulonglong   v;

  FOREACH(mf,toplen)
    case 0xb5: // SamplingFrequency
      ti->AV.Audio.SamplingFreq = readFloat(mf,(unsigned)len);
      break;
    case 0x78b5: // OutputSamplingFrequency
      ti->AV.Audio.OutputSamplingFreq = readFloat(mf,(unsigned)len);
      break;
    case 0x9f: // Channels
      v = readUInt(mf,(unsigned)len);
      if (v<1 || v>255)
        errorjmp(mf,"Invalid Channels value");
      ti->AV.Audio.Channels = (unsigned char)v;
      break;
    case 0x7d7b: // ChannelPositions
      skipbytes(mf,len);
      break;
    case 0x6264: // BitDepth
      v = readUInt(mf,(unsigned)len);
#if 0
      if ((v<1 || v>255) && !IsWritingApp(mf,"AVI-Mux GUI"))
        errorjmp(mf,"Invalid BitDepth: %d",(int)v);
#endif
      ti->AV.Audio.BitDepth = (unsigned char)v;
      break;
  ENDFOR(mf);

  if (ti->AV.Audio.Channels == 0)
    ti->AV.Audio.Channels = 1;
  if (mkv_TruncFloat(ti->AV.Audio.SamplingFreq) == 0)
    ti->AV.Audio.SamplingFreq = mkfi(8000);
  if (mkv_TruncFloat(ti->AV.Audio.OutputSamplingFreq)==0)
    ti->AV.Audio.OutputSamplingFreq = ti->AV.Audio.SamplingFreq;
}

static void CopyStr(char **src,char **dst) {
  size_t l;

  if (!*src)
    return;

  l = strlen(*src)+1;
  memcpy(*dst,*src,l);
  *src = *dst;
  *dst += l;
}

static void parseTrackEntry(MatroskaFile *mf,ulonglong toplen) {
  struct TrackInfo  t,*tp,**tpp;
  ulonglong            v;
  char                    *cp = NULL, *cs = NULL;
  size_t            cplen = 0, cslen = 0, cpadd = 0;
  unsigned            CompScope, num_comp = 0;

  if (mf->nTracks >= MAX_TRACKS)
    errorjmp(mf,"Too many tracks.");

  // clear track info
  memset(&t,0,sizeof(t));

  // fill default values
  t.Enabled = 1;
  t.Default = 1;
  t.Lacing = 1;
  t.TimecodeScale = mkfi(1);
  t.DecodeAll = 1;

  FOREACH(mf,toplen)
    case 0xd7: // TrackNumber
      v = readUInt(mf,(unsigned)len);
      if (v>255)
        errorjmp(mf,"Track number is >255 (%d)",(int)v);
      t.Number = (unsigned char)v;
      break;
    case 0x73c5: // TrackUID
      t.UID = readUInt(mf,(unsigned)len);
      break;
    case 0x83: // TrackType
      v = readUInt(mf,(unsigned)len);
      if (v<1 || v>254)
        errorjmp(mf,"Invalid track type: %d",(int)v);
      t.Type = (unsigned char)v;
      break;
    case 0xb9: // Enabled
      t.Enabled = readUInt(mf,(unsigned)len)!=0;
      break;
    case 0x88: // Default
      t.Default = readUInt(mf,(unsigned)len)!=0;
      break;
    case 0x9c: // Lacing
      t.Lacing = readUInt(mf,(unsigned)len)!=0;
      break;
    case 0x6de7: // MinCache
      v = readUInt(mf,(unsigned)len);
      if (v > 0xffffffff)
        errorjmp(mf,"MinCache is too large");
      t.MinCache = (unsigned)v;
      break;
    case 0x6df8: // MaxCache
      v = readUInt(mf,(unsigned)len);
      if (v > 0xffffffff)
        errorjmp(mf,"MaxCache is too large");
      t.MaxCache = (unsigned)v;
      break;
    case 0x23e383: // DefaultDuration
      t.DefaultDuration = readUInt(mf,(unsigned)len);
      break;
    case 0x23314f: // TrackTimecodeScale
      t.TimecodeScale = readFloat(mf,(unsigned)len);
      break;
    case 0x55ee: // MaxBlockAdditionID
      t.MaxBlockAdditionID = (unsigned)readUInt(mf,(unsigned)len);
      break;
    case 0x536e: // Name
      if (t.Name)
        errorjmp(mf,"Duplicate Track Name");
      STRGETA(mf,t.Name,len);
      break;
    case 0x22b59c: // Language
      readLangCC(mf, len, t.Language);
      break;
    case 0x86: // CodecID
      if (t.CodecID)
        errorjmp(mf,"Duplicate CodecID");
      STRGETA(mf,t.CodecID,len);
      break;
    case 0x63a2: // CodecPrivate
      if (cp)
        errorjmp(mf,"Duplicate CodecPrivate");
      if (len>262144) // 256KB
        errorjmp(mf,"CodecPrivate is too large: %d",(int)len);
      cplen = (unsigned)len;
      cp = alloca(cplen);
      readbytes(mf,cp,(int)cplen);
      break;
    case 0x258688: // CodecName
      skipbytes(mf,len);
      break;
    case 0x3a9697: // CodecSettings
      skipbytes(mf,len);
      break;
    case 0x3b4040: // CodecInfoURL
      skipbytes(mf,len);
      break;
    case 0x26b240: // CodecDownloadURL
      skipbytes(mf,len);
      break;
    case 0xaa: // CodecDecodeAll
      t.DecodeAll = readUInt(mf,(unsigned)len)!=0;
      break;
    case 0x6fab: // TrackOverlay
      v = readUInt(mf,(unsigned)len);
      if (v>255)
        errorjmp(mf,"Track number in TrackOverlay is too large: %d",(int)v);
      t.TrackOverlay = (unsigned char)v;
      break;
    case 0xe0: // VideoInfo
      parseVideoInfo(mf,len,&t);
      break;
    case 0xe1: // AudioInfo
      parseAudioInfo(mf,len,&t);
      break;
    case 0x6d80: // ContentEncodings
      FOREACH(mf,len)
        case 0x6240: // ContentEncoding
          // fill in defaults
          t.CompEnabled = 1;
          t.CompMethod = COMP_ZLIB;
          CompScope = 1;
          if (++num_comp > 1)
            return; // only one compression layer supported
          FOREACH(mf,len)
            case 0x5031: // ContentEncodingOrder
              readUInt(mf,(unsigned)len);
              break;
            case 0x5032: // ContentEncodingScope
              CompScope = (unsigned)readUInt(mf,(unsigned)len);
              break;
            case 0x5033: // ContentEncodingType
              if (readUInt(mf,(unsigned)len) != 0)
                return; // encryption is not supported
              break;
            case 0x5034: // ContentCompression
              FOREACH(mf,len)
                case 0x4254: // ContentCompAlgo
                  v = readUInt(mf,(unsigned)len);
                  t.CompEnabled = 1;
                  switch (v) {
                    case 0: // Zlib
                      t.CompMethod = COMP_ZLIB;
                      break;
                    case 3: // prepend fixed data
                      t.CompMethod = COMP_PREPEND;
                      break;
                    default:
                      return; // unsupported compression, skip track
                  }
                  break;
                case 0x4255: // ContentCompSettings
                  if (len > 256)
                    return;
                  cslen = (unsigned)len;
                  cs = alloca(cslen);
                  readbytes(mf, cs, (int)cslen);
                  break;
              ENDFOR(mf);
              break;
              // TODO Implement Encryption/Signatures
          ENDFOR(mf);
          break;
      ENDFOR(mf);
      break;
  ENDFOR(mf);

  // validate track info
  if (!t.CodecID)
    errorjmp(mf,"Track has no Codec ID");

  if (t.UID != 0) {
    unsigned  i;
    for (i = 0; i < mf->nTracks; ++i)
      if (mf->Tracks[i]->UID == t.UID) // duplicate track entry
        return;
  }

#ifdef MATROSKA_COMPRESSION_SUPPORT
  // handle compressed CodecPrivate
  if (t.CompEnabled && t.CompMethod == COMP_ZLIB && (CompScope & 2) && cplen > 0) {
    z_stream  zs;
    Bytef     tmp[64], *ncp;
    int              code;
    uLong     ncplen;

    memset(&zs,0,sizeof(zs));
    if (inflateInit(&zs) != Z_OK)
      errorjmp(mf, "inflateInit failed");

    zs.next_in = (Bytef *)cp;
    zs.avail_in = (uInt)cplen;

    do {
      zs.next_out = tmp;
      zs.avail_out = sizeof(tmp);

      code = inflate(&zs, Z_NO_FLUSH);
    } while (code == Z_OK);

    if (code != Z_STREAM_END)
      errorjmp(mf, "invalid compressed data in CodecPrivate");

    ncplen = zs.total_out;
    ncp = alloca(ncplen);

    inflateReset(&zs);

    zs.next_in = (Bytef *)cp;
    zs.avail_in = (uInt)cplen;
    zs.next_out = ncp;
    zs.avail_out = ncplen;

    if (inflate(&zs, Z_FINISH) != Z_STREAM_END)
      errorjmp(mf, "inflate failed");

    inflateEnd(&zs);

    cp = (char *)ncp;
    cplen = ncplen;
  }
#endif

  if (t.CompEnabled && !(CompScope & 1)) {
    t.CompEnabled = 0;
    cslen = 0;
  }

  // allocate new track
  tpp = AGET(mf,Tracks);

  // copy strings
  if (t.Name)
    cpadd += strlen(t.Name)+1;
  if (t.CodecID)
    cpadd += strlen(t.CodecID)+1;

  tp = mf->cache->memalloc(mf->cache,sizeof(*tp) + cplen + cslen + cpadd);
  if (tp == NULL)
    errorjmp(mf,"Out of memory");

  memcpy(tp,&t,sizeof(*tp));
  if (cplen) {
    tp->CodecPrivate = tp+1;
    tp->CodecPrivateSize = (unsigned)cplen;
    memcpy(tp->CodecPrivate,cp,cplen);
  }
  if (cslen) {
    tp->CompMethodPrivate = (char *)(tp+1) + cplen;
    tp->CompMethodPrivateSize = (unsigned)cslen;
    memcpy(tp->CompMethodPrivate, cs, cslen);
  }

  cp = (char*)(tp+1) + cplen + cslen;
  CopyStr(&tp->Name,&cp);
  CopyStr(&tp->CodecID,&cp);

  // set default language
  if (!tp->Language[0])
    memcpy(tp->Language, "eng", 4);

  *tpp = tp;
}

static void parseTracks(MatroskaFile *mf,ulonglong toplen) {
  mf->seen.Tracks = 1;
  FOREACH(mf,toplen)
    case 0xae: // TrackEntry
      parseTrackEntry(mf,len);
      break;
  ENDFOR(mf);
}

static void addCue(MatroskaFile *mf,ulonglong pos,ulonglong timecode) {
  struct Cue  *cc = AGET(mf,Cues);
  cc->Time = timecode;
  cc->Position = pos;
  cc->Track = 0;
  cc->Block = 0;
}

static void fixupCues(MatroskaFile *mf) {
  // adjust cues, shift cues if file does not start at 0
  unsigned  i;
  longlong  adjust = mf->firstTimecode * mf->Seg.TimecodeScale;

  for (i=0;i<mf->nCues;++i) {
    mf->Cues[i].Time *= mf->Seg.TimecodeScale;
    mf->Cues[i].Time -= adjust;
  }
}

static void parseCues(MatroskaFile *mf,ulonglong toplen) {
  jmp_buf     jb;
  ulonglong   v;
  struct Cue  cc;
  unsigned    i,j,k;

  mf->seen.Cues = 1;
  mf->nCues = 0;
  cc.Block = 0;

  memcpy(&jb,&mf->jb,sizeof(jb));

  if (setjmp(mf->jb)) {
    memcpy(&mf->jb,&jb,sizeof(jb));
    mf->nCues = 0;
    mf->seen.Cues = 0;
    return;
  }

  FOREACH(mf,toplen)
    case 0xbb: // CuePoint
      FOREACH(mf,len)
        case 0xb3: // CueTime
          cc.Time = readUInt(mf,(unsigned)len);
          break;
        case 0xb7: // CueTrackPositions
          FOREACH(mf,len)
            case 0xf7: // CueTrack
              v = readUInt(mf,(unsigned)len);
              if (v>255)
                errorjmp(mf,"CueTrack points to an invalid track: %d",(int)v);
              cc.Track = (unsigned char)v;
              break;
            case 0xf1: // CueClusterPosition
              cc.Position = readUInt(mf,(unsigned)len);
              break;
            case 0x5378: // CueBlockNumber
              cc.Block = readUInt(mf,(unsigned)len);
              break;
            case 0xea: // CodecState
              readUInt(mf,(unsigned)len);
              break;
            case 0xdb: // CueReference
              FOREACH(mf,len)
                case 0x96: // CueRefTime
                  readUInt(mf,(unsigned)len);
                  break;
                case 0x97: // CueRefCluster
                  readUInt(mf,(unsigned)len);
                  break;
                case 0x535f: // CueRefNumber
                  readUInt(mf,(unsigned)len);
                  break;
                case 0xeb: // CueRefCodecState
                  readUInt(mf,(unsigned)len);
                  break;
              ENDFOR(mf);
              break;
          ENDFOR(mf);
          break;
      ENDFOR(mf);

      if (mf->nCues == 0 && mf->pCluster - mf->pSegment != cc.Position)
        addCue(mf,mf->pCluster - mf->pSegment,mf->firstTimecode);

      memcpy(AGET(mf,Cues),&cc,sizeof(cc));
      break;
  ENDFOR(mf);

  memcpy(&mf->jb,&jb,sizeof(jb));

  ARELEASE(mf,mf,Cues);

  // bubble sort the cues and fuck the losers that write unordered cues
  if (mf->nCues > 0)
    for (i = mf->nCues - 1, k = 1; i > 0 && k > 0; --i)
      for (j = k = 0; j < i; ++j)
        if (mf->Cues[j].Time > mf->Cues[j+1].Time) {
          struct Cue tmp = mf->Cues[j+1];
          mf->Cues[j+1] = mf->Cues[j];
          mf->Cues[j] = tmp;
          ++k;
        }
}

static void parseAttachment(MatroskaFile *mf,ulonglong toplen) {
  struct Attachment a,*pa;

  memset(&a,0,sizeof(a));
  FOREACH(mf,toplen)
    case 0x467e: // Description
      STRGETA(mf,a.Description,len);
      break;
    case 0x466e: // Name
      STRGETA(mf,a.Name,len);
      break;
    case 0x4660: // MimeType
      STRGETA(mf,a.MimeType,len);
      break;
    case 0x46ae: // UID
      a.UID = readUInt(mf,(unsigned)len);
      break;
    case 0x465c: // Data
      a.Position = filepos(mf);
      a.Length = len;
      skipbytes(mf,len);
      break;
  ENDFOR(mf);

  if (!a.Position)
    return;

  pa = AGET(mf,Attachments);
  memcpy(pa,&a,sizeof(a));

  if (a.Description)
    pa->Description = mystrdup(mf->cache,a.Description);
  if (a.Name)
    pa->Name = mystrdup(mf->cache,a.Name);
  if (a.MimeType)
    pa->MimeType = mystrdup(mf->cache,a.MimeType);
}

static void parseAttachments(MatroskaFile *mf,ulonglong toplen) {
  mf->seen.Attachments = 1;

  FOREACH(mf,toplen)
    case 0x61a7: // AttachedFile
      parseAttachment(mf,len);
      break;
  ENDFOR(mf);
}

static void parseChapter(MatroskaFile *mf,ulonglong toplen,struct Chapter *parent) {
  struct ChapterDisplay        *disp;
  struct ChapterProcess        *proc;
  struct ChapterCommand        *cmd;
  struct Chapter        *ch = ASGET(mf,parent,Children);

  memset(ch,0,sizeof(*ch));

  ch->Enabled = 1;

  FOREACH(mf,toplen)
    case 0x73c4: // ChapterUID
      ch->UID = readUInt(mf,(unsigned)len);
      break;
    case 0x6e67: // ChapterSegmentUID
      if (len != sizeof(ch->SegmentUID))
        skipbytes(mf, len);
      else
        readbytes(mf, ch->SegmentUID, sizeof(ch->SegmentUID));
      break;
    case 0x91: // ChapterTimeStart
      ch->Start = readUInt(mf,(unsigned)len);
      break;
    case 0x92: // ChapterTimeEnd
      ch->End = readUInt(mf,(unsigned)len);
      break;
    case 0x98: // ChapterFlagHidden
      ch->Hidden = readUInt(mf,(unsigned)len)!=0;
      break;
    case 0x4598: // ChapterFlagEnabled
      ch->Enabled = readUInt(mf,(unsigned)len)!=0;
      break;
    case 0x8f: // ChapterTrack
      FOREACH(mf,len)
        case 0x89: // ChapterTrackNumber
          *(ulonglong*)(ASGET(mf,ch,Tracks)) = readUInt(mf,(unsigned)len);
          break;
      ENDFOR(mf);
      break;
    case 0x80: // ChapterDisplay
      disp = NULL;

      FOREACH(mf,len)
        case 0x85: // ChapterString
          if (disp==NULL) {
            disp = ASGET(mf,ch,Display);
            memset(disp, 0, sizeof(*disp));
          }
          if (disp->String)
            skipbytes(mf,len); // Ignore duplicate string
          else
            STRGETM(mf,disp->String,len);
          break;
        case 0x437c: // ChapterLanguage
          if (disp==NULL) {
            disp = ASGET(mf,ch,Display);
            memset(disp, 0, sizeof(*disp));
          }
          readLangCC(mf, len, disp->Language);
          break;
        case 0x437e: // ChapterCountry
          if (disp==NULL) {
            disp = ASGET(mf,ch,Display);
            memset(disp, 0, sizeof(*disp));
          }
          readLangCC(mf, len, disp->Country);
          break;
      ENDFOR(mf);

      if (disp && !disp->String)
        --ch->nDisplay;
      break;
    case 0x6944: // ChapProcess
      proc = NULL;

      FOREACH(mf,len)
        case 0x6955: // ChapProcessCodecID
          if (proc == NULL) {
            proc = ASGET(mf, ch, Process);
            memset(proc, 0, sizeof(*proc));
          }
          proc->CodecID = (unsigned)readUInt(mf,(unsigned)len);
          break;
        case 0x450d: // ChapProcessPrivate
          if (proc == NULL) {
            proc = ASGET(mf, ch, Process);
            memset(proc, 0, sizeof(*proc));
          }
          if (proc->CodecPrivate)
            skipbytes(mf, len);
          else {
            proc->CodecPrivateLength = (unsigned)len;
            STRGETM(mf,proc->CodecPrivate,len);
          }
          break;
        case 0x6911: // ChapProcessCommand
          if (proc == NULL) {
            proc = ASGET(mf, ch, Process);
            memset(proc, 0, sizeof(*proc));
          }

          cmd = NULL;

          FOREACH(mf,len)
            case 0x6922: // ChapterCommandTime
              if (cmd == NULL) {
                cmd = ASGET(mf,proc,Commands);
                memset(cmd, 0, sizeof(*cmd));
              }
              cmd->Time = (unsigned)readUInt(mf,(unsigned)len);
              break;
            case 0x6933: // ChapterCommandString
              if (cmd == NULL) {
                cmd = ASGET(mf,proc,Commands);
                memset(cmd, 0, sizeof(*cmd));
              }
              if (cmd->Command)
                skipbytes(mf,len);
              else {
                cmd->CommandLength = (unsigned)len;
                STRGETM(mf,cmd->Command,len);
              }
              break;
          ENDFOR(mf);

          if (cmd && !cmd->Command)
            --proc->nCommands;
          break;
      ENDFOR(mf);

      if (proc && !proc->nCommands)
        --ch->nProcess;
      break;
    case 0xb6: // Nested ChapterAtom
      parseChapter(mf,len,ch);
      break;
  ENDFOR(mf);

  ARELEASE(mf,ch,Tracks);
  ARELEASE(mf,ch,Display);
  ARELEASE(mf,ch,Children);
}

static void parseChapters(MatroskaFile *mf,ulonglong toplen) {
  struct Chapter  *ch;

  mf->seen.Chapters = 1;

  FOREACH(mf,toplen)
    case 0x45b9: // EditionEntry
        ch = AGET(mf,Chapters);
        memset(ch, 0, sizeof(*ch));
         FOREACH(mf,len)
          case 0x45bc: // EditionUID
            ch->UID = readUInt(mf,(unsigned)len);
            break;
          case 0x45bd: // EditionFlagHidden
            ch->Hidden = readUInt(mf,(unsigned)len)!=0;
            break;
          case 0x45db: // EditionFlagDefault
            ch->Default = readUInt(mf,(unsigned)len)!=0;
            break;
          case 0x45dd: // EditionFlagOrdered
            ch->Ordered = readUInt(mf,(unsigned)len)!=0;
            break;
          case 0xb6: // ChapterAtom
            parseChapter(mf,len,ch);
            break;
        ENDFOR(mf);
      break;
  ENDFOR(mf);
}

static void parseTags(MatroskaFile *mf,ulonglong toplen) {
  struct Tag  *tag;
  struct Target *target;
  struct SimpleTag *st;

  mf->seen.Tags = 1;

  FOREACH(mf,toplen)
    case 0x7373: // Tag
      tag = AGET(mf,Tags);
      memset(tag,0,sizeof(*tag));

      FOREACH(mf,len)
        case 0x63c0: // Targets
          FOREACH(mf,len)
            case 0x63c5: // TrackUID
              target = ASGET(mf,tag,Targets);
              target->UID = readUInt(mf,(unsigned)len);
              target->Type = TARGET_TRACK;
              break;
            case 0x63c4: // ChapterUID
              target = ASGET(mf,tag,Targets);
              target->UID = readUInt(mf,(unsigned)len);
              target->Type = TARGET_CHAPTER;
              break;
            case 0x63c6: // AttachmentUID
              target = ASGET(mf,tag,Targets);
              target->UID = readUInt(mf,(unsigned)len);
              target->Type = TARGET_ATTACHMENT;
              break;
            case 0x63c9: // EditionUID
              target = ASGET(mf,tag,Targets);
              target->UID = readUInt(mf,(unsigned)len);
              target->Type = TARGET_EDITION;
              break;
          ENDFOR(mf);
          break;
        case 0x67c8: // SimpleTag
          st = ASGET(mf,tag,SimpleTags);
          memset(st,0,sizeof(*st));

          FOREACH(mf,len)
            case 0x45a3: // TagName
              if (st->Name)
                skipbytes(mf,len);
              else
                STRGETM(mf,st->Name,len);
              break;
            case 0x4487: // TagString
              if (st->Value)
                skipbytes(mf,len);
              else
                STRGETM(mf,st->Value,len);
              break;
            case 0x447a: // TagLanguage
              readLangCC(mf, len, st->Language);
              break;
            case 0x4484: // TagDefault
              st->Default = readUInt(mf,(unsigned)len)!=0;
              break;
          ENDFOR(mf);

          if (!st->Name || !st->Value) {
            mf->cache->memfree(mf->cache,st->Name);
            mf->cache->memfree(mf->cache,st->Value);
            --tag->nSimpleTags;
          }
          break;
      ENDFOR(mf);
      break;
  ENDFOR(mf);
}

static void parseContainer(MatroskaFile *mf) {
  ulonglong len;
  int            id = readID(mf);
  if (id==EOF)
    errorjmp(mf,"Unexpected EOF in parseContainer");

  len = readSize(mf);

  switch (id) {
    case 0x1549a966: // SegmentInfo
      parseSegmentInfo(mf,len);
      break;
    case 0x1f43b675: // Cluster
      parseFirstCluster(mf,len);
      break;
    case 0x1654ae6b: // Tracks
      parseTracks(mf,len);
      break;
    case 0x1c53bb6b: // Cues
      parseCues(mf,len);
      break;
    case 0x1941a469: // Attachments
      parseAttachments(mf,len);
      break;
    case 0x1043a770: // Chapters
      parseChapters(mf,len);
      break;
    case 0x1254c367: // Tags
      parseTags(mf,len);
      break;
  }
}

static void parseContainerPos(MatroskaFile *mf,ulonglong pos) {
  seek(mf,pos);
  parseContainer(mf);
}

static void parsePointers(MatroskaFile *mf) {
  jmp_buf                jb;

  if (mf->pSegmentInfo && !mf->seen.SegmentInfo)
    parseContainerPos(mf,mf->pSegmentInfo);
  if (mf->pCluster && !mf->seen.Cluster)
    parseContainerPos(mf,mf->pCluster);
  if (mf->pTracks && !mf->seen.Tracks)
    parseContainerPos(mf,mf->pTracks);

  memcpy(&jb,&mf->jb,sizeof(jb));

  if (setjmp(mf->jb))
    mf->flags &= ~MPF_ERROR; // ignore errors
  else {
    if (mf->pCues && !mf->seen.Cues)
        parseContainerPos(mf,mf->pCues);
    if (mf->pAttachments && !mf->seen.Attachments)
      parseContainerPos(mf,mf->pAttachments);
    if (mf->pChapters && !mf->seen.Chapters)
      parseContainerPos(mf,mf->pChapters);
    if (mf->pTags && !mf->seen.Tags)
      parseContainerPos(mf,mf->pTags);
  }

  memcpy(&mf->jb,&jb,sizeof(jb));
}

static void parseSegment(MatroskaFile *mf,ulonglong toplen) {
  ulonglong   nextpos;
  unsigned    nSeekHeads = 0, dontstop = 0;
  jmp_buf     jb;

  memcpy(&jb,&mf->jb,sizeof(jb));

  if (setjmp(mf->jb))
    mf->flags &= ~MPF_ERROR;
  else {
    // we want to read data until we find a seekhead or a trackinfo
    FOREACH2(mf,toplen,0x1f43b675)
      case 0x114d9b74: // SeekHead
        if (mf->flags & MKVF_AVOID_SEEKS) {
          skipbytes(mf,len);
          break;
        }

        nextpos = filepos(mf) + len;
        do {
          mf->pSeekHead = 0;
          parseSeekHead(mf,len);
          ++nSeekHeads;
          if (mf->pSeekHead) { // this is possibly a chained SeekHead
            seek(mf,mf->pSeekHead);
            id = readID(mf);
            if (id==EOF) // chained SeekHead points to EOF?
              break;
            if (id != 0x114d9b74) // chained SeekHead doesnt point to a SeekHead?
              break;
            len = readSize(mf);
          }
        } while (mf->pSeekHead && nSeekHeads < 10);
        seek(mf,nextpos); // resume reading segment
        break;
      case 0x1549a966: // SegmentInfo
        mf->pSegmentInfo = cur;
        parseSegmentInfo(mf,len);
        break;
      case 0x1f43b675: // Cluster
        if (!mf->pCluster)
          mf->pCluster = cur;
        if (mf->seen.Cluster) {
          if (len != MAXU64)
            skipbytes(mf,len);
        } else
          parseFirstCluster(mf,len);
        break;
      case 0x1654ae6b: // Tracks
        mf->pTracks = cur;
        parseTracks(mf,len);
        break;
      case 0x1c53bb6b: // Cues
        mf->pCues = cur;
        parseCues(mf,len);
        break;
      case 0x1941a469: // Attachments
        mf->pAttachments = cur;
        parseAttachments(mf,len);
        break;
      case 0x1043a770: // Chapters
        mf->pChapters = cur;
        parseChapters(mf,len);
        break;
      case 0x1254c367: // Tags
        mf->pTags = cur;
        parseTags(mf,len);
        break;
    ENDFOR1(mf);
      // if we have pointers to all key elements
      if (!dontstop && mf->pSegmentInfo && mf->pTracks && mf->pCluster)
        break;
    ENDFOR2();
  }

  memcpy(&mf->jb,&jb,sizeof(jb));

  parsePointers(mf);
}

static void parseBlockAdditions(MatroskaFile *mf, ulonglong toplen, ulonglong timecode, unsigned track) {
  ulonglong        add_id = 1, add_pos, add_len;
  unsigned char        have_add;

  FOREACH(mf, toplen)
    case 0xa6: // BlockMore
      have_add = 0;
      FOREACH(mf, len)
        case 0xee: // BlockAddId
          add_id = readUInt(mf, (unsigned)len);
          break;
        case 0xa5: // BlockAddition
          add_pos = filepos(mf);
          add_len = len;
          skipbytes(mf, len);
          ++have_add;
          break;
      ENDFOR(mf);
      if (have_add == 1 && id > 0 && id < 255) {
        struct QueueEntry *qe = QAlloc(mf);
        qe->Start = qe->End = timecode;
        qe->Position = add_pos;
        qe->Length = (unsigned)add_len;
        qe->flags = FRAME_UNKNOWN_START | FRAME_UNKNOWN_END |
          (((unsigned)add_id << FRAME_STREAM_SHIFT) & FRAME_STREAM_MASK);

        QPut(&mf->Queues[track],qe);
      }
      break;
  ENDFOR(mf);
}

static void parseBlockGroup(MatroskaFile *mf,ulonglong toplen,ulonglong timecode, int blockex) {
  ulonglong        v;
  ulonglong        duration = 0;
  ulonglong        dpos;
  struct QueueEntry *qe,*qf = NULL;
  unsigned char        have_duration = 0, have_block = 0;
  unsigned char        gap = 0;
  unsigned char        lacing = 0;
  unsigned char        ref = 0;
  unsigned char        trackid;
  unsigned        tracknum = 0;
  int                c;
  unsigned        nframes = 0,i;
  unsigned        *sizes;
  signed short        block_timecode;

  if (blockex)
    goto blockex;

  FOREACH(mf,toplen)
    case 0xfb: // ReferenceBlock
      readSInt(mf,(unsigned)len);
      ref = 1;
      break;
blockex:
      cur = start = filepos(mf);
      len = tmplen = toplen;
    case 0xa1: // Block
      have_block = 1;

      dpos = filepos(mf);

      v = readVLUInt(mf);
      if (v>255)
        errorjmp(mf,"Invalid track number in Block: %d",(int)v);
      trackid = (unsigned char)v;

      for (tracknum=0;tracknum<mf->nTracks;++tracknum)
        if (mf->Tracks[tracknum]->Number == trackid) {
          if (mf->trackMask & (1<<tracknum)) // ignore this block
            break;
          goto found;
        }

      // bad trackid/unsupported track
      skipbytes(mf,start + tmplen - filepos(mf)); // shortcut
      return;
found:

      block_timecode = (signed short)readSInt(mf,2);

      // recalculate this block's timecode to final timecode in ns
      timecode = mul3(mf->Tracks[tracknum]->TimecodeScale,
        (timecode - mf->firstTimecode + block_timecode) * mf->Seg.TimecodeScale);

      c = readch(mf);
      if (c==EOF)
        errorjmp(mf,"Unexpected EOF while reading Block flags");

      if (blockex)
        ref = (unsigned char)!(c & 0x80);

      gap = (unsigned char)(c & 0x1);
      lacing = (unsigned char)((c >> 1) & 3);

      if (lacing) {
        c = readch(mf);
        if (c == EOF)
          errorjmp(mf,"Unexpected EOF while reading lacing data");
        nframes = c+1;
      } else
        nframes = 1;
      sizes = alloca(nframes*sizeof(*sizes));

      switch (lacing) {
        case 0: // No lacing
          sizes[0] = (unsigned)(len - filepos(mf) + dpos);
          break;
        case 1: // Xiph lacing
          sizes[nframes-1] = 0;
          for (i=0;i<nframes-1;++i) {
            sizes[i] = 0;
            do {
              c = readch(mf);
              if (c==EOF)
                errorjmp(mf,"Unexpected EOF while reading lacing data");
              sizes[i] += c;
            } while (c==255);
            sizes[nframes-1] += sizes[i];
          }
          sizes[nframes-1] = (unsigned)(len - filepos(mf) + dpos) - sizes[nframes-1];
          break;
        case 3: // EBML lacing
          sizes[nframes-1] = 0;
          sizes[0] = (unsigned)readVLUInt(mf);
          for (i=1;i<nframes-1;++i) {
            sizes[i] = sizes[i-1] + (int)readVLSInt(mf);
            sizes[nframes-1] += sizes[i];
          }
          if (nframes>1)
            sizes[nframes-1] = (unsigned)(len - filepos(mf) + dpos) - sizes[0] - sizes[nframes-1];
          break;
        case 2: // Fixed lacing
          sizes[0] = (unsigned)(len - filepos(mf) + dpos)/nframes;
          for (i=1;i<nframes;++i)
            sizes[i] = sizes[0];
          break;
      }

      v = filepos(mf);
      qf = NULL;
      for (i=0;i<nframes;++i) {
        qe = QAlloc(mf);
        if (!qf)
          qf = qe;

        qe->Start = timecode;
        qe->End = timecode;
        qe->Position = v;
        qe->Length = sizes[i];
        qe->flags = FRAME_UNKNOWN_END | FRAME_KF;
        if (i == nframes-1 && gap)
          qe->flags |= FRAME_GAP;
        if (i > 0)
          qe->flags |= FRAME_UNKNOWN_START;

        QPut(&mf->Queues[tracknum],qe);

        v += sizes[i];
      }

      // we want to still load these bytes into cache
      for (v = filepos(mf) & ~0x3fff; v < len + dpos; v += 0x4000)
        mf->cache->read(mf->cache,v,NULL,0); // touch page

      skipbytes(mf,len - filepos(mf) + dpos);

      if (blockex)
        goto out;
      break;
    case 0x9b: // BlockDuration
      duration = readUInt(mf,(unsigned)len);
      have_duration = 1;
      break;
    case 0x75a1: // BlockAdditions
      if (nframes > 0) // have some frames
        parseBlockAdditions(mf, len, timecode, tracknum);
      else
        skipbytes(mf, len);
      break;
  ENDFOR(mf);

out:
  if (!have_block)
    errorjmp(mf,"Found a BlockGroup without Block");

  if (nframes > 1) {
    ulonglong defd = mf->Tracks[tracknum]->DefaultDuration;
    v = qf->Start;

    if (have_duration) {
      duration = mul3(mf->Tracks[tracknum]->TimecodeScale,
        duration * mf->Seg.TimecodeScale);

      for (qe = qf; nframes > 1; --nframes, qe = qe->next) {
        qe->Start = v;
        v += defd;
        duration -= defd;
        qe->End = v;
#if 0
        qe->flags &= ~(FRAME_UNKNOWN_START|FRAME_UNKNOWN_END);
#endif
      }
      qe->Start = v;
      qe->End = v + duration;
      qe->flags &= ~FRAME_UNKNOWN_END;
    } else if (mf->Tracks[tracknum]->DefaultDuration) {
      for (qe = qf; nframes > 0; --nframes, qe = qe->next) {
        qe->Start = v;
        v += defd;
        qe->End = v;
        qe->flags &= ~(FRAME_UNKNOWN_START|FRAME_UNKNOWN_END);
      }
    }
  } else if (nframes == 1) {
    if (have_duration) {
      qf->End = qf->Start + mul3(mf->Tracks[tracknum]->TimecodeScale,
        duration * mf->Seg.TimecodeScale);
      qf->flags &= ~FRAME_UNKNOWN_END;
    } else if (mf->Tracks[tracknum]->DefaultDuration) {
      qf->End = qf->Start + mf->Tracks[tracknum]->DefaultDuration;
      qf->flags &= ~FRAME_UNKNOWN_END;
    }
  }

  if (ref)
    while (qf) {
      qf->flags &= ~FRAME_KF;
      qf = qf->next;
    }
}

static void ClearQueue(MatroskaFile *mf,struct Queue *q) {
  struct QueueEntry *qe,*qn;

  for (qe=q->head;qe;qe=qn) {
    qn = qe->next;
    qe->next = mf->QFreeList;
    mf->QFreeList = qe;
  }

  q->head = NULL;
  q->tail = NULL;
}

static void EmptyQueues(MatroskaFile *mf) {
  unsigned            i;

  for (i=0;i<mf->nTracks;++i)
    ClearQueue(mf,&mf->Queues[i]);
}

static int  readMoreBlocks(MatroskaFile *mf) {
  ulonglong                toplen, cstop;
  longlong                cp;
  int                        cid, ret = 0;
  jmp_buf                jb;
  volatile unsigned        retries = 0;

  if (mf->readPosition >= mf->pSegmentTop)
    return EOF;

  memcpy(&jb,&mf->jb,sizeof(jb));

  if (setjmp(mf->jb)) { // something evil happened here, try to resync
    // always advance read position no matter what so
    // we don't get caught in an endless loop
    mf->readPosition = filepos(mf);

    ret = EOF;

    if (++retries > 3) // don't try too hard
      goto ex;

    for (;;) {
      if (filepos(mf) >= mf->pSegmentTop)
        goto ex;

      cp = mf->cache->scan(mf->cache,filepos(mf),0x1f43b675); // cluster

      if (cp < 0 || (ulonglong)cp >= mf->pSegmentTop)
        goto ex;

      seek(mf,cp);

      cid = readID(mf);
      if (cid == EOF)
        goto ex;
      if (cid == 0x1f43b675) {
        toplen = readSizeUnspec(mf);
        if (toplen < MAXCLUSTER || toplen == MAXU64) {
          // reset error flags
          mf->flags &= ~MPF_ERROR;
          ret = RBRESYNC;
          break;
        }
      }
    }

    mf->readPosition = cp;
  }

  cstop = mf->cache->getcachesize(mf->cache)>>1;
  if (cstop > MAX_READAHEAD)
    cstop = MAX_READAHEAD;
  cstop += mf->readPosition;

  seek(mf,mf->readPosition);

  while (filepos(mf) < mf->pSegmentTop) {
    cid = readID(mf);
    if (cid == EOF) {
      ret = EOF;
      break;
    }
    toplen = cid == 0x1f43b675 ? readSizeUnspec(mf) : readSize(mf);

    if (cid == 0x1f43b675) { // Cluster
      unsigned char        have_timecode = 0;

      FOREACH2(mf,toplen,0x1f43b675)
        case 0x1f43b675:
          RESTART();
          break;
        case 0xe7: // Timecode
          mf->tcCluster = readUInt(mf,(unsigned)len);
          have_timecode = 1;
          break;
        case 0xa7: // Position
          readUInt(mf,(unsigned)len);
          break;
        case 0xab: // PrevSize
          readUInt(mf,(unsigned)len);
          break;
        case 0x5854: { // SilentTracks
          unsigned  stmask = 0, i, trk;
          FOREACH(mf, len)
            case 0x58d7: // SilentTrackNumber
              trk = (unsigned)readUInt(mf, (unsigned)len);
              for (i = 0; i < mf->nTracks; ++i)
                if (mf->Tracks[i]->Number == trk) {
                  stmask |= 1 << i;
                  break;
                }
              break;
          ENDFOR(mf);
          // TODO pass stmask to reading app
          break; }
        case 0xa0: // BlockGroup
          if (!have_timecode)
            errorjmp(mf,"Found BlockGroup before cluster TimeCode");
          parseBlockGroup(mf,len,mf->tcCluster, 0);
          goto out;
        case 0xa3: // BlockEx
          if (!have_timecode)
            errorjmp(mf,"Found BlockGroup before cluster TimeCode");
          parseBlockGroup(mf, len, mf->tcCluster, 1);
          goto out;
      ENDFOR(mf);
out:;
    } else {
      if (toplen > MAXFRAME)
        errorjmp(mf,"Element in a cluster is too large around %llu, %X [%u]",filepos(mf),cid,(unsigned)toplen);
      if (cid == 0xa0) // BlockGroup
        parseBlockGroup(mf,toplen,mf->tcCluster, 0);
      else if (cid == 0xa3) // BlockEx
        parseBlockGroup(mf, toplen, mf->tcCluster, 1);
      else
        skipbytes(mf,toplen);
    }

    if ((mf->readPosition = filepos(mf)) > cstop)
      break;
  }

  mf->readPosition = filepos(mf);

ex:
  memcpy(&mf->jb,&jb,sizeof(jb));

  return ret;
}

// this is almost the same as readMoreBlocks, except it ensures
// there are no partial frames queued, however empty queues are ok
static int  fillQueues(MatroskaFile *mf,unsigned int mask) {
  unsigned    i,j;
  int              ret = 0;

  for (;;) {
    j = 0;

    for (i=0;i<mf->nTracks;++i)
      if (mf->Queues[i].head && !(mask & (1<<i)))
        ++j;

    if (j>0) // have at least some frames
      return ret;

    if ((ret = readMoreBlocks(mf)) < 0) {
      j = 0;
      for (i=0;i<mf->nTracks;++i)
        if (mf->Queues[i].head && !(mask & (1<<i)))
          ++j;
      if (j) // we adjusted some blocks
        return 0;
      return EOF;
    }
  }
}

static void reindex(MatroskaFile *mf) {
  jmp_buf     jb;
  ulonglong   pos = mf->pCluster;
  ulonglong   step = 10*1024*1024;
  ulonglong   size, tc, isize;
  longlong    next_cluster;
  int              id, have_tc, bad;
  struct Cue  *cue;

  if (pos >= mf->pSegmentTop)
    return;

  if (pos + step * 10 > mf->pSegmentTop)
    step = (mf->pSegmentTop - pos) / 10;
  if (step == 0)
    step = 1;

  memcpy(&jb,&mf->jb,sizeof(jb));

  // remove all cues
  mf->nCues = 0;

  bad = 0;

  while (pos < mf->pSegmentTop) {
    if (!mf->cache->progress(mf->cache,pos,mf->pSegmentTop))
      break;

    if (++bad > 50) {
      pos += step;
      bad = 0;
      continue;
    }

    // find next cluster header
    next_cluster = mf->cache->scan(mf->cache,pos,0x1f43b675); // cluster
    if (next_cluster < 0 || (ulonglong)next_cluster >= mf->pSegmentTop)
      break;

    pos = next_cluster + 4; // prevent endless loops

    if (setjmp(mf->jb)) // something evil happened while reindexing
      continue;

    seek(mf,next_cluster);

    id = readID(mf);
    if (id == EOF)
      break;
    if (id != 0x1f43b675) // shouldn't happen
      continue;

    size = readSizeUnspec(mf);
    if (size == MAXU64)
      break;

    if (size >= MAXCLUSTER || size < 1024)
      continue;

    have_tc = 0;
    size += filepos(mf);

    while (filepos(mf) < (ulonglong)next_cluster + 1024) {
      id = readID(mf);
      if (id == EOF)
        break;

      isize = readVLUInt(mf);

      if (id == 0xe7) { // cluster timecode
        tc = readUInt(mf,(unsigned)isize);
        have_tc = 1;
        break;
      }

      skipbytes(mf,isize);
    }

    if (!have_tc)
      continue;

    seek(mf,size);
    id = readID(mf);

    if (id == EOF)
      break;

    if (id != 0x1f43b675) // cluster
      continue;

    // good cluster, remember it
    cue = AGET(mf,Cues);
    cue->Time = tc;
    cue->Position = next_cluster - mf->pSegment;
    cue->Block = 0;
    cue->Track = 0;

    // advance to the next point
    pos = next_cluster + step;
    if (pos < size)
      pos = size;

    bad = 0;
  }

  fixupCues(mf);

  if (mf->nCues == 0) {
    cue = AGET(mf,Cues);
    cue->Time = mf->firstTimecode;
    cue->Position = mf->pCluster - mf->pSegment;
    cue->Block = 0;
    cue->Track = 0;
  }

  mf->cache->progress(mf->cache,0,0);

  memcpy(&mf->jb,&jb,sizeof(jb));
}

static void fixupChapter(ulonglong adj, struct Chapter *ch) {
  unsigned i;

  if (ch->Start != 0)
    ch->Start -= adj;
  if (ch->End != 0)
    ch->End -= adj;

  for (i=0;i<ch->nChildren;++i)
    fixupChapter(adj,&ch->Children[i]);
}

static longlong        findLastTimecode(MatroskaFile *mf) {
  ulonglong   nd = 0;
  unsigned    n,vtrack;

  if (mf->nTracks == 0)
    return -1;

  for (n=vtrack=0;n<mf->nTracks;++n)
    if (mf->Tracks[n]->Type == TT_VIDEO) {
      vtrack = n;
      goto ok;
    }

  return -1;
ok:

  EmptyQueues(mf);

  if (mf->nCues == 0) {
    mf->readPosition = mf->pCluster + 13000000 > mf->pSegmentTop ? mf->pCluster : mf->pSegmentTop - 13000000;
    mf->tcCluster = 0;
  } else {
    mf->readPosition = mf->Cues[mf->nCues - 1].Position + mf->pSegment;
    mf->tcCluster = mf->Cues[mf->nCues - 1].Time / mf->Seg.TimecodeScale;
  }
  mf->trackMask = ~(1 << vtrack);

  do
    while (mf->Queues[vtrack].head)
    {
      ulonglong   tc = mf->Queues[vtrack].head->flags & FRAME_UNKNOWN_END ?
                          mf->Queues[vtrack].head->Start : mf->Queues[vtrack].head->End;
      if (nd < tc)
        nd = tc;
      QFree(mf,QGet(&mf->Queues[vtrack]));
    }
  while (fillQueues(mf,0) != EOF);

  mf->trackMask = 0;

  EmptyQueues(mf);

  // there may have been an error, but at this point we will ignore it
  if (mf->flags & MPF_ERROR) {
    mf->flags &= ~MPF_ERROR;
    if (nd == 0)
      return -1;
  }

  return nd;
}

static void parseFile(MatroskaFile *mf) {
  ulonglong len = filepos(mf), adjust;
  unsigned  i;
  int            id = readID(mf);

  if (id==EOF)
    errorjmp(mf,"Unexpected EOF at start of file");

  // files with multiple concatenated segments can have only
  // one EBML prolog
  if (len > 0 && id == 0x18538067)
    goto segment;

  if (id!=0x1a45dfa3)
    errorjmp(mf,"First element in file is not EBML");

  parseEBML(mf,readSize(mf));

  // next we need to find the first segment
  for (;;) {
    id = readID(mf);
    if (id==EOF)
      errorjmp(mf,"No segments found in the file");
segment:
    len = readSizeUnspec(mf);
    if (id == 0x18538067) // Segment
      break;
    if (len == MAXU64)
      errorjmp(mf,"No segments found in the file");
    skipbytes(mf,len);
  }

  // found it
  mf->pSegment = filepos(mf);
  if (len == MAXU64) {
    mf->pSegmentTop = MAXU64;
    if (mf->cache->getfilesize) {
      longlong seglen = mf->cache->getfilesize(mf->cache);
      if (seglen > 0)
        mf->pSegmentTop = seglen;
    }
  } else
    mf->pSegmentTop = mf->pSegment + len;
  parseSegment(mf,len);

  // check if we got all data
  if (!mf->seen.SegmentInfo)
    errorjmp(mf,"Couldn't find SegmentInfo");
  if (!mf->seen.Cluster)
    mf->pCluster = mf->pSegmentTop;

  adjust = mf->firstTimecode * mf->Seg.TimecodeScale;

  for (i=0;i<mf->nChapters;++i)
    fixupChapter(adjust, &mf->Chapters[i]);

  fixupCues(mf);

  // release extra memory
  ARELEASE(mf,mf,Tracks);

  // initialize reader
  mf->Queues = mf->cache->memalloc(mf->cache,mf->nTracks * sizeof(*mf->Queues));
  if (mf->Queues == NULL)
    errorjmp(mf, "Ouf of memory");
  memset(mf->Queues, 0, mf->nTracks * sizeof(*mf->Queues));

  // try to detect real duration
  if (!(mf->flags & MKVF_AVOID_SEEKS)) {
    longlong nd = findLastTimecode(mf);
    if (nd > 0)
      mf->Seg.Duration = nd;
  }

  // move to first frame
  mf->readPosition = mf->pCluster;
  mf->tcCluster = mf->firstTimecode;
}

static void DeleteChapter(MatroskaFile *mf,struct Chapter *ch) {
  unsigned i,j;

  for (i=0;i<ch->nDisplay;++i)
    mf->cache->memfree(mf->cache,ch->Display[i].String);
  mf->cache->memfree(mf->cache,ch->Display);
  mf->cache->memfree(mf->cache,ch->Tracks);

  for (i=0;i<ch->nProcess;++i) {
    for (j=0;j<ch->Process[i].nCommands;++j)
      mf->cache->memfree(mf->cache,ch->Process[i].Commands[j].Command);
    mf->cache->memfree(mf->cache,ch->Process[i].Commands);
    mf->cache->memfree(mf->cache,ch->Process[i].CodecPrivate);
  }
  mf->cache->memfree(mf->cache,ch->Process);

  for (i=0;i<ch->nChildren;++i)
    DeleteChapter(mf,&ch->Children[i]);
  mf->cache->memfree(mf->cache,ch->Children);
}

///////////////////////////////////////////////////////////////////////////
// public interface
MatroskaFile  *mkv_OpenEx(InputStream *io,
                          ulonglong base,
                          unsigned flags,
                          char *err_msg,unsigned msgsize)
{
  MatroskaFile        *mf = io->memalloc(io,sizeof(*mf));
  if (mf == NULL) {
    mystrlcpy(err_msg,"Out of memory",msgsize);
    return NULL;
  }

  memset(mf,0,sizeof(*mf));

  mf->cache = io;
  mf->flags = flags;
  io->progress(io,0,0);

  if (setjmp(mf->jb)==0) {
    seek(mf,base);
    parseFile(mf);
  } else { // parser error
    mystrlcpy(err_msg,mf->errmsg,msgsize);
    mkv_Close(mf);
    return NULL;
  }

  return mf;
}

MatroskaFile  *mkv_Open(InputStream *io,
                        char *err_msg,unsigned msgsize)
{
  return mkv_OpenEx(io,0,0,err_msg,msgsize);
}

void              mkv_Close(MatroskaFile *mf) {
  unsigned  i,j;

  if (mf==NULL)
    return;

  for (i=0;i<mf->nTracks;++i)
    mf->cache->memfree(mf->cache,mf->Tracks[i]);
  mf->cache->memfree(mf->cache,mf->Tracks);

  for (i=0;i<mf->nQBlocks;++i)
    mf->cache->memfree(mf->cache,mf->QBlocks[i]);
  mf->cache->memfree(mf->cache,mf->QBlocks);

  mf->cache->memfree(mf->cache,mf->Queues);

  mf->cache->memfree(mf->cache,mf->Seg.Title);
  mf->cache->memfree(mf->cache,mf->Seg.MuxingApp);
  mf->cache->memfree(mf->cache,mf->Seg.WritingApp);
  mf->cache->memfree(mf->cache,mf->Seg.Filename);
  mf->cache->memfree(mf->cache,mf->Seg.NextFilename);
  mf->cache->memfree(mf->cache,mf->Seg.PrevFilename);

  mf->cache->memfree(mf->cache,mf->Cues);

  for (i=0;i<mf->nAttachments;++i) {
    mf->cache->memfree(mf->cache,mf->Attachments[i].Description);
    mf->cache->memfree(mf->cache,mf->Attachments[i].Name);
    mf->cache->memfree(mf->cache,mf->Attachments[i].MimeType);
  }
  mf->cache->memfree(mf->cache,mf->Attachments);

  for (i=0;i<mf->nChapters;++i)
    DeleteChapter(mf,&mf->Chapters[i]);
  mf->cache->memfree(mf->cache,mf->Chapters);

  for (i=0;i<mf->nTags;++i) {
    for (j=0;j<mf->Tags[i].nSimpleTags;++j) {
      mf->cache->memfree(mf->cache,mf->Tags[i].SimpleTags[j].Name);
      mf->cache->memfree(mf->cache,mf->Tags[i].SimpleTags[j].Value);
    }
    mf->cache->memfree(mf->cache,mf->Tags[i].Targets);
    mf->cache->memfree(mf->cache,mf->Tags[i].SimpleTags);
  }
  mf->cache->memfree(mf->cache,mf->Tags);

  mf->cache->memfree(mf->cache,mf);
}

const char    *mkv_GetLastError(MatroskaFile *mf) {
  return mf->errmsg[0] ? mf->errmsg : NULL;
}

SegmentInfo   *mkv_GetFileInfo(MatroskaFile *mf) {
  return &mf->Seg;
}

unsigned int  mkv_GetNumTracks(MatroskaFile *mf) {
  return mf->nTracks;
}

TrackInfo     *mkv_GetTrackInfo(MatroskaFile *mf,unsigned track) {
  if (track>mf->nTracks)
    return NULL;

  return mf->Tracks[track];
}

void              mkv_GetAttachments(MatroskaFile *mf,Attachment **at,unsigned *count) {
  *at = mf->Attachments;
  *count = mf->nAttachments;
}

void              mkv_GetChapters(MatroskaFile *mf,Chapter **ch,unsigned *count) {
  *ch = mf->Chapters;
  *count = mf->nChapters;
}

void              mkv_GetTags(MatroskaFile *mf,Tag **tag,unsigned *count) {
  *tag = mf->Tags;
  *count = mf->nTags;
}

ulonglong     mkv_GetSegmentTop(MatroskaFile *mf) {
  return mf->pSegmentTop;
}

#define        IS_DELTA(f) (!((f)->flags & FRAME_KF) || ((f)->flags & FRAME_UNKNOWN_START))

void  mkv_Seek(MatroskaFile *mf,ulonglong timecode,unsigned flags) {
  int                i,j,m,ret;
  unsigned        n,z,mask;
  ulonglong        m_kftime[MAX_TRACKS];
  unsigned char        m_seendf[MAX_TRACKS];

  if (mf->flags & MKVF_AVOID_SEEKS)
    return;

  if (timecode == 0) {
    EmptyQueues(mf);
    mf->readPosition = mf->pCluster;
    mf->tcCluster = mf->firstTimecode;
    mf->flags &= ~MPF_ERROR;

    return;
  }

  if (mf->nCues==0)
    reindex(mf);

  if (mf->nCues==0)
    return;

  mf->flags &= ~MPF_ERROR;

  i = 0;
  j = mf->nCues - 1;

  for (;;) {
    if (i>j) {
      j = j>=0 ? j : 0;

      if (setjmp(mf->jb)!=0)
        return;

      mkv_SetTrackMask(mf,mf->trackMask);

      if (flags & (MKVF_SEEK_TO_PREV_KEYFRAME | MKVF_SEEK_TO_PREV_KEYFRAME_STRICT)) {
        // we do this in two stages
        // a. find the last keyframes before the require position
        // b. seek to them

        // pass 1
        for (;;) {
          for (n=0;n<mf->nTracks;++n) {
            m_kftime[n] = MAXU64;
            m_seendf[n] = 0;
          }

          EmptyQueues(mf);

          mf->readPosition = mf->Cues[j].Position + mf->pSegment;
          mf->tcCluster = mf->Cues[j].Time;

          for (;;) {
            if ((ret = fillQueues(mf,0)) < 0 || ret == RBRESYNC)
              return;

            // drain queues until we get to the required timecode
            for (n=0;n<mf->nTracks;++n) {
              if (mf->Queues[n].head && (mf->Queues[n].head->Start<timecode || (m_seendf[n] == 0 && m_kftime[n] == MAXU64))) {
                if (IS_DELTA(mf->Queues[n].head))
                  m_seendf[n] = 1;
                else
                  m_kftime[n] = mf->Queues[n].head->Start;
              }

              while (mf->Queues[n].head && mf->Queues[n].head->Start<timecode)
              {
                if (IS_DELTA(mf->Queues[n].head))
                  m_seendf[n] = 1;
                else
                  m_kftime[n] = mf->Queues[n].head->Start;
                QFree(mf,QGet(&mf->Queues[n]));
              }

              // We've drained the queue, so the frame at head is the next one past the requered point.
              // In strict mode we are done, but when seeking is not strict we use the head frame
              // if it's not an audio track (we accept preroll within a frame for audio), and the head frame
              // is a keyframe
              if (!(flags & MKVF_SEEK_TO_PREV_KEYFRAME_STRICT))
                if (mf->Queues[n].head && (mf->Tracks[n]->Type != TT_AUDIO || mf->Queues[n].head->Start<=timecode))
                  if (!IS_DELTA(mf->Queues[n].head))
                    m_kftime[n] = mf->Queues[n].head->Start;
            }

            for (n=0;n<mf->nTracks;++n)
              if (mf->Queues[n].head && mf->Queues[n].head->Start>=timecode)
                goto found;
          }
found:

          for (n=0;n<mf->nTracks;++n)
            if (!(mf->trackMask & (1<<n)) && m_kftime[n]==MAXU64 &&
                m_seendf[n] && j>0)
            {
              // we need to restart the search from prev cue
              --j;
              goto again;
            }

          break;
again:;
        }
      } else
        for (n=0;n<mf->nTracks;++n)
          m_kftime[n] = timecode;

      // now seek to this timecode
      EmptyQueues(mf);

      mf->readPosition = mf->Cues[j].Position + mf->pSegment;
      mf->tcCluster = mf->Cues[j].Time;

      for (mask=0;;) {
        if ((ret = fillQueues(mf,mask)) < 0 || ret == RBRESYNC)
          return;

        // drain queues until we get to the required timecode
        for (n=0;n<mf->nTracks;++n) {
          struct QueueEntry *qe;
          for (qe = mf->Queues[n].head;qe && qe->Start<m_kftime[n];qe = mf->Queues[n].head)
            QFree(mf,QGet(&mf->Queues[n]));
        }

        for (n=z=0;n<mf->nTracks;++n)
          if (m_kftime[n]==MAXU64 || (mf->Queues[n].head && mf->Queues[n].head->Start>=m_kftime[n])) {
            ++z;
            mask |= 1<<n;
          }

        if (z==mf->nTracks)
          return;
      }
    }

    m = (i+j)>>1;

    if (timecode < mf->Cues[m].Time)
      j = m-1;
    else
      i = m+1;
  }
}

void  mkv_SkipToKeyframe(MatroskaFile *mf) {
  unsigned  n,wait;
  ulonglong ht;

  if (setjmp(mf->jb)!=0)
    return;

  // remove delta frames from queues
  do {
    wait = 0;

    if (fillQueues(mf,0)<0)
      return;

    for (n=0;n<mf->nTracks;++n)
      if (mf->Queues[n].head && !(mf->Queues[n].head->flags & FRAME_KF)) {
        ++wait;
        QFree(mf,QGet(&mf->Queues[n]));
      }
  } while (wait);

  // find highest queued time
  for (n=0,ht=0;n<mf->nTracks;++n)
    if (mf->Queues[n].head && ht<mf->Queues[n].head->Start)
      ht = mf->Queues[n].head->Start;

  // ensure the time difference is less than 100ms
  do {
    wait = 0;

    if (fillQueues(mf,0)<0)
      return;

    for (n=0;n<mf->nTracks;++n)
      while (mf->Queues[n].head && mf->Queues[n].head->next &&
          (mf->Queues[n].head->next->flags & FRAME_KF) &&
          ht - mf->Queues[n].head->Start > 100000000)
      {
        ++wait;
        QFree(mf,QGet(&mf->Queues[n]));
      }

  } while (wait);
}

ulonglong mkv_GetLowestQTimecode(MatroskaFile *mf) {
  unsigned  n,seen;
  ulonglong t;

  // find the lowest queued timecode
  for (n=seen=0,t=0;n<mf->nTracks;++n)
    if (mf->Queues[n].head && (!seen || t > mf->Queues[n].head->Start))
      t = mf->Queues[n].head->Start, seen=1;

  return seen ? t : (ulonglong)LL(-1);
}

int              mkv_TruncFloat(MKFLOAT f) {
#ifdef MATROSKA_INTEGER_ONLY
  return (int)(f.v >> 32);
#else
  return (int)f;
#endif
}

#define        FTRACK        0xffffffff

void              mkv_SetTrackMask(MatroskaFile *mf,unsigned int mask) {
  unsigned int          i;

  if (mf->flags & MPF_ERROR)
    return;

  mf->trackMask = mask;

  for (i=0;i<mf->nTracks;++i)
    if (mask & (1<<i))
      ClearQueue(mf,&mf->Queues[i]);
}

int              mkv_ReadFrame(MatroskaFile *mf,
                            unsigned int mask,unsigned int *track,
                            ulonglong *StartTime,ulonglong *EndTime,
                            ulonglong *FilePos,unsigned int *FrameSize,
                            unsigned int *FrameFlags)
{
  unsigned int            i,j;
  struct QueueEntry *qe;

  if (setjmp(mf->jb)!=0)
    return -1;

  do {
    // extract required frame, use block with the lowest timecode
    for (j=FTRACK,i=0;i<mf->nTracks;++i)
      if (!(mask & (1<<i)) && mf->Queues[i].head) {
        j = i;
        ++i;
        break;
      }

    for (;i<mf->nTracks;++i)
      if (!(mask & (1<<i)) && mf->Queues[i].head &&
          mf->Queues[j].head->Start > mf->Queues[i].head->Start)
        j = i;

    if (j != FTRACK) {
      qe = QGet(&mf->Queues[j]);

      *track = j;
      *StartTime = qe->Start;
      *EndTime = qe->End;
      *FilePos = qe->Position;
      *FrameSize = qe->Length;
      *FrameFlags = qe->flags;

      QFree(mf,qe);

      return 0;
    }

    if (mf->flags & MPF_ERROR)
      return -1;

  } while (fillQueues(mf,mask)>=0);

  return EOF;
}

#ifdef MATROSKA_COMPRESSION_SUPPORT
/*************************************************************************
 * Compressed streams support
 ************************************************************************/
struct CompressedStream {
  MatroskaFile          *mf;
  z_stream          zs;

  /* current compressed frame */
  ulonglong          frame_pos;
  unsigned          frame_size;
  char                  frame_buffer[2048];

  /* decoded data buffer */
  char                  decoded_buffer[2048];
  unsigned          decoded_ptr;
  unsigned          decoded_size;

  /* error handling */
  char                  errmsg[128];
};

CompressedStream  *cs_Create(/* in */        MatroskaFile *mf,
                             /* in */        unsigned tracknum,
                             /* out */        char *errormsg,
                             /* in */        unsigned msgsize)
{
  CompressedStream  *cs;
  TrackInfo            *ti;
  int                    code;

  ti = mkv_GetTrackInfo(mf, tracknum);
  if (ti == NULL) {
    mystrlcpy(errormsg, "No such track.", msgsize);
    return NULL;
  }

  if (!ti->CompEnabled) {
    mystrlcpy(errormsg, "Track is not compressed.", msgsize);
    return NULL;
  }

  if (ti->CompMethod != COMP_ZLIB) {
    mystrlcpy(errormsg, "Unsupported compression method.", msgsize);
    return NULL;
  }

  cs = mf->cache->memalloc(mf->cache,sizeof(*cs));
  if (cs == NULL) {
    mystrlcpy(errormsg, "Ouf of memory.", msgsize);
    return NULL;
  }

  memset(&cs->zs,0,sizeof(cs->zs));
  code = inflateInit(&cs->zs);
  if (code != Z_OK) {
    mystrlcpy(errormsg, "ZLib error.", msgsize);
    mf->cache->memfree(mf->cache,cs);
    return NULL;
  }

  cs->frame_size = 0;
  cs->decoded_ptr = cs->decoded_size = 0;
  cs->mf = mf;

  return cs;
}

void                  cs_Destroy(/* in */ CompressedStream *cs) {
  if (cs == NULL)
    return;

  inflateEnd(&cs->zs);
  cs->mf->cache->memfree(cs->mf->cache,cs);
}

/* advance to the next frame in matroska stream, you need to pass values returned
 * by mkv_ReadFrame */
void                  cs_NextFrame(/* in */ CompressedStream *cs,
                               /* in */ ulonglong pos,
                               /* in */ unsigned size)
{
  cs->zs.avail_in = 0;
  inflateReset(&cs->zs);
  cs->frame_pos = pos;
  cs->frame_size = size;
  cs->decoded_ptr = cs->decoded_size = 0;
}

/* read and decode more data from current frame, return number of bytes decoded,
 * 0 on end of frame, or -1 on error */
int                  cs_ReadData(CompressedStream *cs,char *buffer,unsigned bufsize)
{
  char            *cp = buffer;
  unsigned  rd = 0;
  unsigned  todo;
  int            code;

  do {
    /* try to copy data from decoded buffer */
    if (cs->decoded_ptr < cs->decoded_size) {
      todo = cs->decoded_size - cs->decoded_ptr;;
      if (todo > bufsize - rd)
        todo = bufsize - rd;

      memcpy(cp, cs->decoded_buffer + cs->decoded_ptr, todo);

      rd += todo;
      cp += todo;
      cs->decoded_ptr += todo;
    } else {
      /* setup output buffer */
      cs->zs.next_out = (Bytef *)cs->decoded_buffer;
      cs->zs.avail_out = sizeof(cs->decoded_buffer);

      /* try to read more data */
      if (cs->zs.avail_in == 0 && cs->frame_size > 0) {
        todo = cs->frame_size;
        if (todo > sizeof(cs->frame_buffer))
          todo = sizeof(cs->frame_buffer);

        if (cs->mf->cache->read(cs->mf->cache, cs->frame_pos, cs->frame_buffer, todo) != (int)todo) {
          mystrlcpy(cs->errmsg, "File read failed", sizeof(cs->errmsg));
          return -1;
        }

        cs->zs.next_in = (Bytef *)cs->frame_buffer;
        cs->zs.avail_in = todo;

        cs->frame_pos += todo;
        cs->frame_size -= todo;
      }

      /* try to decode more data */
      code = inflate(&cs->zs,Z_NO_FLUSH);
      if (code != Z_OK && code != Z_STREAM_END) {
        mystrlcpy(cs->errmsg, "ZLib error.", sizeof(cs->errmsg));
        return -1;
      }

      /* handle decoded data */
      if (cs->zs.avail_out == sizeof(cs->decoded_buffer)) /* EOF */
        break;

      cs->decoded_ptr = 0;
      cs->decoded_size = sizeof(cs->decoded_buffer) - cs->zs.avail_out;
    }
  } while (rd < bufsize);

  return rd;
}

/* return error message for the last error */
const char          *cs_GetLastError(CompressedStream *cs)
{
  if (!cs->errmsg[0])
    return NULL;
  return cs->errmsg;
}
#endif
