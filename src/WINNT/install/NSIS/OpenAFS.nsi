;OpenAFS Install Script for NSIS
;
; Written by Rob Murawski <rsm4@ieee.org>
;
;Based on:
;NSIS Modern User Interface version 1.63
;MultiLanguage Example Script
;Written by Joost Verburg

!define MUI_PRODUCT "OpenAFS for Windows" ;Define your own software name here
!define MUI_VERSION "1.2.11" ;Define your own software version here
!define MUI_MAJORVERSION 1
!define MUI_MINORVERSION 2
!define MUI_PATCHLEVEL 110


!include "MUI.nsh"
!include Sections.nsh

;--------------------------------
;Configuration

  ;General
  OutFile "OpenAFSforWindows.exe"
  SilentInstall normal
  !define MUI_ICON "..\..\client_cpa\afs_conf.ico"
  !define MUI_UNICON "c:\Program Files\NSIS\Contrib\Icons\normal-uninstall.ico"
  !define AFS_COMPANY_NAME "OpenAFS"
  !define AFS_PRODUCT_NAME "OpenAFS"
  !define AFS_REGKEY_ROOT "Software\TransarcCorporation"
  CRCCheck force

  ;Folder selection page
  InstallDir "$PROGRAMFILES\OpenAFS\AFS"
  
  ;Remember install folder
  InstallDirRegKey HKCU "Software\TransarcCorporation" ""
  
  ;Remember the installer language
  !define MUI_LANGDLL_REGISTRY_ROOT "HKCU" 
  !define MUI_LANGDLL_REGISTRY_KEY "Software\TransarcCorporation" 
  !define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"
  
  ;Where are the files?
  !define AFS_DESTDIR "..\..\..\..\obj\DEST\free"
  !define AFS_CLIENT_BUILDDIR "${AFS_DESTDIR}\root.client\usr\vice\etc"
  !define AFS_WININSTALL_DIR "${AFS_DESTDIR}\WinInstall\Config"
  !define AFS_BUILD_INCDIR "${AFS_DESTDIR}\include"
  !define AFS_CLIENT_LIBDIR "${AFS_DESTDIR}\lib"
  !define AFS_SERVER_BUILDDIR "${AFS_DESTDIR}\root.server\usr\afs\bin"
  !define AFS_ETC_BUILDDIR "${AFS_DESTDIR}\etc"
  !define SDK_DIR "X:"
  
;--------------------------------
;Modern UI Configuration

  !define MUI_CUSTOMPAGECOMMANDS
  !define MUI_WELCOMEPAGE
  !define MUI_COMPONENTSPAGE
  !define MUI_COMPONENTSPAGE_SMALLDESC
  !define MUI_DIRECTORYPAGE

  !define MUI_ABORTWARNING
  !define MUI_FINISHPAGE
  
  !define MUI_UNINSTALLER
  !define MUI_UNCONFIRMPAGE

  
  !insertmacro MUI_PAGECOMMAND_WELCOME
  !insertmacro MUI_PAGECOMMAND_COMPONENTS
  !insertmacro MUI_PAGECOMMAND_DIRECTORY
  Page custom AFSPageGetCellServDB
  Page custom AFSPageGetCellName
  !insertmacro MUI_PAGECOMMAND_INSTFILES
  !insertmacro MUI_PAGECOMMAND_FINISH
  
;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"
  ;!insertmacro MUI_LANGUAGE "French"
  !insertmacro MUI_LANGUAGE "German"
  !insertmacro MUI_LANGUAGE "Spanish"
  !insertmacro MUI_LANGUAGE "SimpChinese"
  !insertmacro MUI_LANGUAGE "TradChinese"    
  !insertmacro MUI_LANGUAGE "Japanese"
  ;!insertmacro MUI_LANGUAGE "Korean"
  ;!insertmacro MUI_LANGUAGE "Italian"
  ;!insertmacro MUI_LANGUAGE "Dutch"
  ;!insertmacro MUI_LANGUAGE "Danish"
  ;!insertmacro MUI_LANGUAGE "Greek"
  ;!insertmacro MUI_LANGUAGE "Russian"
  !insertmacro MUI_LANGUAGE "PortugueseBR"
  ;!insertmacro MUI_LANGUAGE "Polish"
  ;!insertmacro MUI_LANGUAGE "Ukrainian"
  ;!insertmacro MUI_LANGUAGE "Czech"
  ;!insertmacro MUI_LANGUAGE "Slovak"
  ;!insertmacro MUI_LANGUAGE "Croatian"
  ;!insertmacro MUI_LANGUAGE "Bulgarian"
  ;!insertmacro MUI_LANGUAGE "Hungarian"
  ;!insertmacro MUI_LANGUAGE "Thai"
  ;!insertmacro MUI_LANGUAGE "Romanian"
  ;!insertmacro MUI_LANGUAGE "Macedonian"
  ;!insertmacro MUI_LANGUAGE "Turkish"
  
;--------------------------------
;Language Strings
    
  ;Descriptions
  LangString DESC_SecCopyUI ${LANG_ENGLISH} "OpenAFS for Windows: English"
  ;LangString DESC_SecCopyUI ${LANG_FRENCH} "OpenAFS for Windows: French"
  LangString DESC_SecCopyUI ${LANG_GERMAN} "OpenAFS for Windows: German"
  LangString DESC_SecCopyUI ${LANG_SPANISH} "OpenAFS for Windows: Spanish"
  LangString DESC_SecCopyUI ${LANG_SIMPCHINESE} "OpenAFS for Windows: Simplified Chinese"
  LangString DESC_SecCopyUI ${LANG_TRADCHINESE} "OpenAFS for Windows: Traditional Chinese description"
  LangString DESC_SecCopyUI ${LANG_JAPANESE} "OpenAFS for Windows: Japanese description"
  LangString DESC_SecCopyUI ${LANG_KOREAN} "OpenAFS for Windows: Korean description"
  ;LangString DESC_SecCopyUI ${LANG_ITALIAN} "OpenAFS for Windows: Italian description"
  ;LangString DESC_SecCopyUI ${LANG_DUTCH} "OpenAFS for Windows: Dutch description"
  ;LangString DESC_SecCopyUI ${LANG_DANISH} "OpenAFS for Windows: Danish description"
  ;LangString DESC_SecCopyUI ${LANG_GREEK} "OpenAFS for Windows: Greek description"
  ;LangString DESC_SecCopyUI ${LANG_RUSSIAN} "OpenAFS for Windows: Russian description"
  LangString DESC_SecCopyUI ${LANG_PORTUGUESEBR} "OpenAFS for Windows: Portuguese (Brasil) description"
  ;LangString DESC_SecCopyUI ${LANG_POLISH} "OpenAFS for Windows: Polish description"
  ;LangString DESC_SecCopyUI ${LANG_UKRAINIAN} "OpenAFS for Windows: Ukrainian description"
  ;LangString DESC_SecCopyUI ${LANG_CZECH} "OpenAFS for Windows: Czechian description"
  ;LangString DESC_SecCopyUI ${LANG_SLOVAK} "OpenAFS for Windows: Slovakian description"
  ;LangString DESC_SecCopyUI ${LANG_CROATIAN} "OpenAFS for Windows: Slovakian description"
  ;LangString DESC_SecCopyUI ${LANG_BULGARIAN} "OpenAFS for Windows: Bulgarian description"
  ;LangString DESC_SecCopyUI ${LANG_HUNGARIAN} "OpenAFS for Windows: Hungarian description"
  ;LangString DESC_SecCopyUI ${LANG_THAI} "OpenAFS for Windows: Thai description"
  ;LangString DESC_SecCopyUI ${LANG_ROMANIAN} "OpenAFS for Windows: Romanian description"
  ;LangString DESC_SecCopyUI ${LANG_MACEDONIAN} "OpenAFS for Windows: Macedonian description"
  ;LangString DESC_SecCopyUI ${LANG_TURKISH} "OpenAFS for Windows: Turkish description"

  LangString DESC_SecClient ${LANG_ENGLISH} "OpenAFS Client: Allows you to access AFS from your Windows PC."
  LangString DESC_SecClient ${LANG_GERMAN} "OpenAFS Client: Allows you to access AFS from your Windows PC."
  LangString DESC_SecClient ${LANG_SPANISH} "OpenAFS Client: Allows you to access AFS from your Windows PC."
  LangString DESC_SecClient ${LANG_SIMPCHINESE} "OpenAFS Client: Allows you to access AFS from your Windows PC."
  LangString DESC_SecClient ${LANG_TRADCHINESE} "OpenAFS Client: Allows you to access AFS from your Windows PC."
  LangString DESC_SecClient ${LANG_JAPANESE} "OpenAFS Client: Allows you to access AFS from your Windows PC."
  LangString DESC_SecClient ${LANG_KOREAN} "OpenAFS Client: Allows you to access AFS from your Windows PC."
  LangString DESC_SecClient ${LANG_PORTUGUESEBR} "OpenAFS Client: Allows you to access AFS from your Windows PC."
  
  LangString DESC_SecServer ${LANG_ENGLISH} "OpenAFS Server: Allows you to run an AFS file server."
  LangString DESC_SecServer ${LANG_GERMAN} "OpenAFS Server: Allows you to run an AFS file server."
  LangString DESC_SecServer ${LANG_SPANISH} "OpenAFS Server: Allows you to run an AFS file server."
  LangString DESC_SecServer ${LANG_SIMPCHINESE} "OpenAFS Server: Allows you to run an AFS file server."
  LangString DESC_SecServer ${LANG_TRADCHINESE} "OpenAFS Server: Allows you to run an AFS file server."
  LangString DESC_SecServer ${LANG_JAPANESE} "OpenAFS Server: Allows you to run an AFS file server."
  LangString DESC_SecServer ${LANG_KOREAN} "OpenAFS Server: Allows you to run an AFS file server."
  LangString DESC_SecServer ${LANG_PORTUGUESEBR} "OpenAFS Server: Allows you to run an AFS file server."
  
  LangString DESC_SecControl ${LANG_ENGLISH} "OpenAFS Control Center: GUI utilities for managing and configuring AFS."
  LangString DESC_SecControl ${LANG_GERMAN} "OpenAFS Control Center: GUI utilities for managing and configuring AFS."
  LangString DESC_SecControl ${LANG_SPANISH} "OpenAFS Control Center: GUI utilities for managing and configuring AFS."
  LangString DESC_SecControl ${LANG_SIMPCHINESE} "OpenAFS Control Center: GUI utilities for managing and configuring AFS."
  LangString DESC_SecControl ${LANG_TRADCHINESE} "OpenAFS Control Center: GUI utilities for managing and configuring AFS."
  LangString DESC_SecControl ${LANG_JAPANESE} "OpenAFS Control Center: GUI utilities for managing and configuring AFS."
  LangString DESC_SecControl ${LANG_KOREAN} "OpenAFS Control Center: GUI utilities for managing and configuring AFS."
  LangString DESC_SecControl ${LANG_PORTUGUESEBR} "OpenAFS Control Center: GUI utilities for managing and configuring AFS."
  
  LangString DESC_SecDocs ${LANG_ENGLISH} "OpenAFS Supplemental Documentation: Additional documentation for using OpenAFS."
  LangString DESC_SecDocs ${LANG_GERMAN} "OpenAFS Supplemental Documentation: Additional documentation for using OpenAFS."
  LangString DESC_SecDocs ${LANG_SPANISH} "OpenAFS Supplemental Documentation: Additional documentation for using OpenAFS."
  LangString DESC_SecDocs ${LANG_SIMPCHINESE} "OpenAFS Supplemental Documentation: Additional documentation for using OpenAFS."
  LangString DESC_SecDocs ${LANG_TRADCHINESE} "OpenAFS Supplemental Documentation: Additional documentation for using OpenAFS."
  LangString DESC_SecDocs ${LANG_JAPANESE} "OpenAFS Supplemental Documentation: Additional documentation for using OpenAFS."
  LangString DESC_SecDocs ${LANG_KOREAN} "OpenAFS Supplemental Documentation: Additional documentation for using OpenAFS."
  LangString DESC_SecDocs ${LANG_PORTUGUESEBR} "OpenAFS Supplemental Documentation: Additional documentation for using OpenAFS."
  
