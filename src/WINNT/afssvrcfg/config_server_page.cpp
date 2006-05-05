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
#include "cfg_utils.h"
extern "C" {
#include <afs\afs_vosAdmin.h>
#include <afs\afs_bosAdmin.h>
#include <afs\afs_clientAdmin.h>
#include <afs\volser.h>
#include <afs\dirpath.h>
}
#include "config.h"
#include "graphics.h"
#include "get_pw_dlg.h"
#include <stdio.h>
#include <io.h>
#include "get_cur_config.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg);
static void SetupConfigSteps();
static void OnConfig();
static DWORD WINAPI ConfigServer(LPVOID param);
static BOOL CheckCancel();
static void ShowCurrentStep(UINT uiMsgID);
static void ShowCurrentStep(TCHAR *pszMsg);
static void InitProgressBar();
static BOOL CheckResult(int nResult, int nStatus);
static BOOL VosOpenServer();
static void VosCloseServer();
static BOOL ConfigPartition();
static BOOL DefineCellForServer();
static BOOL DefineCellForClient();
static BOOL StartBosServer();
static BOOL StartAuthServer();
static BOOL CreatePrincipalAndKey();
static BOOL StartDbServers();
static BOOL CreateAdminPrincipal();
static BOOL StartFsVlAndSalvager();
static BOOL ConfigSCC();
static BOOL ConfigSCS();
static BOOL CreateRootAfs();
static BOOL StartClient();
static BOOL SetRootAcl();
static BOOL CreateRootCell();
static BOOL MountRootCellStandard();
static BOOL SetRootCellAcl();
static BOOL MountRootCellRW();
static BOOL Replicate();
static BOOL EnableAuthChecking();
static BOOL UpgradeLibHandles();
static BOOL RestartServers();
static BOOL AddToCellServDB();
static BOOL RemoveFromCellServDB();
static BOOL UpdateCellServDB(BOOL bAdding);
static BOOL RestartAllDbServers();
static BOOL UnconfigDB();
static BOOL UnconfigBak();
static BOOL UnconfigFS();
static BOOL UnconfigSCS();
static BOOL UnconfigSCC();
static BOOL PostConfig();
static void ShowConfigControls(BOOL bShow = TRUE);
static void UpdateConfigProgress(int nStepNum);
static void ShowTitle();
static void ViewLog();
static void ShowConfigFailedMsg();
static BOOL Unconfiguring();
static BOOL GetCellServDB(char **ppszCellServDB);
static BOOL FreeCellServDB();
static char *GetVicepName();
static void ShowViewLogButton();
static BOOL CreateRootAfsDriveMapping();
static BOOL StopClient();
static BOOL GetRootVolumeInfo();

BOOL CALLBACK ConfigServerPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp);


/*
 * DEFINITIONS _________________________________________________________________
 *
 */
#define	QUORUM_WAIT_TIMEOUT			3 * 60  			    // 3 minutes in seconds
#define ROOT_VOLUMES_QUOTA			5000				    // k bytes
#define	RX_TIMEOUT					15 * 1000			    // 15 seconds in milleseconds
#define CELLSERVDB_UPDATE_TIMEOUT	RX_TIMEOUT * 2
#define INVALID_PARTITION_ID		(UINT)VOLMAXPARTS + 1
#define CLIENT_START_TIMEOUT        2 * 60                  // 2 minutes in seconds
#define CLIENT_STOP_TIMEOUT         3 * 60                  // 2 minutes in seconds


// The code below is used in so many places that I decided to put it
// into macros so that the main line of the code would be easier to follow.
#define	CHECK_CANCEL	if (CheckCancel()) return FALSE
#define CHECK_RESULT	if (!CheckResult(m_nResult, m_nStatus)) return FALSE

#define IF_WIZ(x)	{ if (g_pWiz) (x); }

// Global variables
static HWND		m_hDlg =		0;	// Window handle of this dialog
static BOOL		m_bConfiguring =	FALSE;	// TRUE if configuring
static BOOL		m_bConfigured =		FALSE;	// TRUE if configuration was successful
static BOOL		m_bConfigFailed =	FALSE;	// TRUE if configuration failed
static BOOL		m_bCheckCancel =	FALSE;	// TRUE if user pressed cancel - we will ask for confirmation
static BOOL		m_bCancel =		FALSE;	// TRUE if user confirmed cancel - we will cancel configuration
static void*		m_hvosServer =		0;	// vos library server handle
static const int	MAX_STEPS =		34;	// Max number of config steps
static afs_status_t	m_nStatus;			// Error code if a cfg library function fails
static int		m_nResult;			// Boolean return code from cfg library calls
static int		m_nNumSteps;			// Number of config steps that will be performed
static BOOL		m_bDbServersRestarted;		// TRUE if all Database servers were restarted
static BOOL		m_bMustChangeClientCell;	// TRUE if client is in different cell than server
static HANDLE		m_hCellServDBUpdateEvent;
static LPCTSTR          m_pszCellServDBUpdateEventName = TEXT("CellServDBUpdateEvent");
static const int	m_CallBackID = 6;
static UINT		m_nPartitionID = INVALID_PARTITION_ID;
static BOOL		m_bCellServDbUpdateErr = FALSE;
static TCHAR		m_szCellServDbUpdateErrMsg[cchRESOURCE];
static LONG		m_nServerUpdates = 0;
static char *		m_pszCellDbHosts = 0;
static BOOL		m_bNoAuthMode = TRUE;
static char 		m_szVicepName[9];
static BOOL		m_bMustExit;
static BOOL		m_bCfgInfoInvalidated;
static BOOL		m_bUnconfiguringLastDBServer;
static BOOL             m_bClientTokensSet;
static BOOL             m_bRootAfsDriveMappingCreated;
static char             m_szDriveToMapTo[3];
static BOOL             m_bWeCreatedRootAfs;
static BOOL             m_bWeCreatedRootCell;

static CRITICAL_SECTION m_CritSec;

typedef BOOL (*STEP_FUNC)();				// All config functions have this signature


/*
 *	Structure that is used to form an array of all possible configuration steps.
 */
struct CONFIG_STEP {
	STEP_STATE eState;		// Where we are at in performing this step
	STEP_FUNC pFunc;		// Function used to perform this step
	UINT nDescCtrlID;		// Control ID of the static used to hold the steps description
	UINT nGraphicCtrlID;	// Control ID of the static that will display the graphic
	UINT nMsgID;			// Message to show when performing this step
	UINT nDescID;			// Step description to show
};


/*
 *	Structure that holds info about the static controls that show the step description
 *	and the graphic that corresponds to the step state.  There are 8 of these; they 
 *	are configured at runtime depending on what configuration steps are to be performed.
 *	These are assigned into the appropriate step structure above.  I could have just
 *	stuck an index from the array of these structures into the struct above, but decided
 *	to just reproduce the two fields for ease of use.
 */
struct STEP_GUI_INFO {
	UINT nDescCtrlID;		// Control ID of the static used to hold the steps description
	UINT nGraphicCtrlID;	// Control ID of the static that will display the graphic
};

static STEP_GUI_INFO StepGuiCtrlIDs[] = {
	{ IDC_STEP1, IDC_STEP1_GRAPHIC },
	{ IDC_STEP2, IDC_STEP2_GRAPHIC },
	{ IDC_STEP3, IDC_STEP3_GRAPHIC },
	{ IDC_STEP4, IDC_STEP4_GRAPHIC },
	{ IDC_STEP5, IDC_STEP5_GRAPHIC },
	{ IDC_STEP6, IDC_STEP6_GRAPHIC },
	{ IDC_STEP7, IDC_STEP7_GRAPHIC },
	{ IDC_STEP8, IDC_STEP8_GRAPHIC }
};


/*
 *	Each step that can possibly be performed is given an ID here.  Each
 *	ID corresponds to an index of a config step in the array below.  This
 *	enum MUST match the order of the steps in the STEPS arrray below.
 */
enum STEP_ID {
	SID_CONFIG_PARTITION,
	SID_DEFINE_CELL_FOR_SERVER,
	SID_DEFINE_CELL_FOR_CLIENT,
	SID_START_BOS,
	SID_START_AUTH,
	SID_CREATE_PRINCIPAL_AND_KEY,
	SID_START_DB,
	SID_START_DB_AND_BAK,
	SID_START_BAK,
	SID_CREATE_ADMIN_PRINCIPAL,
	SID_START_FS_VL_AND_SALVAGER,
	SID_CONFIG_SCC,
	SID_CONFIG_SCS,
	SID_CREATE_ROOT_AFS,
	SID_START_CLIENT,
	SID_SET_ROOT_ACL,
	SID_CREATE_ROOT_CELL,
	SID_MOUNT_ROOT_CELL_STANDARD,
	SID_SET_ROOT_CELL_ACL,
	SID_MOUNT_ROOT_CELL_RW,
	SID_REPLICATE,
	SID_ENABLE_AUTH_CHECKING,
	SID_RESTART_SERVERS,
	SID_ADD_TO_CELLSERVDB,
	SID_RESTART_ALL_DB_SERVERS,
	SID_VOS_OPEN_SERVER,
	SID_UNCONFIG_DB,
	SID_UNCONFIG_BAK,
	SID_UNCONFIG_FS,
	SID_UNCONFIG_SCS,
	SID_UNCONFIG_SCC,
	SID_POST_CONFIG,
	SID_GET_CREDENTIALS,
    SID_GET_ROOT_VOLUME_INFO
};

static CONFIG_STEP m_ConfigSteps[MAX_STEPS * 2];	// Filled with ID's of steps needed to perform configuration

/*
 *	This is our array of config steps.  There is one entry for each possible configuration
 *	step.  Not all steps will necessarily be performed; it depends on the current config of
 *	the machine and what the user chooses.  We prefill here the config function, the res
 *	string to show for the step status message, and if applicable, the res string to show
 *	for the step description.  All steps have a status message that is shown right above the
 *	progress bar during configuration; only certain steps have a description.  These are the
 *	ones that show up in the list of config steps on the dialog and have a state graphic next
 *	to them.  All other values here are changed to their actual value at runtime.  The order
 *	the steps appear here is NOT necessarily the order they will be performed in.  That order
 *	depends on exactly how the server is being configured.
 */
