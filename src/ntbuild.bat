rem Copyright 2000, International Business Machines Corporation and others.
rem All Rights Reserved.
rem 
rem This software has been released under the terms of the IBM Public
rem License.  For details, see the LICENSE file in the top-level source
rem directory or online at http://www.openafs.org/dl/license10.html


REM AFS build environment variables for Windows NT.
REM Modify for local configuration; common defaults shown.
REM ########################################################################

REM ########################################################################
REM NOTE: You will need to copy the NLS files into your windows\system32 
REM directory prior to building non-english files.
REM
REM ########################################################################


REM ########################################################################
REM General required definitions:
REM     SYS_NAME = AFS system name
REM Choose one of "i386_w2k", "amd64_w2k", or "i64_w2k"
SET SYS_NAME=i386_w2k

REM Specify the targeted version of Windows and IE: 0x400 for Win9x/NT4 
REM and above; 0x500 for Windows 2000 and above
SET _WIN32_IE=0x400

REM ########################################################################
REM Location of base folder where source lies, build directory
REM e.g. AFSROOT\SRC is source directory of the build tree (8.3 short name)
set AFSROOT=C:\SRC\OpenAFS

REM ########################################################################
REM NTMakefile required definitions:
REM     AFSVER_CL  = version of the Microsoft compiler:
REM                  "1200" for VC6
REM                  "1300" for VC7 (.NET)
REM                  "1310" for .NET 2003
REM                  "1400" for VC8
set AFSVER_CL=1310

REM Location of Microsoft Visual C++ development folder (8.3 short name)
set MSVCDIR=c:\progra~1\micros~2\vc98

REM Location of Microsoft Platform SDK (8.3 short name)
set MSSDKDIR=c:\progra~1\micros~4

REM Location of npapi.h (from DDK or Platform SDK samples - 8.3 short name)
set NTDDKDIR=c:\progra~1\micros~5

REM Location of netmpr.h/netspi.h (from Windows 95/98 DDK - 8.3 short name)
SET W9XDDKDIR=c:\progra~1\micros~6

REM ########################################################################
REM NTMakefile optional definitions:
REM
REM See NTMakefile.SYS_NAME; will normally use defaults.
REM

IF [%HOMEDRIVE%]==[] SET HOMEDRIVE=C:

REM ########################################################################
REM Options necessary when using bison
REM

set BISON_SIMPLE=c:\bin\bison.simple
set BISON_HAIRY=c:\bin\bison.hairy

REM ########################################################################
REM Accept build type as an argument; default to checked.

if "%1"=="" goto checked
if "%1"=="checked" goto checked
if "%1"=="CHECKED" goto checked

if "%1"=="free" goto free
if "%1"=="FREE" goto free

goto usage

:checked
set AFSBLD_TYPE=CHECKED
set AFSDEV_CRTDEBUG=1
goto ifs_arg

:free
set AFSBLD_TYPE=FREE
set AFSDEV_CRTDEBUG=0
goto ifs_arg

:ifs_arg

set AFSIFS=
if "%2"=="ifs" goto is_ifs
if "%2"=="IFS" goto is_ifs

goto args_done

:is_ifs

set AFSIFS=TRUE

:args_done

REM #######################################################################
REM Construct Variables Required for NTMakefile
REM     AFSDEV_BUILDTYPE = CHECKED / FREE 
REM     AFSDEV_INCLUDE = default include directories
REM     AFSDEV_LIB = default library directories
REM     AFSDEV_BIN = default build binary directories

set AFSDEV_BUILDTYPE=%AFSBLD_TYPE%

set AFSDEV_INCLUDE=%MSSDKDIR%\include;%MSVCDIR%\include
IF "%AFSVER_CL%" == "1400" set AFSDEV_INCLUDE=%AFSDEV_INCLUDE%;%MSVCDIR%\atlmfc\include
IF "%AFSVER_CL%" == "1310" set AFSDEV_INCLUDE=%AFSDEV_INCLUDE%;%MSVCDIR%\atlmfc\include
IF "%AFSVER_CL%" == "1300" set AFSDEV_INCLUDE=%AFSDEV_INCLUDE%;%MSVCDIR%\atlmfc\include
IF "%AFSVER_CL%" == "1200" set AFSDEV_INCLUDE=%AFSDEV_INCLUDE%;%MSVCDIR%\atl\include;%MSVCDIR%\mfc\include
set AFSDEV_INCLUDE=%AFSDEV_INCLUDE%;%NTDDKDIR%\include;%W9XDDKDIR%\include

set AFSDEV_LIB=%MSSDKDIR%\lib;%MSVCDIR%\lib
IF "%AFSVER_CL%" == "1400" set AFSDEV_LIB=%AFSDEV_LIB%;%MSVCDIR%\atlmfc\lib
IF "%AFSVER_CL%" == "1310" set AFSDEV_LIB=%AFSDEV_LIB%;%MSVCDIR%\atlmfc\lib
IF "%AFSVER_CL%" == "1300" set AFSDEV_LIB=%AFSDEV_LIB%;%MSVCDIR%\atlmfc\lib
IF "%AFSVER_CL%" == "1200" set AFSDEV_LIB=%AFSDEV_LIB%;%MSVCDIR%\mfc\lib

set AFSDEV_BIN=%MSSDKDIR%\bin;%MSVCDIR%\bin

goto end

:usage
echo.
echo Usage: %0 [free^|^checked^|^wspp]
echo.

:end
