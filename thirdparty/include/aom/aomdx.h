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

/*!\defgroup aom_decoder AOMedia AOM/AV1 Decoder
 * \ingroup aom
 *
 * @{
 */
/*!\file
 * \brief Provides definitions for using AOM or AV1 within the aom Decoder
 *        interface.
 */
#ifndef AOM_AOM_AOMDX_H_
#define AOM_AOM_AOMDX_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Include controls common to both the encoder and decoder */
#include "aom/aom.h"

/*!\name Algorithm interface for AV1
 *
 * This interface provides the capability to decode AV1 streams.
 * @{
 */
extern aom_codec_iface_t aom_codec_av1_dx_algo;
extern aom_codec_iface_t *aom_codec_av1_dx(void);
/*!@} - end algorithm interface member group*/

/** Data structure that stores bit accounting for debug
 */
typedef struct Accounting Accounting;

#ifndef AOM_INSPECTION_H_
/** Callback that inspects decoder frame data.
 */
typedef void (*aom_inspect_cb)(void *decoder, void *ctx);
#endif

/*!\brief Structure to hold inspection callback and context.
 *
 * Defines a structure to hold the inspection callback function and calling
 * context.
 */
typedef struct aom_inspect_init {
  /*! Inspection callback. */
  aom_inspect_cb inspect_cb;

  /*! Inspection context. */
  void *inspect_ctx;
} aom_inspect_init;

/*!\brief Structure to hold a tile's start address and size in the bitstream.
 *
 * Defines a structure to hold a tile's start address and size in the bitstream.
 */
typedef struct aom_tile_data {
  /*! Tile data size. */
  size_t coded_tile_data_size;
  /*! Tile's start address. */
  const void *coded_tile_data;
  /*! Extra size information. */
  size_t extra_size;
} aom_tile_data;

/*!\brief Structure to hold the external reference frame pointer.
 *
 * Define a structure to hold the external reference frame pointer.
 */
typedef struct av1_ext_ref_frame {
  /*! Start pointer of external references. */
  aom_image_t *img;
  /*! Number of available external references. */
  int num;
} av1_ext_ref_frame_t;

/*!\enum aom_dec_control_id
 * \brief AOM decoder control functions
 *
 * This set of macros define the control functions available for the AOM
 * decoder interface.
 *
 * \sa #aom_codec_control
 */
enum aom_dec_control_id {
  /** control function to get info on which reference frames were updated
   *  by the last decode
   */
  AOMD_GET_LAST_REF_UPDATES = AOM_DECODER_CTRL_ID_START,

  /** check if the indicated frame is corrupted */
  AOMD_GET_FRAME_CORRUPTED,

  /** control function to get info on which reference frames were used
   *  by the last decode
   */
  AOMD_GET_LAST_REF_USED,

  /** control function to get the dimensions that the current frame is decoded
   * at. This may be different to the intended display size for the frame as
   * specified in the wrapper or frame header (see AV1D_GET_DISPLAY_SIZE). */
  AV1D_GET_FRAME_SIZE,

  /** control function to get the current frame's intended display dimensions
   * (as specified in the wrapper or frame header). This may be different to
   * the decoded dimensions of this frame (see AV1D_GET_FRAME_SIZE). */
  AV1D_GET_DISPLAY_SIZE,

  /** control function to get the bit depth of the stream. */
  AV1D_GET_BIT_DEPTH,

  /** control function to get the image format of the stream. */
  AV1D_GET_IMG_FORMAT,

  /** control function to get the size of the tile. */
  AV1D_GET_TILE_SIZE,

  /** control function to set the byte alignment of the planes in the reference
   * buffers. Valid values are power of 2, from 32 to 1024. A value of 0 sets
   * legacy alignment. I.e. Y plane is aligned to 32 bytes, U plane directly
   * follows Y plane, and V plane directly follows U plane. Default value is 0.
   */
  AV1_SET_BYTE_ALIGNMENT,

  /** control function to invert the decoding order to from right to left. The
   * function is used in a test to confirm the decoding independence of tile
   * columns. The function may be used in application where this order
   * of decoding is desired.
   *
   * TODO(yaowu): Rework the unit test that uses this control, and in a future
   *              release, this test-only control shall be removed.
   */
  AV1_INVERT_TILE_DECODE_ORDER,

