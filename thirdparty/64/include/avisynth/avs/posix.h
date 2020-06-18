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

#ifdef AVS_POSIX
#ifndef AVSCORE_POSIX_H
#define AVSCORE_POSIX_H

#ifdef __cplusplus
#include <cstring>
#endif
#include <strings.h>
#include <unistd.h>

// Define these MSVC-extension used in Avisynth
#define __single_inheritance

// These things don't exist in Linux
#define __declspec(x)
#define lstrlen strlen
#define lstrcmp strcmp
#define lstrcmpi strcasecmp
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _strdup strdup
#define SetCurrentDirectory(x) chdir(x)
#define SetCurrentDirectoryW(x) chdir(x)
#define GetCurrentDirectoryW(x) getcwd(x)
#define _putenv putenv
#define _alloca alloca

// Borrowing some compatibility macros from AvxSynth, slightly modified
#define UInt32x32To64(a, b) ((uint64_t)(((uint64_t)((uint32_t)(a))) * ((uint32_t)(b))))
#define Int64ShrlMod32(a, b) ((uint64_t)((uint64_t)(a) >> (b)))
#define Int32x32To64(a, b)  ((int64_t)(((int64_t)((long)(a))) * ((long)(b))))

#define InterlockedIncrement(x) __sync_add_and_fetch((x), 1)
#define InterlockedDecrement(x) __sync_sub_and_fetch((x), 1)
#define MulDiv(nNumber, nNumerator, nDenominator)   (int32_t) (((int64_t) (nNumber) * (int64_t) (nNumerator) + (int64_t) ((nDenominator)/2)) / (int64_t) (nDenominator))

#ifndef TRUE
#define TRUE  true
#endif

#ifndef FALSE
#define FALSE false
#endif

#define S_FALSE       (0x00000001)
#define E_FAIL        (0x80004005)
#define FAILED(hr)    ((hr) & 0x80000000)
#define SUCCEEDED(hr) (!FAILED(hr))

// Statuses copied from comments in exception.cpp
#define STATUS_GUARD_PAGE_VIOLATION 0x80000001
#define STATUS_DATATYPE_MISALIGNMENT 0x80000002
#define STATUS_BREAKPOINT 0x80000003
#define STATUS_SINGLE_STEP 0x80000004
#define STATUS_ACCESS_VIOLATION 0xc0000005
#define STATUS_IN_PAGE_ERROR 0xc0000006
#define STATUS_INVALID_HANDLE 0xc0000008
#define STATUS_NO_MEMORY 0xc0000017
#define STATUS_ILLEGAL_INSTRUCTION 0xc000001d
#define STATUS_NONCONTINUABLE_EXCEPTION 0xc0000025
#define STATUS_INVALID_DISPOSITION 0xc0000026
#define STATUS_ARRAY_BOUNDS_EXCEEDED 0xc000008c
#define STATUS_FLOAT_DENORMAL_OPERAND 0xc000008d
#define STATUS_FLOAT_DIVIDE_BY_ZERO 0xc000008e
#define STATUS_FLOAT_INEXACT_RESULT 0xc000008f
#define STATUS_FLOAT_INVALID_OPERATION 0xc0000090
#define STATUS_FLOAT_OVERFLOW 0xc0000091
#define STATUS_FLOAT_STACK_CHECK 0xc0000092
#define STATUS_FLOAT_UNDERFLOW 0xc0000093
#define STATUS_INTEGER_DIVIDE_BY_ZERO 0xc0000094
#define STATUS_INTEGER_OVERFLOW 0xc0000095
#define STATUS_PRIVILEGED_INSTRUCTION 0xc0000096
#define STATUS_STACK_OVERFLOW 0xc00000fd

// Calling convension
#define __stdcall
#define __cdecl

#endif // AVSCORE_POSIX_H
#endif // AVS_POSIX
