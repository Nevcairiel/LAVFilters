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

#if !defined(__CUDA_VIDEO_H__)
#define __CUDA_VIDEO_H__

#ifndef __cuda_cuda_h__
#include <cuda.h>
#endif // __cuda_cuda_h__

#if defined(__x86_64) || defined(AMD64) || defined(_M_AMD64)
#if (CUDA_VERSION >= 3020) && (!defined(CUDA_FORCE_API_VERSION) || (CUDA_FORCE_API_VERSION >= 3020))
#define __CUVID_DEVPTR64
#endif
#endif

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef void *CUvideodecoder;
typedef struct _CUcontextlock_st *CUvideoctxlock;

typedef enum cudaVideoCodec_enum {
    cudaVideoCodec_MPEG1=0,
    cudaVideoCodec_MPEG2,
    cudaVideoCodec_MPEG4,
    cudaVideoCodec_VC1,
    cudaVideoCodec_H264,
    cudaVideoCodec_JPEG,
    cudaVideoCodec_H264_SVC,
    cudaVideoCodec_H264_MVC,
    cudaVideoCodec_NumCodecs,
    // Uncompressed YUV
    cudaVideoCodec_YUV420 = (('I'<<24)|('Y'<<16)|('U'<<8)|('V')),   // Y,U,V (4:2:0)
    cudaVideoCodec_YV12   = (('Y'<<24)|('V'<<16)|('1'<<8)|('2')),   // Y,V,U (4:2:0)
    cudaVideoCodec_NV12   = (('N'<<24)|('V'<<16)|('1'<<8)|('2')),   // Y,UV  (4:2:0)
    cudaVideoCodec_YUYV   = (('Y'<<24)|('U'<<16)|('Y'<<8)|('V')),   // YUYV/YUY2 (4:2:2)
    cudaVideoCodec_UYVY   = (('U'<<24)|('Y'<<16)|('V'<<8)|('Y')),   // UYVY (4:2:2)
} cudaVideoCodec;

typedef enum cudaVideoSurfaceFormat_enum {
    cudaVideoSurfaceFormat_NV12=0,  // NV12 (currently the only supported output format)
} cudaVideoSurfaceFormat;

typedef enum cudaVideoDeinterlaceMode_enum {
    cudaVideoDeinterlaceMode_Weave=0,   // Weave both fields (no deinterlacing)
    cudaVideoDeinterlaceMode_Bob,       // Drop one field
    cudaVideoDeinterlaceMode_Adaptive,  // Adaptive deinterlacing
} cudaVideoDeinterlaceMode;

typedef enum cudaVideoChromaFormat_enum {
    cudaVideoChromaFormat_Monochrome=0,
    cudaVideoChromaFormat_420,
    cudaVideoChromaFormat_422,
    cudaVideoChromaFormat_444,
} cudaVideoChromaFormat;

typedef enum cudaVideoCreateFlags_enum {
    cudaVideoCreate_Default = 0x00,     // Default operation mode: use dedicated video engines
    cudaVideoCreate_PreferCUDA = 0x01,  // Use a CUDA-based decoder if faster than dedicated engines (requires a valid vidLock object for multi-threading)
    cudaVideoCreate_PreferDXVA = 0x02,  // Go through DXVA internally if possible (requires D3D9 interop)
    cudaVideoCreate_PreferCUVID = 0x04, // Use dedicated video engines directly
} cudaVideoCreateFlags;


typedef struct _CUVIDDECODECREATEINFO
{
    // Decoding
    unsigned long ulWidth;          // Coded Sequence Width
    unsigned long ulHeight;         // Coded Sequence Height
    unsigned long ulNumDecodeSurfaces;  // Maximum number of internal decode surfaces
    cudaVideoCodec CodecType;        // cudaVideoCodec_XXX
    cudaVideoChromaFormat ChromaFormat; // cudaVideoChromaFormat_XXX (only 4:2:0 is currently supported)
    unsigned long ulCreationFlags;  // Decoder creation flags (cudaVideoCreateFlags_XXX)
    unsigned long Reserved1[5];     // Reserved for future use - set to zero
    struct {                        // area of the frame that should be displayed
        short left;
        short top;
        short right;
        short bottom;
    } display_area;
    // Output format
    cudaVideoSurfaceFormat OutputFormat;       // cudaVideoSurfaceFormat_XXX
    cudaVideoDeinterlaceMode DeinterlaceMode;  // cudaVideoDeinterlaceMode_XXX
    unsigned long ulTargetWidth;    // Post-processed Output Width
    unsigned long ulTargetHeight;   // Post-processed Output Height
    unsigned long ulNumOutputSurfaces; // Maximum number of output surfaces simultaneously mapped
    CUvideoctxlock vidLock;         // If non-NULL, context lock used for synchronizing ownership of the cuda context
    struct {                        // target rectangle in the output frame (for aspect ratio conversion)
        short left;
        short top;
        short right;
        short bottom;
    } target_rect;                  // if a null rectangle is specified, {0,0,ulTargetWidth,ulTargetHeight} will be used
    unsigned long Reserved2[5];     // Reserved for future use - set to zero
} CUVIDDECODECREATEINFO;


