@echo off
@rem Create common include file for NSIS installer

del nsi-includes.nsi
echo !define AFS_DESTDIR %AFSROOT%\DEST\%AFSDEV_BUILDTYPE% > nsi-includes.nsi
echo !define MUI_VERSION %1 >> nsi-includes.nsi

echo !define MUI_MAJORVERSION 1 >>nsi-includes.nsi
echo !define MUI_MINORVERSION 2 >>nsi-includes.nsi
echo !define MUI_PATCHLEVEL 1100 >>nsi-includes.nsi
if "%AFSDEV_CL%" == "1310" echo !define CL1310 1 >> nsi-includes.nsi
if "%AFSDEV_BUILDTYPE%" == "CHECKED" echo !define DEBUG 1 >>nsi-includes.nsi
