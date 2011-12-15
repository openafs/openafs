@echo off
rem Copyright 2000, International Business Machines Corporation and others.
rem All Rights Reserved.
rem 
rem This software has been released under the terms of the IBM Public
rem License.  For details, see the LICENSE file in the top-level source
rem directory or online at http://www.openafs.org/dl/license10.html

REM AFS build environment language selection for Windows NT.
REM ########################################################################


REM ########################################################################
REM Accept language name as an argument; default to English.

rem # Detect requests for English
rem
if "%1"=="eng" goto en_US
if "%1"=="english" goto en_US
if "%1"=="English" goto en_US
if "%1"=="ENGLISH" goto en_US
if "%1"=="en_US" goto en_US
if "%1"=="EN_US" goto en_US
if "%1"=="en_us" goto en_US
if "%1"=="EN_us" goto en_US
if "%1"=="" goto en_US

rem # Detect requests for Japanese
rem
if "%1"=="jap" goto ja_JP
if "%1"=="japanese" goto ja_JP
if "%1"=="Japanese" goto ja_JP
if "%1"=="JAPANESE" goto ja_JP
if "%1"=="ja_JP" goto ja_JP
if "%1"=="JA_JP" goto ja_JP
if "%1"=="ja_jp" goto ja_JP
if "%1"=="JA_jp" goto ja_JP

rem # Detect requests for Korean
rem
if "%1"=="kor" goto ko_KR
if "%1"=="korean" goto ko_KR
if "%1"=="Korean" goto ko_KR
if "%1"=="KOREAN" goto ko_KR
if "%1"=="ko_KR" goto ko_KR
if "%1"=="KO_KR" goto ko_KR
if "%1"=="ko_kr" goto ko_KR
if "%1"=="KO_kr" goto ko_KR

rem # Detect requests for Chinese (Simplified; PR China)
rem
if "%1"=="chi" goto zh_CN
if "%1"=="chinese" goto zh_CN
if "%1"=="Chinese" goto zh_CN
if "%1"=="CHINESE" goto zh_CN
if "%1"=="zh_CN" goto zh_CN
if "%1"=="ZH_CN" goto zh_CN
if "%1"=="zh_cn" goto zh_CN
if "%1"=="ZH_cn" goto zh_CN

rem # Detect requests for Chinese (Traditional; Taiwan)
rem
if "%1"=="tai" goto zh_TW
if "%1"=="taiwan" goto zh_TW
if "%1"=="Taiwan" goto zh_TW
if "%1"=="TAIWAN" goto zh_TW
if "%1"=="zh_TW" goto zh_TW
if "%1"=="ZH_TW" goto zh_TW
if "%1"=="zh_tw" goto zh_TW
if "%1"=="ZH_tw" goto zh_TW

rem # Detect requests for Brazilian Portuguese
rem
if "%1"=="bra" goto pt_BR
if "%1"=="por" goto pt_BR
if "%1"=="brazil" goto pt_BR
if "%1"=="Brazil" goto pt_BR
if "%1"=="BRAZIL" goto pt_BR
if "%1"=="pt_BR" goto pt_BR
if "%1"=="PT_BR" goto pt_BR
if "%1"=="pt_br" goto pt_BR
if "%1"=="PT_br" goto pt_BR

rem # Detect requests for German
rem
if "%1"=="ger" goto de_DE
if "%1"=="german" goto de_DE
if "%1"=="German" goto de_DE
if "%1"=="GERMAN" goto de_DE
if "%1"=="de_DE" goto de_DE
if "%1"=="DE_DE" goto de_DE
if "%1"=="de_de" goto de_DE
if "%1"=="DE_de" goto de_DE

rem # Detect requests for Spanish
rem
if "%1"=="esp" goto es_ES
if "%1"=="spa" goto es_ES
if "%1"=="spanish" goto es_ES
if "%1"=="Spanish" goto es_ES
if "%1"=="SPANISH" goto es_ES
if "%1"=="es_es" goto es_ES
if "%1"=="es_ES" goto es_ES
if "%1"=="ES_ES" goto es_ES
if "%1"=="ES_es" goto es_ES

rem # Complain if we couldn't match the requested language
rem
echo Don't know how to build language %1.
goto end

rem # Language Identifiers
rem #
rem # LANGNAME:
rem #    A simple abbreviation reflecting a language and sublanguage.
rem #    Our translation lab picks these and uses them when giving
rem #    us back translated files.
rem #
rem # LANGID:
rem #    A decimal representation of a 16-bit Win32 LANGID matching that
rem #    language and sublanguage. The format and relevant constants are
rem #    defined in WINNT.H--the upper 6 bits are the sublanguage, and
rem #    the lower 10 bits are the language. For example:
rem #    LANG_ENGLISH = 9
rem #    SUBLANG_ENGLISH_US = 1
rem #    LANGID = MAKELANGID(9,1) = 000001 0000001001 = 0x0409 = 1033
rem # 
rem # LANGCP:
rem #    The default code page for this language; this value is used when
rem #    building a .RC file with the /c switch. You'll have to look these
rem #    up in a some table somewhere if you add more languages.
rem

:en_US
set LANGID=1033
set LANGNAME=en_US
set LANGCP=1252
echo Building English resources (%LANGID%, %LANGNAME%)
goto arg2

:ja_JP
set LANGID=1041
set LANGNAME=ja_JP
set LANGCP=932
echo Building Japanese resources (%LANGID%, %LANGNAME%)
goto arg2

:ko_KR
set LANGID=1042
set LANGNAME=ko_KR
set LANGCP=949
echo Building Korean resources (%LANGID%, %LANGNAME%)
goto arg2

:zh_CN
set LANGID=2052
set LANGNAME=zh_CN
set LANGCP=936
echo Building Chinese (Simplified: PR China) resources (%LANGID%, %LANGNAME%)
goto arg2

:zh_TW
set LANGID=1028
set LANGNAME=zh_TW
set LANGCP=950
echo Building Chinese (Traditional: Taiwan) resources (%LANGID%, %LANGNAME%)
goto arg2

:pt_BR
set LANGID=1046
set LANGNAME=pt_BR
set LANGCP=1252
echo Building Brazilian Portuguese resources (%LANGID%, %LANGNAME%)
goto arg2

:es_ES
set LANGID=1034
set LANGNAME=es_ES
set LANGCP=1252
echo Building Spanish resources (%LANGID%, %LANGNAME%)
goto arg2

:de_DE
set LANGID=1032
set LANGNAME=de_DE
set LANGCP=1252
echo Building German resources (%LANGID%, %LANGNAME%)
goto arg2

###############################################################################
# Accept a second command-line argument reflecting a command to execute

:arg2
if "%2"=="" goto end
%2 %3 %4 %5 %6 %7 %8 %9
goto end

:end
echo.

