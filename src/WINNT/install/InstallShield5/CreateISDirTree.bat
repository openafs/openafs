@echo off

rem
rem Copyright (C) 1998  Transarc Corporation.
rem All rights reserved.
rem
rem
rem This file creates the IS5 directory tree.  We couldn't check it into cml
rem because the directory names contain spaces.

echo Creating the IS dir tree...

if not exist "Component Definitions" mkdir "Component Definitions"
if not exist "File Groups" mkdir "File Groups"
if not exist Media mkdir Media
if not exist "Media\Transarc AFS" mkdir "Media\Transarc AFS"
if not exist "Registry Entries" mkdir "Registry Entries"
if not exist "Script Files" mkdir "Script Files"
if not exist "Setup Files" mkdir "Setup Files"

if not exist "Setup Files\Uncompressed Files" mkdir "Setup Files\Uncompressed Files"
if not exist "Setup Files\Uncompressed Files\Language Independent" mkdir "Setup Files\Uncompressed Files\Language Independent"
if not exist "Setup Files\Uncompressed Files\Language Independent\OS Independent" mkdir "Setup Files\Uncompressed Files\Language Independent\OS Independent"
if not exist "Setup Files\Compressed Files" mkdir "Setup Files\Compressed Files"
if not exist "Setup Files\Compressed Files\Language Independent" mkdir "Setup Files\Compressed Files\Language Independent"
if not exist "Setup Files\Compressed Files\Language Independent\OS Independent" mkdir "Setup Files\Compressed Files\Language Independent\OS Independent"

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