static CONFIG_STEP STEPS[MAX_STEPS] = {
	{ SS_STEP_TO_BE_DONE, ConfigPartition,		0, 0, IDS_PARTITION_STEP,					IDS_PARTITION_STEP_DESC },
	{ SS_STEP_TO_BE_DONE, DefineCellForServer,	0, 0, IDS_DEFINE_CELL_NAME_STEP,			0 },
	{ SS_STEP_TO_BE_DONE, DefineCellForClient,	0, 0, IDS_DEFINE_CELL_MEMBERSHIP_STEP,		0 },
	{ SS_STEP_TO_BE_DONE, StartBosServer,		0, 0, IDS_START_BOS_SERVER_STEP,			0 },
	{ SS_STEP_TO_BE_DONE, StartAuthServer,		0, 0, IDS_START_AUTH_SERVER_STEP,			0 },
	{ SS_STEP_TO_BE_DONE, CreatePrincipalAndKey,0, 0, IDS_CREATE_PRINCIPAL_AND_KEY_STEP,	0 },
	{ SS_STEP_TO_BE_DONE, StartDbServers,		0, 0, IDS_START_DB_STEP,					IDS_DB_STEP_DESC },
	{ SS_STEP_TO_BE_DONE, StartDbServers,		0, 0, IDS_START_DB_AND_BK_STEP,				IDS_DB_AND_BK_STEP_DESC },
	{ SS_STEP_TO_BE_DONE, StartDbServers,		0, 0, IDS_START_BK_STEP,					IDS_BK_STEP_DESC },	
	{ SS_STEP_TO_BE_DONE, CreateAdminPrincipal,	0, 0, IDS_CREATE_ADMIN_PRINCIPAL_STEP,		0 },
	{ SS_STEP_TO_BE_DONE, StartFsVlAndSalvager,	0, 0, IDS_START_FS_STEP,					IDS_FS_STEP_DESC },
	{ SS_STEP_TO_BE_DONE, ConfigSCC,			0, 0, IDS_START_SCC_STEP,					IDS_SCC_STEP_DESC },
	{ SS_STEP_TO_BE_DONE, ConfigSCS,			0, 0, IDS_START_SCS_STEP,					IDS_SCS_STEP_DESC },
	{ SS_STEP_TO_BE_DONE, CreateRootAfs,		0, 0, IDS_CREATE_ROOT_AFS_STEP,				IDS_ROOT_AFS_STEP_DESC },
	{ SS_STEP_TO_BE_DONE, StartClient,			0, 0, IDS_START_CLIENT_STEP,				0 },
	{ SS_STEP_TO_BE_DONE, SetRootAcl,			0, 0, IDS_SET_ROOT_ACL_STEP,				0 },
	{ SS_STEP_TO_BE_DONE, CreateRootCell,		0, 0, IDS_CREATE_ROOT_CELL_STEP,			0 },
	{ SS_STEP_TO_BE_DONE, MountRootCellStandard,0, 0, IDS_MOUNT_ROOT_CELL_STANDARD_STEP,	0 },
	{ SS_STEP_TO_BE_DONE, SetRootCellAcl,		0, 0, IDS_SET_ROOT_CELL_ACL_STEP,			0 },
	{ SS_STEP_TO_BE_DONE, MountRootCellRW,		0, 0, IDS_MOUNT_ROOT_CELL_RW_STEP,			0 },
	{ SS_STEP_TO_BE_DONE, Replicate,			0, 0, IDS_REP_STEP,							IDS_REP_STEP_DESC },
	{ SS_STEP_TO_BE_DONE, EnableAuthChecking,	0, 0, IDS_ENABLE_AUTH_CHECKING_STEP,		0 },
	{ SS_STEP_TO_BE_DONE, RestartServers,		0, 0, IDS_RESTART_SERVERS_STEP,				0 },
	{ SS_STEP_TO_BE_DONE, AddToCellServDB,		0, 0, IDS_ADD_TO_CELLSERVDB_STEP,			0 },
	{ SS_STEP_TO_BE_DONE, RestartAllDbServers,	0, 0, IDS_RESTART_ALL_DB_SERVERS_STEP,		0 },
	{ SS_STEP_TO_BE_DONE, VosOpenServer,		0, 0, IDS_NO_MSG_STEP,						0 },
	{ SS_STEP_TO_BE_DONE, UnconfigDB,			0, 0, IDS_UNCONFIG_DB_STEP,					IDS_UNCONFIG_DB_STEP_DESC },
	{ SS_STEP_TO_BE_DONE, UnconfigBak,			0, 0, IDS_UNCONFIG_BK_STEP,					IDS_UNCONFIG_BK_STEP_DESC },	
	{ SS_STEP_TO_BE_DONE, UnconfigFS,			0, 0, IDS_UNCONFIG_FS_STEP,					IDS_UNCONFIG_FS_STEP_DESC },
	{ SS_STEP_TO_BE_DONE, UnconfigSCS,			0, 0, IDS_UNCONFIG_SCS_STEP,				IDS_UNCONFIG_SCS_STEP_DESC },
	{ SS_STEP_TO_BE_DONE, UnconfigSCC,			0, 0, IDS_UNCONFIG_SCC_STEP,				IDS_UNCONFIG_SCC_STEP_DESC },
	{ SS_STEP_TO_BE_DONE, PostConfig,           0, 0, IDS_NO_MSG_STEP,		                0 },
	{ SS_STEP_TO_BE_DONE, UpgradeLibHandles,	0, 0, IDS_GET_CREDENTIALS_STEP,				0 },
	{ SS_STEP_TO_BE_DONE, GetRootVolumeInfo,    0, 0, IDS_NO_MSG_STEP,		                0 },
};


/*
 *	These are the steps to perform when configuring the very first server.
 */
static STEP_ID FirstServerSteps[] = {
	SID_CONFIG_PARTITION,
	SID_DEFINE_CELL_FOR_SERVER,
	SID_DEFINE_CELL_FOR_CLIENT,
	SID_START_BOS,
	SID_START_AUTH,
	SID_CREATE_PRINCIPAL_AND_KEY,
	SID_START_DB,
	SID_CREATE_ADMIN_PRINCIPAL,
	SID_START_FS_VL_AND_SALVAGER,
	SID_CONFIG_SCS,
	SID_VOS_OPEN_SERVER,
	SID_CREATE_ROOT_AFS,
	SID_START_CLIENT,
	SID_SET_ROOT_ACL,
	SID_CREATE_ROOT_CELL,
	SID_MOUNT_ROOT_CELL_STANDARD,
	SID_SET_ROOT_CELL_ACL,
	SID_MOUNT_ROOT_CELL_RW,
	SID_REPLICATE,
	SID_ENABLE_AUTH_CHECKING
};

static STEP_ID InvalidServerInfoSteps[] = {
	SID_DEFINE_CELL_FOR_SERVER,
	SID_START_BOS,
	SID_CREATE_PRINCIPAL_AND_KEY,
	SID_CREATE_ADMIN_PRINCIPAL,
	SID_ENABLE_AUTH_CHECKING
};

static STEP_ID InvalidClientInfoSteps[] = {
	SID_DEFINE_CELL_FOR_CLIENT,
	SID_START_CLIENT
};

static STEP_ID PreconfigSteps[] = {
	SID_GET_CREDENTIALS,    // Always do this so we will always have credentials - need this for the config manager.
    SID_VOS_OPEN_SERVER	    // We'll always do this step so we know we can make vos calls 
};

static STEP_ID UnconfigDbSteps[] = {
	SID_UNCONFIG_DB,
	SID_RESTART_ALL_DB_SERVERS
};

static STEP_ID UnconfigBakSteps[] = {
	SID_UNCONFIG_BAK,
	SID_GET_CREDENTIALS
};

static STEP_ID UnconfigFsSteps[] = {
	SID_UNCONFIG_FS
};

static STEP_ID UnconfigScsSteps[] = {
	SID_UNCONFIG_SCS
};

static STEP_ID UnconfigSccSteps[] = {
	SID_UNCONFIG_SCC
};

static STEP_ID FsSteps[] = {
	SID_START_FS_VL_AND_SALVAGER
};

static STEP_ID DbSteps[] = {
	SID_ADD_TO_CELLSERVDB,
	SID_START_DB,
	SID_RESTART_ALL_DB_SERVERS
};

static STEP_ID DbAndBakSteps[] = {
	SID_ADD_TO_CELLSERVDB,
	SID_START_DB_AND_BAK,
	SID_RESTART_ALL_DB_SERVERS
};

static STEP_ID BakOnlySteps[] = {
	SID_START_BAK,
};

static STEP_ID PartitionSteps[] = {
	SID_CONFIG_PARTITION
};

static STEP_ID CheckRootVolumesSteps[] = {
    SID_GET_ROOT_VOLUME_INFO
};

static STEP_ID RootVolumesSteps[] = {
	SID_CREATE_ROOT_AFS,
	SID_START_CLIENT,		// TODO:  Must check what happens if client started previously and failed because root.afs didn't exist.
	SID_SET_ROOT_ACL,
	SID_CREATE_ROOT_CELL,
	SID_MOUNT_ROOT_CELL_STANDARD,
	SID_SET_ROOT_CELL_ACL,
	SID_MOUNT_ROOT_CELL_RW,
};

static STEP_ID ReplicationSteps[] = {
	SID_REPLICATE
};

static STEP_ID ScsSteps[] = {
	SID_CONFIG_SCS
};

static STEP_ID SccSteps[] = {
	SID_CONFIG_SCC
};

static STEP_ID PostConfigSteps[] = {
	SID_POST_CONFIG
};



/*
 * EXPORTED FUNCTIONS _________________________________________________________________
 *
 */
BOOL Configure(HWND hParent, BOOL& bMustExit)
{	
	int nResult = ModalDialog(IDD_CONFIG_SERVER, hParent, (DLGPROC)ConfigServerPageDlgProc);

    bMustExit = m_bMustExit;

	return !m_bConfigFailed;
}


/*
 * Dialog Proc _________________________________________________________________
 *
 */
BOOL CALLBACK ConfigServerPageDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
		case WM_INITDIALOG:
			OnInitDialog(hwndDlg);
			break;

		case WM_COMMAND:
			switch (LOWORD(wp)) {
				case IDNEXT:
					OnConfig();
					break;

				case IDBACK:
				   IF_WIZ(g_pWiz->SetState(sidSTEP_ELEVEN));
				   break;

				case IDCANCEL:
					// If configuring, handle cancel here.
					// Otherwise, use common handler below.
					if (m_bConfiguring) {
						ShowCurrentStep(IDS_CANCEL_PENDING);
						m_bCheckCancel = TRUE;
						return FALSE;
					} else if (!g_pWiz)
						EndDialog(m_hDlg, m_bConfigured);
					break;
			}
			break;
	}

	if (g_pWiz) {
		if (WizStep_Common_DlgProc (hwndDlg, msg, wp, lp))
			return FALSE;
	}

	return FALSE;
}

