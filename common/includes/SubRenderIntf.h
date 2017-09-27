// ***************************************************************
//  SubRenderIntf.h           version: 1.0.9  -  date: 2015-10-10
//  -------------------------------------------------------------
//  Copyright (C) 2011-2015, BSD license
// ***************************************************************

// 2015-10-10 1.0.9 added some optional information fields
// 2014-03-06 1.0.8 added ISubRenderConsumer2::Clear() interface/method
// 2014-03-05 1.0.7 auto-loading is now the provider's own responsibility
// 2013-07-01 1.0.6 added support for TV level subtitle transport
// 2013-06-05 1.0.5 added support for BT.2020 matrix
// 2012-12-04 1.0.4 changed auto-loading to supports multiple sub renderers
// 2012-10-15 1.0.3 (1) added some comments about interface management
//                  (2) added "context" parameter to "Request/DeliverFrame"
// 2012-09-11 1.0.2 (1) modified "yuvMatrix" fields to match Aegisub ("None")
//                  (2) added a clarification about LPWSTR property case
// 2012-08-30 1.0.1 (1) added mandatory consumer field "arAdjustedVideoSize"
//                  (2) added "subtitle color correction" info text
//                  (3) added mandatory provider field "yuvMatrix"
//                  (4) modified optional consumer field "yuvMatrix"
// 2011-12-20 1.0.0 initial release

// ---------------------------------------------------------------------------
// introduction
// ---------------------------------------------------------------------------

// This header defines a set of interfaces which a subtitle renderer can
// export to make its services available. The purpose of these interfaces is
// to allow a more flexible communication between subtitle renderer and
// subtitle consumer (e.g. video renderer) than DirectShow allows.

// A typical DirectShow subtitle renderer using these interfaces would have
// a subtitle input pin, but no video input pin and no output pins.

// ---------------------------------------------------------------------------
// basic interface principle
// ---------------------------------------------------------------------------

// (1) The subtitle consumer exports the interface "ISubRenderConsumer" and
//     waits for a subtitle renderer to connect.

// (2) The first time the graph is paused or started the subtitle renderer
//     searches through the DirectShow graph and looks for filters which
//     expose the "ISubRenderConsumer" interface.

// (3) If there are multiple subtitle consumers in the graph, the subtitle
//     renderer needs to decide which one to service. This should be done by
//     picking the consumer with the highest merit. There's a "GetMerit()"
//     method in the "ISubRenderConsumer" interface for this purpose.

// (4) Once the subtitle renderer has found a consumer it wants to service,
//     it calls "ISubRenderConsumer.Connect()", providing the consumer with
//     the "ISubRenderProvider" interface, which allows the consumer to
//     configure the subtitle renderer and to fetch rendered subtitles.

// ---------------------------------------------------------------------------
// subtitle color correction
// ---------------------------------------------------------------------------

// Today we have a number of different subtitle formats which define colors
// in different color spaces. E.g. SRT subtitles are usually considered to be
// native RGB, while DVD and Blu-Ray bitmap subtitles are native YCbCr.

// The most difficult to handle format is ASS. ASS subtitle colors are defined
// as RGB values. However, the historically most commonly used ASS renderer
// (VSFilter) and the most commonly used ASS authoring software (Aegisub) both
// render subtitles by drawing them directly onto the YCbCr video. Furthermore
// up until very recently Aegisub always used to convert all videos to RGB by
// using the BT.601 matrix. Practically that means that almost every ASS
// subtitle file out there which is color matched to the video, requires
// rendering in the same way, with hard coded BT.601 matrix, in order to ensure
// that subtitle and video colors match exactly.

// The subtitle interface defined in this header file passes subtitles from
// the subtitle provider to the consumer in RGB format. Since some subtitle
// formats are native YCbCr and due to the ASS color problem mentioned above,
// it may be necessary to perform color correction to guarantee that the
// rendered subtitle colors match perfectly to the video colors.

// When rendering native YCbCr subtitles (DVD and Blu-Ray bitmap subtitles),
// the subtitle renderer should try to use the correct YCbCr -> RGB matrix.
// The subtitle renderer may either use the "yuvMatrix" information field
// which the subtitle consumer may optionally provide. Or alternatively the
// subtitle renderer can auto guess the correct matrix based on subtitle
// resolution. The subtitle renderer informs the consumer about the matrix
// that was used for subtitle YCbCr -> RGB conversion by setting the
// "yuvMatrix" information field.

