/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * INCLUDES _________________________________________________________________
 *
 */

#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "afscfg.h"
#include "resource.h"
#include "graphics.h"
#include <string.h>
#include "get_cur_config.h"
#include "partition_utils.h"
extern "C" {
#include <afs\dirpath.h>
#include <afs\afs_clientAdmin.h>
}
#include <lanahelper.h>


/*
 * DEFINITIONS _________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static void SetConfigDefaults();
static void RunCfgTool();
static void RunWizard();
static void CloseLibHandles(BOOL bExiting = FALSE);

void RegisterWizardHelp();
void RegisterConfigToolHelp();

// These are the prototypes for the dialog procs of each wizard page.
BOOL CALLBACK IntroPageDlgProc(HWND hRHS, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK InfoPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK InfoPage2DlgProc(HWND hRHS, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK FileServerPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK DBServerPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK BackupPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK PartitionPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK RootAfsPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK ReplicationPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK SysControlPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL CALLBACK ConfigServerPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp);


static WIZARD_STATE g_aStates[] = {
    { sidSTEP_ONE,		IDD_INTRO_PAGE,					(DLGPROC)IntroPageDlgProc, 0 },
    { sidSTEP_TWO,		IDD_INFO_PAGE,					(DLGPROC)InfoPageDlgProc, 0 },
    { sidSTEP_THREE,	IDD_INFO_PAGE2_FIRST_SERVER,	(DLGPROC)InfoPage2DlgProc, 0 },
	{ sidSTEP_FOUR,		IDD_INFO_PAGE2_NOT_FIRST_SERVER,(DLGPROC)InfoPage2DlgProc, 0 },
    { sidSTEP_FIVE,		IDD_FILE_SERVER_PAGE,			(DLGPROC)FileServerPageDlgProc, 0 },
    { sidSTEP_SIX,		IDD_DB_SERVER_PAGE,				(DLGPROC)DBServerPageDlgProc, 0 },
	{ sidSTEP_SEVEN,	IDD_BACKUP_SERVER_PAGE,			(DLGPROC)BackupPageDlgProc, 0 },
    { sidSTEP_EIGHT,	IDD_PARTITION_PAGE,				(DLGPROC)PartitionPageDlgProc, 0 },
	{ sidSTEP_NINE,		IDD_ROOT_VOLUMES_PAGE,			(DLGPROC)RootAfsPageDlgProc, 0 },
	{ sidSTEP_TEN,		IDD_REPLICATION_PAGE,			(DLGPROC)ReplicationPageDlgProc, 0 },
	{ sidSTEP_ELEVEN,	IDD_SYS_CONTROL_PAGE,			(DLGPROC)SysControlPageDlgProc, 0 },
	{ sidSTEP_TWELVE,	IDD_CONFIG_SERVER_PAGE,			(DLGPROC)ConfigServerPageDlgProc, 0 }
};

size_t g_nNumStates = sizeof(g_aStates) / sizeof(g_aStates[0]);


// Res ID's of text descriptions of each state
UINT g_StateDesc[] = {
	IDS_INTRO_PAGE,
	IDS_INFO_PAGE,
	IDS_INFO_PAGE2,
	IDS_INFO_PAGE2,
	IDS_FS_PAGE,
	IDS_DB_PAGE,
	IDS_BK_PAGE,
	IDS_PARTITION_PAGE,
	IDS_ROOT_AFS_PAGE,
	IDS_REP_PAGE,
	IDS_SC_PAGE,
	IDS_CONFIG_PAGE
};



/*
 * EXPORTED VARIABLES _________________________________________________________________
 *
 */
LPWIZARD g_pWiz = NULL;
LPPROPSHEET g_pSheet = NULL;

CFG_DATA g_CfgData;

void *g_hToken = 0;
void *g_hCell = 0;
void *g_hClient = 0;
void *g_hServer = 0;

static void *hClientCell = 0;

LOGFILE g_LogFile;



/*
 * EXPORTED FUNCTIONS _________________________________________________________________
 *
 */
