@ECHO OFF

for /f "usebackq tokens=*" %%i in (`call "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -version "[16.0,18.0)" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set VSInstallDir=%%i
)

set "VSCMD_START_DIR=%CD%"
call "%VSInstallDir%\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x86 > NUL

sh build_ffmpeg.sh x86 || EXIT /B 1
MSBuild.exe LAVFilters.sln /nologo /m /t:Rebuild /property:Configuration=Release;Platform=Win32
IF ERRORLEVEL 1 EXIT /B 1

sh build_ffmpeg.sh x64 || EXIT /B 1
MSBuild.exe LAVFilters.sln /nologo /m /t:Rebuild /property:Configuration=Release;Platform=x64
IF ERRORLEVEL 1 EXIT /B 1

PAUSE
