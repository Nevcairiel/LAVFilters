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
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.

#ifndef AVSCORE_CPUID_H
#define AVSCORE_CPUID_H

// For GetCPUFlags.  These are backwards-compatible with those in VirtualDub.
// ending with SSE4_2
// For emulation see https://software.intel.com/en-us/articles/intel-software-development-emulator
enum {
                    /* oldest CPU to support extension */
  CPUF_FORCE        =  0x01,   //  N/A
  CPUF_FPU          =  0x02,   //  386/486DX
  CPUF_MMX          =  0x04,   //  P55C, K6, PII
  CPUF_INTEGER_SSE  =  0x08,   //  PIII, Athlon
  CPUF_SSE          =  0x10,   //  PIII, Athlon XP/MP
  CPUF_SSE2         =  0x20,   //  PIV, K8
  CPUF_3DNOW        =  0x40,   //  K6-2
  CPUF_3DNOW_EXT    =  0x80,   //  Athlon
  CPUF_X86_64       =  0xA0,   //  Hammer (note: equiv. to 3DNow + SSE2, which
                               //          only Hammer will have anyway)
  CPUF_SSE3         = 0x100,   //  PIV+, K8 Venice
  CPUF_SSSE3        = 0x200,   //  Core 2
  CPUF_SSE4         = 0x400,
  CPUF_SSE4_1       = 0x400,   //  Penryn, Wolfdale, Yorkfield
  CPUF_AVX          = 0x800,   //  Sandy Bridge, Bulldozer
  CPUF_SSE4_2       = 0x1000,  //  Nehalem
  // AVS+
  CPUF_AVX2         = 0x2000,   //  Haswell
  CPUF_FMA3         = 0x4000,
  CPUF_F16C         = 0x8000,
  CPUF_MOVBE        = 0x10000,  // Big Endian move
  CPUF_POPCNT       = 0x20000,
  CPUF_AES          = 0x40000,
  CPUF_FMA4         = 0x80000,

  CPUF_AVX512F      = 0x100000,  // AVX-512 Foundation.
  CPUF_AVX512DQ     = 0x200000,  // AVX-512 DQ (Double/Quad granular) Instructions
  CPUF_AVX512PF     = 0x400000,  // AVX-512 Prefetch
  CPUF_AVX512ER     = 0x800000,  // AVX-512 Exponential and Reciprocal
  CPUF_AVX512CD     = 0x1000000, // AVX-512 Conflict Detection
  CPUF_AVX512BW     = 0x2000000, // AVX-512 BW (Byte/Word granular) Instructions
  CPUF_AVX512VL     = 0x4000000, // AVX-512 VL (128/256 Vector Length) Extensions
  CPUF_AVX512IFMA   = 0x8000000, // AVX-512 IFMA integer 52 bit
  CPUF_AVX512VBMI   = 0x10000000,// AVX-512 VBMI
};

#ifdef BUILDING_AVSCORE
int GetCPUFlags();
void SetMaxCPU(int new_flags);
#endif

#endif // AVSCORE_CPUID_H
