// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
// Avisynth v2.6.  Copyright 2006 Klaus Post.
// Avisynth v2.6.  Copyright 2009 Ian Brabham.
// Avisynth+ project
// 20160613: new 16 bit planar pixel_type constants go live!
// 20160725: pixel_type constants 10-12-14 bit + planar RGB + BRG48/64
// 20161005: Fallback of VideoInfo functions to defaults if no function exists
// 20170117: global variables for VfW output OPT_xxxx
// 20170310: new MT mode: MT_SPECIAL_MT
// 20171103: (test with SIZETMOD define: Videoframe offsets to size_t, may affect x64)
// 20171207: C++ Standard Conformance (no change for plugin writers)
// 20180525: AVS_UNUSED define to supress parameter not used warnings
// 2020xxxx: AVS_WINDOWS and AVS_POSIX option see avs/config.h
// 20200305: ScriptEnvironment::VSprintf parameter (void *) changed back to va_list
// 20200330: removed __stdcall from variadic argument functions (Sprintf)
//           (remove test SIZETMOD define for clarity)
//           Integrate Avisynth Neo structures and interface, PFunction, PDevice
// 20200501: frame property support (NewVideoFrameP and other helpers) to legacy IScriptEnvironment.
//           move some former IScriptEnvironment2 functions to IScriptEnvironment:
//           GetEnvProperty (system prop), Allocate, Free (buffer pool)
//           GetVarTry, GetVarBool/Int/String/Double/Long
//           Invoke2, Invoke3, InvokeTry, Invoke2Try, Invoke3Try
//           Interface Version to 8 (classic 2.6 = 6)
// 20200527  Add IScriptEnvironment_Avs25, used internally
// 20200607  AVS frame property enums to match existing Avisynth enum style

// http://www.avisynth.org

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making
// a combined work based on Avisynth.  Thus, the terms and conditions of
// the GNU General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate
// with Avisynth solely through the interfaces defined in avisynth.h,
// regardless of the license terms of these independent modules, and to
// copy and distribute the resulting combined work under terms of your
// choice, provided that every copy of the combined work is accompanied
// by a complete copy of the source code of Avisynth (the version of
// Avisynth used to produce the combined work), being distributed under
// the terms of the GNU General Public License plus this exception.  An
// independent module is a module which is not derived from or based on
// Avisynth, such as 3rd-party filters, import and export plugins, or
// graphical user interfaces.


#ifndef __AVISYNTH_8_H__
#define __AVISYNTH_8_H__

#include "avs/config.h"
#include "avs/capi.h"
#include "avs/types.h"

#ifdef AVS_POSIX
# include "avs/posix.h"
#endif

#if defined(AVS_POSIX)
#define __stdcall
#define __cdecl
#endif

// Important note on AVISYNTH_INTERFACE_VERSION V6->V8 change:
// Note 1: Those few plugins which were using earlier IScriptEnvironment2 despite the big Warning will crash have to be rebuilt.
// Note 2: How to support earlier avisynth interface with an up-to-date avisynth.h:
//         Use the new frame property features adaptively after querying that at least v8 is supported
//         AviSynth property support can be queried (cpp iface example):
//           has_at_least_v8 = true;
//           try { env->CheckVersion(8); } catch (const AvisynthError&) { has_at_least_v8 = false; }
//         and use it:
//           if (has_at_least_v8) dst = env->NewVideoFrameP(vi, &src); else dst = env->NewVideoFrame(vi);

enum {
  AVISYNTH_CLASSIC_INTERFACE_VERSION_25 = 3,
  AVISYNTH_CLASSIC_INTERFACE_VERSION_26BETA = 5,
  AVISYNTH_CLASSIC_INTERFACE_VERSION = 6,
  AVISYNTH_INTERFACE_VERSION = 8
};

/* Compiler-specific crap */

// Tell MSVC to stop precompiling here
#if defined(_MSC_VER) && !defined(__clang__)
  #pragma hdrstop
#endif

// Set up debugging macros for MS compilers; for others, step down to the
// standard <assert.h> interface
#ifdef _MSC_VER
  #include <crtdbg.h>
#else
  #undef _RPT0
  #undef _RPT1
  #undef _RPT2
  #undef _RPT3
  #undef _RPT4
  #undef _RPT5
  #define _RPT0(a,b) ((void)0)
  #define _RPT1(a,b,c) ((void)0)
  #define _RPT2(a,b,c,d) ((void)0)
  #define _RPT3(a,b,c,d,e) ((void)0)
  #define _RPT4(a,b,c,d,e,f) ((void)0)
  #define _RPT5(a,b,c,d,e,f,g) ((void)0)

  #include <cassert>
  #undef _ASSERTE
  #undef _ASSERT
  #define _ASSERTE(x) assert(x)
  #define _ASSERT(x) assert(x)
#endif



// I had problems with Premiere wanting 1-byte alignment for its structures,
// so I now set the Avisynth struct alignment explicitly here.
#pragma pack(push,8)

// The VideoInfo struct holds global information about a clip (i.e.
// information that does not depend on the frame number).  The GetVideoInfo
// method in IClip returns this struct.

enum {SAMPLE_INT8  = 1<<0,
      SAMPLE_INT16 = 1<<1,
      SAMPLE_INT24 = 1<<2,    // Int24 is a very stupid thing to code, but it's supported by some hardware.
      SAMPLE_INT32 = 1<<3,
      SAMPLE_FLOAT = 1<<4};

enum {
   PLANAR_Y=1<<0,
   PLANAR_U=1<<1,
   PLANAR_V=1<<2,
   PLANAR_ALIGNED=1<<3,
   PLANAR_Y_ALIGNED=PLANAR_Y|PLANAR_ALIGNED,
   PLANAR_U_ALIGNED=PLANAR_U|PLANAR_ALIGNED,
   PLANAR_V_ALIGNED=PLANAR_V|PLANAR_ALIGNED,
   PLANAR_A=1<<4,
   PLANAR_R=1<<5,
   PLANAR_G=1<<6,
   PLANAR_B=1<<7,
   PLANAR_A_ALIGNED=PLANAR_A|PLANAR_ALIGNED,
   PLANAR_R_ALIGNED=PLANAR_R|PLANAR_ALIGNED,
   PLANAR_G_ALIGNED=PLANAR_G|PLANAR_ALIGNED,
   PLANAR_B_ALIGNED=PLANAR_B|PLANAR_ALIGNED,
  };

class AvisynthError /* exception */ {
public:
  const char* const msg;
  AvisynthError(const char* _msg) : msg(_msg) {}

// Ensure AvisynthError cannot be publicly assigned!
private:
  AvisynthError& operator=(const AvisynthError&);
}; // end class AvisynthError

enum AvsDeviceType {
  DEV_TYPE_NONE = 0,
  DEV_TYPE_CPU = 1,
  DEV_TYPE_CUDA = 2,
  DEV_TYPE_ANY = 0xFFFF
};

/* Forward references */
#if defined(MSVC)
    #define SINGLE_INHERITANCE __single_inheritance
#else
    #define SINGLE_INHERITANCE
#endif
struct SINGLE_INHERITANCE VideoInfo;
class SINGLE_INHERITANCE VideoFrameBuffer;
class SINGLE_INHERITANCE VideoFrame;
class IClip;
class SINGLE_INHERITANCE PClip;
class SINGLE_INHERITANCE PVideoFrame;
class IScriptEnvironment;
class SINGLE_INHERITANCE AVSValue;
class INeoEnv;
class IFunction;
class SINGLE_INHERITANCE PFunction;
class Device;
class SINGLE_INHERITANCE PDevice;
class AVSMap;



/*
 * Avisynth C++ plugin API code function pointers.
 *
 * In order to maintain binary compatibility with
 * future version do not change the order of the
 * existing function pointers. It will be baked
 * into all existing plugins.
 *
 * Add new function pointers to the end of the
 * structure. The linkage macros generate some
 * protection code to ensure newer plugin do not
 * call non-existing functions in an older host.
 */

struct AVS_Linkage {

  int Size;

/**********************************************************************/

// struct VideoInfo
  bool    (VideoInfo::*HasVideo)() const;
  bool    (VideoInfo::*HasAudio)() const;
  bool    (VideoInfo::*IsRGB)() const;
  bool    (VideoInfo::*IsRGB24)() const;
  bool    (VideoInfo::*IsRGB32)() const;
  bool    (VideoInfo::*IsYUV)() const;
  bool    (VideoInfo::*IsYUY2)() const;
  bool    (VideoInfo::*IsYV24)() const;
  bool    (VideoInfo::*IsYV16)() const;
  bool    (VideoInfo::*IsYV12)() const;
  bool    (VideoInfo::*IsYV411)() const;
  bool    (VideoInfo::*IsY8)() const;
  bool    (VideoInfo::*IsColorSpace)(int c_space) const;
  bool    (VideoInfo::*Is)(int property) const;
  bool    (VideoInfo::*IsPlanar)() const;
  bool    (VideoInfo::*IsFieldBased)() const;
  bool    (VideoInfo::*IsParityKnown)() const;
  bool    (VideoInfo::*IsBFF)() const;
  bool    (VideoInfo::*IsTFF)() const;
  bool    (VideoInfo::*IsVPlaneFirst)() const;
  int     (VideoInfo::*BytesFromPixels)(int pixels) const;
  int     (VideoInfo::*RowSize)(int plane) const;
  int     (VideoInfo::*BMPSize)() const;
  int64_t (VideoInfo::*AudioSamplesFromFrames)(int frames) const;
  int     (VideoInfo::*FramesFromAudioSamples)(int64_t samples) const;
  int64_t (VideoInfo::*AudioSamplesFromBytes)(int64_t bytes) const;
  int64_t (VideoInfo::*BytesFromAudioSamples)(int64_t samples) const;
  int     (VideoInfo::*AudioChannels)() const;
  int     (VideoInfo::*SampleType)() const;
  bool    (VideoInfo::*IsSampleType)(int testtype) const;
  int     (VideoInfo::*SamplesPerSecond)() const;
  int     (VideoInfo::*BytesPerAudioSample)() const;
  void    (VideoInfo::*SetFieldBased)(bool isfieldbased);
  void    (VideoInfo::*Set)(int property);
  void    (VideoInfo::*Clear)(int property);
  int     (VideoInfo::*GetPlaneWidthSubsampling)(int plane) const;
  int     (VideoInfo::*GetPlaneHeightSubsampling)(int plane) const;
  int     (VideoInfo::*BitsPerPixel)() const;
  int     (VideoInfo::*BytesPerChannelSample)() const;
  void    (VideoInfo::*SetFPS)(unsigned numerator, unsigned denominator);
  void    (VideoInfo::*MulDivFPS)(unsigned multiplier, unsigned divisor);
  bool    (VideoInfo::*IsSameColorspace)(const VideoInfo& vi) const;
// end struct VideoInfo

/**********************************************************************/

// class VideoFrameBuffer
  const BYTE* (VideoFrameBuffer::*VFBGetReadPtr)() const;
  BYTE*       (VideoFrameBuffer::*VFBGetWritePtr)();
  int         (VideoFrameBuffer::*GetDataSize)() const;
  int         (VideoFrameBuffer::*GetSequenceNumber)() const;
  int         (VideoFrameBuffer::*GetRefcount)() const;
// end class VideoFrameBuffer

/**********************************************************************/

// class VideoFrame
  int               (VideoFrame::*GetPitch)(int plane) const;
  int               (VideoFrame::*GetRowSize)(int plane) const;
  int               (VideoFrame::*GetHeight)(int plane) const;
  VideoFrameBuffer* (VideoFrame::*GetFrameBuffer)() const;
  int               (VideoFrame::*GetOffset)(int plane) const;
  const BYTE*       (VideoFrame::*VFGetReadPtr)(int plane) const;
  bool              (VideoFrame::*IsWritable)() const;
  BYTE*             (VideoFrame::*VFGetWritePtr)(int plane) const;
  void              (VideoFrame::*VideoFrame_DESTRUCTOR)();
// end class VideoFrame

/**********************************************************************/

// class IClip
  /* nothing */
// end class IClip

/**********************************************************************/

// class PClip
  void (PClip::*PClip_CONSTRUCTOR0)();
  void (PClip::*PClip_CONSTRUCTOR1)(const PClip& x);
  void (PClip::*PClip_CONSTRUCTOR2)(IClip* x);
  void (PClip::*PClip_OPERATOR_ASSIGN0)(IClip* x);
  void (PClip::*PClip_OPERATOR_ASSIGN1)(const PClip& x);
  void (PClip::*PClip_DESTRUCTOR)();
// end class PClip

/**********************************************************************/

// class PVideoFrame
  void (PVideoFrame::*PVideoFrame_CONSTRUCTOR0)();
  void (PVideoFrame::*PVideoFrame_CONSTRUCTOR1)(const PVideoFrame& x);
  void (PVideoFrame::*PVideoFrame_CONSTRUCTOR2)(VideoFrame* x);
  void (PVideoFrame::*PVideoFrame_OPERATOR_ASSIGN0)(VideoFrame* x);
  void (PVideoFrame::*PVideoFrame_OPERATOR_ASSIGN1)(const PVideoFrame& x);
  void (PVideoFrame::*PVideoFrame_DESTRUCTOR)();
// end class PVideoFrame

/**********************************************************************/

// class AVSValue
  void            (AVSValue::*AVSValue_CONSTRUCTOR0)();
  void            (AVSValue::*AVSValue_CONSTRUCTOR1)(IClip* c);
  void            (AVSValue::*AVSValue_CONSTRUCTOR2)(const PClip& c);
  void            (AVSValue::*AVSValue_CONSTRUCTOR3)(bool b);
  void            (AVSValue::*AVSValue_CONSTRUCTOR4)(int i);
  void            (AVSValue::*AVSValue_CONSTRUCTOR5)(float f);
  void            (AVSValue::*AVSValue_CONSTRUCTOR6)(double f);
  void            (AVSValue::*AVSValue_CONSTRUCTOR7)(const char* s);
  void            (AVSValue::*AVSValue_CONSTRUCTOR8)(const AVSValue* a, int size);
  void            (AVSValue::*AVSValue_CONSTRUCTOR9)(const AVSValue& v);
  void            (AVSValue::*AVSValue_DESTRUCTOR)();
  AVSValue&       (AVSValue::*AVSValue_OPERATOR_ASSIGN)(const AVSValue& v);
  const AVSValue& (AVSValue::*AVSValue_OPERATOR_INDEX)(int index) const;
  bool            (AVSValue::*Defined)() const;
  bool            (AVSValue::*IsClip)() const;
  bool            (AVSValue::*IsBool)() const;
  bool            (AVSValue::*IsInt)() const;
  bool            (AVSValue::*IsFloat)() const;
  bool            (AVSValue::*IsString)() const;
  bool            (AVSValue::*IsArray)() const;
  PClip           (AVSValue::*AsClip)() const;
  bool            (AVSValue::*AsBool1)() const;
  int             (AVSValue::*AsInt1)() const;
  const char*     (AVSValue::*AsString1)() const;
  double          (AVSValue::*AsFloat1)() const;
  bool            (AVSValue::*AsBool2)(bool def) const;
  int             (AVSValue::*AsInt2)(int def) const;
  double          (AVSValue::*AsDblDef)(double def) const;
  double          (AVSValue::*AsFloat2)(float def) const;
  const char*     (AVSValue::*AsString2)(const char* def) const;
  int             (AVSValue::*ArraySize)() const;
// end class AVSValue

/**********************************************************************/
  // Reserve pointer space so that we can keep compatibility with Avs "classic" even if it adds functions on its own
  void    (VideoInfo::*reserved[32])();
/**********************************************************************/
  // AviSynth+ additions
  int     (VideoInfo::*NumComponents)() const;
  int     (VideoInfo::*ComponentSize)() const;
  int     (VideoInfo::*BitsPerComponent)() const;
  bool    (VideoInfo::*Is444)() const;
  bool    (VideoInfo::*Is422)() const;
  bool    (VideoInfo::*Is420)() const;
  bool    (VideoInfo::*IsY)() const;
  bool    (VideoInfo::*IsRGB48)() const;
  bool    (VideoInfo::*IsRGB64)() const;
  bool    (VideoInfo::*IsYUVA)() const;
  bool    (VideoInfo::*IsPlanarRGB)() const;
  bool    (VideoInfo::*IsPlanarRGBA)() const;
  /**********************************************************************/

