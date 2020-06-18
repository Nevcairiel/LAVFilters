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

#ifndef AVS_ALIGNMENT_H
#define AVS_ALIGNMENT_H

// Functions and macros to help work with alignment requirements.

// Tells if a number is a power of two.
#define IS_POWER2(n) ((n) && !((n) & ((n) - 1)))

// Tells if the pointer "ptr" is aligned to "align" bytes.
#define IS_PTR_ALIGNED(ptr, align) (((uintptr_t)ptr & ((uintptr_t)(align-1))) == 0)

// Rounds up the number "n" to the next greater multiple of "align"
#define ALIGN_NUMBER(n, align) (((n) + (align)-1) & (~((align)-1)))

// Rounds up the pointer address "ptr" to the next greater multiple of "align"
#define ALIGN_POINTER(ptr, align) (((uintptr_t)(ptr) + (align)-1) & (~(uintptr_t)((align)-1)))

#ifdef __cplusplus

#include <cassert>
#include <cstdlib>
#include <cstdint>
#include "config.h"

#if defined(MSVC) && _MSC_VER<1400
    // needed for VS2013, otherwise C++11 'alignas' works
    #define avs_alignas(x) __declspec(align(x))
#else
    // assumes C++11 support
    #define avs_alignas(x) alignas(x)
#endif

template<typename T>
static bool IsPtrAligned(T* ptr, size_t align)
{
  assert(IS_POWER2(align));
  return (bool)IS_PTR_ALIGNED(ptr, align);
}

template<typename T>
static T AlignNumber(T n, T align)
{
  assert(IS_POWER2(align));
  return ALIGN_NUMBER(n, align);
}

template<typename T>
static T* AlignPointer(T* ptr, size_t align)
{
  assert(IS_POWER2(align));
  return (T*)ALIGN_POINTER(ptr, align);
}

extern "C"
{
#else
#include <stdlib.h>
#endif  // __cplusplus

// Returns a new buffer that is at least the size "nbytes".
// The buffer will be aligned to "align" bytes.
// Returns NULL on error. On successful allocation,
// the returned buffer must be freed using "avs_free".
inline void* avs_malloc(size_t nbytes, size_t align)
{
  if (!IS_POWER2(align))
    return NULL;

  size_t offset = sizeof(void*) + align - 1;

  void *orig = malloc(nbytes + offset);
  if (orig == NULL)
   return NULL;

  void **aligned = (void**)(((uintptr_t)orig + (uintptr_t)offset) & (~(uintptr_t)(align-1)));
  aligned[-1] = orig;
  return aligned;
}

// Buffers allocated using "avs_malloc" must be freed
// using "avs_free" instead of "free".
inline void avs_free(void *ptr)
{
  // Mirroring free()'s semantic requires us to accept NULLs
  if (ptr == NULL)
    return;

  free(((void**)ptr)[-1]);
}

#ifdef __cplusplus
} // extern "C"

// The point of these undef's is to force using the template functions
// if we are in C++ mode. For C, the user can rely only on the macros.
#undef IS_PTR_ALIGNED
#undef ALIGN_NUMBER
#undef ALIGN_POINTER

#endif  // __cplusplus

#endif  //AVS_ALIGNMENT_H
