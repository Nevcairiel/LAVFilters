// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once


#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define VC_EXTRALEAN

#include "streams.h"

extern "C" {
#define __STDC_CONSTANT_MACROS
#include "libavformat/avformat.h"
}

#include "DShowUtil.h"


// TODO: reference additional headers your program requires here