  // frame property access
  AVSMap& (VideoFrame::* getProperties)();
  const AVSMap& (VideoFrame::* getConstProperties)();
  void (VideoFrame::* setProperties)(const AVSMap& properties);

  // PFunction
  void          (AVSValue::* AVSValue_CONSTRUCTOR11)(const PFunction& o);
  bool          (AVSValue::* IsFunction)() const;
  void          (PFunction::* PFunction_CONSTRUCTOR0)();
  void          (PFunction::* PFunction_CONSTRUCTOR1)(IFunction* p);
  void          (PFunction::* PFunction_CONSTRUCTOR2)(const PFunction& p);
  PFunction&    (PFunction::* PFunction_OPERATOR_ASSIGN0)(IFunction* other);
  PFunction&    (PFunction::* PFunction_OPERATOR_ASSIGN1)(const PFunction& other);
  void          (PFunction::* PFunction_DESTRUCTOR)();
  // end PFunction

  // extra VideoFrame functions
  int           (VideoFrame::* VideoFrame_CheckMemory)() const;
  PDevice       (VideoFrame::* VideoFrame_GetDevice)() const;

  // class PDevice, even if only CPU device
  void          (PDevice::* PDevice_CONSTRUCTOR0)();
  void          (PDevice::* PDevice_CONSTRUCTOR1)(Device* p);
  void          (PDevice::* PDevice_CONSTRUCTOR2)(const PDevice& p);
  PDevice&      (PDevice::* PDevice_OPERATOR_ASSIGN0)(Device* other);
  PDevice&      (PDevice::* PDevice_OPERATOR_ASSIGN1)(const PDevice& other);
  void          (PDevice::* PDevice_DESTRUCTOR)();
  AvsDeviceType (PDevice::* PDevice_GetType)() const;
  int           (PDevice::* PDevice_GetId)() const;
  int           (PDevice::* PDevice_GetIndex)() const;
  const char*   (PDevice::* PDevice_GetName)() const;
  // end class PDevice

  /**********************************************************************/
  // Reserve pointer space for Avisynth+
  void          (VideoInfo::* reserved2[64 - 23])();
  /**********************************************************************/

  // AviSynth Neo additions
  INeoEnv*      (__stdcall *GetNeoEnv)(IScriptEnvironment* env);
  // As of V8 most PDevice, PFunction linkage entries are moved to standard avs+ place.
  /**********************************************************************/

  // this part should be identical with AVS_Linkage entries in interface.cpp
};

#ifdef BUILDING_AVSCORE
/* Macro resolution for code inside Avisynth.dll */
# define AVS_BakedCode(arg) ;
# define AVS_LinkCall(arg)
# define AVS_LinkCallV(arg)
# define AVS_LinkCallOpt(arg, argOpt) AVSLinkCall(arg)
# define AVS_LinkCallOptDefault(arg, argDefaultValue) AVSLinkCall(arg())
# define CALL_MEMBER_FN(object,ptrToMember)

#else
/* Macro resolution for code inside user plugin */
# ifdef AVS_LINKAGE_DLLIMPORT
extern __declspec(dllimport) const AVS_Linkage* const AVS_linkage;
# else
extern const AVS_Linkage* AVS_linkage;
# endif

# ifndef offsetof
#  include <stddef.h>
# endif

# define AVS_BakedCode(arg) { arg ; }
# define AVS_LinkCall(arg)  !AVS_linkage || offsetof(AVS_Linkage, arg) >= AVS_linkage->Size ?     0 : (this->*(AVS_linkage->arg))
# define AVS_LinkCall_Void(arg)  !AVS_linkage || offsetof(AVS_Linkage, arg) >= AVS_linkage->Size ?     (void)0 : (this->*(AVS_linkage->arg))
# define AVS_LinkCallV(arg) !AVS_linkage || offsetof(AVS_Linkage, arg) >= AVS_linkage->Size ? *this : (this->*(AVS_linkage->arg))
// Helper macros for fallback option when a function does not exists
#define CALL_MEMBER_FN(object,ptrToMember)  ((object)->*(ptrToMember))
#define AVS_LinkCallOpt(arg, argOpt)  !AVS_linkage ? 0 : \
                                      ( offsetof(AVS_Linkage, arg) >= AVS_linkage->Size ? \
                                        (offsetof(AVS_Linkage, argOpt) >= AVS_linkage->Size ? 0 : CALL_MEMBER_FN(this, AVS_linkage->argOpt)() ) : \
                                        CALL_MEMBER_FN(this, AVS_linkage->arg)() )
// AVS_LinkCallOptDefault puts automatically () only after arg
# define AVS_LinkCallOptDefault(arg, argDefaultValue)  !AVS_linkage || offsetof(AVS_Linkage, arg) >= AVS_linkage->Size ? (argDefaultValue) : ((this->*(AVS_linkage->arg))())

#endif

class PDevice
{
public:
  PDevice() AVS_BakedCode(AVS_LinkCall_Void(PDevice_CONSTRUCTOR0)())
    PDevice(Device* p) AVS_BakedCode(AVS_LinkCall_Void(PDevice_CONSTRUCTOR1)(p))
    PDevice(const PDevice& p) AVS_BakedCode(AVS_LinkCall_Void(PDevice_CONSTRUCTOR2)(p))
    PDevice& operator=(Device* p) AVS_BakedCode(return AVS_LinkCallV(PDevice_OPERATOR_ASSIGN0)(p))
    PDevice& operator=(const PDevice& p) AVS_BakedCode(return AVS_LinkCallV(PDevice_OPERATOR_ASSIGN1)(p))
    ~PDevice() AVS_BakedCode(AVS_LinkCall_Void(PDevice_DESTRUCTOR)())

    int operator!() const { return !e; }
  operator void*() const { return e; }
  Device* operator->() const { return e; }

  AvsDeviceType GetType() const AVS_BakedCode(return AVS_LinkCallOptDefault(PDevice_GetType, DEV_TYPE_NONE))
    int GetId() const AVS_BakedCode(return AVS_LinkCall(PDevice_GetId)())
    int GetIndex() const AVS_BakedCode(return AVS_LinkCall(PDevice_GetIndex)())
    const char* GetName() const AVS_BakedCode(return AVS_LinkCall(PDevice_GetName)())

private:
  Device * e;

#ifdef BUILDING_AVSCORE
public:
  void CONSTRUCTOR0();  /* Damn compiler won't allow taking the address of reserved constructs, make a dummy interlude */
  void CONSTRUCTOR1(Device* p);
  void CONSTRUCTOR2(const PDevice& p);
  PDevice& OPERATOR_ASSIGN0(Device* p);
  PDevice& OPERATOR_ASSIGN1(const PDevice& p);
  void DESTRUCTOR();
#endif
};
struct VideoInfo {
  int width, height;    // width=0 means no video
  unsigned fps_numerator, fps_denominator;
  int num_frames;
  // This is more extensible than previous versions. More properties can be added seeminglesly.

  // Colorspace properties.
/*

Planar match mask  1111.1000.0000.0111.0000.0111.0000.0111
Planar signature   10xx.1000.0000.00xx.0000.00xx.00xx.00xx ?
Planar signature   10xx.1000.0000.0xxx.0000.00xx.000x.x0xx ? *new
Planar filter mask 1111.1111.1111.1111.1111.1111.1110.0111 (typo from old header fixed)

pixel_type mapping
==================
pixel_type bit-map PIYB.Z000.0???.0SSS.0000.0???.????.????
        planar YUV            CCC            HHH.000u.vWWW
     planar RGB(A)            CCC                       AR
         nonplanar            CCC            000.00wx xyAR
Legend
======
Planar YUV:
  Code Bits Remark
  W    0-2  Planar Width Subsampling bits
            Use (X+1) & 3 for GetPlaneWidthSubsampling
              000 => 1        YV12, YV16, YUV420, YUV422
              001 => 2        YV411, YUV9
              010 => reserved
              011 => 0        YV24, YUV444, RGBP
              1xx => reserved
  v    3    VPlaneFirst YV12, YV16, YV24, YV411, YUV9
  u    4    UPlaneFirst I420
  H    7-9  Planar Height Subsampling bits
            Use ((X>>8)+1) & 3 for GetPlaneHeightSubsampling
              000 => 1        YV12, YUV420
              001 => 2        YUV9
              010 => reserved
              011 => 0        YV16, YV24, YV411, YUV422, YUV444, RGBP
              1xx => reserved

Planar RGB
 Code Bits Remark
   R   0   BGR,  (with SSS bits for 8/16 bit/sample or float)
   A   1   BGRA, (with SSS bits for 8/16 bit/sample or float)


Not Planar, Interleaved (I flag)
Code Bits Remark
  R   0   BGR24, and BGRx in future (with SSS bits for 8/16 bit/sample or float)
  A   1   BGR32, and BGRAx in future (with SSS bits for 8/16 bit/sample or float)
  y   2   YUY2
  x   3-4 reserved
  w   5   Raw32

General
Code Bits Remark
  S 16-18 Sample resolution bits
          000 => 8
          001 => 16
          010 => 32 (float)
          011,100 => reserved
          101 => 10 bits
          110 => 12 bits
          111 => 14 bits
for packed RGB(A): only 8 and 16 bits are valid

Other YV12 specific (not used?)
  C 20-22 Chroma Placement values 0-4 see CS_xxx_CHROMA_PLACEMENT

Color family and layout
                       Packed      Planar               Planar  Planar
Code Bits Remark       RGB/RGBA     YUV  YUY2  Y_Grey  RGB/RGBA  YUVA
  R   0                  1/0         -    0      -       1/0       -
  A   1                  0/1         -    0      -       0/1       -
  y   2                   -          -    1      -        0        -
  Z  27   YUVA            0          0    0      0        1        1
  B  28   BGR             1          0    0      0        1*       0
  Y  29   YUV             0          1    1      1        0        0
  I  30   Interleaved     1          0    1      1        0        0
  P  31   Planar          0          1    0      1        1        1
* Planar RGB plane order: G,B,R(,A)

*/
enum {
    CS_YUVA = 1<<27,
    CS_BGR = 1<<28,
    CS_YUV = 1<<29,
    CS_INTERLEAVED = 1<<30,
    CS_PLANAR = 1<<31,

