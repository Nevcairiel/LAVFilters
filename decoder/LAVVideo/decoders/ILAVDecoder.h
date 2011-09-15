/*
 *      Copyright (C) 2011 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 */

#pragma once

/**
 * List of internally used pixel formats
 *
 * Note for high bit-depth formats:
 * All bX (9-16 bit) formats always use 2 bytes, are little-endian, and the values are right-aligned.
 * That means that there are leading zero-bits, and not trailing like in DirectShow
 * The actual number of valid bits is stored in the LAVFrame
 */
enum LAVPixelFormat {
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
};

/**
 * A Video Frame
 *
 * Allocated by the decoder and passed through the processing chain.
 *
 * The Decoder should allocate frames by using ILAVDecoderCallback::AllocateFrame()
 * Frames need to be free'd with ILAVDecoderCallback::ReleaseFrame()
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

  AVRational aspect_ratio;          ///< aspect ratio (unset/invalid if either num or den is 0)

  DXVA2_ExtendedFormat ext_format;  ///< extended format flags (critical uses: indicate progressive/interlaced, indicate limited/full range)
  int key_frame;                    ///< frame is a key frame (field is not mandatory)

  /* destruct function to free any buffers being held by this frame (may be null) */
  void  (*destruct)(struct LAVFrame *);
  void *priv_data;                  ///< private data from the decoder (mostly for destruct)
} LAVFrame;

/**
 * Decoder interface
 *
 * Every decoder needs to implement this to interface with the LAV Video core
 */
class ILAVDecoder
{
  /**
   * Initialize the codec to decode a stream specified by codec and pmt.
   *
   * @param codec Codec Id
   * @param pmt DirectShow Media Type
   * @return S_OK on success, an error code otherwise
   */
  STDMETHOD(InitDecoder)(CodecID codec, CMediaType *pmt) PURE;

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
};
