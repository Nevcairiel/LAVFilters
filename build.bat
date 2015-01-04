@ECHO OFF

call "%VS120COMNTOOLS%vsvars32.bat"

sh build_ffmpeg.sh x86 || EXIT /B 1
devenv LAVFilters.sln /Rebuild "Release|Win32" || EXIT /B 1

sh build_ffmpeg.sh x64 || EXIT /B 1
devenv LAVFilters.sln /Rebuild "Release|x64" || EXIT /B 1

PAUSE