    CS_Shift_Sub_Width   =  0,
    CS_Shift_Sub_Height  =  8,
    CS_Shift_Sample_Bits = 16,

    CS_Sub_Width_Mask    = 7 << CS_Shift_Sub_Width,
    CS_Sub_Width_1       = 3 << CS_Shift_Sub_Width, // YV24
    CS_Sub_Width_2       = 0 << CS_Shift_Sub_Width, // YV12, I420, YV16
    CS_Sub_Width_4       = 1 << CS_Shift_Sub_Width, // YUV9, YV411

    CS_VPlaneFirst       = 1 << 3, // YV12, YV16, YV24, YV411, YUV9
    CS_UPlaneFirst       = 1 << 4, // I420

    CS_Sub_Height_Mask   = 7 << CS_Shift_Sub_Height,
    CS_Sub_Height_1      = 3 << CS_Shift_Sub_Height, // YV16, YV24, YV411
    CS_Sub_Height_2      = 0 << CS_Shift_Sub_Height, // YV12, I420
    CS_Sub_Height_4      = 1 << CS_Shift_Sub_Height, // YUV9

    CS_Sample_Bits_Mask  = 7 << CS_Shift_Sample_Bits,
    CS_Sample_Bits_8     = 0 << CS_Shift_Sample_Bits,
    CS_Sample_Bits_10    = 5 << CS_Shift_Sample_Bits,
    CS_Sample_Bits_12    = 6 << CS_Shift_Sample_Bits,
    CS_Sample_Bits_14    = 7 << CS_Shift_Sample_Bits,
    CS_Sample_Bits_16    = 1 << CS_Shift_Sample_Bits,
    CS_Sample_Bits_32    = 2 << CS_Shift_Sample_Bits,

    CS_PLANAR_MASK       = CS_PLANAR | CS_INTERLEAVED | CS_YUV | CS_BGR | CS_YUVA | CS_Sample_Bits_Mask
    | CS_Sub_Height_Mask | CS_Sub_Width_Mask,
    CS_PLANAR_FILTER     = ~( CS_VPlaneFirst | CS_UPlaneFirst ),

    CS_RGB_TYPE  = 1 << 0,
    CS_RGBA_TYPE = 1 << 1,

    // Specific colorformats
    CS_UNKNOWN = 0,

    CS_BGR24 = CS_RGB_TYPE  | CS_BGR | CS_INTERLEAVED,
    CS_BGR32 = CS_RGBA_TYPE | CS_BGR | CS_INTERLEAVED,
    CS_YUY2  = 1<<2 | CS_YUV | CS_INTERLEAVED,
    //  CS_YV12  = 1<<3  Reserved
    //  CS_I420  = 1<<4  Reserved
    CS_RAW32 = 1<<5 | CS_INTERLEAVED,

    //  YV12 must be 0xA000008 2.5 Baked API will see all new planar as YV12
    //  I420 must be 0xA000010

    CS_GENERIC_YUV420  = CS_PLANAR | CS_YUV | CS_VPlaneFirst | CS_Sub_Height_2 | CS_Sub_Width_2,  // 4:2:0 planar
    CS_GENERIC_YUV422  = CS_PLANAR | CS_YUV | CS_VPlaneFirst | CS_Sub_Height_1 | CS_Sub_Width_2,  // 4:2:2 planar
    CS_GENERIC_YUV444  = CS_PLANAR | CS_YUV | CS_VPlaneFirst | CS_Sub_Height_1 | CS_Sub_Width_1,  // 4:4:4 planar
    CS_GENERIC_Y       = CS_PLANAR | CS_INTERLEAVED | CS_YUV,                                     // Y only (4:0:0)
    CS_GENERIC_RGBP    = CS_PLANAR | CS_BGR | CS_RGB_TYPE,                                        // planar RGB. Though name is RGB but plane order G,B,R
    CS_GENERIC_RGBAP   = CS_PLANAR | CS_BGR | CS_RGBA_TYPE,                                       // planar RGBA
    CS_GENERIC_YUVA420 = CS_PLANAR | CS_YUVA | CS_VPlaneFirst | CS_Sub_Height_2 | CS_Sub_Width_2, // 4:2:0:A planar
    CS_GENERIC_YUVA422 = CS_PLANAR | CS_YUVA | CS_VPlaneFirst | CS_Sub_Height_1 | CS_Sub_Width_2, // 4:2:2:A planar
    CS_GENERIC_YUVA444 = CS_PLANAR | CS_YUVA | CS_VPlaneFirst | CS_Sub_Height_1 | CS_Sub_Width_1, // 4:4:4:A planar

    CS_YV24  = CS_GENERIC_YUV444 | CS_Sample_Bits_8,  // YVU 4:4:4 planar
    CS_YV16  = CS_GENERIC_YUV422 | CS_Sample_Bits_8,  // YVU 4:2:2 planar
    CS_YV12  = CS_GENERIC_YUV420 | CS_Sample_Bits_8,  // YVU 4:2:0 planar
    CS_I420  = CS_PLANAR | CS_YUV | CS_Sample_Bits_8 | CS_UPlaneFirst | CS_Sub_Height_2 | CS_Sub_Width_2,  // YUV 4:2:0 planar
    CS_IYUV  = CS_I420,
    CS_YUV9  = CS_PLANAR | CS_YUV | CS_Sample_Bits_8 | CS_VPlaneFirst | CS_Sub_Height_4 | CS_Sub_Width_4,  // YUV 4:1:0 planar
    CS_YV411 = CS_PLANAR | CS_YUV | CS_Sample_Bits_8 | CS_VPlaneFirst | CS_Sub_Height_1 | CS_Sub_Width_4,  // YUV 4:1:1 planar

    CS_Y8    = CS_GENERIC_Y | CS_Sample_Bits_8,                                                            // Y   4:0:0 planar

    //-------------------------
    // AVS16: new planar constants go live! Experimental PF 160613
    // 10-12-14 bit + planar RGB + BRG48/64 160725

    CS_YUV444P10 = CS_GENERIC_YUV444 | CS_Sample_Bits_10, // YUV 4:4:4 10bit samples
    CS_YUV422P10 = CS_GENERIC_YUV422 | CS_Sample_Bits_10, // YUV 4:2:2 10bit samples
    CS_YUV420P10 = CS_GENERIC_YUV420 | CS_Sample_Bits_10, // YUV 4:2:0 10bit samples
    CS_Y10 = CS_GENERIC_Y | CS_Sample_Bits_10,            // Y   4:0:0 10bit samples

    CS_YUV444P12 = CS_GENERIC_YUV444 | CS_Sample_Bits_12, // YUV 4:4:4 12bit samples
    CS_YUV422P12 = CS_GENERIC_YUV422 | CS_Sample_Bits_12, // YUV 4:2:2 12bit samples
    CS_YUV420P12 = CS_GENERIC_YUV420 | CS_Sample_Bits_12, // YUV 4:2:0 12bit samples
    CS_Y12 = CS_GENERIC_Y | CS_Sample_Bits_12,            // Y   4:0:0 12bit samples

    CS_YUV444P14 = CS_GENERIC_YUV444 | CS_Sample_Bits_14, // YUV 4:4:4 14bit samples
    CS_YUV422P14 = CS_GENERIC_YUV422 | CS_Sample_Bits_14, // YUV 4:2:2 14bit samples
    CS_YUV420P14 = CS_GENERIC_YUV420 | CS_Sample_Bits_14, // YUV 4:2:0 14bit samples
    CS_Y14 = CS_GENERIC_Y | CS_Sample_Bits_14,            // Y   4:0:0 14bit samples

    CS_YUV444P16 = CS_GENERIC_YUV444 | CS_Sample_Bits_16, // YUV 4:4:4 16bit samples
    CS_YUV422P16 = CS_GENERIC_YUV422 | CS_Sample_Bits_16, // YUV 4:2:2 16bit samples
    CS_YUV420P16 = CS_GENERIC_YUV420 | CS_Sample_Bits_16, // YUV 4:2:0 16bit samples
    CS_Y16 = CS_GENERIC_Y | CS_Sample_Bits_16,            // Y   4:0:0 16bit samples

    // 32 bit samples (float)
    CS_YUV444PS = CS_GENERIC_YUV444 | CS_Sample_Bits_32,  // YUV 4:4:4 32bit samples
    CS_YUV422PS = CS_GENERIC_YUV422 | CS_Sample_Bits_32,  // YUV 4:2:2 32bit samples
    CS_YUV420PS = CS_GENERIC_YUV420 | CS_Sample_Bits_32,  // YUV 4:2:0 32bit samples
    CS_Y32 = CS_GENERIC_Y | CS_Sample_Bits_32,            // Y   4:0:0 32bit samples

    // RGB packed
    CS_BGR48 = CS_RGB_TYPE  | CS_BGR | CS_INTERLEAVED | CS_Sample_Bits_16, // BGR 3x16 bit
    CS_BGR64 = CS_RGBA_TYPE | CS_BGR | CS_INTERLEAVED | CS_Sample_Bits_16, // BGR 4x16 bit
    // no packed 32 bit (float) support for these legacy types

    // RGB planar
    CS_RGBP   = CS_GENERIC_RGBP | CS_Sample_Bits_8,  // Planar RGB 8 bit samples
    CS_RGBP8  = CS_GENERIC_RGBP | CS_Sample_Bits_8,  // Planar RGB 8 bit samples
    CS_RGBP10 = CS_GENERIC_RGBP | CS_Sample_Bits_10, // Planar RGB 10bit samples
    CS_RGBP12 = CS_GENERIC_RGBP | CS_Sample_Bits_12, // Planar RGB 12bit samples
    CS_RGBP14 = CS_GENERIC_RGBP | CS_Sample_Bits_14, // Planar RGB 14bit samples
    CS_RGBP16 = CS_GENERIC_RGBP | CS_Sample_Bits_16, // Planar RGB 16bit samples
    CS_RGBPS  = CS_GENERIC_RGBP | CS_Sample_Bits_32, // Planar RGB 32bit samples

    // RGBA planar
    CS_RGBAP   = CS_GENERIC_RGBAP | CS_Sample_Bits_8,  // Planar RGBA 8 bit samples
    CS_RGBAP8  = CS_GENERIC_RGBAP | CS_Sample_Bits_8,  // Planar RGBA 8 bit samples
    CS_RGBAP10 = CS_GENERIC_RGBAP | CS_Sample_Bits_10, // Planar RGBA 10bit samples
    CS_RGBAP12 = CS_GENERIC_RGBAP | CS_Sample_Bits_12, // Planar RGBA 12bit samples
    CS_RGBAP14 = CS_GENERIC_RGBAP | CS_Sample_Bits_14, // Planar RGBA 14bit samples
    CS_RGBAP16 = CS_GENERIC_RGBAP | CS_Sample_Bits_16, // Planar RGBA 16bit samples
    CS_RGBAPS  = CS_GENERIC_RGBAP | CS_Sample_Bits_32, // Planar RGBA 32bit samples

    // Planar YUVA
    CS_YUVA444    = CS_GENERIC_YUVA444 | CS_Sample_Bits_8,  // YUVA 4:4:4 8bit samples
    CS_YUVA422    = CS_GENERIC_YUVA422 | CS_Sample_Bits_8,  // YUVA 4:2:2 8bit samples
    CS_YUVA420    = CS_GENERIC_YUVA420 | CS_Sample_Bits_8,  // YUVA 4:2:0 8bit samples

    CS_YUVA444P10 = CS_GENERIC_YUVA444 | CS_Sample_Bits_10, // YUVA 4:4:4 10bit samples
    CS_YUVA422P10 = CS_GENERIC_YUVA422 | CS_Sample_Bits_10, // YUVA 4:2:2 10bit samples
    CS_YUVA420P10 = CS_GENERIC_YUVA420 | CS_Sample_Bits_10, // YUVA 4:2:0 10bit samples

    CS_YUVA444P12 = CS_GENERIC_YUVA444 | CS_Sample_Bits_12, // YUVA 4:4:4 12bit samples
    CS_YUVA422P12 = CS_GENERIC_YUVA422 | CS_Sample_Bits_12, // YUVA 4:2:2 12bit samples
    CS_YUVA420P12 = CS_GENERIC_YUVA420 | CS_Sample_Bits_12, // YUVA 4:2:0 12bit samples

    CS_YUVA444P14 = CS_GENERIC_YUVA444 | CS_Sample_Bits_14, // YUVA 4:4:4 14bit samples
    CS_YUVA422P14 = CS_GENERIC_YUVA422 | CS_Sample_Bits_14, // YUVA 4:2:2 14bit samples
    CS_YUVA420P14 = CS_GENERIC_YUVA420 | CS_Sample_Bits_14, // YUVA 4:2:0 14bit samples

    CS_YUVA444P16 = CS_GENERIC_YUVA444 | CS_Sample_Bits_16, // YUVA 4:4:4 16bit samples
    CS_YUVA422P16 = CS_GENERIC_YUVA422 | CS_Sample_Bits_16, // YUVA 4:2:2 16bit samples
    CS_YUVA420P16 = CS_GENERIC_YUVA420 | CS_Sample_Bits_16, // YUVA 4:2:0 16bit samples

