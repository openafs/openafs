@echo off
rem Copyright 2000, International Business Machines Corporation and others.
rem All Rights Reserved.
rem 
rem This software has been released under the terms of the IBM Public
rem License.  For details, see the LICENSE file in the top-level source
rem directory or online at http://www.openafs.org/dl/license10.html

set IS5_DEST=%AFSROOT%\DEST
set IS5_WINNT=%AFSROOT%\src\WINNT
set IS5_HELP=%AFSROOT%\src\WINNT\doc\help
set IS5_INCL=%AFSROOT%\DEST\include
set IS5_DOCROOT=%AFSROOT%\src\WINNT\doc

rem -------------- Client_Program_Files.fgl ----------------------------

echo [TopDir] > Client_Program_Files.fgl
echo file0=%IS5_DEST%\root.client\usr\vice\etc\afsshare.exe >> Client_Program_Files.fgl
echo file1=%IS5_DEST%\include\afs\cm_config.h >> Client_Program_Files.fgl
echo file2=%IS5_DEST%\root.client\usr\vice\etc\libosi.dll >> Client_Program_Files.fgl
echo file3=%IS5_DEST%\include\afs\kautils.h >> Client_Program_Files.fgl
echo file4=%IS5_DEST%\root.client\usr\vice\etc\libafsconf.dll >> Client_Program_Files.fgl
echo file5=%IS5_DEST%\root.client\usr\vice\etc\klog.exe >> Client_Program_Files.fgl
echo file6=%IS5_DEST%\root.client\usr\vice\etc\tokens.exe >> Client_Program_Files.fgl
echo file7=%IS5_DEST%\root.client\usr\vice\etc\unlog.exe >> Client_Program_Files.fgl
echo file8=%IS5_DEST%\root.client\usr\vice\etc\fs.exe >> Client_Program_Files.fgl
echo file9=%IS5_DEST%\lib\libafsconf.lib >> Client_Program_Files.fgl
echo file10=%IS5_DEST%\lib\afsauthent.lib >> Client_Program_Files.fgl
echo file11=%IS5_DEST%\root.client\usr\vice\etc\afscreds.exe >> Client_Program_Files.fgl
echo file12=%IS5_DEST%\root.client\usr\vice\etc\afs_shl_ext.dll >> Client_Program_Files.fgl
echo file13=%IS5_DEST%\include\afs\auth.h >> Client_Program_Files.fgl
echo file14=%IS5_DEST%\root.client\usr\vice\etc\afsd_service.exe >> Client_Program_Files.fgl
echo file15=%IS5_DEST%\root.client\usr\vice\etc\afslogon.dll >> Client_Program_Files.fgl
echo file16=%IS5_DEST%\root.client\usr\vice\etc\symlink.exe >> Client_Program_Files.fgl
echo file17=%IS5_DEST%\bin\kpasswd.exe >> Client_Program_Files.fgl
echo file18=%IS5_DEST%\root.server\usr\afs\bin\pts.exe >> Client_Program_Files.fgl
echo file19=%IS5_DEST%\root.server\usr\afs\bin\bos.exe >> Client_Program_Files.fgl
echo file20=%IS5_DEST%\root.server\usr\afs\bin\kas.exe >> Client_Program_Files.fgl
echo file21=%IS5_DEST%\root.server\usr\afs\bin\vos.exe >> Client_Program_Files.fgl
echo file22=%IS5_DEST%\root.server\usr\afs\bin\udebug.exe >> Client_Program_Files.fgl
echo file23=%IS5_DEST%\bin\translate_et.exe >> Client_Program_Files.fgl
echo file24=%IS5_DEST%\etc\rxdebug.exe >> Client_Program_Files.fgl
echo file25=%IS5_DEST%\etc\backup.exe >> Client_Program_Files.fgl
echo file26=%IS5_DEST%\root.server\usr\afs\bin\ClientUninst.dll >> Client_Program_Files.fgl
echo.  >> Client_Program_Files.fgl
echo [General] >> Client_Program_Files.fgl
echo Type=FILELIST >> Client_Program_Files.fgl
echo Version=1.00.000 >> Client_Program_Files.fgl


rem -------------- Client_Common_Files.fgl -----------------------------

echo [TopDir] > Client_Common_Files.fgl
echo file0=%IS5_DEST%\root.client\usr\vice\etc\afs_config.exe >> Client_Common_Files.fgl
echo file1=%IS5_DEST%\lib\afsrpc.dll >> Client_Common_Files.fgl
echo file2=%IS5_DEST%\lib\afsauthent.dll >> Client_Common_Files.fgl
echo file3=%IS5_DEST%\lib\afspthread.dll >> Client_Common_Files.fgl
echo file4=%IS5_DEST%\root.server\usr\afs\bin\afsprocmgmt.dll >> Client_Common_Files.fgl
echo file5=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAppLib.dll >> Client_Common_Files.fgl
echo file6=%IS5_DEST%\root.server\usr\afs\bin\afsadminutil.dll >> Client_Common_Files.fgl
echo file7=%IS5_DEST%\root.server\usr\afs\bin\afsclientadmin.dll >> Client_Common_Files.fgl
echo file8=%IS5_DEST%\root.server\usr\afs\bin\afsvosadmin.dll >> Client_Common_Files.fgl
echo.  >> Client_Common_Files.fgl
echo [General] >> Client_Common_Files.fgl
echo Type=FILELIST >> Client_Common_Files.fgl
echo Version=1.00.000 >> Client_Common_Files.fgl


rem -------------- Client_WinDir_Files.fgl -----------------------------

echo [TopDir] > Client_WinDir_Files.fgl
rem echo file0=%IS5_DEST%\root.client\usr\vice\etc\afsdcell.ini >> Client_WinDir_Files.fgl
echo.  >> Client_WinDir_Files.fgl
echo [General] >> Client_WinDir_Files.fgl
echo Type=FILELIST >> Client_WinDir_Files.fgl
echo Version=1.00.000 >> Client_WinDir_Files.fgl


rem -------------- Client_System32_Files.fgl ---------------------------

echo [TopDir] > Client_System32_Files.fgl
echo file0=%IS5_DEST%\root.client\usr\vice\etc\afs_cpa.cpl >> Client_System32_Files.fgl
echo file1=%MSVCDIR%\REDIST\Msvcrt.dll >> Client_System32_Files.fgl
echo file2=%MSVCDIR%\REDIST\Mfc42.dll >> Client_System32_Files.fgl
echo.  >> Client_System32_Files.fgl
echo [General] >> Client_System32_Files.fgl
echo Type=FILELIST >> Client_System32_Files.fgl
echo Version=1.00.000 >> Client_System32_Files.fgl


rem -------------- Light_Client_Program_Files.fgl ----------------------

echo [TopDir] > Light_Client_Program_Files.fgl
echo file0=%IS5_DEST%\root.client\usr\vice\etc\afsshare.exe >> Light_Client_Program_Files.fgl
echo file1=%IS5_DEST%\include\afs\cm_config.h >> Light_Client_Program_Files.fgl
echo file2=%IS5_DEST%\root.client\usr\vice\etc\libosi.dll >> Light_Client_Program_Files.fgl
echo file3=%IS5_DEST%\include\afs\kautils.h >> Light_Client_Program_Files.fgl
echo file4=%IS5_DEST%\root.client\usr\vice\etc\libafsconf.dll >> Light_Client_Program_Files.fgl
echo file5=%IS5_DEST%\root.client\usr\vice\etc\klog.exe >> Light_Client_Program_Files.fgl
echo file6=%IS5_DEST%\root.client\usr\vice\etc\tokens.exe >> Light_Client_Program_Files.fgl
echo file7=%IS5_DEST%\root.client\usr\vice\etc\unlog.exe >> Light_Client_Program_Files.fgl
echo file8=%IS5_DEST%\root.client\usr\vice\etc\fs.exe >> Light_Client_Program_Files.fgl
echo file9=%IS5_DEST%\lib\libafsconf.lib >> Light_Client_Program_Files.fgl
echo file10=%IS5_DEST%\lib\afsauthent.lib >> Light_Client_Program_Files.fgl
echo file11=%IS5_DEST%\root.client\usr\vice\etc\afscreds.exe >> Light_Client_Program_Files.fgl
echo file12=%IS5_DEST%\root.client\usr\vice\etc\afs_shl_ext.dll >> Light_Client_Program_Files.fgl
echo file13=%IS5_DEST%\root.client\usr\vice\etc\unlog.exe >> Light_Client_Program_Files.fgl
echo file14=%IS5_DEST%\include\afs\auth.h >> Light_Client_Program_Files.fgl
echo file15=%IS5_DEST%\root.client\usr\vice\etc\afslog95.dll >> Light_Client_Program_Files.fgl
echo file16=%IS5_DEST%\root.client\usr\vice\etc\symlink.exe >> Light_Client_Program_Files.fgl
echo file17=%IS5_DEST%\bin\kpasswd.exe >> Light_Client_Program_Files.fgl
echo file18=%IS5_DEST%\root.server\usr\afs\bin\pts.exe >> Light_Client_Program_Files.fgl
echo file19=%IS5_DEST%\root.server\usr\afs\bin\bos.exe >> Light_Client_Program_Files.fgl
echo file20=%IS5_DEST%\root.server\usr\afs\bin\kas.exe >> Light_Client_Program_Files.fgl
echo file21=%IS5_DEST%\root.server\usr\afs\bin\vos.exe >> Light_Client_Program_Files.fgl
echo file22=%IS5_DEST%\root.server\usr\afs\bin\udebug.exe >> Light_Client_Program_Files.fgl
echo file23=%IS5_DEST%\bin\translate_et.exe >> Light_Client_Program_Files.fgl
echo file24=%IS5_DEST%\etc\rxdebug.exe >> Light_Client_Program_Files.fgl
echo file25=%IS5_DEST%\etc\backup.exe >> Light_Client_Program_Files.fgl
echo file26=%IS5_DEST%\root.server\usr\afs\bin\LightClientUninst.dll >> Light_Client_Program_Files.fgl
echo.  >> Light_Client_Program_Files.fgl
echo [General] >> Light_Client_Program_Files.fgl
echo Type=FILELIST >> Light_Client_Program_Files.fgl
echo Version=1.00.000 >> Light_Client_Program_Files.fgl


rem -------------- Light_Client_Common_Files.fgl -----------------------

echo [TopDir] > Light_Client_Common_Files.fgl
echo file0=%IS5_DEST%\root.client\usr\vice\etc\afs_config.exe >> Light_Client_Common_Files.fgl
echo file1=%IS5_DEST%\lib\afsrpc.dll >> Light_Client_Common_Files.fgl
echo file2=%IS5_DEST%\lib\afsauthent.dll >> Light_Client_Common_Files.fgl
echo file3=%IS5_DEST%\lib\afspthread.dll >> Light_Client_Common_Files.fgl
echo file4=%IS5_DEST%\root.server\usr\afs\bin\afsprocmgmt.dll >> Light_Client_Common_Files.fgl
echo file5=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAppLib.dll >> Light_Client_Common_Files.fgl
echo file6=%IS5_DEST%\root.server\usr\afs\bin\afsadminutil.dll >> Light_Client_Common_Files.fgl
echo file7=%IS5_DEST%\root.server\usr\afs\bin\afsclientadmin.dll >> Light_Client_Common_Files.fgl
echo file8=%IS5_DEST%\root.server\usr\afs\bin\afsvosadmin.dll >> Light_Client_Common_Files.fgl
echo.  >> Light_Client_Common_Files.fgl
echo [General] >> Light_Client_Common_Files.fgl
echo Type=FILELIST >> Light_Client_Common_Files.fgl
echo Version=1.00.000 >> Light_Client_Common_Files.fgl


rem -------------- Light_Client_WinDir_Files.fgl -----------------------

echo [TopDir] > Light_Client_WinDir_Files.fgl
rem echo file0=%IS5_DEST%\root.client\usr\vice\etc\afsdcell.ini >> Light_Client_WinDir_Files.fgl
echo.  >> Light_Client_WinDir_Files.fgl
echo [General] >> Light_Client_WinDir_Files.fgl
echo Type=FILELIST >> Light_Client_WinDir_Files.fgl
echo Version=1.00.000 >> Light_Client_WinDir_Files.fgl


rem -------------- Light_Client_System32_Files.fgl ---------------------

echo [TopDir] > Light_Client_System32_Files.fgl
echo file0=%MSVCDIR%\REDIST\Msvcrt.dll >> Light_Client_System32_Files.fgl
echo file1=%MSVCDIR%\REDIST\Mfc42.dll >> Light_Client_System32_Files.fgl
echo file2=%IS5_DEST%\root.client\usr\vice\etc\afs_cpa.cpl >> Light_Client_System32_Files.fgl
echo.  >> Light_Client_System32_Files.fgl
echo [General] >> Light_Client_System32_Files.fgl
echo Type=FILELIST >> Light_Client_System32_Files.fgl
echo Version=1.00.000 >> Light_Client_System32_Files.fgl


rem -------------- Light95_Client_Program_Files.fgl ----------------------

echo [TopDir] > Light95_Client_Program_Files.fgl
echo file0=%IS5_DEST%\root.client\usr\vice\etc\libosi.dll >> Light95_Client_Program_Files.fgl
echo file1=%IS5_DEST%\root.client\usr\vice\etc\libafsconf.dll >> Light95_Client_Program_Files.fgl
echo file2=%IS5_DEST%\root.client\usr\vice\etc\fs.exe >> Light95_Client_Program_Files.fgl
echo file3=%IS5_DEST%\root.client\usr\vice\etc\afscreds.exe >> Light95_Client_Program_Files.fgl
echo file4=%IS5_DEST%\root.client\usr\vice\etc\afs_shl_ext.dll >> Light95_Client_Program_Files.fgl
echo file5=%IS5_DEST%\root.client\usr\vice\etc\afslog95.dll >> Light95_Client_Program_Files.fgl
echo file6=%IS5_DEST%\root.client\usr\vice\etc\symlink.exe >> Light95_Client_Program_Files.fgl
echo file7=%IS5_DEST%\root.server\usr\afs\bin\LightClientUninst.dll >> Light95_Client_Program_Files.fgl
echo file8=%IS5_DEST%\bin\kpasswd.exe >> Light95_Client_Program_Files.fgl
echo.  >> Light95_Client_Program_Files.fgl
echo [General] >> Light95_Client_Program_Files.fgl
echo Type=FILELIST >> Light95_Client_Program_Files.fgl
echo Version=1.00.000 >> Light95_Client_Program_Files.fgl