////////////////////////////////////////////////////////////////////////////////////////////////
//
// H.264 Picture Parameters
//

typedef struct _CUVIDH264DPBENTRY
{
    int PicIdx;             // picture index of reference frame
    int FrameIdx;           // frame_num(short-term) or LongTermFrameIdx(long-term)
    int is_long_term;       // 0=short term reference, 1=long term reference
    int not_existing;       // non-existing reference frame (corresponding PicIdx should be set to -1)
    int used_for_reference; // 0=unused, 1=top_field, 2=bottom_field, 3=both_fields
    int FieldOrderCnt[2];   // field order count of top and bottom fields
} CUVIDH264DPBENTRY;

typedef struct _CUVIDH264MVCEXT
{
    int num_views_minus1;
    int view_id;
    unsigned char inter_view_flag;
    unsigned char num_inter_view_refs_l0;
    unsigned char num_inter_view_refs_l1;
    unsigned char MVCReserved8Bits;
    int InterViewRefsL0[16];
    int InterViewRefsL1[16];
} CUVIDH264MVCEXT;

typedef struct _CUVIDH264SVCEXT
{
    unsigned char profile_idc;
    unsigned char level_idc;
    unsigned char DQId;
    unsigned char DQIdMax;
    unsigned char disable_inter_layer_deblocking_filter_idc;
    unsigned char ref_layer_chroma_phase_y_plus1;
    signed char   inter_layer_slice_alpha_c0_offset_div2;
    signed char   inter_layer_slice_beta_offset_div2;

    unsigned short DPBEntryValidFlag;
    unsigned char inter_layer_deblocking_filter_control_present_flag;
    unsigned char extended_spatial_scalability_idc;
    unsigned char adaptive_tcoeff_level_prediction_flag;
    unsigned char slice_header_restriction_flag;
    unsigned char chroma_phase_x_plus1_flag;
    unsigned char chroma_phase_y_plus1;

    unsigned char tcoeff_level_prediction_flag;
    unsigned char constrained_intra_resampling_flag;
    unsigned char ref_layer_chroma_phase_x_plus1_flag;
    unsigned char store_ref_base_pic_flag;
    unsigned char Reserved8BitsA;
    unsigned char Reserved8BitsB;
    // For the 4 scaled_ref_layer_XX fields below,
    // if (extended_spatial_scalability_idc == 1), SPS field, G.7.3.2.1.4, add prefix "seq_"
    // if (extended_spatial_scalability_idc == 2), SLH field, G.7.3.3.4,
    short scaled_ref_layer_left_offset;
    short scaled_ref_layer_top_offset;
    short scaled_ref_layer_right_offset;
    short scaled_ref_layer_bottom_offset;
    unsigned short Reserved16Bits;
    struct _CUVIDPICPARAMS *pNextLayer; // Points to the picparams for the next layer to be decoded. Linked list ends at the target layer.
    int bRefBaseLayer;                 // whether to store ref base pic
} CUVIDH264SVCEXT;