    CS_YUVA444PS  = CS_GENERIC_YUVA444 | CS_Sample_Bits_32,  // YUVA 4:4:4 32bit samples
    CS_YUVA422PS  = CS_GENERIC_YUVA422 | CS_Sample_Bits_32,  // YUVA 4:2:2 32bit samples
    CS_YUVA420PS  = CS_GENERIC_YUVA420 | CS_Sample_Bits_32,  // YUVA 4:2:0 32bit samples

  };

  int pixel_type;                // changed to int as of 2.5


  int audio_samples_per_second;   // 0 means no audio
  int sample_type;                // as of 2.5
  int64_t num_audio_samples;      // changed as of 2.5
  int nchannels;                  // as of 2.5

  // Imagetype properties

  int image_type;

  enum {
    IT_BFF = 1<<0,
    IT_TFF = 1<<1,
    IT_FIELDBASED = 1<<2
  };

  // Chroma placement bits 20 -> 23  ::FIXME:: Really want a Class to support this
  enum {
    CS_UNKNOWN_CHROMA_PLACEMENT = 0 << 20,
    CS_MPEG1_CHROMA_PLACEMENT   = 1 << 20,
    CS_MPEG2_CHROMA_PLACEMENT   = 2 << 20,
    CS_YUY2_CHROMA_PLACEMENT    = 3 << 20,
    CS_TOPLEFT_CHROMA_PLACEMENT = 4 << 20
  };

  // useful functions of the above
  bool HasVideo() const AVS_BakedCode(return AVS_LinkCall(HasVideo)())
  bool HasAudio() const AVS_BakedCode(return AVS_LinkCall(HasAudio)())
  bool IsRGB() const AVS_BakedCode(return AVS_LinkCall(IsRGB)())
  bool IsRGB24() const AVS_BakedCode(return AVS_LinkCall(IsRGB24)())
  bool IsRGB32() const AVS_BakedCode(return AVS_LinkCall(IsRGB32)())
  bool IsYUV() const AVS_BakedCode(return AVS_LinkCall(IsYUV)())
  bool IsYUY2() const AVS_BakedCode(return AVS_LinkCall(IsYUY2)())

  bool IsYV24()  const AVS_BakedCode(return AVS_LinkCall(IsYV24)())
  bool IsYV16()  const AVS_BakedCode(return AVS_LinkCall(IsYV16)())
  bool IsYV12()  const AVS_BakedCode(return AVS_LinkCall(IsYV12)())
  bool IsYV411() const AVS_BakedCode(return AVS_LinkCall(IsYV411)())
  //bool IsYUV9()  const;
  bool IsY8()    const AVS_BakedCode(return AVS_LinkCall(IsY8)())

  bool IsColorSpace(int c_space) const AVS_BakedCode(return AVS_LinkCall(IsColorSpace)(c_space))

  bool Is(int property) const AVS_BakedCode(return AVS_LinkCall(Is)(property))
  bool IsPlanar() const AVS_BakedCode(return AVS_LinkCall(IsPlanar)())
  bool IsFieldBased() const AVS_BakedCode(return AVS_LinkCall(IsFieldBased)())
  bool IsParityKnown() const AVS_BakedCode(return AVS_LinkCall(IsParityKnown)())
  bool IsBFF() const AVS_BakedCode(return AVS_LinkCall(IsBFF)())
  bool IsTFF() const AVS_BakedCode(return AVS_LinkCall(IsTFF)())

  bool IsVPlaneFirst() const AVS_BakedCode(return AVS_LinkCall(IsVPlaneFirst)())  // Don't use this
  // Will not work on planar images, but will return only luma planes
  int BytesFromPixels(int pixels) const AVS_BakedCode(return AVS_LinkCall(BytesFromPixels)(pixels))
  int RowSize(int plane = 0) const AVS_BakedCode(return AVS_LinkCall(RowSize)(plane))
  int BMPSize() const AVS_BakedCode(return AVS_LinkCall(BMPSize)())

  int64_t AudioSamplesFromFrames(int frames) const AVS_BakedCode(return AVS_LinkCall(AudioSamplesFromFrames)(frames))
  int FramesFromAudioSamples(int64_t samples) const AVS_BakedCode(return AVS_LinkCall(FramesFromAudioSamples)(samples))
  int64_t AudioSamplesFromBytes(int64_t bytes) const AVS_BakedCode(return AVS_LinkCall(AudioSamplesFromBytes)(bytes))
  int64_t BytesFromAudioSamples(int64_t samples) const AVS_BakedCode(return AVS_LinkCall(BytesFromAudioSamples)(samples))
  int AudioChannels() const AVS_BakedCode(return AVS_LinkCall(AudioChannels)())
  int SampleType() const AVS_BakedCode(return AVS_LinkCall(SampleType)())
  bool IsSampleType(int testtype) const AVS_BakedCode(return AVS_LinkCall(IsSampleType)(testtype))
  int SamplesPerSecond() const AVS_BakedCode(return AVS_LinkCall(SamplesPerSecond)())
  int BytesPerAudioSample() const AVS_BakedCode(return AVS_LinkCall(BytesPerAudioSample)())
  void SetFieldBased(bool isfieldbased) AVS_BakedCode(AVS_LinkCall_Void(SetFieldBased)(isfieldbased))
  void Set(int property) AVS_BakedCode(AVS_LinkCall_Void(Set)(property))
  void Clear(int property) AVS_BakedCode(AVS_LinkCall_Void(Clear)(property))
  // Subsampling in bitshifts!
  int GetPlaneWidthSubsampling(int plane) const AVS_BakedCode(return AVS_LinkCall(GetPlaneWidthSubsampling)(plane))
  int GetPlaneHeightSubsampling(int plane) const AVS_BakedCode(return AVS_LinkCall(GetPlaneHeightSubsampling)(plane))
  int BitsPerPixel() const AVS_BakedCode(return AVS_LinkCall(BitsPerPixel)())

  int BytesPerChannelSample() const AVS_BakedCode(return AVS_LinkCall(BytesPerChannelSample)())

  // useful mutator
  void SetFPS(unsigned numerator, unsigned denominator) AVS_BakedCode(AVS_LinkCall_Void(SetFPS)(numerator, denominator))

  // Range protected multiply-divide of FPS
  void MulDivFPS(unsigned multiplier, unsigned divisor) AVS_BakedCode(AVS_LinkCall_Void(MulDivFPS)(multiplier, divisor))

  // Test for same colorspace
  bool IsSameColorspace(const VideoInfo& vi) const AVS_BakedCode(return AVS_LinkCall(IsSameColorspace)(vi))

  // AVS+ extensions
  // 20161005:
  //   Mapping of AVS+ extensions to classic 2.6 functions.
  //   In order to use these extended AVS+ functions for plugins that should work
  //   either with AVS+ or with Classic (8 bit) Avs 2.6 ans earlier AVS+ versions, there is an
  //   automatic fallback mechanism.
  //   From AVS+'s point of view these are not "baked" codes, the primary functions should exist.
  //   Examples:
  //   Is444() is mapped to IsYV24() for classic AVS2.6
  //   ComponentSize() returns constant 1 (1 bytes per pixel component)
  //   BitsPerComponent() returns constant 8 (Classic AVS2.6 is 8 bit only)

  // Returns the number of color channels or planes in a frame
  int NumComponents() const AVS_BakedCode(return AVS_LinkCallOptDefault(NumComponents, (((AVS_LinkCall(IsYUV)()) && !(AVS_LinkCall(IsY8)())) ? 3 : AVS_LinkCall(BytesFromPixels)(1)) ) )

  // Returns the size in bytes of a single component of a pixel
  int ComponentSize() const AVS_BakedCode(return AVS_LinkCallOptDefault(ComponentSize, 1))

  // Returns the bit depth of a single component of a pixel
  int BitsPerComponent() const AVS_BakedCode(return AVS_LinkCallOptDefault(BitsPerComponent, 8))

  // like IsYV24, but bit-depth independent also for YUVA
  bool Is444() const AVS_BakedCode(return AVS_LinkCallOpt(Is444, IsYV24) )

  // like IsYV16, but bit-depth independent also for YUVA
  bool Is422() const AVS_BakedCode(return AVS_LinkCallOpt(Is422, IsYV16) )

  // like IsYV12, but bit-depth independent also for YUVA
  bool Is420() const AVS_BakedCode( return AVS_LinkCallOpt(Is420, IsYV12) )

  // like IsY8, but bit-depth independent
  bool IsY()   const AVS_BakedCode( return AVS_LinkCallOpt(IsY, IsY8) )

  // like IsRGB24 for 16 bit samples
  bool IsRGB48() const AVS_BakedCode( return AVS_LinkCallOptDefault(IsRGB48, false) )

  // like IsRGB32 for 16 bit samples
  bool IsRGB64() const AVS_BakedCode( return AVS_LinkCallOptDefault(IsRGB64, false) )

  // YUVA?
  bool IsYUVA() const AVS_BakedCode( return AVS_LinkCallOptDefault(IsYUVA, false) )

  // Planar RGB?
  bool IsPlanarRGB() const AVS_BakedCode( return AVS_LinkCallOptDefault(IsPlanarRGB, false) )

  // Planar RGBA?
  bool IsPlanarRGBA() const AVS_BakedCode( return AVS_LinkCallOptDefault(IsPlanarRGBA, false) )

}; // end struct VideoInfo




// VideoFrameBuffer holds information about a memory block which is used
// for video data.  For efficiency, instances of this class are not deleted
// when the refcount reaches zero; instead they're stored in a linked list
// to be reused.  The instances are deleted when the corresponding AVS
// file is closed.

class VideoFrameBuffer {
  BYTE* data;
  int data_size;
  // sequence_number is incremented every time the buffer is changed, so
  // that stale views can tell they're no longer valid.
  volatile long sequence_number;

  friend class VideoFrame;
  friend class Cache;
  friend class ScriptEnvironment;
  volatile long refcount;

  // AVS+CUDA extension, does not break plugins if appended here
  Device* device;

protected:
  VideoFrameBuffer(int size, int margin, Device* device);
  VideoFrameBuffer();
  ~VideoFrameBuffer();

public:
  const BYTE* GetReadPtr() const AVS_BakedCode( return AVS_LinkCall(VFBGetReadPtr)() )
  BYTE* GetWritePtr() AVS_BakedCode( return AVS_LinkCall(VFBGetWritePtr)() )
  int GetDataSize() const AVS_BakedCode( return AVS_LinkCall(GetDataSize)() )
  int GetSequenceNumber() const AVS_BakedCode( return AVS_LinkCall(GetSequenceNumber)() )
  int GetRefcount() const AVS_BakedCode( return AVS_LinkCall(GetRefcount)() )

// Ensure VideoFrameBuffer cannot be publicly assigned
private:
    VideoFrameBuffer& operator=(const VideoFrameBuffer&);

}; // end class VideoFrameBuffer


// smart pointer to VideoFrame
class PVideoFrame {

  VideoFrame* p;

  void Init(VideoFrame* x);
  void Set(VideoFrame* x);

public:
  PVideoFrame() AVS_BakedCode(AVS_LinkCall_Void(PVideoFrame_CONSTRUCTOR0)())
    PVideoFrame(const PVideoFrame& x) AVS_BakedCode(AVS_LinkCall_Void(PVideoFrame_CONSTRUCTOR1)(x))
    PVideoFrame(VideoFrame* x) AVS_BakedCode(AVS_LinkCall_Void(PVideoFrame_CONSTRUCTOR2)(x))
    void operator=(VideoFrame* x) AVS_BakedCode(AVS_LinkCall_Void(PVideoFrame_OPERATOR_ASSIGN0)(x))
    void operator=(const PVideoFrame& x) AVS_BakedCode(AVS_LinkCall_Void(PVideoFrame_OPERATOR_ASSIGN1)(x))

    VideoFrame* operator->() const { return p; }

  // for conditional expressions
  operator void*() const { return p; }
  bool operator!() const { return !p; }

  ~PVideoFrame() AVS_BakedCode(AVS_LinkCall_Void(PVideoFrame_DESTRUCTOR)())
#ifdef BUILDING_AVSCORE
public:
  void CONSTRUCTOR0();  /* Damn compiler won't allow taking the address of reserved constructs, make a dummy interlude */
  void CONSTRUCTOR1(const PVideoFrame& x);
  void CONSTRUCTOR2(VideoFrame* x);
  void OPERATOR_ASSIGN0(VideoFrame* x);
  void OPERATOR_ASSIGN1(const PVideoFrame& x);
  void DESTRUCTOR();
#endif
}; // end class PVideoFrame


// VideoFrame holds a "window" into a VideoFrameBuffer.  Operator new
// is overloaded to recycle class instances.

class VideoFrame {
  volatile long refcount;
  VideoFrameBuffer* vfb;