rem -------------- Light95_Client_Common_Files.fgl -----------------------

echo [TopDir] > Light95_Client_Common_Files.fgl
echo file0=%IS5_DEST%\root.client\usr\vice\etc\afs_config.exe >> Light95_Client_Common_Files.fgl
echo file1=%IS5_DEST%\lib\afsrpc.dll >> Light95_Client_Common_Files.fgl
echo file2=%IS5_DEST%\lib\afsauthent.dll >> Light95_Client_Common_Files.fgl
echo file3=%IS5_DEST%\lib\win95\afspthread.dll >> Light95_Client_Common_Files.fgl
echo file4=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAppLib.dll >> Light95_Client_Common_Files.fgl
echo file5=%IS5_DEST%\root.server\usr\afs\bin\afsadminutil.dll >> Light95_Client_Common_Files.fgl
echo file6=%IS5_DEST%\root.server\usr\afs\bin\afsclientadmin.dll >> Light95_Client_Common_Files.fgl
echo.  >> Light95_Client_Common_Files.fgl
echo [General] >> Light95_Client_Common_Files.fgl
echo Type=FILELIST >> Light95_Client_Common_Files.fgl
echo Version=1.00.000 >> Light95_Client_Common_Files.fgl


rem -------------- Light95_Client_WinDir_Files.fgl -----------------------

echo [TopDir] > Light95_Client_WinDir_Files.fgl
rem echo file0=%IS5_DEST%\root.client\usr\vice\etc\afsdcell.ini >> Light95_Client_WinDir_Files.fgl
echo.  >> Light95_Client_WinDir_Files.fgl
echo [General] >> Light95_Client_WinDir_Files.fgl
echo Type=FILELIST >> Light95_Client_WinDir_Files.fgl
echo Version=1.00.000 >> Light95_Client_WinDir_Files.fgl


rem -------------- Light95_Client_System32_Files.fgl ---------------------

echo [TopDir] > Light95_Client_System32_Files.fgl
echo file0=%MSVCDIR%\REDIST\Msvcrt.dll >> Light95_Client_System32_Files.fgl
echo file1=%MSVCDIR%\REDIST\Mfc42.dll >> Light95_Client_System32_Files.fgl
echo file2=%IS5_DEST%\root.client\usr\vice\etc\afs_cpa.cpl >> Light95_Client_System32_Files.fgl
echo.  >> Light95_Client_System32_Files.fgl
echo [General] >> Light95_Client_System32_Files.fgl
echo Type=FILELIST >> Light95_Client_System32_Files.fgl
echo Version=1.00.000 >> Light95_Client_System32_Files.fgl


rem -------------- Server_Program_Files.fgl ----------------------------

echo [TopDir] > Server_Program_Files.fgl
echo file0=%IS5_DEST%\root.server\usr\afs\bin\vlserver.exe >> Server_Program_Files.fgl
echo file1=%IS5_DEST%\root.server\usr\afs\bin\volinfo.exe >> Server_Program_Files.fgl
echo file2=%IS5_DEST%\root.server\usr\afs\bin\volserver.exe >> Server_Program_Files.fgl
echo file3=%IS5_DEST%\root.server\usr\afs\bin\afskill.exe >> Server_Program_Files.fgl
echo file4=%IS5_DEST%\root.server\usr\afs\bin\afssvrcfg.exe >> Server_Program_Files.fgl
echo file5=%IS5_DEST%\root.server\usr\afs\bin\bosctlsvc.exe >> Server_Program_Files.fgl
echo file6=%IS5_DEST%\root.server\usr\afs\bin\bosserver.exe >> Server_Program_Files.fgl
echo file7=%IS5_DEST%\root.server\usr\afs\bin\buserver.exe >> Server_Program_Files.fgl
echo file8=%IS5_DEST%\root.server\usr\afs\bin\fileserver.exe >> Server_Program_Files.fgl
echo file9=%IS5_DEST%\etc\fms.exe >> Server_Program_Files.fgl
echo file10=%IS5_DEST%\etc\butc.exe >> Server_Program_Files.fgl
echo file11=%IS5_DEST%\root.server\usr\afs\bin\kaserver.exe >> Server_Program_Files.fgl
echo file12=%IS5_DEST%\root.server\usr\afs\bin\ptserver.exe >> Server_Program_Files.fgl
echo file13=%IS5_DEST%\root.server\usr\afs\bin\salvager.exe >> Server_Program_Files.fgl
echo file14=%IS5_DEST%\root.server\usr\afs\bin\upclient.exe >> Server_Program_Files.fgl
echo file15=%IS5_DEST%\root.server\usr\afs\bin\upserver.exe >> Server_Program_Files.fgl
echo file16=%IS5_DEST%\root.server\usr\afs\bin\ServerUninst.dll >> Server_Program_Files.fgl
echo.  >> Server_Program_Files.fgl
echo [General] >> Server_Program_Files.fgl
echo Type=FILELIST >> Server_Program_Files.fgl
echo Version=1.00.000 >> Server_Program_Files.fgl


rem -------------- Server_Common_Files.fgl -----------------------------

echo [TopDir] > Server_Common_Files.fgl
echo file0=%IS5_DEST%\root.server\usr\afs\bin\afsbosadmin.dll >> Server_Common_Files.fgl
echo file1=%IS5_DEST%\root.server\usr\afs\bin\afscfgadmin.dll >> Server_Common_Files.fgl
echo file2=%IS5_DEST%\root.server\usr\afs\bin\afsclientadmin.dll >> Server_Common_Files.fgl
echo file3=%IS5_DEST%\root.server\usr\afs\bin\afskasadmin.dll >> Server_Common_Files.fgl
echo file4=%IS5_DEST%\root.server\usr\afs\bin\afsptsadmin.dll >> Server_Common_Files.fgl
echo file5=%IS5_DEST%\root.server\usr\afs\bin\afsvosadmin.dll >> Server_Common_Files.fgl
echo file6=%IS5_DEST%\root.server\usr\afs\bin\afsadminutil.dll >> Server_Common_Files.fgl
echo file7=%IS5_DEST%\lib\afsrpc.dll >> Server_Common_Files.fgl
echo file8=%IS5_DEST%\lib\afsauthent.dll >> Server_Common_Files.fgl
echo file9=%IS5_DEST%\lib\afspthread.dll >> Server_Common_Files.fgl
echo file10=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAppLib.dll >> Server_Common_Files.fgl
echo file11=%IS5_DEST%\root.server\usr\afs\bin\afsprocmgmt.dll >> Server_Common_Files.fgl
echo.  >> Server_Common_Files.fgl
echo [General] >> Server_Common_Files.fgl
echo Type=FILELIST >> Server_Common_Files.fgl
echo Version=1.00.000 >> Server_Common_Files.fgl


rem -------------- Server_WinDir_Files.fgl -----------------------------

echo [TopDir] > Server_WinDir_Files.fgl
echo.  >> Server_WinDir_Files.fgl
echo [General] >> Server_WinDir_Files.fgl
echo Type=FILELIST >> Server_WinDir_Files.fgl
echo Version=1.00.000 >> Server_WinDir_Files.fgl


rem -------------- Server_System32_Files.fgl ---------------------------

echo [TopDir] > Server_System32_Files.fgl
echo file0=%IS5_DEST%\root.server\usr\afs\bin\afsserver.cpl >> Server_System32_Files.fgl
echo file1=%MSVCDIR%\REDIST\Msvcrt.dll >> Server_System32_Files.fgl
echo.  >> Server_System32_Files.fgl
echo [General] >> Server_System32_Files.fgl
echo Type=FILELIST >> Server_System32_Files.fgl
echo Version=1.00.000 >> Server_System32_Files.fgl


rem -------------- Control_Center_Program_Files.fgl --------------------

echo [TopDir] > Control_Center_Program_Files.fgl
echo file0=%IS5_DEST%\root.server\usr\afs\bin\TaAfsServerManager.exe >> Control_Center_Program_Files.fgl
echo file1=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAdmSvr.exe >> Control_Center_Program_Files.fgl
echo file2=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAccountManager.exe >> Control_Center_Program_Files.fgl
echo file3=%IS5_DEST%\root.server\usr\afs\bin\CCUninst.dll >> Control_Center_Program_Files.fgl
echo.  >> Control_Center_Program_Files.fgl
echo [General] >> Control_Center_Program_Files.fgl
echo Type=FILELIST >> Control_Center_Program_Files.fgl
echo Version=1.00.000 >> Control_Center_Program_Files.fgl


rem -------------- Control_Center_Common_Files.fgl ---------------------

echo [TopDir] > Control_Center_Common_Files.fgl
echo file0=%IS5_DEST%\root.server\usr\afs\bin\afsbosadmin.dll >> Control_Center_Common_Files.fgl
echo file1=%IS5_DEST%\root.server\usr\afs\bin\afscfgadmin.dll >> Control_Center_Common_Files.fgl
echo file2=%IS5_DEST%\root.server\usr\afs\bin\afsclientadmin.dll >> Control_Center_Common_Files.fgl
echo file3=%IS5_DEST%\root.server\usr\afs\bin\afskasadmin.dll >> Control_Center_Common_Files.fgl
echo file4=%IS5_DEST%\root.server\usr\afs\bin\afsptsadmin.dll >> Control_Center_Common_Files.fgl
echo file5=%IS5_DEST%\root.server\usr\afs\bin\afsvosadmin.dll >> Control_Center_Common_Files.fgl
echo file6=%IS5_DEST%\root.server\usr\afs\bin\afsadminutil.dll >> Control_Center_Common_Files.fgl
echo file7=%IS5_DEST%\lib\afsrpc.dll >> Control_Center_Common_Files.fgl
echo file8=%IS5_DEST%\lib\afsauthent.dll >> Control_Center_Common_Files.fgl
echo file9=%IS5_DEST%\lib\afspthread.dll >> Control_Center_Common_Files.fgl
echo file10=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAppLib.dll >> Control_Center_Common_Files.fgl
echo file11=%IS5_DEST%\root.client\usr\vice\etc\afs_config.exe >> Control_Center_Common_Files.fgl
echo.  >> Control_Center_Common_Files.fgl
echo [General] >> Control_Center_Common_Files.fgl
echo Type=FILELIST >> Control_Center_Common_Files.fgl
echo Version=1.00.000 >> Control_Center_Common_Files.fgl


rem -------------- Control_Center_WinDir_Files.fgl ---------------------

echo [TopDir] > Control_Center_WinDir_Files.fgl
rem echo file0=%IS5_DEST%\root.client\usr\vice\etc\afsdcell.ini >> Control_Center_Windir_Files.fgl
echo.  >> Control_Center_WinDir_Files.fgl
echo [General] >> Control_Center_WinDir_Files.fgl
echo Type=FILELIST >> Control_Center_WinDir_Files.fgl
echo Version=1.00.000 >> Control_Center_WinDir_Files.fgl


rem -------------- Control_Center_System32_Files.fgl--------------------

echo [TopDir] > Control_Center_System32_Files.fgl
echo file0=%IS5_DEST%\root.client\usr\vice\etc\afs_cpa.cpl >> Control_Center_System32_Files.fgl
echo file1=%MSVCDIR%\REDIST\Msvcrt.dll >> Control_Center_System32_Files.fgl
echo.  >> Control_Center_System32_Files.fgl
echo [General] >> Control_Center_System32_Files.fgl
echo Type=FILELIST >> Control_Center_System32_Files.fgl
echo Version=1.00.000 >> Control_Center_System32_Files.fgl


rem -------------- Generate the Release Notes file groups ---------------

rem English
set FILEGROUP=Release_Notes_English_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\en_US
call :Generate_Release_Notes_File_Group

rem Japanese
set FILEGROUP=Release_Notes_Japanese_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\ja_JP
call :Generate_Release_Notes_File_Group

rem Korean
set FILEGROUP=Release_Notes_Korean_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\ko_KR
call :Generate_Release_Notes_File_Group

rem Trad_Chinese
set FILEGROUP=Release_Notes_Trad_Chinese_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\zh_TW
call :Generate_Release_Notes_File_Group

rem Simp_Chinese
set FILEGROUP=Release_Notes_Simp_Chinese_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\zh_CN
call :Generate_Release_Notes_File_Group

rem German
set FILEGROUP=Release_Notes_German_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\de_DE
call :Generate_Empty_File_Group

rem Spanish
set FILEGROUP=Release_Notes_Spanish_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\es_ES
call :Generate_Empty_File_Group

rem Portuguese
set FILEGROUP=Release_Notes_Portuguese_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\pt_BR
call :Generate_Release_Notes_File_Group

goto Install_Guide_Files

:Generate_Release_Notes_File_Group
echo [TopDir] > %FILEGROUP%
echo file0=%IS5_DOC%\README.txt >> %FILEGROUP%
echo file1=%IS5_DOC%\html\ReleaseNotes\awrns000.htm >> %FILEGROUP%
echo file2=%IS5_DOC%\html\ReleaseNotes\awrns002.htm >> %FILEGROUP%
echo file3=%IS5_DOC%\html\ReleaseNotes\awrns003.htm >> %FILEGROUP%
echo file4=%IS5_DOC%\html\ReleaseNotes\awrns004.htm >> %FILEGROUP%
echo file5=%IS5_DOC%\html\ReleaseNotes\awrns005.htm >> %FILEGROUP%
echo file6=%IS5_DOC%\html\ReleaseNotes\awrns006.htm >> %FILEGROUP%
echo file7=%IS5_DOC%\html\ReleaseNotes\awrns007.htm >> %FILEGROUP%
echo. >> %FILEGROUP%
echo [General] >> %FILEGROUP%
echo Type=FILELIST >> %FILEGROUP%
echo Version=1.00.000 >> %FILEGROUP%
goto :EOF