// When rendering native RGB subtitles (e.g. SRT), the subtitle renderer
// should render the subtitles untouched, and set the "yuvMatrix" information
// field to "None".

// When rendering ASS subtitles, the subtitle renderer should render the
// subtitles in untouched RGB. If the ASS subtitle file has a "yuvMatrix"
// header field, the subtitle renderer has to forward this field to the
// subtitle consumer. If the ASS subtitle file does not have a "yuvMatrix"
// header field, the subtitle renderer should set the "yuvMatrix" field to
// "TV.601".

// Subtitle consumers should read the subtitle renderer's "yuvMatrix" field.
// If it is set to anything other than "None", the subtitle consumer should
// double check whether the subtitle matrix matches the video matrix. If the
// matrixes differ, the consumer should perform the necessary color correction
// to make sure that subtitle and video colors match perfectly.

// ---------------------------------------------------------------------------
// subtitle levels
// ---------------------------------------------------------------------------

// Subtitles are by default transported as premultiplied RGBA pixels with
// PC levels (black = 0; white = 255). All consumers and providers are
// required to support this format.

// Optionally, the consumer can signal (see "supportedLevels" info field)
// that it supports TV levels, too. If it does (and only then), the provider
// can optionally output subtitles in TV levels. If it does, it has to set
// the info field "outputLevels" to "TV".

// ---------------------------------------------------------------------------
// subtitle repositioning
// ---------------------------------------------------------------------------

// In certain situations a user might have specific wishes for subtitle
// positioning. E.g. a 4:3 TV owner may want to have subtitles rendered into
// the big black bars on his TV, when watching 16:9 content. Or a CIH front
// projection owner may want to have subtitles rendered into the active video
// area instead of the black bars of the Cinemascope movie.

// Now some subtitle sources (e.g. many fan made ASS subtitles for Anime) are
// designed to be positioned very exactly. Other subtitle formats (e.g. SRT)
// don't care about positioning at all. The consumer doesn't really know
// which formats can be repostioned and which can't. So this interface allows
// the consumer to express a wish for subtitle positioning. The subtitle
// renderer can then decide whether it can (and wants to) honor this wish.

// If the consumer wants subtitles to be moved, he sets "subtitleTargetRect"
// to a different value (can be bigger or smaller) than "videoOutputRect".

// Example 1: anamorphic 16:9 DVD playback, 4:3 computer monitor
// consumer sets:
// - displayModeSize,     1024x768
// - originalVideoSize,   720x480
// - arAdjustedVideoSize, 853x480
// - videoOutputRect,     0x96x1024x672
// - subtitleTargetRect,  0x0x1024x768                   <-  move request
// subtitle renderer provides:
// - ISubRender.GetVideoOutputRect() -> 0x0x720x480
// - ISubRender.GetClipRect()        -> 0x-80x720x560    <-  request granted

// Example 2: anamorphic 16:9 DVD playback, CIH front projection
// consumer sets:
// - displayModeSize,     1920x1080
// - originalVideoSize,   720x480
// - arAdjustedVideoSize, 853x480
// - videoOutputRect,     0x0x1920x1080
// - subtitleTargetRect,  0x140x1920x940                 <-  move request
// subtitle renderer provides:
// - ISubRender.GetVideoOutputRect() -> 0x0x720x480
// - ISubRender.GetClipRect()        -> 0x62x720x418     <-  request granted

// Example 3: reencoded Anime, ASS subtitles, CIH front projection
// consumer sets:
// - displayModeSize,     1920x1080
// - originalVideoSize,   853x480
// - arAdjustedVideoSize, 853x480
// - videoOutputRect,     0x0x1920x1080
// - subtitleTargetRect,  0x140x1920x940                 <-  move request
// subtitle renderer provides:
// - ISubRender.GetVideoOutputRect() -> 0x0x1920x1080
// - ISubRender.GetClipRect()        -> 0x0x1920x1080    <-  request denied

// ---------------------------------------------------------------------------

#ifndef __SubtitleInterface__
#define __SubtitleInterface__