  // Due to technical reasons these members are not const, but should be treated as such.
  // That means do not modify them once the class has been constructed.
  int offset;
  int pitch, row_size, height;
  int offsetU, offsetV;  // U&V offsets are from top of picture.
  int pitchUV, row_sizeUV, heightUV; // for Planar RGB offsetU, offsetV is for the 2nd and 3rd Plane.
                            // for Planar RGB pitchUV and row_sizeUV = 0, because when no VideoInfo (MakeWriteable)
                            // the decision on existance of UV is checked by zero pitch
  // AVS+ extension, does not break plugins if appended here
  int offsetA;
  int pitchA, row_sizeA; // 4th alpha plane support, pitch and row_size is 0 is none

  AVSMap *properties;

  friend class PVideoFrame;
  void AddRef();
  void Release();

  friend class ScriptEnvironment;
  friend class Cache;

  VideoFrame(VideoFrameBuffer* _vfb, AVSMap* avsmap, int _offset, int _pitch, int _row_size, int _height);
  VideoFrame(VideoFrameBuffer* _vfb, AVSMap* avsmap, int _offset, int _pitch, int _row_size, int _height, int _offsetU, int _offsetV, int _pitchUV, int _row_sizeUV, int _heightUV);
  // for Alpha
  VideoFrame(VideoFrameBuffer* _vfb, AVSMap* avsmap, int _offset, int _pitch, int _row_size, int _height, int _offsetU, int _offsetV, int _pitchUV, int _row_sizeUV, int _heightUV, int _offsetA);
  void* operator new(size_t size);
// TESTME: OFFSET U/V may be switched to what could be expected from AVI standard!
public:
  int GetPitch(int plane=0) const AVS_BakedCode( return AVS_LinkCall(GetPitch)(plane) )
  int GetRowSize(int plane=0) const AVS_BakedCode( return AVS_LinkCall(GetRowSize)(plane) )
  int GetHeight(int plane=0) const AVS_BakedCode( return AVS_LinkCall(GetHeight)(plane) )

  // generally you shouldn't use these three
  VideoFrameBuffer* GetFrameBuffer() const AVS_BakedCode( return AVS_LinkCall(GetFrameBuffer)() )
  int GetOffset(int plane=0) const AVS_BakedCode( return AVS_LinkCall(GetOffset)(plane) )

  // in plugins use env->SubFrame() -- because implementation code is only available inside avisynth.dll. Doh!
  VideoFrame* Subframe(int rel_offset, int new_pitch, int new_row_size, int new_height) const;
  VideoFrame* Subframe(int rel_offset, int new_pitch, int new_row_size, int new_height, int rel_offsetU, int rel_offsetV, int pitchUV) const;
  // for Alpha
  VideoFrame* Subframe(int rel_offset, int new_pitch, int new_row_size, int new_height, int rel_offsetU, int rel_offsetV, int pitchUV, int rel_offsetA) const;

  const BYTE* GetReadPtr(int plane=0) const AVS_BakedCode( return AVS_LinkCall(VFGetReadPtr)(plane) )
  bool IsWritable() const AVS_BakedCode( return AVS_LinkCall(IsWritable)() )
  BYTE* GetWritePtr(int plane=0) const AVS_BakedCode( return AVS_LinkCall(VFGetWritePtr)(plane) )

  AVSMap& getProperties() AVS_BakedCode(return AVS_LinkCallOptDefault(getProperties, (AVSMap&)*(AVSMap*)0))
  const AVSMap& getConstProperties() AVS_BakedCode(return AVS_LinkCallOptDefault(getConstProperties, (const AVSMap&)*(const AVSMap*)0))
  void setProperties(const AVSMap & _properties) AVS_BakedCode(AVS_LinkCall_Void(setProperties)(_properties))

  PDevice GetDevice() const AVS_BakedCode(return AVS_LinkCall(VideoFrame_GetDevice)())

  // 0: OK, 1: NG, -1: disabled or non CPU frame
  int CheckMemory() const AVS_BakedCode(return AVS_LinkCall(VideoFrame_CheckMemory)())

  ~VideoFrame() AVS_BakedCode( AVS_LinkCall_Void(VideoFrame_DESTRUCTOR)() )
#ifdef BUILDING_AVSCORE
public:
  void DESTRUCTOR();  /* Damn compiler won't allow taking the address of reserved constructs, make a dummy interlude */
#endif

// Ensure VideoFrame cannot be publicly assigned
private:
    VideoFrame& operator=(const VideoFrame&);

}; // end class VideoFrame

enum CachePolicyHint {
  // Values 0 to 5 are reserved for old 2.5 plugins
  // do not use them in new plugins

  // New 2.6 explicitly defined cache hints.
  CACHE_NOTHING=10, // Do not cache video.
  CACHE_WINDOW=11, // Hard protect upto X frames within a range of X from the current frame N.
  CACHE_GENERIC=12, // LRU cache upto X frames.
  CACHE_FORCE_GENERIC=13, // LRU cache upto X frames, override any previous CACHE_WINDOW.

  CACHE_GET_POLICY=30, // Get the current policy.
  CACHE_GET_WINDOW=31, // Get the current window h_span.
  CACHE_GET_RANGE=32, // Get the current generic frame range.

  CACHE_AUDIO=50, // Explicitly cache audio, X byte cache.
  CACHE_AUDIO_NOTHING=51, // Explicitly do not cache audio.
  CACHE_AUDIO_NONE=52, // Audio cache off (auto mode), X byte intial cache.
  CACHE_AUDIO_AUTO=53, // Audio cache on (auto mode), X byte intial cache.

  CACHE_GET_AUDIO_POLICY=70, // Get the current audio policy.
  CACHE_GET_AUDIO_SIZE=71, // Get the current audio cache size.

  CACHE_PREFETCH_FRAME=100, // Queue request to prefetch frame N.
  CACHE_PREFETCH_GO=101, // Action video prefetches.

  CACHE_PREFETCH_AUDIO_BEGIN=120, // Begin queue request transaction to prefetch audio (take critical section).
  CACHE_PREFETCH_AUDIO_STARTLO=121, // Set low 32 bits of start.
  CACHE_PREFETCH_AUDIO_STARTHI=122, // Set high 32 bits of start.
  CACHE_PREFETCH_AUDIO_COUNT=123, // Set low 32 bits of length.
  CACHE_PREFETCH_AUDIO_COMMIT=124, // Enqueue request transaction to prefetch audio (release critical section).
  CACHE_PREFETCH_AUDIO_GO=125, // Action audio prefetches.

  CACHE_GETCHILD_CACHE_MODE=200, // Cache ask Child for desired video cache mode.
  CACHE_GETCHILD_CACHE_SIZE=201, // Cache ask Child for desired video cache size.
  CACHE_GETCHILD_AUDIO_MODE=202, // Cache ask Child for desired audio cache mode.
  CACHE_GETCHILD_AUDIO_SIZE=203, // Cache ask Child for desired audio cache size.

  CACHE_GETCHILD_COST=220, // Cache ask Child for estimated processing cost.
    CACHE_COST_ZERO=221, // Child response of zero cost (ptr arithmetic only).
    CACHE_COST_UNIT=222, // Child response of unit cost (less than or equal 1 full frame blit).
    CACHE_COST_LOW=223, // Child response of light cost. (Fast)
    CACHE_COST_MED=224, // Child response of medium cost. (Real time)
    CACHE_COST_HI=225, // Child response of heavy cost. (Slow)

  CACHE_GETCHILD_THREAD_MODE=240, // Cache ask Child for thread safetyness.
    CACHE_THREAD_UNSAFE=241, // Only 1 thread allowed for all instances. 2.5 filters default!
    CACHE_THREAD_CLASS=242, // Only 1 thread allowed for each instance. 2.6 filters default!
    CACHE_THREAD_SAFE=243, //  Allow all threads in any instance.
    CACHE_THREAD_OWN=244, // Safe but limit to 1 thread, internally threaded.

  CACHE_GETCHILD_ACCESS_COST=260, // Cache ask Child for preferred access pattern.
    CACHE_ACCESS_RAND=261, // Filter is access order agnostic.
    CACHE_ACCESS_SEQ0=262, // Filter prefers sequential access (low cost)
    CACHE_ACCESS_SEQ1=263, // Filter needs sequential access (high cost)

  CACHE_AVSPLUS_CONSTANTS = 500,    // Smaller values are reserved for classic Avisynth

  CACHE_DONT_CACHE_ME,              // Filters that don't need caching (eg. trim, cache etc.) should return 1 to this request
  CACHE_SET_MIN_CAPACITY,
  CACHE_SET_MAX_CAPACITY,
  CACHE_GET_MIN_CAPACITY,
  CACHE_GET_MAX_CAPACITY,
  CACHE_GET_SIZE,
  CACHE_GET_REQUESTED_CAP,
  CACHE_GET_CAPACITY,
  CACHE_GET_MTMODE,

  CACHE_IS_CACHE_REQ,
  CACHE_IS_CACHE_ANS,
  CACHE_IS_MTGUARD_REQ,
  CACHE_IS_MTGUARD_ANS,

  CACHE_AVSPLUS_CUDA_CONSTANTS = 600,

  CACHE_GET_DEV_TYPE,           // Device types a filter can return
  CACHE_GET_CHILD_DEV_TYPE,    // Device types a fitler can receive

  CACHE_USER_CONSTANTS = 1000       // Smaller values are reserved for the core

};

// Base class for all filters.
class IClip {
  friend class PClip;
  friend class AVSValue;
  volatile long refcnt;
  void AddRef();
#if BUILDING_AVSCORE
public:
#endif
  void Release();
public:
  IClip() : refcnt(0) {}
  virtual int __stdcall GetVersion() { return AVISYNTH_INTERFACE_VERSION; }
  virtual PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) = 0;
  virtual bool __stdcall GetParity(int n) = 0;  // return field parity if field_based, else parity of first field in frame
  virtual void __stdcall GetAudio(void* buf, int64_t start, int64_t count, IScriptEnvironment* env) = 0;  // start and count are in samples
  /* Need to check GetVersion first, pre v5 will return random crap from EAX reg. */
  virtual int __stdcall SetCacheHints(int cachehints,int frame_range) = 0 ;  // We do not pass cache requests upwards, only to the next filter.
  virtual const VideoInfo& __stdcall GetVideoInfo() = 0;
  virtual ~IClip() {}
}; // end class IClip


// smart pointer to IClip
class PClip {

  IClip* p;

  IClip* GetPointerWithAddRef() const;
  friend class AVSValue;
  friend class VideoFrame;

  void Init(IClip* x);
  void Set(IClip* x);

public:
  PClip() AVS_BakedCode( AVS_LinkCall_Void(PClip_CONSTRUCTOR0)() )
  PClip(const PClip& x) AVS_BakedCode( AVS_LinkCall_Void(PClip_CONSTRUCTOR1)(x) )
  PClip(IClip* x) AVS_BakedCode( AVS_LinkCall_Void(PClip_CONSTRUCTOR2)(x) )
  void operator=(IClip* x) AVS_BakedCode( AVS_LinkCall_Void(PClip_OPERATOR_ASSIGN0)(x) )
  void operator=(const PClip& x) AVS_BakedCode( AVS_LinkCall_Void(PClip_OPERATOR_ASSIGN1)(x) )

  IClip* operator->() const { return p; }

  // useful in conditional expressions
  operator void*() const { return p; }
  bool operator!() const { return !p; }

  ~PClip() AVS_BakedCode( AVS_LinkCall_Void(PClip_DESTRUCTOR)() )
#ifdef BUILDING_AVSCORE
public:
  void CONSTRUCTOR0();  /* Damn compiler won't allow taking the address of reserved constructs, make a dummy interlude */
  void CONSTRUCTOR1(const PClip& x);
  void CONSTRUCTOR2(IClip* x);
  void OPERATOR_ASSIGN0(IClip* x);
  void OPERATOR_ASSIGN1(const PClip& x);
  void DESTRUCTOR();
#endif
}; // end class PClip

// enums for frame property functions
enum AVSPropTypes {
  PROPTYPE_UNSET = 'u', // ptUnset
  PROPTYPE_INT = 'i', // peType
  PROPTYPE_FLOAT = 'f', // ptFloat
  PROPTYPE_DATA = 's', // ptData
  PROPTYPE_CLIP = 'c', // ptClip
  PROPTYPE_FRAME = 'v' // ptFrame
  //  ptFunction = 'm'
};

enum AVSGetPropErrors {
  GETPROPERROR_UNSET = 1, // peUnset
  GETPROPERROR_TYPE = 2, // peType
  GETPROPERROR_INDEX = 4 // peIndex
};

enum AVSPropAppendMode {
  PROPAPPENDMODE_REPLACE = 0, // paReplace
  PROPAPPENDMODE_APPEND = 1, // paAppend
  PROPAPPENDMODE_TOUCH = 2 // paTouch
};


class AVSValue {
public:

