@ECHO OFF

call "%VS140COMNTOOLS%vsvars32.bat"

sh build_ffmpeg.sh x86 || EXIT /B 1
MSBuild.exe LAVFilters.sln /nologo /m /t:Rebuild /property:Configuration=Release;Platform=Win32
IF %ERRORLEVEL% NEQ 0 EXIT /B 1

sh build_ffmpeg.sh x64 || EXIT /B 1
MSBuild.exe LAVFilters.sln /nologo /m /t:Rebuild /property:Configuration=Release;Platform=x64
IF %ERRORLEVEL% NEQ 0 EXIT /B 1

PAUSE
