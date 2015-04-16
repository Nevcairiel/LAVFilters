/*
 * This file is part of libdcadec.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef DCA_CONTEXT_H
#define DCA_CONTEXT_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/**@{*/
#ifdef _WIN32
#define DCADEC_SHARED_EXPORT    __declspec(dllexport)
#define DCADEC_SHARED_IMPORT    __declspec(dllimport)
#else
#define DCADEC_SHARED_EXPORT    __attribute__((visibility("default")))
#define DCADEC_SHARED_IMPORT    __attribute__((visibility("default")))
#endif

#ifdef DCADEC_SHARED
#ifdef DCADEC_INTERNAL
#define DCADEC_API  DCADEC_SHARED_EXPORT
#else
#define DCADEC_API  DCADEC_SHARED_IMPORT
#endif
#else
#define DCADEC_API
#endif
/**@}*/

/**@{*/
#define DCADEC_EINVAL       1   /**< Invalid argument */
#define DCADEC_EBADDATA     2   /**< Invalid bitstream format */
#define DCADEC_EBADCRC      3   /**< CRC check failed */
#define DCADEC_EBADREAD     4   /**< Bitstream navigation error */
#define DCADEC_ENOSYNC      5   /**< Synchronization error */
#define DCADEC_ENOSUP       6   /**< Unsupported feature */
#define DCADEC_ENOMEM       7   /**< Memory allocation error */
#define DCADEC_EOVERFLOW    8   /**< PCM output overflow */
#define DCADEC_EIO          9   /**< I/O error */
#define DCADEC_EOUTCHG     10   /**< PCM output parameters changed */
#define DCADEC_EFAIL       32   /**< Unspecified error */
/**@}*/

/**@{*/
/** Decode DTS core only without extensions */
#define DCADEC_FLAG_CORE_ONLY           0x01

/** Force bit exact DTS core decoding */
#define DCADEC_FLAG_CORE_BIT_EXACT      0x02

/**
 * Force DTS core synthesis using X96 filter, with high frequency subbands set
 * to zero. If actual X96 data is present, it is discarded when this flag is
 * set. Effectively doubles output sample rate.
 */
#define DCADEC_FLAG_CORE_SYNTH_X96      0x04

/** Force DTS core bit width reducion to source PCM resolution */
#define DCADEC_FLAG_CORE_SOURCE_PCM_RES 0x08

/* Use FIR filter for floating point DTS core LFE channel interpolation */
#define DCADEC_FLAG_CORE_LFE_FIR        0x10

/** Extract embedded 2.0 downmix */
#define DCADEC_FLAG_KEEP_DMIX_2CH       0x20

/** Extract embedded 5.1 downmix */
#define DCADEC_FLAG_KEEP_DMIX_6CH       0x40

/** Output native DTS channel layout, not WAVEFORMATEXTENSIBLE layout */
#define DCADEC_FLAG_NATIVE_LAYOUT       0x80

/** Don't conceal errors */
#define DCADEC_FLAG_STRICT              0x100
/**@}*/

/**@{*/
#define DCADEC_PROFILE_UNKNOWN  0       /**< Unknown Profile */
#define DCADEC_PROFILE_DS       0x01    /**< Digital Surround */
#define DCADEC_PROFILE_DS_96_24 0x02    /**< Digital Surround 96/24 */
#define DCADEC_PROFILE_DS_ES    0x04    /**< Digital Surround ES */
#define DCADEC_PROFILE_HD_HRA   0x08    /**< High-Resolution Audio */
#define DCADEC_PROFILE_HD_MA    0x10    /**< Master Audio */
#define DCADEC_PROFILE_EXPRESS  0x20    /**< Express */
/**@}*/

/**
 * Size in bytes of empty padding that must be present after the end of input
 * buffer. libdcadec may overread the input buffer up to this number of bytes.
 */
#define DCADEC_BUFFER_PADDING   8

struct dcadec_context;

struct dcadec_core_info {
    int     nchannels;      /**< Number of primary audio channels */
    int     audio_mode;     /**< Core audio channel arrangement (AMODE) */
    int     lfe_present;    /**< LFE channel presence flag (can be 0, 1 or 2) */
    int     sample_rate;    /**< Core audio sample rate in Hz */
    int     source_pcm_res; /**< Source PCM resolution in bits */
    bool    es_format;      /**< Extended surround (ES) mastering flag */
    int     bit_rate;       /**< Core stream bit rate in bytes per second,
                                 zero or negative if unavailable */
    int     npcmblocks;     /**< Number of audio sample blocks in a frame */
    bool    xch_present;    /**< XCH extension data present and valid */
    bool    xxch_present;   /**< XXCH extension data present and valid */
    bool    xbr_present;    /**< XBR extension data present and valid */
    bool    x96_present;    /**< X96 extension data present and valid */
};

struct dcadec_exss_info {
    int nchannels;          /**< Number of audio channels encoded among all
                                 extension sub-streams */
    int sample_rate;        /**< Maximum encoded audio sample rate in Hz */
    int bits_per_sample;    /**< Highest encoded PCM resolution in bits */
    int profile;            /**< Type of DTS profile encoded */
    bool embedded_stereo;   /**< 2.0 downmix has been embedded into the stream */
    bool embedded_6ch;      /**< 5.1 downmix has been embedded into the stream */
};

