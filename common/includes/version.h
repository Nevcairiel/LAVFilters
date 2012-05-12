#pragma once

#define LAV_VERSION_MAJOR     0
#define LAV_VERSION_MINOR    50
#define LAV_VERSION_REVISION  5

#define LAV_AUDIO "LAV Audio Decoder"
#define LAV_VIDEO "LAV Video Decoder"
#define LAV_SPLITTER "LAV Splitter"

/////////////////////////////////////////////////////////
#ifndef ISPP_INCLUDED

#define DO_MAKE_STR(x) #x
#define MAKE_STR(x) DO_MAKE_STR(x)

#define LAV_VERSION LAV_VERSION_MAJOR.LAV_VERSION_MINOR.LAV_VERSION_REVISION
#define LAV_VERSION_TAG LAV_VERSION_MAJOR, LAV_VERSION_MINOR, LAV_VERSION_REVISION

#define LAV_VERSION_STR MAKE_STR(LAV_VERSION)

#define ENABLE_DEBUG_LOGFILE 0

#endif