/*
 * STATIC FUNCTIONS _________________________________________________________________
 *
 */


/*
 *	This function is the window proc for the static controls used to display the picture
 *	(blue dot, checkmark, or red X) that indicates the state of each step.
 *
 *	Which step to draw the state for is determined and then the state picture is displayed.
 */
static BOOL CALLBACK StepGrahpicDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// We only handle the paint message.  All other message are passed on to the
	// static control's normal window proc.
	if (uMsg == WM_PAINT) {
		for (int ii = 0; ii < m_nNumSteps; ii++) {
			// Find the step that corresponds to the window we are supposed to paint
			if (hwnd == GetDlgItem(m_hDlg, m_ConfigSteps[ii].nGraphicCtrlID)) {
				PaintStepGraphic(hwnd, m_ConfigSteps[ii].eState);	// Show the graphic for this step
				return 0;
			}
		}
	}		

	return CallWindowProc((WNDPROC)Subclass_FindNextHook(hwnd, StepGrahpicDlgProc), hwnd, uMsg, wParam, lParam);
}



/*
 * Event Handler Functions _________________________________________________________________
 *
 */
static void OnInitDialog(HWND hwndDlg)
{
	m_hDlg = hwndDlg;

	// Initialize our global variables - only the ones that should not
	// preserve their state if the user leaves and returns to this page.
	m_bConfiguring = FALSE;
	m_bConfigured = FALSE;
	m_bConfigFailed = FALSE;
	m_bCheckCancel = FALSE;
	m_bCancel = FALSE;
	m_bMustChangeClientCell = FALSE;
	m_hvosServer = 0;
	m_nPartitionID = INVALID_PARTITION_ID;
	m_bCellServDbUpdateErr = FALSE;
	m_nServerUpdates = 0;
	m_pszCellDbHosts = 0;
	m_nNumSteps = 0;
	m_bNoAuthMode = !g_CfgData.bValidServerInfo;
	m_szVicepName[0] = 0;
	m_bMustExit = FALSE;
	m_bCfgInfoInvalidated = FALSE;
	m_bUnconfiguringLastDBServer = g_CfgData.bLastDBServer && ShouldUnconfig(g_CfgData.configDB);
    m_bClientTokensSet = FALSE;
    m_bRootAfsDriveMappingCreated = FALSE;
    m_bWeCreatedRootAfs = FALSE;
    m_bWeCreatedRootCell = FALSE;

	IF_WIZ(g_pWiz->EnableButtons(BACK_BUTTON));
	IF_WIZ(g_pWiz->SetButtonText(IDNEXT, IDS_CONFIGURE));
	IF_WIZ(g_pWiz->SetDefaultControl(IDNEXT));

	TCHAR szMsg[cchRESOURCE] = TEXT("");

	// Show the cellname in the title
	ShowTitle();

	// If this is the wizard, then check if there is nothing to do and
	// inform the user.  If this is not the wizard, then we should
	// not even get to this point.  The config tool will not call
	// the config function if there is nothing to do.
	if (g_pWiz) {
		// Is everything already configured?
		if ((g_CfgData.configFS == CS_ALREADY_CONFIGURED) &&
		   (g_CfgData.configDB == CS_ALREADY_CONFIGURED) &&
		   (g_CfgData.configBak == CS_ALREADY_CONFIGURED) &&
		   (g_CfgData.configPartition == CS_ALREADY_CONFIGURED) &&
		   (g_CfgData.configRootVolumes == CS_ALREADY_CONFIGURED) &&
		   (g_CfgData.configRep == CS_ALREADY_CONFIGURED) &&
		   ((g_CfgData.configSCS == CS_ALREADY_CONFIGURED) ||
		   (g_CfgData.configSCC == CS_ALREADY_CONFIGURED)))
		{
			GetString(szMsg, IDS_ALREADY_CONFIGURED);
		}	
		// Is there nothing to configure?
		else if ((g_CfgData.configFS != CS_CONFIGURE) &&
		   (g_CfgData.configDB != CS_CONFIGURE) &&
		   (g_CfgData.configBak != CS_CONFIGURE) &&
		   (g_CfgData.configPartition != CS_CONFIGURE) &&
		   (g_CfgData.configRootVolumes != CS_CONFIGURE) &&
		   (g_CfgData.configRep != CS_CONFIGURE) &&
		   (g_CfgData.configSCS != CS_CONFIGURE) &&
		   (g_CfgData.configSCC != CS_CONFIGURE))
		{
			GetString(szMsg, IDS_NOTHING_TO_CONFIGURE);
		}

		// If there's a can't configure message, then show it
		if (*szMsg) {
			// Hide all controls except for the message window
			ShowWnd(m_hDlg, IDC_ALL_NEEDED_MSG, FALSE);
			ShowWnd(m_hDlg, IDC_FOLLOWING_STEPS_MSG, FALSE);

			for (int i = 0; i < sizeof(StepGuiCtrlIDs) / sizeof(StepGuiCtrlIDs[0]); i++) {
				ShowWnd(m_hDlg, StepGuiCtrlIDs[i].nDescCtrlID, FALSE);
				ShowWnd(m_hDlg, StepGuiCtrlIDs[i].nGraphicCtrlID, FALSE);
			}
			
			ShowWnd(m_hDlg, IDC_CURRENT_STEP_LABEL, FALSE);
			ShowWnd(m_hDlg, IDC_CURRENT_STEP, FALSE);
			ShowWnd(m_hDlg, IDC_CONFIG_PROGRESS, FALSE);
			ShowWnd(m_hDlg, IDC_PERCENT_COMPLETE_LABEL, FALSE);
			ShowWnd(m_hDlg, IDC_PERCENT_COMPLETE, FALSE);
			ShowWnd(m_hDlg, IDC_CANT_CONFIG_MSG, FALSE);

			// Show the message
			SetWndText(m_hDlg, IDC_STATUS_MSG, szMsg);

			return;
		}
	}

	// Determine which steps to perform and which should be displayed
	SetupConfigSteps();

	if (!g_CfgData.bWizard) {
		OnConfig();
		return;
	}

	// This must be done after SetupConfigSteps(), which assings a nGraphicCtrlID
	// value to the appropriate steps.  After the following code is executed, the graphic
	// for each step will be drawn automatically whenever the dialog is repainted.
	for (UINT ii = 0; ii < MAX_STEPS; ii++) {
		if (m_ConfigSteps[ii].nGraphicCtrlID)
			Subclass_AddHook(GetDlgItem(m_hDlg, m_ConfigSteps[ii].nGraphicCtrlID), StepGrahpicDlgProc);
	}

	IF_WIZ(g_pWiz->EnableButtons(BACK_BUTTON | NEXT_BUTTON));
}

// User has pressed the Config (or Exit) button.
static void OnConfig()
{
	ASSERT(g_CfgData.szCellName[0]);

	// Has user pressed the Exit button?
	if (m_bConfigured) {
		IF_WIZ(g_pWiz->Show(FALSE));
		return;
	}

	// Has user pressed the View Log button?
	if (m_bConfigFailed) {
		ViewLog();
		return;
	}

	// Must we change the client's cell?  See if user minds...
	if (m_bMustChangeClientCell) {
		if (ShowWarning(m_hDlg, IDS_CLIENT_CELL_WILL_CHANGE) == IDCANCEL) {
			return;
		}
	}

	// Create a thread to perform the configuration steps
	DWORD dwThreadID;
	
	// Start configuring...
	HANDLE hThread = CreateThread(0, 0, ConfigServer, 0, 0, &dwThreadID);

	CloseHandle(hThread);
}


/*
 * Utility Functions _________________________________________________________________
 *
 */
static void ShowExitButton()
{
	if (g_pWiz)
		g_pWiz->SetButtonText(IDNEXT, IDS_EXIT);
	else {
        if (m_bMustExit)
            SetWndText(m_hDlg, IDCANCEL, IDS_EXIT);
        else
		    SetWndText(m_hDlg, IDCANCEL, IDS_CLOSE);
    }
}

static void ShowTitle()
{
	ASSERT(g_CfgData.szCellName[0]);
	
	TCHAR szMsg[cchRESOURCE];

	GetString(szMsg, IDS_CONFIG_INTO_CELL_MSG);

	lstrcat(szMsg, g_CfgData.szCellName);
	lstrcat(szMsg, TEXT("."));

	SetWndText(m_hDlg, IDC_TITLE, szMsg);
}

static BOOL Unconfiguring()
{
	return	ShouldUnconfig(g_CfgData.configFS)  ||
			ShouldUnconfig(g_CfgData.configDB)  ||
			ShouldUnconfig(g_CfgData.configBak) ||
			ShouldUnconfig(g_CfgData.configSCS) ||
			ShouldUnconfig(g_CfgData.configSCC);
}

static void AddSteps(STEP_ID *pSteps, int nNumNewSteps)
{
	ASSERT(pSteps);
	ASSERT(nNumNewSteps > 0);

	if (m_nNumSteps + nNumNewSteps > MAX_STEPS) {
		ASSERT(FALSE);
		return;
	}

	// Add the new steps to the array of steps
	for (int nNewStep = 0; nNewStep < nNumNewSteps; nNewStep++) {
		STEP_ID nStepID = pSteps[nNewStep];

		// Add the new step
		m_ConfigSteps[m_nNumSteps++] = STEPS[nStepID];
	}
}