interface ISubRenderConsumer;
interface ISubRenderProvider;
interface ISubRenderFrame;

// ---------------------------------------------------------------------------
// ISubRenderOptions
// ---------------------------------------------------------------------------

// Base interface for both ISubRenderConsumer and ISubRenderProvider.

//[uuid("7CFD3728-235E-4430-9A2D-9F25F426BD70")]
//interface ISubRenderOptions : public IUnknown
DECLARE_INTERFACE_IID_(ISubRenderOptions, IUnknown, "7CFD3728-235E-4430-9A2D-9F25F426BD70")
{
  // Allows one party to get information from the other party.
  // The memory for strings and binary data is allocated by the callee
  // by using LocalAlloc. It is the caller's responsibility to release the
  // memory by calling LocalFree.
  // Field names and LPWSTR values should be read case insensitive.
  STDMETHOD(GetBool     )(LPCSTR field, bool      *value) = 0;
  STDMETHOD(GetInt      )(LPCSTR field, int       *value) = 0;
  STDMETHOD(GetSize     )(LPCSTR field, SIZE      *value) = 0;
  STDMETHOD(GetRect     )(LPCSTR field, RECT      *value) = 0;
  STDMETHOD(GetUlonglong)(LPCSTR field, ULONGLONG *value) = 0;
  STDMETHOD(GetDouble   )(LPCSTR field, double    *value) = 0;
  STDMETHOD(GetString   )(LPCSTR field, LPWSTR    *value, int *chars) = 0;
  STDMETHOD(GetBin      )(LPCSTR field, LPVOID    *value, int *size ) = 0;

  // Allows one party to configure or send information to the other party.
  // The callee should copy the strings/binary data, if needed.
  // Field names and LPWSTR values should be set with the exact case listed
  // in this header (just to be safe).
  STDMETHOD(SetBool     )(LPCSTR field, bool      value) = 0;
  STDMETHOD(SetInt      )(LPCSTR field, int       value) = 0;
  STDMETHOD(SetSize     )(LPCSTR field, SIZE      value) = 0;
  STDMETHOD(SetRect     )(LPCSTR field, RECT      value) = 0;
  STDMETHOD(SetUlonglong)(LPCSTR field, ULONGLONG value) = 0;
  STDMETHOD(SetDouble   )(LPCSTR field, double    value) = 0;
  STDMETHOD(SetString   )(LPCSTR field, LPWSTR    value, int chars) = 0;
  STDMETHOD(SetBin      )(LPCSTR field, LPVOID    value, int size ) = 0;

  // "field" must be zero terminated

  // mandatory fields for consumers:
  // "name",                 LPWSTR,    info,   read only,  get name / description of the consumer
  // "version",              LPWSTR,    info,   read only,  get version number of the consumer
  // "originalVideoSize",    SIZE,      info,   read only,  size of the video before scaling and AR adjustments
  // "arAdjustedVideoSize",  SIZE,      info,   read only,  size of the video after AR adjustments
  // "videoOutputRect",      RECT,      info,   read only,  final pos/size of the video after all scaling operations
  // "subtitleTargetRect",   RECT,      info,   read only,  consumer wish for where to place the subtitles
  // "frameRate",            ULONGLONG, info,   read only,  frame rate of the video after deinterlacing (REFERENCE_TIME)
  // "refreshRate",          double,    info,   read only,  display refresh rate (0, if unknown)

  // mandatory fields for providers:
  // "name",                 LPWSTR,    info,   read only,  get name / description of the provider
  // "version",              LPWSTR,    info,   read only,  get version number of the provider
  // "yuvMatrix",            LPWSTR,    info,   read only,  RGB Subtitles: "None" (fullrange); YCbCr Subtitles: "Levels.Matrix", Levels: TV|PC, Matrix: 601|709|240M|FCC|2020
  // "combineBitmaps",       bool,      option, write/read, must the provider combine all bitmaps into one? (default: false)

  // optional fields for consumers:
  // "videoCropRect",        RECT,      info,   read only,  crops "originalVideoSize" down, e.g. because of detected black bars
  // "croppedVideoOutputRect", RECT,    info,   read only,  final pos/size of the "videoCropRect", after all scaling operations
  // "fullscreenRect",       RECT,      info,   read only,  for fullscreen drawing, this is the rect you want to stay in (left/top can be non-zero!)
  // "displayModeSize",      SIZE,      info,   read only,  display mode width/height
  // "yuvMatrix",            LPWSTR,    info,   read only,  RGB Video: "None" (fullrange); YCbCr Video: "Levels.Matrix", Levels: TV|PC, Matrix: 601|709|240M|FCC|2020
  // "supportedLevels",      int,       info,   read only,  0: PC only (default); 1: PC+TV, no preference; 2: PC+TV, PC preferred; 3: PC+TV, TV preferred

  // optional fields for providers:
  // "outputLevels",         LPWSTR,    info,   read only,  are subtitles rendered/output in RGB "PC" (default) or "TV" levels?
  // "isBitmap",             bool,      info,   read only,  are the subtitles bitmap based or text based?
  // "isMovable",            bool,      info,   read only,  can the subtitles be repositioned safely?
};

