IF EXIST %AFSROOT%\golast.bat DEL %AFSROOT%\golast.bat
ECHO @CD %1 >%AFSROOT%\golast.bat
echo echo Directory - %1 >>%AFSROOT%\golast.bat