; Popup error messages
  LangString CellError ${LANG_ENGLISH} "You must specify a valid CellServDB file to copy during install"
  LangString CellError ${LANG_GERMAN} "You must specify a valid CellServDB file to copy during the install"
  LangString CellError ${LANG_SPANISH} "You must specify a valid CellServDB file to copy during the install"
  LangString CellError ${LANG_SIMPCHINESE} "You must specify a valid CellServDB file to copy during the install"
  LangString CellError ${LANG_TRADCHINESE} "You must specify a valid CellServDB file to copy during the install"
  LangString CellError ${LANG_JAPANESE} "You must specify a valid CellServDB file to copy during the install"
  LangString CellError ${LANG_KOREAN} "You must specify a valid CellServDB file to copy during the install"
  LangString CellError ${LANG_PORTUGUESEBR} "You must specify a valid CellServDB file to copy during the install"
  
  
; Upgrade/re-install strings
   LangString UPGRADE_CLIENT ${LANG_ENGLISH} "Upgrade AFS Client"
   
   
   LangString REINSTALL_SERVER ${LANG_ENGLISH} "Re-install AFS Server"
  
;--------------------------------
; Macros


;--------------------------------
;Reserve Files
  
  ;Things that need to be extracted on first (keep these lines before any File command!)
  ;Only useful for BZIP2 compression
  !insertmacro MUI_RESERVEFILE_LANGDLL
  
;--------------------------------
;Installer Sections

