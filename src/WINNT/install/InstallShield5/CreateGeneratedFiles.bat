@echo off

rem
rem Copyright (C) 1998  Transarc Corporation.
rem All rights reserved.
rem
rem
rem This file generates IS5 files that contain hard coded paths.  We must
rem generate these so the paths are correct for each person doing a build.

echo Generating IS files that contain paths...

call GenIS5.ipr.bat
cd File Groups
rem This next file must run using cmd.exe
cmd /c GenFileGroups.bat
cd ..\Media\Transarc AFS
call GenDefault.mda.bat
cd ..\..
