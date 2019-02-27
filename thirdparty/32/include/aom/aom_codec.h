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

/*!\defgroup codec Common Algorithm Interface
 * This abstraction allows applications to easily support multiple video
 * formats with minimal code duplication. This section describes the interface
 * common to all codecs (both encoders and decoders).
 * @{
 */

/*!\file
 * \brief Describes the codec algorithm interface to applications.
 *
 * This file describes the interface between an application and a
 * video codec algorithm.
 *
 * An application instantiates a specific codec instance by using
 * aom_codec_init() and a pointer to the algorithm's interface structure:
 *     <pre>
 *     my_app.c:
 *       extern aom_codec_iface_t my_codec;
 *       {
 *           aom_codec_ctx_t algo;
 *           res = aom_codec_init(&algo, &my_codec);
 *       }
 *     </pre>
 *
 * Once initialized, the instance is managed using other functions from
 * the aom_codec_* family.
 */
#ifndef AOM_AOM_AOM_CODEC_H_
#define AOM_AOM_AOM_CODEC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "aom/aom_image.h"
#include "aom/aom_integer.h"

/*!\brief Decorator indicating a function is deprecated */
#ifndef AOM_DEPRECATED
#if defined(__GNUC__) && __GNUC__
#define AOM_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define AOM_DEPRECATED
#else
#define AOM_DEPRECATED
#endif
#endif /* AOM_DEPRECATED */

#ifndef AOM_DECLSPEC_DEPRECATED
#if defined(__GNUC__) && __GNUC__
#define AOM_DECLSPEC_DEPRECATED /**< \copydoc #AOM_DEPRECATED */
#elif defined(_MSC_VER)
/*!\brief \copydoc #AOM_DEPRECATED */
#define AOM_DECLSPEC_DEPRECATED __declspec(deprecated)
#else
#define AOM_DECLSPEC_DEPRECATED /**< \copydoc #AOM_DEPRECATED */
#endif
#endif /* AOM_DECLSPEC_DEPRECATED */

/*!\brief Decorator indicating a function is potentially unused */
#ifdef AOM_UNUSED
#elif defined(__GNUC__) || defined(__clang__)
#define AOM_UNUSED __attribute__((unused))
#else
#define AOM_UNUSED
#endif

/*!\brief Decorator indicating that given struct/union/enum is packed */
#ifndef ATTRIBUTE_PACKED
#if defined(__GNUC__) && __GNUC__
#define ATTRIBUTE_PACKED __attribute__((packed))
#elif defined(_MSC_VER)
#define ATTRIBUTE_PACKED
#else
#define ATTRIBUTE_PACKED
#endif
#endif /* ATTRIBUTE_PACKED */

/*!\brief Current ABI version number
 *
 * \internal
 * If this file is altered in any way that changes the ABI, this value
 * must be bumped.  Examples include, but are not limited to, changing
 * types, removing or reassigning enums, adding/removing/rearranging
 * fields to structures
 */
#define AOM_CODEC_ABI_VERSION (3 + AOM_IMAGE_ABI_VERSION) /**<\hideinitializer*/

/*!\brief Algorithm return codes */
typedef enum {
  /*!\brief Operation completed without error */
  AOM_CODEC_OK,

  /*!\brief Unspecified error */
  AOM_CODEC_ERROR,

  /*!\brief Memory operation failed */
  AOM_CODEC_MEM_ERROR,

  /*!\brief ABI version mismatch */
  AOM_CODEC_ABI_MISMATCH,

  /*!\brief Algorithm does not have required capability */
  AOM_CODEC_INCAPABLE,

  /*!\brief The given bitstream is not supported.
   *
   * The bitstream was unable to be parsed at the highest level. The decoder
   * is unable to proceed. This error \ref SHOULD be treated as fatal to the
   * stream. */
  AOM_CODEC_UNSUP_BITSTREAM,

  /*!\brief Encoded bitstream uses an unsupported feature
   *
   * The decoder does not implement a feature required by the encoder. This
   * return code should only be used for features that prevent future
   * pictures from being properly decoded. This error \ref MAY be treated as
   * fatal to the stream or \ref MAY be treated as fatal to the current GOP.
   */
  AOM_CODEC_UNSUP_FEATURE,

  /*!\brief The coded data for this stream is corrupt or incomplete
   *
   * There was a problem decoding the current frame.  This return code
   * should only be used for failures that prevent future pictures from
   * being properly decoded. This error \ref MAY be treated as fatal to the
   * stream or \ref MAY be treated as fatal to the current GOP. If decoding
   * is continued for the current GOP, artifacts may be present.
   */
  AOM_CODEC_CORRUPT_FRAME,

  /*!\brief An application-supplied parameter is not valid.
   *
   */
  AOM_CODEC_INVALID_PARAM,

  /*!\brief An iterator reached the end of list.
   *
   */
  AOM_CODEC_LIST_END

} aom_codec_err_t;