  AVSValue() AVS_BakedCode( AVS_LinkCall_Void(AVSValue_CONSTRUCTOR0)() )
  AVSValue(IClip* c) AVS_BakedCode( AVS_LinkCall_Void(AVSValue_CONSTRUCTOR1)(c) )
  AVSValue(const PClip& c) AVS_BakedCode( AVS_LinkCall_Void(AVSValue_CONSTRUCTOR2)(c) )
  AVSValue(bool b) AVS_BakedCode( AVS_LinkCall_Void(AVSValue_CONSTRUCTOR3)(b) )
  AVSValue(int i) AVS_BakedCode( AVS_LinkCall_Void(AVSValue_CONSTRUCTOR4)(i) )
//  AVSValue(int64_t l);
  AVSValue(float f) AVS_BakedCode( AVS_LinkCall_Void(AVSValue_CONSTRUCTOR5)(f) )
  AVSValue(double f) AVS_BakedCode( AVS_LinkCall_Void(AVSValue_CONSTRUCTOR6)(f) )
  AVSValue(const char* s) AVS_BakedCode( AVS_LinkCall_Void(AVSValue_CONSTRUCTOR7)(s) )
  AVSValue(const AVSValue* a, int size) AVS_BakedCode( AVS_LinkCall_Void(AVSValue_CONSTRUCTOR8)(a, size) )
  AVSValue(const AVSValue& a, int size) AVS_BakedCode( AVS_LinkCall_Void(AVSValue_CONSTRUCTOR8)(&a, size) )
  AVSValue(const AVSValue& v) AVS_BakedCode( AVS_LinkCall_Void(AVSValue_CONSTRUCTOR9)(v) )
  AVSValue(const PFunction& n) AVS_BakedCode(AVS_LinkCall_Void(AVSValue_CONSTRUCTOR11)(n))

  ~AVSValue() AVS_BakedCode( AVS_LinkCall_Void(AVSValue_DESTRUCTOR)() )
  AVSValue& operator=(const AVSValue& v) AVS_BakedCode( return AVS_LinkCallV(AVSValue_OPERATOR_ASSIGN)(v) )

  // Note that we transparently allow 'int' to be treated as 'float'.
  // There are no int<->bool conversions, though.

  bool Defined() const AVS_BakedCode( return AVS_LinkCall(Defined)() )
  bool IsClip() const AVS_BakedCode( return AVS_LinkCall(IsClip)() )
  bool IsBool() const AVS_BakedCode( return AVS_LinkCall(IsBool)() )
  bool IsInt() const AVS_BakedCode( return AVS_LinkCall(IsInt)() )
//  bool IsLong() const;
  bool IsFloat() const AVS_BakedCode( return AVS_LinkCall(IsFloat)() )
  bool IsString() const AVS_BakedCode( return AVS_LinkCall(IsString)() )
  bool IsArray() const AVS_BakedCode(return AVS_LinkCall(IsArray)())
  bool IsFunction() const AVS_BakedCode( return AVS_LinkCall(IsFunction)() )

  PClip AsClip() const AVS_BakedCode( return AVS_LinkCall(AsClip)() )
  bool AsBool() const AVS_BakedCode( return AVS_LinkCall(AsBool1)() )
  int AsInt() const AVS_BakedCode( return AVS_LinkCall(AsInt1)() )
//  int AsLong() const;
  const char* AsString() const AVS_BakedCode( return AVS_LinkCall(AsString1)() )
  double AsFloat() const AVS_BakedCode( return AVS_LinkCall(AsFloat1)() )
  float AsFloatf() const AVS_BakedCode( return float( AVS_LinkCall(AsFloat1)() ) )

  bool AsBool(bool def) const AVS_BakedCode( return AVS_LinkCall(AsBool2)(def) )
  int AsInt(int def) const AVS_BakedCode( return AVS_LinkCall(AsInt2)(def) )
  double AsDblDef(double def) const AVS_BakedCode( return AVS_LinkCall(AsDblDef)(def) ) // Value is still a float
  double AsFloat(float def) const AVS_BakedCode( return AVS_LinkCall(AsFloat2)(def) )
  float AsFloatf(float def) const AVS_BakedCode( return float( AVS_LinkCall(AsFloat2)(def) ) )
  const char* AsString(const char* def) const AVS_BakedCode( return AVS_LinkCall(AsString2)(def) )
  PFunction AsFunction() const; // internal use only

  int ArraySize() const AVS_BakedCode( return AVS_LinkCall(ArraySize)() )

  const AVSValue& operator[](int index) const AVS_BakedCode( return AVS_LinkCallV(AVSValue_OPERATOR_INDEX)(index) )

private:

  short type;  // 'a'rray, 'c'lip, 'b'ool, 'i'nt, 'f'loat, 's'tring, 'v'oid, fu'n'ction, or RFU: 'l'ong ('d'ouble)
  short array_size;
  union {
    IClip* clip;
    bool boolean;
    int integer;
    float floating_pt;
    const char* string;
    const AVSValue* array;
    IFunction* function;
    #ifdef X86_64
    // if ever, only x64 will support. It breaks struct size on 32 bit
    int64_t longlong; // 8 bytes
    double double_pt; // 8 bytes
    #endif
  };

  void Assign(const AVSValue* src, bool init);
#ifdef BUILDING_AVSCORE
public:
  void            CONSTRUCTOR0();  /* Damn compiler won't allow taking the address of reserved constructs, make a dummy interlude */
  void            CONSTRUCTOR1(IClip* c);
  void            CONSTRUCTOR2(const PClip& c);
  void            CONSTRUCTOR3(bool b);
  void            CONSTRUCTOR4(int i);
  void            CONSTRUCTOR5(float f);
  void            CONSTRUCTOR6(double f);
  void            CONSTRUCTOR7(const char* s);
  void            CONSTRUCTOR8(const AVSValue* a, int size);
  void            CONSTRUCTOR9(const AVSValue& v);
  void            CONSTRUCTOR11(const PFunction& n);
  void            DESTRUCTOR();
  AVSValue&       OPERATOR_ASSIGN(const AVSValue& v);
  const AVSValue& OPERATOR_INDEX(int index) const;

  bool            AsBool1() const;
  int             AsInt1() const;
  const char*     AsString1() const;
  double          AsFloat1() const;

  bool            AsBool2(bool def) const;
  int             AsInt2(int def) const;
  double          AsFloat2(float def) const;
  const char*     AsString2(const char* def) const;

#ifdef NEW_AVSVALUE
  void            MarkArrayAsC(); // for C interface, no deep-copy and deep-free
  void            CONSTRUCTOR10(const AVSValue& v, bool c_arrays);
  AVSValue(const AVSValue& v, bool c_arrays);
  void            Assign2(const AVSValue* src, bool init, bool c_arrays);
#endif

#endif
}; // end class AVSValue

#define AVS_UNUSED(x) (void)(x)

// instantiable null filter
class GenericVideoFilter : public IClip {
protected:
  PClip child;
  VideoInfo vi;
public:
  GenericVideoFilter(PClip _child) : child(_child) { vi = child->GetVideoInfo(); }
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) { return child->GetFrame(n, env); }
  void __stdcall GetAudio(void* buf, int64_t start, int64_t count, IScriptEnvironment* env) { child->GetAudio(buf, start, count, env); }
  const VideoInfo& __stdcall GetVideoInfo() { return vi; }
  bool __stdcall GetParity(int n) { return child->GetParity(n); }
  int __stdcall SetCacheHints(int cachehints, int frame_range) { AVS_UNUSED(cachehints); AVS_UNUSED(frame_range); return 0; };  // We do not pass cache requests upwards, only to the next filter.
};


class PFunction
{
public:
  PFunction() AVS_BakedCode(AVS_LinkCall_Void(PFunction_CONSTRUCTOR0)())
  PFunction(IFunction* p) AVS_BakedCode(AVS_LinkCall_Void(PFunction_CONSTRUCTOR1)(p))
  PFunction(const PFunction& p) AVS_BakedCode(AVS_LinkCall_Void(PFunction_CONSTRUCTOR2)(p))
  PFunction& operator=(IFunction* p) AVS_BakedCode(return AVS_LinkCallV(PFunction_OPERATOR_ASSIGN0)(p))
  PFunction& operator=(const PFunction& p) AVS_BakedCode(return AVS_LinkCallV(PFunction_OPERATOR_ASSIGN1)(p))
  ~PFunction() AVS_BakedCode(AVS_LinkCall_Void(PFunction_DESTRUCTOR)())

  int operator!() const { return !e; }
  operator void*() const { return e; }
  IFunction* operator->() const { return e; }

private:
  IFunction * e;

  friend class AVSValue;
  IFunction * GetPointerWithAddRef() const;
  void Init(IFunction* p);
  void Set(IFunction* p);

#ifdef BUILDING_AVSCORE
public:
  void CONSTRUCTOR0();  /* Damn compiler won't allow taking the address of reserved constructs, make a dummy interlude */
  void CONSTRUCTOR1(IFunction* p);
  void CONSTRUCTOR2(const PFunction& p);
  PFunction& OPERATOR_ASSIGN0(IFunction* p);
  PFunction& OPERATOR_ASSIGN1(const PFunction& p);
  void DESTRUCTOR();
#endif
};


#undef CALL_MEMBER_FN
#undef AVS_LinkCallOptDefault
#undef AVS_LinkCallOpt
#undef AVS_LinkCallV
#undef AVS_LinkCall
#undef AVS_BakedCode


#include "avs/cpuid.h"

// IScriptEnvironment GetEnvProperty
enum AvsEnvProperty
{
  AEP_PHYSICAL_CPUS = 1,
  AEP_LOGICAL_CPUS = 2,
  AEP_THREADPOOL_THREADS = 3,
  AEP_FILTERCHAIN_THREADS = 4,
  AEP_THREAD_ID = 5,
  AEP_VERSION = 6,

  // Neo additionals
  AEP_NUM_DEVICES = 901,
  AEP_FRAME_ALIGN = 902,
  AEP_PLANE_ALIGN = 903,

  AEP_SUPPRESS_THREAD = 921,
  AEP_GETFRAME_RECURSIVE = 922,
};

// IScriptEnvironment Allocate
enum AvsAllocType
{
  AVS_NORMAL_ALLOC = 1,
  AVS_POOLED_ALLOC = 2
};


class IScriptEnvironment {
public:
  virtual ~IScriptEnvironment() {}

  virtual /*static*/ int __stdcall GetCPUFlags() = 0;

  virtual char* __stdcall SaveString(const char* s, int length = -1) = 0;
  virtual char* Sprintf(const char* fmt, ...) = 0;
  // note: val is really a va_list; I hope everyone typedefs va_list to a pointer
  // 20200305: (void *) changed back to va_list
  virtual char* __stdcall VSprintf(const char* fmt, va_list val) = 0;

#ifdef AVS_WINDOWS
  __declspec(noreturn) virtual void ThrowError(const char* fmt, ...) = 0;
#else
  virtual void ThrowError(const char* fmt, ...) = 0;
#endif

  class NotFound /*exception*/ {};  // thrown by Invoke and GetVar

  typedef AVSValue (__cdecl *ApplyFunc)(AVSValue args, void* user_data, IScriptEnvironment* env);

  virtual void __stdcall AddFunction(const char* name, const char* params, ApplyFunc apply, void* user_data) = 0;
  virtual bool __stdcall FunctionExists(const char* name) = 0;
  virtual AVSValue __stdcall Invoke(const char* name, const AVSValue args, const char* const* arg_names=0) = 0;

  virtual AVSValue __stdcall GetVar(const char* name) = 0;
  virtual bool __stdcall SetVar(const char* name, const AVSValue& val) = 0;
  virtual bool __stdcall SetGlobalVar(const char* name, const AVSValue& val) = 0;

  virtual void __stdcall PushContext(int level=0) = 0;
  virtual void __stdcall PopContext() = 0;

  // note v8: deprecated in most cases, use NewVideoFrameP is possible
  virtual PVideoFrame __stdcall NewVideoFrame(const VideoInfo& vi, int align=FRAME_ALIGN) = 0;

  virtual bool __stdcall MakeWritable(PVideoFrame* pvf) = 0;