/**
 * Parse DTS packet. Packet data must be already converted into 16-bit
 * big-endian format. Caller must have already established byte stream
 * synchronization. Packet must start with a valid 32-bit sync word. EXSS frame
 * must be aligned on 4-byte boundary if present in the packet.
 *
 * @param dca   Pointer to decoder context.
 *
 * @param data  Pointer to packet data buffer. Buffer must be aligned on 4-byte
 *              boundary and padded at the end with DCADEC_BUFFER_PADDING bytes.
 *
 * @param size  Size in bytes of packet data. Size should not include padding.
 *
 * @return      0 on success, negative error code on failure.
 */
DCADEC_API int dcadec_context_parse(struct dcadec_context *dca, uint8_t *data, size_t size);

/**
 * Get information about DTS core payload of the parsed packet.
 *
 * @param dca   Pointer to decoder context.
 *
 * @return      Pointer to DTS core information structure on success,
 *              NULL on failure. Returned data should be freed with
 *              dcadec_context_free_core_info() function.
 */
DCADEC_API struct dcadec_core_info *dcadec_context_get_core_info(struct dcadec_context *dca);

/**
 * Free DTS core information structure.
 *
 * @param info  Pointer to DTS core information structure.
 */
DCADEC_API void dcadec_context_free_core_info(struct dcadec_core_info *info);

/**
 * Get information about extension sub-stream (EXSS) payload of the parsed
 * packet. When no EXSS is present but backward compatible DTS core sub-stream
 * contains extended audio, then information about extended audio in core
 * sub-stream is returned.
 *
 * @param dca   Pointer to decoder context.
 *
 * @return      Pointer to EXSS information structure on success,
 *              NULL on failure. Returned data should be freed with
 *              dcadec_context_free_exss_info() function.
 */
DCADEC_API struct dcadec_exss_info *dcadec_context_get_exss_info(struct dcadec_context *dca);

/**
 * Free EXSS information structure.
 *
 * @param info  Pointer to EXSS information structure.
 */
DCADEC_API void dcadec_context_free_exss_info(struct dcadec_exss_info *info);

/**
 * Filter the parsed packet and return per-channel PCM data. All parameters
 * except decoder context are optional and can be NULL. This function should be
 * called at least once after each successfull call to dcadec_context_parse().
 * Multiple calls per packet are allowed and return the same data.
 *
 * @param dca       Pointer to decoder context.
 *
 * @param samples   Filled with address of array of pointers to planes
 *                  containing PCM data for active channels. This data is only
 *                  valid until the next call to any libdcadec function.
 *                  Returned array is tightly packed, there are no gaps for
 *                  missing channels. Use channel_mask to determine total number
 *                  of channels and size of returned array. By default channels
 *                  are ordered according to WAVEFORMATEXTENSIBLE specification,
 *                  but if DCADEC_FLAG_NATIVE_LAYOUT flag was set when creating
 *                  decoder context, returned channels are in native DTS order.
 *
 * @param nsamples  Filled with number of PCM samples in each returned plane.
 *
 * @param channel_mask  Filled with bit mask indicating active channels. 1 at
 *                      the given bit position (counting from the least
 *                      significant bit) means that the channel is present in
 *                      the array of pointers to planes, 0 otherwise. Number of
 *                      bits set to 1 indicates the total number of planes
 *                      returned.
 *
 * @param sample_rate       Filled with decoded audio sample rate in Hz.
 *
 * @param bits_per_sample   Filled with decoded audio PCM resolution in bits.
 *
 * @param profile           Filled with type of DTS profile actually decoded.
 *                          This can be different from encoded profile since
 *                          certain extensions may be not decoded.
 *
 * @return                  0 on success, negative error code on failure.
 */
DCADEC_API int dcadec_context_filter(struct dcadec_context *dca, int ***samples,
                                     int *nsamples, int *channel_mask,
                                     int *sample_rate, int *bits_per_sample,
                                     int *profile);

/**
 * Clear all inter-frame history of the decoder. Call this before parsing
 * packets out of sequence, e.g. after seeking to the arbitrary position within
 * the DTS stream.
 *
 * @param dca   Pointer to decoder context.
 */
DCADEC_API void dcadec_context_clear(struct dcadec_context *dca);

/**
 * Create DTS decoder context.
 *
 * @param flags Any number of DCADEC_FLAG_* constants OR'ed together.
 *
 * @return      Pointer to decoder context on success, NULL on failure.
 */
DCADEC_API struct dcadec_context *dcadec_context_create(int flags);

/**
 * Destroy DTS decoder context.
 *
 * @param dca   Pointer to decoder context.
 */
DCADEC_API void dcadec_context_destroy(struct dcadec_context *dca);

/**
 * Convert negative libdcadec error code into string.
 *
 * @param errnum    Error code returned by libdcadec function.
 *
 * @return          Constant string describing error code.
 */
DCADEC_API const char *dcadec_strerror(int errnum);

#endif
