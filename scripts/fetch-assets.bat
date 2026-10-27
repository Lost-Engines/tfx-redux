@echo off
setlocal DisableDelayedExpansion
rem Fetch reference CD images and audio firmware; verify every asset on each run.
if "%~1"=="--help" goto help
if not "%~2"=="" goto usage_error
set "script_dir=%~dp0"
for %%M in (game-files.txt roms.txt) do (
  if not exist "%script_dir%%%M" (
    echo ERROR: missing asset manifest: %script_dir%%%M 1>&2
    exit /b 1
  )
)
rem Match Darker: default to the package directory, beside tfx.exe.
set "destination=%~dp0.."
if not "%~1"=="" set "destination=%~1"
for %%T in (curl.exe certutil.exe tar.exe) do (
  where.exe %%T >nul 2>nul
  if errorlevel 1 (
    echo ERROR: %%T is required to fetch and verify assets. 1>&2
    exit /b 1
  )
)
if not exist "%destination%" mkdir "%destination%"
if not exist "%destination%" exit /b 1
pushd "%destination%" || exit /b 1
set "temporary=%CD%\.fetch-assets-%RANDOM%-%RANDOM%"
mkdir "%temporary%" || (popd & exit /b 1)
set "archive_url="
set "archive_ready="
set "failed=0"
for %%M in (game-files.txt roms.txt) do (
for /f "usebackq eol=# tokens=1-4" %%H in ("%script_dir%%%M") do (
  set "expected=%%H"
  set "file=%%I"
  set "url=%%J"
  set "member=%%K"
  call :fetch
)
)
rmdir /s /q "%temporary%"
if "%failed%"=="1" (
  popd
  exit /b 1
)
echo Game images and audio firmware ready in %CD%
echo Launch TFX normally; it will prepare the game files from cd\tfx.cue on first use.
popd
exit /b 0

:fetch
set "file=%file:/=\%"
set "check_file=%file%"
call :matches
if defined matched (
  echo %file%: OK
  exit /b 0
)
if exist "%file%\" goto fetch_failed
echo Fetching %file%
for %%P in ("%file%") do if not exist "%%~dpP" mkdir "%%~dpP"
if "%url%"=="-" goto bundled
if "%member%"=="-" goto direct
rem Download an archive only once for all of its selected members, even if it fails.
if not "%url%"=="%archive_url%" call :archive
if not defined archive_ready goto fetch_failed
tar.exe -xOf "%temporary%\archive.zip" "%member%" > "%temporary%\rom.tmp"
if errorlevel 1 goto fetch_failed
goto verify
:bundled
copy /b /y "%script_dir%%member%" "%temporary%\rom.tmp" >nul
if errorlevel 1 goto fetch_failed
goto verify
:direct
set "download_target=%temporary%\rom.tmp"
call :download
if errorlevel 1 goto fetch_failed
:verify
set "check_file=%temporary%\rom.tmp"
call :matches
if not defined matched (
  echo ERROR: %file% failed SHA-256 verification. 1>&2
  goto fetch_failed
)
move /y "%temporary%\rom.tmp" "%file%" >nul
if errorlevel 1 goto fetch_failed
exit /b 0
:fetch_failed
echo ERROR: could not fetch a verified %file%. Rerun this script to retry. 1>&2
set "failed=1"
exit /b 0

:archive
set "archive_url=%url%"
set "archive_ready="
set "download_target=%temporary%\archive.zip"
call :download
if not errorlevel 1 set "archive_ready=1"
exit /b 0

:download
rem Keeping the URL in a variable avoids CALL re-expanding percent escapes.
curl.exe --user-agent "Lost Engines TFX Redux" --fail --location --retry 3 --retry-all-errors --retry-delay 2 --connect-timeout 30 --output "%download_target%" "%url%"
exit /b %errorlevel%

:matches
set "matched="
if not exist "%check_file%" exit /b 0
for /f "delims=" %%D in ('certutil.exe -hashfile "%check_file%" SHA256 2^>nul') do if /I "%%D"=="%expected%" set "matched=1"
exit /b 0

:help
echo Usage: %~nx0 [destination]
echo Fetch the TFX 2.05 CD and CM-32L, SC-55 and SCC-1A ROMs (default: package directory, beside tfx.exe).
exit /b 0
:usage_error
echo Usage: %~nx0 [destination] 1>&2
exit /b 2
