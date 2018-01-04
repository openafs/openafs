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

REM Specify the minimum version of IE:
REM   0x500 for Windows 2000 and above
REM   0x501 for Windows XP 32 and above
REM   0x502 for Windows XP 64 and Server 2003 and above
REM   0x600 for Windows Vista and Server 2008 and above
REM   0x700 for Windows 7 and Server 2008 R2 and above
SET _WIN32_IE=0x500

REM Specify the minimum version of the Windows SDK:
SET APPVER=5.1

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
REM                  "1400" for VC8 (VS2005)
REM                  "1500" for VC9 (VS2008)
REM                  "1600" for VC10 (VS2010)
REM                  "1700" for VC11 (VS2012)
set AFSVER_CL=1400

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
REM Code Signing Definitions for signtool.exe (optional)

REM SET CODESIGN_DESC=OpenAFS for Windows
REM SET CODESIGN_TIMESTAMP=<URL for Time Stamp Service>
REM SET CODESIGN_URL=<Support URL displayed in Add/Remove Programs>
REM SET CODESIGN_CROSS_CERT=<Cross signing certificate path>
REM SET CODESIGN_OTHER=<other options required for certificate selection>

REM ########################################################################
REM Symbol Store Support

REM SET SYMSTORE_EXE="C:\WinDDK\7600.16385.0\Debuggers\symstore.exe"
REM SET SYMSTORE_ROOT=<Path to symbol store>
REM SET SYMSTORE_COMMENT=<Comment to add to entries>

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
goto args_done

:free
set AFSBLD_TYPE=FREE
set AFSDEV_CRTDEBUG=0
goto args_done

:args_done

REM #######################################################################
REM Construct Variables Required for NTMakefile
REM     AFSDEV_BUILDTYPE = CHECKED / FREE 
REM     AFSDEV_INCLUDE = default include directories
REM     AFSDEV_LIB = default library directories
REM     AFSDEV_BIN = default build binary directories

set AFSDEV_BUILDTYPE=%AFSBLD_TYPE%

REM Location of Microsoft Visual C++ development folder (8.3 short name)
set MSVCDIR=c:\progra~1\MID05A~1\vc

REM Location of Microsoft Platform SDK (8.3 short name)
set MSSDKDIR=C:\progra~1\MIA713~1\Windows\v6.0a

REM Location of npapi.h (from DDK or Platform SDK samples - 8.3 short name)
set NTDDKDIR=C:\WINDDK\7600.16385.0

REM Location of Microsoft IDN Normalization SDK
set MSIDNNLS=C:\progra~1\MI5913~1

REM Location of Secure Endpoints Kerberos Compatibility SDK 1.0
set KERBEROSCOMPATSDKROOT=c:\progra~2\secure~1\kerber~1

REM Location of the WiX Installer Toolkit
set WIX=c:\tools\wix.2.0.5325

REM Location of Cygwin
set CYGWINDIR=c:\cygwin

REM Location of ActivePerl for Windows
set PERL=c:\perl

REM Location of Microsoft Code Signing Tool
SET SIGNTOOL=C:\winddk\7600.16385.0\bin\amd64\signtool.exe

set AFSDEV_INCLUDE=%MSSDKDIR%\include;%MSVCDIR%\include;%MSIDNNLS%\include
IF "%AFSVER_CL%" == "1700" set AFSDEV_INCLUDE=%AFSDEV_INCLUDE%;%MSVCDIR%\atlmfc\include
IF "%AFSVER_CL%" == "1600" set AFSDEV_INCLUDE=%AFSDEV_INCLUDE%;%MSVCDIR%\atlmfc\include
IF "%AFSVER_CL%" == "1500" set AFSDEV_INCLUDE=%AFSDEV_INCLUDE%;%MSVCDIR%\atlmfc\include
IF "%AFSVER_CL%" == "1400" set AFSDEV_INCLUDE=%AFSDEV_INCLUDE%;%MSVCDIR%\atlmfc\include
IF "%AFSVER_CL%" == "1310" set AFSDEV_INCLUDE=%AFSDEV_INCLUDE%;%MSVCDIR%\atlmfc\include
IF "%AFSVER_CL%" == "1300" set AFSDEV_INCLUDE=%AFSDEV_INCLUDE%;%MSVCDIR%\atlmfc\include
IF "%AFSVER_CL%" == "1200" set AFSDEV_INCLUDE=%AFSDEV_INCLUDE%;%MSVCDIR%\atl\include;%MSVCDIR%\mfc\include
set AFSDEV_INCLUDE=%AFSDEV_INCLUDE%;%NTDDKDIR%\INC\DDK;%NTDDKDIR%\INC\API;

set AFSDEV_LIB=%MSSDKDIR%\lib;%MSVCDIR%\lib
IF "%AFSVER_CL%" == "1700" set AFSDEV_LIB=%AFSDEV_LIB%;%MSVCDIR%\atlmfc\lib
IF "%AFSVER_CL%" == "1600" set AFSDEV_LIB=%AFSDEV_LIB%;%MSVCDIR%\atlmfc\lib
IF "%AFSVER_CL%" == "1500" set AFSDEV_LIB=%AFSDEV_LIB%;%MSVCDIR%\atlmfc\lib
IF "%AFSVER_CL%" == "1400" set AFSDEV_LIB=%AFSDEV_LIB%;%MSVCDIR%\atlmfc\lib
IF "%AFSVER_CL%" == "1310" set AFSDEV_LIB=%AFSDEV_LIB%;%MSVCDIR%\atlmfc\lib
IF "%AFSVER_CL%" == "1300" set AFSDEV_LIB=%AFSDEV_LIB%;%MSVCDIR%\atlmfc\lib
IF "%AFSVER_CL%" == "1200" set AFSDEV_LIB=%AFSDEV_LIB%;%MSVCDIR%\mfc\lib

set AFSDEV_BIN=%MSSDKDIR%\bin;%MSVCDIR%\bin;%PERL%\bin;%CYGWINDIR%\bin;%WIX%

goto end

:usage
echo.
echo Usage: %0 [free^|^checked^|^wspp]
echo.

:end
