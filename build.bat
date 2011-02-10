@ECHO OFF

call "%VS100COMNTOOLS%vsvars32.bat"

sh build_ff_win32.sh
devenv LAVFilters.sln /Build "Release|Win32"

sh build_ff_x64.sh
devenv LAVFilters.sln /Build "Release|x64"

PAUSE