static void GetStepsToPerform()
{
#define	NUM_STEPS(x)	(sizeof((x)) / sizeof(STEP_ID))

	// Is this the first server?
	if (g_CfgData.bFirstServer) {
		// We may have to change the FirstServerSteps, so loop over them
		// and only add the ones we need.  All of the FirstServerSteps
		// are required except for the ones for backup and sys control
		// machine.  If the user doesn't want those then we won't put
		// them into the array of steps to perform.  Also, we may not need
		// to make the AFS partition (if it already exists).
		for (int i = 0; i < NUM_STEPS(FirstServerSteps); i++) {
			STEP_ID curStep = FirstServerSteps[i];

			if ((curStep == SID_START_DB) && ShouldConfig(g_CfgData.configBak))
				curStep = SID_START_DB_AND_BAK;
			else if ((curStep == SID_CONFIG_SCS) && !ShouldConfig(g_CfgData.configSCS))
				continue;
			else if ((curStep == SID_CONFIG_PARTITION) && !ShouldConfig(g_CfgData.configPartition))
				continue;

			AddSteps(&curStep, 1);
		}

		return;
	}

	// Make sure client info is valid
	if (!g_CfgData.bValidClientInfo || (lstrcmp(g_CfgData.szClientCellName, g_CfgData.szCellName) != 0)) {
        m_bMustChangeClientCell = TRUE;
		AddSteps(InvalidClientInfoSteps, NUM_STEPS(InvalidClientInfoSteps));
    }

	// Make sure server info is valid
	if (!g_CfgData.bValidServerInfo)
		AddSteps(InvalidServerInfoSteps, NUM_STEPS(InvalidServerInfoSteps));

	// Add steps that should always be performed
	AddSteps(PreconfigSteps, NUM_STEPS(PreconfigSteps));

	/*
	 *	Do unconfiguration first
	 */

	// Unconfigure File Server?
	if (ShouldUnconfig(g_CfgData.configFS))
		AddSteps(UnconfigFsSteps, NUM_STEPS(UnconfigFsSteps));

	// Unconfigure Database Server? Will also automatically unconfig backup server.
	if (ShouldUnconfig(g_CfgData.configDB))
		AddSteps(UnconfigDbSteps, NUM_STEPS(UnconfigDbSteps));
	// Unconfigure Backup Server?
	else if (ShouldUnconfig(g_CfgData.configBak))
		AddSteps(UnconfigBakSteps, NUM_STEPS(UnconfigBakSteps));

	// Unconfigure System Control Server?
	if (ShouldUnconfig(g_CfgData.configSCS))
		AddSteps(UnconfigScsSteps, NUM_STEPS(UnconfigScsSteps));

	// Unconfigure System Control Client?
	if (ShouldUnconfig(g_CfgData.configSCC))
		AddSteps(UnconfigSccSteps, NUM_STEPS(UnconfigSccSteps));

	/*
	 *	Now do configuration
	 */

	// AFS Partition
	if (ShouldConfig(g_CfgData.configPartition))
		AddSteps(PartitionSteps, NUM_STEPS(PartitionSteps));

	// Database and backup server
	if (ShouldConfig(g_CfgData.configDB)) {
		if (ShouldConfig(g_CfgData.configBak))
			AddSteps(DbAndBakSteps, NUM_STEPS(DbAndBakSteps));
		else
			AddSteps(DbSteps, NUM_STEPS(DbSteps));
	} else if (ShouldConfig(g_CfgData.configBak))
		AddSteps(BakOnlySteps, NUM_STEPS(BakOnlySteps));

	// File server
	if (ShouldConfig(g_CfgData.configFS))
		AddSteps(FsSteps, NUM_STEPS(FsSteps));

    if (!g_CfgData.bRootVolumesExistanceKnown || !g_CfgData.bRootVolumesReplicationKnown)
        AddSteps(CheckRootVolumesSteps, NUM_STEPS(CheckRootVolumesSteps));

	// Root volumes
	if (ShouldConfig(g_CfgData.configRootVolumes))
		AddSteps(RootVolumesSteps, NUM_STEPS(RootVolumesSteps));

	// Replication
	if (ShouldConfig(g_CfgData.configRep))
		AddSteps(ReplicationSteps, NUM_STEPS(ReplicationSteps));

	// System control server
	if (ShouldConfig(g_CfgData.configSCS))
		AddSteps(ScsSteps, NUM_STEPS(ScsSteps));

	// System control client
	if (ShouldConfig(g_CfgData.configSCC))
		AddSteps(SccSteps, NUM_STEPS(SccSteps));

    // Perform any steps necessary after all normal configuration has finished.
    // For instance, if all servers were shut down, then we ask the user if they
    // want the config info invalidated.  Also, if the last db server was stopped,
    // then we will stop the client as well.
	AddSteps(PostConfigSteps, NUM_STEPS(PostConfigSteps));
}

// For steps that should have a place on the dialog, assign them the
// next available position.
static void SetupStepGUI(CONFIG_STEP& step, UINT& nDispPos)
{
	step.eState = SS_STEP_TO_BE_DONE;

	// If this step has a msg ID then it is a step that gets displayed to the
	// user.  Show it in the dialog.
	if (step.nDescID) {
		// Give this step a position on the dialog in which to show its message
		step.nDescCtrlID = StepGuiCtrlIDs[nDispPos].nDescCtrlID;
		step.nGraphicCtrlID = StepGuiCtrlIDs[nDispPos].nGraphicCtrlID;

		// Show this step's text in the proper static control
		SetWndText(m_hDlg, step.nDescCtrlID, step.nDescID);

		// Show the static control
		ShowWnd(m_hDlg, step.nDescCtrlID);
	
		// Show the graphic control
		ShowWnd(m_hDlg, step.nGraphicCtrlID);
	
		nDispPos++;
	}
}

static void SetupConfigSteps()
{
	UINT nDispPos = 0;	// Which StepGuiCtrlID to use, if applicable
	int nStep = 0;

	// Determine which steps are going to be performed.  For the ones that
	// will be, show their description message in the appropriate place on
	// the dialog.
	GetStepsToPerform();
	ASSERT(m_nNumSteps > 0);

	// For steps that should have a place on the dialog, assign them the
	// next available position.
	for (int i = 0; i < m_nNumSteps; i++)
		SetupStepGUI(m_ConfigSteps[i], nDispPos);
}

static BOOL CheckResult(int nResult, int nStatus)
{
	CHECK_CANCEL;

	if (nResult)
		return TRUE;

	ShowError(m_hDlg, nStatus, IDS_CONFIG_ERROR);

	return FALSE;
}

static BOOL CheckCancel()
{
	// If we already know we are cancelling then return
	if (m_bCancel)
		return TRUE;

	// If user didn't press Cancel button, then return FALSE
	if (!m_bCheckCancel)
		return FALSE;

	ASSERT(m_bConfiguring);

	TCHAR szMsg[cchRESOURCE];
	TCHAR szTitle[cchRESOURCE];

	GetString(szMsg, IDS_CANCEL_CONFIG_MSG);
	GetString(szTitle, GetAppTitleID());
	
	// Ask user if they really want to cancel
	int nChoice = MessageBox(m_hDlg, szMsg, szTitle, MB_YESNO | MB_ICONQUESTION);
	
	m_bCancel = (nChoice == IDYES);

	m_bCheckCancel = FALSE;

	return m_bCancel;
}

/*
 *	Show the current config step, UNLESS the user has pressed the Cancel
 *	button, in which case a "cancel pending" message is already being 
 *	displayed and we don't want it replace with this new message.
 */
static void ShowCurrentStep(UINT uiMsgID)
{
	if (!m_bCheckCancel && uiMsgID) {
		SetWndText(m_hDlg, IDC_CURRENT_STEP, uiMsgID);
        ForceUpdateWindow(m_hDlg, IDC_CURRENT_STEP);
    }
}

static void ShowCurrentStep(TCHAR *pszMsg)
{
	if (!m_bCheckCancel && pszMsg) {
		SetWndText(m_hDlg, IDC_CURRENT_STEP, pszMsg);
        ForceUpdateWindow(m_hDlg, IDC_CURRENT_STEP);
    }
}

// Set the range and step increment for the progress bar.
static void InitProgressBar()
{
	SendDlgItemMessage(m_hDlg, IDC_CONFIG_PROGRESS, PBM_SETRANGE, 0, MAKELPARAM(0, m_nNumSteps));
	SendDlgItemMessage(m_hDlg, IDC_CONFIG_PROGRESS, PBM_SETSTEP, 1, 0);
}

static char *GetVicepName()
{
	ASSERT((lstrlen(g_CfgData.szPartitionName) == 1) || (lstrlen(g_CfgData.szPartitionName) == 2));

	// Construct the partition name
	if (!m_szVicepName[0]) 
		sprintf(m_szVicepName, "/vicep%s", GetPartitionNameA());

	return m_szVicepName;
}


/*
 * Configuration Functions _________________________________________________________________
 *
 */

static BOOL VosOpenServer()
{
	ASSERT(m_hvosServer == 0);
	ASSERT(g_CfgData.szHostname[0]);
	ASSERT(g_hCell);

	g_LogFile.Write("Opening server %s.\r\n", GetHostnameA());

	m_nResult = vos_ServerOpen(g_hCell, GetHostnameA(), &m_hvosServer, &m_nStatus);
	CHECK_RESULT;

	return TRUE;
}

static BOOL ConfigPartition()
{
	ASSERT(g_hServer);
	ASSERT(g_CfgData.chDeviceName);

	// Constuct the device name
	char szDevName[] = "?:";
	szDevName[0] = GetDeviceNameA();

	g_LogFile.Write("Adding an AFS partition on device '%s' with name '%s'.\r\n", szDevName, GetVicepName());

	m_nResult = cfg_HostPartitionTableAddEntry(g_hServer, GetVicepName(), szDevName, &m_nStatus);
	CHECK_RESULT;
	
	return TRUE;
}

static BOOL FreeCellServDB()
{
	afs_status_t nIgnore;

	if (m_pszCellDbHosts) {
		cfg_StringDeallocate(m_pszCellDbHosts, &nIgnore);
		CHECK_RESULT;
	}

	return TRUE;
}

static BOOL GetCellServDB(char **ppszCellServDB)
{
	ASSERT(g_CfgData.szCellServDbHostname[0]);

	afs_status_t nIgnore;
	char *pszCellname = 0;

	g_LogFile.Write("Getting CellServDB from host %s.\r\n", GetCellServDbHostnameA());
	m_nResult = cfg_CellServDbEnumerate(GetCellServDbHostnameA(), &pszCellname, ppszCellServDB, &m_nStatus);
	CHECK_RESULT;

	// The cell name from this call better match the cell name we got previously
	if (strcmp(GetCellNameA(), pszCellname) != 0) {
		ShowError(m_hDlg, 0, IDS_WRONG_CELL);
		m_nResult = 0;
		return FALSE;
	}

	cfg_StringDeallocate(pszCellname, &nIgnore);

	g_LogFile.WriteMultistring(*ppszCellServDB);

	return TRUE;
}

// Define cell name and cell membership for server
static BOOL DefineCellForServer()
{
	ASSERT(g_hServer);
	ASSERT(g_CfgData.szCellName[0]);

	// CellServDB entries
	char *pszEntries = 0;

	// If not first server, get list of other hosts
	if (!g_CfgData.bFirstServer) {
		if (!m_pszCellDbHosts) {
			m_nResult = GetCellServDB(&m_pszCellDbHosts);
			if (!m_nResult)
				return m_nResult;
		}
		pszEntries = m_pszCellDbHosts;
	} else {
		// Make the hostname a multistring
		_tcsncat(g_CfgData.szHostname, TEXT("\0"), MAX_PARTITION_NAME_LEN);
		pszEntries = GetHostnameA();
	}

	g_LogFile.Write("Putting this host in cell '%s'.\r\n", GetCellNameA());

	ASSERT(g_CfgData.szCellName[0]);
		
	m_nResult = cfg_HostSetCell(g_hServer, GetCellNameA(), pszEntries, &m_nStatus);
	CHECK_RESULT;

	g_CfgData.bValidServerInfo = TRUE;

	return TRUE;
}

