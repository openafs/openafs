@echo off
if [%1]==[-f] shift
if [%1]==[/f] shift
if [%1]==[-F] shift
if [%1]==[/F] shift
if [%1]==[/q] shift
if [%1]==[/Q] shift
if [%1]==[-q] shift
if [%1]==[-Q] shift
if [%1]==[-f] shift
if [%1]==[/f] shift
if [%1]==[-F] shift
if [%1]==[/F] shift
if not [%1]==[] del %1 <%AFSROOT%\DEST\BIN\rmbat.rsp
if not [%1]==[] shift
if not [%1]==[] %AFSROOT%\DEST\BIN\rmbat %1 %2 %3 %4 %5 %6 %7 %8 
