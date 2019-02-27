/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/*!\defgroup aom AOM
 * \ingroup codecs
 * AOM is aom's newest video compression algorithm that uses motion
 * compensated prediction, Discrete Cosine Transform (DCT) coding of the
 * prediction error signal and context dependent entropy coding techniques
 * based on arithmetic principles. It features:
 *  - YUV 4:2:0 image format
 *  - Macro-block based coding (16x16 luma plus two 8x8 chroma)
 *  - 1/4 (1/8) pixel accuracy motion compensated prediction
 *  - 4x4 DCT transform
 *  - 128 level linear quantizer
 *  - In loop deblocking filter
 *  - Context-based entropy coding
 *
 * @{
 */
/*!\file
 * \brief Provides controls common to both the AOM encoder and decoder.
 */
#ifndef AOM_AOM_AOM_H_
#define AOM_AOM_AOM_H_

#include "aom/aom_codec.h"
#include "aom/aom_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!\brief Control functions
 *
 * The set of macros define the control functions of AOM interface
 */
enum aom_com_control_id {
  /*!\brief pass in an external frame into decoder to be used as reference frame
   */
  AOM_SET_POSTPROC = 3, /**< set the decoder's post processing settings  */
  AOM_SET_DBG_COLOR_REF_FRAME =
      4, /**< set the reference frames to color for each macroblock */
  AOM_SET_DBG_COLOR_MB_MODES = 5, /**< set which macro block modes to color */
  AOM_SET_DBG_COLOR_B_MODES = 6,  /**< set which blocks modes to color */
  AOM_SET_DBG_DISPLAY_MV = 7,     /**< set which motion vector modes to draw */

  /* TODO(jkoleszar): The encoder incorrectly reuses some of these values (5+)
   * for its control ids. These should be migrated to something like the
   * AOM_DECODER_CTRL_ID_START range next time we're ready to break the ABI.
   */
  AV1_GET_REFERENCE = 128, /**< get a pointer to a reference frame */
  AV1_SET_REFERENCE = 129, /**< write a frame into a reference buffer */
  AV1_COPY_REFERENCE =
      130, /**< get a copy of reference frame from the decoder */
  AOM_COMMON_CTRL_ID_MAX,

  AV1_GET_NEW_FRAME_IMAGE = 192, /**< get a pointer to the new frame */
  AV1_COPY_NEW_FRAME_IMAGE =
      193, /**< copy the new frame to an external buffer */

  AOM_DECODER_CTRL_ID_START = 256
};

/*!\brief post process flags
 *
 * The set of macros define AOM decoder post processing flags
 */
enum aom_postproc_level {
  AOM_NOFILTERING = 0,
  AOM_DEBLOCK = 1 << 0,
  AOM_DEMACROBLOCK = 1 << 1,
  AOM_ADDNOISE = 1 << 2,
  AOM_DEBUG_TXT_FRAME_INFO = 1 << 3, /**< print frame information */
  AOM_DEBUG_TXT_MBLK_MODES =
      1 << 4, /**< print macro block modes over each macro block */
  AOM_DEBUG_TXT_DC_DIFF = 1 << 5,   /**< print dc diff for each macro block */
  AOM_DEBUG_TXT_RATE_INFO = 1 << 6, /**< print video rate info (encoder only) */
  AOM_MFQE = 1 << 10
};

/*!\brief post process flags
 *
 * This define a structure that describe the post processing settings. For
 * the best objective measure (using the PSNR metric) set post_proc_flag
 * to AOM_DEBLOCK and deblocking_level to 1.
 */

typedef struct aom_postproc_cfg {
  /*!\brief the types of post processing to be done, should be combination of
   * "aom_postproc_level" */
  int post_proc_flag;
  int deblocking_level; /**< the strength of deblocking, valid range [0, 16] */
  int noise_level; /**< the strength of additive noise, valid range [0, 16] */
} aom_postproc_cfg_t;

/*!\brief AV1 specific reference frame data struct
 *
 * Define the data struct to access av1 reference frames.
 */
typedef struct av1_ref_frame {
  int idx;              /**< frame index to get (input) */
  int use_external_ref; /**< Directly use external ref buffer(decoder only) */
  aom_image_t img;      /**< img structure to populate (output) */
} av1_ref_frame_t;

/*!\cond */
/*!\brief aom decoder control function parameter type
 *
 * defines the data type for each of AOM decoder control function requires
 */
AOM_CTRL_USE_TYPE(AOM_SET_POSTPROC, aom_postproc_cfg_t *)
#define AOM_CTRL_AOM_SET_POSTPROC
AOM_CTRL_USE_TYPE(AOM_SET_DBG_COLOR_REF_FRAME, int)
#define AOM_CTRL_AOM_SET_DBG_COLOR_REF_FRAME
AOM_CTRL_USE_TYPE(AOM_SET_DBG_COLOR_MB_MODES, int)
#define AOM_CTRL_AOM_SET_DBG_COLOR_MB_MODES
AOM_CTRL_USE_TYPE(AOM_SET_DBG_COLOR_B_MODES, int)
#define AOM_CTRL_AOM_SET_DBG_COLOR_B_MODES
AOM_CTRL_USE_TYPE(AOM_SET_DBG_DISPLAY_MV, int)
#define AOM_CTRL_AOM_SET_DBG_DISPLAY_MV
AOM_CTRL_USE_TYPE(AV1_GET_REFERENCE, av1_ref_frame_t *)
#define AOM_CTRL_AV1_GET_REFERENCE
AOM_CTRL_USE_TYPE(AV1_SET_REFERENCE, av1_ref_frame_t *)
#define AOM_CTRL_AV1_SET_REFERENCE
AOM_CTRL_USE_TYPE(AV1_COPY_REFERENCE, av1_ref_frame_t *)
#define AOM_CTRL_AV1_COPY_REFERENCE
AOM_CTRL_USE_TYPE(AV1_GET_NEW_FRAME_IMAGE, aom_image_t *)
#define AOM_CTRL_AV1_GET_NEW_FRAME_IMAGE
AOM_CTRL_USE_TYPE(AV1_COPY_NEW_FRAME_IMAGE, aom_image_t *)
#define AOM_CTRL_AV1_COPY_NEW_FRAME_IMAGE

/*!\endcond */
/*! @} - end defgroup aom */

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOM_AOM_H_