// ---------------------------------------------------------------------------
// ISubRenderConsumer
// ---------------------------------------------------------------------------

// This interface is exposed by every subtitle consumer.

//[uuid("9DF90966-FE9F-4F0E-881E-DAF8A572D900")]
//interface ISubRenderConsumer : public ISubRenderOptions
DECLARE_INTERFACE_IID_(ISubRenderConsumer, ISubRenderOptions, "9DF90966-FE9F-4F0E-881E-DAF8A572D900")
{
  // Called by the subtitle renderer to ask the merit of the consumer.
  // Recommended merits:
  // - Subtitle Manager     0x00080000
  // - Video Renderer       0x00040000
  // - Video Post Processor 0x00020000
  // - Video Decoder        0x00010000
  STDMETHOD(GetMerit)(ULONG *merit) = 0;

  // Called by the subtitle renderer to init the provider <-> consumer
  // connection. The subtitle renderer provides an "ISubRenderProvider"
  // interface for the consumer to store and use. The consumer should
  // call "AddRef()" to make sure that the interface instance stays alive
  // as long as needed.
  STDMETHOD(Connect)(ISubRenderProvider *subtitleRenderer) = 0;

  // Called by the subtitle renderer to close the connection. The
  // consumer should react by immediately "Release()"ing the stored
  // "ISubRenderProvider" instance.
  STDMETHOD(Disconnect)(void) = 0;

  // Called by the subtitle renderer to deliver a rendered subtitle frame
  // to the consumer. The renderer may only deliver frames which were
  // requested before by the consumer.
  // The frames will be delivered in the same order as they were requested.
  // The deliverance can occur in different threads than the request, though.
  // The subtitle renderer can deliver a "NULL" subtitle frame to indicate
  // that the specified frame has no visible subtitles. The subtitle renderer
  // can also reuse the same "ISubRenderFrame" instance for multiple video
  // frames, if the subtitles didn't change.
  // The consumer should "AddRef()" the "ISubRenderFrame", if the consumer
  // wants to use it after returning from "DeliverFrame()". If the consumer
  // does that, it also needs to call "Release()" later when the
  // "ISubRenderFrame" instance is no longer needed.
  // The subtitle renderer should not require the "ISubRenderFrame" instance
  // to be released immediately. The consumer may store it for buffering/queue
  // purposes. All properties of the "ISubRenderFrame" instance must return
  // the correct results until it is released by the consumer.
  STDMETHOD(DeliverFrame)(REFERENCE_TIME start, REFERENCE_TIME stop, LPVOID context, ISubRenderFrame *subtitleFrame) = 0;
};

//[uuid("1A1737C8-2BF8-4BEA-97EA-3AB4FA8F7AC9")]
//interface ISubRenderConsumer2 : public ISubRenderConsumer
DECLARE_INTERFACE_IID_(ISubRenderConsumer2, ISubRenderConsumer, "1A1737C8-2BF8-4BEA-97EA-3AB4FA8F7AC9")
{
  // Called by the subtitle renderer e.g. when the user switches to a
  // different subtitle track. The consumer should immediately release
  // all stored subtitle frames and request them anew from the subtitle
  // renderer.
  STDMETHOD(Clear)(REFERENCE_TIME clearNewerThan = 0) = 0;
};

// ---------------------------------------------------------------------------
// ISubRenderProvider
// ---------------------------------------------------------------------------