:Generate_Empty_File_Group
echo [TopDir] > %FILEGROUP%
echo. >> %FILEGROUP%
echo [General] >> %FILEGROUP%
echo Type=FILELIST >> %FILEGROUP%
echo Version=1.00.000 >> %FILEGROUP%
goto :EOF


rem -------------- Generate the Install Guide file groups ---------------

:Install_Guide_Files

rem English
set FILEGROUP=Install_Guide_English_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\en_US\Html
call :Generate_Install_Guide_File_Group

rem Japanese
set FILEGROUP=Install_Guide_Japanese_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\ja_JP\Html
call :Generate_Install_Guide_File_Group

rem Korean
set FILEGROUP=Install_Guide_Korean_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\ko_KR\Html
call :Generate_Install_Guide_File_Group

rem Trad_Chinese
set FILEGROUP=Install_Guide_Trad_Chinese_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\zh_TW\Html
call :Generate_Install_Guide_File_Group

rem Simp_Chinese
set FILEGROUP=Install_Guide_Simp_Chinese_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\zh_CN\Html
call :Generate_Install_Guide_File_Group

rem German
set FILEGROUP=Install_Guide_German_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\de_DE\Html
call :Generate_Install_Guide_File_Group

rem Spanish
set FILEGROUP=Install_Guide_Spanish_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\es_ES\Html
call :Generate_Install_Guide_File_Group

rem Portuguese
set FILEGROUP=Install_Guide_Portuguese_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\pt_BR\Html
call :Generate_Install_Guide_File_Group

goto Sys_Admin_Guide_Files

:Generate_Install_Guide_File_Group
echo [TopDir] > %FILEGROUP%
echo file0=%IS5_DOC%\InstallGd\awqbg000.htm >> %FILEGROUP%
echo file1=%IS5_DOC%\InstallGd\awqbg002.htm >> %FILEGROUP%
echo file2=%IS5_DOC%\InstallGd\awqbg003.htm >> %FILEGROUP%
echo file3=%IS5_DOC%\InstallGd\awqbg004.htm >> %FILEGROUP%
echo.  >> %FILEGROUP%
echo [General] >> %FILEGROUP%
echo Type=FILELIST >> %FILEGROUP%
echo Version=1.00.000 >> %FILEGROUP%
goto :EOF


rem -------------- Generate the Sys Admin Guide file groups -------------

:Sys_Admin_Guide_Files

rem English
set FILEGROUP=Sys_Admin_Guide_English_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\en_US\Html
call :Generate_Sys_Admin_Guide_File_Group

rem Japanese
set FILEGROUP=Sys_Admin_Guide_Japanese_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\ja_JP\Html
call :Generate_Sys_Admin_Guide_File_Group

rem Korean
set FILEGROUP=Sys_Admin_Guide_Korean_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\ko_KR\Html
call :Generate_Sys_Admin_Guide_File_Group

rem Trad_Chinese
set FILEGROUP=Sys_Admin_Guide_Trad_Chinese_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\zh_TW\Html
call :Generate_Empty_File_Group

rem Simp_Chinese
set FILEGROUP=Sys_Admin_Guide_Simp_Chinese_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\zh_CN\Html
call :Generate_Empty_File_Group

rem German
set FILEGROUP=Sys_Admin_Guide_German_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\de_DE\Html
call :Generate_Empty_File_Group

rem Spanish
set FILEGROUP=Sys_Admin_Guide_Spanish_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\es_ES\Html
call :Generate_Empty_File_Group

rem Portuguese
set FILEGROUP=Sys_Admin_Guide_Portuguese_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\pt_BR\Html
call :Generate_Empty_File_Group

goto Cmd_Ref_Files

:Generate_Sys_Admin_Guide_File_Group
echo [TopDir] > %FILEGROUP%
echo file0=%IS5_DOC%\SysAdminGd\auagd000.htm >> %FILEGROUP%
echo file1=%IS5_DOC%\SysAdminGd\auagd002.htm >> %FILEGROUP%
echo file2=%IS5_DOC%\SysAdminGd\auagd003.htm >> %FILEGROUP%
echo file3=%IS5_DOC%\SysAdminGd\auagd004.htm >> %FILEGROUP%
echo file4=%IS5_DOC%\SysAdminGd\auagd005.htm >> %FILEGROUP%
echo file5=%IS5_DOC%\SysAdminGd\auagd006.htm >> %FILEGROUP%
echo file6=%IS5_DOC%\SysAdminGd\auagd007.htm >> %FILEGROUP%
echo file7=%IS5_DOC%\SysAdminGd\auagd008.htm >> %FILEGROUP%
echo file8=%IS5_DOC%\SysAdminGd\auagd009.htm >> %FILEGROUP%
echo file9=%IS5_DOC%\SysAdminGd\auagd010.htm >> %FILEGROUP%
echo file10=%IS5_DOC%\SysAdminGd\auagd011.htm >> %FILEGROUP%
echo file11=%IS5_DOC%\SysAdminGd\auagd012.htm >> %FILEGROUP%
echo file12=%IS5_DOC%\SysAdminGd\auagd013.htm >> %FILEGROUP%
echo file13=%IS5_DOC%\SysAdminGd\auagd014.htm >> %FILEGROUP%
echo file14=%IS5_DOC%\SysAdminGd\auagd015.htm >> %FILEGROUP%
echo file15=%IS5_DOC%\SysAdminGd\auagd016.htm >> %FILEGROUP%
echo file16=%IS5_DOC%\SysAdminGd\auagd017.htm >> %FILEGROUP%
echo file17=%IS5_DOC%\SysAdminGd\auagd018.htm >> %FILEGROUP%
echo file18=%IS5_DOC%\SysAdminGd\auagd019.htm >> %FILEGROUP%
echo file19=%IS5_DOC%\SysAdminGd\auagd020.htm >> %FILEGROUP%
echo file20=%IS5_DOC%\SysAdminGd\auagd021.htm >> %FILEGROUP%
echo file21=%IS5_DOC%\SysAdminGd\auagd022.htm >> %FILEGROUP%
echo file22=%IS5_DOC%\SysAdminGd\auagd023.htm >> %FILEGROUP%
echo file23=%IS5_DOC%\SysAdminGd\auagd024.htm >> %FILEGROUP%
echo file24=%IS5_DOC%\SysAdminGd\auagd025.htm >> %FILEGROUP%
echo file25=%IS5_DOC%\SysAdminGd\auagd026.htm >> %FILEGROUP%
echo file26=%IS5_DOC%\SysAdminGd\vnode.gif >> %FILEGROUP%
echo file27=%IS5_DOC%\SysAdminGd\fserver1.gif >> %FILEGROUP%
echo file28=%IS5_DOC%\SysAdminGd\fserver2.gif >> %FILEGROUP%
echo file29=%IS5_DOC%\SysAdminGd\overview.gif >> %FILEGROUP%
echo file30=%IS5_DOC%\SysAdminGd\scout1.gif >> %FILEGROUP%
echo file31=%IS5_DOC%\SysAdminGd\scout2.gif >> %FILEGROUP%
echo file32=%IS5_DOC%\SysAdminGd\scout3.gif >> %FILEGROUP%
echo file33=%IS5_DOC%\SysAdminGd\scout4.gif >> %FILEGROUP%
echo file34=%IS5_DOC%\SysAdminGd\cachmgr.gif >> %FILEGROUP%
echo.  >> %FILEGROUP%
echo [General] >> %FILEGROUP%
echo Type=FILELIST >> %FILEGROUP%
echo Version=1.00.000 >> %FILEGROUP%
goto :EOF

rem -------------- Generate the Cmd Ref file groups ---------------------

:Cmd_Ref_Files

rem English
set FILEGROUP=Cmd_Ref_English_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\en_US\Html
call :Generate_Cmd_Ref_File_Group

rem Japanese
set FILEGROUP=Cmd_Ref_Japanese_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\ja_JP\Html
call :Generate_Cmd_Ref_File_Group

rem Korean
set FILEGROUP=Cmd_Ref_Korean_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\ko_KR\Html
call :Generate_Empty_File_Group

rem Trad_Chinese
set FILEGROUP=Cmd_Ref_Trad_Chinese_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\zh_TW\Html
call :Generate_Empty_File_Group

rem Simp_Chinese
set FILEGROUP=Cmd_Ref_Simp_Chinese_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\zh_CN\Html
call :Generate_Empty_File_Group

rem German
set FILEGROUP=Cmd_Ref_German_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\de_DE\Html
call :Generate_Empty_File_Group

rem Spanish
set FILEGROUP=Cmd_Ref_Spanish_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\es_ES\Html
call :Generate_Empty_File_Group

rem Portuguese
set FILEGROUP=Cmd_Ref_Portuguese_Files.fgl
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\pt_BR\Html
call :Generate_Empty_File_Group

goto Doc_Misc_Files

