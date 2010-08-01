// pre-compiled header

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN

#include <Windows.h>
#include <atlstr.h>

#include <intsafe.h>
#include <streams.h>

extern "C" {
#define __STDC_CONSTANT_MACROS
#include "libavformat/avformat.h"
}
