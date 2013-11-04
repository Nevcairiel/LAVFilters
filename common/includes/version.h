#pragma once

#define LAV_VERSION_MAJOR     0
#define LAV_VERSION_MINOR    59
#define LAV_VERSION_REVISION  1

#define LAV_AUDIO "LAV Audio Decoder"
#define LAV_VIDEO "LAV Video Decoder"
#define LAV_SPLITTER "LAV Splitter"

/////////////////////////////////////////////////////////
#ifndef ISPP_INCLUDED

// Set XP as the minimal targeted OS
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0502
#ifdef WINVER
#undef WINVER
#endif
#define WINVER _WIN32_WINNT

#define DO_MAKE_STR(x) #x
#define MAKE_STR(x) DO_MAKE_STR(x)

#define LAV_VERSION LAV_VERSION_MAJOR.LAV_VERSION_MINOR.LAV_VERSION_REVISION
#define LAV_VERSION_TAG LAV_VERSION_MAJOR, LAV_VERSION_MINOR, LAV_VERSION_REVISION

#define LAV_VERSION_STR MAKE_STR(LAV_VERSION)

#ifdef LAV_DEBUG_RELEASE
#define ENABLE_DEBUG_LOGFILE 1
#else
#define ENABLE_DEBUG_LOGFILE 0
#endif

#endif
