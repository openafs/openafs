/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSAPPLIB_H
#define AFSAPPLIB_H

/*
 * The AFS Application Library is a collection of handy support code
 * for UI-level applications written for Windows NT and Windows 95.
 *
 * The various components of the library provide common dialogs,
 * error code translation, task threading, error dialogs, additional
 * Windows dialog controls, and many other functions.
 *
 * The goal of this library is to shorten application development time
 * by providing pre-packaged functions, while ensuring a common user
 * interface for Transarc applications.
 *
 */


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#ifdef EXPORTED
#undef EXPORTED
#endif
#ifdef EXPORT_AFSAPPLIB
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED __declspec(dllimport)
#endif

#ifndef APP_HINST
#   define APP_HINST AfsAppLib_GetAppInstance()
#endif
#ifndef APPLIB_HINST
#   define APPLIB_HINST AfsAppLib_GetInstance()
#endif
#ifndef THIS_HINST
#   ifdef EXPORT
#      define THIS_HINST APPLIB_HINST
#   else
#      define THIS_HINST APP_HINST
#   endif
#endif

#ifndef cchNAME
#define cchNAME 256
#endif

#ifdef DBG
#ifndef DEBUG
#define DEBUG
#endif
#ifdef NDEBUG
#undef NDEBUG
#endif
#endif

#include <windows.h>
#include <windowsx.h>


/*
 * INCLUSIONS _________________________________________________________________
 *
 */

         // This library relies on the TaLocale suite to provide
         // access to localized resources.
         //
#include <WINNT/TaLocale.h>

         // This library provides support for performing all operations
         // through a remote administration server. If you want to use
         // these capabilities AND you want to use the asc_* routines
         // from TaAfsAdmSvrClient.lib directly, you must include
         // <TaAfsAdmSvrClient.h> before including this header.
         //
#ifdef TAAFSADMSVRCLIENT_H
#ifndef TAAFSADMSVRINTERNAL_H
#ifndef TAAFSADMSVRCLIENTINTERNAL_H
#include <WINNT/al_admsvr.h>
#endif // TAAFSADMSVRCLIENTINTERNAL_H
#endif // TAAFSADMSVRINTERNAL_H
#endif // TAAFSADMSVRCLIENT_H

         // A few general-purpose AfsAppLib headers
         //
#include <WINNT/al_resource.h>
#include <WINNT/al_messages.h>

         // In addition to the prototypes you'll find in this header file,
         // the following source files provide many useful features--
         // take a few moments and examine their headers individually to
         // see what they can do for your application.
         //
         // Each header is associated with one source file, and each pair 
         // can easily (well, hopefully easily) be copied off to other 
         // applications.
         //
#include <WINNT/hashlist.h>	// general-purpose list management code
#include <WINNT/resize.h>	// window resizing functions
#include <WINNT/subclass.h>	// window subclass code
#include <WINNT/dialog.h>	// general window control routines
#include <WINNT/ctl_spinner.h>	// enhanced version of MSCTLS_UPDOWN
#include <WINNT/ctl_elapsed.h>	// elapsed-time entry window control
#include <WINNT/ctl_time.h>	// absolute-time entry window control
#include <WINNT/ctl_date.h>	// absolute-date entry window control
#include <WINNT/ctl_sockaddr.h>	// IP address entry window control
#include <WINNT/settings.h>	// version-controlled settings
#include <WINNT/checklist.h>	// checked-item listbox control
#include <WINNT/fastlist.h>	// fast treeview/listview replacement
#include <WINNT/al_wizard.h>	// easy wizard generation code
#include <WINNT/al_progress.h>	// easy threaded progress-dialog code
#include <WINNT/regexp.h>	// regular-expression pattern matching


/*
 * GENERAL ____________________________________________________________________
 *
 */

         // AfsAppLib_SetAppName
         // ...records the display name of your application, so AfsAppLib
         //    can add it to dialog boxes' titles.
         //
EXPORTED void AfsAppLib_SetAppName (LPTSTR pszName);
EXPORTED void AfsAppLib_GetAppName (LPTSTR pszName);

         // AfsAppLib_SetMainWindow
         // ...specifies which window represents your application; the library
         //    subclasses this window and uses it to ensure some tasks are
         //    performed by the UI thread.
         //
EXPORTED void AfsAppLib_SetMainWindow (HWND hMain);
EXPORTED HWND AfsAppLib_GetMainWindow (void);