// The subtitle renderer provides the consumer with this interface, when
// calling the "ISubRenderConsumer.Connect()" method.

//[uuid("20752113-C883-455A-BA7B-ABA4E9115CA8")]
//interface ISubRenderProvider : public ISubRenderOptions
DECLARE_INTERFACE_IID_(ISubRenderProvider, ISubRenderOptions, "20752113-C883-455A-BA7B-ABA4E9115CA8")
{
  // Called by the consumer to request a rendered subtitle frame.
  // The subtitle renderer will deliver the frame when it is completed, by
  // calling "ISubRenderConsumer.DeliverFrame()".
  // The subtitle renderer must pass the "context" parameter to the
  // consumer when calling "DeliverFrame()".
  // Depending on the internal thread design of the subtitle renderer,
  // "RequestFrame()" can return at once, with delivery being performed
  // asynchronously in a different thread. Alternatively, "RequestFrame()"
  // may also block until the frame was delivered. The consumer should not
  // depend on either threading model, but leave this decision to the
  // subtitle renderer.
  STDMETHOD(RequestFrame)(REFERENCE_TIME start, REFERENCE_TIME stop, LPVOID context) = 0;

  // Called by the consumer to close the connection. The subtitle renderer
  // should react by immediately "Release()"ing any stored
  // "ISubRenderConsumer" interface instances pointing to this specific
  // consumer.
  STDMETHOD(Disconnect)(void) = 0;
};

// ---------------------------------------------------------------------------
// ISubRenderFrame
// ---------------------------------------------------------------------------

// This interface is the reply to a consumer's frame render request.

//[uuid("81746AB5-9407-4B43-A014-1FAAC340F973")]
//interface ISubRenderFrame : public IUnknown
DECLARE_INTERFACE_IID_(ISubRenderFrame, IUnknown, "81746AB5-9407-4B43-A014-1FAAC340F973")
{
  // "GetOutputRect()" specifies for which video rect the subtitles were
  // rendered. If the subtitle renderer doesn't scale the subtitles at all,
  // which is the recommended method for bitmap (DVD/PGS) subtitles formats,
  // GetOutputRect() should return "0, 0, originalVideoSize". If the subtitle
  // renderer scales the subtitles, which is the recommend method for text
  // (SRT, ASS) subtitle formats, GetOutputRect() should aim to match the
  // consumer's "videoOutputRect". In any case, the consumer can look at
  // GetOutputRect() to see if (and how) the rendered subtitles need to be
  // scaled before blending them onto the video image.
  STDMETHOD(GetOutputRect)(RECT *outputRect) = 0;

  // "GetClipRect()" specifies how the consumer should clip the rendered
  // subtitles, before blending them onto the video image. Usually,
  // GetClipRect() should be identical to "GetVideoOutputRect()", unless the
  // subtitle renderer repositioned the subtitles (see the top of this header
  // for more information about repositioning).
  STDMETHOD(GetClipRect)(RECT *clipRect) = 0;

  // How many separate bitmaps does this subtitle frame consist of?
  // The subtitle renderer should combine small subtitle elements which are
  // positioned near to each other, in order to optimize performance.
  // Ideally, if there are e.g. two subtitle lines, one at the top and one
  // at the bottom of the frame, the subtitle renderer should output two
  // bitmaps per frame.
  STDMETHOD(GetBitmapCount)(int *count) = 0;

  // Returns the premultiplied RGBA pixel data for the specified bitmap.
  // The ID is guaranteed to change if the content of the bitmap has changed.
  // The ID can stay identical if only the position changes.
  // Reusing the same ID for unchanged bitmaps can improve performance.
  // Subtitle bitmaps may move in and out of the video frame rectangle, so
  // the position of the subtitle bitmaps can become negative. The consumer
  // is required to do proper clipping if the subtitle bitmap is partially
  // outside the video rectangle.
  // The memory pointed to by "pixels" is only valid until the next
  // "GetBitmap" call, or until the "ISubRenderFrame" instance is released.
  STDMETHOD(GetBitmap)(int index, ULONGLONG *id, POINT *position, SIZE *size, LPCVOID *pixels, int *pitch) = 0;
};

// ===========================================================================

#endif // __SubtitelInterface__
