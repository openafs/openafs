@echo off
rem Copyright 2000, International Business Machines Corporation and others.
rem All Rights Reserved.
rem 
rem This software has been released under the terms of the IBM Public
rem License.  For details, see the LICENSE file in the top-level source
rem directory or online at http://www.openafs.org/dl/license10.html

rem This file runs the IS5 command line tools to create the OpenAFS setup media.

echo Building the setup media...

Set SavePath=%Path%

Path %IS5ROOT%\Program;%PATH%

ISbuild -p"%AFSROOT%\src\WINNT\install\InstallShield5" -m"OpenAFS" 

if not exist "Media\OpenAFS\Disk Images\disk1" goto nocopylicense
mkdir "Media\OpenAFS\Disk Images\disk1\License"
copy ..\..\license\lang\*.rtf "Media\OpenAFS\Disk Images\disk1\License"
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

