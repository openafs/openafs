@echo off

rem
rem Copyright (C) 1998  Transarc Corporation.
rem All rights reserved.
rem
rem
rem This file runs the IS5 command line tools to create the Transarc AFS setup media.

echo Building the setup media...

Set SavePath=%Path%

Path %IS5ROOT%\Program;%PATH%

ISbuild -p"%AFSROOT%\src\WINNT\install\InstallShield5" -m"Transarc AFS" 

if not exist "Media\Transarc AFS\Disk Images\disk1" goto nocopylicense
mkdir "Media\Transarc AFS\Disk Images\disk1\License"
copy ..\..\license\lang\*.rtf "Media\Transarc AFS\Disk Images\disk1\License"
:nocopylicense

If errorlevel 1 goto BuildErrorOccurred

rem Skip over the error handling and exit
  Goto Done

rem Report the build error; then exit
:BuildErrorOccurred
  Echo Error on build; media not built. 

:Done
rem Restore the search path
  Path=%SavePath%
  Set SavePath=