/*
 * REMOTE ADMINISTRATION ______________________________________________________
 *
 */

         // AfsAppLib_OpenAdminServer
         // AfsAppLib_CloseAdminServer
         // ...enables the caller of the AfsAppLib library to indicate
         //    that AFS administrative functions should be performed by
         //    out-farming the actual operations onto an administrative
         //    server process, possibly running on another machine.
         //    By calling the OpenAdminServer() routine specifying a
         //    machine name or IP address, the AfsAppLib library will attempt
         //    to connect to an already-running administrative server on
         //    that machine; if successful, all further administrative
         //    tasks (until CloseAdminServer() is called) will be performed
         //    on that remote server. Similarly, by calling OpenAdminServer()
         //    but passing NULL as the address, the AfsAppLib library will
         //    use a separate administrative process running on the local
         //    computer, forking a new administrative server process if
         //    none is currently running now.
         //
EXPORTED BOOL AfsAppLib_OpenAdminServer (LPTSTR pszAddress = NULL, ULONG *pStatus = NULL);
EXPORTED void AfsAppLib_CloseAdminServer (void);

         // AfsAppLib_GetAdminServerClientID
         // ...every process which interacts with a remote administration
         //    server is assigned a client ID by that server, which the
         //    client process passes on each RPC to identify itself to the
         //    server.  If AfsAppLib_OpenAdminServer() has previously been
         //    called, the AfsAppLib_GetAdminServerClientID() routine can
         //    be used to obtain the client id which the administrative
         //    server has assigned to this process.
         //
EXPORTED DWORD AfsAppLib_GetAdminServerClientID (void);


/*
 * CELLLIST ___________________________________________________________________
 *
 */

         // AfsAppLib_GetCellList
         // ...obtains a list of cells:
         //    - if a registry path is supplied, that path is enumerated
         //      and the names of the keys below it returned as a cell list
         //    - if a registry path is not supplied, the list of cells
         //      contacted by the AFS client is returned.
         //    - if another cell list is supplied, that list is copied.
         //    The local cell will be in element 0.  The AfsAppLib_FreeCellList
         //    routine should be called when the CELLLIST structure is no
         //    longer needed.
         //
typedef struct // CELLLIST
   {
   LPTSTR *aCells;
   size_t nCells;
   HKEY hkBase;
   TCHAR szRegPath[ MAX_PATH ];
   } CELLLIST, *LPCELLLIST;

EXPORTED LPCELLLIST AfsAppLib_GetCellList (HKEY hkBase = NULL, LPTSTR pszRegPath = NULL);
EXPORTED LPCELLLIST AfsAppLib_GetCellList (LPCELLLIST lpcl);
EXPORTED void AfsAppLib_AddToCellList (LPCELLLIST lpcl, LPTSTR pszCell);
EXPORTED void AfsAppLib_FreeCellList (LPCELLLIST lpcl);


/*
 * BROWSE _____________________________________________________________________
 *
 */

         // AfsAppLib_ShowBrowseDialog
         // ...creates a modal dialog that lets the user select an AFS
         //    user or group
         //
typedef enum
   {
   btLOCAL_USER,
   btLOCAL_GROUP,
   btANY_USER,
   btANY_GROUP
   } BROWSETYPE;

typedef struct
   {
   HWND hParent;	// [in] Parent window for browse dialog
   TCHAR szCell[ cchNAME ];	// [inout] Cell name
   TCHAR szNamed[ cchNAME ];	// [inout] Selected principal
   BROWSETYPE bt;	// [in] type of prinicipals to show
   int idsTitle;	// [in] string ID for dialog title
   int idsPrompt;	// [in] string ID for Edit prompt
   int idsNone;	// [in] string ID for checkbox (or 0)
   LPCELLLIST lpcl;	// [in] from AfsAppLib_GetCellList()
   PVOID hCreds;	// [in] credentials for enumeration
   } BROWSEDLG_PARAMS, *LPBROWSEDLG_PARAMS;

EXPORTED BOOL AfsAppLib_ShowBrowseDialog (LPBROWSEDLG_PARAMS lpp);


         // AfsAppLib_ShowBrowseFilesetDialog
         // ...creates a modal dialog that lets the user select a fileset
         //