extern "C" int WINAPI WinMain (HINSTANCE hInst, HINSTANCE hPrev, LPSTR pszCmdLineA, int nCmdShow)
{
	afs_status_t nStatus;

	TaLocale_LoadCorrespondingModule (hInst);

	// Tell the applib our application's name
	TCHAR szAppName[cchRESOURCE];
	AfsAppLib_SetAppName(GetResString(GetAppTitleID(), szAppName));

	// Open the admin libraries
	int nResult = afsclient_Init(&nStatus);
	if (!nResult) {
		ShowError(0, nStatus, IDS_CANT_INIT_ADMIN_LIBS);
		return 0;
	}

	// Open the log file
	char szLogPath[MAX_PATH];
	sprintf(szLogPath, "%s\\%s", AFSDIR_SERVER_LOGS_DIRPATH, LOG_FILE_NAME);
	if (!g_LogFile.Open(szLogPath))
		ShowError(0, 0, IDS_CANT_OPEN_LOG_FILE);

	// Register the help file with the applib's help system
	AfsAppLib_RegisterHelpFile(TEXT("TaAfsCfg.hlp"));

    /* Start up sockets */
    WSADATA WSAjunk;
    WSAStartup(0x0101, &WSAjunk);

	memset(&g_CfgData, 0, sizeof(CFG_DATA));

	// Get this machine's local name
	char szLocalName[sizeof(g_CfgData.szLocalName) / sizeof(TCHAR)];
    if (gethostname(szLocalName, sizeof(szLocalName)) != 0) {
   		ShowError(GetFocus(), WSAGetLastError(), IDS_ERROR_LOCAL_HOST_NAME);
		return 0;
	}
    CopyAnsiToString(g_CfgData.szLocalName, szLocalName);

	// Turn the local name into a host name
    struct hostent *pHostEnt = gethostbyname(szLocalName);
    if (!pHostEnt) {
   		ShowError(GetFocus(), WSAGetLastError(), IDS_ERROR_HOST_NAME);
		return 0;
	}
    CopyAnsiToString(g_CfgData.szHostname, pHostEnt->h_name, sizeof(g_CfgData.szHostname));

	RegisterFastListClass();

	// Get current config status
	BOOL bCanceled = FALSE;
	DWORD dwStatus = GetCurrentConfig(NULL, bCanceled);
	if (dwStatus || bCanceled) {
		if (!bCanceled)
       		ShowError(GetFocus(), dwStatus, IDS_CONFIG_CHECK_FAILED);
		return 0;
	}

	// Run the appropriate interface
	if ((strstr(_strlwr(pszCmdLineA), "wizard") != 0))
		RunWizard();
	else
		RunCfgTool();

	FreePartitionTable();

	// Disconnect from the config and admin libraries
	CloseLibHandles(TRUE);

	return 0;
}

/*
 *	This is a dialog proc that is shared by all of the wizard pages.  It
 *	handles things that are common to all of them.  Each page also has its
 *	own dialog proc.
 */
BOOL CALLBACK WizStep_Common_DlgProc(HWND hRHS, UINT msg, WPARAM wp, LPARAM lp)
{
	// Get the dialog's resource ID
	int nIDD = GetWindowLong(hRHS, GWL_ID);
	
	if (AfsAppLib_HandleHelp(nIDD, hRHS, msg, wp, lp))
		return TRUE;
	
	switch (msg) {
		case WM_INITDIALOG:	MakeBold(hRHS, IDC_TITLE);
							RedrawGraphic();
							break;

		case WM_COMMAND:
			switch (LOWORD(wp)) {
				case IDCANCEL:
					QueryCancelWiz();
					return TRUE;
			}
			break;
	}

	return FALSE;
}

BOOL QueryCancelWiz()
{
	int nChoice = Message(MB_YESNO, IDS_WIZARD_APP_TITLE, IDS_CANCEL_DESC);
	if (nChoice == IDYES) {
		g_LogFile.Write("User has chosen to cancel the program.\r\n");
		if (g_pWiz)
			g_pWiz->Show(FALSE);
		return TRUE;
	}

	return FALSE;
}


/*
 *  Accessor functions for the g_CfgData variable.  There are versions
 *  for both TCHARs and char *'s.
 */
TCHAR GetDeviceName()           { return g_CfgData.chDeviceName; }
LPTSTR GetPartitionName()       { return g_CfgData.szPartitionName; }
LPTSTR GetHostName()            { return g_CfgData.szHostname; }
LPTSTR GetSysControlMachine()   { return g_CfgData.szSysControlMachine; }
LPTSTR GetCellName()            { return g_CfgData.szCellName; }
LPTSTR GetServerPW()            { return g_CfgData.szServerPW; }
LPTSTR GetAdminName()           { return g_CfgData.szAdminName; }
LPTSTR GetAdminPW()             { return g_CfgData.szAdminPW; }
LPTSTR GetAdminUID()            { return g_CfgData.szAdminUID; }
LPTSTR GetLocalName()           { return g_CfgData.szLocalName; }
LPTSTR GetHostname()            { return g_CfgData.szHostname; }
LPTSTR GetCellServDbHostname()  { return g_CfgData.szCellServDbHostname; }
LPTSTR GetClientCellName()      { return g_CfgData.szClientCellName; }
LPTSTR GetSalvageLogFileName()  { return g_CfgData.szSalvageLogFileName; }

