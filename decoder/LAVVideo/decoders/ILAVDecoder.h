/*
 *      Copyright (C) 2011 Hendrik Leppkes
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

#pragma once

#include "LAVVideoSettings.h"

/**
 * List of internally used pixel formats
 *
 * Note for high bit-depth formats:
 * All bX (9-16 bit) formats always use 2 bytes, are little-endian, and the values are right-aligned.
 * That means that there are leading zero-bits, and not trailing like in DirectShow
 * The actual number of valid bits is stored in the LAVFrame
 */
typedef enum LAVPixelFormat {
  LAVPixFmt_None = -1,
  /* planar YUV */
  LAVPixFmt_YUV420,      ///< YUV 4:2:0, 8 bit
  LAVPixFmt_YUV420bX,    ///< YUV 4:2:0, 9-16 bit

  LAVPixFmt_YUV422,      ///< YUV 4:2:2, 8 bit
  LAVPixFmt_YUV422bX,    ///< YUV 4:2:2, 9-16 bit

  LAVPixFmt_YUV444,      ///< YUV 4:4:4, 8 bit
  LAVPixFmt_YUV444bX,    ///< YUV 4:4:4, 9-16 bit

  /* packed/half-packed YUV */
  LAVPixFmt_NV12,        ///< YUV 4:2:0, U/V interleaved
  LAVPixFmt_YUY2,        ///< YUV 4:2:2, packed, YUYV order

  /* RGB */
  LAVPixFmt_RGB24,       ///< RGB24, in BGR order
  LAVPixFmt_RGB32,       ///< RGB32, in BGRA order (A is invalid and should be 0xFF)
  LAVPixFmt_ARGB32,      ///< ARGB32, in BGRA order

  LAVPixFmt_NB,          ///< number of formats
} LAVPixelFormat;

/**
 * Structure describing a pixel format
 */
typedef struct LAVPixFmtDesc {
  int codedbytes;        ///< coded byte per pixel in one plane (for packed and multibyte formats)
  int planes;            ///< number of planes
  int planeWidth[4];     ///< log2 width factor
  int planeHeight[4];    ///< log2 height factor
} LAVPixFmtDesc;

/**
 * Get the Pixel Format Descriptor for the given format
 */
LAVPixFmtDesc getPixelFormatDesc(LAVPixelFormat pixFmt);

/**
 * Map the LAV Pixel Format to a FFMpeg pixel format (for swscale, etc)
 */
PixelFormat getFFPixelFormatFromLAV(LAVPixelFormat pixFmt, int bpp);

/**
 * A Video Frame
 *
 * Allocated by the decoder and passed through the processing chain.
 *
 * The Decoder should allocate frames by using ILAVVideoCallback::AllocateFrame()
 * Frames need to be free'd with ILAVVideoCallback::ReleaseFrame()
 *
 * FIXME: Right now the avcodec decoder always reuses the buffers the next time its called!
 * NYI: Some Post-Processing filters will require to hang on to frames longer then the normal delivery process.
 * NYI: That means that the image buffers should not be re-used unless "destruct" released them.
 */
typedef struct LAVFrame {
  int width;                        ///< width of the frame (in pixel)
  int height;                       ///< height of the frame (in pixel)

  BYTE *data[4];                    ///< pointer to the picture planes
  int stride[4];                    ///< stride of the planes (in bytes)

  LAVPixelFormat format;            ///< pixel format of the frame
  int bpp;                          ///< bits per pixel, only meaningful for YUV420bX, YUV422bX or YUV444bX

  REFERENCE_TIME rtStart;           ///< start time of the frame. unset if AV_NOPTS_VALUE
  REFERENCE_TIME rtStop;            ///< stop time of the frame. unset if AV_NOPTS_VALUE
  int repeat;                       ///< repeat frame flag, signals how much the frame should be delayed

  AVRational aspect_ratio;          ///< display aspect ratio (unset/invalid if either num or den is 0)
  REFERENCE_TIME avgFrameDuration;  ///< frame duration used for the media type to indicate fps (AV_NOPTS_VALUE or 0 = use from input pin)

  DXVA2_ExtendedFormat ext_format;  ///< extended format flags (critical uses: indicate progressive/interlaced, indicate limited/full range)
  int key_frame;                    ///< frame is a key frame (field is not mandatory)

  /* destruct function to free any buffers being held by this frame (may be null) */
  void  (*destruct)(struct LAVFrame *);
  void *priv_data;                  ///< private data from the decoder (mostly for destruct)
} LAVFrame;

/**
 * Allocate buffers for the LAVFrame "data" element to fit the pixfmt with the given stride
 *
 * This method also fills the stride argument in the LAVFrame properly.
 * Its required that width/height and format are already set on the frame.
 *
 * @param pFrame Frame to fill
 * @param stride stride to use (in pixel). If 0, a stride will be computed to fill usual alignment rules
 * @return HRESULT
 */