typedef struct
   {
   HWND hParent;	// [in] parent window for browse dialog
   TCHAR szCell[ cchNAME ];	// [inout] cell name
   TCHAR szFileset[ cchNAME ];	// [inout] selected fileset
   int idsTitle;	// [in] string ID for title (or 0)
   int idsPrompt;	// [in] string ID for prompt (or 0)
   LPCELLLIST lpcl;	// [in] NULL to disable cell selection
   PVOID pInternal;
   } BROWSESETDLG_PARAMS, *LPBROWSESETDLG_PARAMS;

EXPORTED BOOL AfsAppLib_ShowBrowseFilesetDialog (LPBROWSESETDLG_PARAMS lpp);


/*
 * COVER ______________________________________________________________________
 *
 */

         // AfsAppLib_CoverClient
         // AfsAppLib_CoverWindow
         // ...hides the specified window (or just its client area), showing
         //    instead a simple etched rectangle filled with the descriptive
         //    text that you supply. An optional button can be shown in the
         //    lower-right corner; when pressed, the parent of the covered
         //    window receives a WM_COMMAND/IDC_COVERBUTTON message.
         //
EXPORTED void AfsAppLib_CoverClient (HWND hWnd, LPTSTR pszDesc, LPTSTR pszButton = NULL);
EXPORTED void AfsAppLib_CoverWindow (HWND hWnd, LPTSTR pszDesc, LPTSTR pszButton = NULL);

         // AfsAppLib_Uncover
         // ...removes a cover (if any) from the specified window, re-showing
         //    the controls previously hidden underneath the cover.
         //
EXPORTED void AfsAppLib_Uncover (HWND hWnd);


/*
 * CREDENTIALS ________________________________________________________________
 *
 */

         // AfsAppLib_CrackCredentials
         // ...obtains information about the specified credentials cookie.
         //    returns TRUE if the data could be successfully parsed.
         //
EXPORTED BOOL AfsAppLib_CrackCredentials (PVOID hCreds, LPTSTR pszCell = NULL, LPTSTR pszUser = NULL, LPSYSTEMTIME pst = NULL, ULONG *pStatus = NULL);

         // AfsAppLib_GetCredentials
         // ...returns nonzero if the calling process has AFS credentials within
         //    the specified cell.  Specify NULL as the cell ID to query
         //    credentials within the local cell. The return code is actually
         //    a token handle which can be supplied to the AFS administrative
         //    functions.
         //
EXPORTED PVOID AfsAppLib_GetCredentials (LPCTSTR pszCell = NULL, ULONG *pStatus = NULL);

         // AfsAppLib_SetCredentials
         // ...obtains new credentials for the calling process; performs no UI.
         //    If successful, returns a nonzero token handle which can be
         //    supplied to the AFS administrative functions.
         //
EXPORTED PVOID AfsAppLib_SetCredentials (LPCTSTR pszCell, LPCTSTR pszUser, LPCTSTR pszPassword, ULONG *pStatus = NULL);

         // AfsAppLib_IsUserAdmin
         // ...queries the KAS database for a particular cell to determine
         //    whether the specified identity has administrative privileges.
         //    The hCreds passed in should be for an admin, or for the user
         //    being queried.
         //
EXPORTED BOOL AfsAppLib_IsUserAdmin (PVOID hCreds, LPTSTR pszUser);

         // AfsAppLib_ShowOpenCellDialog
         // ...presents a dialog which allows the user to select a cell,
         //    and optionally obtain new AFS credentials within that cell.
         //    the caller may optionally specify an alternate dialog template;
         //    if not, a default template will be used. the caller may also
         //    optionally supply a DLGPROC, which will be called to handle all
         //    dialog messages--if this hook returns FALSE, the default handler
         //    will be called; if it returns TRUE, no further processing for
         //    that message is performed.
         //
typedef struct
   {
   int idd;	// [in] dialog template (or 0)
   DLGPROC hookproc;	// [in] dialog procedure (or NULL)
   HWND hParent;	// [in] parent window (or NULL)
   int idsDesc;	// [in] string ID for description
   BOOL *pfShowWarningEver;	// [in] "don't ask again" checkbox
   } BADCREDSDLG_PARAMS, *LPBADCREDSDLG_PARAMS;