typedef struct _CUVIDH264PICPARAMS
{
    // SPS
    int log2_max_frame_num_minus4;
    int pic_order_cnt_type;
    int log2_max_pic_order_cnt_lsb_minus4;
    int delta_pic_order_always_zero_flag;
    int frame_mbs_only_flag;
    int direct_8x8_inference_flag;
    int num_ref_frames;             // NOTE: shall meet level 4.1 restrictions
    unsigned char residual_colour_transform_flag;
    unsigned char bit_depth_luma_minus8;    // Must be 0 (only 8-bit supported)
    unsigned char bit_depth_chroma_minus8;  // Must be 0 (only 8-bit supported)
    unsigned char qpprime_y_zero_transform_bypass_flag;
    // PPS
    int entropy_coding_mode_flag;
    int pic_order_present_flag;
    int num_ref_idx_l0_active_minus1;
    int num_ref_idx_l1_active_minus1;
    int weighted_pred_flag;
    int weighted_bipred_idc;
    int pic_init_qp_minus26;
    int deblocking_filter_control_present_flag;
    int redundant_pic_cnt_present_flag;
    int transform_8x8_mode_flag;
    int MbaffFrameFlag;
    int constrained_intra_pred_flag;
    int chroma_qp_index_offset;
    int second_chroma_qp_index_offset;
    int ref_pic_flag;
    int frame_num;
    int CurrFieldOrderCnt[2];
    // DPB
    CUVIDH264DPBENTRY dpb[16];          // List of reference frames within the DPB
    // Quantization Matrices (raster-order)
    unsigned char WeightScale4x4[6][16];
    unsigned char WeightScale8x8[2][64];
    // FMO/ASO
    unsigned char fmo_aso_enable;
    unsigned char num_slice_groups_minus1;
    unsigned char slice_group_map_type;
    signed char pic_init_qs_minus26;
    unsigned int slice_group_change_rate_minus1;
    union
    {
        unsigned long long slice_group_map_addr;
        const unsigned char *pMb2SliceGroupMap;
    } fmo;
    unsigned int  Reserved[12];
    // SVC/MVC
    union
    {
        CUVIDH264MVCEXT mvcext;
        CUVIDH264SVCEXT svcext;
    };
} CUVIDH264PICPARAMS;


////////////////////////////////////////////////////////////////////////////////////////////////
//
// MPEG-2 Picture Parameters
//

typedef struct _CUVIDMPEG2PICPARAMS
{
    int ForwardRefIdx;          // Picture index of forward reference (P/B-frames)
    int BackwardRefIdx;         // Picture index of backward reference (B-frames)
    int picture_coding_type;
    int full_pel_forward_vector;
    int full_pel_backward_vector;
    int f_code[2][2];
    int intra_dc_precision;
    int frame_pred_frame_dct;
    int concealment_motion_vectors;
    int q_scale_type;
    int intra_vlc_format;
    int alternate_scan;
    int top_field_first;
    // Quantization matrices (raster order)
    unsigned char QuantMatrixIntra[64];
    unsigned char QuantMatrixInter[64];
} CUVIDMPEG2PICPARAMS;

////////////////////////////////////////////////////////////////////////////////////////////////
//
// MPEG-4 Picture Parameters
//

// MPEG-4 has VOP types instead of Picture types
#define I_VOP 0
#define P_VOP 1
#define B_VOP 2
#define S_VOP 3

typedef struct _CUVIDMPEG4PICPARAMS
{
    int ForwardRefIdx;          // Picture index of forward reference (P/B-frames)
    int BackwardRefIdx;         // Picture index of backward reference (B-frames)
    // VOL
    int video_object_layer_width;
    int video_object_layer_height;
    int vop_time_increment_bitcount;
    int top_field_first;
    int resync_marker_disable;
    int quant_type;
    int quarter_sample;
    int short_video_header;
    int divx_flags;
    // VOP
    int vop_coding_type;
    int vop_coded;
    int vop_rounding_type;
    int alternate_vertical_scan_flag;
    int interlaced;
    int vop_fcode_forward;
    int vop_fcode_backward;
    int trd[2];
    int trb[2];
    // Quantization matrices (raster order)
    unsigned char QuantMatrixIntra[64];
    unsigned char QuantMatrixInter[64];
    int gmc_enabled;
} CUVIDMPEG4PICPARAMS;

////////////////////////////////////////////////////////////////////////////////////////////////
//
// VC1 Picture Parameters
//