  /** control function to set the skip loop filter flag. Valid values are
   * integers. The decoder will skip the loop filter when its value is set to
   * nonzero. If the loop filter is skipped the decoder may accumulate decode
   * artifacts. The default value is 0.
   */
  AV1_SET_SKIP_LOOP_FILTER,

  /** control function to retrieve a pointer to the Accounting struct.  When
   * compiled without --enable-accounting, this returns AOM_CODEC_INCAPABLE.
   * If called before a frame has been decoded, this returns AOM_CODEC_ERROR.
   * The caller should ensure that AOM_CODEC_OK is returned before attempting
   * to dereference the Accounting pointer.
   */
  AV1_GET_ACCOUNTING,

  /** control function to get last decoded frame quantizer. Returned value uses
   * internal quantizer scale defined by the codec.
   */
  AOMD_GET_LAST_QUANTIZER,

  /** control function to set the range of tile decoding. A value that is
   * greater and equal to zero indicates only the specific row/column is
   * decoded. A value that is -1 indicates the whole row/column is decoded.
   * A special case is both values are -1 that means the whole frame is
   * decoded.
   */
  AV1_SET_DECODE_TILE_ROW,
  AV1_SET_DECODE_TILE_COL,
  /** control function to set the tile coding mode. A value that is equal to
   *  zero indicates the tiles are coded in normal tile mode. A value that is
   *  1 indicates the tiles are coded in large-scale tile mode.
   */
  AV1_SET_TILE_MODE,
  /** control function to get the frame header information of an encoded frame
   * in the bitstream. This provides a way to access a frame's header data.
   */
  AV1D_GET_FRAME_HEADER_INFO,
  /** control function to get the start address and size of a tile in the coded
   * bitstream. This provides a way to access a specific tile's bitstream data.
   */
  AV1D_GET_TILE_DATA,
  /** control function to set the external references' pointers in the decoder.
   *  This is used while decoding the tile list OBU in large-scale tile coding
   *  mode.
   */
  AV1D_SET_EXT_REF_PTR,
  /** control function to enable the ext-tile software debug and testing code in
   * the decoder.
   */
  AV1D_EXT_TILE_DEBUG,

  /** control function to enable the row based multi-threading of decoding. A
   * value that is equal to 1 indicates that row based multi-threading is
   * enabled.
   */
  AV1D_SET_ROW_MT,

  /** control function to indicate whether bitstream is in Annex-B format. */
  AV1D_SET_IS_ANNEXB,

  /** control function to indicate which operating point to use. A scalable
   *  stream may define multiple operating points, each of which defines a
   *  set of temporal and spatial layers to be processed. The operating point
   *  index may take a value between 0 and operating_points_cnt_minus_1 (which
   *  is at most 31).
   */
  AV1D_SET_OPERATING_POINT,

  /** control function to indicate whether to output one frame per temporal
   *  unit (the default), or one frame per spatial layer.
   *  In a scalable stream, each temporal unit corresponds to a single "frame"
   *  of video, and within a temporal unit there may be multiple spatial layers
   *  with different versions of that frame.
   *  For video playback, only the highest-quality version (within the
   *  selected operating point) is needed, but for some use cases it is useful
   *  to have access to multiple versions of a frame when they are available.
   */
  AV1D_SET_OUTPUT_ALL_LAYERS,

  /** control function to set an aom_inspect_cb callback that is invoked each
   * time a frame is decoded.  When compiled without --enable-inspection, this
   * returns AOM_CODEC_INCAPABLE.
   */
  AV1_SET_INSPECTION_CALLBACK,

  /** control function to set the skip film grain flag. Valid values are
   * integers. The decoder will skip the film grain when its value is set to
   * nonzero. The default value is 0.
   */
  AV1D_SET_SKIP_FILM_GRAIN,

  AOM_DECODER_CTRL_ID_MAX,
};

/*!\cond */
/*!\brief AOM decoder control function parameter type
 *
 * Defines the data types that AOMD control functions take. Note that
 * additional common controls are defined in aom.h
 *
 */