typedef struct
   {
   int idd;	// [in] dialog template (or 0)
   DLGPROC hookproc;	// [in] dialog procedure (or NULL)
   HWND hParent;	// [in] parent window (or NULL)
   int idsDesc;	// [in] string ID for dialog text
   LPCELLLIST lpcl;	// [in] from AfsAppLib_GetCellList()
   BADCREDSDLG_PARAMS bcdp;	// [in] params for bad creds dialog
   TCHAR szCell[ cchNAME ];	// [out] selected cell
   PVOID hCreds;	// [out] credentials in cell
   } OPENCELLDLG_PARAMS, *LPOPENCELLDLG_PARAMS;

EXPORTED BOOL AfsAppLib_ShowOpenCellDialog (LPOPENCELLDLG_PARAMS lpp);

         // AfsAppLib_ShowCredentialsDialog
         // ...presents a dialog which displays the current AFS credentials
         //    and allows the user to obtain new credentials. An alternate
         //    dialog template and hook procedure can be specified.
         //
typedef struct
   {
   int idd;	// [in] dialog template (or 0)
   DLGPROC hookproc;	// [in] dialog procedure (or NULL)
   HWND hParent;	// [in] parent window (or NULL)
   TCHAR szCell[ MAX_PATH ];	// [in out] current cell
   BOOL fIgnoreBadCreds;	// [in] TRUE to skip bad creds dialog
   BADCREDSDLG_PARAMS bcdp;	// [in] params for bad creds dialog
   TCHAR szIdentity[ cchNAME ];	// [out] current DCE identity in szCell
   TCHAR szPassword[ cchNAME ];	// [out] password entered (or "")
   PVOID hCreds;	// [in out] credentials in cell
   } CREDENTIALSDLG_PARAMS, *LPCREDENTIALSDLG_PARAMS;

EXPORTED BOOL AfsAppLib_ShowCredentialsDialog (LPCREDENTIALSDLG_PARAMS lpp);

         // AfsAppLib_CheckCredentials
         // ...tests the current credentials to see if they represent
         //    a user with administrative access within the target cell.
         //
typedef struct
   {
   PVOID hCreds;	// [in] credentials to query
   BOOL fShowWarning;	// [in] TRUE to present warning dialog
   BADCREDSDLG_PARAMS bcdp;	// [in] params for bad creds dialog
   } CHECKCREDS_PARAMS, *LPCHECKCREDS_PARAMS;

EXPORTED BOOL AfsAppLib_CheckCredentials (LPCHECKCREDS_PARAMS lpp);

         // AfsAppLib_CheckForExpiredCredentials
         // ...tests the user's credentials in the specified cell to see if
         //    they have expired; if so, calls AfsAppLib_CheckCredentials to
         //    display a warning dialog--if the user accepts the warning
         //    dialog, calls AfsAppLib_ShowCredentialsDialog. All that UI
         //    is modeless; this routine returns immediately.
         //
EXPORTED void AfsAppLib_CheckForExpiredCredentials (LPCREDENTIALSDLG_PARAMS lpp);


/*
 * TASKING ____________________________________________________________________
 *
 */

typedef struct
   {
   int idTask;	// task ID requested
   HWND hReply;	// window to which to reply
   PVOID lpUser;	// parameter supplied by user
   BOOL rc;	// TRUE if successful
   ULONG status;	// if (rc == FALSE), error code
   PVOID pReturn;	// allocated storage for return data
   } TASKPACKET, *LPTASKPACKET;

         // AfsAppLib_InitTaskQueue
         // ...allows use of StartTask to perform background tasks.
         //    call this routine only once per application, and only
         //    if you want to use StartTask().
         //
typedef struct
   {
   LPTASKPACKET (*fnCreateTaskPacket)(int idTask, HWND hReply, PVOID lpUser);
   void (*fnPerformTask)(LPTASKPACKET ptp);
   void (*fnFreeTaskPacket)(LPTASKPACKET ptp);
   int nThreadsMax;
   } TASKQUEUE_PARAMS, *LPTASKQUEUE_PARAMS;

EXPORTED void AfsAppLib_InitTaskQueue (LPTASKQUEUE_PARAMS lpp);

         // StartTask
         // ...pushes a request onto the task queue created by a previous
         //    call to AfsAppLib_InitTaskQueue. A background thread will
         //    be allocated to popping off the request and performing it,
         //    optionally posting a WM_ENDTASK message to the specified
         //    window upon completion.
         //