static BOOL StopClient()
{
    g_LogFile.Write("Stopping the client.\r\n");

    m_nResult = cfg_ClientStop(g_hClient, CLIENT_STOP_TIMEOUT, &m_nStatus);
    CHECK_RESULT;

    return TRUE;
}

static BOOL DefineCellForClient()
{
	ASSERT(g_hClient);
	ASSERT(g_CfgData.szCellName[0]);

    // Stop the client
    if (!StopClient())
        return FALSE;

	// CellServDB entries
	char *pszEntries = 0;

	// If not first server, get list of other hosts
	if (!g_CfgData.bFirstServer) {
		if (!m_pszCellDbHosts) {
			m_nResult = GetCellServDB(&m_pszCellDbHosts);
			if (!m_nResult)
				return m_nResult;
		}
    	pszEntries = m_pszCellDbHosts;
	} else {
		// Make the hostname a multistring
		_tcsncat(g_CfgData.szHostname, TEXT("\0"), MAX_PARTITION_NAME_LEN);
		pszEntries = GetHostnameA();
	}

	g_LogFile.Write("Putting the AFS Client in this host's cell.\r\n");

	m_nResult = cfg_ClientSetCell(g_hClient, GetCellNameA(), pszEntries, &m_nStatus);
	CHECK_RESULT;

	// Update our state info about the client
	g_CfgData.bValidClientInfo = TRUE;
	lstrcpy(g_CfgData.szClientCellName, g_CfgData.szCellName);

	if (!g_CfgData.bFirstServer) {
		if (!UpgradeLibHandles())
			return FALSE;
	}

	return TRUE;
}

static BOOL StartBosServer()
{
	ASSERT(g_hServer);

	g_LogFile.Write("Starting the bos server in %s mode.\r\n", m_bNoAuthMode ? "no auth" : "auth");
	
	m_nResult = cfg_BosServerStart(g_hServer, m_bNoAuthMode, BOSSERVER_START_TIMEOUT, &m_nStatus);
	CHECK_RESULT;

	return TRUE;
}

static BOOL StartAuthServer()
{
	ASSERT(g_hServer);
	ASSERT(g_CfgData.bFirstServer);

	g_LogFile.Write("Starting the authentication server.\r\n");

	m_nResult = cfg_AuthServerStart(g_hServer, &m_nStatus);
	CHECK_RESULT;

	g_CfgData.bAuthServerRunning = TRUE;

	return TRUE;
}

static BOOL CreatePrincipalAndKey()
{
	ASSERT(g_hServer);

	if (!UpgradeLibHandles())
		return FALSE;

	// Create AFS server principal and put key in local Keyfile
	LPTSTR pszServerPW = 0;

	g_LogFile.Write("Setting the AFS Principal.\r\n");

	if (g_CfgData.bFirstServer) {
		ASSERT(g_CfgData.szServerPW[0]);
		pszServerPW = GetServerPW();
	}

    BOOL bDone = FALSE;

    while (!bDone) {
    	m_nResult = cfg_HostSetAfsPrincipal(g_hServer, (short)g_CfgData.bFirstServer, S2A(pszServerPW), &m_nStatus);

        if (m_nStatus == ADMCFGAFSPASSWDINVALID)
            MsgBox(m_hDlg, IDS_BAD_PW, GetAppTitleID(), MB_OK);

        if ((m_nStatus == ADMCFGAFSPASSWDINVALID) || (m_nStatus == ADMCFGAFSKEYNOTAVAILABLE)) {
            // Ask user for the AFS principal password
            if (!GetAfsPrincipalPassword(m_hDlg, pszServerPW)) {
                m_bCancel = TRUE;
                return FALSE;
            }
        } else
            bDone = TRUE;
    }

	CHECK_RESULT;

	return TRUE;
}

static BOOL StartDbServers()
{
	ASSERT(g_hServer);

	g_LogFile.Write("Starting the following servers: Protection   Volume Location   ");
	if (IsStepEnabled(g_CfgData.configBak))
		g_LogFile.Write("Backup   ");
	if (!g_CfgData.bFirstServer)
		g_LogFile.Write("Authentication");
	g_LogFile.Write("\r\n");

	// Start Protection, Volume Location, and Backup (optional) database servers
	m_nResult = cfg_DbServersStart(g_hServer, ShouldConfig(g_CfgData.configBak), &m_nStatus);
	CHECK_RESULT;

	// Must wait for this now so we can then talk to them.
	if (g_CfgData.bFirstServer) {
		g_LogFile.Write("Waiting for database servers to reach quorum.\r\n");
		m_nResult = cfg_DbServersWaitForQuorum(g_hServer, QUORUM_WAIT_TIMEOUT, &m_nStatus);
		CHECK_RESULT;
	}

	return TRUE;
}

static BOOL CreateAdminPrincipal()
{
	ASSERT(g_hServer);
	ASSERT(g_CfgData.szAdminName[0]);
	
	// Create generic admin principal and put in local Userlist
	char *pszAdminPW = 0;
	int nUID = 0;

	if (g_CfgData.bFirstServer) {
		ASSERT(g_CfgData.szAdminPW[0]);
		ASSERT(g_CfgData.szAdminUID[0]);
		pszAdminPW = GetAdminPWA();
		nUID = atoi(GetAdminUIDA());
		g_LogFile.Write("Setting Admin Principal to '%s' and UID to %d.\r\n", GetAdminNameA(), nUID);
	} else
		g_LogFile.Write("Setting Admin Principal to '%s'.\r\n", GetAdminNameA());

	m_nResult = cfg_HostSetAdminPrincipal(g_hServer, (short)g_CfgData.bFirstServer, GetAdminNameA(), pszAdminPW, nUID, &m_nStatus);
	CHECK_RESULT;

	g_CfgData.bAdminPrincipalCreated = TRUE;

	if (g_CfgData.bFirstServer) {
		if (!UpgradeLibHandles())
			return FALSE;
	}

	return TRUE;
}

static BOOL StartFsVlAndSalvager()
{
	ASSERT(g_hServer);

	g_LogFile.Write("Starting the File Server.\r\n");

	m_nResult = cfg_FileServerStart(g_hServer, &m_nStatus);
	CHECK_RESULT;
                
	return TRUE;
}

static BOOL ConfigSCS()
{
	ASSERT(g_hServer);

	g_LogFile.Write("Configuring the System Control Server.\r\n");

	m_nResult = cfg_SysBinServerStart(g_hServer, TRUE, FALSE, &m_nStatus);
	CHECK_RESULT;

	return TRUE;
}

static BOOL ConfigSCC()
{
	ASSERT(g_hServer);
	ASSERT(g_CfgData.szSysControlMachine[0]);
	
	g_LogFile.Write("Configuring the System Control Client.\r\n");

	m_nResult = cfg_SysControlClientStart(g_hServer, GetSysControlMachineA(), &m_nStatus);
	CHECK_RESULT;

	return TRUE;
}

static BOOL GetPartitionID()
{
	if (m_nPartitionID != INVALID_PARTITION_ID)
		return TRUE;

	g_LogFile.Write("Translating the parition name '%s' to an ID.\r\n", GetVicepName());

	m_nResult = vos_PartitionNameToId(GetVicepName(), &m_nPartitionID, &m_nStatus);
	CHECK_RESULT;

	g_LogFile.Write("The ID for partition '%s' is %d.\r\n", GetVicepName(), m_nPartitionID);

	ASSERT(m_nPartitionID != INVALID_PARTITION_ID);

	return TRUE;
}

static BOOL GetRootVolumeInfo()
{
    BOOL bResult;

    if (!g_CfgData.bRootVolumesExistanceKnown) {
        m_nStatus = DoRootVolumesExist(bResult);
        m_nResult = !m_nStatus;
        CHECK_RESULT;
    
        g_CfgData.bRootVolumesExistanceKnown = TRUE;
    }

    if (!g_CfgData.bRootVolumesReplicationKnown) {
        m_nStatus = AreRootVolumesReplicated(bResult);
        m_nResult = !m_nStatus;
        CHECK_RESULT;
    
        g_CfgData.bRootVolumesReplicationKnown = TRUE;
    }

    return TRUE;
}

static BOOL CreateRootAfs()
{
	ASSERT(g_hCell);
	ASSERT(m_hvosServer);

    // If the root.afs volume already exists, then just return.  We can get to this step
    // and root.afs already exist if:
    //
    //    1)  We could not determine the status of the root volumes when the app was started 
    //        and the user asked us to create the root volumes if they don't exist.
    //
    //    2)  Since there is only one page from which the user decides if they want to create
    //        the root volumes, there is a chance that one of the root volumes may exist while
    //        the other doesn't.  If that is the case the user is told that the "root volumes"
    //        don't exist.  If they choose to create them, we do this check here to make sure
    //        we only create the one that doesn't exist.
    //
    if (g_CfgData.bRootAfsExists)
		return TRUE;

	if (!GetPartitionID())
		return FALSE;
    
    // If the client is running then stop it - creating root.afs will confuse it.
    // It will be started again after root.afs is created.
    if (!StopClient())
        return FALSE;

	g_LogFile.Write("Creating volume root.afs on partition %d with a quota of %d.\r\n", m_nPartitionID, ROOT_VOLUMES_QUOTA);

	m_nResult = vos_VolumeCreate(g_hCell, m_hvosServer, 0, m_nPartitionID, "root.afs", ROOT_VOLUMES_QUOTA, &g_CfgData.nRootAfsID, &m_nStatus);

	CHECK_RESULT;

	g_LogFile.Write("Volume root.afs was created with an ID of %d.\r\n", g_CfgData.nRootAfsID);

	g_CfgData.bRootAfsExists = TRUE;

    m_bWeCreatedRootAfs = TRUE;

	return TRUE;
}

static BOOL StartClient()
{
	ASSERT(g_hClient);
    
	g_LogFile.Write("Starting the AFS Client.\r\n");

	m_nResult = cfg_ClientStart(g_hClient, CLIENT_START_TIMEOUT, &m_nStatus);
	CHECK_RESULT;
	
	return TRUE;
}