/*! \brief Codec capabilities bitfield
 *
 *  Each codec advertises the capabilities it supports as part of its
 *  ::aom_codec_iface_t interface structure. Capabilities are extra interfaces
 *  or functionality, and are not required to be supported.
 *
 *  The available flags are specified by AOM_CODEC_CAP_* defines.
 */
typedef long aom_codec_caps_t;
#define AOM_CODEC_CAP_DECODER 0x1 /**< Is a decoder */
#define AOM_CODEC_CAP_ENCODER 0x2 /**< Is an encoder */

/*! \brief Initialization-time Feature Enabling
 *
 *  Certain codec features must be known at initialization time, to allow for
 *  proper memory allocation.
 *
 *  The available flags are specified by AOM_CODEC_USE_* defines.
 */
typedef long aom_codec_flags_t;

/*!\brief Codec interface structure.
 *
 * Contains function pointers and other data private to the codec
 * implementation. This structure is opaque to the application.
 */
typedef const struct aom_codec_iface aom_codec_iface_t;

/*!\brief Codec private data structure.
 *
 * Contains data private to the codec implementation. This structure is opaque
 * to the application.
 */
typedef struct aom_codec_priv aom_codec_priv_t;

/*!\brief Iterator
 *
 * Opaque storage used for iterating over lists.
 */
typedef const void *aom_codec_iter_t;

/*!\brief Codec context structure
 *
 * All codecs \ref MUST support this context structure fully. In general,
 * this data should be considered private to the codec algorithm, and
 * not be manipulated or examined by the calling application. Applications
 * may reference the 'name' member to get a printable description of the
 * algorithm.
 */
typedef struct aom_codec_ctx {
  const char *name;             /**< Printable interface name */
  aom_codec_iface_t *iface;     /**< Interface pointers */
  aom_codec_err_t err;          /**< Last returned error */
  const char *err_detail;       /**< Detailed info, if available */
  aom_codec_flags_t init_flags; /**< Flags passed at init time */
  union {
    /**< Decoder Configuration Pointer */
    const struct aom_codec_dec_cfg *dec;
    /**< Encoder Configuration Pointer */
    const struct aom_codec_enc_cfg *enc;
    const void *raw;
  } config;               /**< Configuration pointer aliasing union */
  aom_codec_priv_t *priv; /**< Algorithm private storage */
} aom_codec_ctx_t;

/*!\brief Bit depth for codec
 * *
 * This enumeration determines the bit depth of the codec.
 */
typedef enum aom_bit_depth {
  AOM_BITS_8 = 8,   /**<  8 bits */
  AOM_BITS_10 = 10, /**< 10 bits */
  AOM_BITS_12 = 12, /**< 12 bits */
} aom_bit_depth_t;

/*!\brief Superblock size selection.
 *
 * Defines the superblock size used for encoding. The superblock size can
 * either be fixed at 64x64 or 128x128 pixels, or it can be dynamically
 * selected by the encoder for each frame.
 */
typedef enum aom_superblock_size {
  AOM_SUPERBLOCK_SIZE_64X64,   /**< Always use 64x64 superblocks. */
  AOM_SUPERBLOCK_SIZE_128X128, /**< Always use 128x128 superblocks. */
  AOM_SUPERBLOCK_SIZE_DYNAMIC  /**< Select superblock size dynamically. */
} aom_superblock_size_t;

/*
 * Library Version Number Interface
 *
 * For example, see the following sample return values:
 *     aom_codec_version()           (1<<16 | 2<<8 | 3)
 *     aom_codec_version_str()       "v1.2.3-rc1-16-gec6a1ba"
 *     aom_codec_version_extra_str() "rc1-16-gec6a1ba"
 */

/*!\brief Return the version information (as an integer)
 *
 * Returns a packed encoding of the library version number. This will only
 * include
 * the major.minor.patch component of the version number. Note that this encoded
 * value should be accessed through the macros provided, as the encoding may
 * change
 * in the future.
 *
 */
int aom_codec_version(void);

/*!\brief Return the version major number */
#define aom_codec_version_major() ((aom_codec_version() >> 16) & 0xff)

/*!\brief Return the version minor number */
#define aom_codec_version_minor() ((aom_codec_version() >> 8) & 0xff)

