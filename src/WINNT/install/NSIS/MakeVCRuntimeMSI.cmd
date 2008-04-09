@echo off
setlocal
if "%1"=="debug" set DR=Debug
if "%1"=="retail" set DR=Retail
if "%DR%"=="" goto usage
if "%2"=="x86" set PL=Intel
if "%2"=="x64" set PL=x64
if "%PL%"=="" goto usage

candle vcruntime.wxs -dConfig=%DR% -dPlatform=%PL%
light  vcruntime.wixobj

echo Done!
goto done

:usage

echo. Usage: MakeVCRuntimeMSI.cmd {debug/retail} {x86/x64}

:done
