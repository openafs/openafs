@echo off
rem Copyright 2000, International Business Machines Corporation and others.
rem All Rights Reserved.
rem 
rem This software has been released under the terms of the IBM Public
rem License.  For details, see the LICENSE file in the top-level source
rem directory or online at http://www.openafs.org/dl/license10.html

rem This file copies the IS5 files from the CML dir to the IS5 dirs.

echo Populating the IS dir tree...

copy Default.cdf "Component Definitions"
copy Default.fgl "Component Definitions"

copy Default.fdf "File Groups"
copy GenFileGroups.bat "File Groups"

copy GenDefault.mda.bat "Media\OpenAFS"

copy Default.rge "Registry Entries"

copy setup.rul "Script Files"

rem Only copy this file when NOT doing a WSPP build
if not defined AFSBLD_IS_WSPP copy setup.bmp "Setup Files\Uncompressed Files\Language Independent\OS Independent"
if not defined AFSBLD_IS_WSPP copy _isuser.dll "Setup Files\Uncompressed Files\Language Independent\OS Independent"

copy %AFSROOT%\DEST\root.server\usr\afs\bin\InstallUtils.dll "Setup Files\Compressed Files\Language Independent\OS Independent"
copy %AFSROOT%\DEST\root.server\usr\afs\bin\afs_setup_utils_*.dll "Setup Files\Compressed Files\Language Independent\OS Independent"

copy Default.shell "Shell Objects\Default.shl"

copy Default.shl "String Tables"

copy lang\en_US\value.shl "String Tables\0009-English"
copy lang\ja_JP\value.shl "String Tables\0011-Japanese"
copy lang\ko_KR\value.shl "String Tables\0012-Korean"
copy lang\zh_TW\value.shl "String Tables\0404-Chinese (Taiwan)"
copy lang\zh_CN\value.shl "String Tables\0804-Chinese (PRC)"
copy lang\de_DE\value.shl "String Tables\0007-German"
copy lang\pt_BR\value.shl "String Tables\0416-Portuguese (Brazilian)"
copy lang\es_ES\value.shl "String Tables\000a-Spanish"

copy Build.tsb "Text Substitutions"
copy Setup.tsb "Text Substitutions"