AOM_CTRL_USE_TYPE(AOMD_GET_LAST_REF_UPDATES, int *)
#define AOM_CTRL_AOMD_GET_LAST_REF_UPDATES
AOM_CTRL_USE_TYPE(AOMD_GET_FRAME_CORRUPTED, int *)
#define AOM_CTRL_AOMD_GET_FRAME_CORRUPTED
AOM_CTRL_USE_TYPE(AOMD_GET_LAST_REF_USED, int *)
#define AOM_CTRL_AOMD_GET_LAST_REF_USED
AOM_CTRL_USE_TYPE(AOMD_GET_LAST_QUANTIZER, int *)
#define AOM_CTRL_AOMD_GET_LAST_QUANTIZER
AOM_CTRL_USE_TYPE(AV1D_GET_DISPLAY_SIZE, int *)
#define AOM_CTRL_AV1D_GET_DISPLAY_SIZE
AOM_CTRL_USE_TYPE(AV1D_GET_BIT_DEPTH, unsigned int *)
#define AOM_CTRL_AV1D_GET_BIT_DEPTH
AOM_CTRL_USE_TYPE(AV1D_GET_IMG_FORMAT, aom_img_fmt_t *)
#define AOM_CTRL_AV1D_GET_IMG_FORMAT
AOM_CTRL_USE_TYPE(AV1D_GET_TILE_SIZE, unsigned int *)
#define AOM_CTRL_AV1D_GET_TILE_SIZE
AOM_CTRL_USE_TYPE(AV1D_GET_FRAME_SIZE, int *)
#define AOM_CTRL_AV1D_GET_FRAME_SIZE
AOM_CTRL_USE_TYPE(AV1_INVERT_TILE_DECODE_ORDER, int)
#define AOM_CTRL_AV1_INVERT_TILE_DECODE_ORDER
AOM_CTRL_USE_TYPE(AV1_GET_ACCOUNTING, Accounting **)
#define AOM_CTRL_AV1_GET_ACCOUNTING
AOM_CTRL_USE_TYPE(AV1_SET_DECODE_TILE_ROW, int)
#define AOM_CTRL_AV1_SET_DECODE_TILE_ROW
AOM_CTRL_USE_TYPE(AV1_SET_DECODE_TILE_COL, int)
#define AOM_CTRL_AV1_SET_DECODE_TILE_COL
AOM_CTRL_USE_TYPE(AV1_SET_TILE_MODE, unsigned int)
#define AOM_CTRL_AV1_SET_TILE_MODE
AOM_CTRL_USE_TYPE(AV1D_GET_FRAME_HEADER_INFO, aom_tile_data *)
#define AOM_CTRL_AV1D_GET_FRAME_HEADER_INFO
AOM_CTRL_USE_TYPE(AV1D_GET_TILE_DATA, aom_tile_data *)
#define AOM_CTRL_AV1D_GET_TILE_DATA
AOM_CTRL_USE_TYPE(AV1D_SET_EXT_REF_PTR, av1_ext_ref_frame_t *)
#define AOM_CTRL_AV1D_SET_EXT_REF_PTR
AOM_CTRL_USE_TYPE(AV1D_EXT_TILE_DEBUG, unsigned int)
#define AOM_CTRL_AV1D_EXT_TILE_DEBUG
AOM_CTRL_USE_TYPE(AV1D_SET_ROW_MT, unsigned int)
#define AOM_CTRL_AV1D_SET_ROW_MT
AOM_CTRL_USE_TYPE(AV1D_SET_SKIP_FILM_GRAIN, int)
#define AOM_CTRL_AV1D_SET_SKIP_FILM_GRAIN
AOM_CTRL_USE_TYPE(AV1D_SET_IS_ANNEXB, unsigned int)
#define AOM_CTRL_AV1D_SET_IS_ANNEXB
AOM_CTRL_USE_TYPE(AV1D_SET_OPERATING_POINT, int)
#define AOM_CTRL_AV1D_SET_OPERATING_POINT
AOM_CTRL_USE_TYPE(AV1D_SET_OUTPUT_ALL_LAYERS, int)
#define AOM_CTRL_AV1D_SET_OUTPUT_ALL_LAYERS
AOM_CTRL_USE_TYPE(AV1_SET_INSPECTION_CALLBACK, aom_inspect_init *)
#define AOM_CTRL_AV1_SET_INSPECTION_CALLBACK
/*!\endcond */
/*! @} - end defgroup aom_decoder */

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOM_AOMDX_H_