LPTSTR GetClientNetbiosName()
{
    static TCHAR szValue[MAX_MACHINE_NAME_LEN + 1] = "";

	if ( szValue[0] == 0 )
		CopyAnsiToString(GetClientNetbiosNameA(), szValue);

    return szValue;
}

char GetDeviceNameA()
{
    static char szValueA[2];

    TCHAR devName[2] = TEXT("X");
    devName[0] = g_CfgData.chDeviceName;

    CopyStringToAnsi(szValueA, devName);

    return szValueA[0];
}

char *GetPartitionNameA()
{
    static char szValueA[MAX_PARTITION_NAME_LEN + 1];

    CopyStringToAnsi(szValueA, g_CfgData.szPartitionName);

    return szValueA;
}

char *GetSysControlMachineA()
{
    static char szValueA[MAX_MACHINE_NAME_LEN + 1];

    CopyStringToAnsi(szValueA, g_CfgData.szSysControlMachine);

    return szValueA;
}

char *GetCellNameA()
{
    static char szValueA[MAX_CELL_NAME_LEN + 1];

    CopyStringToAnsi(szValueA, g_CfgData.szCellName);

    return szValueA;
}

char *GetServerPWA()
{
    static char szValueA[MAX_SERVER_PW_LEN + 1];

    CopyStringToAnsi(szValueA, g_CfgData.szServerPW);

    return szValueA;
}

char *GetAdminNameA()
{
    static char szValueA[MAX_ADMIN_NAME_LEN + 1];

    CopyStringToAnsi(szValueA, g_CfgData.szAdminName);

    return szValueA;
}

char *GetAdminPWA()
{
    static char szValueA[MAX_ADMIN_PW_LEN + 1];

    CopyStringToAnsi(szValueA, g_CfgData.szAdminPW);

    return szValueA;
}

char *GetAdminUIDA()
{
    static char szValueA[MAX_UID_LEN + 1];

    CopyStringToAnsi(szValueA, g_CfgData.szAdminUID);

    return szValueA;
}

char *GetLocalNameA()
{
    static char szValueA[MAX_MACHINE_NAME_LEN + 1];

    CopyStringToAnsi(szValueA, g_CfgData.szLocalName);

    return szValueA;
}

char *GetHostnameA()
{
    static char szValueA[MAX_MACHINE_NAME_LEN + 1];

    CopyStringToAnsi(szValueA, g_CfgData.szHostname);

    return szValueA;
}

char *GetCellServDbHostnameA()
{
    static char szValueA[MAX_MACHINE_NAME_LEN + 1];

    CopyStringToAnsi(szValueA, g_CfgData.szCellServDbHostname);

    return szValueA;
}

char *GetClientCellNameA()
{
    static char szValueA[MAX_CELL_NAME_LEN + 1];

    CopyStringToAnsi(szValueA, g_CfgData.szClientCellName);

    return szValueA;
}

char *GetClientNetbiosNameA()
{
    static char szValueA[MAX_MACHINE_NAME_LEN + 1]="";
	
	if ( szValueA[0] == 0 )
        lana_GetNetbiosName(szValueA, LANA_NETBIOS_NAME_FULL);

	return szValueA;
}

char *GetSalvageLogFileNameA()
{
    static char szValueA[_MAX_PATH];

    CopyStringToAnsi(szValueA, g_CfgData.szSalvageLogFileName);

    return szValueA;
}


