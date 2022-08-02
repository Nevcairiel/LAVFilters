// Avisynth C Interface Version 0.20
// Copyright 2003 Kevin Atkinson

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
// As a special exception, I give you permission to link to the
// Avisynth C interface with independent modules that communicate with
// the Avisynth C interface solely through the interfaces defined in
// avisynth_c.h, regardless of the license terms of these independent
// modules, and to copy and distribute the resulting combined work
// under terms of your choice, provided that every copy of the
// combined work is accompanied by a complete copy of the source code
// of the Avisynth C interface and Avisynth itself (with the version
// used to produce the combined work), being distributed under the
// terms of the GNU General Public License plus this exception.  An
// independent module is a module which is not derived from or based
// on Avisynth C Interface, such as 3rd-party filters, import and
// export plugins, or graphical user interfaces.

#ifndef AVS_CONFIG_H
#define AVS_CONFIG_H

// Undefine this to get cdecl calling convention
#define AVSC_USE_STDCALL 1

// NOTE TO PLUGIN AUTHORS:
// Because FRAME_ALIGN can be substantially higher than the alignment
// a plugin actually needs, plugins should not use FRAME_ALIGN to check for
// alignment. They should always request the exact alignment value they need.
// This is to make sure that plugins work over the widest range of AviSynth
// builds possible.
#define FRAME_ALIGN 64

#if   defined(_M_AMD64) || defined(__x86_64)
#   define X86_64
#elif defined(_M_IX86) || defined(__i386__)
#   define X86_32
// VS2017 introduced _M_ARM64
#elif defined(_M_ARM64) || defined(__aarch64__)
#   define ARM64
#elif defined(_M_ARM) || defined(__arm__)
#   define ARM32
#elif defined(__PPC64__)
#   define PPC64
#elif defined(_M_PPC) || defined(__PPC__) || defined(__POWERPC__)
#   define PPC32
#elif defined(__riscv)
#   define RISCV
#elif defined(__sparc_v9__)
#   define SPARC
#elif defined(__mips__)
#   define MIPS
#else
#   error Unsupported CPU architecture.
#endif

//            VC++  LLVM-Clang-cl   MinGW-Gnu
// MSVC        x          x
// MSVC_PURE   x
// CLANG                  x
// GCC                                  x

#if defined(__clang__)
// Check clang first. clang-cl also defines __MSC_VER
// We set MSVC because they are mostly compatible
#   define CLANG
#if defined(_MSC_VER)
#   define MSVC
#   define AVS_FORCEINLINE __attribute__((always_inline))
#else
#   define AVS_FORCEINLINE __attribute__((always_inline)) inline
#endif
#elif   defined(_MSC_VER)
#   define MSVC
#   define MSVC_PURE
#   define AVS_FORCEINLINE __forceinline
#elif defined(__GNUC__)
#   define GCC
#   define AVS_FORCEINLINE __attribute__((always_inline)) inline
#elif defined(__INTEL_COMPILER) || defined(__INTEL_LLVM_COMPILER)
// Intel C++ Compilers with MSVC command line interface will not appear here rather at _MSC_VER
#   define AVS_FORCEINLINE inline
#   undef __forceinline
#   define __forceinline inline
#else
#   error Unsupported compiler.
#   define AVS_FORCEINLINE inline
#   undef __forceinline
#   define __forceinline inline
#endif

#if defined(_WIN32)
#   define AVS_WINDOWS
#elif defined(__linux__)
#   define AVS_LINUX
#   define AVS_POSIX
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#   define AVS_BSD
#   define AVS_POSIX
#elif defined(__APPLE__)
#   define AVS_MACOS
#   define AVS_POSIX
#elif defined(__HAIKU__)
#   define AVS_HAIKU
#   define AVS_POSIX
#else
#   error Operating system unsupported.
#endif

// useful warnings disabler macros for supported compilers

#if defined(_MSC_VER)
#define DISABLE_WARNING_PUSH           __pragma(warning( push ))
#define DISABLE_WARNING_POP            __pragma(warning( pop ))
#define DISABLE_WARNING(warningNumber) __pragma(warning( disable : warningNumber ))

#define DISABLE_WARNING_UNREFERENCED_LOCAL_VARIABLE      DISABLE_WARNING(4101)
#define DISABLE_WARNING_UNREFERENCED_FUNCTION            DISABLE_WARNING(4505)
// other warnings you want to deactivate...

#elif defined(__GNUC__) || defined(__clang__)
#define DO_PRAGMA(X) _Pragma(#X)
#define DISABLE_WARNING_PUSH           DO_PRAGMA(GCC diagnostic push)
#define DISABLE_WARNING_POP            DO_PRAGMA(GCC diagnostic pop)
#define DISABLE_WARNING(warningName)   DO_PRAGMA(GCC diagnostic ignored #warningName)

#define DISABLE_WARNING_UNREFERENCED_LOCAL_VARIABLE      DISABLE_WARNING(-Wunused-variable)
#define DISABLE_WARNING_UNREFERENCED_FUNCTION            DISABLE_WARNING(-Wunused-function)
// other warnings you want to deactivate...

#else
#define DISABLE_WARNING_PUSH
#define DISABLE_WARNING_POP
#define DISABLE_WARNING_UNREFERENCED_LOCAL_VARIABLE
#define DISABLE_WARNING_UNREFERENCED_FUNCTION
// other warnings you want to deactivate...

#endif

#if defined(AVS_WINDOWS) && defined(_USING_V110_SDK71_)
// Windows XP does not have proper initialization for
// thread local variables.
// Use workaround instead __declspec(thread)
#define XP_TLS
#endif

#ifndef MSVC
// GCC and Clang can be used on big endian systems, MSVC can't.
#  if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#    define AVS_ENDIANNESS "little"
#  elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#    define AVS_ENDIANNESS "big"
#  else
#    define AVS_ENDIANNESS "middle"
#  endif
#else
#define AVS_ENDIANNESS "little"
#endif

#endif //AVS_CONFIG_H
