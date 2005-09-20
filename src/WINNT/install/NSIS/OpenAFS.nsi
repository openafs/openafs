; OpenAFS Install Script for NSIS
; This version compiles with NSIS v2.07
;
; Originally written by Rob Murawski <rsm4@ieee.org>
;
;Redistribution and use in source and binary forms, with or without modification, are permitted
;provided that the following conditions are met:
;
;  Redistributions of source code must retain the above copyright notice, this list of conditions
;  and the following disclaimer. Redistributions in binary form must reproduce the above copyright
;  notice, this list of conditions and the following disclaimer in the documentation and/or other
;  materials provided with the distribution. The name of the author may not be used to endorse or
;  promote products derived from this software without specific prior written permission.
;
;  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
;  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
;  PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
;  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
;  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
;  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
;  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
;  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;
;
;     Some code originally based on:
;     NSIS Modern User Interface version 1.63
;     MultiLanguage Example Script
;     Written by Joost Verburg

; Read in the environment information
!include ${INCLUDEDIR}\nsi-includes.nsi

!ifndef RELEASE
!ifndef DEBUG
Name "OpenAFS ${AFS_VERSION} ${__DATE__} ${__TIME__}"
!else
Name "OpenAFS ${AFS_VERSION} ${__DATE__} ${__TIME__} Checked/Debug"
!endif               ; End DEBUG/!DEBUG
!else
!ifndef DEBUG
Name "OpenAFS ${AFS_VERSION}"
!else                ; DEBUG
Name "OpenAFS ${AFS_VERSION} Checked/Debug"
!endif               ; End DEBUG/!DEBUG
!endif
VIProductVersion "${AFS_VERSION}.00"
VIAddVersionKey "ProductName" "OpenAFS"
VIAddVersionKey "CompanyName" "OpenAFS.org"
VIAddVersionKey "ProductVersion" ${AFS_VERSION}
VIAddVersionKey "FileVersion" ${AFS_VERSION}
VIAddVersionKey "FileDescription" "OpenAFS for Windows Installer"
VIAddVersionKey "LegalCopyright" "(C)2000-2005"
!ifdef DEBUG
VIAddVersionKey "PrivateBuild" "Checked/Debug"
!endif               ; End DEBUG


!include "MUI.nsh"
!include Sections.nsh

;--------------------------------
;Configuration

!define REPLACEDLL_NOREGISTER

  ;General
!ifndef AFSIFS
!ifndef DEBUG
  OutFile "${AFS_DESTDIR}\WinInstall\OpenAFSforWindows.exe"
!else
  OutFile "${AFS_DESTDIR}\WinInstall\OpenAFSforWindows-DEBUG.exe"
!endif
!else
!ifndef DEBUG
  OutFile "${AFS_DESTDIR}\WinInstall\OpenAFSforWindows-IFS.exe"
!else
  OutFile "${AFS_DESTDIR}\WinInstall\OpenAFSforWindows-IFS-DEBUG.exe"
!endif
!endif
  SilentInstall normal
  SetCompressor /solid lzma
  !define MUI_ICON "..\..\client_config\OpenAFS.ico"
  !define MUI_UNICON "..\..\client_config\OpenAFS.ico"
  !define AFS_COMPANY_NAME "OpenAFS"
  !define AFS_PRODUCT_NAME "OpenAFS"
  !define AFS_REGKEY_ROOT "Software\TransarcCorporation"
  CRCCheck force

  ;Folder selection page
  InstallDir "$PROGRAMFILES\OpenAFS"      ; Install to shorter path
  
  ;Remember install folder
  InstallDirRegKey HKCU ${AFS_REGKEY_ROOT} ""
  
  ;Remember the installer language
  !define MUI_LANGDLL_REGISTRY_ROOT "HKCU" 
  !define MUI_LANGDLL_REGISTRY_KEY ${AFS_REGKEY_ROOT}
  !define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"
  
  ;Where are the files?
  !define AFS_CLIENT_BUILDDIR "${AFS_DESTDIR}\root.client\usr\vice\etc"
  !define AFS_WININSTALL_DIR "${AFS_DESTDIR}\WinInstall\Config"
  !define AFS_BUILD_INCDIR "${AFS_DESTDIR}\include"
  !define AFS_CLIENT_LIBDIR "${AFS_DESTDIR}\lib"
  !define AFS_SERVER_BUILDDIR "${AFS_DESTDIR}\root.server\usr\afs\bin"
  !define AFS_ETC_BUILDDIR "${AFS_DESTDIR}\etc"
  !define SYSTEMDIR   "$%SystemRoot%\System32" 
  
;--------------------------------
;Modern UI Configuration

  ;!define MUI_LICENSEPAGE
  !define MUI_CUSTOMPAGECOMMANDS
  !define MUI_WELCOMEPAGE
  !define MUI_COMPONENTSPAGE
  !define MUI_COMPONENTSPAGE_SMALLDESC
  !define MUI_DIRECTORYPAGE

  !define MUI_ABORTWARNING
  !define MUI_FINISHPAGE
  
  !define MUI_UNINSTALLER
  !define MUI_UNCONFIRMPAGE
  
  
  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  Page custom AFSPageGetCellServDB
  Page custom AFSPageGetCellName
  Page custom AFSPageConfigAFSCreds
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH
  
  ;LicenseData "Licenses.rtf"
;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"
  ;!insertmacro MUI_LANGUAGE "French"
  !insertmacro MUI_LANGUAGE "German"
  !insertmacro MUI_LANGUAGE "Spanish"
  !insertmacro MUI_LANGUAGE "SimpChinese"
  !insertmacro MUI_LANGUAGE "TradChinese"    
  !insertmacro MUI_LANGUAGE "Japanese"
  !insertmacro MUI_LANGUAGE "Korean"
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

  LangString DESC_secClient ${LANG_ENGLISH} "OpenAFS Client: Allows you to access AFS from your Windows PC."
  LangString DESC_secClient ${LANG_GERMAN} "OpenAFS Client: Allows you to access AFS from your Windows PC."
  LangString DESC_secClient ${LANG_SPANISH} "OpenAFS Client: Allows you to access AFS from your Windows PC."
  LangString DESC_secClient ${LANG_SIMPCHINESE} "OpenAFS Client: Allows you to access AFS from your Windows PC."
  LangString DESC_secClient ${LANG_TRADCHINESE} "OpenAFS Client: Allows you to access AFS from your Windows PC."
  LangString DESC_secClient ${LANG_JAPANESE} "OpenAFS Client: Allows you to access AFS from your Windows PC."
  LangString DESC_secClient ${LANG_KOREAN} "OpenAFS Client: Allows you to access AFS from your Windows PC."
  LangString DESC_secClient ${LANG_PORTUGUESEBR} "OpenAFS Client: Allows you to access AFS from your Windows PC."
  
  LangString DESC_secLoopback ${LANG_ENGLISH} "MS Loopback adapter: Installs the adapter for a more reliable AFS client."
  LangString DESC_secLoopback ${LANG_GERMAN} "MS Loopback adapter: Installs the adapter for a more reliable AFS client."
  LangString DESC_secLoopback ${LANG_SPANISH} "MS Loopback adapter: Installs the adapter for a more reliable AFS client."
  LangString DESC_secLoopback ${LANG_SIMPCHINESE} "MS Loopback adapter: Installs the adapter for a more reliable AFS client."
  LangString DESC_secLoopback ${LANG_TRADCHINESE} "MS Loopback adapter: Installs the adapter for a more reliable AFS client."
  LangString DESC_secLoopback ${LANG_JAPANESE} "MS Loopback adapter: Installs the adapter for a more reliable AFS client."
  LangString DESC_secLoopback ${LANG_KOREAN} "MS Loopback adapter: Installs the adapter for a more reliable AFS client."
  LangString DESC_secLoopback ${LANG_PORTUGUESEBR} "MS Loopback adapter: Installs the adapter for a more reliable AFS client."

  LangString DESC_secServer ${LANG_ENGLISH} "OpenAFS Server: Allows you to run an AFS file server. This option requires the AFS Client."
  LangString DESC_secServer ${LANG_GERMAN} "OpenAFS Server: Allows you to run an AFS file server. This option requires the AFS Client."
  LangString DESC_secServer ${LANG_SPANISH} "OpenAFS Server: Allows you to run an AFS file server. This option requires the AFS Client."
  LangString DESC_secServer ${LANG_SIMPCHINESE} "OpenAFS Server: Allows you to run an AFS file server. This option requires the AFS Client."
  LangString DESC_secServer ${LANG_TRADCHINESE} "OpenAFS Server: Allows you to run an AFS file server. This option requires the AFS Client."
  LangString DESC_secServer ${LANG_JAPANESE} "OpenAFS Server: Allows you to run an AFS file server. This option requires the AFS Client."
  LangString DESC_secServer ${LANG_KOREAN} "OpenAFS Server: Allows you to run an AFS file server. This option requires the AFS Client."
  LangString DESC_secServer ${LANG_PORTUGUESEBR} "OpenAFS Server: Allows you to run an AFS file server. This option requires the AFS Client."
  
  LangString DESC_secControl ${LANG_ENGLISH} "Control Center: GUI utilities for managing and configuring AFS servers.  This option requires the AFS Client."
  LangString DESC_secControl ${LANG_GERMAN} "Control Center: GUI utilities for managing and configuring AFS servers.  This option requires the AFS Client."
  LangString DESC_secControl ${LANG_SPANISH} "Control Center: GUI utilities for managing and configuring AFS servers.  This option requires the AFS Client."
  LangString DESC_secControl ${LANG_SIMPCHINESE} "Control Center: GUI utilities for managing and configuring AFS servers.  This option requires the AFS Client."
  LangString DESC_secControl ${LANG_TRADCHINESE} "Control Center: GUI utilities for managing and configuring AFS servers.  This option requires the AFS Client."
  LangString DESC_secControl ${LANG_JAPANESE} "Control Center: GUI utilities for managing and configuring AFS servers.  This option requires the AFS Client."
  LangString DESC_secControl ${LANG_KOREAN} "Control Center: GUI utilities for managing and configuring AFS servers.  This option requires the AFS Client."
  LangString DESC_secControl ${LANG_PORTUGUESEBR} "Control Center: GUI utilities for managing and configuring AFS servers.  This option requires the AFS Client."
  
  LangString DESC_secDocs ${LANG_ENGLISH} "Supplemental Documentation: Additional documentation for using OpenAFS."
  LangString DESC_secDocs ${LANG_GERMAN} "Supplemental Documentation: Additional documentation for using OpenAFS."
  LangString DESC_secDocs ${LANG_SPANISH} "Supplemental Documentation: Additional documentation for using OpenAFS."
  LangString DESC_secDocs ${LANG_SIMPCHINESE} "Supplemental Documentation: Additional documentation for using OpenAFS."
  LangString DESC_secDocs ${LANG_TRADCHINESE} "Supplemental Documentation: Additional documentation for using OpenAFS."
  LangString DESC_secDocs ${LANG_JAPANESE} "Supplemental Documentation: Additional documentation for using OpenAFS."
  LangString DESC_secDocs ${LANG_KOREAN} "Supplemental Documentation: Additional documentation for using OpenAFS."
  LangString DESC_secDocs ${LANG_PORTUGUESEBR} "Supplemental Documentation: Additional documentation for using OpenAFS."
  
  LangString DESC_secSDK ${LANG_ENGLISH} "SDK: Header files and libraries for developing software with OpenAFS."
  LangString DESC_secSDK ${LANG_GERMAN} "SDK: Header files and libraries for developing software with OpenAFS."
  LangString DESC_secSDK ${LANG_SPANISH} "SDK: Header files and libraries for developing software with OpenAFS."
  LangString DESC_secSDK ${LANG_SIMPCHINESE} "SDK: Header files and libraries for developing software with OpenAFS."
  LangString DESC_secSDK ${LANG_TRADCHINESE} "SDK: Header files and libraries for developing software with OpenAFS."
  LangString DESC_secSDK ${LANG_JAPANESE} "SDK: Header files and libraries for developing software with OpenAFS."
  LangString DESC_secSDK ${LANG_KOREAN} "SDK: Header files and libraries for developing software with OpenAFS."
  LangString DESC_secSDK ${LANG_PORTUGUESEBR} "SDK: Header files and libraries for developing software with OpenAFS."
  
  LangString DESC_secDEBUG ${LANG_ENGLISH} "Debug symbols: Used for debugging problems with OpenAFS."
  LangString DESC_secDEBUG ${LANG_GERMAN} "Debug symbols: Used for debugging problems with OpenAFS."
  LangString DESC_secDEBUG ${LANG_SPANISH} "Debug symbols: Used for debugging problems with OpenAFS."
  LangString DESC_secDEBUG ${LANG_SIMPCHINESE} "Debug symbols: Used for debugging problems with OpenAFS."
  LangString DESC_secDEBUG ${LANG_TRADCHINESE} "Debug symbols: Used for debugging problems with OpenAFS."
  LangString DESC_secDEBUG ${LANG_JAPANESE} "Debug symbols: Used for debugging problems with OpenAFS."
  LangString DESC_secDEBUG ${LANG_KOREAN} "Debug symbols: Used for debugging problems with OpenAFS."
  LangString DESC_secDEBUG ${LANG_PORTUGUESEBR} "Debug symbols: Used for debugging problems with OpenAFS."

; Popup error messages
  LangString CellError ${LANG_ENGLISH} "You must specify a valid CellServDB file to copy during install"
  LangString CellError ${LANG_GERMAN} "You must specify a valid CellServDB file to copy during the install"
  LangString CellError ${LANG_SPANISH} "You must specify a valid CellServDB file to copy during the install"
  LangString CellError ${LANG_SIMPCHINESE} "You must specify a valid CellServDB file to copy during the install"
  LangString CellError ${LANG_TRADCHINESE} "You must specify a valid CellServDB file to copy during the install"
  LangString CellError ${LANG_JAPANESE} "You must specify a valid CellServDB file to copy during the install"
  LangString CellError ${LANG_KOREAN} "You must specify a valid CellServDB file to copy during the install"
  LangString CellError ${LANG_PORTUGUESEBR} "You must specify a valid CellServDB file to copy during the install"
  
  LangString CellNameError ${LANG_ENGLISH} "You must specify a cell name for your client to use."
  LangString CellNameError ${LANG_GERMAN} "You must specify a cell name for your client to use."
  LangString CellNameError ${LANG_SPANISH} "You must specify a cell name for your client to use."
  LangString CellNameError ${LANG_SIMPCHINESE} "You must specify a cell name for your client to use."
  LangString CellNameError ${LANG_TRADCHINESE} "You must specify a cell name for your client to use."
  LangString CellNameError ${LANG_JAPANESE} "You must specify a cell name for your client to use."
  LangString CellNameError ${LANG_KOREAN} "You must specify a cell name for your client to use."
  LangString CellNameError ${LANG_PORTUGUESEBR} "You must specify a cell name for your client to use."
  
  LangString URLError ${LANG_ENGLISH} "You must specify a URL if you choose the option to download the CellServDB."
  LangString URLError ${LANG_GERMAN} "You must specify a URL if you choose the option to download the CellServDB."
  LangString URLError ${LANG_SPANISH} "You must specify a URL if you choose the option to download the CellServDB."
  LangString URLError ${LANG_SIMPCHINESE} "You must specify a URL if you choose the option to download the CellServDB."
  LangString URLError ${LANG_TRADCHINESE} "You must specify a URL if you choose the option to download the CellServDB."
  LangString URLError ${LANG_JAPANESE} "You must specify a URL if you choose the option to download the CellServDB."
  LangString URLError ${LANG_KOREAN} "You must specify a URL if you choose the option to download the CellServDB."
  LangString URLError ${LANG_PORTUGUESEBR} "You must specify a URL if you choose the option to download the CellServDB."

  
; Upgrade/re-install strings
   LangString UPGRADE_CLIENT ${LANG_ENGLISH} "Upgrade AFS Client"
   LangString UPGRADE_CLIENT ${LANG_GERMAN} "Upgrade AFS Client"
   LangString UPGRADE_CLIENT ${LANG_SPANISH} "Upgrade AFS Client"
   LangString UPGRADE_CLIENT ${LANG_SIMPCHINESE} "Upgrade AFS Client"
   LangString UPGRADE_CLIENT ${LANG_TRADCHINESE} "Upgrade AFS Client"
   LangString UPGRADE_CLIENT ${LANG_JAPANESE} "Upgrade AFS Client"
   LangString UPGRADE_CLIENT ${LANG_KOREAN} "Upgrade AFS Client"
   LangString UPGRADE_CLIENT ${LANG_PORTUGUESEBR} "Upgrade AFS Client"
 
   LangString REINSTALL_CLIENT ${LANG_ENGLISH} "Re-install AFS Client"
   LangString REINSTALL_CLIENT ${LANG_GERMAN} "Re-install AFS Client"
   LangString REINSTALL_CLIENT ${LANG_SPANISH} "Re-install AFS Client"
   LangString REINSTALL_CLIENT ${LANG_SIMPCHINESE} "Re-install AFS Client"
   LangString REINSTALL_CLIENT ${LANG_TRADCHINESE} "Re-install AFS Client"
   LangString REINSTALL_CLIENT ${LANG_JAPANESE} "Re-install AFS Client"
   LangString REINSTALL_CLIENT ${LANG_KOREAN} "Re-install AFS Client"
   LangString REINSTALL_CLIENT ${LANG_PORTUGUESEBR} "Re-install AFS Client"
  
   LangString UPGRADE_SERVER ${LANG_ENGLISH} "Upgrade AFS Server"
   LangString UPGRADE_SERVER ${LANG_GERMAN} "Upgrade AFS Server"
   LangString UPGRADE_SERVER ${LANG_SPANISH} "Upgrade AFS Server"
   LangString UPGRADE_SERVER ${LANG_SIMPCHINESE} "Upgrade AFS Server"
   LangString UPGRADE_SERVER ${LANG_TRADCHINESE} "Upgrade AFS Server"
   LangString UPGRADE_SERVER ${LANG_JAPANESE} "Upgrade AFS Server"
   LangString UPGRADE_SERVER ${LANG_KOREAN} "Upgrade AFS Server"
   LangString UPGRADE_SERVER ${LANG_PORTUGUESEBR} "Upgrade AFS Server"
    
   LangString REINSTALL_SERVER ${LANG_ENGLISH} "Re-install AFS Server"
   LangString REINSTALL_SERVER ${LANG_GERMAN} "Re-install AFS Server"
   LangString REINSTALL_SERVER ${LANG_SPANISH} "Re-install AFS Server"
   LangString REINSTALL_SERVER ${LANG_SIMPCHINESE} "Re-install AFS Server"
   LangString REINSTALL_SERVER ${LANG_TRADCHINESE} "Re-install AFS Server"
   LangString REINSTALL_SERVER ${LANG_JAPANESE} "Re-install AFS Server"
   LangString REINSTALL_SERVER ${LANG_KOREAN} "Re-install AFS Server"
   LangString REINSTALL_SERVER ${LANG_PORTUGUESEBR} "Re-install AFS Server"
  
  ReserveFile "CellServPage.ini"
  ReserveFile "AFSCell.ini"
  !insertmacro MUI_RESERVEFILE_INSTALLOPTIONS ;InstallOptions plug-in
  !insertmacro MUI_RESERVEFILE_LANGDLL ;Language selection dialog