BOOL GetLibHandles(afs_status_t *pStatus)
{
	ASSERT(g_CfgData.szHostname[0]);

	int nResult;
	
	// ************************* NOTE ********************************
	// * You MUST have already determined whether or not the host
	// * and client config info is valid before calling this function.
	// ***************************************************************

	// This function can be called at any time to get handles to the cell and
	// the config library.  It will try to get the most powerful handle to the
	// cell that it can, and then use that to open the config library.  If the
	// libraries are already open, it will close them first.  Two handles to
	// the config library will be opened, one to the server to be configured,
	// and one to the client machine we are configuring from.  
	
	// There are two types of cell handles, NULL and Standard.  A null handle
	// can make calls to any server except DB servers.  We need this primarily
	// to talk to the bos server to determine the machine's current configuration,
	// and to configure the client information if it is not already.  A standard 
	// handle can talk to any server.  Standard handles can be either authenticated 
	// or unauthenticated.

	// Close all current handles
	CloseLibHandles();

	g_LogFile.Write("Getting handles to the cell and the config library.\r\n");

    if (!hClientCell) {
    	// Start by getting a null cell handle and using it to open the client
    	// connection to the config library.
    	g_LogFile.Write("Opening a NULL cell handle to use with the client config library handle.\r\n");
    	nResult = afsclient_NullCellOpen(&hClientCell, pStatus);
    	if (!nResult) {
            g_LogFile.Write("Failed to open a NULL cell handle: %lx.\r\n", (unsigned long)*pStatus);
    		return FALSE;
    	}
    
    	// Get the client handle.  We never need a better handle than this
        // for the client, and so this handle will never be upgraded.
    	g_LogFile.Write("Getting config handle for the client.\r\n");
    	if (!cfg_HostOpen(hClientCell, GetHostnameA(), &g_hClient, pStatus)) {
            g_LogFile.Write("Failed to get config handle:  %lx.\r\n", (unsigned long)*pStatus);
    		return FALSE;
    	}
    }

	// Now we need to get the most powerful cell handle that we can	and use it
	// to open the config library for the server.

	// If the client info is valid and we know what cell the server should be in,
	// and the client has that cell in its CellServDB, then we can get a Standard cell
	// handle.  However there is an exception: if this is the first server in the 
	// cell then there may not yet be an authentication server to talk to.  In that
	// case we use a null cell handle.
    if (g_CfgData.bValidClientInfo &&   // Client config is valid
        g_CfgData.szCellName[0] &&      // We have a cell name
        (!g_CfgData.bFirstServer || g_CfgData.bAuthServerRunning))  // Auth server is running
    {
		g_LogFile.Write("Opening a non-NULL cell handle to use with the server config library handle.\r\n");

        // Do we have the user info necessary to authenticate?
        BOOL bHaveUserInfo = g_CfgData.szAdminName[0] && g_CfgData.szAdminPW[0];

		// Open a standard cell handle.  szAdminName and szAdminPW will either be NULL, or
		// if they have been entered by the user, will be the admin name and password strings.
		if ((!g_CfgData.bFirstServer || g_CfgData.bAdminPrincipalCreated) && bHaveUserInfo) {
			g_LogFile.Write("Getting tokens in cell %s for user '%s'.\r\n", GetCellNameA(), GetAdminNameA());
			nResult = afsclient_TokenGetNew(GetCellNameA(), GetAdminNameA(), GetAdminPWA(), &g_hToken, pStatus);
		} else {
			g_LogFile.Write("Getting unauthenticated tokens for cell '%s'.\r\n", GetCellNameA());
			nResult = afsclient_TokenGetNew(GetCellNameA(), "", "", &g_hToken, pStatus);
		}

		if (!nResult) {
			g_LogFile.Write("Failed to get tokens for the specified cell:  %lx.\r\n", (unsigned long)*pStatus);
			return FALSE;
		}

		// If the admin name and password are NULL, then this will be an unauthenticated
		// connection to the cell.
		g_LogFile.Write("Getting cell handle for cell %s.\r\n", GetCellNameA());
		nResult = afsclient_CellOpen(GetCellNameA(), g_hToken, &g_hCell, pStatus);
		if (!nResult) {
            g_LogFile.Write("Failed to open the cell:  %lx.\r\n", (unsigned long)*pStatus);
			return FALSE;
		}
	} else {
       	g_LogFile.Write("Opening a NULL cell handle to use with the server config library handle.\r\n");
    	nResult = afsclient_NullCellOpen(&g_hCell, pStatus);
    	if (!nResult) {
            g_LogFile.Write("Failed to open a NULL cell handle:  %lx.\r\n", (unsigned long)*pStatus);
    		return FALSE;
    	}
    }

	// Get the server handle
	g_LogFile.Write("Getting config library handle for the server.\r\n");
	if (!cfg_HostOpen(g_hCell, GetHostnameA(), &g_hServer, pStatus)) {
        g_LogFile.Write("Failed to get config library handle for the server:  %lx.\r\n", (unsigned long)*pStatus);
		return FALSE;
	}

	return TRUE;
}

BOOL GetHandles(HWND hParentDlg)
{
    afs_status_t nStatus;

    if (!GetLibHandles(&nStatus)) {
        ShowError(hParentDlg, nStatus, IDS_GET_TOKENS_ERROR);
        g_CfgData.szAdminPW[0] = 0;
        return FALSE;
    }

    return TRUE;
}


/*
 * Static FUNCTIONS _________________________________________________________________
 *
 */
