@echo off
rem Copyright 2000, International Business Machines Corporation and others.
rem All Rights Reserved.
rem 
rem This software has been released under the terms of the IBM Public
rem License.  For details, see the LICENSE file in the top-level source
rem directory or online at http://www.openafs.org/dl/license10.html

rem This file generates IS5 files that contain hard coded paths.  We must
rem generate these so the paths are correct for each person doing a build.

echo Generating IS files that contain paths...

call GenIS5.ipr.bat
cd File Groups
rem This next file must run using cmd.exe
cmd /c GenFileGroups.bat
cd ..\Media\OpenAFS
call GenDefault.mda.bat
cd ..\..
