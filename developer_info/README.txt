This directory contains header files of non-standard Interfaces that LAV Filters support.
---------------------------------------------------------------------------------------------

----------------------------------------------
IKeyFrameInfo - implemented by LAV Splitter
---------------------------------------------
IKeyFrameInfo allows players to query the position of key frames, so they can redirect seeking
requests to those positions for very smooth seek events. Only fully supported on MKV files.

----------------------------------------------
ITrackInfo - implemented by LAV Splitter
---------------------------------------------
ITrackInfo is an interface to obtain additional information about the streams in a file.
The order to query the streams is the same as returned by IAMStreamSelect::Info

----------------------------------------------
IGraphRebuildDelegate
---------------------------------------------
IGraphRebuildDelegate is not an interface implemented by LAV Splitter itself.
It is designed to offer the ability to take over the graph building process from the players side.
It only exports one function which LAV Splitter will call when a stream change happens on the users requests,
and then the player can take care of the graph changes itself instead of relying on LAV Splitter to do it.

To use IGraphRebuildDelegate, the player needs to implement it, and share the implementing class with LAV Splitter
through the "IObjectWithSite" interface, which is implemented by LAV Splitter.

----------------------------------------------
LAVSplitterSettings / LAVAudioSettings
----------------------------------------------
These interfaces are used to configure LAV programmatically, so the player can do configuration changes.
