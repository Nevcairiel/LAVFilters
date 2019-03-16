/*
 *      Copyright (C) 2010-2019 Hendrik Leppkes
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
#include "ILAVPinInfo.h"

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
  LAVPixFmt_P016,        ///< YUV 4:2:0, 10 to 16-bit, U/V interleaved, MSB aligned

  /* RGB */
  LAVPixFmt_RGB24,       ///< RGB24, in BGR order
  LAVPixFmt_RGB32,       ///< RGB32, in BGRA order (A is invalid and should be 0xFF)
  LAVPixFmt_ARGB32,      ///< ARGB32, in BGRA order
  LAVPixFmt_RGB48,       ///< RGB48, in RGB order (16-bit per pixel)

  LAVPixFmt_DXVA2,       ///< DXVA2 Surface
  LAVPixFmt_D3D11,       ///< D3D11 Surface

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
AVPixelFormat getFFPixelFormatFromLAV(LAVPixelFormat pixFmt, int bpp);

typedef struct LAVDirectBuffer {
  BYTE *data[4];                    ///< pointer to the picture planes
  ptrdiff_t stride[4];              ///< stride of the planes (in bytes)
} LAVDirectBuffer;

typedef struct LAVFrameSideData {
  GUID guidType;                    ///< type of the side data
  BYTE *data;                       ///< side data
  size_t size;                      ///< size
} LAVFrameSideData;

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
  BYTE *stereo[4];                  ///< pointer to the second view picture planes
  ptrdiff_t stride[4];              ///< stride of the planes (in bytes)

  LAVPixelFormat format;            ///< pixel format of the frame
  int bpp;                          ///< bits per pixel, only meaningful for YUV420bX, YUV422bX or YUV444bX

  REFERENCE_TIME rtStart;           ///< start time of the frame. unset if AV_NOPTS_VALUE
  REFERENCE_TIME rtStop;            ///< stop time of the frame. unset if AV_NOPTS_VALUE
  int repeat;                       ///< repeat frame flag, signals how much the frame should be delayed

  AVRational aspect_ratio;          ///< display aspect ratio (unset/invalid if either num or den is 0)
  REFERENCE_TIME avgFrameDuration;  ///< frame duration used for the media type to indicate fps (AV_NOPTS_VALUE or 0 = use from input pin)

  DXVA2_ExtendedFormat ext_format;  ///< extended format flags (critical uses: indicate progressive/interlaced, indicate limited/full range)
  int key_frame;                    ///< frame is a key frame (field is not mandatory)
  int interlaced;                   ///< frame is interlaced
  int tff;                          ///< top field is first
  char frame_type;                  ///< frame type char (I/P/B/?)

  int flags;                        ///< frame flags
#define LAV_FRAME_FLAG_BUFFER_MODIFY        0x00000001
#define LAV_FRAME_FLAG_END_OF_SEQUENCE      0x00000002
#define LAV_FRAME_FLAG_FLUSH                0x00000004
#define LAV_FRAME_FLAG_REDRAW               0x00000008
#define LAV_FRAME_FLAG_DXVA_NOADDREF        0x00000010
#define LAV_FRAME_FLAG_MVC                  0x00000020

  LAVFrameSideData *side_data;
  int side_data_count;

  /* destruct function to free any buffers being held by this frame (may be null) */
  void  (*destruct)(struct LAVFrame *);
  void *priv_data;                  ///< private data from the decoder (mostly for destruct)

  bool direct;
  bool (*direct_lock)(struct LAVFrame *, struct LAVDirectBuffer *);
  void (*direct_unlock)(struct LAVFrame *);
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
HRESULT AllocLAVFrameBuffers(LAVFrame *pFrame, ptrdiff_t stride = 0);

/**
 * Destruct a LAV Frame, freeing its data pointers
 */
HRESULT FreeLAVFrameBuffers(LAVFrame *pFrame);

/**
 * Copy a LAV Frame, including a memcpy of the data
 */
HRESULT CopyLAVFrame(LAVFrame *pSrc, LAVFrame **ppDst);

/**
 * Copy the buffers in the LAV Frame, calling destruct on the old buffers.
 *
 * Usually useful to release decoder-specific buffers, and move to memory buffers
 */
HRESULT CopyLAVFrameInPlace(LAVFrame *pFrame);

/**
 * Add Side Data to the frame and return a pointer to it
 */
BYTE * AddLAVFrameSideData(LAVFrame *pFrame, GUID guidType, size_t size);

/**
 * Get a side data entry from the frame by its type
 */
BYTE * GetLAVFrameSideData(LAVFrame *pFrame, GUID guidType, size_t *pSize);

typedef struct LAVPinInfo
{
  DWORD flags;              ///< Flags that describe the video content (see ILAVPinInfo.h for valid values)
  AVPixelFormat pix_fmt;    ///< The pixel format used
  int has_b_frames;
} LAVPinInfo;

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
   * Get Decode Flags
   *
   * @return flags
   */
  STDMETHOD_(DWORD, GetDecodeFlags)() PURE;