static BOOL SetTokensInClient()
{
    if (m_bClientTokensSet)
        return TRUE;

	g_LogFile.Write("Putting our tokens into the AFS Client.\r\n");

    m_nResult = afsclient_TokenSet(g_hToken, &m_nStatus);
    CHECK_RESULT;

    m_bClientTokensSet = TRUE;

    return TRUE;
}

static BOOL CreateRootAfsDriveMapping()
{
    if (m_bRootAfsDriveMappingCreated)
        return TRUE;

    g_LogFile.Write("Attempting to create a drive mapping into AFS.\r\n");

	char szAfsRootDir[_MAX_PATH];
	sprintf(szAfsRootDir, "\\\\%s\\all", GetClientNetbiosNameA());

    strcpy(m_szDriveToMapTo, "_:");

    // We will try all drives from D to Z.
    char chDriveLetter;

try_again:
    NETRESOURCE nr;
    memset (&nr, 0x00, sizeof(NETRESOURCE));

    for (chDriveLetter = 'D'; (chDriveLetter <= 'Z') && !m_bRootAfsDriveMappingCreated; chDriveLetter++) {
        m_szDriveToMapTo[0] = chDriveLetter;
        g_LogFile.Write("Attempting to map %s to %s: ", m_szDriveToMapTo, szAfsRootDir);

        nr.dwType=RESOURCETYPE_DISK;
        nr.lpLocalName=m_szDriveToMapTo;
        nr.lpRemoteName=szAfsRootDir;
        nr.dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
        DWORD res=WNetAddConnection2(&nr,NULL,NULL,0);
        m_bRootAfsDriveMappingCreated = (res == NO_ERROR);
        // m_bRootAfsDriveMappingCreated = (WNetAddConnection(A2S(szAfsRootDir), TEXT(""), A2S(m_szDriveToMapTo)) == NO_ERROR);
        g_LogFile.Write(m_bRootAfsDriveMappingCreated ? "succeeded.\r\n" : "failed.\r\n");
    }        

    // If we couldn't map a drive, then ask the user to unmap something.
    if (!m_bRootAfsDriveMappingCreated) {
    	int nChoice = MsgBox(m_hDlg, IDS_CANT_MAP_ROOT_AFS, GetAppTitleID(), MB_ICONEXCLAMATION | MB_OKCANCEL);
        if (nChoice == IDOK)
            goto try_again;
    }            

    return m_bRootAfsDriveMappingCreated;
}

static BOOL SetRootAcl()
{
    // Only do this if we just created root.afs
    if (!m_bWeCreatedRootAfs)
        return TRUE;

    if (!SetTokensInClient())
        return FALSE;

    if (!CreateRootAfsDriveMapping())
        return FALSE;

	g_LogFile.Write("Setting the ACL on root.afs.\r\n");

	acl_t acl = { READ, NO_WRITE, LOOKUP, NO_DELETE, NO_INSERT, NO_LOCK, NO_ADMIN };

	m_nResult = afsclient_ACLEntryAdd(m_szDriveToMapTo, "system:anyuser", &acl, &m_nStatus);
	CHECK_RESULT;
	
	return TRUE;
}


static BOOL CreateRootCell()
{
	ASSERT(g_hCell);
	ASSERT(m_hvosServer);

    // If the root.cell volume already exists, then just return.  We can get to this step
    // and root.cell already exist if:
    //
    //    1)  We could not determine the status of the root volumes when the app was started 
    //        and the user asked us to create the root volumes if they don't exist.
    //
    //    2)  Since there is only one page from which the user decides if they want to create
    //        the root volumes, there is a chance that one of the root volumes may exist while
    //        the other doesn't.  If that is the case the user is told that the "root volumes"
    //        don't exist.  If they choose to create them, we do this check here to make sure
    //        we only create the one that doesn't exist.
    //
    if (g_CfgData.bRootCellExists)
		return TRUE;

    // If root.afs exists and we did not just create it, then we cannot make root.cell.  For
    // now, just pretend we succeeded.  TODO: We must handle this better in a future version.
    // We can't at this time because the program strings are frozen - we can't add new
    // error messages.
	if (g_CfgData.bRootAfsExists && !m_bWeCreatedRootAfs)
        return TRUE;
	
	if (!GetPartitionID())
		return FALSE;

	g_LogFile.Write("Creating volume root.cell on partition %d with a quota of %d.\r\n", m_nPartitionID, ROOT_VOLUMES_QUOTA);

	m_nResult = vos_VolumeCreate(g_hCell, m_hvosServer, 0, m_nPartitionID, "root.cell", ROOT_VOLUMES_QUOTA, &g_CfgData.nRootCellID, &m_nStatus);
	CHECK_RESULT;
	
	g_LogFile.Write("Volume root.cell was created with an ID of %d.\r\n", g_CfgData.nRootCellID);

	g_CfgData.bRootCellExists = TRUE;

    m_bWeCreatedRootCell = TRUE;

	return TRUE;
}

static char *GetRootCellDir()
{
	static char szDir[MAX_CELL_NAME_LEN + 5] = "";

	if (!szDir[0]) {
		ASSERT(g_CfgData.szCellName[0]);
		sprintf(szDir, "%s\\%s", m_szDriveToMapTo, GetCellNameA());
	}

	return szDir;
}

static char *GetRootCellReadWriteDir()
{
	static char szDir[MAX_CELL_NAME_LEN + 5] = "";

	if (!szDir[0]) {
		ASSERT(g_CfgData.szCellName[0]);
		sprintf(szDir, "%s\\.%s", m_szDriveToMapTo, GetCellNameA());
	}

	return szDir;
}

static BOOL MountRootCellStandard()
{
	ASSERT(g_CfgData.szCellName[0]);

    // Only do this if we just created root.cell
    if (!m_bWeCreatedRootCell)
        return TRUE;

    if (!SetTokensInClient())
        return FALSE;

    if (!CreateRootAfsDriveMapping())
        return FALSE;

	g_LogFile.Write("Mouting root.cell with a Standard mount point at path %s.\r\n", GetRootCellDir());
	
	m_nResult = afsclient_MountPointCreate(g_hCell, GetRootCellDir(), "root.cell", READ_ONLY, CHECK_VOLUME, &m_nStatus);
	CHECK_RESULT;

	return TRUE;
}

static BOOL SetRootCellAcl()
{
    // Only do this if we just created root.cell
    if (!m_bWeCreatedRootCell)
        return TRUE;

    if (!SetTokensInClient())
        return FALSE;

	g_LogFile.Write("Setting the ACL on root.cell (dir %s).\r\n", GetRootCellDir());

	acl_t acl = { READ, NO_WRITE, LOOKUP, NO_DELETE, NO_INSERT, NO_LOCK, NO_ADMIN };

	m_nResult = afsclient_ACLEntryAdd(GetRootCellDir(), "system:anyuser", &acl, &m_nStatus);
	CHECK_RESULT;

	return TRUE;
}

static BOOL MountRootCellRW()
{
	ASSERT(g_CfgData.szCellName[0]);

    // Only do this if we just created root.cell
    if (!m_bWeCreatedRootCell)
        return TRUE;

    if (!SetTokensInClient())
        return FALSE;

    if (!CreateRootAfsDriveMapping())
        return FALSE;

	g_LogFile.Write("Mounting root.cell with a Read/Write mount point at path %s.\r\n", GetRootCellReadWriteDir());

	m_nResult = afsclient_MountPointCreate(g_hCell, GetRootCellReadWriteDir(), "root.cell", READ_WRITE, CHECK_VOLUME, &m_nStatus);
	CHECK_RESULT;

	return TRUE;
}

static BOOL Replicate()
{
	ASSERT(g_hCell);
	ASSERT(m_hvosServer);

	if (!GetPartitionID())
		return FALSE;

	// If only one of the volumes is not replicated, then only replicate
	// that volume, or if we could not determine if they were replicated
    // until configuration began, and they are replicated, then do nothing.

	if (g_CfgData.bRootAfsExists && !g_CfgData.bRootAfsReplicated) {
		g_LogFile.Write("Creating a read only site for volume root.afs using partition ID %d and volume ID %d.\r\n", m_nPartitionID, g_CfgData.nRootAfsID);
	
		m_nResult = vos_VLDBReadOnlySiteCreate(g_hCell, m_hvosServer, 0, m_nPartitionID, g_CfgData.nRootAfsID, &m_nStatus);
		CHECK_RESULT;
	
		g_LogFile.Write("Releasing the root.afs volume using volume ID %d.\r\n", g_CfgData.nRootAfsID);
	
		m_nResult = vos_VolumeRelease(g_hCell, 0, g_CfgData.nRootAfsID, VOS_NORMAL, &m_nStatus);
		CHECK_RESULT;

		g_CfgData.bRootAfsReplicated = TRUE;
	}

	if (g_CfgData.bRootCellExists && !g_CfgData.bRootCellReplicated) {
		g_LogFile.Write("Creating a read only site for volume root.cell using partition ID %d and volume ID %d.\r\n", m_nPartitionID, g_CfgData.nRootCellID);
	
		m_nResult = vos_VLDBReadOnlySiteCreate(g_hCell, m_hvosServer, 0, m_nPartitionID, g_CfgData.nRootCellID, &m_nStatus);
		CHECK_RESULT;
	
		g_LogFile.Write("Releasing the root.cell volume using volume ID %d.\r\n", g_CfgData.nRootCellID);
	
		m_nResult = vos_VolumeRelease(g_hCell, 0, g_CfgData.nRootCellID, VOS_NORMAL, &m_nStatus);
		CHECK_RESULT;

		g_CfgData.bRootCellReplicated = TRUE;
	}

	return TRUE;
}

static BOOL EnableAuthChecking()
{
	ASSERT(g_hCell);
	ASSERT(g_CfgData.szHostname[0]);

	void *hbosServer;

	g_LogFile.Write("Bos open of server '%s'.\r\n", GetHostnameA());
	m_nResult = bos_ServerOpen(g_hCell, GetHostnameA(), &hbosServer, &m_nStatus);
	CHECK_RESULT;

	ASSERT(hbosServer);

	g_LogFile.Write("Enabling auth checking.\r\n");
	m_nResult = bos_AuthSet(hbosServer, BOS_AUTH_REQUIRED, &m_nStatus);
	CHECK_RESULT;

	g_LogFile.Write("Closing bos server connection.\r\n");
	m_nResult = bos_ServerClose(hbosServer, &m_nStatus);
	CHECK_RESULT;

	return TRUE;
}