:Generate_Cmd_Ref_File_Group
echo [TopDir] > %FILEGROUP%
echo file0=%IS5_DOC%\CmdRef\auarf000.htm >> %FILEGROUP%
echo file1=%IS5_DOC%\CmdRef\auarf002.htm >> %FILEGROUP%
echo file2=%IS5_DOC%\CmdRef\auarf003.htm >> %FILEGROUP%
echo file3=%IS5_DOC%\CmdRef\auarf004.htm >> %FILEGROUP%
echo file4=%IS5_DOC%\CmdRef\auarf005.htm >> %FILEGROUP%
echo file5=%IS5_DOC%\CmdRef\auarf006.htm >> %FILEGROUP%
echo file6=%IS5_DOC%\CmdRef\auarf007.htm >> %FILEGROUP%
echo file7=%IS5_DOC%\CmdRef\auarf008.htm >> %FILEGROUP%
echo file8=%IS5_DOC%\CmdRef\auarf009.htm >> %FILEGROUP%
echo file9=%IS5_DOC%\CmdRef\auarf010.htm >> %FILEGROUP%
echo file10=%IS5_DOC%\CmdRef\auarf011.htm >> %FILEGROUP%
echo file11=%IS5_DOC%\CmdRef\auarf012.htm >> %FILEGROUP%
echo file12=%IS5_DOC%\CmdRef\auarf013.htm >> %FILEGROUP%
echo file13=%IS5_DOC%\CmdRef\auarf014.htm >> %FILEGROUP%
echo file14=%IS5_DOC%\CmdRef\auarf015.htm >> %FILEGROUP%
echo file15=%IS5_DOC%\CmdRef\auarf016.htm >> %FILEGROUP%
echo file16=%IS5_DOC%\CmdRef\auarf017.htm >> %FILEGROUP%
echo file17=%IS5_DOC%\CmdRef\auarf018.htm >> %FILEGROUP%
echo file18=%IS5_DOC%\CmdRef\auarf019.htm >> %FILEGROUP%
echo file19=%IS5_DOC%\CmdRef\auarf020.htm >> %FILEGROUP%
echo file20=%IS5_DOC%\CmdRef\auarf021.htm >> %FILEGROUP%
echo file21=%IS5_DOC%\CmdRef\auarf022.htm >> %FILEGROUP%
echo file22=%IS5_DOC%\CmdRef\auarf023.htm >> %FILEGROUP%
echo file23=%IS5_DOC%\CmdRef\auarf024.htm >> %FILEGROUP%
echo file24=%IS5_DOC%\CmdRef\auarf025.htm >> %FILEGROUP%
echo file25=%IS5_DOC%\CmdRef\auarf026.htm >> %FILEGROUP%
echo file26=%IS5_DOC%\CmdRef\auarf027.htm >> %FILEGROUP%
echo file27=%IS5_DOC%\CmdRef\auarf028.htm >> %FILEGROUP%
echo file28=%IS5_DOC%\CmdRef\auarf029.htm >> %FILEGROUP%
echo file29=%IS5_DOC%\CmdRef\auarf030.htm >> %FILEGROUP%
echo file30=%IS5_DOC%\CmdRef\auarf031.htm >> %FILEGROUP%
echo file31=%IS5_DOC%\CmdRef\auarf032.htm >> %FILEGROUP%
echo file32=%IS5_DOC%\CmdRef\auarf033.htm >> %FILEGROUP%
echo file33=%IS5_DOC%\CmdRef\auarf034.htm >> %FILEGROUP%
echo file34=%IS5_DOC%\CmdRef\auarf035.htm >> %FILEGROUP%
echo file35=%IS5_DOC%\CmdRef\auarf036.htm >> %FILEGROUP%
echo file36=%IS5_DOC%\CmdRef\auarf037.htm >> %FILEGROUP%
echo file37=%IS5_DOC%\CmdRef\auarf038.htm >> %FILEGROUP%
echo file38=%IS5_DOC%\CmdRef\auarf039.htm >> %FILEGROUP%
echo file39=%IS5_DOC%\CmdRef\auarf040.htm >> %FILEGROUP%
echo file40=%IS5_DOC%\CmdRef\auarf041.htm >> %FILEGROUP%
echo file41=%IS5_DOC%\CmdRef\auarf042.htm >> %FILEGROUP%
echo file42=%IS5_DOC%\CmdRef\auarf043.htm >> %FILEGROUP%
echo file43=%IS5_DOC%\CmdRef\auarf044.htm >> %FILEGROUP%
echo file44=%IS5_DOC%\CmdRef\auarf045.htm >> %FILEGROUP%
echo file45=%IS5_DOC%\CmdRef\auarf046.htm >> %FILEGROUP%
echo file46=%IS5_DOC%\CmdRef\auarf047.htm >> %FILEGROUP%
echo file47=%IS5_DOC%\CmdRef\auarf048.htm >> %FILEGROUP%
echo file48=%IS5_DOC%\CmdRef\auarf049.htm >> %FILEGROUP%
echo file49=%IS5_DOC%\CmdRef\auarf050.htm >> %FILEGROUP%
echo file50=%IS5_DOC%\CmdRef\auarf051.htm >> %FILEGROUP%
echo file51=%IS5_DOC%\CmdRef\auarf052.htm >> %FILEGROUP%
echo file52=%IS5_DOC%\CmdRef\auarf053.htm >> %FILEGROUP%
echo file53=%IS5_DOC%\CmdRef\auarf054.htm >> %FILEGROUP%
echo file54=%IS5_DOC%\CmdRef\auarf055.htm >> %FILEGROUP%
echo file55=%IS5_DOC%\CmdRef\auarf056.htm >> %FILEGROUP%
echo file56=%IS5_DOC%\CmdRef\auarf057.htm >> %FILEGROUP%
echo file57=%IS5_DOC%\CmdRef\auarf058.htm >> %FILEGROUP%
echo file58=%IS5_DOC%\CmdRef\auarf059.htm >> %FILEGROUP%
echo file59=%IS5_DOC%\CmdRef\auarf060.htm >> %FILEGROUP%
echo file60=%IS5_DOC%\CmdRef\auarf061.htm >> %FILEGROUP%
echo file61=%IS5_DOC%\CmdRef\auarf062.htm >> %FILEGROUP%
echo file62=%IS5_DOC%\CmdRef\auarf063.htm >> %FILEGROUP%
echo file63=%IS5_DOC%\CmdRef\auarf064.htm >> %FILEGROUP%
echo file64=%IS5_DOC%\CmdRef\auarf065.htm >> %FILEGROUP%
echo file65=%IS5_DOC%\CmdRef\auarf066.htm >> %FILEGROUP%
echo file66=%IS5_DOC%\CmdRef\auarf067.htm >> %FILEGROUP%
echo file67=%IS5_DOC%\CmdRef\auarf068.htm >> %FILEGROUP%
echo file68=%IS5_DOC%\CmdRef\auarf069.htm >> %FILEGROUP%
echo file69=%IS5_DOC%\CmdRef\auarf070.htm >> %FILEGROUP%
echo file70=%IS5_DOC%\CmdRef\auarf071.htm >> %FILEGROUP%
echo file71=%IS5_DOC%\CmdRef\auarf072.htm >> %FILEGROUP%
echo file72=%IS5_DOC%\CmdRef\auarf073.htm >> %FILEGROUP%
echo file73=%IS5_DOC%\CmdRef\auarf074.htm >> %FILEGROUP%
echo file74=%IS5_DOC%\CmdRef\auarf075.htm >> %FILEGROUP%
echo file75=%IS5_DOC%\CmdRef\auarf076.htm >> %FILEGROUP%
echo file76=%IS5_DOC%\CmdRef\auarf077.htm >> %FILEGROUP%
echo file77=%IS5_DOC%\CmdRef\auarf078.htm >> %FILEGROUP%
echo file78=%IS5_DOC%\CmdRef\auarf079.htm >> %FILEGROUP%
echo file79=%IS5_DOC%\CmdRef\auarf080.htm >> %FILEGROUP%
echo file80=%IS5_DOC%\CmdRef\auarf081.htm >> %FILEGROUP%
echo file81=%IS5_DOC%\CmdRef\auarf082.htm >> %FILEGROUP%
echo file82=%IS5_DOC%\CmdRef\auarf083.htm >> %FILEGROUP%
echo file83=%IS5_DOC%\CmdRef\auarf084.htm >> %FILEGROUP%
echo file84=%IS5_DOC%\CmdRef\auarf085.htm >> %FILEGROUP%
echo file85=%IS5_DOC%\CmdRef\auarf086.htm >> %FILEGROUP%
echo file86=%IS5_DOC%\CmdRef\auarf087.htm >> %FILEGROUP%
echo file87=%IS5_DOC%\CmdRef\auarf088.htm >> %FILEGROUP%
echo file88=%IS5_DOC%\CmdRef\auarf089.htm >> %FILEGROUP%
echo file89=%IS5_DOC%\CmdRef\auarf090.htm >> %FILEGROUP%
echo file90=%IS5_DOC%\CmdRef\auarf091.htm >> %FILEGROUP%
echo file91=%IS5_DOC%\CmdRef\auarf092.htm >> %FILEGROUP%
echo file92=%IS5_DOC%\CmdRef\auarf093.htm >> %FILEGROUP%
echo file93=%IS5_DOC%\CmdRef\auarf094.htm >> %FILEGROUP%
echo file94=%IS5_DOC%\CmdRef\auarf095.htm >> %FILEGROUP%
echo file95=%IS5_DOC%\CmdRef\auarf096.htm >> %FILEGROUP%
echo file96=%IS5_DOC%\CmdRef\auarf097.htm >> %FILEGROUP%
echo file97=%IS5_DOC%\CmdRef\auarf098.htm >> %FILEGROUP%
echo file98=%IS5_DOC%\CmdRef\auarf099.htm >> %FILEGROUP%
echo file99=%IS5_DOC%\CmdRef\auarf100.htm >> %FILEGROUP%
echo file100=%IS5_DOC%\CmdRef\auarf101.htm >> %FILEGROUP%
echo file101=%IS5_DOC%\CmdRef\auarf102.htm >> %FILEGROUP%
echo file102=%IS5_DOC%\CmdRef\auarf103.htm >> %FILEGROUP%
echo file103=%IS5_DOC%\CmdRef\auarf104.htm >> %FILEGROUP%
echo file104=%IS5_DOC%\CmdRef\auarf105.htm >> %FILEGROUP%
echo file105=%IS5_DOC%\CmdRef\auarf106.htm >> %FILEGROUP%
echo file106=%IS5_DOC%\CmdRef\auarf107.htm >> %FILEGROUP%
echo file107=%IS5_DOC%\CmdRef\auarf108.htm >> %FILEGROUP%
echo file108=%IS5_DOC%\CmdRef\auarf109.htm >> %FILEGROUP%
echo file109=%IS5_DOC%\CmdRef\auarf110.htm >> %FILEGROUP%
echo file110=%IS5_DOC%\CmdRef\auarf111.htm >> %FILEGROUP%
echo file111=%IS5_DOC%\CmdRef\auarf112.htm >> %FILEGROUP%
echo file112=%IS5_DOC%\CmdRef\auarf113.htm >> %FILEGROUP%
echo file113=%IS5_DOC%\CmdRef\auarf114.htm >> %FILEGROUP%
echo file114=%IS5_DOC%\CmdRef\auarf115.htm >> %FILEGROUP%
echo file115=%IS5_DOC%\CmdRef\auarf116.htm >> %FILEGROUP%
echo file116=%IS5_DOC%\CmdRef\auarf117.htm >> %FILEGROUP%
echo file117=%IS5_DOC%\CmdRef\auarf118.htm >> %FILEGROUP%
echo file118=%IS5_DOC%\CmdRef\auarf119.htm >> %FILEGROUP%
echo file119=%IS5_DOC%\CmdRef\auarf120.htm >> %FILEGROUP%
echo file120=%IS5_DOC%\CmdRef\auarf121.htm >> %FILEGROUP%
echo file121=%IS5_DOC%\CmdRef\auarf122.htm >> %FILEGROUP%
echo file122=%IS5_DOC%\CmdRef\auarf123.htm >> %FILEGROUP%
echo file123=%IS5_DOC%\CmdRef\auarf124.htm >> %FILEGROUP%
echo file124=%IS5_DOC%\CmdRef\auarf125.htm >> %FILEGROUP%
echo file125=%IS5_DOC%\CmdRef\auarf126.htm >> %FILEGROUP%
echo file126=%IS5_DOC%\CmdRef\auarf127.htm >> %FILEGROUP%
echo file127=%IS5_DOC%\CmdRef\auarf128.htm >> %FILEGROUP%
echo file128=%IS5_DOC%\CmdRef\auarf129.htm >> %FILEGROUP%
echo file129=%IS5_DOC%\CmdRef\auarf130.htm >> %FILEGROUP%
echo file130=%IS5_DOC%\CmdRef\auarf131.htm >> %FILEGROUP%
echo file131=%IS5_DOC%\CmdRef\auarf132.htm >> %FILEGROUP%
echo file132=%IS5_DOC%\CmdRef\auarf133.htm >> %FILEGROUP%
echo file133=%IS5_DOC%\CmdRef\auarf134.htm >> %FILEGROUP%
echo file134=%IS5_DOC%\CmdRef\auarf135.htm >> %FILEGROUP%
echo file135=%IS5_DOC%\CmdRef\auarf136.htm >> %FILEGROUP%
echo file136=%IS5_DOC%\CmdRef\auarf137.htm >> %FILEGROUP%
echo file137=%IS5_DOC%\CmdRef\auarf138.htm >> %FILEGROUP%
echo file138=%IS5_DOC%\CmdRef\auarf139.htm >> %FILEGROUP%
echo file139=%IS5_DOC%\CmdRef\auarf140.htm >> %FILEGROUP%
echo file140=%IS5_DOC%\CmdRef\auarf141.htm >> %FILEGROUP%
echo file141=%IS5_DOC%\CmdRef\auarf142.htm >> %FILEGROUP%
echo file142=%IS5_DOC%\CmdRef\auarf143.htm >> %FILEGROUP%
echo file143=%IS5_DOC%\CmdRef\auarf144.htm >> %FILEGROUP%
echo file144=%IS5_DOC%\CmdRef\auarf145.htm >> %FILEGROUP%
echo file145=%IS5_DOC%\CmdRef\auarf146.htm >> %FILEGROUP%
echo file146=%IS5_DOC%\CmdRef\auarf147.htm >> %FILEGROUP%
echo file147=%IS5_DOC%\CmdRef\auarf148.htm >> %FILEGROUP%
echo file148=%IS5_DOC%\CmdRef\auarf149.htm >> %FILEGROUP%
echo file149=%IS5_DOC%\CmdRef\auarf150.htm >> %FILEGROUP%
echo file150=%IS5_DOC%\CmdRef\auarf151.htm >> %FILEGROUP%
echo file151=%IS5_DOC%\CmdRef\auarf152.htm >> %FILEGROUP%
echo file152=%IS5_DOC%\CmdRef\auarf153.htm >> %FILEGROUP%
echo file153=%IS5_DOC%\CmdRef\auarf154.htm >> %FILEGROUP%
echo file154=%IS5_DOC%\CmdRef\auarf155.htm >> %FILEGROUP%
echo file155=%IS5_DOC%\CmdRef\auarf156.htm >> %FILEGROUP%
echo file156=%IS5_DOC%\CmdRef\auarf157.htm >> %FILEGROUP%
echo file157=%IS5_DOC%\CmdRef\auarf158.htm >> %FILEGROUP%
echo file158=%IS5_DOC%\CmdRef\auarf159.htm >> %FILEGROUP%
echo file159=%IS5_DOC%\CmdRef\auarf160.htm >> %FILEGROUP%
echo file160=%IS5_DOC%\CmdRef\auarf161.htm >> %FILEGROUP%
echo file161=%IS5_DOC%\CmdRef\auarf162.htm >> %FILEGROUP%
echo file162=%IS5_DOC%\CmdRef\auarf163.htm >> %FILEGROUP%
echo file163=%IS5_DOC%\CmdRef\auarf164.htm >> %FILEGROUP%
echo file164=%IS5_DOC%\CmdRef\auarf165.htm >> %FILEGROUP%
echo file165=%IS5_DOC%\CmdRef\auarf166.htm >> %FILEGROUP%
echo file166=%IS5_DOC%\CmdRef\auarf167.htm >> %FILEGROUP%
echo file167=%IS5_DOC%\CmdRef\auarf168.htm >> %FILEGROUP%
echo file168=%IS5_DOC%\CmdRef\auarf169.htm >> %FILEGROUP%
echo file169=%IS5_DOC%\CmdRef\auarf170.htm >> %FILEGROUP%
echo file170=%IS5_DOC%\CmdRef\auarf171.htm >> %FILEGROUP%
echo file171=%IS5_DOC%\CmdRef\auarf172.htm >> %FILEGROUP%
echo file172=%IS5_DOC%\CmdRef\auarf173.htm >> %FILEGROUP%
echo file173=%IS5_DOC%\CmdRef\auarf174.htm >> %FILEGROUP%
echo file174=%IS5_DOC%\CmdRef\auarf175.htm >> %FILEGROUP%
echo file175=%IS5_DOC%\CmdRef\auarf176.htm >> %FILEGROUP%
echo file176=%IS5_DOC%\CmdRef\auarf177.htm >> %FILEGROUP%
echo file177=%IS5_DOC%\CmdRef\auarf178.htm >> %FILEGROUP%
echo file178=%IS5_DOC%\CmdRef\auarf179.htm >> %FILEGROUP%
echo file179=%IS5_DOC%\CmdRef\auarf180.htm >> %FILEGROUP%
echo file180=%IS5_DOC%\CmdRef\auarf181.htm >> %FILEGROUP%
echo file181=%IS5_DOC%\CmdRef\auarf182.htm >> %FILEGROUP%
echo file182=%IS5_DOC%\CmdRef\auarf183.htm >> %FILEGROUP%
echo file183=%IS5_DOC%\CmdRef\auarf184.htm >> %FILEGROUP%
echo file184=%IS5_DOC%\CmdRef\auarf185.htm >> %FILEGROUP%
echo file185=%IS5_DOC%\CmdRef\auarf186.htm >> %FILEGROUP%
echo file186=%IS5_DOC%\CmdRef\auarf187.htm >> %FILEGROUP%
echo file187=%IS5_DOC%\CmdRef\auarf188.htm >> %FILEGROUP%
echo file188=%IS5_DOC%\CmdRef\auarf189.htm >> %FILEGROUP%
echo file189=%IS5_DOC%\CmdRef\auarf190.htm >> %FILEGROUP%
echo file190=%IS5_DOC%\CmdRef\auarf191.htm >> %FILEGROUP%
echo file191=%IS5_DOC%\CmdRef\auarf192.htm >> %FILEGROUP%
echo file192=%IS5_DOC%\CmdRef\auarf193.htm >> %FILEGROUP%
echo file193=%IS5_DOC%\CmdRef\auarf194.htm >> %FILEGROUP%
echo file194=%IS5_DOC%\CmdRef\auarf195.htm >> %FILEGROUP%
echo file195=%IS5_DOC%\CmdRef\auarf196.htm >> %FILEGROUP%
echo file196=%IS5_DOC%\CmdRef\auarf197.htm >> %FILEGROUP%
echo file197=%IS5_DOC%\CmdRef\auarf198.htm >> %FILEGROUP%
echo file198=%IS5_DOC%\CmdRef\auarf199.htm >> %FILEGROUP%
echo file199=%IS5_DOC%\CmdRef\auarf200.htm >> %FILEGROUP%
echo file200=%IS5_DOC%\CmdRef\auarf201.htm >> %FILEGROUP%
echo file201=%IS5_DOC%\CmdRef\auarf202.htm >> %FILEGROUP%
echo file202=%IS5_DOC%\CmdRef\auarf203.htm >> %FILEGROUP%
echo file203=%IS5_DOC%\CmdRef\auarf204.htm >> %FILEGROUP%
echo file204=%IS5_DOC%\CmdRef\auarf205.htm >> %FILEGROUP%
echo file205=%IS5_DOC%\CmdRef\auarf206.htm >> %FILEGROUP%
echo file206=%IS5_DOC%\CmdRef\auarf207.htm >> %FILEGROUP%
echo file207=%IS5_DOC%\CmdRef\auarf208.htm >> %FILEGROUP%
echo file208=%IS5_DOC%\CmdRef\auarf209.htm >> %FILEGROUP%
echo file209=%IS5_DOC%\CmdRef\auarf210.htm >> %FILEGROUP%
echo file210=%IS5_DOC%\CmdRef\auarf211.htm >> %FILEGROUP%
echo file211=%IS5_DOC%\CmdRef\auarf212.htm >> %FILEGROUP%
echo file212=%IS5_DOC%\CmdRef\auarf213.htm >> %FILEGROUP%
echo file213=%IS5_DOC%\CmdRef\auarf214.htm >> %FILEGROUP%
echo file214=%IS5_DOC%\CmdRef\auarf215.htm >> %FILEGROUP%
echo file215=%IS5_DOC%\CmdRef\auarf216.htm >> %FILEGROUP%
echo file216=%IS5_DOC%\CmdRef\auarf217.htm >> %FILEGROUP%
echo file217=%IS5_DOC%\CmdRef\auarf218.htm >> %FILEGROUP%
echo file218=%IS5_DOC%\CmdRef\auarf219.htm >> %FILEGROUP%
echo file219=%IS5_DOC%\CmdRef\auarf220.htm >> %FILEGROUP%
echo file220=%IS5_DOC%\CmdRef\auarf221.htm >> %FILEGROUP%
echo file221=%IS5_DOC%\CmdRef\auarf222.htm >> %FILEGROUP%
echo file222=%IS5_DOC%\CmdRef\auarf223.htm >> %FILEGROUP%
echo file223=%IS5_DOC%\CmdRef\auarf224.htm >> %FILEGROUP%
echo file224=%IS5_DOC%\CmdRef\auarf225.htm >> %FILEGROUP%
echo file225=%IS5_DOC%\CmdRef\auarf226.htm >> %FILEGROUP%
echo file226=%IS5_DOC%\CmdRef\auarf227.htm >> %FILEGROUP%
echo file227=%IS5_DOC%\CmdRef\auarf228.htm >> %FILEGROUP%
echo file228=%IS5_DOC%\CmdRef\auarf229.htm >> %FILEGROUP%
echo file229=%IS5_DOC%\CmdRef\auarf230.htm >> %FILEGROUP%
echo file230=%IS5_DOC%\CmdRef\auarf231.htm >> %FILEGROUP%
echo file231=%IS5_DOC%\CmdRef\auarf232.htm >> %FILEGROUP%
echo file232=%IS5_DOC%\CmdRef\auarf233.htm >> %FILEGROUP%
echo file233=%IS5_DOC%\CmdRef\auarf234.htm >> %FILEGROUP%
echo file234=%IS5_DOC%\CmdRef\auarf235.htm >> %FILEGROUP%
echo file235=%IS5_DOC%\CmdRef\auarf236.htm >> %FILEGROUP%
echo file236=%IS5_DOC%\CmdRef\auarf237.htm >> %FILEGROUP%
echo file237=%IS5_DOC%\CmdRef\auarf238.htm >> %FILEGROUP%
echo file238=%IS5_DOC%\CmdRef\auarf239.htm >> %FILEGROUP%
echo file239=%IS5_DOC%\CmdRef\auarf240.htm >> %FILEGROUP%
echo file240=%IS5_DOC%\CmdRef\auarf241.htm >> %FILEGROUP%
echo file241=%IS5_DOC%\CmdRef\auarf242.htm >> %FILEGROUP%
echo file242=%IS5_DOC%\CmdRef\auarf243.htm >> %FILEGROUP%
echo file243=%IS5_DOC%\CmdRef\auarf244.htm >> %FILEGROUP%
echo file244=%IS5_DOC%\CmdRef\auarf245.htm >> %FILEGROUP%
echo file245=%IS5_DOC%\CmdRef\auarf246.htm >> %FILEGROUP%
echo file246=%IS5_DOC%\CmdRef\auarf247.htm >> %FILEGROUP%
echo file247=%IS5_DOC%\CmdRef\auarf248.htm >> %FILEGROUP%
echo file248=%IS5_DOC%\CmdRef\auarf249.htm >> %FILEGROUP%
echo file249=%IS5_DOC%\CmdRef\auarf250.htm >> %FILEGROUP%
echo file250=%IS5_DOC%\CmdRef\auarf251.htm >> %FILEGROUP%
echo file251=%IS5_DOC%\CmdRef\auarf252.htm >> %FILEGROUP%
echo file252=%IS5_DOC%\CmdRef\auarf253.htm >> %FILEGROUP%
echo file253=%IS5_DOC%\CmdRef\auarf254.htm >> %FILEGROUP%
echo file254=%IS5_DOC%\CmdRef\auarf255.htm >> %FILEGROUP%
echo file255=%IS5_DOC%\CmdRef\auarf256.htm >> %FILEGROUP%
echo file256=%IS5_DOC%\CmdRef\auarf257.htm >> %FILEGROUP%
echo file257=%IS5_DOC%\CmdRef\auarf258.htm >> %FILEGROUP%
echo file258=%IS5_DOC%\CmdRef\auarf259.htm >> %FILEGROUP%
echo file259=%IS5_DOC%\CmdRef\auarf260.htm >> %FILEGROUP%
echo file260=%IS5_DOC%\CmdRef\auarf261.htm >> %FILEGROUP%
echo file261=%IS5_DOC%\CmdRef\auarf262.htm >> %FILEGROUP%
echo file262=%IS5_DOC%\CmdRef\auarf263.htm >> %FILEGROUP%
echo file263=%IS5_DOC%\CmdRef\auarf264.htm >> %FILEGROUP%
echo file264=%IS5_DOC%\CmdRef\auarf265.htm >> %FILEGROUP%
echo file265=%IS5_DOC%\CmdRef\auarf266.htm >> %FILEGROUP%
echo file266=%IS5_DOC%\CmdRef\auarf267.htm >> %FILEGROUP%
echo file267=%IS5_DOC%\CmdRef\auarf268.htm >> %FILEGROUP%
echo file268=%IS5_DOC%\CmdRef\auarf269.htm >> %FILEGROUP%
echo file269=%IS5_DOC%\CmdRef\auarf270.htm >> %FILEGROUP%
echo file270=%IS5_DOC%\CmdRef\auarf271.htm >> %FILEGROUP%
echo file271=%IS5_DOC%\CmdRef\auarf272.htm >> %FILEGROUP%
echo file272=%IS5_DOC%\CmdRef\auarf273.htm >> %FILEGROUP%
echo file273=%IS5_DOC%\CmdRef\auarf274.htm >> %FILEGROUP%
echo file274=%IS5_DOC%\CmdRef\auarf275.htm >> %FILEGROUP%
echo file275=%IS5_DOC%\CmdRef\auarf276.htm >> %FILEGROUP%
echo file276=%IS5_DOC%\CmdRef\auarf277.htm >> %FILEGROUP%
echo file277=%IS5_DOC%\CmdRef\auarf278.htm >> %FILEGROUP%
echo file278=%IS5_DOC%\CmdRef\auarf279.htm >> %FILEGROUP%
echo file279=%IS5_DOC%\CmdRef\auarf280.htm >> %FILEGROUP%
echo file280=%IS5_DOC%\CmdRef\auarf281.htm >> %FILEGROUP%
echo file281=%IS5_DOC%\CmdRef\auarf282.htm >> %FILEGROUP%
echo file282=%IS5_DOC%\CmdRef\auarf283.htm >> %FILEGROUP%
echo file283=%IS5_DOC%\CmdRef\auarf284.htm >> %FILEGROUP%
echo.  >> %FILEGROUP%
echo [General] >> %FILEGROUP%
echo Type=FILELIST >> %FILEGROUP%
echo Version=1.00.000 >> %FILEGROUP%
goto :EOF

