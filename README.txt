LAV Filters - ffmpeg based DirectShow Splitter and Decoders

LAV Filters are a set of DirectShow filters based on the libavformat and libavcodec libraries
from the ffmpeg project, which will allow you to play virtually any format in a DirectShow player.

The filters are still under Development, so not every feature is finished, or every format supported.

Prereqs
=============================
VC++2010 Runtime (http://www.microsoft.com/downloads/details.aspx?FamilyID=a7b7a05e-6de6-4d3a-a423-37bf0912db84)

Install
=============================
- Unpack
- Register (regsvr32 LAVFSplitter.ax)
	Registering requires administrative rights.
	On Vista/7 also make sure to start it in an elevated shell.

Using it
=============================
By default the splitter will register for all media formats that have been
tested and found working at least partially.
This currently includes (but is not limited to)
	MKV/WebM, AVI, MP4/MOV, TS/M2TS/MPG, FLV, OGG

However, some other splitters register in a "bad" way and force all players
to use them. The Haali Media Splitter is one of those, and to give priority
to the LAVFSplitter you have to either uninstall Haali or rename its .ax file
at least temporarily.

The Audio Decoder will register with a relatively high merit, which should make
it the preferred decoder by default. Most players offer a way to choose the preferred
decoder however.

Compiling
=============================
Compiling is pretty straight forward using VC++2010 (included project files).
It does, however, require that you build your own ffmpeg.
You need to place the full ffmpeg package in a directory called "ffmpeg" in the 
main source directory (the directory this file was in). There are scripts to 
build a proper ffmpeg included.

I recommend using my fork of ffmpeg, as it includes additional patches for 
media compatibility:
http://git.1f0.de/gitweb?p=ffmpeg.git;a=summary

Feedback
=============================
GitHub Project: http://github.com/Nevcairiel/LAVFSplitter
Doom9: http://forum.doom9.org/showthread.php?t=156191
You can, additionally, reach me on IRC in the MPC-HC channel on freenode (#mpc-hc)