typedef struct _CUVIDVC1PICPARAMS
{
    int ForwardRefIdx;      // Picture index of forward reference (P/B-frames)
    int BackwardRefIdx;     // Picture index of backward reference (B-frames)
    int FrameWidth;         // Actual frame width
    int FrameHeight;        // Actual frame height
    // PICTURE
    int intra_pic_flag;     // Set to 1 for I,BI frames
    int ref_pic_flag;       // Set to 1 for I,P frames
    int progressive_fcm;    // Progressive frame
    // SEQUENCE
    int profile;
    int postprocflag;
    int pulldown;
    int interlace;
    int tfcntrflag;
    int finterpflag;
    int psf;
    int multires;
    int syncmarker;
    int rangered;
    int maxbframes;
    // ENTRYPOINT
    int panscan_flag;
    int refdist_flag;
    int extended_mv;
    int dquant;
    int vstransform;
    int loopfilter;
    int fastuvmc;
    int overlap;
    int quantizer;
    int extended_dmv;
    int range_mapy_flag;
    int range_mapy;
    int range_mapuv_flag;
    int range_mapuv;
    int rangeredfrm;    // range reduction state
} CUVIDVC1PICPARAMS;

////////////////////////////////////////////////////////////////////////////////////////////////
//
// JPEG Picture Parameters
//

typedef struct _CUVIDJPEGPICPARAMS
{
    int Reserved;
} CUVIDJPEGPICPARAMS;

////////////////////////////////////////////////////////////////////////////////////////////////
//
// Picture Parameters for Decoding
//

typedef struct _CUVIDPICPARAMS
{
    int PicWidthInMbs;      // Coded Frame Size
    int FrameHeightInMbs;   // Coded Frame Height
    int CurrPicIdx;         // Output index of the current picture
    int field_pic_flag;     // 0=frame picture, 1=field picture
    int bottom_field_flag;  // 0=top field, 1=bottom field (ignored if field_pic_flag=0)
    int second_field;       // Second field of a complementary field pair
    // Bitstream data
    unsigned int nBitstreamDataLen;        // Number of bytes in bitstream data buffer
    const unsigned char *pBitstreamData;   // Ptr to bitstream data for this picture (slice-layer)
    unsigned int nNumSlices;               // Number of slices in this picture
    const unsigned int *pSliceDataOffsets; // nNumSlices entries, contains offset of each slice within the bitstream data buffer
    int ref_pic_flag;       // This picture is a reference picture
    int intra_pic_flag;     // This picture is entirely intra coded
    unsigned int Reserved[30];             // Reserved for future use
    // Codec-specific data
    union {
        CUVIDMPEG2PICPARAMS mpeg2;          // Also used for MPEG-1
        CUVIDH264PICPARAMS h264;
        CUVIDVC1PICPARAMS vc1;
        CUVIDMPEG4PICPARAMS mpeg4;
        CUVIDJPEGPICPARAMS jpeg;
        unsigned int CodecReserved[1024];
    } CodecSpecific;
} CUVIDPICPARAMS;


////////////////////////////////////////////////////////////////////////////////////////////////
//
// Post-processing
//

typedef struct _CUVIDPROCPARAMS
{
    int progressive_frame;  // Input is progressive (deinterlace_mode will be ignored)
    int second_field;       // Output the second field (ignored if deinterlace mode is Weave)
    int top_field_first;    // Input frame is top field first (1st field is top, 2nd field is bottom)
    int unpaired_field;     // Input only contains one field (2nd field is invalid)
    // The fields below are used for raw YUV input
    unsigned int reserved_flags;        // Reserved for future use (set to zero)
    unsigned int reserved_zero;         // Reserved (set to zero)
    unsigned long long raw_input_dptr;  // Input CUdeviceptr for raw YUV extensions
    unsigned int raw_input_pitch;       // pitch in bytes of raw YUV input (should be aligned appropriately)
    unsigned int raw_input_format;      // Reserved for future use (set to zero)
    unsigned long long raw_output_dptr; // Reserved for future use (set to zero)
    unsigned int raw_output_pitch;      // Reserved for future use (set to zero)
    unsigned int Reserved[48];
    void *Reserved3[3];
} CUVIDPROCPARAMS;

////////////////////////////////////////////////////////////////////////////////////////////////
//
// In order to maximize decode latencies, there should be always at least 2 pictures in the decode
// queue at any time, in order to make sure that all decode engines are always busy.
//
// Overall data flow:
//  - cuvidCreateDecoder(...)
//  For each picture:
//  - cuvidDecodePicture(N)
//  - cuvidMapVideoFrame(N-4)
//  - do some processing in cuda
//  - cuvidUnmapVideoFrame(N-4)
//  - cuvidDecodePicture(N+1)
//  - cuvidMapVideoFrame(N-3)
//    ...
//  - cuvidDestroyDecoder(...)
//
// NOTE:
// - In the current version, the cuda context MUST be created from a D3D device, using cuD3D9CtxCreate function.
//   For multi-threaded operation, the D3D device must also be created with the D3DCREATE_MULTITHREADED flag.
// - There is a limit to how many pictures can be mapped simultaneously (ulNumOutputSurfaces)
// - cuVidDecodePicture may block the calling thread if there are too many pictures pending
//   in the decode queue
//
////////////////////////////////////////////////////////////////////////////////////////////////