rem -------------- Generate the Doc Misc file groups --------------------

:Doc_Misc_Files

rem English
set FILEGROUP=Doc_Misc_English_Files.fgl
set IS5_LANG=en_US
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\en_US\Html
call :Generate_Doc_Misc_File_Group
copy %AFSROOT%\src\WINNT\license\lang\%IS5_LANG%.rtf %AFSROOT%\src\WINNT\install\InstallShield5\lang\%IS5_LANG%\license.rtf

rem Japanese
set FILEGROUP=Doc_Misc_Japanese_Files.fgl
set IS5_LANG=ja_JP
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\ja_JP\Html
call :Generate_Doc_Misc_File_Group
copy %AFSROOT%\src\WINNT\license\lang\%IS5_LANG%.rtf %AFSROOT%\src\WINNT\install\InstallShield5\lang\%IS5_LANG%\license.rtf

rem Korean
set FILEGROUP=Doc_Misc_Korean_Files.fgl
set IS5_LANG=ko_KR
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\ko_KR\Html
call :Generate_Doc_Misc_File_Group
copy %AFSROOT%\src\WINNT\license\lang\%IS5_LANG%.rtf %AFSROOT%\src\WINNT\install\InstallShield5\lang\%IS5_LANG%\license.rtf

rem Trad_Chinese
set FILEGROUP=Doc_Misc_Trad_Chinese_Files.fgl
set IS5_LANG=zh_TW
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\zh_TW\Html
call :Generate_Doc_Misc_File_Group
copy %AFSROOT%\src\WINNT\license\lang\%IS5_LANG%.rtf %AFSROOT%\src\WINNT\install\InstallShield5\lang\%IS5_LANG%\license.rtf

rem Simp_Chinese
set FILEGROUP=Doc_Misc_Simp_Chinese_Files.fgl
set IS5_LANG=zh_CN
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\zh_CN\Html
call :Generate_Doc_Misc_File_Group
copy %AFSROOT%\src\WINNT\license\lang\%IS5_LANG%.rtf %AFSROOT%\src\WINNT\install\InstallShield5\lang\%IS5_LANG%\license.rtf

rem German
set FILEGROUP=Doc_Misc_German_Files.fgl
set IS5_LANG=de_DE
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\de_DE\Html
call :Generate_Doc_Misc_File_Group
copy %AFSROOT%\src\WINNT\license\lang\%IS5_LANG%.rtf %AFSROOT%\src\WINNT\install\InstallShield5\lang\%IS5_LANG%\license.rtf

rem Spanish
set FILEGROUP=Doc_Misc_Spanish_Files.fgl
set IS5_LANG=es_ES
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\es_ES\Html
call :Generate_Doc_Misc_File_Group
copy %AFSROOT%\src\WINNT\license\lang\%IS5_LANG%.rtf %AFSROOT%\src\WINNT\install\InstallShield5\lang\%IS5_LANG%\license.rtf

rem Portuguese
set FILEGROUP=Doc_Misc_Portuguese_Files.fgl
set IS5_LANG=pt_BR
set IS5_DOC=%IS5_DOCROOT%\install\Documentation\pt_BR\Html
call :Generate_Doc_Misc_File_Group
copy %AFSROOT%\src\WINNT\license\lang\%IS5_LANG%.rtf %AFSROOT%\src\WINNT\install\InstallShield5\lang\%IS5_LANG%\license.rtf

goto Doc_Files

:Generate_Doc_Misc_File_Group
echo [TopDir] > %FILEGROUP%
echo file0=%IS5_DOC%\banner.gif >> %FILEGROUP%
echo file1=%IS5_DOC%\books.gif >> %FILEGROUP%
echo file2=%IS5_DOC%\bot.gif >> %FILEGROUP%
echo file3=%IS5_DOC%\index.gif >> %FILEGROUP%
echo file4=%IS5_DOC%\index.htm >> %FILEGROUP%
echo file5=%IS5_DOC%\next.gif >> %FILEGROUP%
echo file6=%IS5_DOC%\prev.gif >> %FILEGROUP%
echo file7=%IS5_DOC%\toc.gif >> %FILEGROUP%
echo file8=%IS5_DOC%\top.gif >> %FILEGROUP%
echo file9=%AFSROOT%\src\WINNT\install\InstallShield5\lang\%IS5_LANG%\license.rtf >> %FILEGROUP%
echo.  >> %FILEGROUP%
echo [General] >> %FILEGROUP%
echo Type=FILELIST >> %FILEGROUP%
echo Version=1.00.000 >> %FILEGROUP%
goto :EOF

rem -------------- Doc_Files.fgl ------------------------------

:Doc_Files
echo [TopDir] > Doc_Files.fgl
echo file0=%IS5_DEST%\root.server\usr\afs\bin\DocsUninst.dll >> Doc_Files.fgl
echo.  >> Doc_Files.fgl
echo [General] >> Doc_Files.fgl
echo Type=FILELIST >> Doc_Files.fgl
echo Version=1.00.000 >> Doc_Files.fgl