/*!\brief Return the version patch number */
#define aom_codec_version_patch() ((aom_codec_version() >> 0) & 0xff)

/*!\brief Return the version information (as a string)
 *
 * Returns a printable string containing the full library version number. This
 * may
 * contain additional text following the three digit version number, as to
 * indicate
 * release candidates, prerelease versions, etc.
 *
 */
const char *aom_codec_version_str(void);

/*!\brief Return the version information (as a string)
 *
 * Returns a printable "extra string". This is the component of the string
 * returned
 * by aom_codec_version_str() following the three digit version number.
 *
 */
const char *aom_codec_version_extra_str(void);

/*!\brief Return the build configuration
 *
 * Returns a printable string containing an encoded version of the build
 * configuration. This may be useful to aom support.
 *
 */
const char *aom_codec_build_config(void);

/*!\brief Return the name for a given interface
 *
 * Returns a human readable string for name of the given codec interface.
 *
 * \param[in]    iface     Interface pointer
 *
 */
const char *aom_codec_iface_name(aom_codec_iface_t *iface);

/*!\brief Convert error number to printable string
 *
 * Returns a human readable string for the last error returned by the
 * algorithm. The returned error will be one line and will not contain
 * any newline characters.
 *
 *
 * \param[in]    err     Error number.
 *
 */
const char *aom_codec_err_to_string(aom_codec_err_t err);

/*!\brief Retrieve error synopsis for codec context
 *
 * Returns a human readable string for the last error returned by the
 * algorithm. The returned error will be one line and will not contain
 * any newline characters.
 *
 *
 * \param[in]    ctx     Pointer to this instance's context.
 *
 */
const char *aom_codec_error(aom_codec_ctx_t *ctx);

/*!\brief Retrieve detailed error information for codec context
 *
 * Returns a human readable string providing detailed information about
 * the last error.
 *
 * \param[in]    ctx     Pointer to this instance's context.
 *
 * \retval NULL
 *     No detailed information is available.
 */
const char *aom_codec_error_detail(aom_codec_ctx_t *ctx);

/* REQUIRED FUNCTIONS
 *
 * The following functions are required to be implemented for all codecs.
 * They represent the base case functionality expected of all codecs.
 */

/*!\brief Destroy a codec instance
 *
 * Destroys a codec context, freeing any associated memory buffers.
 *
 * \param[in] ctx   Pointer to this instance's context
 *
 * \retval #AOM_CODEC_OK
 *     The codec algorithm initialized.
 * \retval #AOM_CODEC_MEM_ERROR
 *     Memory allocation failed.
 */
aom_codec_err_t aom_codec_destroy(aom_codec_ctx_t *ctx);

/*!\brief Get the capabilities of an algorithm.
 *
 * Retrieves the capabilities bitfield from the algorithm's interface.
 *
 * \param[in] iface   Pointer to the algorithm interface
 *
 */
aom_codec_caps_t aom_codec_get_caps(aom_codec_iface_t *iface);

/*!\brief Control algorithm
 *
 * This function is used to exchange algorithm specific data with the codec
 * instance. This can be used to implement features specific to a particular
 * algorithm.
 *
 * This wrapper function dispatches the request to the helper function
 * associated with the given ctrl_id. It tries to call this function
 * transparently, but will return #AOM_CODEC_ERROR if the request could not
 * be dispatched.
 *
 * Note that this function should not be used directly. Call the
 * #aom_codec_control wrapper macro instead.
 *
 * \param[in]     ctx              Pointer to this instance's context
 * \param[in]     ctrl_id          Algorithm specific control identifier
 *
 * \retval #AOM_CODEC_OK
 *     The control request was processed.
 * \retval #AOM_CODEC_ERROR
 *     The control request was not processed.
 * \retval #AOM_CODEC_INVALID_PARAM
 *     The data was not valid.
 */
aom_codec_err_t aom_codec_control_(aom_codec_ctx_t *ctx, int ctrl_id, ...);
#if defined(AOM_DISABLE_CTRL_TYPECHECKS) && AOM_DISABLE_CTRL_TYPECHECKS
#define aom_codec_control(ctx, id, data) aom_codec_control_(ctx, id, data)
#define AOM_CTRL_USE_TYPE(id, typ)
#define AOM_CTRL_USE_TYPE_DEPRECATED(id, typ)
#define AOM_CTRL_VOID(id, typ)

#else
/*!\brief aom_codec_control wrapper macro
 *
 * This macro allows for type safe conversions across the variadic parameter
 * to aom_codec_control_().
 *
 * \internal
 * It works by dispatching the call to the control function through a wrapper
 * function named with the id parameter.
 */
