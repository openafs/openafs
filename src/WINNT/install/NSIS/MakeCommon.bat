@echo off
@rem Create common include file for NSIS installer

echo !define AFS_DESTDIR %AFSROOT%\OBJ\DEST\%AFSDEV_BUILDTYPE% > nsi-includes.nsi
echo !define MUI_VERSION %1 >> nsi-includes.nsi

echo !define MUI_MAJORVERSION 1 >>nsi-includes.nsi
echo !define MUI_MINORVERSION 2 >>nsi-includes.nsi
echo !define MUI_PATCHLEVEL 1100 >>nsi-includes.nsi