HRESULT AllocLAVFrameBuffers(LAVFrame *pFrame, int stride = 0);

/**
 * Interface into the LAV Video core for the decoder implementations
 * This interface offers all required functions to properly communicate with the core
 */
interface ILAVVideoCallback
{
  /**
   * Allocate and initialize a new frame
   *
   * @param ppFrame variable to receive the frame
   * @return HRESULT
   */
  STDMETHOD(AllocateFrame)(LAVFrame **ppFrame) PURE;

  /**
   * Destruct and release frame
   * This function calls the "destruct" function on the frame, and releases it afterwards
   *
   * @param ppFrame variable of the frame, will be nulled
   * @return HRESULT
   */
  STDMETHOD(ReleaseFrame)(LAVFrame **ppFrame) PURE;

  /**
   * Deliver the frame
   * The decoder gives up control of the frame at this point, and hands it over to the processing chain
   *
   * @param pFrame frame to deliver
   * @return HRESULT
   */
  STDMETHOD(Deliver)(LAVFrame *pFrame) PURE;

  /**
   * Get the extension of the currently loaded file
   *
   * @result WCHAR string to the extension. Callers needs to free the memory with CoTaskMemFree
   */
  STDMETHOD_(LPWSTR, GetFileExtension)() PURE;

  /**
   * Check wether a filter matching the clsid is in the graph
   *
   * @result TRUE if the filter was found, false otherwise
   */
  STDMETHOD_(BOOL, FilterInGraph)(const GUID &clsid) PURE;

  /**
   * Check wether VC-1 timestamps are DTS instead of PTS
   *
   * @result TRUE if DTS, FALSE if PTS
   */
  STDMETHOD_(BOOL, VC1IsDTS)() PURE;
};

/**
 * Decoder interface
 *
 * Every decoder needs to implement this to interface with the LAV Video core
 */
interface ILAVDecoder
{
  /**
   * Virtual destructor
   */
  virtual ~ILAVDecoder(void) {};

  /**
   * Initialize interfaces with the LAV Video core
   * This function should also be used to create all interfaces with external DLLs
   *
   * @param pSettings reference to the settings interface
   * @param pCallback reference to the callback interface
   * @return S_OK on success, error code if this decoder is lacking an external support dll
   */
  STDMETHOD(InitInterfaces)(ILAVVideoSettings *pSettings, ILAVVideoCallback *pCallback) PURE;

  /**
   * Initialize the codec to decode a stream specified by codec and pmt.
   *
   * @param codec Codec Id
   * @param pmt DirectShow Media Type
   * @return S_OK on success, an error code otherwise
   */
  STDMETHOD(InitDecoder)(CodecID codec, const CMediaType *pmt) PURE;

  /**
   * Decode a frame.
   *
   * @param buffer the input buffer containing the data to be decoded
   * @param buflen length of the buffer, in bytes
   * @param rtStart start time as delivered in the DirectShow input sample
   * @param rtStop stop time as delivered in the DirectShow input sample
   * @param bSyncPoint TRUE if the input sample indicated a sync point
   * @param bDiscontinuity TRUE if the input sample indicated a discontinuity
   * @return S_OK if decoding was successfull, S_FALSE if no frame could be extracted, an error code if the decoder is not compatible with the bitstream
   *
   * Note: When returning an actual error code, the filter will switch to the fallback software decoder! This should only be used for catastrophic failures,
   * like trying to decode a unsupported format on a hardware decoder.
   */
  STDMETHOD(Decode)(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity) PURE;

  /**
   * Flush the decoder after a seek.
   * The decoder should discard any remaining data.
   *
   * @return unused
   */
  STDMETHOD(Flush)() PURE;

  /**
   * End of Stream
   * The decoder is asked to output any buffered frames for immediate delivery
   *
   * @return unused
   */
  STDMETHOD(EndOfStream)() PURE;

  /**
   * Query the decoder for the current pixel format
   * Mostly used by the media type creation logic before playback starts
   *
   * @return the pixel format used in the decoding process
   */
  STDMETHOD(GetPixelFormat)(LAVPixelFormat *pPix, int *pBpp) PURE;

  /**
   * Get the frame duration.
   *
   * This function is not mandatory, and if you cannot provide any specific duration, return 0.
   */
  STDMETHOD_(REFERENCE_TIME, GetFrameDuration)() PURE;
};

/**
 * Decoder creation functions
 *
 * They are listed here so that including their header files is not required
 */
ILAVDecoder *CreateDecoderAVCodec();
ILAVDecoder *CreateDecoderCUVID();