#define aom_codec_control(ctx, id, data) \
  aom_codec_control_##id(ctx, id, data) /**<\hideinitializer*/

/*!\brief aom_codec_control type definition macro
 *
 * This macro allows for type safe conversions across the variadic parameter
 * to aom_codec_control_(). It defines the type of the argument for a given
 * control identifier.
 *
 * \internal
 * It defines a static function with
 * the correctly typed arguments as a wrapper to the type-unsafe internal
 * function.
 */
#define AOM_CTRL_USE_TYPE(id, typ)                                           \
  static aom_codec_err_t aom_codec_control_##id(aom_codec_ctx_t *, int, typ) \
      AOM_UNUSED;                                                            \
                                                                             \
  static aom_codec_err_t aom_codec_control_##id(aom_codec_ctx_t *ctx,        \
                                                int ctrl_id, typ data) {     \
    return aom_codec_control_(ctx, ctrl_id, data);                           \
  } /**<\hideinitializer*/

/*!\brief aom_codec_control deprecated type definition macro
 *
 * Like #AOM_CTRL_USE_TYPE, but indicates that the specified control is
 * deprecated and should not be used. Consult the documentation for your
 * codec for more information.
 *
 * \internal
 * It defines a static function with the correctly typed arguments as a
 * wrapper to the type-unsafe internal function.
 */
#define AOM_CTRL_USE_TYPE_DEPRECATED(id, typ)                            \
  AOM_DECLSPEC_DEPRECATED static aom_codec_err_t aom_codec_control_##id( \
      aom_codec_ctx_t *, int, typ) AOM_DEPRECATED AOM_UNUSED;            \
                                                                         \
  AOM_DECLSPEC_DEPRECATED static aom_codec_err_t aom_codec_control_##id( \
      aom_codec_ctx_t *ctx, int ctrl_id, typ data) {                     \
    return aom_codec_control_(ctx, ctrl_id, data);                       \
  } /**<\hideinitializer*/

/*!\brief aom_codec_control void type definition macro
 *
 * This macro allows for type safe conversions across the variadic parameter
 * to aom_codec_control_(). It indicates that a given control identifier takes
 * no argument.
 *
 * \internal
 * It defines a static function without a data argument as a wrapper to the
 * type-unsafe internal function.
 */
#define AOM_CTRL_VOID(id)                                               \
  static aom_codec_err_t aom_codec_control_##id(aom_codec_ctx_t *, int) \
      AOM_UNUSED;                                                       \
                                                                        \
  static aom_codec_err_t aom_codec_control_##id(aom_codec_ctx_t *ctx,   \
                                                int ctrl_id) {          \
    return aom_codec_control_(ctx, ctrl_id);                            \
  } /**<\hideinitializer*/

#endif

/*!\brief OBU types. */
typedef enum ATTRIBUTE_PACKED {
  OBU_SEQUENCE_HEADER = 1,
  OBU_TEMPORAL_DELIMITER = 2,
  OBU_FRAME_HEADER = 3,
  OBU_TILE_GROUP = 4,
  OBU_METADATA = 5,
  OBU_FRAME = 6,
  OBU_REDUNDANT_FRAME_HEADER = 7,
  OBU_TILE_LIST = 8,
  OBU_PADDING = 15,
} OBU_TYPE;

/*!\brief OBU metadata types. */
typedef enum {
  OBU_METADATA_TYPE_AOM_RESERVED_0 = 0,
  OBU_METADATA_TYPE_HDR_CLL = 1,
  OBU_METADATA_TYPE_HDR_MDCV = 2,
  OBU_METADATA_TYPE_SCALABILITY = 3,
  OBU_METADATA_TYPE_ITUT_T35 = 4,
  OBU_METADATA_TYPE_TIMECODE = 5,
} OBU_METADATA_TYPE;

/*!\brief Returns string representation of OBU_TYPE.
 *
 * \param[in]     type            The OBU_TYPE to convert to string.
 */
const char *aom_obu_type_to_string(OBU_TYPE type);

/*!\brief Config Options
 *
 * This type allows to enumerate and control options defined for control
 * via config file at runtime.
 */
typedef struct cfg_options {
  /*!\brief Reflects if ext_partition should be enabled
   *
   * If this value is non-zero it enabled the feature
   */
  unsigned int ext_partition;
} cfg_options_t;

/*!@} - end defgroup codec*/
#ifdef __cplusplus
}
#endif
#endif  // AOM_AOM_AOM_CODEC_H_