;----------------------
; OpenAFS CLIENT
Section "AFS Client" SecClient

  SetShellVarContext all
   ; Do client components
  SetOutPath "$INSTDIR\Client\Program"
  File "${AFS_CLIENT_BUILDDIR}\afsshare.exe"
  File "${AFS_BUILD_INCDIR}\afs\cm_config.h"
  File "${AFS_CLIENT_BUILDDIR}\libosi.dll"
  File "${AFS_BUILD_INCDIR}\afs\kautils.h"
  File "${AFS_CLIENT_BUILDDIR}\libafsconf.dll"
  File "${AFS_CLIENT_BUILDDIR}\klog.exe"
  File "${AFS_CLIENT_BUILDDIR}\tokens.exe"
  File "${AFS_CLIENT_BUILDDIR}\unlog.exe"
  File "${AFS_CLIENT_BUILDDIR}\fs.exe"
  File "${AFS_CLIENT_LIBDIR}\libafsconf.lib"
  File "${AFS_CLIENT_LIBDIR}\afsauthent.lib"
  File "${AFS_CLIENT_BUILDDIR}\afscreds.exe"
  File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext.dll"
  File "${AFS_BUILD_INCDIR}\afs\auth.h"
  File "${AFS_CLIENT_BUILDDIR}\afsd_service.exe"
  File "${AFS_CLIENT_BUILDDIR}\afslogon.dll"
  File "${AFS_CLIENT_BUILDDIR}\symlink.exe"
  File "${AFS_DESTDIR}\bin\kpasswd.exe"
  File "${AFS_SERVER_BUILDDIR}\pts.exe"
  File "${AFS_SERVER_BUILDDIR}\bos.exe"
  File "${AFS_SERVER_BUILDDIR}\kas.exe"
  File "${AFS_SERVER_BUILDDIR}\vos.exe"
  File "${AFS_SERVER_BUILDDIR}\udebug.exe"
  File "${AFS_DESTDIR}\bin\translate_et.exe"
  File "${AFS_DESTDIR}\etc\rxdebug.exe"
  File "${AFS_DESTDIR}\etc\backup.exe"
  
  
  ; Client_headers
   SetOutPath "$INSTDIR\Client\Program\Include"
   File "${AFS_BUILD_INCDIR}\lock.h"
   File "${AFS_BUILD_INCDIR}\lwp.h"
   File "${AFS_BUILD_INCDIR}\preempt.h"
   File "${AFS_BUILD_INCDIR}\timer.h"
   File "${AFS_BUILD_INCDIR}\des.h"
   File "${AFS_BUILD_INCDIR}\des_conf.h"
   File "${AFS_BUILD_INCDIR}\mit-cpyright.h"
   ;File "${AFS_BUILD_INCDIR}\des-odd.h"
   File "${AFS_BUILD_INCDIR}\crypt.h"
   File "${AFS_BUILD_INCDIR}\pthread.h"
   File "${AFS_BUILD_INCDIR}\dbrpc.h"
   File "${AFS_BUILD_INCDIR}\basic.h"
   File "${AFS_BUILD_INCDIR}\osidebug.h"
   File "${AFS_BUILD_INCDIR}\osiltype.h"
   File "${AFS_BUILD_INCDIR}\osistatl.h"
   File "${AFS_BUILD_INCDIR}\trylock.h"
   File "${AFS_BUILD_INCDIR}\main.h"
   File "${AFS_BUILD_INCDIR}\osibasel.h"
   File "${AFS_BUILD_INCDIR}\osifd.h"
   File "${AFS_BUILD_INCDIR}\osiqueue.h"
   File "${AFS_BUILD_INCDIR}\osiutils.h"
   File "${AFS_BUILD_INCDIR}\osi.h"
   File "${AFS_BUILD_INCDIR}\osidb.h"
   File "${AFS_BUILD_INCDIR}\osilog.h"
   File "${AFS_BUILD_INCDIR}\osisleep.h"
   File "${AFS_BUILD_INCDIR}\perf.h"
   File "${AFS_BUILD_INCDIR}\ubik.h"
   File "${AFS_BUILD_INCDIR}\ubik_int.h"
   
   
   
   SetOutPath "$INSTDIR\Client\Program\Include\afs"
   File "${AFS_BUILD_INCDIR}\afs\afs_args.h"
   File "${AFS_BUILD_INCDIR}\afs\debug.h"
   File "${AFS_BUILD_INCDIR}\afs\param.h"
   File "${AFS_BUILD_INCDIR}\afs\afs_sysnames.h"
   ;File "${AFS_BUILD_INCDIR}\afs\permit_xprt.h"
   File "${AFS_BUILD_INCDIR}\afs\stds.h"
   File "${AFS_BUILD_INCDIR}\afs\icl.h"
   File "${AFS_BUILD_INCDIR}\afs\procmgmt.h"
   File "${AFS_BUILD_INCDIR}\afs\afsutil.h"
   File "${AFS_BUILD_INCDIR}\afs\assert.h"
   File "${AFS_BUILD_INCDIR}\afs\dirent.h"
   File "${AFS_BUILD_INCDIR}\afs\errors.h"
   File "${AFS_BUILD_INCDIR}\afs\itc.h"
   File "${AFS_BUILD_INCDIR}\afs\vice.h"
   File "${AFS_BUILD_INCDIR}\afs\pthread_glock.h"
   File "${AFS_BUILD_INCDIR}\afs\errmap_nt.h"
   File "${AFS_BUILD_INCDIR}\afs\dirpath.h"
   File "${AFS_BUILD_INCDIR}\afs\ktime.h"
   File "${AFS_BUILD_INCDIR}\afs\fileutil.h"
   File "${AFS_BUILD_INCDIR}\afs\secutil_nt.h"
   File "${AFS_BUILD_INCDIR}\afs\com_err.h"
   File "${AFS_BUILD_INCDIR}\afs\error_table.h"
   ;File "${AFS_BUILD_INCDIR}\afs\mit_sipb-cr.h"
   File "${AFS_BUILD_INCDIR}\afs\cmd.h"
   File "${AFS_BUILD_INCDIR}\afs\rxgen_consts.h"
   File "${AFS_BUILD_INCDIR}\afs\afsint.h"
   File "${AFS_BUILD_INCDIR}\afs\afscbint.h"
   File "${AFS_BUILD_INCDIR}\afs\audit.h"
   File "${AFS_BUILD_INCDIR}\afs\acl.h"
   File "${AFS_BUILD_INCDIR}\afs\prs_fs.h"
   File "${AFS_BUILD_INCDIR}\afs\afsd.h"
   File "${AFS_BUILD_INCDIR}\afs\cm.h"
   File "${AFS_BUILD_INCDIR}\afs\cm_buf.h"
   File "${AFS_BUILD_INCDIR}\afs\cm_cell.h"
   File "${AFS_BUILD_INCDIR}\afs\cm_config.h"
   File "${AFS_BUILD_INCDIR}\afs\cm_conn.h"
   File "${AFS_BUILD_INCDIR}\afs\cm_ioctl.h"
   File "${AFS_BUILD_INCDIR}\afs\cm_scache.h"
   File "${AFS_BUILD_INCDIR}\afs\cm_server.h"
   File "${AFS_BUILD_INCDIR}\afs\cm_user.h"
   File "${AFS_BUILD_INCDIR}\afs\cm_utils.h"
   File "${AFS_BUILD_INCDIR}\afs\fs_utils.h"
   File "${AFS_BUILD_INCDIR}\afs\krb.h"
   File "${AFS_BUILD_INCDIR}\afs\krb_prot.h"
   File "${AFS_BUILD_INCDIR}\afs\smb.h"
   File "${AFS_BUILD_INCDIR}\afs\smb3.h"
   File "${AFS_BUILD_INCDIR}\afs\smb_iocons.h"
   File "${AFS_BUILD_INCDIR}\afs\smb_ioctl.h"
   File "${AFS_BUILD_INCDIR}\afs\afsrpc.h"
   File "${AFS_BUILD_INCDIR}\afs\afssyscalls.h"
   File "${AFS_BUILD_INCDIR}\afs\pioctl_nt.h"
   File "${AFS_BUILD_INCDIR}\afs\auth.h"
   File "${AFS_BUILD_INCDIR}\afs\cellconfig.h"
   File "${AFS_BUILD_INCDIR}\afs\keys.h"
   File "${AFS_BUILD_INCDIR}\afs\ptserver.h"
   File "${AFS_BUILD_INCDIR}\afs\ptint.h"
   File "${AFS_BUILD_INCDIR}\afs\pterror.h"
   File "${AFS_BUILD_INCDIR}\afs\ptint.h"
   File "${AFS_BUILD_INCDIR}\afs\pterror.h"
   File "${AFS_BUILD_INCDIR}\afs\ptclient.h"
   File "${AFS_BUILD_INCDIR}\afs\prserver.h"
   File "${AFS_BUILD_INCDIR}\afs\print.h"
   File "${AFS_BUILD_INCDIR}\afs\prerror.h"
   File "${AFS_BUILD_INCDIR}\afs\prclient.h"
   File "${AFS_BUILD_INCDIR}\afs\kautils.h"
   File "${AFS_BUILD_INCDIR}\afs\kauth.h"
   File "${AFS_BUILD_INCDIR}\afs\kaport.h"
   File "${AFS_BUILD_INCDIR}\afs\vl_opcodes.h"
   File "${AFS_BUILD_INCDIR}\afs\vlserver.h"
   File "${AFS_BUILD_INCDIR}\afs\vldbint.h"
   File "${AFS_BUILD_INCDIR}\afs\usd.h"
   File "${AFS_BUILD_INCDIR}\afs\bubasics.h"
   File "${AFS_BUILD_INCDIR}\afs\butc.h"
   File "${AFS_BUILD_INCDIR}\afs\bumon.h"
   File "${AFS_BUILD_INCDIR}\afs\butm.h"
   File "${AFS_BUILD_INCDIR}\afs\tcdata.h"
   File "${AFS_BUILD_INCDIR}\afs\budb.h"
   ;File "${AFS_BUILD_INCDIR}\afs\budb_errors.h"
   File "${AFS_BUILD_INCDIR}\afs\budb_client.h"
   File "${AFS_BUILD_INCDIR}\afs\dir.h"
   File "${AFS_BUILD_INCDIR}\afs\fssync.h"
   File "${AFS_BUILD_INCDIR}\afs\ihandle.h"
   File "${AFS_BUILD_INCDIR}\afs\nfs.h"
   File "${AFS_BUILD_INCDIR}\afs\ntops.h"
   File "${AFS_BUILD_INCDIR}\afs\partition.h"
   File "${AFS_BUILD_INCDIR}\afs\viceinode.h"
   File "${AFS_BUILD_INCDIR}\afs\vnode.h"
   File "${AFS_BUILD_INCDIR}\afs\volume.h"
   File "${AFS_BUILD_INCDIR}\afs\voldefs.h"
   File "${AFS_BUILD_INCDIR}\afs\volser.h"
   File "${AFS_BUILD_INCDIR}\afs\volint.h"
   File "${AFS_BUILD_INCDIR}\afs\fs_stats.h"
   File "${AFS_BUILD_INCDIR}\afs\bosint.h"
   File "${AFS_BUILD_INCDIR}\afs\bnode.h"
   
   
   SetOutPath "$INSTDIR\Client\Program\Include\rx"
   File "${AFS_BUILD_INCDIR}\rx\rx.h"
   File "${AFS_BUILD_INCDIR}\rx\rx_packet.h"
   File "${AFS_BUILD_INCDIR}\rx\rx_user.h"
   File "${AFS_BUILD_INCDIR}\rx\rx_event.h"
   File "${AFS_BUILD_INCDIR}\rx\rx_queue.h"
   File "${AFS_BUILD_INCDIR}\rx\rx_globals.h"
   File "${AFS_BUILD_INCDIR}\rx\rx_clock.h"
   File "${AFS_BUILD_INCDIR}\rx\rx_misc.h"
   File "${AFS_BUILD_INCDIR}\rx\rx_multi.h"
   File "${AFS_BUILD_INCDIR}\rx\rx_null.h"
   File "${AFS_BUILD_INCDIR}\rx\rx_lwp.h"
   File "${AFS_BUILD_INCDIR}\rx\rx_pthread.h"
   File "${AFS_BUILD_INCDIR}\rx\rx_xmit_nt.h"
   File "${AFS_BUILD_INCDIR}\rx\xdr.h"
   File "${AFS_BUILD_INCDIR}\rx\rxkad.h"
   
   

   ; Client Sample
   SetOutPath "$INSTDIR\Client\Program\Sample"
   File "..\..\afsd\sample\token.c"
   
   Call AFSLangFiles
   
   ; Do SYSTEM32 DIR
   SetOutPath "$SYSDIR"
   File "${AFS_CLIENT_BUILDDIR}\afs_cpa.cpl"
   ;File "${SDK_DIR}\REDIST\msvcrt.dll"
   ;File "${SDK_DIR}\REDIST\mfc42.dll"
   SetOutPath "$INSTDIR\Common"
   File "${AFS_WININSTALL_DIR}\Msvcr71.dll"
   
  ; Do WINDOWSDIR components
  ; Get AFS CellServDB file
  Call afs.GetCellServDB
  
  ReadINIStr $R0 $0 "Field 2" "State"
  StrCmp $R0 "1" UsePkg DontUsePkg
 UsePkg:
   SetOutPath "$WINDIR"
   File "afsdcell.ini"
DontUsePkg:
   ReadINIStr $R0 $0 "Field 6" "State"
   StrCmp $R0 "1" UseFile DontUseFile
UseFile:
   ReadINIStr $R0 $0 "Field 7" "State"
   CopyFiles $R0 "$WINDIR\afsdcell.ini"
