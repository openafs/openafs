@echo off

rem
rem Copyright (C) 1998  Transarc Corporation.
rem All rights reserved.
rem
rem
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

