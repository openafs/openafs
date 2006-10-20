@echo off
rem Copyright 2000, International Business Machines Corporation and others.
rem All Rights Reserved.
rem 
rem This software has been released under the terms of the IBM Public
rem License.  For details, see the LICENSE file in the top-level source
rem directory or online at http://www.openafs.org/dl/license10.html

rem This file runs the IS5 command line compiler to compile the setup script.

echo.
echo Compiling the setup script...
echo.
echo NOTE:  Ignore "Function defined but never called" warnings
echo.

Set SavePath=%Path%

Path %IS5ROOT%\Program;%PATH%

Compile -I%IS5ROOT%\Include "Script Files\setup.rul"

If errorlevel 1 goto CompilerErrorOccurred

rem Skip over the error handling and exit
  Goto Done

rem Report the compiler error then exit
:CompilerErrorOccurred
  Echo Error on compile; media not built. 
  Goto Done

:Done
rem Restore the search path
  Path=%SavePath%
  Set SavePath=