#define LAV_VIDEO_DEC_FLAG_STREAMAR_BLACKLIST     0x00000001
#define LAV_VIDEO_DEC_FLAG_ONLY_DTS               0x00000002
#define LAV_VIDEO_DEC_FLAG_LAVSPLITTER            0x00000008
#define LAV_VIDEO_DEC_FLAG_DVD                    0x00000010
#define LAV_VIDEO_DEC_FLAG_NO_MT                  0x00000020
#define LAV_VIDEO_DEC_FLAG_SAGE_HACK              0x00000040
#define LAV_VIDEO_DEC_FLAG_LIVE                   0x00000080

  /**
   * Get the input media type
   *
   * @result media type
   */
  STDMETHOD_(CMediaType&, GetInputMediaType)() PURE;

  /**
   * Query the LAVPinInfo
   */
  STDMETHOD(GetLAVPinInfo)(LAVPinInfo &info) PURE;

  /**
   * Get a reference to the output pin
   */
  STDMETHOD_(CBasePin*, GetOutputPin)() PURE;

  /**
   * Get the output media type
   *
   * @result media type
   */
  STDMETHOD_(CMediaType&, GetOutputMediaType)() PURE;

  /**
   * Strip the packet for DVD decoding
   */
  STDMETHOD(DVDStripPacket)(BYTE*& p, long& len) PURE;

  /**
   * Get a Dummy frame used for flusing
   */
  STDMETHOD_(LAVFrame*,GetFlushFrame)() PURE;

  /**
   * Ask the decoder to release all DXVA resources
   */
  STDMETHOD(ReleaseAllDXVAResources)() PURE;

  /**
   * Get the index of the GPU device to be used for HW decoding, DWORD_MAX if not set
   */
  STDMETHOD_(DWORD, GetGPUDeviceIndex)() PURE;

  /**
   * Check if the input is using a dynamic allocator
   */
  STDMETHOD_(BOOL, HasDynamicInputAllocator)() PURE;

  /**
  * Set the x264 build info
  */
  STDMETHOD(SetX264Build)(int nBuild) PURE;

  /**
  * Get the x264 build info
  */
  STDMETHOD_(int, GetX264Build)() PURE;
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
   * Check if the decoder is functional
   */
  STDMETHOD(Check)() PURE;

  /**
   * Initialize the codec to decode a stream specified by codec and pmt.
   *
   * @param codec Codec Id
   * @param pmt DirectShow Media Type
   * @return S_OK on success, an error code otherwise
   */
  STDMETHOD(InitDecoder)(AVCodecID codec, const CMediaType *pmt) PURE;

  /**
   * Decode a frame.
   *
   * @param pSample Media Sample to decode
   * @return S_OK if decoding was successfull, S_FALSE if no frame could be extracted, an error code if the decoder is not compatible with the bitstream
   *
   * Note: When returning an actual error code, the filter will switch to the fallback software decoder! This should only be used for catastrophic failures,
   * like trying to decode a unsupported format on a hardware decoder.
   */
  STDMETHOD(Decode)(IMediaSample *pSample) PURE;

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

  /**
   * Query whether the format can potentially be interlaced.
   * This function should return false if the format can 100% not be interlaced, and true if it can be interlaced (but also progressive).
   */
  STDMETHOD_(BOOL, IsInterlaced)(BOOL bAllowGuess) PURE;

  /**
   * Allows the decoder to handle an allocator.
   * Used by DXVA2 decoding
   */
  STDMETHOD(InitAllocator)(IMemAllocator **ppAlloc) PURE;

  /**
   * Function called after connection is established, with the pin as argument
   */
  STDMETHOD(PostConnect)(IPin *pPin) PURE;

  /**
   * Notify the decoder the output connection was broken
   */
  STDMETHOD(BreakConnect)() PURE;

  /**
   * Get the number of sample buffers optimal for this decoder
   */
  STDMETHOD_(long, GetBufferCount)(long *pMaxBuffers = nullptr) PURE;

  /**
   * Get the name of the decoder
   */
  STDMETHOD_(const WCHAR*, GetDecoderName)() PURE;

  /**
   * Get whether the decoder outputs thread-safe buffers
   */
  STDMETHOD(HasThreadSafeBuffers)() PURE;

  /**
   * Toggle direct frame output mode for hardware decoders
   */
  STDMETHOD(SetDirectOutput)(BOOL bDirect) PURE;

  /**
   * Get the number of available hw accel devices
   */
  STDMETHOD_(DWORD, GetHWAccelNumDevices)() PURE;

  /**
   * Get information about a hwaccel device
   */
  STDMETHOD(GetHWAccelDeviceInfo)(DWORD dwIndex, BSTR *pstrDeviceName, DWORD *dwDeviceIdentifier) PURE;

  /**
   * Get the description of the currently active hwaccel device
   */
  STDMETHOD(GetHWAccelActiveDevice)(BSTR *pstrDeviceName) PURE;
};

/**
 * Decoder creation functions
 *
 * They are listed here so that including their header files is not required
 */
ILAVDecoder *CreateDecoderAVCodec();
ILAVDecoder *CreateDecoderWMV9MFT();
ILAVDecoder *CreateDecoderCUVID();
ILAVDecoder *CreateDecoderQuickSync();
ILAVDecoder *CreateDecoderDXVA2();
ILAVDecoder *CreateDecoderDXVA2Native();
ILAVDecoder *CreateDecoderD3D11();
ILAVDecoder *CreateDecoderMSDKMVC();

HRESULT VerifyD3D9Device(DWORD & dwIndex, DWORD dwDeviceId);
HRESULT VerifyD3D11Device(DWORD & dwIndex, DWORD dwDeviceId);