static BOOL UpgradeLibHandles()
{
	ASSERT(g_CfgData.szCellName[0]);
	ASSERT(g_CfgData.szAdminName[0]);
	ASSERT(g_CfgData.szAdminPW[0]);

	g_LogFile.Write("Getting credentials in cell '%s' as admin '%s'.\r\n", GetCellNameA(), GetAdminNameA());

	m_nResult = GetLibHandles(&m_nStatus);
	CHECK_RESULT;

	return TRUE;
}

static BOOL RestartServers()
{
	ASSERT(g_hCell);
	
	void *hbosServer;

	g_LogFile.Write("Bos open of server '%s'.\r\n", GetHostnameA());
	m_nResult = bos_ServerOpen(g_hCell, GetHostnameA(), &hbosServer, &m_nStatus);
	CHECK_RESULT;

	ASSERT(hbosServer);

	g_LogFile.Write("Stopping and restarting all bos processes.\r\n");
	m_nResult = bos_ProcessAllStopAndRestart(hbosServer, BOS_RESTART_BOS_SERVER, &m_nStatus);
	CHECK_RESULT;

	g_LogFile.Write("Closing bos server connection.\r\n");
	m_nResult = bos_ServerClose(hbosServer, &m_nStatus);
	CHECK_RESULT;

	return TRUE;
}

void DbAddHostCallback(void *callBackId, cfg_cellServDbStatus_t *statusItemP, int status)
{
	// Is this our call back ID?
	if (callBackId != (void *)&m_CallBackID)
		return;

	// Update the var that tracks server updates so the main config thread won't
	// think we have timed out.
	EnterCriticalSection(&m_CritSec);
	m_nServerUpdates++;
	LeaveCriticalSection(&m_CritSec);

	// Is this the last callback?
	if (!statusItemP) {
		m_nStatus = status;
		m_nResult = !status;
		SetEvent(m_hCellServDBUpdateEvent);		// Signal config thread that we are done
		return;
	}

	UINT nStrID;

	// Did the update of the current host succeed?
	if (statusItemP->status == 0)
		nStrID = IDS_UPDATING_CELLSERVDB_HOST_SUCCEEDED;
	else {
		m_bCellServDbUpdateErr = TRUE;
		nStrID = IDS_UPDATING_CELLSERVDB_HOST_FAILED;
	}

	TCHAR szMsg[cchRESOURCE];
	GetString(szMsg, nStrID);
	_tcscat(szMsg, A2S(statusItemP->fsDbHost));
	ShowCurrentStep(szMsg);

	g_LogFile.Write("Update of the CellServDB file on host %s %s.\r\n", statusItemP->fsDbHost, statusItemP->status ? "failed" : "succeeded");

	cfg_CellServDbStatusDeallocate(statusItemP, &m_nStatus);
	// We don't care if this fails
}

static BOOL UpdateCellServDB(BOOL bAdding)
{
	ASSERT(g_hServer);
	ASSERT(g_CfgData.szCellName[0]);

	int nMaxUpdates;				 

	cfg_cellServDbUpdateCallBack_t callBack = (cfg_cellServDbUpdateCallBack_t)DbAddHostCallback;

	// Create the event that the callback routine will use to tell us it is finished.
	// If we fail to create the event then don't use a callback routine.
	m_hCellServDBUpdateEvent = CreateEvent(NULL /* Sec */, FALSE /* Manual Reset */, FALSE /* Initial State */, m_pszCellServDBUpdateEventName);
	if (!m_hCellServDBUpdateEvent) {
		// Cause the CHECK_RESULT below to fail
		m_nResult = 0;
		m_nStatus = GetLastError();
	}
	CHECK_RESULT;

	// Create our critical section
	InitializeCriticalSection(&m_CritSec);

	// Update CellServDB via a SCM if the user specified one
	char *pszSCM = 0;
	if (g_CfgData.szSysControlMachine[0]) {
		pszSCM = GetSysControlMachineA();
		g_LogFile.Write("We will update CellServDB using Sys Control Machine %s.\r\n", pszSCM);
	}

	// Update CellServDB on all servers
	g_LogFile.Write("Updating CellServDB on all servers in the cell.\r\n");
	if (bAdding)	
		m_nResult = cfg_CellServDbAddHost(g_hServer, pszSCM, callBack, (void *)&m_CallBackID, &nMaxUpdates, &m_nStatus);
	else
		m_nResult = cfg_CellServDbRemoveHost(g_hServer, pszSCM, callBack, (void *)&m_CallBackID, &nMaxUpdates, &m_nStatus);
	CHECK_RESULT;

	// Update CellServDB for the client on this machine
	g_LogFile.Write("Updating the client's CellServDB.\r\n");
	if (bAdding)
		cfg_ClientCellServDbAdd(g_hClient, GetCellNameA(), GetHostnameA(), &m_nStatus);
	else
		cfg_ClientCellServDbRemove(g_hClient, GetCellNameA(), GetHostnameA(), &m_nStatus);
	CHECK_RESULT;

	BOOL bDone = FALSE;

	while (!bDone) {
		switch (WaitForSingleObject(m_hCellServDBUpdateEvent, CELLSERVDB_UPDATE_TIMEOUT))
		{
			case WAIT_OBJECT_0:	bDone = TRUE;	// The callback function signalled us that it is done.	
								break;

			case WAIT_TIMEOUT: 
				// We timed out so see if a server was updated.  If it was then all is cool
				// and we can keep going (after clearing the server update count flag).
				EnterCriticalSection(&m_CritSec);

				if (m_nServerUpdates)
					m_nServerUpdates = 0;
				else {
					// There were no server updates, so we really did timeout
					TCHAR szMsg[cchRESOURCE];
					GetString(szMsg, IDS_CELLSERVDB_UPDATE_PROBLEM);
					_tcscat(m_szCellServDbUpdateErrMsg, szMsg);
					bDone = TRUE;
				}
			
				LeaveCriticalSection(&m_CritSec);

				break;
			default:
				// No other return values are valid when waiting on an event object
				ASSERT(FALSE);
				break;
		}
	}

	DeleteCriticalSection(&m_CritSec);

	CloseHandle(m_hCellServDBUpdateEvent);

	// See if a failure occurred in the callback
	CHECK_RESULT;

	return TRUE;
}

static BOOL AddToCellServDB()
{
	return UpdateCellServDB(TRUE);
}

static BOOL RemoveFromCellServDB()
{
	return UpdateCellServDB(FALSE);
}

static BOOL RestartAllDbServers()
{
	ASSERT(g_hServer);

	if (m_bUnconfiguringLastDBServer)
		return TRUE;

	g_LogFile.Write("Restarting all DB servers.\r\n");
	m_nResult = cfg_DbServersRestartAll(g_hServer, &m_nStatus);
	CHECK_RESULT;

	g_LogFile.Write("Waiting for all DB servers to reach quorum.\r\n");
	m_nResult = cfg_DbServersWaitForQuorum(g_hServer, QUORUM_WAIT_TIMEOUT, &m_nStatus);
	CHECK_RESULT;

	m_bDbServersRestarted = TRUE;

	return TRUE;
}

static BOOL UnconfigDB()
{
	ASSERT(g_hServer);

	m_nResult = RemoveFromCellServDB();
	CHECK_RESULT;

	m_nResult = cfg_DbServersStop(g_hServer, &m_nStatus);
	CHECK_RESULT;

	return TRUE;
}

static BOOL UnconfigBak()
{
	ASSERT(g_hServer);
	
	int nResult = cfg_DbServersStopAllBackup(g_hServer, &m_nStatus);
	CHECK_RESULT;

	return TRUE;
}

static BOOL UnconfigFS()
{
	ASSERT(g_hServer);

	m_nResult = cfg_FileServerStop(g_hServer, &m_nStatus);
	CHECK_RESULT;

	return TRUE;
}

static BOOL UnconfigSCS()
{
	ASSERT(g_hServer);

	m_nResult = cfg_UpdateServerStop(g_hServer, &m_nStatus);
	CHECK_RESULT;

    // Since we are no longer the SCS machine, we better null this guy.
    g_CfgData.szSysControlMachine[0] = 0;

	return TRUE;
}

static BOOL UnconfigSCC()
{
	ASSERT(g_hServer);

	m_nResult = cfg_UpdateClientStop(g_hServer, cfg_upclientSysBosSuffix, &m_nStatus);
	CHECK_RESULT;

	return TRUE;
}

static BOOL AllServicesUnconfigured()
{
#define NOTCONFIGURED(x)	(((x) == CS_UNCONFIGURE) || ((x) == CS_NULL))

	return (NOTCONFIGURED(g_CfgData.configFS) &&
			NOTCONFIGURED(g_CfgData.configDB) &&
			NOTCONFIGURED(g_CfgData.configBak) &&
			NOTCONFIGURED(g_CfgData.configSCS) &&
			NOTCONFIGURED(g_CfgData.configSCC));
}

static BOOL PostConfig()
{
	ASSERT(g_hServer);

	short isStarted = 0, bosProcsRunning = 0;

	if (!AllServicesUnconfigured())
		return TRUE;

    // If there is now no cell, then stop the client
    if (m_bUnconfiguringLastDBServer) {
        ShowCurrentStep(IDS_STOP_CLIENT_STEP);
        StopClient();
        m_bMustExit = TRUE;
    }

    // Ask user if the config info should be invalided
	g_LogFile.Write("No services are configured so we will ask the user if they want to invalidate the server config info.\r\n");
	int nChoice = Message(MB_OK | MB_YESNO, GetAppTitleID(), IDS_INVALIDATE_CFG_INFO);
	if (nChoice == IDNO) {
		g_LogFile.Write("User has chosen NOT to invalidate the server config info.\r\n");
		return TRUE;
	}

	ShowCurrentStep(IDS_INVALIDATE_CONFIG_INFO_STEP);
	
    g_LogFile.Write("User has chosen to invalidate the server config info.\r\n");

	g_LogFile.Write("Stopping the bos server.\r\n");
	m_nResult = cfg_BosServerStop(g_hServer, BOSSERVER_STOP_TIMEOUT, &m_nStatus);
	CHECK_RESULT;

	g_LogFile.Write("Invalidating the config info.\r\n");
	m_nResult = cfg_HostInvalidate(g_hServer, &m_nStatus);
	CHECK_RESULT;

	m_bCfgInfoInvalidated = TRUE;

    m_bMustExit = TRUE;

	return TRUE;
}

static void VosCloseServer()
{
	if (m_hvosServer) {
		g_LogFile.Write("Closing the connection to this server.\r\n");
		vos_ServerClose(m_hvosServer, &m_nStatus);
		m_hvosServer = 0;
	}
}