  virtual void __stdcall BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height) = 0;

  typedef void (__cdecl *ShutdownFunc)(void* user_data, IScriptEnvironment* env);
  virtual void __stdcall AtExit(ShutdownFunc function, void* user_data) = 0;

  virtual void __stdcall CheckVersion(int version = AVISYNTH_INTERFACE_VERSION) = 0;

  virtual PVideoFrame __stdcall Subframe(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size, int new_height) = 0;

  virtual int __stdcall SetMemoryMax(int mem) = 0;

  virtual int __stdcall SetWorkingDir(const char * newdir) = 0;

  virtual void* __stdcall ManageCache(int key, void* data) = 0;

  enum PlanarChromaAlignmentMode {
            PlanarChromaAlignmentOff,
            PlanarChromaAlignmentOn,
            PlanarChromaAlignmentTest };

  virtual bool __stdcall PlanarChromaAlignment(PlanarChromaAlignmentMode key) = 0;

  virtual PVideoFrame __stdcall SubframePlanar(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size,
                                               int new_height, int rel_offsetU, int rel_offsetV, int new_pitchUV) = 0;

  // **** AVISYNTH_INTERFACE_VERSION 5 **** defined since classic Avisynth 2.6 beta
  virtual void __stdcall DeleteScriptEnvironment() = 0;

  virtual void __stdcall ApplyMessage(PVideoFrame* frame, const VideoInfo& vi, const char* message, int size,
                                     int textcolor, int halocolor, int bgcolor) = 0;

  virtual const AVS_Linkage* __stdcall GetAVSLinkage() = 0;

  // **** AVISYNTH_INTERFACE_VERSION 6 **** defined since classic Avisynth 2.6
  // noThrow version of GetVar
  virtual AVSValue __stdcall GetVarDef(const char* name, const AVSValue& def = AVSValue()) = 0;

  // **** AVISYNTH_INTERFACE_VERSION 8 **** AviSynth+ 3.6.0-
  virtual PVideoFrame __stdcall SubframePlanarA(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size,
    int new_height, int rel_offsetU, int rel_offsetV, int new_pitchUV, int rel_offsetA) = 0;

  virtual void __stdcall copyFrameProps(const PVideoFrame& src, PVideoFrame& dst) = 0;
  virtual const AVSMap* __stdcall getFramePropsRO(const PVideoFrame& frame) = 0;
  virtual AVSMap* __stdcall getFramePropsRW(PVideoFrame& frame) = 0;

  virtual int __stdcall propNumKeys(const AVSMap* map) = 0;

  virtual const char* __stdcall propGetKey(const AVSMap* map, int index) = 0;
  virtual int __stdcall propNumElements(const AVSMap* map, const char* key) = 0;
  virtual char __stdcall propGetType(const AVSMap* map, const char* key) = 0;

  virtual int64_t __stdcall propGetInt(const AVSMap* map, const char* key, int index, int* error) = 0;
  virtual double __stdcall propGetFloat(const AVSMap* map, const char* key, int index, int* error) = 0;
  virtual const char* __stdcall propGetData(const AVSMap* map, const char* key, int index, int* error) = 0;
  virtual int __stdcall propGetDataSize(const AVSMap* map, const char* key, int index, int* error) = 0;
  virtual PClip __stdcall propGetClip(const AVSMap* map, const char* key, int index, int* error) = 0;
  virtual const PVideoFrame __stdcall propGetFrame(const AVSMap* map, const char* key, int index, int* error) = 0;

  virtual int __stdcall propDeleteKey(AVSMap* map, const char* key) = 0;

  virtual int __stdcall propSetInt(AVSMap* map, const char* key, int64_t i, int append) = 0;
  virtual int __stdcall propSetFloat(AVSMap* map, const char* key, double d, int append) = 0;
  virtual int __stdcall propSetData(AVSMap* map, const char* key, const char* d, int length, int append) = 0;
  virtual int __stdcall propSetClip(AVSMap* map, const char* key, PClip& clip, int append) = 0;
  virtual int __stdcall propSetFrame(AVSMap* map, const char* key, const PVideoFrame& frame, int append) = 0;

  virtual const int64_t* __stdcall propGetIntArray(const AVSMap* map, const char* key, int* error) = 0;
  virtual const double* __stdcall propGetFloatArray(const AVSMap* map, const char* key, int* error) = 0;
  virtual int __stdcall propSetIntArray(AVSMap* map, const char* key, const int64_t* i, int size) = 0;
  virtual int __stdcall propSetFloatArray(AVSMap* map, const char* key, const double* d, int size) = 0;

  virtual AVSMap* __stdcall createMap() = 0;
  virtual void __stdcall freeMap(AVSMap* map) = 0;
  virtual void __stdcall clearMap(AVSMap* map) = 0;

  // NewVideoFrame with frame property source.
  virtual PVideoFrame __stdcall NewVideoFrameP(const VideoInfo& vi, PVideoFrame* propSrc, int align = FRAME_ALIGN) = 0;

  // Note: do not declare existing names like 'NewVideoFrame' again with different parameters since MSVC will reorder it
  // in the vtable and group it together with the first NewVideoFrame variant.
  // This results in shifting all vtable method pointers after NewVideoFrame and breaks all plugins who expect the old order.
  // E.g. ApplyMessage will be called instead of GetAVSLinkage

  // Generic query to ask for various system properties
  virtual size_t  __stdcall GetEnvProperty(AvsEnvProperty prop) = 0;

  // Support functions
  virtual void* __stdcall Allocate(size_t nBytes, size_t alignment, AvsAllocType type) = 0;
  virtual void __stdcall Free(void* ptr) = 0;

  // these GetVar versions (renamed differently) were moved from IScriptEnvironment2

  // Returns TRUE and the requested variable. If the method fails, returns FALSE and does not touch 'val'.
  virtual bool  __stdcall GetVarTry(const char* name, AVSValue* val) const = 0; // ex virtual bool  __stdcall GetVar(const char* name, AVSValue* val) const = 0;
  // Return the value of the requested variable.
  // If the variable was not found or had the wrong type,
  // return the supplied default value.
  virtual bool __stdcall GetVarBool(const char* name, bool def) const = 0;
  virtual int  __stdcall GetVarInt(const char* name, int def) const = 0;
  virtual double  __stdcall GetVarDouble(const char* name, double def) const = 0;
  virtual const char* __stdcall GetVarString(const char* name, const char* def) const = 0;
  // brand new in v8 - though no real int64 support yet
  virtual int64_t __stdcall GetVarLong(const char* name, int64_t def) const = 0;

  // 'Invoke' functions moved here from internal ScriptEnvironments are renamed in order to keep vtable order
  // Invoke functions with 'Try' will return false instead of throwing NotFound().
  // ex-IScriptEnvironment2
  virtual bool __stdcall InvokeTry(AVSValue* result, const char* name, const AVSValue& args, const char* const* arg_names = 0) = 0;
  // Since V8
  virtual AVSValue __stdcall Invoke2(const AVSValue& implicit_last, const char* name, const AVSValue args, const char* const* arg_names = 0) = 0;
  // Ex-INeo
  virtual bool __stdcall Invoke2Try(AVSValue* result, const AVSValue& implicit_last, const char* name, const AVSValue args, const char* const* arg_names = 0) = 0;
  virtual AVSValue __stdcall Invoke3(const AVSValue& implicit_last, const PFunction& func, const AVSValue args, const char* const* arg_names = 0) = 0;
  virtual bool __stdcall Invoke3Try(AVSValue* result, const AVSValue& implicit_last, const PFunction& func, const AVSValue args, const char* const* arg_names = 0) = 0;

}; // end class IScriptEnvironment

// used internally
class IScriptEnvironment_Avs25 {
public:
  virtual ~IScriptEnvironment_Avs25() {}

  virtual /*static*/ int __stdcall GetCPUFlags() = 0;

  virtual char* __stdcall SaveString(const char* s, int length = -1) = 0;
  virtual char* Sprintf(const char* fmt, ...) = 0;
  virtual char* __stdcall VSprintf(const char* fmt, va_list val) = 0;

#ifdef AVS_WINDOWS
  __declspec(noreturn) virtual void ThrowError(const char* fmt, ...) = 0;
#else
  virtual void ThrowError(const char* fmt, ...) = 0;
#endif

  class NotFound /*exception*/ {};  // thrown by Invoke and GetVar

  typedef AVSValue(__cdecl* ApplyFunc)(AVSValue args, void* user_data, IScriptEnvironment* env);

  virtual void __stdcall AddFunction25(const char* name, const char* params, ApplyFunc apply, void* user_data) = 0;
  virtual bool __stdcall FunctionExists(const char* name) = 0;
  virtual AVSValue __stdcall Invoke25(const char* name, const AVSValue args, const char* const* arg_names = 0) = 0;

  virtual AVSValue __stdcall GetVar(const char* name) = 0;
  virtual bool __stdcall SetVar(const char* name, const AVSValue& val) = 0;
  virtual bool __stdcall SetGlobalVar(const char* name, const AVSValue& val) = 0;

  virtual void __stdcall PushContext(int level = 0) = 0;
  virtual void __stdcall PopContext() = 0;

  virtual PVideoFrame __stdcall NewVideoFrame(const VideoInfo& vi, int align = FRAME_ALIGN) = 0;

  virtual bool __stdcall MakeWritable(PVideoFrame* pvf) = 0;

  virtual void __stdcall BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height) = 0;

  typedef void(__cdecl* ShutdownFunc)(void* user_data, IScriptEnvironment* env);
  virtual void __stdcall AtExit(ShutdownFunc function, void* user_data) = 0;

  virtual void __stdcall CheckVersion(int version = AVISYNTH_CLASSIC_INTERFACE_VERSION_25) = 0;

  virtual PVideoFrame __stdcall Subframe(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size, int new_height) = 0;

  virtual int __stdcall SetMemoryMax(int mem) = 0;

  virtual int __stdcall SetWorkingDir(const char* newdir) = 0;

  // specially returns 1 for key MC_QueryAvs25 to check if called from AVS2.5 interface
  virtual void* __stdcall ManageCache25(int key, void* data) = 0;

  enum PlanarChromaAlignmentMode {
    PlanarChromaAlignmentOff,
    PlanarChromaAlignmentOn,
    PlanarChromaAlignmentTest
  };

  virtual bool __stdcall PlanarChromaAlignment(IScriptEnvironment::PlanarChromaAlignmentMode key) = 0;

  virtual PVideoFrame __stdcall SubframePlanar(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size,
    int new_height, int rel_offsetU, int rel_offsetV, int new_pitchUV) = 0;

  // Despite the name, we provide entries up to V6 in case someone requests
  // a V3 interface and still wants to use V5-V6 functions

  // **** AVISYNTH_INTERFACE_VERSION 5 **** defined since classic Avisynth 2.6 beta
  virtual void __stdcall DeleteScriptEnvironment() = 0;

  virtual void __stdcall ApplyMessage(PVideoFrame* frame, const VideoInfo& vi, const char* message, int size,
    int textcolor, int halocolor, int bgcolor) = 0;

  virtual const AVS_Linkage* __stdcall GetAVSLinkage() = 0;

  // **** AVISYNTH_INTERFACE_VERSION 6 **** defined since classic Avisynth 2.6
  // noThrow version of GetVar
  virtual AVSValue __stdcall GetVarDef(const char* name, const AVSValue& def = AVSValue()) = 0;

}; // end class IScriptEnvironment_Avs25


enum MtMode
{
  MT_INVALID = 0,
  MT_NICE_FILTER = 1,
  MT_MULTI_INSTANCE = 2,
  MT_SERIALIZED = 3,
  MT_SPECIAL_MT = 4,
  MT_MODE_COUNT = 5
};

class IJobCompletion
{
public:

  virtual ~IJobCompletion() {}
  virtual void __stdcall Wait() = 0;
  virtual AVSValue __stdcall Get(size_t i) = 0;
  virtual size_t __stdcall Size() const = 0;
  virtual size_t __stdcall Capacity() const = 0;
  virtual void __stdcall Reset() = 0;
  virtual void __stdcall Destroy() = 0;
};

class IScriptEnvironment2;
class Prefetcher;
typedef AVSValue (*ThreadWorkerFuncPtr)(IScriptEnvironment2* env, void* data);


/* -----------------------------------------------------------------------------
   Note to plugin authors: The interface in IScriptEnvironment2 is
      preliminary / under construction / only for testing / non-final etc.!
      As long as you see this note here, IScriptEnvironment2 might still change,
      in which case your plugin WILL break. This also means that you are welcome
      to test it and give your feedback about any ideas, improvements, or issues
      you might have.
   ----------------------------------------------------------------------------- */
class IScriptEnvironment2 : public IScriptEnvironment{
public:
  virtual ~IScriptEnvironment2() {}

  // V8: SubframePlanarA, GetEnvProperty, GetVar versions, Allocate, Free, no-throw Invoke moved to IScriptEnvironment

  // Plugin functions
  virtual bool __stdcall LoadPlugin(const char* filePath, bool throwOnError, AVSValue *result) = 0;
  virtual void __stdcall AddAutoloadDir(const char* dirPath, bool toFront) = 0;
  virtual void __stdcall ClearAutoloadDirs() = 0;
  virtual void __stdcall AutoloadPlugins() = 0;
  virtual void __stdcall AddFunction(const char* name, const char* params, ApplyFunc apply, void* user_data, const char *exportVar) = 0;
  virtual bool __stdcall InternalFunctionExists(const char* name) = 0;

  // Threading
  virtual void __stdcall SetFilterMTMode(const char* filter, MtMode mode, bool force) = 0; // If filter is "DEFAULT_MT_MODE", sets the default MT mode
  virtual IJobCompletion* __stdcall NewCompletion(size_t capacity) = 0;
  virtual void __stdcall ParallelJob(ThreadWorkerFuncPtr jobFunc, void* jobData, IJobCompletion* completion) = 0;

  // These lines are needed so that we can overload the older functions from IScriptEnvironment.
  using IScriptEnvironment::Invoke;
  using IScriptEnvironment::AddFunction;

}; // end class IScriptEnvironment2


// To allow Avisynth+ add functions to IScriptEnvironment2,
// Neo defines another new interface, INeoEnv.
// INeoEnv and the legacy interfaces (IScriptEnvironment/IScriptEnvironment2)
// share the same ScriptEnvironment instance. The function with the same signature
// is exactly identical and there is no limitation to switch interfaces.
// You can use any interface you like.
class INeoEnv {
public:
  virtual ~INeoEnv() {}

  typedef IScriptEnvironment::NotFound NotFound;
  typedef IScriptEnvironment::ApplyFunc ApplyFunc;
  typedef IScriptEnvironment::ShutdownFunc ShutdownFunc;

  virtual void __stdcall DeleteScriptEnvironment() = 0;