// Create/Destroy the decoder object
extern CUresult CUDAAPI cuvidCreateDecoder(CUvideodecoder *phDecoder, CUVIDDECODECREATEINFO *pdci);
extern CUresult CUDAAPI cuvidDestroyDecoder(CUvideodecoder hDecoder);

// Decode a single picture (field or frame)
extern CUresult CUDAAPI cuvidDecodePicture(CUvideodecoder hDecoder, CUVIDPICPARAMS *pPicParams);

#if !defined(__CUVID_DEVPTR64) || defined(__CUVID_INTERNAL)
// Post-process and map a video frame for use in cuda
extern CUresult CUDAAPI cuvidMapVideoFrame(CUvideodecoder hDecoder, int nPicIdx,
                                           unsigned int *pDevPtr, unsigned int *pPitch,
                                           CUVIDPROCPARAMS *pVPP);
// Unmap a previously mapped video frame
extern CUresult CUDAAPI cuvidUnmapVideoFrame(CUvideodecoder hDecoder, unsigned int DevPtr);
#endif

#if defined(__x86_64) || defined(AMD64) || defined(_M_AMD64)
extern CUresult CUDAAPI cuvidMapVideoFrame64(CUvideodecoder hDecoder, int nPicIdx, unsigned long long *pDevPtr,
                                             unsigned int *pPitch, CUVIDPROCPARAMS *pVPP);
extern CUresult CUDAAPI cuvidUnmapVideoFrame64(CUvideodecoder hDecoder, unsigned long long DevPtr);
#if defined(__CUVID_DEVPTR64) && !defined(__CUVID_INTERNAL)
#define cuvidMapVideoFrame      cuvidMapVideoFrame64
#define cuvidUnmapVideoFrame    cuvidUnmapVideoFrame64
#endif
#endif

// Get the pointer to the d3d9 surface that is the decode RT
extern CUresult CUDAAPI cuvidGetVideoFrameSurface(CUvideodecoder hDecoder, int nPicIdx, void **pSrcSurface);

////////////////////////////////////////////////////////////////////////////////////////////////
//
// Context-locking: to facilitate multi-threaded implementations, the following 4 functions
// provide a simple mutex-style host synchronization. If a non-NULL context is specified
// in CUVIDDECODECREATEINFO, the codec library will acquire the mutex associated with the given
// context before making any cuda calls.
// A multi-threaded application could create a lock associated with a context handle so that
// multiple threads can safely share the same cuda context:
//  - use cuCtxPopCurrent immediately after context creation in order to create a 'floating' context
//    that can be passed to cuvidCtxLockCreate.
//  - When using a floating context, all cuda calls should only be made within a cuvidCtxLock/cuvidCtxUnlock section.
//
// NOTE: This is a safer alternative to cuCtxPushCurrent and cuCtxPopCurrent, and is not related to video
// decoder in any way (implemented as a critical section associated with cuCtx{Push|Pop}Current calls).

extern CUresult CUDAAPI cuvidCtxLockCreate(CUvideoctxlock *pLock, CUcontext ctx);
extern CUresult CUDAAPI cuvidCtxLockDestroy(CUvideoctxlock lck);
extern CUresult CUDAAPI cuvidCtxLock(CUvideoctxlock lck, unsigned int reserved_flags);
extern CUresult CUDAAPI cuvidCtxUnlock(CUvideoctxlock lck, unsigned int reserved_flags);

////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
}

// Auto-lock helper for C++ applications
class CCtxAutoLock
{
private:
    CUvideoctxlock m_ctx;
public:
    CCtxAutoLock(CUvideoctxlock ctx):m_ctx(ctx) { cuvidCtxLock(m_ctx,0); }
    ~CCtxAutoLock() { cuvidCtxUnlock(m_ctx,0); }
};

#endif /* __cplusplus */

#endif // __CUDA_VIDEO_H__
