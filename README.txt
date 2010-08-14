LAVFSplitter - a libavformat based directshow media splitter

Prereqs
=============================
VC++2010 Runtime (http://www.microsoft.com/downloads/details.aspx?FamilyID=a7b7a05e-6de6-4d3a-a423-37bf0912db8)

Install
=============================
- Unpack
- Register (regsvr32 LAVFSplitter.ax)
	Registering requires adminitrative rights.
	On Vista/7 also make sure to start it in an elevated shell.

Using it
=============================
By default the splitter will register for all media formats that have been
tested and found working at least partially.
This currently includes (but is not limited to)
	MKV/WebM, AVI, MP4/MOV, TS/M2TS/MPG, FLV

However, some other splitters register in a "bad" way and force all players
to use them. The Haali Media Splitter is one of those, and to give priority
to the LAVFSplitter you have to either uninstall Haali or rename its .ax file
at least temporarily.

Feedback
=============================
There currently is no official place for feedback.
A thread on Doom9 is going to be created eventually.
You can, however, reach me on IRC in the MPC-HC channel on freenode (#mpc-hc)