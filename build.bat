@ECHO OFF

call "%VS110COMNTOOLS%vsvars32.bat"

sh build_ffmpeg.sh x86
devenv LAVFilters.sln /Rebuild "Release|Win32"

sh build_ffmpeg.sh x64
devenv LAVFilters.sln /Rebuild "Release|x64"

PAUSE