;--------------------------------
; Macros
; Macro - Upgrade DLL File
; Written by Joost Verburg
; ------------------------
;
; Parameters:
; LOCALFILE   - Location of the new DLL file (on the compiler system)
; DESTFILE    - Location of the DLL file that should be upgraded
;              (on the user's system)
; TEMPBASEDIR - Directory on the user's system to store a temporary file
;               when the system has to be rebooted.
;               For Win9x support, this should be on the same volume as the
;               DESTFILE!
;               The Windows temp directory could be located on any volume,
;               so you cannot use  this directory.
;
; Define REPLACEDLL_NOREGISTER if you want to upgrade a DLL that does not
; have to be registered.
;
; Note: If you want to support Win9x, you can only use
;       short filenames (8.3).
;
; Example of usage:
; !insertmacro ReplaceDLL "dllname.dll" "$SYSDIR\dllname.dll" "$SYSDIR"
;

!macro ReplaceDLL LOCALFILE DESTFILE TEMPBASEDIR

  Push $R0
  Push $R1
  Push $R2
  Push $R3
  Push $R4
  Push $R5

  ;------------------------
  ;Unique number for labels

  !define REPLACEDLL_UNIQUE ${__LINE__}

  ;------------------------
  ;Copy the parameters used on run-time to a variable
  ;This allows the usage of variables as paramter

  StrCpy $R4 "${DESTFILE}"
  StrCpy $R5 "${TEMPBASEDIR}"

  ;------------------------
  ;Check file and version
  ;
  IfFileExists $R4 0 replacedll.copy_${REPLACEDLL_UNIQUE}
  
  ;ClearErrors
  ;  GetDLLVersionLocal "${LOCALFILE}" $R0 $R1
  ;  GetDLLVersion $R4 $R2 $R3
  ;IfErrors replacedll.upgrade_${REPLACEDLL_UNIQUE}
  ;
  ;IntCmpU $R0 $R2 0 replacedll.done_${REPLACEDLL_UNIQUE} \
  ;  replacedll.upgrade_${REPLACEDLL_UNIQUE}
  ;IntCmpU $R1 $R3 replacedll.done_${REPLACEDLL_UNIQUE} \
  ;  replacedll.done_${REPLACEDLL_UNIQUE} \
  ;  replacedll.upgrade_${REPLACEDLL_UNIQUE}

  ;------------------------
  ;Let's replace the DLL!

  SetOverwrite try

  ;replacedll.upgrade_${REPLACEDLL_UNIQUE}:
    !ifndef REPLACEDLL_NOREGISTER
      ;Unregister the DLL
      UnRegDLL $R4
    !endif

  ;------------------------
  ;Try to copy the DLL directly

  ClearErrors
    StrCpy $R0 $R4
    Call :replacedll.file_${REPLACEDLL_UNIQUE}
  IfErrors 0 replacedll.noreboot_${REPLACEDLL_UNIQUE}

  ;------------------------
  ;DLL is in use. Copy it to a temp file and Rename it on reboot.

  GetTempFileName $R0 $R5
    Call :replacedll.file_${REPLACEDLL_UNIQUE}
  Rename /REBOOTOK $R0 $R4

  ;------------------------
  ;Register the DLL on reboot

  !ifndef REPLACEDLL_NOREGISTER
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\RunOnce" \
      "Register $R4" 'rundll32.exe "$R4",DllRegisterServer'
  !endif

  Goto replacedll.done_${REPLACEDLL_UNIQUE}

  ;------------------------
  ;DLL does not exist - just extract

  replacedll.copy_${REPLACEDLL_UNIQUE}:
    StrCpy $R0 $R4
    Call :replacedll.file_${REPLACEDLL_UNIQUE}

  ;------------------------
  ;Register the DLL

  replacedll.noreboot_${REPLACEDLL_UNIQUE}:
    !ifndef REPLACEDLL_NOREGISTER
      RegDLL $R4
    !endif

  ;------------------------
  ;Done

  replacedll.done_${REPLACEDLL_UNIQUE}:

  Pop $R5
  Pop $R4
  Pop $R3
  Pop $R2
  Pop $R1
  Pop $R0

  ;------------------------
  ;End

  Goto replacedll.end_${REPLACEDLL_UNIQUE}

  ;------------------------
  ;Called to extract the DLL

  replacedll.file_${REPLACEDLL_UNIQUE}:
    File /oname=$R0 "${LOCALFILE}"
    Return

  replacedll.end_${REPLACEDLL_UNIQUE}:

 ;------------------------
 ;Restore settings

 SetOverwrite lastused
 
 !undef REPLACEDLL_UNIQUE

!macroend


;--------------------------------
;Reserve Files
  
  ;Things that need to be extracted on first (keep these lines before any File command!)
  ;Only useful for BZIP2 compression
  !insertmacro MUI_RESERVEFILE_LANGDLL
  
;--------------------------------
; User Variables

var REG_SUB_KEY
var REG_VALUE
var REG_DATA_1
var REG_DATA_2
var REG_DATA_3
var REG_DATA_4


;--------------------------------
;Installer Sections

;----------------------
; OpenAFS CLIENT
Section "!AFS Client" secClient

  SetShellVarContext all

  ; Check for bad previous installation (if we are doing a new install)
  Call IsAnyAFSInstalled
  Pop $R0
  StrCmp $R0 "0" +1 skipCheck
  Call CheckPathForAFS
  skipCheck:

  ; Stop any running services or we can't replace the files
  ; Stop the running processes
  GetTempFileName $R0
  File /oname=$R0 "${AFS_WININSTALL_DIR}\Killer.exe"   ; Might not have the MSVCR71.DLL file to run
  nsExec::Exec '$R0 afsd_service.exe'
  nsExec::Exec '$R0 afscreds.exe'
  Exec "afscreds.exe -z"
  ; in case we are upgrading an old version that does not support -z
  nsExec::Exec '$R0 afscreds.exe'
!IFDEF INSTALL_KFW
  ;nsExec::Exec '$R0 krbcc32s.exe'
!ENDIF

  nsExec::Exec "net stop TransarcAFSDaemon"
  nsExec::Exec "net stop TransarcAFSServer"
  
   ; Do client components
  SetOutPath "$INSTDIR\Client\Program"
  File "${AFS_CLIENT_BUILDDIR}\afsshare.exe"
  !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\libosi.dll" "$INSTDIR\Client\Program\libosi.dll" "$INSTDIR"
  !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\libafsconf.dll" "$INSTDIR\Client\Program\libafsconf.dll" "$INSTDIR"
  File "${AFS_CLIENT_BUILDDIR}\klog.exe"
  File "${AFS_CLIENT_BUILDDIR}\tokens.exe"
  File "${AFS_CLIENT_BUILDDIR}\unlog.exe"
  File "${AFS_CLIENT_BUILDDIR}\fs.exe"
  File "${AFS_CLIENT_BUILDDIR}\afsdacl.exe"
  File "${AFS_CLIENT_BUILDDIR}\cmdebug.exe"
  File "${AFS_CLIENT_BUILDDIR}\aklog.exe"
  File "${AFS_CLIENT_BUILDDIR}\afscreds.exe"
  !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_shl_ext.dll" "$INSTDIR\Client\Program\afs_shl_ext.dll" "$INSTDIR"
  File "${AFS_CLIENT_BUILDDIR}\afsd_service.exe"
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
  !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_cpa.cpl" "$INSTDIR\Client\Program\afs_cpa.cpl" "$INSTDIR"
  
  SetOutPath "$SYSDIR"
  !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afslogon.dll" "$SYSDIR\afslogon.dll" "$INSTDIR"
  File "${AFS_CLIENT_BUILDDIR}\afscpcc.exe"
!ifdef AFSIFS
!ifndef DEBUG
  !insertmacro ReplaceDLL "..\..\afsrdr\objfre_w2K_x86\i386\afsrdr.sys" "$SYSDIR\DRIVERS\afsrdr.sys" "$INSTDIR"
!else
  !insertmacro ReplaceDLL "..\..\afsrdr\objchk_w2K_x86\i386\afsrdr.sys" "$SYSDIR\DRIVERS\afsrdr.sys" "$INSTDIR"
!endif
!endif
   
   Call AFSLangFiles

  ; Get AFS CellServDB file
  Call afs.GetCellServDB

  GetTempFileName $R0
  File /oname=$R0 "${AFS_WININSTALL_DIR}\AdminGroup.exe"
  nsExec::Exec '$R0 -create'

!ifdef INSTALL_KFW
  ; Include Kerberos for Windows files in the installer...
  SetOutPath "$INSTDIR\kfw\bin\"
  File "${KFW_SOURCE}\bin\*"
  SetOutPath "$INSTDIR\kfw\doc"
  File "${KFW_SOURCE}\doc\*"
!endif
   
  ;Store install folder
  WriteRegStr HKCU "${AFS_REGKEY_ROOT}\Client" "" $INSTDIR
  Call AFSCommon.Install
  
  ; Write registry entries
  WriteRegStr HKCR "*\shellex\ContextMenuHandlers\AFS Client Shell Extension" "" "{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}"
  WriteRegStr HKCR "CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}" "" "AFS Client Shell Extension"
  WriteRegStr HKCR "CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}\InprocServer32" "" "$INSTDIR\Client\Program\afs_shl_ext.dll"
  WriteRegStr HKCR "CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}\InprocServer32" "ThreadingModel" "Apartment"
  WriteRegStr HKCR "FOLDER\shellex\ContextMenuHandlers\AFS Client Shell Extension" "" "{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}"
  WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" "{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}" "AFS Client Shell Extension"
  WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Control Panel\Cpls" "afs_cpa" "$INSTDIR\Client\Program\afs_cpa.cpl"

  ; Support for apps that wrote submount data directly to afsdsbmt.ini
  WriteRegStr HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\IniFileMapping\afsdsbmt.ini" "AFS Mappings" "USR:Software\OpenAFS\Client\mappings"
  WriteRegStr HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\IniFileMapping\afsdsbmt.ini" "AFS Submounts" "SYS:OpenAFS\Client\Submounts"
  
  ; AFS Reg entries
  DeleteRegKey HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "VersionString" ${AFS_VERSION}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "Title" "AFS Client"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "Description" "AFS Client"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "PathName" "$INSTDIR\Client"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "Software Type" "File System"
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "MajorVersion" ${AFS_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "MinorVersion" ${AFS_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "PatchLevel" ${AFS_PATCHLEVEL}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\${AFS_VERSION}" "VersionString" ${AFS_VERSION}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\${AFS_VERSION}" "Title" "AFS Client"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\${AFS_VERSION}" "Description" "AFS Client"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\${AFS_VERSION}" "Software Type" "File System"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Client\${AFS_VERSION}" "PathName" "$INSTDIR\Client\Program"
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Client\${AFS_VERSION}" "MajorVersion" ${AFS_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Client\${AFS_VERSION}" "MinorVersion" ${AFS_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Client\${AFS_VERSION}" "PatchLevel" ${AFS_PATCHLEVEL}
!ifdef DEBUG
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "Debug" 1
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Client\${AFS_VERSION}" "Debug" 1
!else
   ; Delete the DEBUG string
   DeleteRegValue HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "Debug"
   DeleteRegValue HKLM "${AFS_REGKEY_ROOT}\AFS Client\${AFS_VERSION}" "Debug"
!endif

   ; On Windows 2000 work around KB301673.  This is fixed in Windows XP and 2003
   Call GetWindowsVersion
   Pop $R1
   StrCmp $R1 "2000" +1 +2
   WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\NetBT\Parameters" "SmbDeviceEnabled" 0
  
  ;Write start menu entries
  CreateDirectory "$SMPROGRAMS\OpenAFS\Client"
  CreateShortCut "$SMPROGRAMS\OpenAFS\Uninstall OpenAFS.lnk" "$INSTDIR\Uninstall.exe"
  
  ; Create command line options for AFSCreds...
  StrCpy $R2 ""
  ReadINIStr $R1 $2 "Field 3" "State"
  StrCmp $R1 "1" +1 +2
  StrCpy $R2 "-A "
  ReadINIStr $R1 $2 "Field 5" "State"
  StrCmp $R1 "1" +1 +2
  StrCpy $R2 "$R2-M "
  ReadINIStr $R1 $2 "Field 7" "State"
  StrCmp $R1 "1" +1 +2
  StrCpy $R2 "$R2-N "
  ReadINIStr $R1 $2 "Field 9" "State"
  StrCmp $R1 "1" +1 +2
  StrCpy $R2 "$R2-Q "
  ReadINIStr $R1 $2 "Field 13" "State"
  StrCmp $R1 "1" +1 +2
  StrCpy $R2 "$R2-S"
 
  WriteRegStr HKLM "SOFTWARE\OpenAFS\Client" "AfscredsShortcutParams" "$R2"
  
  CreateShortCut "$SMPROGRAMS\OpenAFS\Client\Authentication.lnk" "$INSTDIR\Client\Program\afscreds.exe" "$R2"
  
  ReadINIStr $R1 $2 "Field 1" "State"
  StrCmp $R1 "1" +1 +2
  CreateShortCut "$SMSTARTUP\AFS Credentials.lnk" "$INSTDIR\Client\Program\afscreds.exe" "$R2"

  Push "$INSTDIR\Client\Program"
  Call AddToUniquePath
  Push "$INSTDIR\Common"
  Call AddToUniquePath
  
!ifdef INSTALL_KFW
  ; Add kfw to path too
  Push "$INSTDIR\kfw\bin"
  Call AddToUniquePath
!endif
   
  ; Create the AFS service
  SetOutPath "$INSTDIR\Common"
  File "${AFS_WININSTALL_DIR}\Service.exe"
  nsExec::Exec "net stop TransarcAFSDaemon"
  nsExec::Exec "net stop AfsRdr"
  ;IMPORTANT!  If we are not refreshing the config files, do NOT remove the service
  ;Don't re-install because it must be present or we wouldn't have passed the Reg check
 
  ReadRegStr $R2 HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "Cell"
  StrCmp $R2 "" +1 skipremove
  nsExec::Exec '$INSTDIR\Common\Service.exe u TransarcAFSDaemon'
  nsExec::Exec '$INSTDIR\Common\Service.exe TransarcAFSDaemon "$INSTDIR\Client\Program\afsd_service.exe" "OpenAFS Client Service"'
  nsExec::Exec '$INSTDIR\Common\Service.exe u AfsRdr'
!ifdef AFSIFS
  nsExec::Exec '$INSTDIR\Common\Service.exe AfsRdr "System32\DRIVERS\afsrdr.sys" "AFS Redirector"'
!endif
skipremove:
  Delete "$INSTDIR\Common\service.exe"

  ; Daemon entries
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon" "" ""
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\NetworkProvider" "ProviderPath" "$SYSDIR\afslogon.dll"
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\NetworkProvider" "AuthentProviderPath" "$SYSDIR\afslogon.dll"
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\NetworkProvider" "Class" 2
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\NetworkProvider" "VerboseLogging" 10

  ; Must also add HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\NetworkProvider\HwOrder
  ; and HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\NetworkProvider\Order
  ; to also include the service name.
  Call AddProvider
  ReadINIStr $R0 $1 "Field 7" "State"
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\NetworkProvider" "LogonOptions" $R0
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\NetworkProvider" "LogonScript" "$INSTDIR\Client\Program\afscreds.exe -:%s -x -a -m -n -q"
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\NetworkProvider" "Name" "OpenAFSDaemon"

  ;Write cell name
  ReadINIStr $R0 $1 "Field 2" "State"
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "Cell" $R0
  ReadINIStr $R0 $1 "Field 3" "State"
  WriteRegDWORD HKLM "SOFTWARE\OpenAFS\Client" "ShowTrayIcon" 1
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "SecurityLevel" $R0
  ReadINIStr $R0 $1 "Field 5" "State"  
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "FreelanceClient" $R0
  ReadINIStr $R0 $1 "Field 9" "State"
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "UseDNS" $R0
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "NetbiosName" "AFS"
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "MountRoot" "/afs"
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "RxMaxMTU" 1260
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "IsGateway" 0
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "HideDotFiles" 1

  ; Find Lana By Name appears to be causing grief for many people 
  ; I do not have time to track this down so I am simply going to disable it
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "NoFindLanaByName" 1

  strcpy $REG_SUB_KEY "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon" 
  strcpy $REG_VALUE   "DependOnGroup" 
  strcpy $REG_DATA_1  "PNP_TDI"
  strcpy $REG_DATA_2  ""
  strcpy $REG_DATA_3  ""
  strcpy $REG_DATA_4  ""
  Call RegWriteMultiStr
  strcpy $REG_SUB_KEY "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon" 
  strcpy $REG_VALUE   "DependOnService" 
  strcpy $REG_DATA_1  "Tcpip"
  strcpy $REG_DATA_2  "NETBIOS"
  strcpy $REG_DATA_3  "RpcSs"
!ifdef AFSIFS
  strcpy $REG_DATA_4  "AfsRdr"
!else
  strcpy $REG_DATA_4  ""
!endif
  Call RegWriteMultiStr
!ifdef AFSIFS
  strcpy $REG_SUB_KEY "SYSTEM\CurrentControlSet\Services\AfsRdr" 
  strcpy $REG_VALUE   "DependOnService" 
  strcpy $REG_DATA_1  "Tcpip"
  strcpy $REG_DATA_2  ""
  strcpy $REG_DATA_3  ""
  strcpy $REG_DATA_4  ""
  Call RegWriteMultiStr
!endif

  ; WinLogon Event Notification
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\AfsLogon" "Asynchronous" 0
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\AfsLogon" "Impersonate"  1
  WriteRegStr HKLM "Software\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\AfsLogon" "DLLName" "afslogon.dll"
  WriteRegStr HKLM "Software\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\AfsLogon" "Logon" "AFS_Logon_Event"
  WriteRegStr HKLM "Software\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\AfsLogon" "Logoff" "AFS_Logoff_Event"
  WriteRegStr HKLM "Software\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\AfsLogon" "Startup" "AFS_Startup_Event"

  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\KFWLogon" "Asynchronous" 0
  WriteRegDWORD HKLM "Software\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\KFWLogon" "Impersonate"  0
  WriteRegStr HKLM "Software\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\KFWLogon" "DLLName" "afslogon.dll"
  WriteRegStr HKLM "Software\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\KFWLogon" "Logon" "KFW_Logon_Event"

  SetRebootFlag true
  
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenAFS" "DisplayIcon" "$INSTDIR\Uninstall.exe,0"  
  Call CreateDesktopIni
  
SectionEnd



; MS Loopback adapter
Section "!MS Loopback Adapter" secLoopback

Call afs.InstallMSLoopback

SectionEnd


;------------------------
; OpenAFS SERVER  
Section /o "AFS Server" secServer

  SetShellVarContext all

  ; Check for bad previous installation (if we are doing a new install)
  Call IsAnyAFSInstalled
  Pop $R0
  StrCmp $R0 "0" +1 skipCheck
  Call CheckPathForAFS
skipCheck:

  ; Stop any running services or we can't replace the files
  ; Stop the running processes
  GetTempFileName $R0
  File /oname=$R0 "${AFS_WININSTALL_DIR}\Killer.exe"
  nsExec::Exec '$R0 afscreds.exe'
  Exec "afscreds.exe -z"
  ; in case we are upgrading an old version that does not support -z
  Sleep 2000
  nsExec::Exec '$R0 afscreds.exe'
!IFDEF INSTALL_KFW
  ;nsExec::Exec '$R0 krbcc32s.exe'
!ENDIF

  Delete $R0
  
  nsExec::Exec "net stop TransarcAFSDaemon"
  nsExec::Exec "net stop TransarcAFSServer"

  CreateDirectory "$INSTDIR\Server\usr\afs\etc"
  CreateDirectory "$INSTDIR\Server\usr\afs\local"
  CreateDirectory "$INSTDIR\Server\usr\afs\etc\logs"
  
  SetOutPath "$INSTDIR\Server\usr\afs\bin"  
  File "${AFS_SERVER_BUILDDIR}\afskill.exe"
  File "${AFS_SERVER_BUILDDIR}\afssvrcfg.exe"
  File "${AFS_SERVER_BUILDDIR}\asetkey.exe"
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
   Call AFSLangFiles
   
   SetOutPath "$SYSDIR"
  !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afsserver.cpl" "$SYSDIR\afsserver.cpl" "$INSTDIR"
   
  ;Store install folder
  WriteRegStr HKCU "${AFS_REGKEY_ROOT}\AFS Server" "" $INSTDIR
  
  DeleteRegKey HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "VersionString" ${AFS_VERSION}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "Title" "AFS Server"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "Description" "AFS Server for Windows"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "PathName" "$INSTDIR\Server"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "Software Type" "File System"
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "MajorVersion" ${AFS_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "MinorVersion" ${AFS_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "PatchLevel" ${AFS_PATCHLEVEL}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\${AFS_VERSION}" "VersionString" ${AFS_VERSION}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\${AFS_VERSION}" "Title" "AFS Server"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\${AFS_VERSION}" "Description" "AFS Server for Windows"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\${AFS_VERSION}" "Software Type" "File System"
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Server\${AFS_VERSION}" "PathName" "$INSTDIR\Server"
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Server\${AFS_VERSION}" "MajorVersion" ${AFS_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Server\${AFS_VERSION}" "MinorVersion" ${AFS_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Server\${AFS_VERSION}" "PatchLevel" ${AFS_PATCHLEVEL}
!ifdef DEBUG
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "Debug" 1
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Server\${AFS_VERSION}" "Debug" 1
!else
   ; Delete the DEBUG string
   DeleteRegValue HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "Debug"
   DeleteRegValue HKLM "${AFS_REGKEY_ROOT}\AFS Server\${AFS_VERSION}" "Debug"
!endif
  ; Install the service
  SetOutPath "$INSTDIR\Common"
  File "${AFS_WININSTALL_DIR}\Service.exe"
!ifdef DEBUG
  File "${AFS_WININSTALL_DIR}\Service.pdb"
!endif

  ; Check if the service exists--if it does, this is an upgrade/re-install
  ReadRegStr $R0 HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSServer" "ImagePath"
  StrCmp $R0 "$INSTDIR\Server\usr\afs\bin\bosctlsvc.exe" SkipStartup
  
  ; If an uninstall was done, but we kept the config files, also skip
  IfFileExists "$INSTDIR\Server\usr\afs\etc\ThisCell" SkipStartup

  ; Make the server config wizard auto-start on bootup if this is an install (not an upgrade)
  WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\RunOnce" "AFS Server Wizard" '"$INSTDIR\Server\usr\afs\bin\afssvrcfg.exe" /wizard"'
  
  
SkipStartup:
  ;Don't want to whack existing settings... Make users un-install and then re-install if they want that
  ;nsExec::Exec '$INSTDIR\Common\service.exe u TransarcAFSServer'
  nsExec::Exec '$INSTDIR\Common\service.exe TransarcAFSServer "$INSTDIR\Server\usr\afs\bin\bosctlsvc.exe" "OpenAFS AFS Server"'
  Delete "$INSTDIR\Common\service.exe"
  
  CreateDirectory "$SMPROGRAMS\OpenAFS\Server"
  CreateShortCut "$SMPROGRAMS\OpenAFS\Server\Server Configuration.lnk" "$INSTDIR\Server\usr\afs\bin\afssvrcfg.exe"
  
  
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenAFS" "DisplayIcon" "$INSTDIR\Uninstall.exe,0"  

SectionEnd


;----------------------------
; OpenAFS Control Center
Section /o "AFS Control Center" secControl

  SetShellVarContext all

   SetOutPath "$INSTDIR\Control Center"
  File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager.exe"
  File "${AFS_SERVER_BUILDDIR}\TaAfsAdmSvr.exe"
  File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager.exe"
   

 ;AFS Server common files
 Call AFSCommon.Install
 Call AFSLangFiles
 SetOutPath "$INSTDIR\Common"

   ;Store install folder
  WriteRegStr HKCU "${AFS_REGKEY_ROOT}\AFS Control Center\CurrentVersion" "PathName" $INSTDIR
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\CurrentVersion" "VersionString" ${AFS_VERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\CurrentVersion" "MajorVersion" ${AFS_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\CurrentVersion" "MinorVersion" ${AFS_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\CurrentVersion" "PatchLevel" ${AFS_PATCHLEVEL}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\${AFS_VERSION}" "VersionString" ${AFS_VERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\${AFS_VERSION}" "MajorVersion" ${AFS_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\${AFS_VERSION}" "MinorVersion" ${AFS_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\${AFS_VERSION}" "PatchLevel" ${AFS_PATCHLEVEL}
!ifdef DEBUG
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\CurrentVersion" "Debug" 1
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\${AFS_VERSION}" "Debug" 1
!else
   ; Delete the DEBUG string
   DeleteRegValue HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\CurrentVersion" "Debug"
   DeleteRegValue HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\${AFS_VERSION}" "Debug"
!endif

  ;Write start menu entries
  CreateDirectory "$SMPROGRAMS\OpenAFS\Control Center"
  CreateShortCut "$SMPROGRAMS\OpenAFS\Control Center\Account Manager.lnk" "$INSTDIR\Control Center\TaAfsAccountManager.exe"
  CreateShortCut "$SMPROGRAMS\OpenAFS\Control Center\Server Manager.lnk" "$INSTDIR\Control Center\TaAfsServerManager.exe"
  
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenAFS" "DisplayIcon" "$INSTDIR\Uninstall.exe,0"  

SectionEnd   


;----------------------------
; OpenAFS Supplemental Documentation
Section /o "Supplemental Documentation" secDocs
  SetShellVarContext all

   StrCmp $LANGUAGE ${LANG_ENGLISH} DoEnglish
   StrCmp $LANGUAGE ${LANG_GERMAN} DoGerman
   StrCmp $LANGUAGE ${LANG_SPANISH} DoSpanish
   StrCmp $LANGUAGE ${LANG_JAPANESE} DoJapanese
   StrCmp $LANGUAGE ${LANG_KOREAN} DoKorean
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
   SetOutPath "$INSTDIR\Documentation\html\CmdRef"
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
   SetOutPath "$INSTDIR\Documentation\html\CmdRef"
   ;File "..\..\doc\install\Documentation\es_ES\html\CmdRef\*"
   SetOutPath "$INSTDIR\Documentation\html\InstallGd"
   ;File "..\..\doc\install\Documentation\es_ES\html\InstallGd\*"
   SetOutPath "$INSTDIR\Documentation\html\ReleaseNotes"
   ;File "..\..\doc\install\Documentation\es_ES\html\ReleaseNotes\*"
   SetOutPath "$INSTDIR\Documentation\html\SysAdminGd"
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
   ;File "..\..\doc\install\Documentation\ja_JP\html\ReleaseNotes\*"
   SetOutPath "$INSTDIR\Documentation\html\SysAdminGd"
   ;File "..\..\doc\install\Documentation\ja_JP\html\SysAdminGd\*"
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
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\CurrentVersion" "VersionString" ${AFS_VERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\CurrentVersion" "MajorVersion" ${AFS_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\CurrentVersion" "MinorVersion" ${AFS_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\CurrentVersion" "PatchLevel" ${AFS_PATCHLEVEL}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\${AFS_VERSION}" "VersionString" ${AFS_VERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\${AFS_VERSION}" "MajorVersion" ${AFS_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\${AFS_VERSION}" "MinorVersion" ${AFS_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS Supplemental Documentation\${AFS_VERSION}" "PatchLevel" ${AFS_PATCHLEVEL}

  ; Write start menu shortcut
  SetOutPath "$SMPROGRAMS\OpenAFS"
  CreateShortCut "$SMPROGRAMS\OpenAFS\Documentation.lnk" "$INSTDIR\Documentation\html\index.htm"
  
  
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenAFS" "DisplayIcon" "$INSTDIR\Uninstall.exe,0"  
  CreateShortCut "$SMPROGRAMS\OpenAFS\Uninstall OpenAFS.lnk" "$INSTDIR\Uninstall.exe"
  Call AFSCommon.Install
SectionEnd  
  

Section /o "Software Development Kit (SDK)" secSDK

    SetOutPath "$INSTDIR\Client\Program\lib"
    File /r "${AFS_CLIENT_LIBDIR}\*.*"

    SetOutPath "$INSTDIR\Client\Program\Include"
    File /r "${AFS_BUILD_INCDIR}\*.*"    

   ; Client Sample
   SetOutPath "$INSTDIR\Client\Program\Sample"
   File "..\..\afsd\sample\token.c"

   ;Store install folder
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS SDK" "" $INSTDIR
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS SDK\CurrentVersion" "VersionString" ${AFS_VERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS SDK\CurrentVersion" "MajorVersion" ${AFS_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS SDK\CurrentVersion" "MinorVersion" ${AFS_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS SDK\CurrentVersion" "PatchLevel" ${AFS_PATCHLEVEL}
  WriteRegStr HKLM "${AFS_REGKEY_ROOT}\AFS SDK\${AFS_VERSION}" "VersionString" ${AFS_VERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS SDK\${AFS_VERSION}" "MajorVersion" ${AFS_MAJORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS SDK\${AFS_VERSION}" "MinorVersion" ${AFS_MINORVERSION}
  WriteRegDWORD HKLM "${AFS_REGKEY_ROOT}\AFS SDK\${AFS_VERSION}" "PatchLevel" ${AFS_PATCHLEVEL}

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenAFS" "DisplayIcon" "$INSTDIR\Uninstall.exe,0"  

  SetOutPath "$SMPROGRAMS\OpenAFS"
  CreateShortCut "$SMPROGRAMS\OpenAFS\Uninstall OpenAFS.lnk" "$INSTDIR\Uninstall.exe"

  Call AFSCommon.Install
SectionEnd


Section /o "Debug symbols" secDebug
   SectionGetFlags ${secClient} $R0
   IntOp $R0 $R0 & ${SF_SELECTED}
   IntCmp $R0 ${SF_SELECTED} +1 DoServer
  
  ; Do client components
  SetOutPath "$INSTDIR\Client\Program"
  File "${AFS_CLIENT_BUILDDIR}\afsshare.pdb"
  File "${AFS_CLIENT_BUILDDIR}\libosi.pdb"
  File "${AFS_CLIENT_BUILDDIR}\libafsconf.pdb"
  File "${AFS_CLIENT_BUILDDIR}\klog.pdb"
  File "${AFS_CLIENT_BUILDDIR}\tokens.pdb"
  File "${AFS_CLIENT_BUILDDIR}\unlog.pdb"
  File "${AFS_CLIENT_BUILDDIR}\fs.pdb"
  File "${AFS_CLIENT_BUILDDIR}\afsdacl.pdb"
  File "${AFS_CLIENT_BUILDDIR}\cmdebug.pdb"
  File "${AFS_CLIENT_BUILDDIR}\aklog.pdb"
  File "${AFS_CLIENT_BUILDDIR}\afscreds.pdb"
  File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext.pdb"
  File "${AFS_CLIENT_BUILDDIR}\afsd_service.pdb"
  File "${AFS_CLIENT_BUILDDIR}\symlink.pdb"
  File "${AFS_DESTDIR}\bin\kpasswd.pdb"
  File "${AFS_DESTDIR}\bin\pts.pdb"
  File "${AFS_SERVER_BUILDDIR}\bos.pdb"
  File "${AFS_SERVER_BUILDDIR}\kas.pdb"
  File "${AFS_SERVER_BUILDDIR}\vos.pdb"
  File "${AFS_SERVER_BUILDDIR}\udebug.pdb"
  File "${AFS_DESTDIR}\bin\translate_et.pdb"
  File "${AFS_DESTDIR}\etc\rxdebug.pdb"
  File "${AFS_DESTDIR}\etc\backup.pdb"
  File "${AFS_CLIENT_BUILDDIR}\afs_cpa.pdb"

  SetOutPath "$SYSDIR"
  File "${AFS_CLIENT_BUILDDIR}\afslogon.pdb"
  
DoServer:
   SectionGetFlags ${secServer} $R0
   IntOp $R0 $R0 & ${SF_SELECTED}
   IntCmp $R0 ${SF_SELECTED} +1 DoControl

  ; Do server components
  SetOutPath "$INSTDIR\Server\usr\afs\bin"  
  File "${AFS_SERVER_BUILDDIR}\afskill.pdb"
  File "${AFS_SERVER_BUILDDIR}\afssvrcfg.pdb"
  File "${AFS_SERVER_BUILDDIR}\asetkey.pdb"
  File "${AFS_SERVER_BUILDDIR}\bosctlsvc.pdb"
  File "${AFS_SERVER_BUILDDIR}\bosserver.pdb"
  File "${AFS_SERVER_BUILDDIR}\buserver.pdb"
  File "${AFS_ETC_BUILDDIR}\butc.pdb"
  File "${AFS_SERVER_BUILDDIR}\fileserver.pdb"
  File "${AFS_ETC_BUILDDIR}\fms.pdb"
  File "${AFS_SERVER_BUILDDIR}\kaserver.pdb"
  File "${AFS_SERVER_BUILDDIR}\ptserver.pdb"
  File "${AFS_SERVER_BUILDDIR}\salvager.pdb"
  File "${AFS_SERVER_BUILDDIR}\upclient.pdb"
  File "${AFS_SERVER_BUILDDIR}\upserver.pdb"
  File "${AFS_SERVER_BUILDDIR}\vlserver.pdb"
  File "${AFS_SERVER_BUILDDIR}\volinfo.pdb"
  File "${AFS_SERVER_BUILDDIR}\volserver.pdb"

  ; Do server common components
 File "${AFS_SERVER_BUILDDIR}\afsvosadmin.pdb"
 File "${AFS_SERVER_BUILDDIR}\afsbosadmin.pdb"
 File "${AFS_SERVER_BUILDDIR}\afscfgadmin.pdb"
 File "${AFS_SERVER_BUILDDIR}\afskasadmin.pdb"
 File "${AFS_SERVER_BUILDDIR}\afsptsadmin.pdb"
 
   SetOutPath "$SYSDIR"
   File "${AFS_SERVER_BUILDDIR}\afsserver.pdb"

   ; Do control center components
DoControl:
   SectionGetFlags ${secControl} $R0
   IntOp $R0 $R0 & ${SF_SELECTED}
   IntCmp $R0 ${SF_SELECTED} +1 DoCommon

   SetOutPath "$INSTDIR\Control Center"   
  File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager.pdb"
  File "${AFS_SERVER_BUILDDIR}\TaAfsAdmSvr.pdb"
  File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager.pdb"

DoCommon:
  SetOutPath "$INSTDIR\Common"
!IFDEF CL_1400
   File "${SYSTEMDIR}\msvcr80d.pdb"
   File "${SYSTEMDIR}\msvcp80d.pdb"
   File "${SYSTEMDIR}\mfc80d.pdb"
!ELSE
!IFDEF CL_1310
   File "${SYSTEMDIR}\msvcr71d.pdb"
   File "${SYSTEMDIR}\msvcp71d.pdb"
   File "${SYSTEMDIR}\mfc71d.pdb"
!ELSE
!IFDEF CL_1300
   File "${SYSTEMDIR}\msvcr70d.pdb"
   File "${SYSTEMDIR}\msvcp70d.pdb"
   File "${SYSTEMDIR}\mfc70d.pdb"
!ELSE
   File "${SYSTEMDIR}\mfc42d.pdb"
   File "${SYSTEMDIR}\msvcp60d.pdb"
   File "${SYSTEMDIR}\msvcrtd.pdb"
!ENDIF
!ENDIF
!ENDIF
  
; Common Areas
   SetOutPath "$INSTDIR\Common"
   File "${AFS_CLIENT_BUILDDIR}\afs_config.pdb"
   File "${AFS_SERVER_BUILDDIR}\afsadminutil.pdb"
   File "${AFS_DESTDIR}\lib\afsauthent.pdb"
   File "${AFS_DESTDIR}\lib\afspthread.pdb"
   File "${AFS_DESTDIR}\lib\afsrpc.pdb"
   File "${AFS_SERVER_BUILDDIR}\afsclientadmin.pdb"
   File "${AFS_SERVER_BUILDDIR}\afsprocmgmt.pdb"
   File "${AFS_SERVER_BUILDDIR}\afsvosadmin.pdb"
   File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib.pdb"
   File "${AFS_SERVER_BUILDDIR}\afsvosadmin.pdb"
   File "${AFS_SERVER_BUILDDIR}\afsbosadmin.pdb"
   File "${AFS_SERVER_BUILDDIR}\afscfgadmin.pdb"
   File "${AFS_SERVER_BUILDDIR}\afskasadmin.pdb"
   File "${AFS_SERVER_BUILDDIR}\afsptsadmin.pdb"

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

   Call IsUserAdmin
   Pop $R0
   StrCmp $R0 "true" contInstall

   MessageBox MB_OK|MB_ICONSTOP|MB_TOPMOST "You must be an administrator of this machine to install this software."
   Abort
   
contInstall:

   ; Set Install Type text
   InstTypeSetText 0 "AFS Client"
   InstTypeSetText 1 "AFS Administrator"
   InstTypeSetText 2 "AFS Server"
   InstTypeSetText 3 "AFS Developer Tools"

   ; Set sections in each install type
   SectionSetInstTypes 0 15		; AFS Client
   SectionSetInstTypes 1 15		; Loopback adapter
   SectionSetInstTypes 2 4              ; AFS Server
   SectionSetInstTypes 3 6              ; AFS Control Center
   SectionSetInstTypes 4 14		; Documentation
   SectionSetInstTypes 5 8		; SDK
!ifndef DEBUG
   SectionSetInstTypes 6 8		; Debug symbols
!else
   SectionSetInstTypes 6 15		; Debug symbols
!endif

   ; Check that RPC functions are installed (I believe any one of these can be present for
   ; OpenAFS to work)
   ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\RPC\ClientProtocols" "ncacn_np"
   StrCmp $R0 "rpcrt4.dll" contInstall2
   ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\RPC\ClientProtocols" "ncacn_ip_tcp"
   StrCmp $R0 "rpcrt4.dll" contInstall2
   ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\RPC\ClientProtocols" "ncadg_ip_udp"
   StrCmp $R0 "rpcrt4.dll" contInstall2
   ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\RPC\ClientProtocols" "ncacn_http"
   StrCmp $R0 "rpcrt4.dll" contInstall2
   
   MessageBox MB_OK|MB_ICONSTOP|MB_TOPMOST "An error was detected with your Windows RPC installation. Please make sure Windows RPC is installed before installing OpenAFS."
   Abort


contInstall2:
   ; If the Loopback is already installed, we mark the option OFF and Read Only
   ; so the user can not select it.
   Call afs.isLoopbackInstalled
   IntCmp $R1 0 SkipLoop
   SectionGetFlags ${secLoopback} $0
   IntOp $0 $0 & ${SECTION_OFF}
   IntOp $0 $0 | ${SF_RO}
   SectionSetFlags ${secLoopback} $0
   ; And disable the loopback in the types
   SectionSetInstTypes 1 0		; Loopback adapter
   
SkipLoop:
   ; Never install debug symbols unless explicitly selected, except in DEBUG mode
	!IFNDEF DEBUG
   SectionGetFlags ${secDebug} $0
	IntOp $0 $0 & ${SECTION_OFF}
	SectionSetFlags ${secDebug} $0
   !ELSE
   SectionGetFlags ${secDebug} $0
	IntOp $0 $0 | ${SF_SELECTED}
	SectionSetFlags ${secDebug} $0
   !ENDIF
   ; Our logic should be like this.
   ;     1) If no AFS components are installed, we do a clean install with default options. (Client/Docs)
   ;     2) If existing modules are installed, we keep them selected
   ;     3) If it is an upgrade, we set the text accordingly, else we mark it as a re-install
   ;  TODO: Downgrade?
   Call IsAnyAFSInstalled
   Pop $R0
   StrCmp $R0 "0" DefaultOptions
   
   Call ShouldClientInstall
   Pop $R2
   
   ; Check if it was an IBM/Transarc version
   ReadRegStr $R0 HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon" "DisplayName"
   StrCmp $R0 "IBM AFS Client" DoIBM
   StrCmp $R0 "Transarc AFS Client" DoIBM
NotIBM:
   StrCpy $R9 ""
   StrCmp $R2 "0" NoClient
   StrCmp $R2 "1" ReinstallClient
   StrCmp $R2 "2" UpgradeClient
   StrCmp $R2 "3" DowngradeClient
   goto Continue
DoIBM:
   ReadRegDWORD $R0 HKLM "Software\TransarcCorporation\AFS Client\CurrentVersion" "MajorVersion"
   StrCmp $R0 "3" +1 NotIBM
   StrCpy $R9 "IBM"
   goto UpgradeClient

Continue:
	SectionGetFlags ${secClient} $0
	IntOp $0 $0 | ${SF_SELECTED}
	SectionSetFlags ${secClient} $0
    ;# !insertmacro SelectSection ${secClient}
   goto skipClient
NoClient:
	;StrCpy $1 ${secClient} ; Gotta remember which section we are at now...
	SectionGetFlags ${secClient} $0
	IntOp $0 $0 & ${SECTION_OFF}
	SectionSetFlags ${secClient} $0
   goto skipClient
UpgradeClient:
	SectionGetFlags ${secClient} $0
	IntOp $0 $0 | ${SF_SELECTED}
	SectionSetFlags ${secClient} $0
   SectionSetText ${secClient} $(UPGRADE_CLIENT)
   goto skipClient
ReinstallClient:
	SectionGetFlags ${secClient} $0
	IntOp $0 $0 | ${SF_SELECTED}
	SectionSetFlags ${secClient} $0
   SectionSetText ${secClient} $(REINSTALL_CLIENT)
   goto skipClient
DowngradeClient:
	SectionGetFlags ${secClient} $0
	IntOp $0 $0 | ${SF_SELECTED}
	SectionSetFlags ${secClient} $0
   SectionSetText ${secClient} $(REINSTALL_CLIENT)
   goto skipClient

   
skipClient:   
   
   Call ShouldServerInstall
   Pop $R2
   StrCmp $R2 "0" NoServer
   StrCmp $R2 "1" ReinstallServer
   StrCmp $R2 "2" UpgradeServer
   StrCmp $R2 "3" DowngradeServer
   
   SectionGetFlags ${secServer} $0
   IntOp $0 $0 | ${SF_SELECTED}
   SectionSetFlags ${secServer} $0
   ;# !insertmacro UnselectSection ${secServer}
   goto skipServer

UpgradeServer:
   SectionGetFlags ${secServer} $0
   IntOp $0 $0 | ${SF_SELECTED}
   SectionSetFlags ${secServer} $0
   SectionSetText ${secServer} $(UPGRADE_SERVER)
   goto skipServer

ReinstallServer:
   SectionGetFlags ${secServer} $0
   IntOp $0 $0 | ${SF_SELECTED}
   SectionSetFlags ${secServer} $0
   SectionSetText ${secServer} $(REINSTALL_SERVER)
   goto skipServer

DowngradeServer:
   SectionGetFlags ${secServer} $0
   IntOp $0 $0 | ${SF_SELECTED}
   SectionSetFlags ${secServer} $0
   SectionSetText ${secServer} $(REINSTALL_SERVER)
   goto skipServer
   
NoServer:
   SectionGetFlags ${secServer} $0
   IntOp $0 $0 & ${SECTION_OFF}
   SectionSetFlags ${secServer} $0
   ;# !insertmacro UnselectSection ${secServer}
   goto skipServer
   
skipServer:
   ; Check control center
   Call IsControlInstalled
   Pop $R2
   StrCmp $R2 "0" NoControl

   SectionGetFlags ${secControl} $0
   IntOp $0 $0 | ${SF_SELECTED}
   SectionSetFlags ${secControl} $0
   goto CheckDocs
   
NoControl:   
   SectionGetFlags ${secControl} $0
   IntOp $0 $0 & ${SECTION_OFF}
   SectionSetFlags ${secControl} $0
   ;# !insertmacro UnselectSection ${secControl}

CheckDocs:
   ; Check Documentation
   Call IsDocumentationInstalled
   Pop $R2
   StrCmp $R2 "0" NoDocs
   SectionGetFlags ${secDocs} $0
   IntOp $0 $0 | ${SF_SELECTED}
   SectionSetFlags ${secDocs} $0
   goto CheckSDK
   
NoDocs:
   SectionGetFlags ${secDocs} $0
   IntOp $0 $0 & ${SECTION_OFF}
   SectionSetFlags ${secDocs} $0
   goto CheckSDK
   
; To check the SDK, we simply look to see if the files exist.  If they do,
; the SDK is installed.  If not, we don't need to push it on the user.
; If they are there, we want to make sure they match the installed version.
CheckSDK:
   IfFileExists "$INSTDIR\Client\Program\Include\main.h" +1 NoSDK
   SectionGetFlags ${secSDK} $0
   IntOp $0 $0 | ${SF_SELECTED}
   SectionSetFlags ${secSDK} $0
   goto end
   
NoSDK:
   SectionGetFlags ${secSDK} $0
   IntOp $0 $0 & ${SECTION_OFF}
   SectionSetFlags ${secSDK} $0
   goto end
   
DefaultOptions:
   ; Client Selected
   SectionGetFlags ${secClient} $0
   IntOp $0 $0 | ${SF_SELECTED}
   SectionSetFlags ${secClient} $0

   ; Server NOT selected
   SectionGetFlags ${secServer} $0
   IntOp $0 $0 & ${SECTION_OFF}
   SectionSetFlags ${secServer} $0
   
   ; Control Center NOT selected
   SectionGetFlags ${secControl} $0
   IntOp $0 $0 & ${SECTION_OFF}
   SectionSetFlags ${secControl} $0
   ;# !insertmacro UnselectSection ${secControl}

   ; Documentation NOT selected
   SectionGetFlags ${secDocs} $0
   IntOp $0 $0 & ${SECTION_OFF}
   SectionSetFlags ${secDocs} $0
   ;# !insertmacro UnselectSection ${secDocs}
   
   ; SDK not selected
   SectionGetFlags ${secSDK} $0
   IntOp $0 $0 & ${SECTION_OFF}
   SectionSetFlags ${secSDK} $0
   ;# !insertmacro UnselectSection ${secSDK}
   
   goto end

end:
   Pop $0
  
   Push $R0
  
  ; See if we can set a default installation path...
  ReadRegStr $R0 HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "PathName"
  StrCmp $R0 "" TryServer
  Push $R0
  Call GetParent
  
  ; Work around bug in 1.3.5000, 1.3.5100, 1.3.5200, 1.3.5201, 1.3.5299 installers...
  ReadRegStr $R0 HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "MajorVersion"
  StrCmp $R0 "1" +1 SkipParent
  ReadRegStr $R0 HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "MinorVersion"
  StrCmp $R0 "3" +1 SkipParent
  ReadRegStr $R0 HKLM "${AFS_REGKEY_ROOT}\AFS Client\CurrentVersion" "PatchLevel"
  StrCmp $R0 "5000" UpParent
  StrCmp $R0 "5100" UpParent
  StrCmp $R0 "5200" UpParent
  StrCmp $R0 "5201" UpParent
  StrCmp $R0 "5299" UpParent
  goto SkipParent
  
UpParent:
   Call GetParent
  
SkipParent:
  Pop $R0
  StrCpy $INSTDIR $R0
  goto Nope
  
TryServer:
  ReadRegStr $R0 HKLM "${AFS_REGKEY_ROOT}\AFS Server\CurrentVersion" "PathName"
  StrCmp $R0 "" TryControl
  Push $R0
  Call GetParent
  Pop $R0
  StrCpy $INSTDIR $R0
  goto Nope
   
TryControl:
  ReadRegStr $R0 HKLM "${AFS_REGKEY_ROOT}\AFS Control Center\CurrentVersion" "PathName"
  StrCmp $R0 "" Nope
  StrCpy $INSTDIR $R0
  
Nope:
  Pop $R0
  
  GetTempFilename $0
  File /oname=$0 CellServPage.ini
  GetTempFilename $1
  File /oname=$1 AFSCell.ini
  GetTempFilename $2
  File /oname=$2 AFSCreds.ini
  ;File /oname=$1 ConfigURL.ini
  
FunctionEnd


;--------------------------------
; These are our cleanup functions
Function .onInstFailed
Delete $0
Delete $1
FunctionEnd

Function .onInstSuccess
Delete $0
Delete $1
FunctionEnd


;--------------------------------
;Descriptions

  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${secServer} $(DESC_secServer)
  !insertmacro MUI_DESCRIPTION_TEXT ${secClient} $(DESC_secClient)
  !insertmacro MUI_DESCRIPTION_TEXT ${secControl} $(DESC_secControl)
  !insertmacro MUI_DESCRIPTION_TEXT ${secDocs} $(DESC_secDocs)
  !insertmacro MUI_DESCRIPTION_TEXT ${secSDK} $(DESC_secSDK)
  !insertmacro MUI_DESCRIPTION_TEXT ${secLoopback} $(DESC_secLoopback)
  !insertmacro MUI_DESCRIPTION_TEXT ${secDebug} $(DESC_secDebug)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END
 
;--------------------------------
;Uninstaller Section

Section "Uninstall"
  ; Make sure the user REALLY wants to do this, unless they did a silent uninstall, in which case...let them!
  IfSilent StartRemove     ; New in v2.0b4
  MessageBox MB_YESNO "Are you sure you want to remove OpenAFS from this machine?" IDYES StartRemove
  abort
  
StartRemove:
  
  SetShellVarContext all
  ; Stop the running processes
  GetTempFileName $R0
  File /oname=$R0 "${AFS_WININSTALL_DIR}\Killer.exe"
  nsExec::Exec '$R0 afscreds.exe'
  Exec "afscreds.exe -z"
  ; in case we are upgrading an old version that does not support -z
  Sleep 2000
  nsExec::Exec '$R0 afscreds.exe'
!IFDEF INSTALL_KFW
  nsExec::Exec '$R0 krbcc32s.exe'
!ENDIF

  ; Delete the AFS service
  GetTempFileName $R0
  File /oname=$R0 "${AFS_WININSTALL_DIR}\Service.exe"
  nsExec::Exec "net stop TransarcAFSDaemon"
  nsExec::Exec "net stop TransarcAFSServer"
  nsExec::Exec '$R0 u TransarcAFSDaemon'
  ; After we stop the service, but before we delete it, we have to remove the volume data
  ; This is because the storage locations are in the registry under the service key.
  ; Call un.RemoveAFSVolumes
  nsExec::Exec '$R0 u TransarcAFSServer'
  Delete $R0
  
  Call un.RemoveProvider

  Push "$INSTDIR\Client\Program"
  Call un.RemoveFromPath
  Push "$INSTDIR\Common"
  Call un.RemoveFromPath
!ifdef INSTALL_KFW
  Push "$INSTDIR\kfw\bin"
  Call un.RemoveFromPath
!endif
  
  ; Delete documentation
  Delete "$INSTDIR\Documentation\README.TXT"
  Delete "$INSTDIR\Documentation\html\*"
  Delete "$INSTDIR\Documentation\html\CmdRef\*"
  Delete "$INSTDIR\Documentation\html\InstallGd\*"
  Delete "$INSTDIR\Documentation\html\ReleaseNotes\*"
  Delete "$INSTDIR\Documentation\html\SysAdminGd\*"

   Delete /REBOOTOK "$INSTDIR\Common\afs_config.exe"
   Delete /REBOOTOK "$INSTDIR\Common\afs_shl_ext.dll"
   Delete /REBOOTOK "$INSTDIR\Common\afsadminutil.dll"
   Delete /REBOOTOK "$INSTDIR\Common\lib\afsauthent.dll"
   Delete /REBOOTOK "$INSTDIR\Common\lib\afspthread.dll"
   Delete /REBOOTOK "$INSTDIR\Common\lib\afsrpc.dll"
   Delete /REBOOTOK "$INSTDIR\Common\afsclientadmin.dll"
   Delete /REBOOTOK "$INSTDIR\Common\afsprocmgmt.dll"
   Delete /REBOOTOK "$INSTDIR\Common\afsvosadmin.dll"
   Delete /REBOOTOK "$INSTDIR\Common\TaAfsAppLib.dll"
   Delete /REBOOTOK "$INSTDIR\Common\afsvosadmin.dll"
   Delete /REBOOTOK "$INSTDIR\Common\afsbosadmin.dll"
   Delete /REBOOTOK "$INSTDIR\Common\afscfgadmin.dll"
   Delete /REBOOTOK "$INSTDIR\Common\afskasadmin.dll"
   Delete /REBOOTOK "$INSTDIR\Common\afsptsadmin.dll"

   Delete /REBOOTOK "$INSTDIR\Common\afs_config.pdb"
   Delete /REBOOTOK "$INSTDIR\Common\afs_shl_ext.pdb"
   Delete /REBOOTOK "$INSTDIR\Common\afsadminutil.pdb"
   Delete /REBOOTOK "$INSTDIR\Common\lib\afsauthent.pdb"
   Delete /REBOOTOK "$INSTDIR\Common\lib\afspthread.pdb"
   Delete /REBOOTOK "$INSTDIR\Common\lib\afsrpc.pdb"
   Delete /REBOOTOK "$INSTDIR\Common\afsclientadmin.pdb"
   Delete /REBOOTOK "$INSTDIR\Common\afsprocmgmt.pdb"
   Delete /REBOOTOK "$INSTDIR\Common\afsvosadmin.pdb"
   Delete /REBOOTOK "$INSTDIR\Common\TaAfsAppLib.pdb"
   Delete /REBOOTOK "$INSTDIR\Common\afsvosadmin.pdb"
   Delete /REBOOTOK "$INSTDIR\Common\afsbosadmin.pdb"
   Delete /REBOOTOK "$INSTDIR\Common\afscfgadmin.pdb"
   Delete /REBOOTOK "$INSTDIR\Common\afskasadmin.pdb"
   Delete /REBOOTOK "$INSTDIR\Common\afsptsadmin.pdb"
!IFDEF DEBUG
!IFDEF CL_1400
   Delete /REBOOTOK "$INSTDIR\bin\msvcr80d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr80d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp80d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp80d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\mfc80d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\mfc80d.pdb"
!ELSE
!IFDEF CL_1310
   Delete /REBOOTOK "$INSTDIR\bin\msvcr71d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr71d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp71d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp71d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\mfc71d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\mfc71d.pdb"
!ELSE
!IFDEF CL_1300
   Delete /REBOOTOK "$INSTDIR\bin\msvcr70d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr70d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp70d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp70d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\mfc70d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\mfc70d.pdb"
!ELSE
   Delete /REBOOTOK "$INSTDIR\bin\mfc42d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\mfc42d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp60d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp60d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\msvcrtd.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcrtd.pdb"
!ENDIF
!ENDIF
!ENDIF
!ELSE
!IFDEF CL_1400
   Delete /REBOOTOK "$INSTDIR\bin\mfc80.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr80.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp80.dll"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80CHS.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80CHT.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80DEU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80ENU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80ESP.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80FRA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80ITA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80JPN.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80KOR.DLL"
!ELSE
!IFDEF CL_1310
   Delete /REBOOTOK "$INSTDIR\bin\mfc71.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr71.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp71.dll"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71CHS.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71CHT.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71DEU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71ENU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71ESP.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71FRA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71ITA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71JPN.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71KOR.DLL"
!ELSE
!IFDEF CL_1300
   Delete /REBOOTOK "$INSTDIR\bin\mfc70.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr70.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp70.dll"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70CHS.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70CHT.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70DEU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70ENU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70ESP.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70FRA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70ITA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70JPN.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70KOR.DLL"
!ELSE
   Delete /REBOOTOK "$INSTDIR\bin\mfc42.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp60.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcrt.dll"
!ENDIF
!ENDIF
!ENDIF
!ENDIF
  
   IfSilent SkipDel
;  IfFileExists "$INSTDIR\Client\CellServDB" CellExists SkipDelAsk
;  CellExists:
  MessageBox MB_YESNO "Would you like to keep your configuration information?" IDYES SkipDel
  Delete "$INSTDIR\Client\CellServDB"

; Only remove krb5.ini if KfW was installed
!IFDEF INSTALL_KFW
  Delete "$WINDIR\krb5.ini"
!ENDIF
  Delete "$INSTDIR\Client\afsdns.ini"
  
  GetTempFileName $R0
  File /oname=$R0 "${AFS_WININSTALL_DIR}\AdminGroup.exe"
  nsExec::Exec '$R0 -remove'

  SkipDel:
  Delete "$WINDIR\afsd_init.log"
  Delete "$INSTDIR\Uninstall.exe"

  ; Remove server
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\afskill.exe"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\afssvrcfg.exe"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\bosctlsvc.exe"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\bosserver.exe"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\buserver.exe"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\butc.exe"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\fileserver.exe"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\fms.exe"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\kaserver.exe"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\ptserver.exe"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\salvager.exe"
  Delete "$INSTDIR\Server\usr\afs\bin\ServerUninst.dll"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\upclient.exe"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\upserver.exe"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\vlserver.exe"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\volinfo.exe"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\volserver.exe"

  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\afskill.pdb"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\afssvrcfg.pdb"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\bosctlsvc.pdb"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\bosserver.pdb"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\buserver.pdb"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\butc.pdb"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\fileserver.pdb"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\fms.pdb"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\kaserver.pdb"
  Delete /REBOOTOK "$INSTDIR\Server\usr\afs\bin\ptserver.pdb"
  Delete "$INSTDIR\Server\usr\afs\bin\salvager.pdb"
  Delete "$INSTDIR\Server\usr\afs\bin\ServerUninst.pdb"
  Delete "$INSTDIR\Server\usr\afs\bin\upclient.pdb"
  Delete "$INSTDIR\Server\usr\afs\bin\upserver.pdb"
  Delete "$INSTDIR\Server\usr\afs\bin\vlserver.pdb"
  Delete "$INSTDIR\Server\usr\afs\bin\volinfo.pdb"
  Delete "$INSTDIR\Server\usr\afs\bin\volserver.pdb"

  RMDir /r "$INSTDIR\Server\usr\afs\bin"
  ; do not delete the server configuration files
  ; or we will lose the volumes and authentication
  ; databases
  ;RmDir /r "$INSTDIR\Server\usr\afs\etc\logs"
  ;RmDir /r "$INSTDIR\Server\usr\afs\etc"
  ;RmDir /r "$INSTDIR\Server\usr\afs\local"
  ;RMDIR /r "$INSTDIR\Server\usr\afs\logs"
  
  Delete /REBOOTOK "$SYSDIR\afsserver.cpl"
  Delete /REBOOTOK "$SYSDIR\afs_cpa.cpl"
  Delete /REBOOTOK "$SYSDIR\afslogon.dll"
  Delete /REBOOTOK "$SYSDIR\afscpcc.exe"

  Delete /REBOOTOK "$SYSDIR\afsserver.pdb"
  Delete /REBOOTOK "$SYSDIR\afs_cpa.pdb"
  Delete /REBOOTOK "$SYSDIR\afslogon.pdb"
  Delete /REBOOTOK "$SYSDIR\afscpcc.pdb"

  RMDir /r "$INSTDIR\Documentation\html\CmdRef"
  RMDir /r "$INSTDIR\Documentation\html\InstallGd"
  RMDir /r "$INSTDIR\Documentation\html\ReleaseNotes"
  RMDir /r "$INSTDIR\Documentation\html\SysAdminGd"
  RMDIr /r "$INSTDIR\Documentation\html"
  
  RMDir "$INSTDIR\Documentation"
  ; Delete DOC short cut
  Delete /REBOOTOK "$INSTDIR\Client\Program\afscreds.exe"

  Delete /REBOOTOK "$INSTDIR\Client\Program\afscreds.pdb"

  Delete /REBOOTOK "$INSTDIR\Client\Program\*"
  Delete /REBOOTOK "$INSTDIR\Client\Program\Include\*"
  Delete /REBOOTOK "$INSTDIR\Client\Program\Include\afs\*"
  Delete /REBOOTOK "$INSTDIR\Client\Program\Include\rx\*"
  Delete /REBOOTOK "$INSTDIR\Client\Program\Sample\*"
  RMDir  "$INSTDIR\Client\Program\Sample"
  RMDir  "$INSTDIR\Client\Program\Include\afs"
  RMDir  "$INSTDIR\Client\Program\Include\rx"
  RMDir  "$INSTDIR\Client\Program\Include"
  RMDir  "$INSTDIR\Client\Program"
  RMDir  "$INSTDIR\Client"

!IFDEF DEBUG  
!IFDEF CL_1400
   Delete /REBOOTOK "$INSTDIR\bin\msvcr80d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr80d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp80d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp80d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\mfc80d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\mfc80d.pdb"
!ELSE
!IFDEF CL_1310
   Delete /REBOOTOK "$INSTDIR\bin\msvcr71d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr71d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp71d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp71d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\mfc71d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\mfc71d.pdb"
!ELSE
!IFDEF CL_1300
   Delete /REBOOTOK "$INSTDIR\bin\msvcr70d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr70d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp70d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp70d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\mfc70d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\mfc70d.pdb"
!ELSE
   Delete /REBOOTOK "$INSTDIR\bin\mfc42d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\mfc42d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp60d.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp60d.pdb"
   Delete /REBOOTOK "$INSTDIR\bin\msvcrtd.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcrtd.pdb"
!ENDIF
!ENDIF
!ENDIF
!ELSE
!IFDEF CL_1400
   Delete /REBOOTOK "$INSTDIR\bin\mfc80.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr80.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp80.dll"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80CHS.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80CHT.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80DEU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80ENU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80ESP.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80FRA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80ITA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80JPN.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC80KOR.DLL"
!ELSE
!IFDEF CL_1310
   Delete /REBOOTOK "$INSTDIR\bin\mfc71.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr71.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp71.dll"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71CHS.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71CHT.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71DEU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71ENU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71ESP.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71FRA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71ITA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71JPN.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC71KOR.DLL"
!ELSE
!IFDEF CL_1300
   Delete /REBOOTOK "$INSTDIR\bin\mfc70.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcr70.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp70.dll"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70CHS.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70CHT.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70DEU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70ENU.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70ESP.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70FRA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70ITA.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70JPN.DLL"
   Delete /REBOOTOK "$INSTDIR\bin\MFC70KOR.DLL"
!ELSE
   Delete /REBOOTOK "$INSTDIR\bin\mfc42.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcp60.dll"
   Delete /REBOOTOK "$INSTDIR\bin\msvcrt.dll"
!ENDIF
!ENDIF
!ENDIF
!ENDIF

  Delete /REBOOTOK "$INSTDIR\Common\*"
  RMDir "$INSTDIR\Common"

!ifdef INSTALL_KFW
  ;Remove KfW files
  Delete /REBOOTOK "$INSTDIR\kfw\bin\*"
  RMDIR  /r "$INSTDIR\kfw\bin"
  Delete /REBOOTOK "$INSTDIR\kfw\doc\*"
  RMDIR  /r "$INSTDIR\kfw\doc"
  RMDIR  /r "$INSTDIR\kfw"
!endif

  Delete "$SMPROGRAMS\OpenAFS\Documentation.lnk"

  ; Remove control center
  Delete /REBOOTOK "$INSTDIR\Control Center\TaAfsAccountManager.exe"
  Delete /REBOOTOK "$INSTDIR\Control Center\TaAfsAdmSvr.exe"
  Delete /REBOOTOK "$INSTDIR\Control Center\TaAfsServerManager.exe"
  Delete /REBOOTOK "$INSTDIR\Control Center\CCUninst.dll"
  Delete /REBOOTOK "$INSTDIR\Control Center\TaAfsAccountManager.pdb"
  Delete /REBOOTOK "$INSTDIR\Control Center\TaAfsAdmSvr.pdb"
  Delete /REBOOTOK "$INSTDIR\Control Center\TaAfsServerManager.pdb"
  RMDir  "$INSTDIR\Control Center"
  
  Delete "$SMPROGRAMS\OpenAFS\Uninstall OpenAFS.lnk"
  Delete "$SMPROGRAMS\OpenAFS\Client\Authentication.lnk"
  Delete "$SMPROGRAMS\OpenAFS\Control Center\Account Manager.lnk"
  Delete "$SMPROGRAMS\OpenAFS\Control Center\Server Manager.lnk"
  RMDIR "$SMPROGRAMS\OpenAFS\Control Center"
  RMDir /r "$SMPROGRAMS\OpenAFS\Client"
  RMDir /r "$SMPROGRAMS\OpenAFS"
  Delete "$SMSTARTUP\AFS Credentials.lnk"
  
  ReadRegStr $R0 HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon" "CachePath"
  IfErrors +2
  Delete "$R0\AFSCache"
  Delete "C:\AFSCache"

  DeleteRegKey HKCR "*\shellex\ContextMenuHandlers\AFS Client Shell Extension"
  DeleteRegKey HKCR "CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}\InprocServer32"
  DeleteRegKey HKCR "CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}"
  DeleteRegKey HKCR "FOLDER\shellex\ContextMenuHandlers\AFS Client Shell Extension"
  DeleteRegValue HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" "{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}"
  DeleteRegValue HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Control Panel\Cpls" "afs_cpa"

  ; WinLogon Event Notification
  DeleteRegKey HKLM "Software\Microsoft\Windows NT\CurrentVersion\Winlogon\Notify\AfsLogon"

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
  DeleteRegValue HKLM "SYSTEM\CurrentControlSet\Services\NetBT\Parameters" "SmbDeviceEnabled"

  ; Support for apps that wrote submount data directly to afsdsbmt.ini
  DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\IniFileMapping\afsdsbmt.ini"

  RMDir  "$INSTDIR"

SectionEnd

;--------------------------------
;Uninstaller Functions

Function un.onInit

  ;Get language from registry
  ReadRegStr $LANGUAGE HKCU "Software\OpenAFS\AFS" "Installer Language"

FunctionEnd

Function un.onUninstSuccess

  IfSilent SkipAsk
  MessageBox MB_OK "Please reboot your machine to complete uninstallation of the software"
  SkipAsk:

FunctionEnd

;------------------------------
; Get the CellServDB file from the Internet

Function afs.GetCellServDB

;Check if we should download CellServDB
ReadINIStr $R0 $0 "Field 4" "State"
StrCmp $R0 "1" DoDownload

;Do nothing if we're keeping the existing file
ReadINIStr $R0 $0 "Field 2" "State"
StrCmp $R0 "1" done

ReadINIStr $R0 $0 "Field 6" "State"
StrCmp $R0 "1" CheckOther

ReadINIStr $R0 $0 "Field 3" "State"
StrCmp $R0 "1" UsePackaged

; If none of these, grab file from other location
goto UsePackaged

DoDownload:
   ReadINIStr $R0 $0 "Field 5" "State"
   NSISdl::download $R0 "$INSTDIR\Client\CellServDB"
   Pop $R0 ;Get the return value
   StrCmp $R0 "success" +2
      MessageBox MB_OK|MB_ICONSTOP "Download failed: $R0"
   goto done

UsePackaged:
   SetOutPath "$INSTDIR\Client"
   File "CellServDB"
   goto done
   
CheckOther:
   ReadINIStr $R0 $0 "Field 7" "State"
   StrCmp $R0 "" done
   CopyFiles $R0 "$INSTDIR\Client\CellServDB"
   
done:

FunctionEnd

Function AddProvider
   Push $R0
   Push $R1
   ReadRegStr $R0 HKLM "SYSTEM\CurrentControlSet\Control\NetworkProvider\HWOrder" "ProviderOrder"
   Push $R0
   StrCpy $R0 "TransarcAFSDaemon"
   Push $R0
   Call StrStr
   Pop $R0
   StrCmp $R0 "" +1 DoOther
   ReadRegStr $R1 HKLM "SYSTEM\CurrentControlSet\Control\NetworkProvider\HWOrder" "ProviderOrder"   
   StrCpy $R0 "$R1,TransarcAFSDaemon"
   WriteRegStr HKLM "SYSTEM\CurrentControlSet\Control\NetworkProvider\HWOrder" "ProviderOrder" $R0
DoOther:
   ReadRegStr $R0 HKLM "SYSTEM\CurrentControlSet\Control\NetworkProvider\Order" "ProviderOrder"
   Push $R0
   StrCpy $R0 "TransarcAFSDaemon"
   Push $R0
   Call StrStr
   Pop $R0
   StrCmp $R0 "" +1 End
   ReadRegStr $R1 HKLM "SYSTEM\CurrentControlSet\Control\NetworkProvider\Order" "ProviderOrder"   
   StrCpy $R0 "$R1,TransarcAFSDaemon"
   WriteRegStr HKLM "SYSTEM\CurrentControlSet\Control\NetworkProvider\Order" "ProviderOrder" $R0   
End:
   Pop $R1
   Pop $R0
FunctionEnd

Function un.RemoveProvider
   Push $R0
   StrCpy $R0 "TransarcAFSDaemon"
   Push $R0
   StrCpy $R0 "SYSTEM\CurrentControlSet\Control\NetworkProvider\HWOrder"
   Call un.RemoveFromProvider
   StrCpy $R0 "TransarcAFSDaemon"
   Push $R0
   StrCpy $R0 "SYSTEM\CurrentControlSet\Control\NetworkProvider\Order"
   Call un.RemoveFromProvider
   Pop $R0
FunctionEnd

Function un.RemoveFromProvider
  Exch $0
  Push $1
  Push $2
  Push $3
  Push $4
  Push $5
  Push $6

  ReadRegStr $1 HKLM "$R0" "ProviderOrder"
    StrCpy $5 $1 1 -1 # copy last char
    StrCmp $5 "," +2 # if last char != ,
      StrCpy $1 "$1," # append ,
    Push $1
    Push "$0,"
    Call un.StrStr ; Find `$0,` in $1
    Pop $2 ; pos of our dir
    StrCmp $2 "" unRemoveFromPath_done
      ; else, it is in path
      # $0 - path to add
      # $1 - path var
      StrLen $3 "$0,"
      StrLen $4 $2
      StrCpy $5 $1 -$4 # $5 is now the part before the path to remove
      StrCpy $6 $2 "" $3 # $6 is now the part after the path to remove
      StrCpy $3 $5$6

      StrCpy $5 $3 1 -1 # copy last char
      StrCmp $5 "," 0 +2 # if last char == ,
        StrCpy $3 $3 -1 # remove last char

      WriteRegStr HKLM "$R0" "ProviderOrder" $3
      
  unRemoveFromPath_done:
    Pop $6
    Pop $5
    Pop $4
    Pop $3
    Pop $2
    Pop $1
    Pop $0
FunctionEnd

Function CheckPathForAFS
   Push $0
   Push $1
   Push $2
   Push $3
   ReadRegStr $1 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH"
   StrCpy $1 "$1;"
loop:
   Push $1
   Push ";"
   Call StrStr
   Pop $0
   StrLen $2 $0
   StrCpy $3 $1 -$2
   IfFileExists "$3\afsd_service.exe" Error
   StrCpy $1 $0 32768 1
   StrLen $2 $1
   IntCmp $2 0 Done Done loop
   goto Done
Error:
   MessageBox MB_ICONSTOP|MB_OK|MB_TOPMOST "This installer is unable to upgrade the previous version of AFS. Please uninstall the current AFS version before continuing."
   Abort "Unable to install OpenAFS"
Done:
   Pop $3
   Pop $2
   Pop $1
   Pop $0
FunctionEnd

Function AddToUniquePath
   Pop $R0
   Push $R0
   Push "$R0;"
   ReadRegStr $R0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH"
   Push "$R0;"
   Call StrStr
   Pop $R0
   StrCmp $R0 "" +1 Done
   Call AddToPath
Done:
FunctionEnd


;-------------------------------
;Do the page to get the CellServDB

Function AFSPageGetCellServDB
  ; Skip this page if we are not installing the client
  SectionGetFlags ${secClient} $R0
  IntOp $R0 $R0 & ${SF_SELECTED}
  StrCmp $R0 "0" Skip
  
  ; Set the install options here
  
startOver:
  WriteINIStr $0 "Field 2" "Flags" "DISABLED"
  WriteINIStr $0 "Field 3" "State" "1"
  WriteINISTR $0 "Field 4" "State" "0"
  WriteINIStr $0 "Field 6" "State" "0"
  
  ; If there is an existing afsdcell.ini file, migrate it to CellServDB
  IfFileExists "$WINDIR\afsdcell.ini" +1 +3
  CopyFiles /SILENT "$WINDIR\afsdcell.ini" "$INSTDIR\Client\CellServDB"
  Delete "$WINDIR\afsdcell.ini"
  ; If there is an existing CellServDB file, allow the user to choose it and make it default
  IfFileExists "$INSTDIR\Client\CellServDB" +1 notpresent
  WriteINIStr $0 "Field 2" "Flags" "ENABLED"
  WriteINIStr $0 "Field 2" "State" "1"
  WriteINIStr $0 "Field 3" "State" "0"
  
  notpresent:
  
  !insertmacro MUI_HEADER_TEXT "CellServDB Configuration" "Please choose a method for installing the CellServDB file:" 
  InstallOptions::dialog $0
  Pop $R1
  StrCmp $R1 "cancel" exit
  StrCmp $R1 "back" done
  StrCmp $R1 "success" done
exit: Quit
done:

   ; Check that if a file is set, a valid filename is entered...
   ReadINIStr $R0 $0 "Field 6" "State"
   StrCmp $R0 "1" CheckFileName
   
   ;Check if a URL is specified, one *IS* specified
   ReadINIStr $R0 $0 "Field 4" "State"
   StrCmp $R0 "1" CheckURL Skip
   
   CheckURL:
   ReadINIStr $R0 $0 "Field 5" "State"
   StrCmp $R0 "" +1 Skip
   MessageBox MB_OK|MB_ICONSTOP $(URLError)
   WriteINIStr $0 "Field 4" "State" "0"
   goto startOver
   
   CheckFileName:
   ReadINIStr $R0 $0 "Field 7" "State"
   IfFileExists $R0 Skip

   MessageBox MB_OK|MB_ICONSTOP $(CellError)
   WriteINIStr $0 "Field 6" "State" "0"
   goto startOver
   
   Skip:
   
FunctionEnd


Function AFSPageGetCellName
   IfSilent good
  ; Skip this page if we are not installing the client
  SectionGetFlags ${secClient} $R0
  IntOp $R0 $R0 & ${SF_SELECTED}
  StrCmp $R0 "0" good
  
startOver:
   ; We want to read in the existing parameters and make them the defaults
   
   ;AFS Crypt security
   ReadRegDWORD $R1 HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "SecurityLevel"
   StrCmp $R1 "" +3
   WriteINIStr $1 "Field 3" "State" $R1
   goto +2
   WriteINIStr $1 "Field 3" "State" "1"
   
   ;Use DNS
   ReadRegDWORD $R1 HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "UseDNS"
   StrCmp $R1 "" +3
   WriteINIStr $1 "Field 9" "State" $R1
   goto +2
   WriteINIStr $1 "Field 9" "State" "1"
   
   ; Use integrated logon
   ReadRegDWORD $R1 HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "LogonOptions"
   StrCmp $R1 "" +3
   WriteINIStr $1 "Field 7" "State" $R1
   goto +2
   WriteINIStr $1 "Field 7" "State" "0"
   
   ; If this is a server install, we do NOT want to recommend the Freelance client
   ; And we do not need to ask for the cell name.
   SectionGetFlags ${secServer} $R1
   IntOp $R1 $R1 & ${SF_SELECTED}
   StrCmp $R1 "1" +1 NotServer
   WriteINIStr $1 "Field 6" "Text" "Enable AFS Freelance client (Not Recommended for servers)"
   ReadRegDWORD $R1 HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "FreelanceClient"
   StrCmp $R1 "" +3
   WriteINIStr $1 "Field 5" "State" $R1
   goto +2
   WriteINIStr $1 "Field 5" "State" "0"
   WriteINIStr $1 "Field 1" "Flags" "DISABLED"
   WriteINIStr $1 "Field 2" "Flags" "DISABLED"
   goto SkipServerTest
NotServer:
   WriteINIStr $1 "Field 6" "Text" "Enable AFS Freelance client (Recommended)"
   ReadRegDWORD $R1 HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "FreelanceClient"
   StrCmp $R1 "" +3
   WriteINIStr $1 "Field 5" "State" $R1
   goto +2
   WriteINIStr $1 "Field 5" "State" "1"
   WriteINIStr $1 "Field 1" "Flags" ""
   WriteINIStr $1 "Field 2" "Flags" ""
SkipServerTest:
   ; Get the current cell name, if any
   ReadRegStr $R1 HKLM "SYSTEM\CurrentControlSet\Services\TransarcAFSDaemon\Parameters" "Cell"
   StrCmp $R1 "" +2
   WriteINIStr $1 "Field 2" "State" $R1
  !insertmacro MUI_HEADER_TEXT "Client Cell Name Configuration" "Please enter the name for your default cell:" 
  InstallOptions::dialog $1
  Pop $R1
  StrCmp $R1 "cancel" exit
  StrCmp $R1 "back" done
  StrCmp $R1 "success" done
exit: Quit
done:
   ReadINIStr $R0 $1 "Field 2" "State"
   StrCmp $R0 "" +1 good
   
   MessageBox MB_OK|MB_ICONSTOP $(CellNameError)
   goto startOver
good:
FunctionEnd


;---------------------------------------------------------
;Do the page to get the afscreds.exe startup configuration

Function AFSPageConfigAFSCreds
  ; Skip this page if we are not installing the client
  SectionGetFlags ${secClient} $R0
  IntOp $R0 $R0 & ${SF_SELECTED}
  StrCmp $R0 "0" done
  
  ; Set the install options here
  
  !insertmacro MUI_HEADER_TEXT "AFS Credentials Configuration" "Please choose default options for configuring the AFS Credentials program:" 
  InstallOptions::dialog $2
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
!ifndef DEBUG
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenAFS" "DisplayVersion" "${AFS_VERSION}"
!else
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenAFS" "DisplayVersion" "${AFS_VERSION} Checked/Debug"
!endif
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenAFS" "URLInfoAbout" "http://www.openafs.org/"

FunctionEnd


;-------------------
; Get the currently installed version and place it on the stack
; Modifies: Nothing
Function GetInstalledVersion
   Push $R0
   Push $R1
   Push $R4
   
   ReadRegStr $R0 HKLM "Software\TransarcCorporation\$R2\CurrentVersion" "VersionString"
   StrCmp $R0 "" NotTransarc done
   
   
NotTransarc:
   ReadRegStr $R0 HKLM "${AFS_REGKEY_ROOT}\$R2\CurrentVersion" "VersionString"
   StrCmp $R0 "" done
   
done:
   Pop $R4
   Pop $R1
   Exch $R0
FunctionEnd

; Functions to get each component of the version number
Function GetInstalledVersionMajor
   Push $R0
   Push $R1
   Push $R4
   
   ReadRegDWORD $R0 HKLM "Software\TransarcCorporation\$R2\CurrentVersion" "MajorVersion"
   StrCmp $R0 "" NotTransarc done
   
   
NotTransarc:
   ReadRegDWORD $R0 HKLM "${AFS_REGKEY_ROOT}\$R2\CurrentVersion" "MajorVersion"
   StrCmp $R0 "" done
   
done:
   Pop $R4
   Pop $R1
   Exch $R0
FunctionEnd

Function GetInstalledVersionMinor
   Push $R0
   Push $R1
   Push $R4
   
   ReadRegDWORD $R0 HKLM "Software\TransarcCorporation\$R2\CurrentVersion" "MinorVersion"
   StrCmp $R0 "" NotTransarc done
   
   
NotTransarc:
   ReadRegDWORD $R0 HKLM "${AFS_REGKEY_ROOT}\$R2\CurrentVersion" "MinorVersion"
   StrCmp $R0 "" done
   
done:
   Pop $R4
   Pop $R1
   Exch $R0
FunctionEnd

Function GetInstalledVersionPatch
   Push $R0
   Push $R1
   Push $R4
   
   ReadRegDWORD $R0 HKLM "Software\TransarcCorporation\$R2\CurrentVersion" "PatchLevel"
   StrCmp $R0 "" NotTransarc done
   
   
NotTransarc:
   ReadRegDWORD $R0 HKLM "${AFS_REGKEY_ROOT}\$R2\CurrentVersion" "PatchLevel"
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
   StrCpy $R2 "AFS Client"
   Call GetInstalledVersion
   Pop $R0
   
   StrCmp $R0 "" NotInstalled
   ; Now we see if it's an older or newer version
   
   Call GetInstalledVersionMajor
   Pop $R0
   IntCmpU $R0 ${AFS_MAJORVERSION} +1 Upgrade Downgrade

   Call GetInstalledVersionMinor
   Pop $R0
   IntCmpU $R0 ${AFS_MINORVERSION} +1 Upgrade Downgrade
   
   Call GetInstalledVersionPatch
   Pop $R0
   IntCmpU $R0 ${AFS_PATCHLEVEL} Reinstall Upgrade Downgrade
   
Reinstall:
   StrCpy $R0 "1"
   Exch $R0
   goto end
   
Upgrade:
   StrCpy $R0 "2"
   Exch $R0
   goto end
   
Downgrade:
   StrCpy $R0 "3"
   Exch $R0
   goto end
   
NotInstalled:
   StrCpy $R0 "0"
   Exch $R0
end:   
FunctionEnd

;-------------------------------
; Check how the server options should be set
Function ShouldServerInstall
   Push $R0
   StrCpy $R2 "AFS Server"
   Call GetInstalledVersion
   Pop $R0
   
   StrCmp $R0 "" NotInstalled
   ; Now we see if it's an older or newer version

   Call GetInstalledVersionMajor
   Pop $R0
   IntCmpU $R0 ${AFS_MAJORVERSION} +1 Upgrade Downgrade

   Call GetInstalledVersionMinor
   Pop $R0
   IntCmpU $R0 ${AFS_MINORVERSION} +1 Upgrade Downgrade
   
   Call GetInstalledVersionPatch
   Pop $R0
   IntCmpU $R0 ${AFS_PATCHLEVEL} Reinstall Upgrade Downgrade
   
Reinstall:
   StrCpy $R0 "1"
   Exch $R0
   goto end
   
Upgrade:
   StrCpy $R0 "2"
   Exch $R0
   goto end
   
Downgrade:
   StrCpy $R0 "3"
   Exch $R0
   goto end
   
   
NotInstalled:
   StrCpy $R0 "0"
   Exch $R0
end:   
FunctionEnd


; See if AFS Server is installed
; Returns: "1" if it is, 0 if it is not (on the stack)
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


; See if AFS Client is installed
; Returns: "1" if it is, 0 if it is not (on the stack)
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
; Returns: "1" if it is, 0 if it is not (on the stack)
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


; See if Control Center is installed
; Returns: "1" if it is, 0 if it is not (on the stack)
Function IsControlInstalled
   Push $R0
   StrCpy $R2 "AFS Control Center"
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


!ifdef USE_GETPARAMETERS
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
!endif


;Check to see if any AFS component is installed
;Returns: Value on stack: "1" if it is, "0" if it is not
Function IsAnyAFSInstalled
   Push $R0
   Push $R1
   Push $R2
   Push $R3
   Call IsClientInstalled
   Pop $R0
   Call IsServerInstalled
   Pop $R1
   Call IsControlInstalled
   Pop $R2
   Call IsDocumentationInstalled
   Pop $R3
   ; Now we must see if ANY of the $Rn values are 1
   StrCmp $R0 "1" SomethingInstalled
   StrCmp $R1 "1" SomethingInstalled
   StrCmp $R2 "1" SomethingInstalled
   StrCmp $R3 "1" SomethingInstalled
   ;Nothing installed
   StrCpy $R0 "0"
   goto end
SomethingInstalled:
   StrCpy $R0 "1"
end:
   Pop $R3
   Pop $R2
   Pop $R1
   Exch $R0
FunctionEnd


;Install English Language Files
Function AFSLangFiles
   ; Common files
   SetOutPath "$INSTDIR\Common"
   File "${AFS_CLIENT_BUILDDIR}\afs_config.exe"
  !insertmacro ReplaceDLL "${AFS_DESTDIR}\lib\afsauthent.dll" "$INSTDIR\Common\afsauthent.dll" "$INSTDIR"
  !insertmacro ReplaceDLL "${AFS_DESTDIR}\lib\afspthread.dll" "$INSTDIR\Common\afspthread.dll" "$INSTDIR"
  !insertmacro ReplaceDLL "${AFS_DESTDIR}\lib\afsrpc.dll" "$INSTDIR\Common\afsrpc.dll" "$INSTDIR"
  !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afsadminutil.dll"    "$INSTDIR\Common\afsadminutil.dll"    "$INSTDIR"
  !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afsclientadmin.dll"  "$INSTDIR\Common\afsclientadmin.dll"  "$INSTDIR" 
  !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afsprocmgmt.dll"     "$INSTDIR\Common\afsprocmgmt.dll"     "$INSTDIR" 
  !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afsvosadmin.dll"     "$INSTDIR\Common\afsvosadmin.dll"     "$INSTDIR" 
  !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAppLib.dll"     "$INSTDIR\Common\TaAfsAppLib.dll"     "$INSTDIR" 
  !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afsvosadmin.dll"     "$INSTDIR\Common\afsvosadmin.dll"     "$INSTDIR" 
  !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afsbosadmin.dll"     "$INSTDIR\Common\afsbosadmin.dll"     "$INSTDIR" 
  !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afscfgadmin.dll"     "$INSTDIR\Common\afscfgadmin.dll"     "$INSTDIR" 
  !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afskasadmin.dll"     "$INSTDIR\Common\afskasadmin.dll"     "$INSTDIR" 
  !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afsptsadmin.dll"     "$INSTDIR\Common\afsptsadmin.dll"     "$INSTDIR" 

 SetOutPath "$INSTDIR\Common"

!IFDEF DEBUG
!IFDEF CL_1400
   !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcr80d.dll" "$INSTDIR\Common\msvcr80d.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcp80d.dll" "$INSTDIR\Common\msvcp80d.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\mfc80d.dll" "$INSTDIR\Common\mfc80d.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80CHS.DLL" "$INSTDIR\Common\MFC80CHS.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80CHT.DLL" "$INSTDIR\Common\MFC80CHT.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80DEU.DLL" "$INSTDIR\Common\MFC80DEU.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80ENU.DLL" "$INSTDIR\Common\MFC80ENU.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80ESP.DLL" "$INSTDIR\Common\MFC80ESP.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80FRA.DLL" "$INSTDIR\Common\MFC80FRA.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80ITA.DLL" "$INSTDIR\Common\MFC80ITA.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80JPN.DLL" "$INSTDIR\Common\MFC80JPN.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80KOR.DLL" "$INSTDIR\Common\MFC80KOR.DLL" "$INSTDIR"
!ELSE
!IFDEF CL_1310
   !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcr71d.dll" "$INSTDIR\Common\msvcr71d.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcp71d.dll" "$INSTDIR\Common\msvcp71d.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\mfc71d.dll" "$INSTDIR\Common\mfc71d.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71CHS.DLL" "$INSTDIR\Common\MFC71CHS.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71CHT.DLL" "$INSTDIR\Common\MFC71CHT.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71DEU.DLL" "$INSTDIR\Common\MFC71DEU.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71ENU.DLL" "$INSTDIR\Common\MFC71ENU.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71ESP.DLL" "$INSTDIR\Common\MFC71ESP.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71FRA.DLL" "$INSTDIR\Common\MFC71FRA.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71ITA.DLL" "$INSTDIR\Common\MFC71ITA.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71JPN.DLL" "$INSTDIR\Common\MFC71JPN.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71KOR.DLL" "$INSTDIR\Common\MFC71KOR.DLL" "$INSTDIR"
!ELSE
!IFDEF CL_1300
   !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcr70d.dll" "$INSTDIR\Common\msvcr70d.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcp70d.dll" "$INSTDIR\Common\msvcp70d.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\mfc70d.dll" "$INSTDIR\Common\mfc70d.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70CHS.DLL" "$INSTDIR\Common\MFC70CHS.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70CHT.DLL" "$INSTDIR\Common\MFC70CHT.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70DEU.DLL" "$INSTDIR\Common\MFC70DEU.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70ENU.DLL" "$INSTDIR\Common\MFC70ENU.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70ESP.DLL" "$INSTDIR\Common\MFC70ESP.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70FRA.DLL" "$INSTDIR\Common\MFC70FRA.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70ITA.DLL" "$INSTDIR\Common\MFC70ITA.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70JPN.DLL" "$INSTDIR\Common\MFC70JPN.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70KOR.DLL" "$INSTDIR\Common\MFC70KOR.DLL" "$INSTDIR"
!ELSE
   !insertmacro ReplaceDLL "${SYSTEMDIR}\mfc42d.dll" "$INSTDIR\Common\mfc42d.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcp60d.dll" "$INSTDIR\Common\msvcp60d.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcrtd.dll" "$INSTDIR\Common\msvcrtd.dll" "$INSTDIR"
!ENDIF
!ENDIF
!ENDIF
!ELSE
!IFDEF CL_1400
   !insertmacro ReplaceDLL "${SYSTEMDIR}\mfc80.dll" "$INSTDIR\Common\mfc80.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcr80.dll" "$INSTDIR\Common\msvcr80.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcp80.dll" "$INSTDIR\Common\msvcp80.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80CHS.DLL" "$INSTDIR\Common\MFC80CHS.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80CHT.DLL" "$INSTDIR\Common\MFC80CHT.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80DEU.DLL" "$INSTDIR\Common\MFC80DEU.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80ENU.DLL" "$INSTDIR\Common\MFC80ENU.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80ESP.DLL" "$INSTDIR\Common\MFC80ESP.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80FRA.DLL" "$INSTDIR\Common\MFC80FRA.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80ITA.DLL" "$INSTDIR\Common\MFC80ITA.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80JPN.DLL" "$INSTDIR\Common\MFC80JPN.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC80KOR.DLL" "$INSTDIR\Common\MFC80KOR.DLL" "$INSTDIR"
!ELSE
!IFDEF CL_1310
   !insertmacro ReplaceDLL "${SYSTEMDIR}\mfc71.dll" "$INSTDIR\Common\mfc71.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcr71.dll" "$INSTDIR\Common\msvcr71.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcp71.dll" "$INSTDIR\Common\msvcp71.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71CHS.DLL" "$INSTDIR\Common\MFC71CHS.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71CHT.DLL" "$INSTDIR\Common\MFC71CHT.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71DEU.DLL" "$INSTDIR\Common\MFC71DEU.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71ENU.DLL" "$INSTDIR\Common\MFC71ENU.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71ESP.DLL" "$INSTDIR\Common\MFC71ESP.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71FRA.DLL" "$INSTDIR\Common\MFC71FRA.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71ITA.DLL" "$INSTDIR\Common\MFC71ITA.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71JPN.DLL" "$INSTDIR\Common\MFC71JPN.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC71KOR.DLL" "$INSTDIR\Common\MFC71KOR.DLL" "$INSTDIR"
!ELSE
!IFDEF CL_1300
   !insertmacro ReplaceDLL "${SYSTEMDIR}\mfc70.dll" "$INSTDIR\Common\mfc70.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcr70.dll" "$INSTDIR\Common\msvcr70.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcp70.dll" "$INSTDIR\Common\msvcp70.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70CHS.DLL" "$INSTDIR\Common\MFC70CHS.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70CHT.DLL" "$INSTDIR\Common\MFC70CHT.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70DEU.DLL" "$INSTDIR\Common\MFC70DEU.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70ENU.DLL" "$INSTDIR\Common\MFC70ENU.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70ESP.DLL" "$INSTDIR\Common\MFC70ESP.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70FRA.DLL" "$INSTDIR\Common\MFC70FRA.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70ITA.DLL" "$INSTDIR\Common\MFC70ITA.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70JPN.DLL" "$INSTDIR\Common\MFC70JPN.DLL" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\MFC70KOR.DLL" "$INSTDIR\Common\MFC70KOR.DLL" "$INSTDIR"
!ELSE
   !insertmacro ReplaceDLL "${SYSTEMDIR}\mfc42.dll" "$INSTDIR\Common\mfc42.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcp60.dll" "$INSTDIR\Common\msvcp60.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${SYSTEMDIR}\msvcrt.dll" "$INSTDIR\Common\msvcrt.dll" "$INSTDIR"
!ENDIF
!ENDIF
!ENDIF   
!ENDIF

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

   SetOutPath "$INSTDIR\Client\Program"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afscreds_1033.dll"    "$INSTDIR\Client\Program\afscreds_1033.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1033.dll" "$INSTDIR\Client\Program\afs_shl_ext_1033.dll" "$INSTDIR"
!ifdef DEBUG
   ;File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1033.pdb"
   ;File "${AFS_CLIENT_BUILDDIR}\afscreds_1033.pdb"
!endif

   SetOutPath "$INSTDIR\Common"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_config_1033.dll"           "$INSTDIR\Common\afs_config_1033.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_cpa_1033.dll"              "$INSTDIR\Common\afs_cpa_1033.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afseventmsg_1033.dll"          "$INSTDIR\Common\afseventmsg_1033.dll" "$INSTDIR"
  ;!insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afs_setup_utils_1033.dll"      "$INSTDIR\Common\afs_setup_utils_1033.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afsserver_1033.dll"            "$INSTDIR\Common\afsserver_1033.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afssvrcfg_1033.dll"            "$INSTDIR\Common\afssvrcfg_1033.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1033.dll"  "$INSTDIR\Common\TaAfsAccountManager_1033.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1033.dll"          "$INSTDIR\Common\TaAfsAppLib_1033.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1033.dll"   "$INSTDIR\Common\TaAfsServerManager_1033.dll" "$INSTDIR"
   File "..\..\doc\help\en_US\afs-cc.CNT"
   File "..\..\doc\help\en_US\afs-cc.hlp"
   File "..\..\doc\help\en_US\afs-light.CNT"
   File "..\..\doc\help\en_US\afs-light.hlp"
   File "..\..\doc\help\en_US\afs-nt.CNT"
   File "..\..\doc\help\en_US\afs-nt.HLP"
   File "..\..\doc\help\en_US\taafscfg.CNT"
   File "..\..\doc\help\en_US\taafscfg.hlp"
   File "..\..\doc\help\en_US\taafssvrmgr.CNT"
   File "..\..\doc\help\en_US\taafssvrmgr.hlp"
   File "..\..\doc\help\en_US\taafsusrmgr.CNT"
   File "..\..\doc\help\en_US\taafsusrmgr.hlp"

!ifdef DEBUG
   ;File "${AFS_CLIENT_BUILDDIR}\afs_config_1033.pdb"
   ;File "${AFS_CLIENT_BUILDDIR}\afs_cpa_1033.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afseventmsg_1033.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afsserver_1033.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afssvrcfg_1033.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1033.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1033.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1033.pdb"
!ENDIF
   goto done

DoGerman:

   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\de_DE\README.TXT"

   SetOutPath "$INSTDIR\Client\Program"
  !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afscreds_1032.dll"                      "$INSTDIR\Client\Program\afscreds_1032.dll" "$INSTDIR"
  !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1032.dll" "$INSTDIR\Client\Program\afs_shl_ext_1032.dll" "$INSTDIR"
!ifdef DEBUG
   ;File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1032.pdb"
   ;File "${AFS_CLIENT_BUILDDIR}\afscreds_1032.pdb"
!endif

   SetOutPath "$INSTDIR\Common"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_config_1032.dll"           "$INSTDIR\Common\afs_config_1032.dll" "$INSTDIR"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_cpa_1032.dll"              "$INSTDIR\Common\afs_cpa_1032.dll" "$INSTDIR" 
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afseventmsg_1032.dll"          "$INSTDIR\Common\afseventmsg_1032.dll" "$INSTDIR" 
  ;!insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afs_setup_utils_1032.dll"      "$INSTDIR\Common\afs_setup_utils_1032.dll" "$INSTDIR" 
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afsserver_1032.dll"            "$INSTDIR\Common\afsserver_1032.dll" "$INSTDIR" 
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afssvrcfg_1032.dll"            "$INSTDIR\Common\afssvrcfg_1032.dll" "$INSTDIR" 
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1032.dll"  "$INSTDIR\Common\TaAfsAccountManager_1032.dll" "$INSTDIR" 
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1032.dll"          "$INSTDIR\Common\TaAfsAppLib_1032.dll" "$INSTDIR" 
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1032.dll"   "$INSTDIR\Common\TaAfsServerManager_1032.dll" "$INSTDIR" 
   File "..\..\doc\help\de_DE\afs-cc.CNT"
   File "..\..\doc\help\de_DE\afs-cc.hlp"
   File "..\..\doc\help\de_DE\afs-light.CNT"
   File "..\..\doc\help\de_DE\afs-light.hlp"
   File "..\..\doc\help\de_DE\afs-nt.CNT"
   File "..\..\doc\help\de_DE\afs-nt.HLP"
   File "..\..\doc\help\de_DE\taafscfg.CNT"
   File "..\..\doc\help\de_DE\taafscfg.hlp"
   File "..\..\doc\help\de_DE\taafssvrmgr.CNT"
   File "..\..\doc\help\de_DE\taafssvrmgr.hlp"
   File "..\..\doc\help\de_DE\taafsusrmgr.CNT"
   File "..\..\doc\help\de_DE\taafsusrmgr.hlp"

!ifdef DEBUG
   ;File "${AFS_CLIENT_BUILDDIR}\afs_config_1032.pdb"
   ;File "${AFS_CLIENT_BUILDDIR}\afs_cpa_1032.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afseventmsg_1032.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afsserver_1032.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afssvrcfg_1032.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1032.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1032.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1032.pdb"
!ENDIF
   goto done   

DoSpanish:

   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\es_ES\README.TXT"

   SetOutPath "$INSTDIR\Client\Program"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afscreds_1034.dll"     "$INSTDIR\Client\Program\afscreds_1034.dll" "$INSTDIR" 
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1034.dll" "$INSTDIR\Client\Program\afs_shl_ext_1034.dll" "$INSTDIR"
!ifdef DEBUG
   ;File "${AFS_CLIENT_BUILDDIR}\afscreds_1034.pdb"
   ;File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1034.pdb"
!endif

   SetOutPath "$INSTDIR\Common"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_config_1034.dll"          "$INSTDIR\Common\afs_config_1034.dll" "$INSTDIR"  
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_cpa_1034.dll"             "$INSTDIR\Common\afs_cpa_1034.dll" "$INSTDIR"  
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afseventmsg_1034.dll"         "$INSTDIR\Common\afseventmsg_1034.dll" "$INSTDIR"  
  ;!insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afs_setup_utils_1034.dll"     "$INSTDIR\Common\afs_setup_utils_1034.dll" "$INSTDIR"  
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afsserver_1034.dll"           "$INSTDIR\Common\afsserver_1034.dll" "$INSTDIR"  
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afssvrcfg_1034.dll"           "$INSTDIR\Common\afssvrcfg_1034.dll" "$INSTDIR"  
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1034.dll" "$INSTDIR\Common\TaAfsAccountManager_1034.dll" "$INSTDIR"  
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1034.dll"         "$INSTDIR\Common\TaAfsAppLib_1034.dll" "$INSTDIR"  
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1034.dll"  "$INSTDIR\Common\TaAfsServerManager_1034.dll" "$INSTDIR"  
   File "..\..\doc\help\es_ES\afs-cc.CNT"
   File "..\..\doc\help\es_ES\afs-cc.hlp"
   File "..\..\doc\help\es_ES\afs-light.CNT"
   File "..\..\doc\help\es_ES\afs-light.hlp"
   File "..\..\doc\help\es_ES\afs-nt.CNT"
   File "..\..\doc\help\es_ES\afs-nt.HLP"
   File "..\..\doc\help\es_ES\taafscfg.CNT"
   File "..\..\doc\help\es_ES\taafscfg.hlp"
   File "..\..\doc\help\es_ES\taafssvrmgr.CNT"
   File "..\..\doc\help\es_ES\taafssvrmgr.hlp"
   File "..\..\doc\help\es_ES\taafsusrmgr.CNT"
   File "..\..\doc\help\es_ES\taafsusrmgr.hlp"

!ifdef DEBUG
   ;File "${AFS_CLIENT_BUILDDIR}\afs_config_1034.pdb"
   ;File "${AFS_CLIENT_BUILDDIR}\afs_cpa_1034.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afseventmsg_1034.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afsserver_1034.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afssvrcfg_1034.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1034.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1034.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1034.pdb"
!ENDIF
   goto done

DoJapanese:

   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\ja_JP\README.TXT"

   SetOutPath "$INSTDIR\Client\Program"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afscreds_1041.dll"  "$INSTDIR\Client\Program\afscreds_1041.dll" "$INSTDIR"  
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1041.dll" "$INSTDIR\Client\Program\afs_shl_ext_1041.dll" "$INSTDIR"
!ifdef DEBUG
   ;File "${AFS_CLIENT_BUILDDIR}\afscreds_1041.pdb"
   ;File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1041.pdb"
!endif

   SetOutPath "$INSTDIR\Common"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_config_1041.dll"           "$INSTDIR\Common\afs_config_1041.dll" "$INSTDIR"   
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_cpa_1041.dll"              "$INSTDIR\Common\afs_cpa_1041.dll" "$INSTDIR"   
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afseventmsg_1041.dll"          "$INSTDIR\Common\afseventmsg_1041.dll" "$INSTDIR"   
  ;!insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afs_setup_utils_1041.dll"      "$INSTDIR\Common\afs_setup_utils_1041.dll" "$INSTDIR"   
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afsserver_1041.dll"            "$INSTDIR\Common\afsserver_1041.dll" "$INSTDIR"   
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afssvrcfg_1041.dll"            "$INSTDIR\Common\afssvrcfg_1041.dll" "$INSTDIR"   
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1041.dll"  "$INSTDIR\Common\TaAfsAccountManager_1041.dll" "$INSTDIR"   
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1041.dll"          "$INSTDIR\Common\TaAfsAppLib_1041.dll" "$INSTDIR"   
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1041.dll"   "$INSTDIR\Common\TaAfsServerManager_1041.dll" "$INSTDIR"   
   File "..\..\doc\help\ja_JP\afs-cc.CNT"
   File "..\..\doc\help\ja_JP\afs-cc.hlp"
   File "..\..\doc\help\ja_JP\afs-light.CNT"
   File "..\..\doc\help\ja_JP\afs-light.hlp"
   File "..\..\doc\help\ja_JP\afs-nt.CNT"
   File "..\..\doc\help\ja_JP\afs-nt.HLP"
   File "..\..\doc\help\ja_JP\taafscfg.CNT"
   File "..\..\doc\help\ja_JP\taafscfg.hlp"
   File "..\..\doc\help\ja_JP\taafssvrmgr.CNT"
   File "..\..\doc\help\ja_JP\taafssvrmgr.hlp"
   File "..\..\doc\help\ja_JP\taafsusrmgr.CNT"
   File "..\..\doc\help\ja_JP\taafsusrmgr.hlp"

!ifdef DEBUG
   ;File "${AFS_CLIENT_BUILDDIR}\afs_config_1041.pdb"
   ;File "${AFS_CLIENT_BUILDDIR}\afs_cpa_1041.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afseventmsg_1041.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afsserver_1041.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afssvrcfg_1041.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1041.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1041.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1041.pdb"
!ENDIF
   goto done
   
DoKorean:

   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\ko_KR\README.TXT"

   SetOutPath "$INSTDIR\Client\Program"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afscreds_1042.dll"  "$INSTDIR\Client\Program\afscreds_1042.dll" "$INSTDIR"   
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1042.dll" "$INSTDIR\Client\Program\afs_shl_ext_1042.dll" "$INSTDIR"
!ifdef DEBUG
   ;File "${AFS_CLIENT_BUILDDIR}\afscreds_1042.pdb"
   ;File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1042.pdb"
!endif

   SetOutPath "$INSTDIR\Common"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_config_1042.dll"           "$INSTDIR\Common\afs_config_1042.dll" "$INSTDIR"    
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_cpa_1042.dll"              "$INSTDIR\Common\afs_cpa_1042.dll" "$INSTDIR"    
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afseventmsg_1042.dll"          "$INSTDIR\Common\afseventmsg_1042.dll" "$INSTDIR"    
  ;!insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afs_setup_utils_1042.dll"      "$INSTDIR\Common\afs_setup_utils_1042.dll" "$INSTDIR"    
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afsserver_1042.dll"            "$INSTDIR\Common\afsserver_1042.dll" "$INSTDIR"    
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afssvrcfg_1042.dll"            "$INSTDIR\Common\afssvrcfg_1042.dll" "$INSTDIR"    
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1042.dll"  "$INSTDIR\Common\TaAfsAccountManager_1042.dll" "$INSTDIR"    
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1042.dll"          "$INSTDIR\Common\TaAfsAppLib_1042.dll" "$INSTDIR"    
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1042.dll"   "$INSTDIR\Common\TaAfsServerManager_1042.dll" "$INSTDIR"    
   File "..\..\doc\help\ko_KR\afs-cc.CNT"
   File "..\..\doc\help\ko_KR\afs-cc.hlp"
   File "..\..\doc\help\ko_KR\afs-light.CNT"
   File "..\..\doc\help\ko_KR\afs-light.hlp"
   File "..\..\doc\help\ko_KR\afs-nt.CNT"
   File "..\..\doc\help\ko_KR\afs-nt.HLP"
   File "..\..\doc\help\ko_KR\taafscfg.CNT"
   File "..\..\doc\help\ko_KR\taafscfg.hlp"
   File "..\..\doc\help\ko_KR\taafssvrmgr.CNT"
   File "..\..\doc\help\ko_KR\taafssvrmgr.hlp"
   File "..\..\doc\help\ko_KR\taafsusrmgr.CNT"
   File "..\..\doc\help\ko_KR\taafsusrmgr.hlp"

!ifdef DEBUG
   ;File "${AFS_CLIENT_BUILDDIR}\afs_config_1042.pdb"
   ;File "${AFS_CLIENT_BUILDDIR}\afs_cpa_1042.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afseventmsg_1042.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afsserver_1042.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afssvrcfg_1042.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1042.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1042.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1042.pdb"
!ENDIF
   goto done


DoPortugueseBR:

   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\pt_BR\README.TXT"

   SetOutPath "$INSTDIR\Client\Program"
   !insertmacro ReplaceDLL  "${AFS_CLIENT_BUILDDIR}\afscreds_1046.dll"  "$INSTDIR\Client\Program\afscreds_1046.dll" "$INSTDIR"    
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1046.dll" "$INSTDIR\Client\Program\afs_shl_ext_1046.dll" "$INSTDIR"
!ifdef DEBUG
   ;File "${AFS_CLIENT_BUILDDIR}\afscreds_1046.pdb"
   ;File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1046.pdb"
!endif

   SetOutPath "$INSTDIR\Common"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_config_1046.dll"           "$INSTDIR\Common\afs_config_1046.dll" "$INSTDIR"     
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_cpa_1046.dll"              "$INSTDIR\Common\afs_cpa_1046.dll" "$INSTDIR"     
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afseventmsg_1046.dll"          "$INSTDIR\Common\afseventmsg_1046.dll" "$INSTDIR"     
  ;!insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afs_setup_utils_1046.dll"      "$INSTDIR\Common\afs_setup_utils_1046.dll" "$INSTDIR"     
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afsserver_1046.dll"            "$INSTDIR\Common\afsserver_1046.dll" "$INSTDIR"     
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afssvrcfg_1046.dll"            "$INSTDIR\Common\afssvrcfg_1046.dll" "$INSTDIR"     
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1046.dll"  "$INSTDIR\Common\TaAfsAccountManager_1046.dll" "$INSTDIR"     
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1046.dll"          "$INSTDIR\Common\TaAfsAppLib_1046.dll" "$INSTDIR"     
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1046.dll"   "$INSTDIR\Common\TaAfsServerManager_1046.dll" "$INSTDIR"     
   File "..\..\doc\help\pt_BR\afs-cc.CNT"
   File "..\..\doc\help\pt_BR\afs-cc.hlp"
   File "..\..\doc\help\pt_BR\afs-light.CNT"
   File "..\..\doc\help\pt_BR\afs-light.hlp"
   File "..\..\doc\help\pt_BR\afs-nt.CNT"
   File "..\..\doc\help\pt_BR\afs-nt.HLP"
   File "..\..\doc\help\pt_BR\taafscfg.CNT"
   File "..\..\doc\help\pt_BR\taafscfg.hlp"
   File "..\..\doc\help\pt_BR\taafssvrmgr.CNT"
   File "..\..\doc\help\pt_BR\taafssvrmgr.hlp"
   File "..\..\doc\help\pt_BR\taafsusrmgr.CNT"
   File "..\..\doc\help\pt_BR\taafsusrmgr.hlp"

!ifdef DEBUG
   ;File "${AFS_CLIENT_BUILDDIR}\afs_config_1046.pdb"
   ;File "${AFS_CLIENT_BUILDDIR}\afs_cpa_1046.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afseventmsg_1046.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afsserver_1046.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afssvrcfg_1046.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1046.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1046.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1046.pdb"
!ENDIF
   goto done
   
DoSimpChinese:

   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\zh_CN\README.TXT"

   SetOutPath "$INSTDIR\Client\Program"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afscreds_2052.dll"   "$INSTDIR\Client\Program\afscreds_2052.dll" "$INSTDIR"     
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_2052.dll" "$INSTDIR\Client\Program\afs_shl_ext_2052.dll" "$INSTDIR"
!ifdef DEBUG
   ;File "${AFS_CLIENT_BUILDDIR}\afscreds_2052.pdb"
   ;File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_2052.pdb"
!endif

   SetOutPath "$INSTDIR\Common"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_config_2052.dll"           "$INSTDIR\Common\afs_config_2052.dll" "$INSTDIR"      
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_cpa_2052.dll"              "$INSTDIR\Common\afs_cpa_2052.dll" "$INSTDIR"      
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afseventmsg_2052.dll"          "$INSTDIR\Common\afseventmsg_2052.dll" "$INSTDIR"      
  ;!insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afs_setup_utils_2052.dll"      "$INSTDIR\Common\afs_setup_utils_2052.dll" "$INSTDIR"      
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afsserver_2052.dll"            "$INSTDIR\Common\afsserver_2052.dll" "$INSTDIR"      
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afssvrcfg_2052.dll"            "$INSTDIR\Common\afssvrcfg_2052.dll" "$INSTDIR"      
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_2052.dll"  "$INSTDIR\Common\TaAfsAccountManager_2052.dll" "$INSTDIR"      
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_2052.dll"          "$INSTDIR\Common\TaAfsAppLib_2052.dll" "$INSTDIR"      
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_2052.dll"   "$INSTDIR\Common\TaAfsServerManager_2052.dll" "$INSTDIR"      
   File "..\..\doc\help\zh_CN\afs-cc.CNT"
   File "..\..\doc\help\zh_CN\afs-cc.hlp"
   File "..\..\doc\help\zh_CN\afs-light.CNT"
   File "..\..\doc\help\zh_CN\afs-light.hlp"
   File "..\..\doc\help\zh_CN\afs-nt.CNT"
   File "..\..\doc\help\zh_CN\afs-nt.HLP"
   File "..\..\doc\help\zh_CN\taafscfg.CNT"
   File "..\..\doc\help\zh_CN\taafscfg.hlp"
   File "..\..\doc\help\zh_CN\taafssvrmgr.CNT"
   File "..\..\doc\help\zh_CN\taafssvrmgr.hlp"
   File "..\..\doc\help\zh_CN\taafsusrmgr.CNT"
   File "..\..\doc\help\zh_CN\taafsusrmgr.hlp"

!ifdef DEBUG
   ;File "${AFS_CLIENT_BUILDDIR}\afs_config_2052.pdb"
   ;File "${AFS_CLIENT_BUILDDIR}\afs_cpa_2052.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afseventmsg_2052.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afsserver_2052.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afssvrcfg_2052.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_2052.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_2052.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_2052.pdb"
!ENDIF
   goto done
   
DoTradChinese:

   SetOutPath "$INSTDIR\Documentation"
   File "..\..\doc\install\Documentation\zh_TW\README.TXT"

   SetOutPath "$INSTDIR\Client\Program"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afscreds_1028.dll"  "$INSTDIR\Client\Program\_1028.dll" "$INSTDIR"      
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1028.dll" "$INSTDIR\Client\Program\afs_shl_ext_1028.dll" "$INSTDIR"
!ifdef DEBUG
   ;File "${AFS_CLIENT_BUILDDIR}\afscreds_1028.pdb"
   ;File "${AFS_CLIENT_BUILDDIR}\afs_shl_ext_1028.pdb"
!endif

   SetOutPath "$INSTDIR\Common"
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_config_1028.dll"           "$INSTDIR\Common\afs_config_1028.dll" "$INSTDIR"       
   !insertmacro ReplaceDLL "${AFS_CLIENT_BUILDDIR}\afs_cpa_1028.dll"              "$INSTDIR\Common\afs_cpa_1028.dll" "$INSTDIR"       
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afseventmsg_1028.dll"          "$INSTDIR\Common\afseventmsg_1028.dll" "$INSTDIR"       
  ;!insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afs_setup_utils_1028.dll"      "$INSTDIR\Common\afs_setup_utils_1028.dll" "$INSTDIR"       
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afsserver_1028.dll"            "$INSTDIR\Common\afsserver_1028.dll" "$INSTDIR"       
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\afssvrcfg_1028.dll"            "$INSTDIR\Common\afssvrcfg_1028.dll" "$INSTDIR"       
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1028.dll"  "$INSTDIR\Common\TaAfsAccountManager_1028.dll" "$INSTDIR"       
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1028.dll"          "$INSTDIR\Common\TaAfsAppLib_1028.dll" "$INSTDIR"       
   !insertmacro ReplaceDLL "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1028.dll"   "$INSTDIR\Common\TaAfsServerManager_1028.dll" "$INSTDIR"       
   File "..\..\doc\help\zh_TW\afs-cc.CNT"
   File "..\..\doc\help\zh_TW\afs-cc.hlp"
   File "..\..\doc\help\zh_TW\afs-light.CNT"
   File "..\..\doc\help\zh_TW\afs-light.hlp"
   File "..\..\doc\help\zh_TW\afs-nt.CNT"
   File "..\..\doc\help\zh_TW\afs-nt.HLP"
   File "..\..\doc\help\zh_TW\taafscfg.CNT"
   File "..\..\doc\help\zh_TW\taafscfg.hlp"
   File "..\..\doc\help\zh_TW\taafssvrmgr.CNT"
   File "..\..\doc\help\zh_TW\taafssvrmgr.hlp"
   File "..\..\doc\help\zh_TW\taafsusrmgr.CNT"
   File "..\..\doc\help\zh_TW\taafsusrmgr.hlp"

!ifdef DEBUG
   ;File "${AFS_CLIENT_BUILDDIR}\afs_config_1028.pdb"
   ;File "${AFS_CLIENT_BUILDDIR}\afs_cpa_1028.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afseventmsg_1028.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afsserver_1028.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\afssvrcfg_1028.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAccountManager_1028.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsAppLib_1028.pdb"
   ;File "${AFS_SERVER_BUILDDIR}\TaAfsServerManager_1028.pdb"
!ENDIF
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
  Push $2
  Push $3

  # don't add if the path doesn't exist
  IfFileExists $0 "" AddToPath_done

  ReadEnvStr $1 PATH
  Push "$1;"
  Push "$0;"
  Call StrStr
  Pop $2
  StrCmp $2 "" "" AddToPath_done
  Push "$1;"
  Push "$0\;"
  Call StrStr
  Pop $2
  StrCmp $2 "" "" AddToPath_done
  GetFullPathName /SHORT $3 $0
  Push "$1;"
  Push "$3;"
  Call StrStr
  Pop $2
  StrCmp $2 "" "" AddToPath_done
  Push "$1;"
  Push "$3\;"
  Call StrStr
  Pop $2
  StrCmp $2 "" "" AddToPath_done

  Call IsNT
  Pop $1
  StrCmp $1 1 AddToPath_NT
    ; Not on NT
    StrCpy $1 $WINDIR 2
    FileOpen $1 "$1\autoexec.bat" a
    FileSeek $1 -1 END
    FileReadByte $1 $2
    IntCmp $2 26 0 +2 +2 # DOS EOF
      FileSeek $1 -1 END # write over EOF
    FileWrite $1 "$\r$\nSET PATH=%PATH%;$3$\r$\n"
    FileClose $1
    SetRebootFlag true
    Goto AddToPath_done

  AddToPath_NT:
    ReadRegStr $1 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH"
    StrCpy $2 $1 1 -1 # copy last char
    StrCmp $2 ";" 0 +2 # if last char == ;
      StrCpy $1 $1 -1 # remove last char
    StrCmp $1 "" AddToPath_NTdoIt
      StrCpy $0 "$1;$0"
    AddToPath_NTdoIt:
      WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH" $0
      SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

  AddToPath_done:
    Pop $3
    Pop $2
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
  Push $5
  Push $6

  IntFmt $6 "%c" 26 # DOS EOF

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
    Goto unRemoveFromPath_dosLoop

    unRemoveFromPath_dosLoop:
      FileRead $1 $3
      StrCpy $5 $3 1 -1 # read last char
      StrCmp $5 $6 0 +2 # if DOS EOF
        StrCpy $3 $3 -1 # remove DOS EOF so we can compare
      StrCmp $3 "$0$\r$\n" unRemoveFromPath_dosLoopRemoveLine
      StrCmp $3 "$0$\n" unRemoveFromPath_dosLoopRemoveLine
      StrCmp $3 "$0" unRemoveFromPath_dosLoopRemoveLine
      StrCmp $3 "" unRemoveFromPath_dosLoopEnd
      FileWrite $2 $3
      Goto unRemoveFromPath_dosLoop
      unRemoveFromPath_dosLoopRemoveLine:
        SetRebootFlag true
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
    ReadRegStr $1 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH"
    StrCpy $5 $1 1 -1 # copy last char
    StrCmp $5 ";" +2 # if last char != ;
      StrCpy $1 "$1;" # append ;
    Push $1
    Push "$0;"
    Call un.StrStr ; Find `$0;` in $1
    Pop $2 ; pos of our dir
    StrCmp $2 "" unRemoveFromPath_done
      ; else, it is in path
      # $0 - path to add
      # $1 - path var
      StrLen $3 "$0;"
      StrLen $4 $2
      StrCpy $5 $1 -$4 # $5 is now the part before the path to remove
      StrCpy $6 $2 "" $3 # $6 is now the part after the path to remove
      StrCpy $3 $5$6

      StrCpy $5 $3 1 -1 # copy last char
      StrCmp $5 ";" 0 +2 # if last char == ;
        StrCpy $3 $3 -1 # remove last char

      WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH" $3
      SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

  unRemoveFromPath_done:
    Pop $6
    Pop $5
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
!macro IsNT un
Function ${un}IsNT
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
!macroend
!insertmacro IsNT ""
!insertmacro IsNT "un."

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Uninstall stuff
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;====================================================
; StrStr - Finds a given string in another given string.
;               Returns -1 if not found and the pos if found.
;          Input: head of the stack - string to find
;                      second in the stack - string to find in
;          Output: head of the stack
;====================================================
!macro StrStr un
Function ${un}StrStr
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
!macroend
!insertmacro StrStr ""
!insertmacro StrStr "un."


!ifdef ADDSHAREDDLLUSED
; AddSharedDLL
 ;
 ; Increments a shared DLLs reference count.
 ; Use by passing one item on the stack (the full path of the DLL).
 ;
 ; Usage:
 ;   Push $SYSDIR\myDll.dll
 ;   Call AddSharedDLL
 ;

 Function AddSharedDLL
   Exch $R1
   Push $R0
   ReadRegDword $R0 HKLM Software\Microsoft\Windows\CurrentVersion\SharedDLLs $R1
   IntOp $R0 $R0 + 1
   WriteRegDWORD HKLM Software\Microsoft\Windows\CurrentVersion\SharedDLLs $R1 $R0
   Pop $R0
   Pop $R1
 FunctionEnd

 
; un.RemoveSharedDLL
 ;
 ; Decrements a shared DLLs reference count, and removes if necessary.
 ; Use by passing one item on the stack (the full path of the DLL).
 ; Note: for use in the main installer (not the uninstaller), rename the
 ; function to RemoveSharedDLL.
 ;
 ; Usage:
 ;   Push $SYSDIR\myDll.dll
 ;   Call un.RemoveSharedDLL
 ;

 Function un.RemoveSharedDLL
   Exch $R1
   Push $R0
   ReadRegDword $R0 HKLM Software\Microsoft\Windows\CurrentVersion\SharedDLLs $R1
   StrCmp $R0 "" remove
     IntOp $R0 $R0 - 1
     IntCmp $R0 0 rk rk uk
     rk:
       DeleteRegValue HKLM Software\Microsoft\Windows\CurrentVersion\SharedDLLs $R1
     goto Remove
     uk:
       WriteRegDWORD HKLM Software\Microsoft\Windows\CurrentVersion\SharedDLLs $R1 $R0
     Goto noremove
   remove:
     Delete /REBOOTOK $R1
   noremove:
   Pop $R0
   Pop $R1
 FunctionEnd
!endif


; Installs the loopback adpater and disables it on Windows 2000
Function afs.InstallMSLoopback
   GetTempFileName $R0
   File /oname=$R0 "${AFS_WININSTALL_DIR}\afsloopback.dll"
   nsExec::Exec "rundll32.exe $R0 doLoopBackEntry AFS 10.254.254.253 255.255.255.252"
   Delete $R0
FunctionEnd

Function afs.isLoopbackInstalled
   SetOutPath $TEMP
   File "${AFS_WININSTALL_DIR}\afsloopback.dll"
   System::Call "$TEMP\afsloopback.dll::IsLoopbackInstalled() i().r11"
   Delete "$TEMP\afsloopback.dll"
FunctionEnd


; GetWindowsVersion
;
; Based on Yazno's function, http://yazno.tripod.com/powerpimpit/
; Updated by Joost Verburg
;
; Returns on top of stack
;
; Windows Version (95, 98, ME, NT x.x, 2000, XP, 2003)
; or
; '' (Unknown Windows Version)
;
; Usage:
;   Call GetWindowsVersion
;   Pop $R0
;   ; at this point $R0 is "NT 4.0" or whatnot

Function GetWindowsVersion

  Push $R0
  Push $R1

  ClearErrors

  ReadRegStr $R0 HKLM \
  "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion

  IfErrors 0 lbl_winnt
  
  ; we are not NT
  ReadRegStr $R0 HKLM \
  "SOFTWARE\Microsoft\Windows\CurrentVersion" VersionNumber

  StrCpy $R1 $R0 1
  StrCmp $R1 '4' 0 lbl_error

  StrCpy $R1 $R0 3

  StrCmp $R1 '4.0' lbl_win32_95
  StrCmp $R1 '4.9' lbl_win32_ME lbl_win32_98

  lbl_win32_95:
    StrCpy $R0 '95'
  Goto lbl_done

  lbl_win32_98:
    StrCpy $R0 '98'
  Goto lbl_done

  lbl_win32_ME:
    StrCpy $R0 'ME'
  Goto lbl_done

  lbl_winnt:

  StrCpy $R1 $R0 1

  StrCmp $R1 '3' lbl_winnt_x
  StrCmp $R1 '4' lbl_winnt_x

  StrCpy $R1 $R0 3

  StrCmp $R1 '5.0' lbl_winnt_2000
  StrCmp $R1 '5.1' lbl_winnt_XP
  StrCmp $R1 '5.2' lbl_winnt_2003 lbl_error

  lbl_winnt_x:
    StrCpy $R0 "NT $R0" 6
  Goto lbl_done

  lbl_winnt_2000:
    Strcpy $R0 '2000'
  Goto lbl_done

  lbl_winnt_XP:
    Strcpy $R0 'XP'
  Goto lbl_done

  lbl_winnt_2003:
    Strcpy $R0 '2003'
  Goto lbl_done

  lbl_error:
    Strcpy $R0 ''
  lbl_done:

  Pop $R1
  Exch $R0

FunctionEnd


; Author: Lilla (lilla@earthlink.net) 2003-06-13
; function IsUserAdmin uses plugin \NSIS\PlusgIns\UserInfo.dll
; This function is based upon code in \NSIS\Contrib\UserInfo\UserInfo.nsi
; This function was tested under NSIS 2 beta 4 (latest CVS as of this writing).
;
; Usage:
;   Call IsUserAdmin
;   Pop $R0   ; at this point $R0 is "true" or "false"
;
Function IsUserAdmin
Push $R0
Push $R1
Push $R2

ClearErrors
UserInfo::GetName
IfErrors Win9x
Pop $R1
UserInfo::GetAccountType
Pop $R2

StrCmp $R2 "Admin" 0 Continue
; Observation: I get here when running Win98SE. (Lilla)
; The functions UserInfo.dll looks for are there on Win98 too, 
; but just don't work. So UserInfo.dll, knowing that admin isn't required
; on Win98, returns admin anyway. (per kichik)
; MessageBox MB_OK 'User "$R1" is in the Administrators group'
StrCpy $R0 "true"
Goto Done

Continue:
; You should still check for an empty string because the functions
; UserInfo.dll looks for may not be present on Windows 95. (per kichik)
StrCmp $R2 "" Win9x
StrCpy $R0 "false"
;MessageBox MB_OK 'User "$R1" is in the "$R2" group'
Goto Done

Win9x:
; comment/message below is by UserInfo.nsi author:
; This one means you don't need to care about admin or
; not admin because Windows 9x doesn't either
;MessageBox MB_OK "Error! This DLL can't run under Windows 9x!"
StrCpy $R0 "false"

Done:
;MessageBox MB_OK 'User= "$R1"  AccountType= "$R2"  IsUserAdmin= "$R0"'

Pop $R2
Pop $R1
Exch $R0
FunctionEnd

; GetParent
 ; input, top of stack  (e.g. C:\Program Files\Poop)
 ; output, top of stack (replaces, with e.g. C:\Program Files)
 ; modifies no other variables.
 ;
 ; Usage:
 ;   Push "C:\Program Files\Directory\Whatever"
 ;   Call GetParent
 ;   Pop $R0
 ;   ; at this point $R0 will equal "C:\Program Files\Directory"

Function GetParent

  Exch $R0
  Push $R1
  Push $R2
  Push $R3
  
  StrCpy $R1 0
  StrLen $R2 $R0
  
  loop:
    IntOp $R1 $R1 + 1
    IntCmp $R1 $R2 get 0 get
    StrCpy $R3 $R0 1 -$R1
    StrCmp $R3 "\" get
  Goto loop
  
  get:
    StrCpy $R0 $R0 -$R1
    
    Pop $R3
    Pop $R2
    Pop $R1
    Exch $R0
    
FunctionEnd


;--------------------------------
;Handle what must and what must not be installed
Function .onSelChange
   ; If they install the server, they MUST install the client
   SectionGetFlags ${secServer} $R0
   IntOp $R0 $R0 & ${SF_SELECTED}
   StrCmp $R0 "1" MakeClientSelected
   
   ; If they install the control center, we'll give them the client.
   ; It may not be required, but it's a bit more useful
   SectionGetFlags ${secControl} $R0
   IntOp $R0 $R0 & ${SF_SELECTED}
   StrCmp $R0 "1" MakeClientSelected
   goto end
   
MakeClientSelected:
   SectionGetFlags ${secClient} $R0
   IntOp $R0 $R0 | ${SF_SELECTED}
   SectionSetFlags ${secClient} $R0
   
end:
FunctionEnd

Function RegWriteMultiStr
!define HKEY_CLASSES_ROOT        0x80000000
!define HKEY_CURRENT_USER        0x80000001
!define HKEY_LOCAL_MACHINE       0x80000002
!define HKEY_USERS               0x80000003
!define HKEY_PERFORMANCE_DATA    0x80000004
!define HKEY_PERFORMANCE_TEXT    0x80000050
!define HKEY_PERFORMANCE_NLSTEXT 0x80000060
!define HKEY_CURRENT_CONFIG      0x80000005
!define HKEY_DYN_DATA            0x80000006

!define KEY_QUERY_VALUE          0x0001
!define KEY_SET_VALUE            0x0002
!define KEY_CREATE_SUB_KEY       0x0004
!define KEY_ENUMERATE_SUB_KEYS   0x0008
!define KEY_NOTIFY               0x0010
!define KEY_CREATE_LINK          0x0020

!define REG_NONE                 0
!define REG_SZ                   1
!define REG_EXPAND_SZ            2
!define REG_BINARY               3
!define REG_DWORD                4
!define REG_DWORD_LITTLE_ENDIAN  4
!define REG_DWORD_BIG_ENDIAN     5
!define REG_LINK                 6
!define REG_MULTI_SZ             7

!define RegCreateKey             "Advapi32::RegCreateKeyA(i, t, *i) i"
!define RegSetValueEx            "Advapi32::RegSetValueExA(i, t, i, i, i, i) i"
!define RegCloseKey              "Advapi32::RegCloseKeyA(i) i"

  Exch $R0
  Push $1
  Push $2
  Push $9

  SetPluginUnload alwaysoff
  ; Create a buffer for the multi_sz value
  System::Call "*(&t${NSIS_MAX_STRLEN}) i.r1"
  ; Open/create the registry key
  System::Call "${RegCreateKey}(${HKEY_LOCAL_MACHINE}, '$REG_SUB_KEY', .r0) .r9"
  ; Failed?
  IntCmp $9 0 write
    MessageBox MB_OK|MB_ICONSTOP "Can't create registry key! ($9)"
    Goto noClose

  write:
    ; Fill in the buffer with our strings
    StrCpy $2 $1                            ; Initial position

    StrLen $9 '$REG_DATA_1'                 ; Length of first string
    IntOp $9 $9 + 1                         ; Plus null
    System::Call "*$2(&t$9 '$REG_DATA_1')"  ; Place the string
    IntOp $2 $2 + $9                        ; Advance to the next position

    StrCmp '$REG_DATA_2' "" terminate
    StrLen $9 '$REG_DATA_2'                 ; Length of second string
    IntOp $9 $9 + 1                         ; Plus null
    System::Call "*$2(&t$9 '$REG_DATA_2')"  ; Place the string
    IntOp $2 $2 + $9                        ; Advance to the next position

    StrCmp '$REG_DATA_3' "" terminate
    StrLen $9 '$REG_DATA_3'                 ; Length of third string
    IntOp $9 $9 + 1                         ; Plus null
    System::Call "*$2(&t$9 '$REG_DATA_3')"  ; Place the string
    IntOp $2 $2 + $9                        ; Advance to the next position

    StrCmp '$REG_DATA_4' "" terminate
    StrLen $9 '$REG_DATA_4'                 ; Length of third string
    IntOp $9 $9 + 1                         ; Plus null
    System::Call "*$2(&t$9 '$REG_DATA_4')"  ; Place the string
    IntOp $2 $2 + $9                        ; Advance to the next position

  terminate:
    System::Call "*$2(&t1 '')"              ; Place the terminating null
    IntOp $2 $2 + 1                         ; Advance to the next position

    ; Create/write the value
    IntOp $2 $2 - $1                        ; Total length
    System::Call "${RegSetValueEx}(r0, '$REG_VALUE', 0, ${REG_MULTI_SZ}, r1, r2) .r9"
    ; Failed?
    IntCmp $9 0 done
      MessageBox MB_OK|MB_ICONSTOP "Can't set key value! ($9)"
      Goto done

  done:
    ; Close the registry key
    System::Call "${RegCloseKey}(r0)"

noClose:
  ; Clear the buffer
  SetPluginUnload manual
  System::Free $1

  Pop $9
  Pop $2
  Pop $1
  Exch $R0
FunctionEnd

Function CreateDesktopIni
   WriteIniStr "$INSTDIR\Desktop.ini" ".ShellClassInfo" "IconFile" "client\program\afsd_service.exe"
   WriteIniStr "$INSTDIR\Desktop.ini" ".ShellClassInfo" "IconIndex" "0"
   SetFileAttributes "$INSTDIR\Desktop.ini" HIDDEN|SYSTEM
   SetFileAttributes "$INSTDIR\" READONLY
FunctionEnd