rem -------------- Lang_English_Files.fgl ------------------------------

echo [TopDir] > Lang_English_Files.fgl
echo file0=%IS5_DEST%\root.server\usr\afs\bin\afseventmsg_1033.dll >> Lang_English_Files.fgl
echo file1=%IS5_DEST%\root.server\usr\afs\bin\afs_setup_utils_1033.dll >> Lang_English_Files.fgl
echo file2=%IS5_DEST%\root.server\usr\afs\bin\afsserver_1033.dll >> Lang_English_Files.fgl
echo file3=%IS5_DEST%\root.server\usr\afs\bin\afssvrcfg_1033.dll >> Lang_English_Files.fgl
echo file4=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAccountManager_1033.dll >> Lang_English_Files.fgl
echo file5=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAppLib_1033.dll >> Lang_English_Files.fgl
echo file6=%IS5_DEST%\root.server\usr\afs\bin\TaAfsServerManager_1033.dll >> Lang_English_Files.fgl
echo file7=%IS5_DEST%\root.client\usr\vice\etc\afscreds_1033.dll >> Lang_English_Files.fgl
echo file8=%IS5_DEST%\root.client\usr\vice\etc\afs_config_1033.dll >> Lang_English_Files.fgl
echo file9=%IS5_DEST%\root.client\usr\vice\etc\afs_cpa_1033.dll >> Lang_English_Files.fgl
echo file10=%IS5_DEST%\root.client\usr\vice\etc\afs_shl_ext_1033.dll >> Lang_English_Files.fgl
echo file11=%IS5_HELP%\en_US\afs-nt.hlp >> Lang_English_Files.fgl
echo file12=%IS5_HELP%\en_US\afs-nt.cnt >> Lang_English_Files.fgl
echo file13=%IS5_HELP%\en_US\taafssvrmgr.cnt >> Lang_English_Files.fgl
echo file14=%IS5_HELP%\en_US\taafssvrmgr.hlp >> Lang_English_Files.fgl
echo file15=%IS5_HELP%\en_US\taafsusrmgr.cnt >> Lang_English_Files.fgl
echo file16=%IS5_HELP%\en_US\taafsusrmgr.hlp >> Lang_English_Files.fgl
echo file17=%IS5_HELP%\en_US\afs-cc.cnt >> Lang_English_Files.fgl
echo file18=%IS5_HELP%\en_US\afs-cc.hlp >> Lang_English_Files.fgl
echo file19=%IS5_HELP%\en_US\afs-light.cnt >> Lang_English_Files.fgl
echo file20=%IS5_HELP%\en_US\afs-light.hlp >> Lang_English_Files.fgl
echo file21=%IS5_HELP%\en_US\taafscfg.cnt >> Lang_English_Files.fgl
echo file22=%IS5_HELP%\en_US\taafscfg.hlp >> Lang_English_Files.fgl
echo.  >> Lang_English_Files.fgl
echo [General] >> Lang_English_Files.fgl
echo Type=FILELIST >> Lang_English_Files.fgl
echo Version=1.00.000 >> Lang_English_Files.fgl


rem -------------- Lang_Simp_Chinese_Files.fgl -------------------------

echo [TopDir] > Lang_Simp_Chinese_Files.fgl
echo file0=%IS5_DEST%\root.server\usr\afs\bin\afseventmsg_2052.dll >> Lang_Simp_Chinese_Files.fgl
echo file1=%IS5_DEST%\root.server\usr\afs\bin\afs_setup_utils_2052.dll >> Lang_Simp_Chinese_Files.fgl
echo file2=%IS5_DEST%\root.server\usr\afs\bin\afsserver_2052.dll >> Lang_Simp_Chinese_Files.fgl
echo file3=%IS5_DEST%\root.server\usr\afs\bin\afssvrcfg_2052.dll >> Lang_Simp_Chinese_Files.fgl
echo file4=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAccountManager_2052.dll >> Lang_Simp_Chinese_Files.fgl
echo file5=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAppLib_2052.dll >> Lang_Simp_Chinese_Files.fgl
echo file6=%IS5_DEST%\root.server\usr\afs\bin\TaAfsServerManager_2052.dll >> Lang_Simp_Chinese_Files.fgl
echo file7=%IS5_DEST%\root.client\usr\vice\etc\afscreds_2052.dll >> Lang_Simp_Chinese_Files.fgl
echo file8=%IS5_DEST%\root.client\usr\vice\etc\afs_config_2052.dll >> Lang_Simp_Chinese_Files.fgl
echo file9=%IS5_DEST%\root.client\usr\vice\etc\afs_cpa_2052.dll >> Lang_Simp_Chinese_Files.fgl
echo file10=%IS5_DEST%\root.client\usr\vice\etc\afs_shl_ext_2052.dll >> Lang_Simp_Chinese_Files.fgl
echo file11=%IS5_HELP%\zh_CN\afs-nt.hlp >> Lang_Simp_Chinese_Files.fgl
echo file12=%IS5_HELP%\zh_CN\afs-nt.cnt >> Lang_Simp_Chinese_Files.fgl
echo file13=%IS5_HELP%\zh_CN\taafssvrmgr.cnt >> Lang_Simp_Chinese_Files.fgl
echo file14=%IS5_HELP%\zh_CN\taafssvrmgr.hlp >> Lang_Simp_Chinese_Files.fgl
echo file15=%IS5_HELP%\zh_CN\taafsusrmgr.cnt >> Lang_Simp_Chinese_Files.fgl
echo file16=%IS5_HELP%\zh_CN\taafsusrmgr.hlp >> Lang_Simp_Chinese_Files.fgl
echo file17=%IS5_HELP%\zh_CN\afs-cc.cnt >> Lang_Simp_Chinese_Files.fgl
echo file18=%IS5_HELP%\zh_CN\afs-cc.hlp >> Lang_Simp_Chinese_Files.fgl
echo file19=%IS5_HELP%\zh_CN\afs-light.cnt >> Lang_Simp_Chinese_Files.fgl
echo file20=%IS5_HELP%\zh_CN\afs-light.hlp >> Lang_Simp_Chinese_Files.fgl
echo file21=%IS5_HELP%\zh_CN\taafscfg.cnt >> Lang_Simp_Chinese_Files.fgl
echo file22=%IS5_HELP%\zh_CN\taafscfg.hlp >> Lang_Simp_Chinese_Files.fgl
echo.  >> Lang_Simp_Chinese_Files.fgl
echo [General] >> Lang_Simp_Chinese_Files.fgl
echo Type=FILELIST >> Lang_Simp_Chinese_Files.fgl
echo Version=1.00.000 >> Lang_Simp_Chinese_Files.fgl


rem -------------- Lang_Trad_Chinese_Files.fgl -------------------------

echo [TopDir] > Lang_Trad_Chinese_Files.fgl
echo file0=%IS5_DEST%\root.server\usr\afs\bin\afseventmsg_1028.dll >> Lang_Trad_Chinese_Files.fgl
echo file1=%IS5_DEST%\root.server\usr\afs\bin\afs_setup_utils_1028.dll >> Lang_Trad_Chinese_Files.fgl
echo file2=%IS5_DEST%\root.server\usr\afs\bin\afsserver_1028.dll >> Lang_Trad_Chinese_Files.fgl
echo file3=%IS5_DEST%\root.server\usr\afs\bin\afssvrcfg_1028.dll >> Lang_Trad_Chinese_Files.fgl
echo file4=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAccountManager_1028.dll >> Lang_Trad_Chinese_Files.fgl
echo file5=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAppLib_1028.dll >> Lang_Trad_Chinese_Files.fgl
echo file6=%IS5_DEST%\root.server\usr\afs\bin\TaAfsServerManager_1028.dll >> Lang_Trad_Chinese_Files.fgl
echo file7=%IS5_DEST%\root.client\usr\vice\etc\afscreds_1028.dll >> Lang_Trad_Chinese_Files.fgl
echo file8=%IS5_DEST%\root.client\usr\vice\etc\afs_config_1028.dll >> Lang_Trad_Chinese_Files.fgl
echo file9=%IS5_DEST%\root.client\usr\vice\etc\afs_cpa_1028.dll >> Lang_Trad_Chinese_Files.fgl
echo file10=%IS5_DEST%\root.client\usr\vice\etc\afs_shl_ext_1028.dll >> Lang_Trad_Chinese_Files.fgl
echo file11=%IS5_HELP%\zh_TW\afs-nt.hlp >> Lang_Trad_Chinese_Files.fgl
echo file12=%IS5_HELP%\zh_TW\afs-nt.cnt >> Lang_Trad_Chinese_Files.fgl
echo file13=%IS5_HELP%\zh_TW\taafssvrmgr.cnt >> Lang_Trad_Chinese_Files.fgl
echo file14=%IS5_HELP%\zh_TW\taafssvrmgr.hlp >> Lang_Trad_Chinese_Files.fgl
echo file15=%IS5_HELP%\zh_TW\taafsusrmgr.cnt >> Lang_Trad_Chinese_Files.fgl
echo file16=%IS5_HELP%\zh_TW\taafsusrmgr.hlp >> Lang_Trad_Chinese_Files.fgl
echo file17=%IS5_HELP%\zh_TW\afs-cc.cnt >> Lang_Trad_Chinese_Files.fgl
echo file18=%IS5_HELP%\zh_TW\afs-cc.hlp >> Lang_Trad_Chinese_Files.fgl
echo file19=%IS5_HELP%\zh_TW\afs-light.cnt >> Lang_Trad_Chinese_Files.fgl
echo file20=%IS5_HELP%\zh_TW\afs-light.hlp >> Lang_Trad_Chinese_Files.fgl
echo file21=%IS5_HELP%\zh_TW\taafscfg.cnt >> Lang_Trad_Chinese_Files.fgl
echo file22=%IS5_HELP%\zh_TW\taafscfg.hlp >> Lang_Trad_Chinese_Files.fgl
echo.  >> Lang_Trad_Chinese_Files.fgl
echo [General] >> Lang_Trad_Chinese_Files.fgl
echo Type=FILELIST >> Lang_Trad_Chinese_Files.fgl
echo Version=1.00.000 >> Lang_Trad_Chinese_Files.fgl


rem -------------- Lang_Korean_Files.fgl -------------------------------

echo [TopDir] > Lang_Korean_Files.fgl
echo file0=%IS5_DEST%\root.server\usr\afs\bin\afseventmsg_1042.dll >> Lang_Korean_Files.fgl
echo file1=%IS5_DEST%\root.server\usr\afs\bin\afs_setup_utils_1042.dll >> Lang_Korean_Files.fgl
echo file2=%IS5_DEST%\root.server\usr\afs\bin\afsserver_1042.dll >> Lang_Korean_Files.fgl
echo file3=%IS5_DEST%\root.server\usr\afs\bin\afssvrcfg_1042.dll >> Lang_Korean_Files.fgl
echo file4=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAccountManager_1042.dll >> Lang_Korean_Files.fgl
echo file5=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAppLib_1042.dll >> Lang_Korean_Files.fgl
echo file6=%IS5_DEST%\root.server\usr\afs\bin\TaAfsServerManager_1042.dll >> Lang_Korean_Files.fgl
echo file7=%IS5_DEST%\root.client\usr\vice\etc\afscreds_1042.dll >> Lang_Korean_Files.fgl
echo file8=%IS5_DEST%\root.client\usr\vice\etc\afs_config_1042.dll >> Lang_Korean_Files.fgl
echo file9=%IS5_DEST%\root.client\usr\vice\etc\afs_cpa_1042.dll >> Lang_Korean_Files.fgl
echo file10=%IS5_DEST%\root.client\usr\vice\etc\afs_shl_ext_1042.dll >> Lang_Korean_Files.fgl
echo file11=%IS5_HELP%\ko_KR\afs-nt.hlp >> Lang_Korean_Files.fgl
echo file12=%IS5_HELP%\ko_KR\afs-nt.cnt >> Lang_Korean_Files.fgl
echo file13=%IS5_HELP%\ko_KR\taafssvrmgr.cnt >> Lang_Korean_Files.fgl
echo file14=%IS5_HELP%\ko_KR\taafssvrmgr.hlp >> Lang_Korean_Files.fgl
echo file15=%IS5_HELP%\ko_KR\taafsusrmgr.cnt >> Lang_Korean_Files.fgl
echo file16=%IS5_HELP%\ko_KR\taafsusrmgr.hlp >> Lang_Korean_Files.fgl
echo file17=%IS5_HELP%\ko_KR\afs-cc.cnt >> Lang_Korean_Files.fgl
echo file18=%IS5_HELP%\ko_KR\afs-cc.hlp >> Lang_Korean_Files.fgl
echo file19=%IS5_HELP%\ko_KR\afs-light.cnt >> Lang_Korean_Files.fgl
echo file20=%IS5_HELP%\ko_KR\afs-light.hlp >> Lang_Korean_Files.fgl
echo file21=%IS5_HELP%\ko_KR\taafscfg.cnt >> Lang_Korean_Files.fgl
echo file22=%IS5_HELP%\ko_KR\taafscfg.hlp >> Lang_Korean_Files.fgl
echo.  >> Lang_Korean_Files.fgl
echo [General] >> Lang_Korean_Files.fgl
echo Type=FILELIST >> Lang_Korean_Files.fgl
echo Version=1.00.000 >> Lang_Korean_Files.fgl


rem -------------- Lang_Japanese_Files.fgl -----------------------------

