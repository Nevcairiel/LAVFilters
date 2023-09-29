#ifndef _AVS_VERSION_H_
#define _AVS_VERSION_H_

#include "arch.h"

#define       AVS_PPSTR_(x)    	#x
#define       AVS_PPSTR(x)    	AVS_PPSTR_(x)

#define       AVS_PROJECT       AviSynth+
#define       AVS_MAJOR_VER     3
#define       AVS_MINOR_VER     7
#define       AVS_BUGFIX_VER    3
#ifdef RELEASE_TARBALL
#define       AVS_FULLVERSION	AVS_PPSTR(AVS_PROJECT) " " AVS_PPSTR(AVS_MAJOR_VER) "." AVS_PPSTR(AVS_MINOR_VER) "." AVS_PPSTR(AVS_BUGFIX_VER) " (" AVS_PPSTR(AVS_ARCH) ")"
#else
#define       AVS_DEVNEXT_REV   
#define       AVS_DEV_REVDATE   
#define       AVS_DEV_GITHASH   
#define       AVS_SEQREV        		// e.g. 1576
#define       AVS_BRANCH        		// e.g. master
#define       AVS_FULLVERSION	AVS_PPSTR(AVS_PROJECT) " " AVS_PPSTR(AVS_MAJOR_VER) "." AVS_PPSTR(AVS_MINOR_VER) "." AVS_PPSTR(AVS_BUGFIX_VER) " (r" AVS_PPSTR(AVS_SEQREV) ", " AVS_PPSTR(AVS_BRANCH) ", " AVS_PPSTR(AVS_ARCH) ")"
#endif

#endif  //  _AVS_VERSION_H_
