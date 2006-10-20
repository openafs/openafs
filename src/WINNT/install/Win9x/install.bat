@echo off

if [%2]==[] goto noparm
if [%2]==[] goto noparm
if [%3]==[] goto noparm
if not [%5]==[] goto BEGIN
:noparm
ECHO  1 parameter = full source path to .\winstall area e.g.  d:\dest\wininstall
echo  2 & 3 parameters = target drive target folder c: afscli 
echo  4 parameters CellName e.g k56.almaden.ibm.com
echo  5 parameter cache size e.g. 40000
goto xit

:begin
rem Generate Unstall.bat
echo echo off > %2\%3\uninstall.bat
echo %2\%3\util_cr * "-[HKEY_CLASSES_ROOT\CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}]" >> %2\%3\uninstall.bat
echo %2\%3\util_cr * "-[HKEY_CLASSES_ROOT\*\shellex\ContextMenuHandlers] @" >> %2\%3\uninstall.bat
echo %2\%3\util_cr * "-[HKEY_CLASSES_ROOT\*\shellex\ContextMenuHandlers\AFS Client Shell Extension]" >> %2\%3\uninstall.bat
echo %2\%3\util_cr * "-[HKEY_CLASSES_ROOT\Folder\shellex\ContextMenuHandlers\AFS Client Shell Extension]" >> %2\%3\uninstall.bat
echo %2\%3\util_cr * "-[HKEY_LOCAL_MACHINE\Software\CLASSES\Folder\shellex\ContextMenuHandlers] @" >> %2\%3\uninstall.bat
echo %2\%3\util_cr * "-[HKEY_LOCAL_MACHINE\Software\IBM\WinAfsLoad]" >> %2\%3\uninstall.bat
echo %2\%3\util_cr * "-[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\App Paths\afsd.exe]" >> %2\%3\uninstall.bat
echo %2\%3\util_cr * "-[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved] {DC515C27-6CAC-11D1-BAE7-00C04FD140D2}" >> %2\%3\uninstall.bat
echo %2\%3\util_cr * "-[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Uninstall\AFS Windows 9x Client]" >> %2\%3\uninstall.bat
echo %2\%3\util_cr * "-[HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\VxD\mmap]" >> %2\%3\uninstall.bat
echo %2\%3\util_cr * "+[HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Runonce] doit=%2\%3\runonce.bat" >> %2\%3\uninstall.bat
echo copy %2\%3\unrunonce %2\%3\runonce.bat >> %2\%3\uninstall.bat
echo if exist c:\autoafs.bat del c:\autoafs.bat >> %2\%3\uninstall.bat
echo echo Please reboot your system now! >> %2\%3\uninstall.bat

ECHO del %2\%3\*.tmp >%2\%3\unrunonce
ECHO del %2\%3\*.dll >>%2\%3\unrunonce
ECHO del %2\%3\*.exe >>%2\%3\unrunonce
ECHO del %2\%3\*.ini >>%2\%3\unrunonce
ECHO del %2\%3\*.vxd >>%2\%3\unrunonce
ECHO del %2\%3\*.hlp >>%2\%3\unrunonce
ECHO del %2\%3\*.cnt >>%2\%3\unrunonce
ECHO del %2\%3\*.doc >>%2\%3\unrunonce
ECHO del %2\%3\*.info >>%2\%3\unrunonce
ECHO del %2\%3\*.pif >>%2\%3\unrunonce
ECHO del %2\%3\tmp\*.dll >>%2\%3\unrunonce
ECHO del %2\%3\tmp\*.exe >>%2\%3\unrunonce
echo del %2\%3\runonce.bat >>%2\%3\unrunonce

echo /afs;%2\afscache;%5 >Cache.Info
echo %4> ThisCell

echo set path=%2\%3;%%path%% > C:\autoafs.bat
echo set afsconf=%2\%3 >> C:\autoafs.bat
if exist c:\autoexec.bat echo if exist autoafs.bat autoafs>>c:\autoexec.bat

%2
if not exist %2\%3\nul mkdir \%3
if not exist %2\%3\tmp\nul mkdir \%3\tmp

rem copy files over
set fp=runonce.PIF
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=license.txt
if not exist %1\%fp% goto nofile
copy %1\%fp%  %2\%3\.

set fp=util_cr.exe
if not exist %1\%fp% goto nofile
copy %1\%fp%  %2\%3\.

set fp=ThisCell
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=cache.info
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=AFSD.PIF
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=CellServDB
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=ReadMe.doc
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=WinAfsLoad.exe
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=afswin9x.HLP
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=afswin9x.CNT
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=unlog.exe
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=afs_shl_ext_1033.dll
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\tmp\.

