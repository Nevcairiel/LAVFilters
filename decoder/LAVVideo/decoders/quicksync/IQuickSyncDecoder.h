/*
 * Copyright (c) 2011, INTEL CORPORATION
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 * Neither the name of INTEL CORPORATION nor the names of its contributors may
 * be used to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#define QS_DEC_DLL_NAME "IntelQuickSyncDecoder.dll"
#define QS_DEC_VERSION  "v0.27 Beta"

// Forward declarations
struct IDirect3DDeviceManager9;

// Return value of the check function.
// Caps are bitwise OR of the following values
enum QsCaps
{
    QS_CAP_UNSUPPORTED      = 0,
    QS_CAP_HW_ACCELERATION  = 1,
    QS_CAP_SW_EMULATION     = 2
};

// This struct holds an output frame + meta data
struct QsFrameData
{
    QsFrameData() { memset(this, 0, sizeof(QsFrameData)); }

    enum QsFrameType
    {
        Invalid = 0,
        I       = 1,
        P       = 2,
        B       = 3
    };

    // QsFrameStructure affects how color information is stored within a frame
    // 4:2:0 example:
    // fsProgressiveFrame: uv[0][0] affects y[0-1][0-1]
    // fsInterlacedFrame   uv[0][0] affects y[0][0-1] and y[2][0-1]
    enum QsFrameStructure
    {
        fsProgressiveFrame = 0,  // Note: a progressive frame can hold interlaced content
        fsInterlacedFrame  = 1,  // Two fields
        fsField            = 2   // Single field
    };

    // Pointers to data buffer
    // Memory should not be freed externally!
    // Packed UV surfaces (NV12) should use the 'u' pointer.
    union { unsigned char* y; unsigned char* red;   };
    union { unsigned char* u; unsigned char* green; };
    union { unsigned char* v; unsigned char* blue;  };
    union { unsigned char* a; unsigned char* alpha; };

    DWORD            fourCC;             // Standard fourCC codes. Limited to NV12 in this version.
    RECT             rcFull;             // Note: these RECTs are according to WIN32 API standard (not DirectShow)
    RECT             rcClip;             // They hold the coordinates of the top-left and bottom right pixels
                                         // So expect values like {0, 0, 1919, 1079} for 1080p.
    DWORD            dwStride;           // Line width + padding (in bytes). Distance between lines. Always modulu 16.
    REFERENCE_TIME   rtStart, rtStop;    // Start and stop time stamps. Ther latter is always rtStart+1
    DWORD            dwInterlaceFlags;   // Same as dwTypeSpecificFlags (in AM_SAMPLE2_PROPERTIES)
    bool             bFilm;              // true only when a frame has a double field attribute (AM_VIDEO_FLAG_REPEAT_FIELD)
    DWORD            dwPictAspectRatioX; // Display aspect ratio (NOT pixel aspect ratio)
    DWORD            dwPictAspectRatioY;
    QsFrameType      frameType;          // Mainly used for future capability. Will always return I.
    QsFrameStructure frameStructure;     // See QsFrameStructure enum comments
    bool             bReadOnly;          // If true, the frame's content can be overwritten (most likely bReadOnly will remain false forever)
    bool             bCorrupted;         // If true, the HW decoder reported corruption in this frame
};

// config for QuickSync component
struct CQsConfig
{
    CQsConfig()
    {
        memset(this, 0, sizeof(CQsConfig));
    }

    // misc
    union
    {
        unsigned misc;
        struct
        {
            unsigned nOutputQueueLength     :  6; // use a minimum of 8 frame for more accurate frame rate calculations
            bool     bMod16Width            :  1; // image width is always modulu 16
            bool     bEnableMultithreading  :  1; // enable worker threads for low latency decode (better performance, more power)
            bool     bTimeStampCorrection   :  1; // when true time stamp will be generated.
                                                  // when false -> DS filter will do this. implies disabled output queue (nOutputQueueLength=0)
            bool     bEnableMtCopy          :  1; // enables MT frame copy
            bool     bEnableMtDecode        :  1; // decode on a worker thread
            bool     bEnableMtProcessing    :  1; // perform post decode processing on another thread
            bool     bEnableVideoProcessing :  1;
            bool     bEnableSwEmulation     :  1; // When true, a SW version of the decoder will be used (if possible) if HW fails
            unsigned reserved1              : 19;
        };
    };

    // Codec support
    union
    {
        unsigned codecs;
        struct
        {
            bool  bEnableDvdDecoding :  1;
            bool  bEnableH264        :  1;
            bool  bEnableMPEG2       :  1;
            bool  bEnableVC1         :  1;
            bool  bEnableWMV9        :  1;
            unsigned reserved2       : 27;
        };
    };

    // Video post processing
    union
    {
        unsigned vpp;
        struct
        {
            bool  bEnableDeinterlacing :  1;
            bool  bEnableFilmDetection :  1;
            unsigned reserved3         : 30;
        };
    };
};

// Interafce to QuickSync component
struct IQuickSyncDecoder
{
    typedef HRESULT (*TQS_DeliverSurfaceCallback) (void* obj, QsFrameData* data);

    // useless constructor to keep several compilers happy...
    IQuickSyncDecoder() {}


    // Object is OK
    virtual bool getOK() = 0;
    
    // Test if the decoder supports the media type
    // Return codes are:
    // S_OK - support with HW acceleration.
    // S_FALSE - support with SW implementation
    // E_FAIL - no support
    // Other errors indicate bad parameters.
    virtual HRESULT TestMediaType(const AM_MEDIA_TYPE* mtIn, FOURCC fourCC) = 0;

    // Initialize decoder
    virtual HRESULT InitDecoder(const AM_MEDIA_TYPE* mtIn, FOURCC fourCC) = 0;

    // Decode a sample. Decoded frames are delivered via a callback.
    // Call SetDeliverSurfaceCallback to specify the callback
    virtual HRESULT Decode(IMediaSample* pIn) = 0;

    // Flush decoder
    // When deliverFrames == true, the frames will be delivered via a callback (same as Decode).
    virtual HRESULT Flush(bool deliverFrames = true) = 0;

    // OnSeek/NewSegment - marks the decoding of a new segment.
    // Resets the decoder discarding old data.
    virtual HRESULT OnSeek(REFERENCE_TIME segmentStart) = 0;

    // Marks the start of a flush. All calls to Decode will be ignored.
    // Usually called asynchroniously from the application thread.
    virtual HRESULT BeginFlush() = 0;
    
    // Marks the end of a flush. All calls to Decode after this function is called will be accepted.
    // Usually called asynchroniously from the application thread.
    // An implicit OnSeek call will be generated after the next Decode call.
    virtual HRESULT EndFlush() = 0;

    // Call this function to pass a D3D device manager to the decoder from the EVR
    // Must be used in full screen exlusive mode
    virtual void SetD3DDeviceManager(IDirect3DDeviceManager9* pDeviceManager) = 0;

    // Sets the callback funtion for a DeliverSurface event. This the only method for frame delivery.
    virtual void SetDeliverSurfaceCallback(void* obj, TQS_DeliverSurfaceCallback func) = 0;

    // Fills the pConfig struct with current config.
    // If called after construction will contain the defaults.
    virtual void GetConfig(CQsConfig* pConfig) = 0;

    // Call this function to modify the decoder config.
    // Must be called before calling the InitDecoder method.
    virtual void SetConfig(CQsConfig* pConfig) = 0;

protected:
    // Ban copying!
    IQuickSyncDecoder& operator=(const IQuickSyncDecoder&);
    IQuickSyncDecoder(const IQuickSyncDecoder&);
    // Ban destruction of interface
    ~IQuickSyncDecoder() {}
};

// exported functions
extern "C" 
{
    IQuickSyncDecoder* __stdcall createQuickSync();
    void               __stdcall destroyQuickSync(IQuickSyncDecoder*);
    void               __stdcall getVersion(char* ver, const char** license);
    DWORD              __stdcall check();
}