EXPORTED void StartTask (int idTask, HWND hReply = 0, PVOID lpUser = 0);


/*
 * ERROR DIALOGS ______________________________________________________________
 *
 */

         // ErrorDialog
         // ...creates a modeless error dialog containing the specified
         //    error text. if a non-zero status code is specified, the
         //    error dialog will also translate that error code and
         //    display its description. These routines create a modeless
         //    dialog, and can be called by any thread.
         //
EXPORTED void cdecl ErrorDialog (DWORD dwStatus, LPTSTR pszError, LPTSTR pszFmt = NULL, ...);
EXPORTED void cdecl ErrorDialog (DWORD dwStatus, int idsError, LPTSTR pszFmt = NULL, ...);

         // FatalErrorDialog
         // ...identical to ErrorDialog, except that PostQuitMessage()
         //    is performed after the error dialog is dismissed.
         //
EXPORTED void cdecl FatalErrorDialog (DWORD dwStatus, LPTSTR pszError, LPTSTR pszFmt = NULL, ...);
EXPORTED void cdecl FatalErrorDialog (DWORD dwStatus, int idsError, LPTSTR pszFmt = NULL, ...);

         // ImmediateErrorDialog
         // ...identical to ErrorDialog, except that the error dialog
         //    is modal.
         //
EXPORTED void cdecl ImmediateErrorDialog (DWORD dwStatus, LPTSTR pszError, LPTSTR pszFmt = NULL, ...);
EXPORTED void cdecl ImmediateErrorDialog (DWORD dwStatus, int idsError, LPTSTR pszFmt = NULL, ...);


/*
 * MODELESS DIALOGS ___________________________________________________________
 *
 * Modeless dialogs aren't treated as dialogs unless you call IsDialogMessage()
 * for them in your pump--that is, tab won't work, nor will RETURN or ESC do
 * what you expect, nor will hotkeys.  The routines below allow you to specify
 * that a given window should be treated as a modeless dialog--by calling one
 * routine in your main pump, you can ensure all such modeless dialogs will
 * have IsDialogMessage called for them:
 *
 *    MSG msg;
 *    while (GetMessage (&msg, NULL, 0, 0))
 *       {
 * ->    if (AfsAppLib_IsModelessDialogMessage (&msg))
 * ->       continue;
 *
 *       TranslateMessage (&msg);
 *       DispatchMessage (&msg);
 *       }
 * 
 * An equivalent technique is:
 *
 *    AfsAppLib_MainPump();
 *
 * ...the MainPump() routine does exactly what the first example did.
 *
 */

         // AfsAppLib_IsModelessDialogMessage
         // ...called from your applications pump, this routine
         //    calls IsDialogMessage() for each window which
         //    has been registered by AfsAppLib_RegisterModelessDialog.
         //
EXPORTED BOOL AfsAppLib_IsModelessDialogMessage (MSG *lpm);

         // AfsAppLib_RegisterModelessDialog
         // ...ensures that AfsAppLib_IsModelessDialogMessage will
         //    call IsDialogMessage() for the specified window
         //
EXPORTED void AfsAppLib_RegisterModelessDialog (HWND hDlg);

         // AfsAppLib_SetPumpRoutine
         // ...can be used to specify a routine which will be called for
         //    your application to process every message--if not done,
         //    AfsAppLib_MainPump and ModalDialog will instead simply
         //    call TranslateMessage/DispatchMessage.
         //
EXPORTED void AfsAppLib_SetPumpRoutine (void (*fnPump)(MSG *lpm));

         // AfsAppLib_MainPump
         // ...Calls GetMessage() until the WM_QUIT message is received;
         //    for each message, calls AfsAppLib_IsModelessDialogMessage
         //    followed by the pump routine which was specified by
         //    AfsAppLib_SetPumpRoutine.
         //
EXPORTED void AfsAppLib_MainPump (void);


/*
 * WINDOW DATA ________________________________________________________________
 *
 * Ever been frustrated by Get/SetWindowLong's inability to dynamically
 * grow the space associated with a window? Worse, how do you know
 * that GetWindowLong(hWnd,3) isn't already used by some other routine?
 *
 * The routines below solve both problems: by specifying indices by a
 * descriptive name rather than an integer index, collisions are eliminated.
 * And as new fields are specified, they're dynamically added--the data-space
 * associated with a window grows indefinitely as needed.
 *
 * Note that GetWindowData will return 0 unless SetWindowData
 * has been used to explicitly change that field for that window.
 *
 */

         // GetWindowData
         // SetWindowData
         // ...Alternatives to GetWindowLong and SetWindowLong; these
         //    routines use descriptive field indices rather than
         //    the integer indices used by the Get/SetWindowLong routines,
         //    and can grow windows' data-space as necessary.
         //
