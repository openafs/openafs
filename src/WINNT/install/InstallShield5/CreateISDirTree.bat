@echo off
rem Copyright 2000, International Business Machines Corporation and others.
rem All Rights Reserved.
rem 
rem This software has been released under the terms of the IBM Public
rem License.  For details, see the LICENSE file in the top-level source
rem directory or online at http://www.openafs.org/dl/license10.html

rem This file creates the IS5 directory tree.  We couldn't check it into cml
rem because the directory names contain spaces.

echo Creating the IS dir tree...

if not exist "Component Definitions" mkdir "Component Definitions"
if not exist "File Groups" mkdir "File Groups"
if not exist Media mkdir Media
if not exist "Media\OpenAFS" mkdir "Media\OpenAFS"
if not exist "Registry Entries" mkdir "Registry Entries"
if not exist "Script Files" mkdir "Script Files"
if not exist "Setup Files" mkdir "Setup Files"

if not exist "Registry Entries" mkdir "Registry Entries"
if not exist "Script Files" mkdir "Script Files"
if not exist "Setup Files" mkdir "Setup Files"

set sub1=Uncompressed Files
call :gencomp
set sub1=Compressed Files
call :gencomp
goto shell

:gencomp
if not exist "Setup Files\%sub1%" mkdir "Setup Files\%sub1%"
set sub2=Language Independent
call :gencomp2
set sub2=0009-English
call :gencomp2
set sub2=0007-German
call :gencomp2
set sub2=0011-Japanese
call :gencomp2
set sub2=0012-Korean
call :gencomp2
set sub2=0416-Portuguese (Brazilian)
call :gencomp2
set sub2=0404-Chinese (Taiwan)
call :gencomp2
set sub2=000a-Spanish
call :gencomp2
set sub2=0804-Chinese (PRC)
call :gencomp2
goto :eof

:gencomp2
rem echo ]%sub1%] ]%sub2%]
if not exist "Setup Files\%sub1%\%sub2%" mkdir "Setup Files\%sub1%\%sub2%"
if not exist "Setup Files\%sub1%\%sub2%\OS Independent" mkdir "Setup Files\%sub1%\%sub2%\OS Independent"
if not exist "Setup Files\%sub1%\%sub2%\Intel 32" mkdir "Setup Files\%sub1%\%sub2%\Intel 32"
goto :eof

:shell
if not exist "Shell Objects" mkdir "Shell Objects"
if not exist "String Tables" mkdir "String Tables"
if not exist "String Tables\0009-English" mkdir "String Tables\0009-English"
if not exist "String Tables\0011-Japanese" mkdir "String Tables\0011-Japanese"
if not exist "String Tables\0012-Korean" mkdir "String Tables\0012-Korean"
if not exist "String Tables\0404-Chinese (Taiwan)" mkdir "String Tables\0404-Chinese (Taiwan)"
if not exist "String Tables\0804-Chinese (PRC)" mkdir "String Tables\0804-Chinese (PRC)"
if not exist "String Tables\0007-German" mkdir "String Tables\0007-German"
if not exist "String Tables\0416-Portuguese (Brazilian)" mkdir "String Tables\0416-Portuguese (Brazilian)"
if not exist "String Tables\000a-Spanish" mkdir "String Tables\000a-Spanish"
if not exist "Text Substitutions" mkdir "Text Substitutions"

:eof