echo [TopDir] > Lang_Japanese_Files.fgl
echo file0=%IS5_DEST%\root.server\usr\afs\bin\afseventmsg_1041.dll >> Lang_Japanese_Files.fgl
echo file1=%IS5_DEST%\root.server\usr\afs\bin\afs_setup_utils_1041.dll >> Lang_Japanese_Files.fgl
echo file2=%IS5_DEST%\root.server\usr\afs\bin\afsserver_1041.dll >> Lang_Japanese_Files.fgl
echo file3=%IS5_DEST%\root.server\usr\afs\bin\afssvrcfg_1041.dll >> Lang_Japanese_Files.fgl
echo file4=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAccountManager_1041.dll >> Lang_Japanese_Files.fgl
echo file5=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAppLib_1041.dll >> Lang_Japanese_Files.fgl
echo file6=%IS5_DEST%\root.server\usr\afs\bin\TaAfsServerManager_1041.dll >> Lang_Japanese_Files.fgl
echo file7=%IS5_DEST%\root.client\usr\vice\etc\afscreds_1041.dll >> Lang_Japanese_Files.fgl
echo file8=%IS5_DEST%\root.client\usr\vice\etc\afs_config_1041.dll >> Lang_Japanese_Files.fgl
echo file9=%IS5_DEST%\root.client\usr\vice\etc\afs_cpa_1041.dll >> Lang_Japanese_Files.fgl
echo file10=%IS5_DEST%\root.client\usr\vice\etc\afs_shl_ext_1041.dll >> Lang_Japanese_Files.fgl
echo file11=%IS5_HELP%\ja_JP\afs-nt.hlp >> Lang_Japanese_Files.fgl
echo file12=%IS5_HELP%\ja_JP\afs-nt.cnt >> Lang_Japanese_Files.fgl
echo file13=%IS5_HELP%\ja_JP\taafssvrmgr.cnt >> Lang_Japanese_Files.fgl
echo file14=%IS5_HELP%\ja_JP\taafssvrmgr.hlp >> Lang_Japanese_Files.fgl
echo file15=%IS5_HELP%\ja_JP\taafsusrmgr.cnt >> Lang_Japanese_Files.fgl
echo file16=%IS5_HELP%\ja_JP\taafsusrmgr.hlp >> Lang_Japanese_Files.fgl
echo file17=%IS5_HELP%\ja_JP\afs-cc.cnt >> Lang_Japanese_Files.fgl
echo file18=%IS5_HELP%\ja_JP\afs-cc.hlp >> Lang_Japanese_Files.fgl
echo file19=%IS5_HELP%\ja_JP\afs-light.cnt >> Lang_Japanese_Files.fgl
echo file20=%IS5_HELP%\ja_JP\afs-light.hlp >> Lang_Japanese_Files.fgl
echo file21=%IS5_HELP%\ja_JP\taafscfg.cnt >> Lang_Japanese_Files.fgl
echo file22=%IS5_HELP%\ja_JP\taafscfg.hlp >> Lang_Japanese_Files.fgl
echo.  >> Lang_Japanese_Files.fgl
echo [General] >> Lang_Japanese_Files.fgl
echo Type=FILELIST >> Lang_Japanese_Files.fgl
echo Version=1.00.000 >> Lang_Japanese_Files.fgl


rem -------------- Lang_German_Files.fgl -----------------------------

echo [TopDir] > Lang_German_Files.fgl
echo file0=%IS5_DEST%\root.server\usr\afs\bin\afseventmsg_1032.dll >> Lang_German_Files.fgl
echo file1=%IS5_DEST%\root.server\usr\afs\bin\afs_setup_utils_1032.dll >> Lang_German_Files.fgl
echo file2=%IS5_DEST%\root.server\usr\afs\bin\afsserver_1032.dll >> Lang_German_Files.fgl
echo file3=%IS5_DEST%\root.server\usr\afs\bin\afssvrcfg_1032.dll >> Lang_German_Files.fgl
echo file4=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAccountManager_1032.dll >> Lang_German_Files.fgl
echo file5=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAppLib_1032.dll >> Lang_German_Files.fgl
echo file6=%IS5_DEST%\root.server\usr\afs\bin\TaAfsServerManager_1032.dll >> Lang_German_Files.fgl
echo file7=%IS5_DEST%\root.client\usr\vice\etc\afscreds_1032.dll >> Lang_German_Files.fgl
echo file8=%IS5_DEST%\root.client\usr\vice\etc\afs_config_1032.dll >> Lang_German_Files.fgl
echo file9=%IS5_DEST%\root.client\usr\vice\etc\afs_cpa_1032.dll >> Lang_German_Files.fgl
echo file10=%IS5_DEST%\root.client\usr\vice\etc\afs_shl_ext_1032.dll >> Lang_German_Files.fgl
echo file11=%IS5_HELP%\de_DE\afs-nt.hlp >> Lang_German_Files.fgl
echo file12=%IS5_HELP%\de_DE\afs-nt.cnt >> Lang_German_Files.fgl
echo file13=%IS5_HELP%\de_DE\taafssvrmgr.cnt >> Lang_German_Files.fgl
echo file14=%IS5_HELP%\de_DE\taafssvrmgr.hlp >> Lang_German_Files.fgl
echo file15=%IS5_HELP%\de_DE\taafsusrmgr.cnt >> Lang_German_Files.fgl
echo file16=%IS5_HELP%\de_DE\taafsusrmgr.hlp >> Lang_German_Files.fgl
echo file17=%IS5_HELP%\de_DE\afs-cc.cnt >> Lang_German_Files.fgl
echo file18=%IS5_HELP%\de_DE\afs-cc.hlp >> Lang_German_Files.fgl
echo file19=%IS5_HELP%\de_DE\afs-light.cnt >> Lang_German_Files.fgl
echo file20=%IS5_HELP%\de_DE\afs-light.hlp >> Lang_German_Files.fgl
echo file21=%IS5_HELP%\de_DE\taafscfg.cnt >> Lang_German_Files.fgl
echo file22=%IS5_HELP%\de_DE\taafscfg.hlp >> Lang_German_Files.fgl
echo.  >> Lang_German_Files.fgl
echo [General] >> Lang_German_Files.fgl
echo Type=FILELIST >> Lang_German_Files.fgl
echo Version=1.00.000 >> Lang_German_Files.fgl


rem -------------- Lang_Spanish_Files.fgl -----------------------------

echo [TopDir] > Lang_Spanish_Files.fgl
echo file0=%IS5_DEST%\root.server\usr\afs\bin\afseventmsg_1034.dll >> Lang_Spanish_Files.fgl
echo file1=%IS5_DEST%\root.server\usr\afs\bin\afs_setup_utils_1034.dll >> Lang_Spanish_Files.fgl
echo file2=%IS5_DEST%\root.server\usr\afs\bin\afsserver_1034.dll >> Lang_Spanish_Files.fgl
echo file3=%IS5_DEST%\root.server\usr\afs\bin\afssvrcfg_1034.dll >> Lang_Spanish_Files.fgl
echo file4=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAccountManager_1034.dll >> Lang_Spanish_Files.fgl
echo file5=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAppLib_1034.dll >> Lang_Spanish_Files.fgl
echo file6=%IS5_DEST%\root.server\usr\afs\bin\TaAfsServerManager_1034.dll >> Lang_Spanish_Files.fgl
echo file7=%IS5_DEST%\root.client\usr\vice\etc\afscreds_1034.dll >> Lang_Spanish_Files.fgl
echo file8=%IS5_DEST%\root.client\usr\vice\etc\afs_config_1034.dll >> Lang_Spanish_Files.fgl
echo file9=%IS5_DEST%\root.client\usr\vice\etc\afs_cpa_1034.dll >> Lang_Spanish_Files.fgl
echo file10=%IS5_DEST%\root.client\usr\vice\etc\afs_shl_ext_1034.dll >> Lang_Spanish_Files.fgl
echo file11=%IS5_HELP%\es_ES\afs-nt.hlp >> Lang_Spanish_Files.fgl
echo file12=%IS5_HELP%\es_ES\afs-nt.cnt >> Lang_Spanish_Files.fgl
echo file13=%IS5_HELP%\es_ES\taafssvrmgr.cnt >> Lang_Spanish_Files.fgl
echo file14=%IS5_HELP%\es_ES\taafssvrmgr.hlp >> Lang_Spanish_Files.fgl
echo file15=%IS5_HELP%\es_ES\taafsusrmgr.cnt >> Lang_Spanish_Files.fgl
echo file16=%IS5_HELP%\es_ES\taafsusrmgr.hlp >> Lang_Spanish_Files.fgl
echo file17=%IS5_HELP%\es_ES\afs-cc.cnt >> Lang_Spanish_Files.fgl
echo file18=%IS5_HELP%\es_ES\afs-cc.hlp >> Lang_Spanish_Files.fgl
echo file19=%IS5_HELP%\es_ES\afs-light.cnt >> Lang_Spanish_Files.fgl
echo file20=%IS5_HELP%\es_ES\afs-light.hlp >> Lang_Spanish_Files.fgl
echo file21=%IS5_HELP%\es_ES\taafscfg.cnt >> Lang_Spanish_Files.fgl
echo file22=%IS5_HELP%\es_ES\taafscfg.hlp >> Lang_Spanish_Files.fgl
echo.  >> Lang_Spanish_Files.fgl
echo [General] >> Lang_Spanish_Files.fgl
echo Type=FILELIST >> Lang_Spanish_Files.fgl
echo Version=1.00.000 >> Lang_Spanish_Files.fgl


rem -------------- Lang_Portuguese_Files.fgl -----------------------------

echo [TopDir] > Lang_Portuguese_Files.fgl
echo file0=%IS5_DEST%\root.server\usr\afs\bin\afseventmsg_1046.dll >> Lang_Portuguese_Files.fgl
echo file1=%IS5_DEST%\root.server\usr\afs\bin\afs_setup_utils_1046.dll >> Lang_Portuguese_Files.fgl
echo file2=%IS5_DEST%\root.server\usr\afs\bin\afsserver_1046.dll >> Lang_Portuguese_Files.fgl
echo file3=%IS5_DEST%\root.server\usr\afs\bin\afssvrcfg_1046.dll >> Lang_Portuguese_Files.fgl
echo file4=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAccountManager_1046.dll >> Lang_Portuguese_Files.fgl
echo file5=%IS5_DEST%\root.server\usr\afs\bin\TaAfsAppLib_1046.dll >> Lang_Portuguese_Files.fgl
echo file6=%IS5_DEST%\root.server\usr\afs\bin\TaAfsServerManager_1046.dll >> Lang_Portuguese_Files.fgl
echo file7=%IS5_DEST%\root.client\usr\vice\etc\afscreds_1046.dll >> Lang_Portuguese_Files.fgl
echo file8=%IS5_DEST%\root.client\usr\vice\etc\afs_config_1046.dll >> Lang_Portuguese_Files.fgl
echo file9=%IS5_DEST%\root.client\usr\vice\etc\afs_cpa_1046.dll >> Lang_Portuguese_Files.fgl
echo file10=%IS5_DEST%\root.client\usr\vice\etc\afs_shl_ext_1046.dll >> Lang_Portuguese_Files.fgl
echo file11=%IS5_HELP%\pt_BR\afs-nt.hlp >> Lang_Portuguese_Files.fgl
echo file12=%IS5_HELP%\pt_BR\afs-nt.cnt >> Lang_Portuguese_Files.fgl
echo file13=%IS5_HELP%\pt_BR\taafssvrmgr.cnt >> Lang_Portuguese_Files.fgl
echo file14=%IS5_HELP%\pt_BR\taafssvrmgr.hlp >> Lang_Portuguese_Files.fgl
echo file15=%IS5_HELP%\pt_BR\taafsusrmgr.cnt >> Lang_Portuguese_Files.fgl
echo file16=%IS5_HELP%\pt_BR\taafsusrmgr.hlp >> Lang_Portuguese_Files.fgl
echo file17=%IS5_HELP%\pt_BR\afs-cc.cnt >> Lang_Portuguese_Files.fgl
echo file18=%IS5_HELP%\pt_BR\afs-cc.hlp >> Lang_Portuguese_Files.fgl
echo file19=%IS5_HELP%\pt_BR\afs-light.cnt >> Lang_Portuguese_Files.fgl
echo file20=%IS5_HELP%\pt_BR\afs-light.hlp >> Lang_Portuguese_Files.fgl
echo file21=%IS5_HELP%\pt_BR\taafscfg.cnt >> Lang_Portuguese_Files.fgl
echo file22=%IS5_HELP%\pt_BR\taafscfg.hlp >> Lang_Portuguese_Files.fgl
echo.  >> Lang_Portuguese_Files.fgl
echo [General] >> Lang_Portuguese_Files.fgl
echo Type=FILELIST >> Lang_Portuguese_Files.fgl
echo Version=1.00.000 >> Lang_Portuguese_Files.fgl


rem -------------- Client_Afs_Header_Files.fgl --------------------------

