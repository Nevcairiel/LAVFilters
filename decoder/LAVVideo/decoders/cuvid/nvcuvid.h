/*
 * Copyright 1993-2008 NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO USER:
 *
 * This source code is subject to NVIDIA ownership rights under U.S. and
 * international Copyright laws.  Users and possessors of this source code
 * are hereby granted a nonexclusive, royalty-free license to use this code
 * in individual and commercial software.
 *
 * NVIDIA MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOURCE
 * CODE FOR ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR
 * IMPLIED WARRANTY OF ANY KIND.  NVIDIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOURCE CODE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION,  ARISING OUT OF OR IN CONNECTION WITH THE USE
 * OR PERFORMANCE OF THIS SOURCE CODE.
 *
 * U.S. Government End Users.   This source code is a "commercial item" as
 * that term is defined at  48 C.F.R. 2.101 (OCT 1995), consisting  of
 * "commercial computer  software"  and "commercial computer software
 * documentation" as such terms are  used in 48 C.F.R. 12.212 (SEPT 1995)
 * and is provided to the U.S. Government only as a commercial end item.
 * Consistent with 48 C.F.R.12.212 and 48 C.F.R. 227.7202-1 through
 * 227.7202-4 (JUNE 1995), all U.S. Government End Users acquire the
 * source code with only those rights set forth herein.
 *
 * Any use of this source code in individual and commercial software must
 * include, in the user documentation and internal comments to the code,
 * the above Disclaimer and U.S. Government End Users Notice.
 */

#if !defined(__NVCUVID_H__)
#define __NVCUVID_H__

#include "cuviddec.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

////////////////////////////////////////////////////////////////////////////////////////////////
//
// High-level helper APIs for video sources
//

typedef void *CUvideosource;
typedef void *CUvideoparser;
typedef long long CUvideotimestamp;

////////////////////////////////////////////////////////////////////////////////////////////////
//
// video data structures
//

// Video Source State
typedef enum {
    cudaVideoState_Error   = -1,    // Error state (invalid source)
    cudaVideoState_Stopped = 0,     // Source is stopped (or reached end-of-stream)
    cudaVideoState_Started = 1,     // Source is running and delivering data
} cudaVideoState;

// Audio compression
typedef enum {
    cudaAudioCodec_MPEG1=0,         // MPEG-1 Audio
    cudaAudioCodec_MPEG2,           // MPEG-2 Audio
    cudaAudioCodec_MP3,             // MPEG-1 Layer III Audio
    cudaAudioCodec_AC3,             // Dolby Digital (AC3) Audio
    cudaAudioCodec_LPCM,            // PCM Audio
} cudaAudioCodec;


// Video format
typedef struct
{
    cudaVideoCodec codec;           // Compression format
    struct {
        unsigned int numerator;     // frame rate numerator   (0 = unspecified or variable frame rate)
        unsigned int denominator;   // frame rate denominator (0 = unspecified or variable frame rate)
    } frame_rate;                   // frame rate = numerator / denominator (for example: 30000/1001)
    int progressive_sequence;       // 0=interlaced, 1=progressive
    unsigned int coded_width;       // coded frame width
    unsigned int coded_height;      // coded frame height
    struct {                        // area of the frame that should be displayed
        int left;                   // typical example:
        int top;                    //   coded_width = 1920, coded_height = 1088
        int right;                  //   display_area = { 0,0,1920,1080 }
        int bottom;
    } display_area;
    cudaVideoChromaFormat chroma_format;    // Chroma format
    unsigned int bitrate;           // video bitrate (bps, 0=unknown)
    struct {                        // Display Aspect Ratio = x:y (4:3, 16:9, etc)
        int x;
        int y;
    } display_aspect_ratio;
    struct {
        unsigned char video_format;
        unsigned char color_primaries;
        unsigned char transfer_characteristics;
        unsigned char matrix_coefficients;
    } video_signal_description;
    unsigned int seqhdr_data_length;          // Additional bytes following (CUVIDEOFORMATEX)
} CUVIDEOFORMAT;

// Video format including raw sequence header information
typedef struct
{
    CUVIDEOFORMAT format;
    unsigned char raw_seqhdr_data[1024];
} CUVIDEOFORMATEX;


// Audio Format
typedef struct
{
    cudaAudioCodec codec;       // Compression format
    unsigned int channels;      // number of audio channels
    unsigned int samplespersec; // sampling frequency
    unsigned int bitrate;       // For uncompressed, can also be used to determine bits per sample
    unsigned int reserved1;
    unsigned int reserved2;
} CUAUDIOFORMAT;



////////////////////////////////////////////////////////////////////////////////////////////////
//
// video source
//

// Data packet
typedef enum {
    CUVID_PKT_ENDOFSTREAM   = 0x01,   // Set when this is the last packet for this stream
    CUVID_PKT_TIMESTAMP     = 0x02,   // Timestamp is valid
    CUVID_PKT_DISCONTINUITY = 0x04,   // Set when a discontinuity has to be signalled
} CUvideopacketflags;