EXPORTED DWORD GetWindowData (HWND hWnd, LPTSTR pszField);
EXPORTED DWORD SetWindowData (HWND hWnd, LPTSTR pszField, DWORD dwNewData);


/*
 * IMAGE LISTS ________________________________________________________________
 *
 */

         // AfsAppLib_CreateImageList
         // ...creates an initial IMAGELIST, containing several
         //    AFS-specific icons (which are included in this library's
         //    resources). fLarge indicates whether the imagelist should
         //    be composed of 32x32 or 16x16 icons; the highest visible
         //    color depth image will automatically be selected for each icon.
         //
#define imageSERVER           0
#define imageSERVER_ALERT     1
#define imageSERVER_UNMON     2
#define imageSERVICE          3
#define imageSERVICE_ALERT    4
#define imageSERVICE_STOPPED  5
#define imageAGGREGATE        6
#define imageAGGREGATE_ALERT  7
#define imageFILESET          8
#define imageFILESET_ALERT    9
#define imageFILESET_LOCKED  10
#define imageBOSSERVICE      11
#define imageCELL            12
#define imageSERVERKEY       13
#define imageUSER            14
#define imageGROUP           15

#define imageNEXT            16  // next ID given by AfsAppLib_AddToImageList()

EXPORTED HIMAGELIST AfsAppLib_CreateImageList (BOOL fLarge);

         // AfsAppLib_AddToImageList
         // ...easy wrapper for adding additional icons to an image list.
         //
EXPORTED void AfsAppLib_AddToImageList (HIMAGELIST hil, int idi, BOOL fLarge);


/*
 * HELP _______________________________________________________________________
 *
 * These routines make implementing context-sensitive help fairly easy.
 * To use them, your application should contain the following code at startup
 * for each dialog in your application:
 *
 *    static DWORD IDH_MAIN_HELP_CTX[] = {
 *       IDC_BUTTON, IDH_BUTTON_HELP_ID,
 *       IDC_EDIT,   IDH_EDIT_HELP_ID,
 *       0, 0
 *    };
 *
 *    AfsAppLib_RegisterHelp (IDD_MAIN, IDH_MAIN_HELP_CTX, IDH_MAIN_HELP_OVIEW);
 *
 * You'll also have to call one other routine, at least once:
 *
 *    AfsAppLib_RegisterHelpFile (TEXT("myapp.hlp"));
 *
 * Within IDD_MAIN's dialog proc, call the following:
 *
 *    BOOL CALLBACK IddMain_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
 *    {
 *       if (AfsAppLib_HandleHelp (IDD_MAIN, hDlg, msg, wp, lp))
 *          return TRUE;
 *       ...
 *    }
 *
 * That's it--context help will work for the dialog, and if you've attached
 * a "Help" button (which should be IDHELP==9), it will conjure an overall
 * help topic.
 *
 */

         // AfsAppLib_RegisterHelpFile
         // ...specifies the help file that the library should use to display
         //    help topics.
         //
EXPORTED void AfsAppLib_RegisterHelpFile (LPTSTR pszFilename);

         // AfsAppLib_RegisterHelp
         // ...adds another dialog to the library's list of CSH-enabled
         //    dialogs. Dialogs are referenced by their resource ID--that's
         //    what the "idd" thing is.
         //
EXPORTED void AfsAppLib_RegisterHelp (int idd, DWORD *adwContext, int idhOverview);

         // AfsAppLib_HandleHelp
         // ...handles CSH messages and IDHELP button presses; returns TRUE
         //    if the given message has been handled and needs no further
         //    processing.
         //
EXPORTED BOOL AfsAppLib_HandleHelp (int idd, HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);


