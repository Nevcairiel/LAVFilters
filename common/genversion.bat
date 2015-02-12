@ECHO OFF
SETLOCAL

PUSHD "%~dp0"

SET nbMAJOR_PART=0
SET nbCOMMIT_PART=0
SET nbHASH_PART=00000
SET OLDVER=

:: check for git presence
CALL git describe >NUL 2>&1
IF ERRORLEVEL 1 (
    GOTO NOGIT
)

:: Get git-describe output
FOR /F "tokens=*" %%A IN ('"git describe --long --abbrev=5 HEAD"') DO (
  SET strFILE_VERSION=%%A
)

:: Split into tag, nb commits, hash
FOR /F "tokens=1,2,3 delims=-" %%A IN ("%strFILE_VERSION%") DO (
  SET nbMAJOR_PART=%%A
  SET nbCOMMIT_PART=%%B
  SET nbHASH_PART=%%C
)

:: strip the "g" off the hash
SET nbHASH_PART=%nbHASH_PART:~1%

:WRITE_VER

:: check if info changed, and write if needed
IF EXIST includes\version_rev.h (
    SET /P OLDVER=<includes\version_rev.h
)
SET NEWVER=#define LAV_VERSION_BUILD %nbCOMMIT_PART%
IF NOT "%NEWVER%" == "%OLDVER%" (
    :: swapped order to avoid trailing newlines
    > includes\version_rev.h ECHO %NEWVER%
)
GOTO :END

:NOGIT
echo Git not found
goto WRITE_VER

:END
POPD
ENDLOCAL