set fp=afsauthent.dll
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\tmp\.

set fp=afsshare.exe
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=fs.exe
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=klog.exe
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=kpasswd.exe
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=libosi.dll
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\tmp\.

set fp=libafsconf.dll
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\tmp\.

set fp=pts.exe
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=tokens.exe
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=afs_shl_ext.dll
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\tmp\.

set fp=pthread.dll
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\tmp\.

set fp=afsrpc.dll
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\tmp\.

set fp=afsd.exe
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\tmp\.

set fp=mmap.vxd
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

set fp=sock.vxd
if not exist %1\%fp%  goto nofile
copy %1\%fp%  %2\%3\.

echo Update Registry
%1\util_cr * "+[HKEY_CLASSES_ROOT\CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}] @=AFS Client Shell Extension"
%1\util_cr * "+[HKEY_CLASSES_ROOT\CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}\InprocServer32] @=%2\%3\afs_shl_ext.dll"
%1\util_cr * "+[HKEY_CLASSES_ROOT\CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}\InprocServer32] THREADINGMODEL=Apartment"
%1\util_cr * "+[HKEY_CLASSES_ROOT\*\shellex\ContextMenuHandlers] @=afs_shl_ext"
%1\util_cr * "+[HKEY_CLASSES_ROOT\*\shellex\ContextMenuHandlers\AFS Client Shell Extension] @={DC515C27-6CAC-11D1-BAE7-00C04FD140D2}"
%1\util_cr * "+[HKEY_CLASSES_ROOT\Folder\shellex\ContextMenuHandlers\AFS Client Shell Extension] @={DC515C27-6CAC-11D1-BAE7-00C04FD140D2}"
%1\util_cr * "+[HKEY_CURRENT_USER\Software\IBM]"
%1\util_cr * "+[HKEY_CURRENT_USER\Software\IBM\AFS]"
%1\util_cr * "+[HKEY_CURRENT_USER\Software\IBM\AFS\Window] PowerResumeDelay=hex:14,00,00,00"
%1\util_cr * "+[HKEY_CURRENT_USER\Software\IBM\AFS\Window] LoginTime=hex:28,00,00,00"
%1\util_cr * "+[HKEY_CURRENT_USER\Software\IBM\AFS\Security]"
%1\util_cr * "+[HKEY_LOCAL_MACHINE\Software\CLASSES\CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}] @=AFS Client Shell Extension"
%1\util_cr * "+[HKEY_LOCAL_MACHINE\Software\CLASSES\CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}\InprocServer32] @=%2\%3\afs_shl_ext.dll"
%1\util_cr * "+[HKEY_LOCAL_MACHINE\Software\CLASSES\CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}\InprocServer32] THREADINGMODEL=Apartment"
%1\util_cr * "+[HKEY_LOCAL_MACHINE\Software\CLASSES\Folder\shellex\ContextMenuHandlers] @="afs_shl_ext"
%1\util_cr * "+[HKEY_LOCAL_MACHINE\Software\IBM\AFS]"
%1\util_cr * "+[HKEY_LOCAL_MACHINE\Software\IBM\AFS\1.00.000]"
%1\util_cr * "+[HKEY_LOCAL_MACHINE\Software\IBM\WinAfsLoad] InstallDir=%2\%3"
%1\util_cr * "+[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\App Paths\afsd.exe] Path=%2\%3"
%1\util_cr * "+[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\App Paths\afsd.exe] @=%2\%3\afsd.exe"
%1\util_cr * "+[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved] {DC515C27-6CAC-11D1-BAE7-00C04FD140D2}=AFS Client Shell Extension"
%1\util_cr * "+[HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\VxD\mmap] STATICVXD=%2\%3\mmap.vxd"
%1\util_cr * "+[HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Runonce] doit=%2\%3\runonce.bat"
%1\util_cr * "+[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Uninstall\AFS Windows 9x Client] UninstallString=%2\%3\uninstall.bat"
%1\util_cr * "+[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Uninstall\AFS Windows 9x Client] DisplayName=AFS Windows 9x Client"

echo copy %2\%3\tmp\*.* %2\%3\. > %2\%3\runonce.bat
ECHO del %2\%3\tmp\*.dll >>%2\%3\runonce.bat
ECHO del %2\%3\tmp\*.exe >>%2\%3\runonce.bat
echo del %2\%3\runonce.bat >>%2\%3\runonce.bat

notepad %2\%3\license.txt
echo  You need to reboot the system now
goto xit

:nofile
echo  Installation incomplete missing file:%1\%fp%
goto xit

:XIT 
set fp= 