/*
 * GENERAL ____________________________________________________________________
 *
 */

         // AfsApplib_CreateFont
         // ...loads a string resource and uses its content to create a
         //    font. The string resource should be of the form:
         //       "TypeFace,Size,Flags"--
         //    where:
         //       TypeFace is "MS Sans Serif", "Times New Roman", or other font
         //       Size is the point-size of the font (no decimals!)
         //       Flags is combination of B, I, U (bold, italicized, underlined)
         //
         //    Some examples, then:
         //       IDS_BIG_FONT     "Times New Roman,20,BU"
         //       IDS_LITTLE_FONT  "Arial,8,I"
         //       IDS_NORMAL_FONT  "MS Sans Serif,10"
         //
         //    The returned font handle should be destroyed with DeleteObject()
         //    when no longer needed.
         //
EXPORTED HFONT AfsAppLib_CreateFont (int idsFont);

         // AfsAppLib_GetInstance
         // ...returns the handle representing this library, for use in
         //    loading resources.
         //
EXPORTED HINSTANCE AfsAppLib_GetInstance (void);

         // AfsAppLib_GetAppInstance
         // ...returns the handle representing the application which
         //    loaded this library. If a .DLL loaded this library,
         //    you may need to call AfsAppLib_SetAppInstance().
         //
EXPORTED HINSTANCE AfsAppLib_GetAppInstance (void);
EXPORTED void AfsAppLib_SetAppInstance (HINSTANCE hInst);

         // AfsAppLib_AnimateIcon
         // ...used to animate the 8-frame spinning Transarc logo.
         //    specify NULL for piFrameLast to stop the spinning,
         //    otherwise, provide a pointer to an int that will be used
         //    to track the current frame.
         //
EXPORTED void AfsAppLib_AnimateIcon (HWND hIcon, int *piFrameLast = NULL);

         // AfsAppLib_StartAnimation
         // AfsAppLib_StopAnimation
         // ...an alternate technique for calling AfsAppLib_AnimateIcon;
         //    using these routines causes a timer to be created which will
         //    animate the Transarc logo within the specified window.
         //
EXPORTED void AfsAppLib_StartAnimation (HWND hIcon, int fps = 8);
EXPORTED void AfsAppLib_StopAnimation (HWND hIcon);

         // AfsAppLib_IsTimeInFuture
         // ...returns TRUE if the specified time (GMT) is in the
         //    future; handy for checking credentials' expiration dates.
         //
EXPORTED BOOL AfsAppLib_IsTimeInFuture (LPSYSTEMTIME pstTest);

         // AfsAppLib_UnixTimeToSystemTime
         // ...translate a unix time DWORD into a SYSTEMTIME structure
         //
EXPORTED void AfsAppLib_UnixTimeToSystemTime (LPSYSTEMTIME pst, ULONG ut, BOOL fElapsed = FALSE);

         // AfsAppLib_TranslateError
         // ...obtains descriptive text for the given status code.
         //    This routine is just a wrapper around TaLocale's
         //    FormatError(); either routine produces identical output.
         //    Both routines call back into a hook installed by the
         //    AfsAppLib library to provide AFS-specific error translation..
         //    If successful, the resulting string will appear as:
         //       "Unable to create an RPC binding to host. (0x0E008001)"
         //    If unsuccessful, the resulting string will appear as:
         //       "0x0E008001"
         //
EXPORTED BOOL AfsAppLib_TranslateError (LPTSTR pszText, ULONG status, LANGID idLanguage = 0);

         // AfsAppLib_GetLocalCell
         // ...returns the local cell into which this machine is configured.
         //
EXPORTED BOOL AfsAppLib_GetLocalCell (LPTSTR pszCell, ULONG *pStatus = NULL);

         // REALLOC
         // ...general-purpose array reallocator:
         //       int *aInts = NULL;
         //       size_t cInts = 0;
         //       if (!REALLOC (aInts, cInts, 128, 16)) { ... }
         //    the code above ensures that there will be 128 ints available
         //    as iInts[0]..aInts[127]; the 16 indicates the minimum allocation
         //    granularity (i.e., asking for 129 ints will actually get you
         //    144 of them). Reallocation only increases array size, and is
         //    only performed when necessary; newly-allocated space is zero-
         //    initialized, and old data is always retained across reallocation.
         //    Use GlobalFree((HGLOBAL)aInts) to free the array when done.
         //    Use it a few times, and you'll be hooked: amazingly useful.
         //
#ifndef REALLOC
#define REALLOC(_a,_c,_r,_i) AfsAppLib_ReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
EXPORTED BOOL AfsAppLib_ReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc);
#endif


#endif