static void CloseLibHandles(BOOL bExiting)
{
     afs_status_t nStatus;

	// We will close them in the reverse order of their creation.

	if (g_hServer) {
		cfg_HostClose(g_hServer, &nStatus);
		g_hServer = 0;
	}

	if (g_hCell) {
		afsclient_CellClose(g_hCell, &nStatus);
		g_hCell = 0;
	}

	if (g_hToken) {
		afsclient_TokenClose(g_hToken, &nStatus);
		g_hToken = 0;
	}

    // Only close the client cfg and cell handles if we are exiting.
	if (bExiting) {
        if (g_hClient) {
    		cfg_HostClose(g_hClient, &nStatus);
	    	g_hClient = 0;
        }

    	if (hClientCell) {
    		afsclient_CellClose(hClientCell, &nStatus);
    		hClientCell = 0;
    	}
    }
}

static void SetConfigDefaults()
{
	if (g_CfgData.bWizard) {
		if (g_CfgData.configFS == CS_NULL)
			g_CfgData.configFS = CS_CONFIGURE;

		if (g_CfgData.configDB == CS_NULL)
			g_CfgData.configDB = CS_CONFIGURE;
		
		if (g_CfgData.configBak == CS_NULL)
			g_CfgData.configBak = CS_CONFIGURE;

		if (g_CfgData.configPartition == CS_NULL)
			g_CfgData.configPartition = CS_CONFIGURE;

		if (g_CfgData.configRootVolumes == CS_NULL)
			g_CfgData.configRootVolumes = CS_CONFIGURE;

		if (g_CfgData.configRep == CS_NULL)
			g_CfgData.configRep = CS_CONFIGURE;

		if (g_CfgData.configSCS == CS_NULL)
			g_CfgData.configSCS = CS_DONT_CONFIGURE;

		if (g_CfgData.configSCC == CS_NULL)
			g_CfgData.configSCC = CS_DONT_CONFIGURE;
	}

	lstrcpy(g_CfgData.szAdminName, TEXT("admin"));
	lstrcpy(g_CfgData.szAdminUID, TEXT("0"));

	g_CfgData.bUseNextUid = TRUE;
}

	
// Prototypes for each property page's dialog proc
BOOL CALLBACK PartitionsPageDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ServicesPageDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

static void RunCfgTool()
{
	// If the client info is invalid, then the config tool cannot run.  Inform the user and
	// ask if they want to run the wizard instead.
	if (!g_CfgData.bValidClientInfo) {
		int nChoice = MsgBox(0, IDS_NEED_CLIENT_INFO, GetAppTitleID(), MB_YESNO | MB_ICONSTOP);
		if (nChoice == IDYES)
			RunWizard();
		return;
	}

	// If the server info is invalid, then the config tool cannot run.  The Wizard must be run
	// to initially configure the server.  Inform the user and ask if they want to run the wizard instead.
	if (!g_CfgData.bValidServerInfo) {
		int nChoice = MsgBox(0, IDS_NEED_SERVER_INFO, GetAppTitleID(), MB_OKCANCEL | MB_ICONEXCLAMATION);
		if (nChoice == IDOK)
			RunWizard();
		return;
	}

	g_CfgData.bWizard = FALSE;

	SetConfigDefaults();

	RegisterConfigToolHelp();

	// Create the prop sheet
	g_pSheet = PropSheet_Create(IDS_CFG_TOOL_APP_TITLE, TRUE);
	
	// Add the pages
	PropSheet_AddTab(g_pSheet, IDS_PARTITIONS_PAGE_TITLE, IDD_PARTITIONS_PAGE, (DLGPROC)PartitionsPageDlgProc, 0, TRUE, TRUE);
	PropSheet_AddTab(g_pSheet, IDS_SERVICES_PAGE_TITLE, IDD_SERVICES_PAGE, (DLGPROC)ServicesPageDlgProc, 0, TRUE);

	// Let the user see it
	PropSheet_ShowModal(g_pSheet);
}

static void RunWizard()
{
	g_CfgData.bWizard = TRUE;

    SetConfigDefaults();

	RegisterWizardHelp();
	
	g_pWiz = new WIZARD;
	g_pWiz->SetDialogTemplate(IDD_WIZARD, IDC_WIZARD_LEFTPANE, IDC_WIZARD_RIGHTPANE, IDBACK, IDNEXT);
	g_pWiz->SetGraphic(IDB_GRAPHIC_16, IDB_GRAPHIC_256);
	g_pWiz->SetStates(g_aStates, g_nNumStates);
	g_pWiz->SetGraphicCallback(PaintPageGraphic);

	g_pWiz->SetState(sidSTEP_ONE);
	g_pWiz->Show();

	delete g_pWiz;

	g_pWiz = 0;
}

