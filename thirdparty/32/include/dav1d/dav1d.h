/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DAV1D_H
#define DAV1D_H

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stdarg.h>

#include "common.h"
#include "picture.h"
#include "data.h"
#include "version.h"

typedef struct Dav1dContext Dav1dContext;
typedef struct Dav1dRef Dav1dRef;

#define DAV1D_MAX_FRAME_THREADS 256
#define DAV1D_MAX_TILE_THREADS 64

typedef struct Dav1dLogger {
    void *cookie; ///< Custom data to pass to the callback.
    /**
     * Logger callback. May be NULL to disable logging.
     *
     * @param cookie Custom pointer passed to all calls.
     * @param format The vprintf compatible format string.
     * @param     ap List of arguments referenced by the format string.
     */
    void (*callback)(void *cookie, const char *format, va_list ap);
} Dav1dLogger;

typedef struct Dav1dSettings {
    int n_frame_threads;
    int n_tile_threads;
    int apply_grain;
    int operating_point; ///< select an operating point for scalable AV1 bitstreams (0 - 31)
    int all_layers; ///< output all spatial layers of a scalable AV1 biststream
    unsigned frame_size_limit; ///< maximum frame size, in pixels (0 = unlimited)
    uint8_t reserved[32]; ///< reserved for future use
    Dav1dPicAllocator allocator; ///< Picture allocator callback.
    Dav1dLogger logger; ///< Logger callback.
} Dav1dSettings;

/**
 * Get library version.
 */
DAV1D_API const char *dav1d_version(void);

/**
 * Initialize settings to default values.
 *
 * @param s Input settings context.
 */
DAV1D_API void dav1d_default_settings(Dav1dSettings *s);

/**
 * Allocate and open a decoder instance.
 *
 * @param c_out The decoder instance to open. *c_out will be set to the
 *              allocated context.
 * @param     s Input settings context.
 *
 * @note The context must be freed using dav1d_close() when decoding is
 *       finished.
 *
 * @return 0 on success, or < 0 (a negative DAV1D_ERR code) on error.
 */
DAV1D_API int dav1d_open(Dav1dContext **c_out, const Dav1dSettings *s);

/**
 * Parse a Sequence Header OBU from bitstream data.
 *
 * @param out Output Sequence Header.
 * @param buf The data to be parser.
 * @param sz  Size of the data.
 *
 * @return 0 on success, or < 0 (a negative DAV1D_ERR code) on error.
 *
 * @note It is safe to feed this function data containing other OBUs than a
 *       Sequence Header, as they will simply be ignored. If there is more than
 *       one Sequence Header OBU present, only the last will be returned.
 */
DAV1D_API int dav1d_parse_sequence_header(Dav1dSequenceHeader *out,
                                          const uint8_t *buf, const size_t sz);

/**
 * Feed bitstream data to the decoder.
 *
 * @param   c Input decoder instance.
 * @param  in Input bitstream data. On success, ownership of the reference is
 *            passed to the library.
 *
 * @return
 *         0: Success, and the data was consumed.
 *  DAV1D_ERR(EAGAIN): The data can't be consumed. dav1d_get_picture() should
 *                     be called to get one or more frames before the function
 *                     can consume new data.
 *  other negative DAV1D_ERR codes: Error during decoding or because of invalid
 *                                  passed-in arguments.
 */
DAV1D_API int dav1d_send_data(Dav1dContext *c, Dav1dData *in);

/**
 * Return a decoded picture.
 *
 * @param   c Input decoder instance.
 * @param out Output frame. The caller assumes ownership of the returned
 *            reference.
 *
 * @return
 *         0: Success, and a frame is returned.
 *  DAV1D_ERR(EAGAIN): Not enough data to output a frame. dav1d_send_data()
 *                     should be called with new input.
 *  other negative DAV1D_ERR codes: Error during decoding or because of invalid
 *                                  passed-in arguments.
 *
 * @note To drain buffered frames from the decoder (i.e. on end of stream),
 *       call this function until it returns DAV1D_ERR(EAGAIN).
 *
 * @code{.c}
 *  Dav1dData data = { 0 };
 *  Dav1dPicture p = { 0 };
 *  int res;
 *
 *  read_data(&data);
 *  do {
 *      res = dav1d_send_data(c, &data);
 *      // Keep going even if the function can't consume the current data
 *         packet. It eventually will after one or more frames have been
 *         returned in this loop.
 *      if (res < 0 && res != DAV1D_ERR(EAGAIN))
 *          free_and_abort();
 *      res = dav1d_get_picture(c, &p);
 *      if (res < 0) {
 *          if (res != DAV1D_ERR(EAGAIN))
 *              free_and_abort();
 *      } else
 *          output_and_unref_picture(&p);
 *  // Stay in the loop as long as there's data to consume.
 *  } while (data.sz || read_data(&data) == SUCCESS);
 *
 *  // Handle EOS by draining all buffered frames.
 *  do {
 *      res = dav1d_get_picture(c, &p);
 *      if (res < 0) {
 *          if (res != DAV1D_ERR(EAGAIN))
 *              free_and_abort();
 *      } else
 *          output_and_unref_picture(&p);
 *  } while (res == 0);
 * @endcode
 */
DAV1D_API int dav1d_get_picture(Dav1dContext *c, Dav1dPicture *out);

/**
 * Close a decoder instance and free all associated memory.
 *
 * @param c_out The decoder instance to close. *c_out will be set to NULL.
 */
DAV1D_API void dav1d_close(Dav1dContext **c_out);

/**
 * Flush all delayed frames in decoder and clear internal decoder state,
 * to be used when seeking.
 *
 * @param c Input decoder instance.
 *
 * @note Decoding will start only after a valid sequence header OBU is
 *       delivered to dav1d_send_data().
 *
 */
DAV1D_API void dav1d_flush(Dav1dContext *c);

# ifdef __cplusplus
}
# endif

#endif /* DAV1D_H */