DontUseFile:
   
  ;Store install folder
  WriteRegStr HKCU "${AFS_REGKEY_ROOT}\Client" "" $INSTDIR
  Call AFSCommon.Install
  
  
  ; Write registry entries
  WriteRegStr HKCR "*\shellex\ContextMenuHandlers\AFS Client Shell Extension" "(Default)" "{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}"
  WriteRegStr HKCR "CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}" "(Default)" "AFS Client Shell Extension"
  WriteRegStr HKCR "CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}\InprocServer32" "(Default)" "$INSTDIR\Client\Program\afs_shl_ext.dll"
  WriteRegStr HKCR "CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}\InprocServer32" "ThreadingModel" "Apartment"
  WriteRegStr HKCR "FOLDER\shellex\ContextMenuHandlers\AFS Client Shell Extension" "(Default)" "{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}"
  
  ; AFS Reg entries
  DeleteRegKey HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "VersionString" ${MUI_VERSION}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "Title" "AFS Client"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "Description" "AFS Client"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "PathName" "$INSTDIR\Client\Program"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "Software Type" "File System"
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "MajorVersion" ${MUI_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "MinorVersion" ${MUI_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "PatchLevel" ${MUI_PATCHLEVEL}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\${MUI_VERSION}" "VersionString" ${MUI_VERSION}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\${MUI_VERSION}" "Title" "AFS Client"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\${MUI_VERSION}" "Description" "AFS Client"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\${MUI_VERSION}" "Software Type" "File System"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\${MUI_VERSION}" "PathName" "$INSTDIR\Client\Program"
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Client\${MUI_VERSION}" "MajorVersion" ${MUI_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Client\${MUI_VERSION}" "MinorVersion" ${MUI_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Client\${MUI_VERSION}" "PatchLevel" ${MUI_PATCHLEVEL}

  ; Daemon entries
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon" "(Default)" ""
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\NetworkProvider" "AuthentProviderPath" "$INSTDIR\Client\Program\afslogon.dll"
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\NetworkProvider" "Class" 2
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\NetworkProvider" "LogonOptions" 0
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\NetworkProvider" "LogonScript" "$INSTDIR\Client\Program\afscreds.exe -:%s -x"
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\NetworkProvider" "Name" "OpenAFSDaemon"
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\NetworkProvider" "ProviderPath" "$INSTDIR\Client\Program\afslogon.dll"
   
  ;Write start menu entries
  CreateDirectory "$SMPROGRAMS\OpenAFS\Client"
  CreateShortCut '"$SMPROGRAMS\OpenAFS\Uninstall OpenAFS.lnk"' '"$INSTDIR\Uninstall.exe"'
  CreateShortCut '"$SMPROGRAMS\OpenAFS\Client\Authentication.lnk"' '"$INSTDIR\Client\Program\afscreds.exe"'
  CreateShortCut '"$SMSTARTUP\AFS Credentials.lnk"' '"$INSTDIR\Client\Program\afscreds.exe"'

  Push "$INSTDIR\Client\Program"
  Call AddToPath
  Push "$INSTDIR\Common"
  Call AddToPath
  
  ; Create the AFS service
  GetTempFileName $R0
  File /oname=$R0 "${AFS_WININSTALL_DIR}\Service.exe"
  nsExec::Exec '$R0 TransarcAFSDaemon "$INSTDIR\Client\Program\afsd_service.exe" "OpenAFS Client Service"'
  Delete $R0

  ;Write cell name
  ReadINIStr $R0 $1 "Field 2" "State"
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "Cell" $R0
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "ShowTrayIcon" 1
  
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  
SectionEnd

;------------------------
; OpenAFS SERVER  
Section "AFS Server" SecServer

  SetShellVarContext all

  CreateDirectory "$INSTDIR\Server\usr\afs\etc"
  CreateDirectory "$INSTDIR\Server\usr\afs\local"
  CreateDirectory "$INSTDIR\Server\usr\afs\logs"
  
  SetOutPath "$INSTDIR\Server\usr\afs\bin"  
  File "${AFS_SERVER_BUILDDIR}\afskill.exe"
  File "${AFS_SERVER_BUILDDIR}\afssvrcfg.exe"
  File "${AFS_SERVER_BUILDDIR}\bosctlsvc.exe"
  File "${AFS_SERVER_BUILDDIR}\bosserver.exe"
  File "${AFS_SERVER_BUILDDIR}\buserver.exe"
  File "${AFS_ETC_BUILDDIR}\butc.exe"
  File "${AFS_SERVER_BUILDDIR}\fileserver.exe"
  File "${AFS_ETC_BUILDDIR}\fms.exe"
  File "${AFS_SERVER_BUILDDIR}\kaserver.exe"
  File "${AFS_SERVER_BUILDDIR}\ptserver.exe"
  File "${AFS_SERVER_BUILDDIR}\salvager.exe"
  File "${AFS_SERVER_BUILDDIR}\upclient.exe"
  File "${AFS_SERVER_BUILDDIR}\upserver.exe"
  File "${AFS_SERVER_BUILDDIR}\vlserver.exe"
  File "${AFS_SERVER_BUILDDIR}\volinfo.exe"
  File "${AFS_SERVER_BUILDDIR}\volserver.exe"
 
 ;AFS Server common files
 SetOutPath "$INSTDIR\Common"
 File "${AFS_SERVER_BUILDDIR}\afsvosadmin.dll"
 File "${AFS_SERVER_BUILDDIR}\afsbosadmin.dll"
 File "${AFS_SERVER_BUILDDIR}\afscfgadmin.dll"
 File "${AFS_SERVER_BUILDDIR}\afskasadmin.dll"
 File "${AFS_SERVER_BUILDDIR}\afsptsadmin.dll"
 SetOutPath "$INSTDIR\Common"
 File "${AFS_WININSTALL_DIR}\Msvcr71.dll"
   Call AFSLangFiles
   
  ;Store install folder
  WriteRegStr HKCU "${AFS_REGKEY_ROOT}\AFS Server" "" $INSTDIR
  
  DeleteRegKey HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "VersionString" ${MUI_VERSION}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "Title" "AFS Server"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "Description" "AFS Server for Windows"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "PathName" "$INSTDIR\Server"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "Software Type" "File System"
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "MajorVersion" ${MUI_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "MinorVersion" ${MUI_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "PatchLevel" ${MUI_PATCHLEVEL}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\${MUI_VERSION}" "VersionString" ${MUI_VERSION}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\${MUI_VERSION}" "Title" "AFS Server"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\${MUI_VERSION}" "Description" "AFS Server for Windows"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\${MUI_VERSION}" "Software Type" "File System"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\${MUI_VERSION}" "PathName" "$INSTDIR\Server"
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Server\${MUI_VERSION}" "MajorVersion" ${MUI_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Server\${MUI_VERSION}" "MinorVersion" ${MUI_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Server\${MUI_VERSION}" "PatchLevel" ${MUI_PATCHLEVEL}

  ; Install the service
  GetTempFileName $R0
  File /oname=$R0 "${AFS_WININSTALL_DIR}\Service.exe"
  nsExec::Exec '$R0 TransarcAFSServer "$INSTDIR\Server\usr\afs\bin\bosctlsvc.exe" "OpenAFS AFS Server"'
  Delete $R0
  
  CreateShortCut "$SMPROGRAMS\OpenAFS\Server\Configuration Wizard.lnk" '"$INSTDIR\Server\usr\afs\bin\afssvrcfg.exe" /wizard'
  
  WriteUninstaller "$INSTDIR\Uninstall.exe"

SectionEnd


;----------------------------
; OpenAFS Control Center
Section "AFS Control Center" SecControl

  SetShellVarContext all

   SetOutPath "$INSTDIR\Control Center"
  File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager.exe"
  File "${AFS_SERVER_BUILDDIR}\TaAfsAdmSvr.exe"
  File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager.exe"
   
 ;AFS Server common files
 Call AFSCommon.Install
 Call AFSLangFiles
 SetOutPath "$INSTDIR\Common"
 File "${AFS_SERVER_BUILDDIR}\afsvosadmin.dll"
 File "${AFS_SERVER_BUILDDIR}\afsbosadmin.dll"
 File "${AFS_SERVER_BUILDDIR}\afscfgadmin.dll"
 File "${AFS_SERVER_BUILDDIR}\afskasadmin.dll"
 File "${AFS_SERVER_BUILDDIR}\afsptsadmin.dll"

  SetOutPath "$INSTDIR\Common"
  File "${AFS_WININSTALL_DIR}\Msvcr71.dll"
      
   
   ;Store install folder
  WriteRegStr HKCU "${AFS_REGKEY_ROOT}\AFS Control Center\CurrentVersion" "PathName" $INSTDIR
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\CurrentVersion" "VersionString" ${MUI_VERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\CurrentVersion" "MajorVersion" ${MUI_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\CurrentVersion" "MinorVersion" ${MUI_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\CurrentVersion" "PatchLevel" ${MUI_PATCHLEVEL}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\${MUI_VERSION}" "VersionString" ${MUI_VERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\${MUI_VERSION}" "MajorVersion" ${MUI_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\${MUI_VERSION}" "MinorVersion" ${MUI_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\${MUI_VERSION}" "PatchLevel" ${MUI_PATCHLEVEL}
  

  ;Write start menu entries
  CreateDirectory "$SMPROGRAMS\OpenAFS\Control Center"
  CreateShortCut "$SMPROGRAMS\OpenAFS\Control Center\Account Manager.lnk" "$INSTDIR\Control Center\TaAfsAccountManager.exe"
  CreateShortCut "$SMPROGRAMS\OpenAFS\Control Center\Server Manager.lnk" "$INSTDIR\Control Center\TaAfsServerManager.exe"
  
  WriteUninstaller "$INSTDIR\Uninstall.exe"

SectionEnd   


;----------------------------
; OpenAFS Supplemental Documentation
Section "Supplemental Documentation" SecDocs
  SetShellVarContext all

   StrCmp $LANGUAGE ${LANG_ENGLISH} DoEnglish
   StrCmp $LANGUAGE ${LANG_GERMAN} DoGerman
   StrCmp $LANGUAGE ${LANG_SPANISH} DoSpanish
   StrCmp $LANGUAGE ${LANG_JAPANESE} DoJapanese
   ;StrCmp $LANGUAGE ${LANG_KOREAN} DoKorean
   StrCmp $LANGUAGE ${LANG_PORTUGUESEBR} DoPortugueseBR
   StrCmp $LANGUAGE ${LANG_SIMPCHINESE} DoSimpChinese
   StrCmp $LANGUAGE ${LANG_TRADCHINESE} DoTradChinese
   
   
DoEnglish:
   SetOutPath "$INSTDIR\Documentation\html"
   File "..\..\doc\install\Documentation\en_US\html\*"
   SetOutPath "$INSTDIR\Documentation\html\CmdRef"
   File "..\..\doc\install\Documentation\en_US\html\CmdRef\*"
   SetOutPath "$INSTDIR\Documentation\html\InstallGd"
   File "..\..\doc\install\Documentation\en_US\html\InstallGd\*"
   SetOutPath "$INSTDIR\Documentation\html\ReleaseNotes"
   File "..\..\doc\install\Documentation\en_US\html\ReleaseNotes\*"
   SetOutPath "$INSTDIR\Documentation\html\SysAdminGd"
   File "..\..\doc\install\Documentation\en_US\html\SysAdminGd\*"
   goto DoneLanguage
   
DoGerman:
   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\de_DE\README.TXT"
   SetOutPath "$INSTDIR\Documentation\html"
   File "..\..\doc\install\Documentation\de_DE\html\*"
   ;SetOutPath "$INSTDIR\Documentation\html\CmdRef"
   ;File "..\..\doc\install\Documentation\de_DE\html\CmdRef\*"
   SetOutPath "$INSTDIR\Documentation\html\InstallGd"
   File "..\..\doc\install\Documentation\de_DE\html\InstallGd\*"
   ;SetOutPath "$INSTDIR\Documentation\html\ReleaseNotes"
   ;File "..\..\doc\install\Documentation\de_DE\html\ReleaseNotes\*"
   ;SetOutPath "$INSTDIR\Documentation\html\SysAdminGd"
   ;File "..\..\doc\install\Documentation\de_DE\html\SysAdminGd\*"
   goto DoneLanguage
   
DoSpanish:
   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\es_ES\README.TXT"
   SetOutPath "$INSTDIR\Documentation\html"
   File "..\..\doc\install\Documentation\es_ES\html\*"
   ;SetOutPath "$INSTDIR\Documentation\html\CmdRef"
   ;File "..\..\doc\install\Documentation\es_ES\html\CmdRef\*"
   SetOutPath "$INSTDIR\Documentation\html\InstallGd"
   File "..\..\doc\install\Documentation\es_ES\html\InstallGd\*"
   ;SetOutPath "$INSTDIR\Documentation\html\ReleaseNotes"
   ;File "..\..\doc\install\Documentation\es_ES\html\ReleaseNotes\*"
   ;SetOutPath "$INSTDIR\Documentation\html\SysAdminGd"
   ;File "..\..\doc\install\Documentation\es_ES\html\SysAdminGd\*"
   goto DoneLanguage

DoJapanese:
   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\ja_JP\README.TXT"
   SetOutPath "$INSTDIR\Documentation\html"
   File "..\..\doc\install\Documentation\ja_JP\html\*"
   SetOutPath "$INSTDIR\Documentation\html\CmdRef"
   File "..\..\doc\install\Documentation\ja_JP\html\CmdRef\*"
   SetOutPath "$INSTDIR\Documentation\html\InstallGd"
   File "..\..\doc\install\Documentation\ja_JP\html\InstallGd\*"
   SetOutPath "$INSTDIR\Documentation\html\ReleaseNotes"
   File "..\..\doc\install\Documentation\ja_JP\html\ReleaseNotes\*"
   SetOutPath "$INSTDIR\Documentation\html\SysAdminGd"
   File "..\..\doc\install\Documentation\ja_JP\html\SysAdminGd\*"
   goto DoneLanguage
   
DoKorean:
   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\ko_KR\README.TXT"
   SetOutPath "$INSTDIR\Documentation\html"
   File "..\..\doc\install\Documentation\ko_KR\html\*"
   ;SetOutPath "$INSTDIR\Documentation\html\CmdRef"
   ;File "..\..\doc\install\Documentation\ko_KR\html\CmdRef\*"
   SetOutPath "$INSTDIR\Documentation\html\InstallGd"
   File "..\..\doc\install\Documentation\ko_KR\html\InstallGd\*"
   SetOutPath "$INSTDIR\Documentation\html\ReleaseNotes"
   File "..\..\doc\install\Documentation\ko_KR\html\ReleaseNotes\*"
   SetOutPath "$INSTDIR\Documentation\html\SysAdminGd"
   File "..\..\doc\install\Documentation\ko_KR\html\SysAdminGd\*"
   goto DoneLanguage
   
DoPortugueseBR:
   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\pt_BR\README.TXT"
   SetOutPath "$INSTDIR\Documentation\html"
   File "..\..\doc\install\Documentation\pt_BR\html\*"
   ;SetOutPath "$INSTDIR\Documentation\html\CmdRef"
   ;File "..\..\doc\install\Documentation\pt_BR\html\CmdRef\*"
   SetOutPath "$INSTDIR\Documentation\html\InstallGd"
   File "..\..\doc\install\Documentation\pt_BR\html\InstallGd\*"
   SetOutPath "$INSTDIR\Documentation\html\ReleaseNotes"
   File "..\..\doc\install\Documentation\pt_BR\html\ReleaseNotes\*"
   ;SetOutPath "$INSTDIR\Documentation\html\SysAdminGd"
   ;File "..\..\doc\install\Documentation\pt_BR\html\SysAdminGd\*"
   goto DoneLanguage

DoSimpChinese:
   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\zh_CN\README.TXT"
   SetOutPath "$INSTDIR\Documentation\html"
   File "..\..\doc\install\Documentation\zh_CN\html\*"
   ;SetOutPath "$INSTDIR\Documentation\html\CmdRef"
   ;File "..\..\doc\install\Documentation\zh_CN\html\CmdRef\*"
   SetOutPath "$INSTDIR\Documentation\html\InstallGd"
   File "..\..\doc\install\Documentation\zh_CN\html\InstallGd\*"
   SetOutPath "$INSTDIR\Documentation\html\ReleaseNotes"
   File "..\..\doc\install\Documentation\zh_CN\html\ReleaseNotes\*"
   ;SetOutPath "$INSTDIR\Documentation\html\SysAdminGd"
   ;File "..\..\doc\install\Documentation\zh_CN\html\SysAdminGd\*"
   goto DoneLanguage
   
DoTradChinese:
   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\zh_TW\README.TXT"
   SetOutPath "$INSTDIR\Documentation\html"
   File "..\..\doc\install\Documentation\zh_TW\html\*"
   ;SetOutPath "$INSTDIR\Documentation\html\CmdRef"
   ;File "..\..\doc\install\Documentation\zh_TW\html\CmdRef\*"
   SetOutPath "$INSTDIR\Documentation\html\InstallGd"
   File "..\..\doc\install\Documentation\zh_TW\html\InstallGd\*"
   SetOutPath "$INSTDIR\Documentation\html\ReleaseNotes"
   File "..\..\doc\install\Documentation\zh_TW\html\ReleaseNotes\*"
   ;SetOutPath "$INSTDIR\Documentation\html\SysAdminGd"
   ;File "..\..\doc\install\Documentation\zh_TW\html\SysAdminGd\*"
   goto DoneLanguage
   
   
DoneLanguage:
   ;Store install folder
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation" "" $INSTDIR
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\CurrentVersion" "VersionString" ${MUI_VERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\CurrentVersion" "MajorVersion" ${MUI_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\CurrentVersion" "MinorVersion" ${MUI_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\CurrentVersion" "PatchLevel" ${MUI_PATCHLEVEL}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\${MUI_VERSION}" "VersionString" ${MUI_VERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\${MUI_VERSION}" "MajorVersion" ${MUI_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\${MUI_VERSION}" "MinorVersion" ${MUI_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\${MUI_VERSION}" "PatchLevel" ${MUI_PATCHLEVEL}

  ; Write start menu shortcut
  SetOutPath "$SMPROGRAMS\OpenAFS"
  CreateShortCut "$SMPROGRAMS\OpenAFS\Documentation.lnk" "$INSTDIR\Documentation\html\index.htm"
  
  
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateShortCut "$SMPROGRAMS\OpenAFS\Uninstall OpenAFS.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd  
  
;Display the Finish header
;Insert this macro after the sections if you are not using a finish page
;!insertmacro MUI_SECTIONS_FINISHHEADER

;--------------------------------
;Installer Functions

Function .onInit

  !insertmacro MUI_LANGDLL_DISPLAY
  
  ; Set the default install options
  	Push $0

   Call ShouldClientInstall
   Pop $R2
   
   StrCmp $R2 "0" NoClient
   StrCmp $R2 "2" UpgradeClient
   
	StrCpy $1 ${secClient} ; Gotta remember which section we are at now...
	SectionGetFlags ${secClient} $0
	IntOp $0 $0 | ${SF_SELECTED}
	SectionSetFlags ${secClient} $0
	# !insertmacro SelectSection ${secClient}
   goto skipClient
NoClient:
	StrCpy $1 ${secClient} ; Gotta remember which section we are at now...
	SectionGetFlags ${secClient} $0
	IntOp $0 $0 | ${SECTION_OFF}
	SectionSetFlags ${secClient} $0
   goto skipClient
UpgradeClient:
	SectionGetFlags ${secClient} $0
	IntOp $0 $0 | ${SF_SELECTED}
	SectionSetFlags ${secClient} $0
   SectionSetText ${secClient} $(UPGRADE_CLIENT)
   goto skipClient

   
skipClient:   
   
   
   Call IsServerInstalled
   Pop $R2
   StrCmp $R2 "0" NoServer
   
	SectionGetFlags ${secServer} $0
	IntOp $0 $0 & ${SF_SELECTED}
	SectionSetFlags ${secServer} $0
	# !insertmacro UnselectSection ${secServer}
   goto skipServer

NoServer:
	SectionGetFlags ${secServer} $0
	IntOp $0 $0 & ${SECTION_OFF}
	SectionSetFlags ${secServer} $0
	# !insertmacro UnselectSection ${secServer}
   
skipServer:   
	SectionGetFlags ${secControl} $0
	IntOp $0 $0 & ${SECTION_OFF}
	SectionSetFlags ${secControl} $0
	# !insertmacro UnselectSection ${secControl}

	SectionGetFlags ${secDocs} $0
	IntOp $0 $0 | ${SF_SELECTED}
	SectionSetFlags ${secDocs} $0
	# !insertmacro UnselectSection ${secDocs}

	Pop $0
  
  
  
  GetTempFilename $0
  File /oname=$0 CellServPage.ini
  GetTempFilename $1
  File /oname=$1 AFSCell.ini
   
  
FunctionEnd





;--------------------------------
; These are our cleanup functions
Function .onInstFailed
Delete $0

FunctionEnd

Function .onInstSuccess
Delete $0

FunctionEnd


;--------------------------------
;Descriptions

!insertmacro MUI_FUNCTIONS_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecServer} $(DESC_SecServer)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecClient} $(DESC_SecClient)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecControl} $(DESC_SecControl)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDocs} $(DESC_SecDocs)
!insertmacro MUI_FUNCTIONS_DESCRIPTION_END
 
;--------------------------------
;Uninstaller Section

Section "Uninstall"
  
  SetShellVarContext all
  ; Delete the AFS service
  GetTempFileName $R0
  File /oname=$R0 "${AFS_WININSTALL_DIR}\Service.exe"
  nsExec::Exec "net stop TransarcAFSDaemon"
  nsExec::Exec "net stop TransarcAFSServer"
  nsExec::Exec '$R0 u TransarcAFSDaemon'
  nsExec::Exec '$R0 u TransarcAFSServer'
  Delete $R0
  
  Push "$INSTDIR\Client\Program"
  Call un.RemoveFromPath
  Push "$INSTDIR\Common"
  Call un.RemoveFromPath
  
  ; Delete documentation
  Delete "$INSTDIR\Documentation\README.TXT"
  Delete "$INSTDIR\Documentation\html\*"
  Delete "$INSTDIR\Documentation\html\CmdRef\*"
  Delete "$INSTDIR\Documentation\html\InstallGd\*"
  Delete "$INSTDIR\Documentation\html\ReleaseNotes\*"
  Delete "$INSTDIR\Documentation\html\SysAdminGd\*"


  Delete "$WINDIR\afsdcell.ini"
  
  Delete "$INSTDIR\Uninstall.exe"

  ; Remove server
  Delete "$INSTDIR\Server\usr\afs\bin\afskill.exe"
  Delete "$INSTDIR\Server\usr\afs\bin\afssvrcfg.exe"
  Delete "$INSTDIR\Server\usr\afs\bin\bosctlsvc.exe"
  Delete "$INSTDIR\Server\usr\afs\bin\bosserver.exe"
  Delete "$INSTDIR\Server\usr\afs\bin\buserver.exe"
  Delete "$INSTDIR\Server\usr\afs\bin\butc.exe"
  Delete "$INSTDIR\Server\usr\afs\bin\fileserver.exe"
  Delete "$INSTDIR\Server\usr\afs\bin\fms.exe"
  Delete "$INSTDIR\Server\usr\afs\bin\kaserver.exe"
  Delete "$INSTDIR\Server\usr\afs\bin\ptserver.exe"
  Delete "$INSTDIR\Server\usr\afs\bin\salvager.exe"
  Delete "$INSTDIR\Server\usr\afs\bin\ServerUninst.dll"
  Delete "$INSTDIR\Server\usr\afs\bin\upclient.exe"
  Delete "$INSTDIR\Server\usr\afs\bin\upserver.exe"
  Delete "$INSTDIR\Server\usr\afs\bin\vlserver.exe"
  Delete "$INSTDIR\Server\usr\afs\bin\volinfo.exe"
  Delete "$INSTDIR\Server\usr\afs\bin\volserver.exe"
  RMDir "$INSTDIR\Server\usr\afs\bin"
  RmDir /r "$INSTDIR\Server\usr\afs\etc"
  RmDir /r "$INSTDIR\Server\usr\afs\local"
  RMDIR /r "$INSTDIR\Server\usr\afs\logs"
  
  RMDir /r "$INSTDIR\Documentation\html\CmdRef"
  RMDir /r "$INSTDIR\Documentation\html\InstallGd"
  RMDir /r "$INSTDIR\Documentation\html\ReleaseNotes"
  RMDir /r "$INSTDIR\Documentation\html\SysAdminGd"
  RMDIr /r "$INSTDIR\Documentation\html"
  
  RMDir "$INSTDIR\Documentation"
  ; Delete DOC short cut
  RMDir /r "$INSTDIR\Client\Program"
  RMDir /r "$INSTDIR\Client"
  RMDir /r "$INSTDIR\Common"

  Delete "$SMPROGRAMS\OpenAFS\Documentation.lnk"

  ; Remove control center
  Delete "$INSTDIR\Control Center\TaAfsAccountManager.exe"
  Delete "$INSTDIR\Control Center\TaAfsAdmSvr.exe"
  Delete "$INSTDIR\Control Center\TaAfsServerManager.exe"
  Delete "$INSTDIR\Control Center\CCUninst.dll"
  RMDir "$INSTDIR\Control Center"
  
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\OpenAFS\Uninstall OpenAFS.lnk"
  Delete "$SMPROGRAMS\OpenAFS\Client\Authentication.lnk"
  Delete "$SMPROGRAMS\OpenAFS\Control Center\Account Manager.lnk"
  Delete "$SMPROGRAMS\OpenAFS\Control Center\Server Manager.lnk"
  RMDIR "$SMPROGRAMS\OpenAFS\Control Center"
  RMDir /r "$SMPROGRAMS\OpenAFS\Client"
  RMDir /r "$SMPROGRAMS\OpenAFS"
  
  DeleteRegKey HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion"
  DeleteRegKey HKLM "${AFS_REGKEY_ROOT}\AFS Client"
  DeleteRegKey HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\CurrentVersion"
  DeleteRegKey HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation"
  DeleteRegKey HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\CurrentVersion"
  DeleteRegKey HKLM "${AFS_REGKEY_ROOT}\AFS Control Center"
  DeleteRegKey HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion"
  DeleteRegKey HKLM "${AFS_REGKEY_ROOT}\AFS Server"
  DeleteRegKey /ifempty HKLM "${AFS_REGKEY_ROOT}"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenAFS"

  ;Display the Finish header
  !insertmacro MUI_UNFINISHHEADER

SectionEnd

;--------------------------------
;Uninstaller Functions

Function un.onInit

  ;Get language from registry
  ReadRegStr $LANGUAGE HKCU "Software\OpenAFS\AFS" "Installer Language"
  
FunctionEnd


;------------------------------
; Get the CellServDB file from the Internet

Function afs.GetCellServDB

;Check if we should download CellServDB
ReadINIStr $R0 $0 "Field 4" "State"
StrCmp $R0 "0" CheckIncl

   ReadINIStr $R0 $0 "Field 5" "State"
   NSISdl::download $R0 "$WINDIR\afsdcell.ini"
   Pop $R0 ;Get the return value
   StrCmp $R0 "success" +2
      MessageBox MB_OK|MB_ICONSTOP "Download failed: $R0"
   goto done
CheckIncl:
   ReadINIStr $R0 $0 "Field 3" "State"
   StrCmp $R0 "0" CheckOther
   SetOutPath "$WINDIR"
   File "afsdcell.ini"
   goto done
   
CheckOther:
   ReadINIStr $R0 $0 "Field 7" "State"
   CopyFiles $R0 "$WINDIR\afsdcell.ini"
   
done:

FunctionEnd



;-------------------------------
;Do the page to get the CellServDB

Function AFSPageGetCellServDB
  ; Set the install options here
  
startOver:
  WriteINIStr $0 "Field 2" "Flags" "DISABLED"
  WriteINIStr $0 "Field 3" "State" "1"
  
  InstallOptions::dialog $0
  Pop $R1
  StrCmp $R1 "cancel" exit
  StrCmp $R1 "back" done
  StrCmp $R1 "success" done
exit: Quit
done:

   ; Check that if a file is set, a filename is entered...
   ReadINIStr $R0 $0 "Field 6" "State"
   StrCmp $R0 "1" CheckFileName Skip
CheckFileName:
   ReadINIStr $R0 $0 "Field 7" "State"
   IfFileExists $R0 Skip

   MessageBox MB_OK|MB_ICONSTOP $(CellError)
   WriteINIStr $0 "Field 6" "State" "0"
   goto startOver
   
   Skip:
   
FunctionEnd


Function AFSPageGetCellName
   Call IsSilent
   Pop $R1
   StrCmp $R1 "/S" exit
  InstallOptions::dialog $1
  Pop $R1
  StrCmp $R1 "cancel" exit
  StrCmp $R1 "back" done
  StrCmp $R1 "success" done
exit: Quit
done:
FunctionEnd

;-------------
; Common install routines for each module
Function AFSCommon.Install

WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenAFS" "DisplayName" "OpenAFS for Windows"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenAFS" "UninstallString" "$INSTDIR\uninstall.exe"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenAFS" "DisplayVersion" "${MUI_VERSION}"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenAFS" "URLInfoAbout" "http://www.openafs.org/"

FunctionEnd


; Check if install should be silent
Function IsSilent
  Push $0
  Push $CMDLINE
  Push "/S"
  Call StrStr
  Pop $0
  StrCpy $0 $0 3
  StrCmp $0 "/S" silent
  StrCmp $0 "/S " silent
    StrCpy $0 0
    Goto notsilent
  silent: StrCpy $0 1
  notsilent: Exch $0
FunctionEnd


; StrStr function
Function StrStr
  Exch $R1 ; st=haystack,old$R1, $R1=needle
  Exch    ; st=old$R1,haystack
  Exch $R2 ; st=old$R1,old$R2, $R2=haystack
  Push $R3
  Push $R4
  Push $R5
  StrLen $R3 $R1
  StrCpy $R4 0
  ; $R1=needle
  ; $R2=haystack
  ; $R3=len(needle)
  ; $R4=cnt
  ; $R5=tmp
  loop:
    StrCpy $R5 $R2 $R3 $R4
    StrCmp $R5 $R1 done
    StrCmp $R5 "" done
    IntOp $R4 $R4 + 1
    Goto loop
  done:
  StrCpy $R1 $R2 "" $R4
  Pop $R5
  Pop $R4
  Pop $R3
  Pop $R2
  Exch $R1
FunctionEnd

;-------------------
; Get the currently installed version and place it on the stack
; Modifies: Nothing
Function GetInstalledVersion
   Push $R0
   Push $R1
   Push $R4
   
   ReadRegDWORD $R0 HKLM "Software\TransarcCorporation\$R2\CurrentVersion" "VersionString"
   StrCmp $R0 "" NotTransarc
   
   
NotTransarc:
   ReadRegDWORD $R0 HKLM "${AFS_REGKEY_ROOT}\$R2\CurrentVersion" "VersionString"
   StrCmp $R0 "" done
   
done:
   Pop $R4
   Pop $R1
   Exch $R0
FunctionEnd


;-------------------------------
; Check if the client should be checked for default install
Function ShouldClientInstall
   Push $R0
   StrCpy $R2 "Client"
   Call GetInstalledVersion
   Pop $R0
   
   StrCmp $R0 "" NotInstalled
   
   StrCpy $R0 "0"
   Exch $R0
   goto end
   
NotInstalled:
   StrCpy $R0 "1"
   Exch $R0
end:   
FunctionEnd


; See if AFS Client is installed
Function IsServerInstalled
   Push $R0
   StrCpy $R2 "AFS Server"
   Call GetInstalledVersion
   Pop $R0
   
   StrCmp $R0 "" NotInstalled
   
   StrCpy $R0 "1"
   Exch $R0
   goto end
   
NotInstalled:
   StrCpy $R0 "0"
   Exch $R0
end:   
FunctionEnd


; See if AFS Server is installed
Function IsClientInstalled
   Push $R0
   StrCpy $R2 "AFS Client"
   Call GetInstalledVersion
   Pop $R0
   
   StrCmp $R0 "" NotInstalled
   
   StrCpy $R0 "1"
   Exch $R0
   goto end
   
NotInstalled:
   StrCpy $R0 "0"
   Exch $R0
end:   
FunctionEnd



; See if AFS Documentation is installed
Function IsDocumentationInstalled
   Push $R0
   StrCpy $R2 "AFS Supplemental Documentation"
   Call GetInstalledVersion
   Pop $R0
   
   StrCmp $R0 "" NotInstalled
   
   StrCpy $R0 "1"
   Exch $R0
   goto end
   
NotInstalled:
   StrCpy $R0 "0"
   Exch $R0
end:   
FunctionEnd


; See if COntrol Center is installed
Function IsControlInstalled
   Push $R0
   StrCpy $R2 "Control_Center"
   Call GetInstalledVersion
   Pop $R0
   
   StrCmp $R0 "" NotInstalled
   
   StrCpy $R0 "1"
   Exch $R0
   goto end
   
NotInstalled:
   StrCpy $R0 "0"
   Exch $R0
end:   
FunctionEnd


; GetParameters
; input, none
; output, top of stack (replaces, with e.g. whatever)
; modifies no other variables.

Function GetParameters
  Push $R0
  Push $R1
  Push $R2
  StrCpy $R0 $CMDLINE 1
  StrCpy $R1 '"'
  StrCpy $R2 1
  StrCmp $R0 '"' loop
    StrCpy $R1 ' ' ; we're scanning for a space instead of a quote
  loop:
    StrCpy $R0 $CMDLINE 1 $R2
    StrCmp $R0 $R1 loop2
    StrCmp $R0 "" loop2
    IntOp $R2 $R2 + 1
    Goto loop
  loop2:
    IntOp $R2 $R2 + 1
    StrCpy $R0 $CMDLINE 1 $R2
    StrCmp $R0 " " loop2
  StrCpy $R0 $CMDLINE "" $R2
  Pop $R2
  Pop $R1
  Exch $R0
FunctionEnd


;Check to see if any AFS component is installed
Function IsAnyAFSInstalled

FunctionEnd


;Install English Language Files
Function AFSLangFiles
   ; Common files
   SetOutPath "$INSTDIR\Common"
   File "${AFS_CLIENT_BUILDDIR}\afs_config.exe"
   File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext.dll"
   File "${AFS_SERVER_BUILDDIR}\afsadminutil.dll"
   File "${AFS_DESTDIR}\lib\afsauthent.dll"
   File "${AFS_DESTDIR}\lib\afspthread.dll"
   File "${AFS_DESTDIR}\lib\afsrpc.dll"
   File "${AFS_SERVER_BUILDDIR}\afsclientadmin.dll"
   File "${AFS_SERVER_BUILDDIR}\afsprocmgmt.dll"
   File "${AFS_SERVER_BUILDDIR}\afsvosadmin.dll"
   File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib.dll"

   StrCmp $LANGUAGE ${LANG_ENGLISH} DoEnglish
   StrCmp $LANGUAGE ${LANG_GERMAN} DoGerman
   StrCmp $LANGUAGE ${LANG_SPANISH} DoSpanish
   StrCmp $LANGUAGE ${LANG_JAPANESE} DoJapanese
   StrCmp $LANGUAGE ${LANG_KOREAN} DoKorean
   StrCmp $LANGUAGE ${LANG_PORTUGUESEBR} DoPortugueseBR
   StrCmp $LANGUAGE ${LANG_SIMPCHINESE} DoSimpChinese
   StrCmp $LANGUAGE ${LANG_TRADCHINESE} DoTradChinese
   
DoEnglish:

   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\en_US\README.TXT"

   SetOutPath "$INSTDIR\Common"
   File "${AFS_CLIENT_BUILDDIR}\afs_config_1033.dll"
   File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1033.dll"
   File "${AFS_CLIENT_BUILDDIR}\afscreds_1033.dll"
   File "${AFS_SERVER_BUILDDIR}\afseventmsg_1033.dll"
   ;File "${AFS_SERVER_BUILDDIR}\afs_setup_utils_1033.dll"
   File "${AFS_SERVER_BUILDDIR}\afsserver_1033.dll"
   File "${AFS_SERVER_BUILDDIR}\afssvrcfg_1033.dll"
   File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1033.dll"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1033.dll"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1033.dll"
   goto done

DoGerman:

   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\de_DE\README.TXT"

   SetOutPath "$INSTDIR\Common"
   File "${AFS_CLIENT_BUILDDIR}\afs_config_1032.dll"
   File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1032.dll"
   File "${AFS_CLIENT_BUILDDIR}\afscreds_1032.dll"
   File "${AFS_SERVER_BUILDDIR}\afseventmsg_1032.dll"
   ;File "${AFS_SERVER_BUILDDIR}\afs_setup_utils_1032.dll"
   File "${AFS_SERVER_BUILDDIR}\afsserver_1032.dll"
   File "${AFS_SERVER_BUILDDIR}\afssvrcfg_1032.dll"
   File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1032.dll"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1032.dll"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1032.dll"
   goto done   

DoSpanish:

   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\es_ES\README.TXT"

   SetOutPath "$INSTDIR\Common"
   File "${AFS_CLIENT_BUILDDIR}\afs_config_1034.dll"
   File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1034.dll"
   File "${AFS_CLIENT_BUILDDIR}\afscreds_1034.dll"
   File "${AFS_SERVER_BUILDDIR}\afseventmsg_1034.dll"
   ;File "${AFS_SERVER_BUILDDIR}\afs_setup_utils_1034.dll"
   File "${AFS_SERVER_BUILDDIR}\afsserver_1034.dll"
   File "${AFS_SERVER_BUILDDIR}\afssvrcfg_1034.dll"
   File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1034.dll"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1034.dll"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1034.dll"
   goto done

DoJapanese:

   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\ja_JP\README.TXT"

   SetOutPath "$INSTDIR\Common"
   File "${AFS_CLIENT_BUILDDIR}\afs_config_1041.dll"
   File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1041.dll"
   File "${AFS_CLIENT_BUILDDIR}\afscreds_1041.dll"
   File "${AFS_SERVER_BUILDDIR}\afseventmsg_1041.dll"
   ;File "${AFS_SERVER_BUILDDIR}\afs_setup_utils_1041.dll"
   File "${AFS_SERVER_BUILDDIR}\afsserver_1041.dll"
   File "${AFS_SERVER_BUILDDIR}\afssvrcfg_1041.dll"
   File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1041.dll"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1041.dll"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1041.dll"
   goto done
   
DoKorean:

   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\ko_KR\README.TXT"

   SetOutPath "$INSTDIR\Common"
   File "${AFS_CLIENT_BUILDDIR}\afs_config_1042.dll"
   File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1042.dll"
   File "${AFS_CLIENT_BUILDDIR}\afscreds_1042.dll"
   File "${AFS_SERVER_BUILDDIR}\afseventmsg_1042.dll"
   ;File "${AFS_SERVER_BUILDDIR}\afs_setup_utils_1042.dll"
   File "${AFS_SERVER_BUILDDIR}\afsserver_1042.dll"
   File "${AFS_SERVER_BUILDDIR}\afssvrcfg_1042.dll"
   File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1042.dll"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1042.dll"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1042.dll"
   goto done


DoPortugueseBR:

   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\pt_BR\README.TXT"

   SetOutPath "$INSTDIR\Common"
   File "${AFS_CLIENT_BUILDDIR}\afs_config_1046.dll"
   File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1046.dll"
   File "${AFS_CLIENT_BUILDDIR}\afscreds_1046.dll"
   File "${AFS_SERVER_BUILDDIR}\afseventmsg_1046.dll"
   ;File "${AFS_SERVER_BUILDDIR}\afs_setup_utils_1046.dll"
   File "${AFS_SERVER_BUILDDIR}\afsserver_1046.dll"
   File "${AFS_SERVER_BUILDDIR}\afssvrcfg_1046.dll"
   File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1046.dll"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1046.dll"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1046.dll"
   goto done
   
DoSimpChinese:

   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\zh_CN\README.TXT"

   SetOutPath "$INSTDIR\Common"
   File "${AFS_CLIENT_BUILDDIR}\afs_config_2052.dll"
   File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_2052.dll"
   File "${AFS_CLIENT_BUILDDIR}\afscreds_2052.dll"
   File "${AFS_SERVER_BUILDDIR}\afseventmsg_2052.dll"
   ;File "${AFS_SERVER_BUILDDIR}\afs_setup_utils_2052.dll"
   File "${AFS_SERVER_BUILDDIR}\afsserver_2052.dll"
   File "${AFS_SERVER_BUILDDIR}\afssvrcfg_2052.dll"
   File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_2052.dll"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_2052.dll"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_2052.dll"
   goto done
   
DoTradChinese:

   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\zh_TW\README.TXT"

   SetOutPath "$INSTDIR\Common"
   File "${AFS_CLIENT_BUILDDIR}\afs_config_1028.dll"
   File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1028.dll"
   File "${AFS_CLIENT_BUILDDIR}\afscreds_1028.dll"
   File "${AFS_SERVER_BUILDDIR}\afseventmsg_1028.dll"
   ;File "${AFS_SERVER_BUILDDIR}\afs_setup_utils_1028.dll"
   File "${AFS_SERVER_BUILDDIR}\afsserver_1028.dll"
   File "${AFS_SERVER_BUILDDIR}\afssvrcfg_1028.dll"
   File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1028.dll"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1028.dll"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1028.dll"
   goto done
   
done:
FunctionEnd



;====================================================
; AddToPath - Adds the given dir to the search path.
;        Input - head of the stack
;        Note - Win9x systems requires reboot
;====================================================
Function AddToPath
  Exch $0
  Push $1
  
  Call IsNT
  Pop $1
  StrCmp $1 1 AddToPath_NT
    ; Not on NT
    StrCpy $1 $WINDIR 2
    FileOpen $1 "$1\autoexec.bat" a
    FileSeek $1 0 END
    GetFullPathName /SHORT $0 $0
    FileWrite $1 "$\r$\nSET PATH=%PATH%;$0$\r$\n"
    FileClose $1
    Goto AddToPath_done

  AddToPath_NT:
    ReadRegStr $1 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH"
    StrCmp $1 "" AddToPath_NTdoIt
      StrCpy $0 "$1;$0"
      Goto AddToPath_NTdoIt
    AddToPath_NTdoIt:
      WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH" $0
      SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000
  
  AddToPath_done:
    Pop $1
    Pop $0
FunctionEnd

;====================================================
; RemoveFromPath - Remove a given dir from the path
;     Input: head of the stack
;====================================================
Function un.RemoveFromPath
  Exch $0
  Push $1
  Push $2
  Push $3
  Push $4
  
  Call un.IsNT
  Pop $1
  StrCmp $1 1 unRemoveFromPath_NT
    ; Not on NT
    StrCpy $1 $WINDIR 2
    FileOpen $1 "$1\autoexec.bat" r
    GetTempFileName $4
    FileOpen $2 $4 w
    GetFullPathName /SHORT $0 $0
    StrCpy $0 "SET PATH=%PATH%;$0"
    SetRebootFlag true
    Goto unRemoveFromPath_dosLoop
    
    unRemoveFromPath_dosLoop:
      FileRead $1 $3
      StrCmp $3 "$0$\r$\n" unRemoveFromPath_dosLoop
      StrCmp $3 "$0$\n" unRemoveFromPath_dosLoop
      StrCmp $3 "$0" unRemoveFromPath_dosLoop
      StrCmp $3 "" unRemoveFromPath_dosLoopEnd
      FileWrite $2 $3
      Goto unRemoveFromPath_dosLoop
    
    unRemoveFromPath_dosLoopEnd:
      FileClose $2
      FileClose $1
      StrCpy $1 $WINDIR 2
      Delete "$1\autoexec.bat"
      CopyFiles /SILENT $4 "$1\autoexec.bat"
      Delete $4
      Goto unRemoveFromPath_done

  unRemoveFromPath_NT:
    StrLen $2 $0
    ReadRegStr $1 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH"
    Push $1
    Push $0
    Call un.StrStr ; Find $0 in $1
    Pop $0 ; pos of our dir
    IntCmp $0 -1 unRemoveFromPath_done
      ; else, it is in path
      StrCpy $3 $1 $0 ; $3 now has the part of the path before our dir
      IntOp $2 $2 + $0 ; $2 now contains the pos after our dir in the path (';')
      IntOp $2 $2 + 1 ; $2 now containts the pos after our dir and the semicolon.
      StrLen $0 $1
      StrCpy $1 $1 $0 $2
      StrCpy $3 "$3$1"

      WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH" $3
      SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000
  
  unRemoveFromPath_done:
    Pop $4
    Pop $3
    Pop $2
    Pop $1
    Pop $0
FunctionEnd

;====================================================
; IsNT - Returns 1 if the current system is NT, 0
;        otherwise.
;     Output: head of the stack
;====================================================
Function IsNT
  Push $0
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
  StrCmp $0 "" 0 IsNT_yes
  ; we are not NT.
  Pop $0
  Push 0
  Return

  IsNT_yes:
    ; NT!!!
    Pop $0
    Push 1
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Uninstall sutff
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


;====================================================
; StrStr - Finds a given string in another given string.
;               Returns -1 if not found and the pos if found.
;          Input: head of the stack - string to find
;                      second in the stack - string to find in
;          Output: head of the stack
;====================================================
Function un.StrStr
  Push $0
  Exch
  Pop $0 ; $0 now have the string to find
  Push $1
  Exch 2
  Pop $1 ; $1 now have the string to find in
  Exch
  Push $2
  Push $3
  Push $4
  Push $5

  StrCpy $2 -1
  StrLen $3 $0
  StrLen $4 $1
  IntOp $4 $4 - $3

  unStrStr_loop:
    IntOp $2 $2 + 1
    IntCmp $2 $4 0 0 unStrStrReturn_notFound
    StrCpy $5 $1 $3 $2
    StrCmp $5 $0 unStrStr_done unStrStr_loop

  unStrStrReturn_notFound:
    StrCpy $2 -1

  unStrStr_done:
    Pop $5
    Pop $4
    Pop $3
    Exch $2
    Exch 2
    Pop $0
    Pop $1
FunctionEnd

;====================================================
; IsNT - Returns 1 if the current system is NT, 0
;        otherwise.
;     Output: head of the stack
;====================================================
Function un.IsNT
  Push $0
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
  StrCmp $0 "" 0 unIsNT_yes
  ; we are not NT.
  Pop $0
  Push 0
  Return

  unIsNT_yes:
    ; NT!!!
    Pop $0
    Push 1
FunctionEnd


