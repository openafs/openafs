@echo off

if "%1"=="" goto show_usage

rem Instead of running the script from the current location
rem we copy ourselves over to the TMP directory and run
rem from there.

if not "%2"=="intemp" goto copy_and_run

rem Do the actual work

setlocal
echo This will replace the current installation of OpenAFS with
echo the upgraded version at :
echo   "%~f1"
set /p response=Continue? (y/n):
if "%response%"=="y" goto do_this
if "%response%"=="Y" goto do_this
endlocal

echo Cancelling.  No changes were made to your computer.

goto end_script

:do_this
endlocal

echo Running MSIEXEC:
echo on

msiexec /fvomus "%~f1"

@echo off
echo Done.

goto end_script

:copy_and_run

copy "%~f0" "%TMP%\tmp_replace_afs.cmd"
"%TMP%\tmp_replace_afs.cmd" "%~f1" intemp

goto end_script

:show_usage
echo %0: Upgrade an existing installation of OpenAFS
echo.
echo This command replaces the existing installation with a new
echo installation.  You need to obtain the new .MSI from openafs.org
echo and run this command as follows :
echo.
echo   %0 [path to new .msi]
echo.
echo The latest release can be found at the following URL:
echo      http://www.openafs.org/release/latest.html
echo.

:end_script