typedef struct _CUVIDSOURCEDATAPACKET
{
    unsigned long flags;            // Combination of CUVID_PKT_XXX flags
    unsigned long payload_size;     // number of bytes in the payload (may be zero if EOS flag is set)
    const unsigned char *payload;   // Pointer to packet payload data (may be NULL if EOS flag is set)
    CUvideotimestamp timestamp;     // Presentation timestamp (10MHz clock), only valid if CUVID_PKT_TIMESTAMP flag is set
} CUVIDSOURCEDATAPACKET;

// Callback for packet delivery
typedef int (CUDAAPI *PFNVIDSOURCECALLBACK)(void *, CUVIDSOURCEDATAPACKET *);

typedef struct _CUVIDSOURCEPARAMS
{
    unsigned int ulClockRate;   // Timestamp units in Hz (0=default=10000000Hz)
    unsigned int uReserved1[7]; // Reserved for future use - set to zero
    void *pUserData;    // Parameter passed in to the data handlers
    PFNVIDSOURCECALLBACK pfnVideoDataHandler;   // Called to deliver audio packets
    PFNVIDSOURCECALLBACK pfnAudioDataHandler;   // Called to deliver video packets
    void *pvReserved2[8];   // Reserved for future use - set to NULL
} CUVIDSOURCEPARAMS;

typedef enum {
    CUVID_FMT_EXTFORMATINFO = 0x100,    // Return extended format structure (CUVIDEOFORMATEX)
} CUvideosourceformat_flags;

#if !defined(__APPLE__)
// Video file source
CUresult CUDAAPI cuvidCreateVideoSource(CUvideosource *pObj, const char *pszFileName, CUVIDSOURCEPARAMS *pParams);
CUresult CUDAAPI cuvidCreateVideoSourceW(CUvideosource *pObj, const wchar_t *pwszFileName, CUVIDSOURCEPARAMS *pParams);
CUresult CUDAAPI cuvidDestroyVideoSource(CUvideosource obj);
CUresult CUDAAPI cuvidSetVideoSourceState(CUvideosource obj, cudaVideoState state);
cudaVideoState CUDAAPI cuvidGetVideoSourceState(CUvideosource obj);
CUresult CUDAAPI cuvidGetSourceVideoFormat(CUvideosource obj, CUVIDEOFORMAT *pvidfmt, unsigned int flags);
CUresult CUDAAPI cuvidGetSourceAudioFormat(CUvideosource obj, CUAUDIOFORMAT *paudfmt, unsigned int flags);
#endif

////////////////////////////////////////////////////////////////////////////////////////////////
//
// Video parser
//

typedef struct _CUVIDPARSERDISPINFO
{
    int picture_index;
    int progressive_frame;
    int top_field_first;
    int repeat_first_field; // Number of additional fields (1=ivtc, 2=frame doubling, 4=frame tripling, -1=unpaired field)
    CUvideotimestamp timestamp;
} CUVIDPARSERDISPINFO;

//
// Parser callbacks
// The parser will call these synchronously from within cuvidParseVideoData(), whenever a picture is ready to
// be decoded and/or displayed.
//
typedef int (CUDAAPI *PFNVIDSEQUENCECALLBACK)(void *, CUVIDEOFORMAT *);
typedef int (CUDAAPI *PFNVIDDECODECALLBACK)(void *, CUVIDPICPARAMS *);
typedef int (CUDAAPI *PFNVIDDISPLAYCALLBACK)(void *, CUVIDPARSERDISPINFO *);

typedef struct _CUVIDPARSERPARAMS
{
    cudaVideoCodec CodecType;       // cudaVideoCodec_XXX
    unsigned int ulMaxNumDecodeSurfaces;    // Max # of decode surfaces (parser will cycle through these)
    unsigned int ulClockRate;       // Timestamp units in Hz (0=default=10000000Hz)
    unsigned int ulErrorThreshold;  // % Error threshold (0-100) for calling pfnDecodePicture (100=always call pfnDecodePicture even if picture bitstream is fully corrupted)
    unsigned int ulMaxDisplayDelay; // Max display queue delay (improves pipelining of decode with display) - 0=no delay (recommended values: 2..4)
    unsigned int uReserved1[5]; // Reserved for future use - set to 0
    void *pUserData;        // User data for callbacks
    PFNVIDSEQUENCECALLBACK pfnSequenceCallback; // Called before decoding frames and/or whenever there is a format change
    PFNVIDDECODECALLBACK pfnDecodePicture;      // Called when a picture is ready to be decoded (decode order)
    PFNVIDDISPLAYCALLBACK pfnDisplayPicture;    // Called whenever a picture is ready to be displayed (display order)
    void *pvReserved2[7];           // Reserved for future use - set to NULL
    CUVIDEOFORMATEX *pExtVideoInfo; // [Optional] sequence header data from system layer
} CUVIDPARSERPARAMS;


CUresult CUDAAPI cuvidCreateVideoParser(CUvideoparser *pObj, CUVIDPARSERPARAMS *pParams);
CUresult CUDAAPI cuvidParseVideoData(CUvideoparser obj, CUVIDSOURCEDATAPACKET *pPacket);
CUresult CUDAAPI cuvidDestroyVideoParser(CUvideoparser obj);


////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif // __NVCUVID_H__