static void ShowConfigControls(BOOL bShow)
{
	ShowWnd(m_hDlg, IDC_CURRENT_STEP_LABEL, bShow);
	ShowWnd(m_hDlg, IDC_CURRENT_STEP, bShow);
	ShowWnd(m_hDlg, IDC_CONFIG_PROGRESS, bShow);
	ShowWnd(m_hDlg, IDC_PERCENT_COMPLETE_LABEL, bShow);
	ShowWnd(m_hDlg, IDC_PERCENT_COMPLETE, bShow);
	ShowWnd(m_hDlg, IDC_STATUS_MSG, !bShow);
}

static void UpdateConfigProgress(int nStepNum)
{
	// Update the progress bar
	SendDlgItemMessage(m_hDlg, IDC_CONFIG_PROGRESS, PBM_STEPIT, 0, 0);
			
	// Update the percent complete
	TCHAR buf[16];
	_stprintf(buf, TEXT("%2d%%"), nStepNum * 100 / m_nNumSteps);

	SetWndText(m_hDlg, IDC_PERCENT_COMPLETE, buf);
	ForceUpdateWindow(m_hDlg, IDC_PERCENT_COMPLETE);
}

static void ViewLog()
{
	char szCmdLine[MAX_PATH];

	if (_access(g_LogFile.GetPath(), 0) != 0) {
		ShowError(m_hDlg, 0, IDS_ERROR_NO_LOG_FILE);
		return;
	}

	sprintf(szCmdLine, "notepad.exe %s", g_LogFile.GetPath());
	
	UINT result = WinExec(szCmdLine, SW_SHOW);
	if (result < 32)
		ShowError(m_hDlg, result, IDS_VIEW_LOG_ERROR);
}

static void AssignFailure(int nCurStep, int nLastMainStep)
{
	// A config step has failed, so we will first set its state to
	// failure.  If the step does not have a place on the dialog
	// the we must find some other control to show the red X that
	// indicates failure.

	m_ConfigSteps[nCurStep].eState = SS_STEP_FAILED;

	// Is the step displayed on the dialog?
	if (m_ConfigSteps[nCurStep].nDescID != 0)
		return;

	// It isn't so find nearest one that is
	for (int ii = nCurStep + 1; ii < m_nNumSteps; ii++) {
		CONFIG_STEP& step = m_ConfigSteps[ii];
		if (step.nDescID != 0) {
			step.eState = SS_STEP_FAILED;
			IF_WIZ(ForceUpdateWindow(m_hDlg, step.nGraphicCtrlID));
			return;
		}
	}

	// There is no step on the dialog that is after us, so
	// use the last one that is there.
	m_ConfigSteps[nLastMainStep].eState = SS_STEP_FAILED;
	IF_WIZ(ForceUpdateWindow(m_hDlg, m_ConfigSteps[nLastMainStep].nGraphicCtrlID));
}

static int GetLastMainStep()
{
	for (int ii = m_nNumSteps - 1; ii >= 0; ii--) {
		if (m_ConfigSteps[ii].nDescID != 0)
			return ii;
	}

	ASSERT(FALSE);	// This should never happen!

	return 0;
}

static void ShowConfigFailedMsg()
{
	LPTSTR pszMsg = FormatString(IDS_CONFIG_FAILED, TEXT("%hs%hs"), LOG_FILE_NAME, AFSDIR_SERVER_LOGS_DIRPATH);

	SetWndText(m_hDlg, IDC_STATUS_MSG, pszMsg);

	FreeString(pszMsg);
}

static void ShowViewLogButton()
{
	if (g_pWiz)
		g_pWiz->SetButtonText(IDNEXT, IDS_VIEW_LOG);
	else {
		ShowAndEnable(m_hDlg, IDNEXT);
		MoveWnd(m_hDlg, IDCANCEL, -45, 0);
        // I had to add the code below because of a problem, where if config failed, the
        // error message dialog would display over our window, and when our window next got
        // fully displayed, the buttons would be misdrawn.
        ForceUpdateWindow(m_hDlg, IDNEXT);
        ForceUpdateWindow(m_hDlg, IDCANCEL);
	}
}

static DWORD WINAPI ConfigServer(LPVOID param)
{
	HWND hLogo;
	
	if (!g_pWiz) {
		hLogo = GetDlgItem(m_hDlg, IDC_LOGO);
		AfsAppLib_StartAnimation(hLogo);
	}

	// Disable all buttons (doesn't affect the Cancel button)
	IF_WIZ(g_pWiz->EnableButtons(0));

	g_LogFile.Write("Configuring server\r\n");

	m_bConfiguring = TRUE;
	m_bConfigured = FALSE;
	m_bDbServersRestarted = FALSE;

	// Hide the message window and show the config controls
	ShowConfigControls();

	InitProgressBar();

	BOOL bResult = TRUE;
	int nStepNum = 0;

    int nLastMainStep;
	IF_WIZ(nLastMainStep = GetLastMainStep());
	
	// Loop over each config step performing the ones that are enabled.
	for (int nCurStep = 0; (nCurStep < m_nNumSteps) && bResult; nCurStep++) {
		CONFIG_STEP& step = m_ConfigSteps[nCurStep];

		nStepNum++;

		// Show this step's status message
		ShowCurrentStep(step.nMsgID);

		step.eState = SS_STEP_IN_PROGRESS;

		// If this is a displayed step, then update its display
		if (step.nGraphicCtrlID)
			IF_WIZ(ForceUpdateWindow(m_hDlg, step.nGraphicCtrlID));

		if (CheckCancel())
			break;

		// Perform the config function
		bResult = step.pFunc();

		if (bResult) {
            if (g_pWiz) {
    			// Changing a step's state changes what picture is shown on the dialog
    			// (if that state is displayed on the dialog).
    			
    			// If this is the last displayed step, then don't change its state
    			// because there may still be more steps to perform.  We want to use
    			// the last step's picture to indicate the final config state.
    			
    			// If not last step, then change state
    			if (nCurStep != nLastMainStep)
    				step.eState = SS_STEP_FINISHED;
    
    			// If last step then go ahead and show the state - but do it on the last
    			// step displayed on the dialog (nLastMainStep).
    			if (nCurStep == m_nNumSteps - 1) {
    				m_ConfigSteps[nLastMainStep].eState = SS_STEP_FINISHED;
    				// Do the next line so ForceUpdateWindow below will redraw the
    				// correct control
    				step = m_ConfigSteps[nLastMainStep];
    			}
            }

			UpdateConfigProgress(nStepNum);
		} else {
			// Show the 'X' error marker on the next unprocessed step, or on the
			// last step if there are no more.
			IF_WIZ(AssignFailure(nCurStep, nLastMainStep));
			step.eState = SS_STEP_FAILED;
		}

		if (step.nGraphicCtrlID)
			IF_WIZ(ForceUpdateWindow(m_hDlg, step.nGraphicCtrlID));
	}

	// Close m_hvosServer if it is open
	VosCloseServer();

	// Hide the config controls and show the message window
	ShowConfigControls(FALSE);

	// Did we succeed?
	if (CheckCancel()) {
		g_LogFile.Write("User has canceled configuration.\r\n");
		SetWndText(m_hDlg, IDC_STATUS_MSG, IDS_CONFIG_CANCELED);
		IF_WIZ(g_pWiz->EnableButtons(BACK_BUTTON));
		if (!g_pWiz)
			ShowExitButton();
	} else if (bResult) {
		g_LogFile.Write("Configuration succeeded.\r\n");
		if (g_CfgData.bFirstServer)
			SetWndText(m_hDlg, IDC_STATUS_MSG, IDS_CONFIG_SUCCEEDED_FIRST_SERVER);
		else if (m_bDbServersRestarted)
			SetWndText(m_hDlg, IDC_STATUS_MSG, IDS_CONFIG_SUCCEEDED_NEED_CELLSERVDB_UPDATE);
		else
			SetWndText(m_hDlg, IDC_STATUS_MSG, IDS_CONFIG_SUCCEEDED);
		IF_WIZ(g_pWiz->EnableButtons(NEXT_BUTTON));
		ShowExitButton();
		m_bConfigured = TRUE;
		
		// Disable cancel button
		IF_WIZ(SetEnable(g_pWiz->GetWindow(), IDCANCEL, ES_DISABLE));
	} else {
		g_LogFile.Write("Configuration has failed.\r\n");
		ShowConfigFailedMsg();
		// Prevent partial configuration
		if (!g_CfgData.bValidServerInfo)
			cfg_HostInvalidate(g_hServer, &m_nStatus);
		IF_WIZ(g_pWiz->EnableButtons(NEXT_BUTTON));
		ShowViewLogButton();
		m_bConfigFailed = TRUE;
	}

	if (!g_pWiz)
		AfsAppLib_StopAnimation(hLogo);

	IF_WIZ(g_pWiz->SetDefaultControl(IDNEXT));

	m_bConfiguring = FALSE;

    // Show the user any CellServDB update errors
    if (m_bCellServDbUpdateErr) {
		TCHAR szTitle[cchRESOURCE], szMsg[cchRESOURCE + MAX_PATH];
		GetString(szTitle, GetAppTitleID());
		GetString(szMsg, IDS_CELLSERVDB_UPDATE_ERRORS_ARE_IN_LOG_FILE);
		lstrcat(szMsg, A2S(g_LogFile.GetPath()));
		lstrcat(szMsg, TEXT("."));
		MessageBox(m_hDlg, szMsg, szTitle, MB_OK | MB_ICONEXCLAMATION);
	}

    // Warn the user if we are going to force the config manager to exit
	if (m_bMustExit) {
        if (m_bUnconfiguringLastDBServer)    
    		MsgBox(m_hDlg, IDS_CELL_IS_GONE_MUST_EXIT, GetAppTitleID(), MB_OK | MB_ICONSTOP);
        else
		    MsgBox(m_hDlg, IDS_CONFIG_INFO_INVALIDATED, GetAppTitleID(), MB_OK);
	}

    // Unmap the root afs drive if we had one
    if (m_bRootAfsDriveMappingCreated) {
        // Let's not bother the user if this fails; we'll just log it.
        g_LogFile.Write("Attempting to remove the drive mapping to root.afs: ");
        BOOL bCanceled = WNetCancelConnection(A2S(m_szDriveToMapTo), TRUE) == NO_ERROR;
        g_LogFile.Write(bCanceled ? "succeeded.\r\n" : "failed.\r\n");
        m_bRootAfsDriveMappingCreated = !bCanceled;
    }

	return 0;
}