  virtual const AVS_Linkage* __stdcall GetAVSLinkage() = 0;

  // Get legacy interface (Avisynth+)
  virtual IScriptEnvironment2* __stdcall GetEnv2() = 0;
  // Get compatibility interface for AVS CPP 2.5 plugins
  virtual IScriptEnvironment_Avs25* __stdcall GetEnv25() = 0;

  // Generic system to ask for various properties
  virtual size_t  __stdcall GetEnvProperty(AvsEnvProperty prop) = 0;
#ifdef INTEL_INTRINSICS
  virtual int __stdcall GetCPUFlags() = 0;
#endif

  // Plugin functions
  virtual bool __stdcall LoadPlugin(const char* filePath, bool throwOnError, AVSValue *result) = 0;
  virtual void __stdcall AddAutoloadDir(const char* dirPath, bool toFront) = 0;
  virtual void __stdcall ClearAutoloadDirs() = 0;
  virtual void __stdcall AutoloadPlugins() = 0;

  virtual void __stdcall AddFunction(
    const char* name, const char* params, ApplyFunc apply, void* user_data) = 0;
  virtual void __stdcall AddFunction(
    const char* name, const char* params, ApplyFunc apply, void* user_data, const char *exportVar) = 0;
  virtual bool __stdcall FunctionExists(const char* name) = 0;
  virtual bool __stdcall InternalFunctionExists(const char* name) = 0;

  // Invoke function. Throws NotFound exception when the specified function does not exist.
  virtual AVSValue __stdcall Invoke(
    const char* name, const AVSValue args, const char* const* arg_names = 0) = 0;
  virtual AVSValue __stdcall Invoke2(
    const AVSValue& implicit_last, const char* name, const AVSValue args, const char* const* arg_names = 0) = 0;
  virtual AVSValue __stdcall Invoke3(
    const AVSValue& implicit_last,
    const PFunction& func, const AVSValue args, const char* const* arg_names = 0) = 0;

  // These versions of Invoke will return false instead of throwing NotFound().
  virtual bool __stdcall InvokeTry(
    AVSValue* result, const char* name, const AVSValue& args, const char* const* arg_names = 0) = 0;
  virtual bool __stdcall Invoke2Try(
    AVSValue* result, const AVSValue& implicit_last,
    const char* name, const AVSValue args, const char* const* arg_names = 0) = 0;
  virtual bool __stdcall Invoke3Try(
    AVSValue* result, const AVSValue& implicit_last,
    const PFunction& func, const AVSValue args, const char* const* arg_names = 0) = 0;

  // Throws exception when the requested variable is not found.
  virtual AVSValue __stdcall GetVar(const char* name) = 0;

  // noThrow version of GetVar
  virtual AVSValue __stdcall GetVarDef(const char* name, const AVSValue& def = AVSValue()) = 0;

  // Returns TRUE and the requested variable. If the method fails, returns FALSE and does not touch 'val'.
  virtual bool  __stdcall GetVarTry(const char* name, AVSValue* val) const = 0;

  // Return the value of the requested variable.
  // If the variable was not found or had the wrong type,
  // return the supplied default value.
  virtual bool __stdcall GetVarBool(const char* name, bool def) const = 0;
  virtual int  __stdcall GetVarInt(const char* name, int def) const = 0;
  virtual double  __stdcall GetVarDouble(const char* name, double def) const = 0;
  virtual const char* __stdcall GetVarString(const char* name, const char* def) const = 0;
  virtual int64_t __stdcall GetVarLong(const char* name, int64_t def) const = 0;

  virtual bool __stdcall SetVar(const char* name, const AVSValue& val) = 0;
  virtual bool __stdcall SetGlobalVar(const char* name, const AVSValue& val) = 0;

  // Switch local variables
  virtual void __stdcall PushContext(int level = 0) = 0;
  virtual void __stdcall PopContext() = 0;

  // Global variable frame support
  virtual void __stdcall PushContextGlobal() = 0;
  virtual void __stdcall PopContextGlobal() = 0;

  // Allocate new video frame
  // Align parameter is no longer supported
  virtual PVideoFrame __stdcall NewVideoFrame(const VideoInfo& vi) = 0; // current device is used
  virtual PVideoFrame __stdcall NewVideoFrame(const VideoInfo& vi, const PDevice& device) = 0;
  // as above but with property sources
  virtual PVideoFrame __stdcall NewVideoFrame(const VideoInfo& vi, PVideoFrame *propSrc) = 0; // current device is used + frame property source
  virtual PVideoFrame __stdcall NewVideoFrame(const VideoInfo& vi, const PDevice& device, PVideoFrame* propSrc) = 0; // current device is used + frame property source

  // Frame related operations
  virtual bool __stdcall MakeWritable(PVideoFrame* pvf) = 0;
  virtual void __stdcall BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height) = 0;

  virtual PVideoFrame __stdcall Subframe(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size, int new_height) = 0;
  virtual PVideoFrame __stdcall SubframePlanar(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size,
    int new_height, int rel_offsetU, int rel_offsetV, int new_pitchUV) = 0;
  virtual PVideoFrame __stdcall SubframePlanarA(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size,
    int new_height, int rel_offsetU, int rel_offsetV, int new_pitchUV, int rel_offsetA) = 0;

  // frame properties support
  virtual void __stdcall copyFrameProps(const PVideoFrame& src, PVideoFrame& dst) = 0;
  virtual const AVSMap* __stdcall getFramePropsRO(const PVideoFrame& frame) = 0;
  virtual AVSMap* __stdcall getFramePropsRW(PVideoFrame& frame) = 0;

  virtual int __stdcall propNumKeys(const AVSMap* map) = 0;
  virtual const char* __stdcall propGetKey(const AVSMap* map, int index) = 0;
  virtual int __stdcall propNumElements(const AVSMap* map, const char* key) = 0;
  virtual char __stdcall propGetType(const AVSMap* map, const char* key) = 0;

  virtual int64_t __stdcall propGetInt(const AVSMap* map, const char* key, int index, int* error) = 0;
  virtual double __stdcall propGetFloat(const AVSMap* map, const char* key, int index, int* error) = 0;
  virtual const char* __stdcall propGetData(const AVSMap* map, const char* key, int index, int* error) = 0;
  virtual int __stdcall propGetDataSize(const AVSMap* map, const char* key, int index, int* error) = 0;
  virtual PClip __stdcall propGetClip(const AVSMap* map, const char* key, int index, int* error) = 0;
  virtual const PVideoFrame __stdcall propGetFrame(const AVSMap* map, const char* key, int index, int* error) = 0;

  virtual int __stdcall propDeleteKey(AVSMap* map, const char* key) = 0;

  virtual int __stdcall propSetInt(AVSMap* map, const char* key, int64_t i, int append) = 0;
  virtual int __stdcall propSetFloat(AVSMap* map, const char* key, double d, int append) = 0;
  virtual int __stdcall propSetData(AVSMap* map, const char* key, const char* d, int length, int append) = 0;
  virtual int __stdcall propSetClip(AVSMap* map, const char* key, PClip& clip, int append) = 0;
  virtual int __stdcall propSetFrame(AVSMap* map, const char* key, const PVideoFrame& frame, int append) = 0;

  virtual const int64_t *__stdcall propGetIntArray(const AVSMap* map, const char* key, int* error) = 0;
  virtual const double *__stdcall propGetFloatArray(const AVSMap* map, const char* key, int* error) = 0;
  virtual int __stdcall propSetIntArray(AVSMap* map, const char* key, const int64_t* i, int size) = 0;
  virtual int __stdcall propSetFloatArray(AVSMap* map, const char* key, const double* d, int size) = 0;

  virtual AVSMap* __stdcall createMap() = 0;
  virtual void __stdcall freeMap(AVSMap* map) = 0;
  virtual void __stdcall clearMap(AVSMap* map) = 0;

  // Support functions
  virtual void* __stdcall Allocate(size_t nBytes, size_t alignment, AvsAllocType type) = 0;
  virtual void __stdcall Free(void* ptr) = 0;

  virtual char* __stdcall SaveString(const char* s, int length = -1) = 0;
  virtual char* __stdcall SaveString(const char* s, int length, bool escape) = 0;
  virtual char* Sprintf(const char* fmt, ...) = 0;
  virtual char* __stdcall VSprintf(const char* fmt, va_list val) = 0;

  __declspec(noreturn) virtual void ThrowError(const char* fmt, ...) = 0;

  virtual void __stdcall ApplyMessage(PVideoFrame* frame, const VideoInfo& vi, const char* message, int size,
    int textcolor, int halocolor, int bgcolor) = 0;

  // Setting
  virtual int __stdcall SetMemoryMax(int mem) = 0;
  virtual int __stdcall SetMemoryMax(AvsDeviceType type, int index, int mem) = 0;

  virtual bool __stdcall PlanarChromaAlignment(IScriptEnvironment::PlanarChromaAlignmentMode key) = 0;
  virtual int __stdcall SetWorkingDir(const char * newdir) = 0;
  virtual void* __stdcall ManageCache(int key, void* data) = 0;

  virtual void __stdcall AtExit(ShutdownFunc function, void* user_data) = 0;
  virtual void __stdcall CheckVersion(int version = AVISYNTH_INTERFACE_VERSION) = 0;

  // Threading
  virtual void __stdcall SetFilterMTMode(const char* filter, MtMode mode, bool force) = 0;
  virtual IJobCompletion* __stdcall NewCompletion(size_t capacity) = 0;
  virtual void __stdcall ParallelJob(ThreadWorkerFuncPtr jobFunc, void* jobData, IJobCompletion* completion) = 0;

  // CUDA Support
  virtual PDevice __stdcall GetDevice(AvsDeviceType dev_type, int dev_index) const = 0;
  virtual PDevice __stdcall GetDevice() const = 0; // get current device
  virtual AvsDeviceType __stdcall GetDeviceType() const = 0;
  virtual int __stdcall GetDeviceId() const  = 0;
  virtual int __stdcall GetDeviceIndex() const  = 0;
  virtual void* __stdcall GetDeviceStream() const = 0;
  virtual void __stdcall DeviceAddCallback(void(*cb)(void*), void* user_data) = 0;

  virtual PVideoFrame __stdcall GetFrame(PClip c, int n, const PDevice& device) = 0;

};

// support interface conversion
struct PNeoEnv {
  INeoEnv* p;
  PNeoEnv() : p() { }
  PNeoEnv(IScriptEnvironment* env)
#ifdef BUILDING_AVSCORE
    ;
#else
  : p(!AVS_linkage || offsetof(AVS_Linkage, GetNeoEnv) >= AVS_linkage->Size ? 0 : AVS_linkage->GetNeoEnv(env)) { }
#endif

  int operator!() const { return !p; }
  operator void*() const { return p; }
  INeoEnv* operator->() const { return p; }
#ifdef BUILDING_AVSCORE
  inline operator IScriptEnvironment2*();
  inline operator IScriptEnvironment_Avs25* ();
#else
  operator IScriptEnvironment2*() { return p->GetEnv2(); }
  operator IScriptEnvironment_Avs25* () { return p->GetEnv25(); }
#endif
};


// avisynth.dll exports this; it's a way to use it as a library, without
// writing an AVS script or without going through AVIFile.
AVSC_API(IScriptEnvironment*, CreateScriptEnvironment)(int version = AVISYNTH_INTERFACE_VERSION);


// These are some global variables you can set in your script to change AviSynth's behavior.
#define VARNAME_AllowFloatAudio   "OPT_AllowFloatAudio"   // Allow WAVE_FORMAT_IEEE_FLOAT audio output
#define VARNAME_VDubPlanarHack    "OPT_VDubPlanarHack"    // Hack YV16 and YV24 chroma plane order for old VDub's
#define VARNAME_AVIPadScanlines   "OPT_AVIPadScanlines"   // Have scanlines mod4 padded in all pixel formats
#define VARNAME_UseWaveExtensible "OPT_UseWaveExtensible" // Use WAVEFORMATEXTENSIBLE when describing audio to Windows
#define VARNAME_dwChannelMask     "OPT_dwChannelMask"     // Integer audio channel mask. See description of WAVEFORMATEXTENSIBLE for more info.
#define VARNAME_Enable_V210       "OPT_Enable_V210"       // AVS+ use V210 instead of P210 (VfW)
#define VARNAME_Enable_Y3_10_10   "OPT_Enable_Y3_10_10"   // AVS+ use Y3[10][10] instead of P210 (VfW)
#define VARNAME_Enable_Y3_10_16   "OPT_Enable_Y3_10_16"   // AVS+ use Y3[10][16] instead of P216 (VfW)
#define VARNAME_Enable_b64a       "OPT_Enable_b64a"       // AVS+ use b64a instead of BRA[64] (VfW)
#define VARNAME_Enable_PlanarToPackedRGB "OPT_Enable_PlanarToPackedRGB" // AVS+ convert Planar RGB to packed RGB (VfW)

// C exports
#include "avs/capi.h"
AVSC_API(IScriptEnvironment2*, CreateScriptEnvironment2)(int version = AVISYNTH_INTERFACE_VERSION);

#ifndef BUILDING_AVSCORE
#undef AVS_UNUSED
#endif

#pragma pack(pop)

#endif //__AVISYNTH_8_H__