echo [TopDir] > Client_Afs_Header_Files.fgl
echo file0=%IS5_INCL%\afs\afs_args.h >> Client_Afs_Header_Files.fgl
echo file1=%IS5_INCL%\afs\debug.h >> Client_Afs_Header_Files.fgl
echo file2=%IS5_INCL%\afs\param.h >> Client_Afs_Header_Files.fgl
echo file3=%IS5_INCL%\afs\afs_sysnames.h >> Client_Afs_Header_Files.fgl
echo file4=%IS5_INCL%\afs\permit_xprt.h >> Client_Afs_Header_Files.fgl
echo file5=%IS5_INCL%\afs\stds.h >> Client_Afs_Header_Files.fgl
echo file6=%IS5_INCL%\afs\icl.h >> Client_Afs_Header_Files.fgl
echo file7=%IS5_INCL%\afs\procmgmt.h >> Client_Afs_Header_Files.fgl
echo file8=%IS5_INCL%\afs\afsutil.h >> Client_Afs_Header_Files.fgl
echo file9=%IS5_INCL%\afs\assert.h >> Client_Afs_Header_Files.fgl
echo file10=%IS5_INCL%\afs\dirent.h >> Client_Afs_Header_Files.fgl
echo file11=%IS5_INCL%\afs\errors.h >> Client_Afs_Header_Files.fgl
echo file12=%IS5_INCL%\afs\itc.h >> Client_Afs_Header_Files.fgl
echo file13=%IS5_INCL%\afs\vice.h >> Client_Afs_Header_Files.fgl
echo file14=%IS5_INCL%\afs\pthread_glock.h >> Client_Afs_Header_Files.fgl
echo file15=%IS5_INCL%\afs\errmap_nt.h >> Client_Afs_Header_Files.fgl
echo file16=%IS5_INCL%\afs\dirpath.h >> Client_Afs_Header_Files.fgl
echo file17=%IS5_INCL%\afs\ktime.h >> Client_Afs_Header_Files.fgl
echo file18=%IS5_INCL%\afs\fileutil.h >> Client_Afs_Header_Files.fgl
echo file19=%IS5_INCL%\afs\secutil_nt.h >> Client_Afs_Header_Files.fgl
echo file20=%IS5_INCL%\afs\com_err.h >> Client_Afs_Header_Files.fgl
echo file21=%IS5_INCL%\afs\error_table.h >> Client_Afs_Header_Files.fgl
echo file22=%IS5_INCL%\afs\mit-sipb-cr.h >> Client_Afs_Header_Files.fgl
echo file23=%IS5_INCL%\afs\cmd.h >> Client_Afs_Header_Files.fgl
echo file24=%IS5_INCL%\afs\rxgen_consts.h >> Client_Afs_Header_Files.fgl
echo file25=%IS5_INCL%\afs\afsint.h >> Client_Afs_Header_Files.fgl
echo file26=%IS5_INCL%\afs\afscbint.h >> Client_Afs_Header_Files.fgl
echo file27=%IS5_INCL%\afs\audit.h >> Client_Afs_Header_Files.fgl
echo file28=%IS5_INCL%\afs\acl.h >> Client_Afs_Header_Files.fgl
echo file29=%IS5_INCL%\afs\prs_fs.h >> Client_Afs_Header_Files.fgl
echo file30=%IS5_INCL%\afs\afsd.h >> Client_Afs_Header_Files.fgl
echo file31=%IS5_INCL%\afs\cm.h >> Client_Afs_Header_Files.fgl
echo file32=%IS5_INCL%\afs\cm_buf.h >> Client_Afs_Header_Files.fgl
echo file33=%IS5_INCL%\afs\cm_cell.h >> Client_Afs_Header_Files.fgl
echo file34=%IS5_INCL%\afs\cm_config.h >> Client_Afs_Header_Files.fgl
echo file35=%IS5_INCL%\afs\cm_conn.h >> Client_Afs_Header_Files.fgl
echo file36=%IS5_INCL%\afs\cm_ioctl.h >> Client_Afs_Header_Files.fgl
echo file37=%IS5_INCL%\afs\cm_scache.h >> Client_Afs_Header_Files.fgl
echo file38=%IS5_INCL%\afs\cm_server.h >> Client_Afs_Header_Files.fgl
echo file39=%IS5_INCL%\afs\cm_user.h >> Client_Afs_Header_Files.fgl
echo file40=%IS5_INCL%\afs\cm_utils.h >> Client_Afs_Header_Files.fgl
echo file41=%IS5_INCL%\afs\fs_utils.h >> Client_Afs_Header_Files.fgl
echo file42=%IS5_INCL%\afs\krb.h >> Client_Afs_Header_Files.fgl
echo file43=%IS5_INCL%\afs\krb_prot.h >> Client_Afs_Header_Files.fgl
echo file44=%IS5_INCL%\afs\smb.h >> Client_Afs_Header_Files.fgl
echo file45=%IS5_INCL%\afs\smb3.h >> Client_Afs_Header_Files.fgl
echo file46=%IS5_INCL%\afs\smb_iocons.h >> Client_Afs_Header_Files.fgl
echo file47=%IS5_INCL%\afs\smb_ioctl.h >> Client_Afs_Header_Files.fgl
echo file48=%IS5_INCL%\afs\afsrpc.h >> Client_Afs_Header_Files.fgl
echo file49=%IS5_INCL%\afs\afssyscalls.h >> Client_Afs_Header_Files.fgl
echo file50=%IS5_INCL%\afs\pioctl_nt.h >> Client_Afs_Header_Files.fgl
echo file51=%IS5_INCL%\afs\auth.h >> Client_Afs_Header_Files.fgl
echo file52=%IS5_INCL%\afs\cellconfig.h >> Client_Afs_Header_Files.fgl
echo file53=%IS5_INCL%\afs\keys.h >> Client_Afs_Header_Files.fgl
echo file54=%IS5_INCL%\afs\ptserver.h >> Client_Afs_Header_Files.fgl
echo file55=%IS5_INCL%\afs\ptint.h >> Client_Afs_Header_Files.fgl
echo file56=%IS5_INCL%\afs\pterror.h >> Client_Afs_Header_Files.fgl
echo file57=%IS5_INCL%\afs\ptclient.h >> Client_Afs_Header_Files.fgl
echo file58=%IS5_INCL%\afs\prserver.h >> Client_Afs_Header_Files.fgl
echo file59=%IS5_INCL%\afs\print.h >> Client_Afs_Header_Files.fgl
echo file60=%IS5_INCL%\afs\prerror.h >> Client_Afs_Header_Files.fgl
echo file61=%IS5_INCL%\afs\prclient.h >> Client_Afs_Header_Files.fgl
echo file62=%IS5_INCL%\afs\kautils.h >> Client_Afs_Header_Files.fgl
echo file63=%IS5_INCL%\afs\kauth.h >> Client_Afs_Header_Files.fgl
echo file64=%IS5_INCL%\afs\kaport.h >> Client_Afs_Header_Files.fgl
echo file65=%IS5_INCL%\afs\vl_opcodes.h >> Client_Afs_Header_Files.fgl
echo file66=%IS5_INCL%\afs\vlserver.h >> Client_Afs_Header_Files.fgl
echo file67=%IS5_INCL%\afs\vldbint.h >> Client_Afs_Header_Files.fgl
echo file68=%IS5_INCL%\afs\usd.h >> Client_Afs_Header_Files.fgl
echo file69=%IS5_INCL%\afs\bubasics.h >> Client_Afs_Header_Files.fgl
echo file70=%IS5_INCL%\afs\butc.h >> Client_Afs_Header_Files.fgl
echo file71=%IS5_INCL%\afs\bumon.h >> Client_Afs_Header_Files.fgl
echo file72=%IS5_INCL%\afs\butm.h >> Client_Afs_Header_Files.fgl
echo file73=%IS5_INCL%\afs\tcdata.h >> Client_Afs_Header_Files.fgl
echo file74=%IS5_INCL%\afs\budb.h >> Client_Afs_Header_Files.fgl
echo file75=%IS5_INCL%\afs\budb_errs.h >> Client_Afs_Header_Files.fgl
echo file76=%IS5_INCL%\afs\budb_client.h >> Client_Afs_Header_Files.fgl
echo file77=%IS5_INCL%\afs\dir.h >> Client_Afs_Header_Files.fgl
echo file78=%IS5_INCL%\afs\fssync.h >> Client_Afs_Header_Files.fgl
echo file79=%IS5_INCL%\afs\ihandle.h >> Client_Afs_Header_Files.fgl
echo file80=%IS5_INCL%\afs\nfs.h >> Client_Afs_Header_Files.fgl
echo file81=%IS5_INCL%\afs\ntops.h >> Client_Afs_Header_Files.fgl
echo file82=%IS5_INCL%\afs\partition.h >> Client_Afs_Header_Files.fgl
echo file83=%IS5_INCL%\afs\viceinode.h >> Client_Afs_Header_Files.fgl
echo file84=%IS5_INCL%\afs\vnode.h >> Client_Afs_Header_Files.fgl
echo file85=%IS5_INCL%\afs\volume.h >> Client_Afs_Header_Files.fgl
echo file86=%IS5_INCL%\afs\voldefs.h >> Client_Afs_Header_Files.fgl
echo file87=%IS5_INCL%\afs\volser.h >> Client_Afs_Header_Files.fgl
echo file88=%IS5_INCL%\afs\volint.h >> Client_Afs_Header_Files.fgl
echo file89=%IS5_INCL%\afs\fs_stats.h >> Client_Afs_Header_Files.fgl
echo file90=%IS5_INCL%\afs\bosint.h >> Client_Afs_Header_Files.fgl
echo file91=%IS5_INCL%\afs\bnode.h >> Client_Afs_Header_Files.fgl
echo. >> Client_Afs_Header_Files.fgl
echo [General] >> Client_Afs_Header_Files.fgl
echo Type=FILELIST >> Client_Afs_Header_Files.fgl
echo Version=1.00.000 >> Client_Afs_Header_Files.fgl


rem -------------- Client_Rx_Header_Files.fgl ---------------------------

echo [TopDir] > Client_Rx_Header_Files.fgl
echo file0=%IS5_INCL%\rx\rx.h >> Client_Rx_Header_Files.fgl
echo file1=%IS5_INCL%\rx\rx_packet.h >> Client_Rx_Header_Files.fgl
echo file2=%IS5_INCL%\rx\rx_user.h >> Client_Rx_Header_Files.fgl
echo file3=%IS5_INCL%\rx\rx_event.h >> Client_Rx_Header_Files.fgl
echo file4=%IS5_INCL%\rx\rx_queue.h >> Client_Rx_Header_Files.fgl
echo file5=%IS5_INCL%\rx\rx_globals.h >> Client_Rx_Header_Files.fgl
echo file6=%IS5_INCL%\rx\rx_clock.h >> Client_Rx_Header_Files.fgl
echo file7=%IS5_INCL%\rx\rx_misc.h >> Client_Rx_Header_Files.fgl
echo file8=%IS5_INCL%\rx\rx_multi.h >> Client_Rx_Header_Files.fgl
echo file9=%IS5_INCL%\rx\rx_null.h >> Client_Rx_Header_Files.fgl
echo file10=%IS5_INCL%\rx\rx_lwp.h >> Client_Rx_Header_Files.fgl
echo file11=%IS5_INCL%\rx\rx_pthread.h >> Client_Rx_Header_Files.fgl
echo file12=%IS5_INCL%\rx\rx_xmit_nt.h >> Client_Rx_Header_Files.fgl
echo file13=%IS5_INCL%\rx\xdr.h >> Client_Rx_Header_Files.fgl
echo file14=%IS5_INCL%\rx\rxkad.h >> Client_Rx_Header_Files.fgl
echo. >> Client_Rx_Header_Files.fgl
echo [General] >> Client_Rx_Header_Files.fgl
echo Type=FILELIST >> Client_Rx_Header_Files.fgl
echo Version=1.00.000 >> Client_Rx_Header_Files.fgl


rem -------------- Client_Main_Header_Files.fgl -------------------------

echo [TopDir] > Client_Main_Header_Files.fgl
echo file0=%IS5_INCL%\lock.h >> Client_Main_Header_Files.fgl
echo file1=%IS5_INCL%\lwp.h >> Client_Main_Header_Files.fgl
echo file2=%IS5_INCL%\preempt.h >> Client_Main_Header_Files.fgl
echo file3=%IS5_INCL%\timer.h >> Client_Main_Header_Files.fgl
echo file4=%IS5_INCL%\des.h >> Client_Main_Header_Files.fgl
echo file5=%IS5_INCL%\des_conf.h >> Client_Main_Header_Files.fgl
echo file6=%IS5_INCL%\mit-cpyright.h >> Client_Main_Header_Files.fgl
echo file7=%IS5_INCL%\des_odd.h >> Client_Main_Header_Files.fgl
echo file8=%IS5_INCL%\crypt.h >> Client_Main_Header_Files.fgl
echo file9=%IS5_INCL%\pthread.h >> Client_Main_Header_Files.fgl
echo file10=%IS5_INCL%\dbrpc.h >> Client_Main_Header_Files.fgl
echo file11=%IS5_INCL%\basic.h >> Client_Main_Header_Files.fgl
echo file12=%IS5_INCL%\osidebug.h >> Client_Main_Header_Files.fgl
echo file13=%IS5_INCL%\osiltype.h >> Client_Main_Header_Files.fgl
echo file14=%IS5_INCL%\osistatl.h >> Client_Main_Header_Files.fgl
echo file15=%IS5_INCL%\trylock.h >> Client_Main_Header_Files.fgl
echo file16=%IS5_INCL%\main.h >> Client_Main_Header_Files.fgl
echo file17=%IS5_INCL%\osibasel.h >> Client_Main_Header_Files.fgl
echo file18=%IS5_INCL%\osifd.h >> Client_Main_Header_Files.fgl
echo file19=%IS5_INCL%\osiqueue.h >> Client_Main_Header_Files.fgl
echo file20=%IS5_INCL%\osiutils.h >> Client_Main_Header_Files.fgl
echo file21=%IS5_INCL%\osi.h >> Client_Main_Header_Files.fgl
echo file22=%IS5_INCL%\osidb.h >> Client_Main_Header_Files.fgl
echo file23=%IS5_INCL%\osilog.h >> Client_Main_Header_Files.fgl
echo file24=%IS5_INCL%\osisleep.h >> Client_Main_Header_Files.fgl
echo file25=%IS5_INCL%\perf.h >> Client_Main_Header_Files.fgl
echo file26=%IS5_INCL%\ubik.h >> Client_Main_Header_Files.fgl
echo file27=%IS5_INCL%\ubik_int.h >> Client_Main_Header_Files.fgl
echo. >> Client_Main_Header_Files.fgl
echo [General] >> Client_Main_Header_Files.fgl
echo Type=FILELIST >> Client_Main_Header_Files.fgl
echo Version=1.00.000 >> Client_Main_Header_Files.fgl


rem -------------- Client_Sample_Files.fgl ------------------------------

echo [TopDir] > Client_Sample_Files.fgl
echo file0=%IS5_WINNT%\afsd\sample\token.c >> Client_Sample_Files.fgl
echo. >> Client_Sample_Files.fgl
echo [General] >> Client_Sample_Files.fgl
echo Type=FILELIST >> Client_Sample_Files.fgl
echo Version=1.00.000 >> Client_Sample_Files.fgl

:EOF
