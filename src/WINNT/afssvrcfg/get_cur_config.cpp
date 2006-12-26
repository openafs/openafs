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
#include "partition_utils.h"
#include "resource.h"

extern "C" {
#include "afs_vosAdmin.h"
#include "afs\vlserver.h"
}


/*
 * DEFINITIONS _________________________________________________________________
 *
 */
static LPPROGRESSDISPLAY pProg;
static const int MAX_STEPS = 10;
static BOOL bCancel;
static BOOL bBakConfigured;
static vos_vldbEntry_t vldbRootAfsEntry;
static vos_vldbEntry_t vldbRootCellEntry;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */
BOOL CALLBACK GetCurConfigDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp);
static DWORD CALLBACK GetCurrentConfigState(LPPROGRESSDISPLAY ppd, LPARAM lp);
static void ShowMsg(UINT uiMsgID);
static void NextStep(UINT uiMsgID);


/*
 * EXPORTED FUNCTIONS _________________________________________________________________
 *
 */
int GetCurrentConfig(HWND hParent, BOOL& bCanceled)
{
    pProg = 0;
    bCancel = FALSE;
    bBakConfigured = FALSE;

    pProg = New2 (PROGRESSDISPLAY,(hParent, IDD_GET_CURRENT_CONFIG, (DLGPROC)GetCurConfigDlgProc));
    pProg->SetProgressRange(0, MAX_STEPS);

    HWND hLogo = GetDlgItem(pProg->GetWindow(), IDC_LOGO);
    AfsAppLib_StartAnimation(hLogo);

    pProg->Show(GetCurrentConfigState, 0);

    AfsAppLib_StopAnimation(hLogo);

    int dwResult = pProg->GetStatus();

    pProg->Close();

    bCanceled = bCancel;

    return dwResult;
}


/*
 * STATIC FUNCTIONS _________________________________________________________________
 *
 */


/*
 * Dialog Proc _________________________________________________________________
 *
 */
static BOOL CALLBACK GetCurConfigDlgProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    BOOL bOn = FALSE;
	
    switch (msg) {
    case WM_INITDIALOG:
	bCancel = FALSE;
	break;

    case WM_COMMAND:
	switch (LOWORD(wp))
	{
	case IDCANCEL:	if (QueryCancelWiz()) {
	    bCancel = TRUE;
	    ShowMsg(IDS_CANCEL_GET_CONFIG);
	}
	    break;
	}
	break;
    }

    return FALSE;
}

static BOOL IsClientConfigured(BOOL& bConfigured, afs_status_t& nStatus)
{
    if (bCancel)
	return FALSE;

    bConfigured = FALSE;

    NextStep(IDS_CHECK_AFS_CLIENT);

    short isInstalled;
    afs_status_t configStatus;
    char *pszCellName = 0;

    g_LogFile.Write("Is the AFS Client installed on this machine: ");
    if (!cfg_ClientQueryStatus(GetHostnameA(), &isInstalled, &g_CfgData.nClientVersion, &configStatus, &pszCellName, &nStatus)) {
	ImmediateErrorDialog(nStatus, IDS_ERROR_AFS_CLIENT_CHECK);
	return FALSE;
    }

    g_LogFile.WriteBoolResult((BOOL)isInstalled);

    bConfigured = (BOOL)(configStatus == 0);
    if (bConfigured)
	lstrncpy(g_CfgData.szClientCellName, A2S(pszCellName), MAX_CELL_NAME_LEN);
    else
	g_LogFile.WriteError("The client configuration information on this host is not valid", configStatus);

    if (!isInstalled) {
	g_LogFile.Write("ERROR:  AFS Client is not installed.  The AFS Server requires the AFS Client.\r\n");
	ImmediateErrorDialog(0, IDS_ERROR_AFS_CLIENT_NOT_INSTALLED);
	nStatus = -1;	// Just need something nonzero
	return FALSE;
    }

    return TRUE;
}	

/*
 *	NOTE:	This function has an important side effect.  If this machine
 *			is not the first in a new cell, then this function will get the
 *			cell name from the config info.  The cell name may be needed in 
 *			config calls that come later, and so this function must be called
 *			before they are.
 */
static int IsConfigInfoValid(BOOL& bValid, afs_status_t& nStatus)
{
    if (bCancel)
	return FALSE;
	
    afs_status_t configStatus;
    char *pszCellName = 0;

    NextStep(IDS_CHECK_CONFIG_INFO);

    bValid = FALSE;

    g_LogFile.Write("Is there valid configuration information on this machine: ");
    int nResult = cfg_HostQueryStatus(GetHostnameA(), &configStatus, &pszCellName, &nStatus);
    if (!nResult)
	return FALSE;

    g_LogFile.WriteBoolResult((configStatus == 0));

    if (configStatus == 0)
	lstrncpy(g_CfgData.szCellName, A2S(pszCellName), MAX_CELL_NAME_LEN);
    else
	g_LogFile.WriteError("The configuration information on this host is not valid", configStatus);

    bValid = (BOOL)(configStatus == 0);

    return TRUE;
}

static int StartBosServer(afs_status_t& nStatus)
{
    short isStarted;
    short isBosProc;

    g_LogFile.Write("Checking if bos server is running.\r\n");
    int nResult = cfg_BosServerQueryStatus(g_hServer, &isStarted, &isBosProc, &nStatus);
    if (!nResult)
	return nResult;

    if (!isStarted) {
	g_LogFile.Write("Starting the bos server in %s mode.\r\n", g_CfgData.bValidServerInfo ? "auth" : "no auth");
	nResult = cfg_BosServerStart(g_hServer, !g_CfgData.bValidServerInfo, BOSSERVER_START_TIMEOUT, &nStatus);
    }

    return nResult;
}		

static BOOL AreWeLastDBServer(BOOL& bLast, afs_status_t& nStatus)
{
    ASSERT(g_CfgData.szHostname[0]);

    char *pszCellname = 0;
    char *pszCellServDB = 0;

    bLast = FALSE;

    g_LogFile.Write("Checking if this machine is the last DB server in the cell.\r\n");

    g_LogFile.Write("Getting CellServDB from host %s.\r\n", GetHostnameA());
    int nResult = cfg_CellServDbEnumerate(GetHostnameA(), &pszCellname, &pszCellServDB, &nStatus);
    if (!nResult)
	return FALSE;	

    if (!pszCellServDB) {
	g_LogFile.Write("There are no DB servers in CellServDB!!!!!");
	ASSERT(FALSE);		// This should not be possible
	return FALSE;
    }

    char *psz = pszCellServDB;

    int i;
    for (i = 0; *psz; psz += strlen(psz) + 1)
	i++;

    if (i == 1) {
	ASSERT(lstrcmp(g_CfgData.szHostname, A2S(pszCellServDB)) == 0);
	g_LogFile.Write("This machine IS the last DB server in the cell.\r\n");
	bLast = TRUE;
    } else
	g_LogFile.Write("This machine is NOT the last DB server in the cell.\r\n");

    return TRUE;
}

static afs_status_t IsFSConfigured(BOOL& bConfigured)
{
    g_LogFile.Write("Is this machine a file server: ");
	
    bConfigured = FALSE;

    short isStarted;
    afs_status_t nStatus;

    int nResult = cfg_FileServerQueryStatus(g_hServer, &isStarted, &nStatus);
    if (!nResult)
	return nStatus;

    bConfigured = isStarted;	
	
    g_LogFile.WriteBoolResult(bConfigured);

    return 0;
}	

static afs_status_t IsDBConfigured(BOOL& bConfigured)
{
    g_LogFile.Write("Is this machine a db (or bak) server: ");

    bConfigured = FALSE;

    short isStarted, isBakStarted;
    afs_status_t nStatus;

    int nResult = cfg_DbServersQueryStatus(g_hServer, &isStarted, &isBakStarted, 0, &nStatus);
    if (!nResult)
	return nStatus;

    bConfigured = isStarted;
    bBakConfigured = isBakStarted;

    g_LogFile.Write(bConfigured ? "Yes DB" : "No DB");
    g_LogFile.Write(", %s.\r\n", bBakConfigured ? "Yes Bak" : "No Bak");

    return 0;
}	

static afs_status_t IsBakConfigured(BOOL& bConfigured)
{
    bConfigured = bBakConfigured;
	
    return 0;
}

static afs_status_t DoesAPartitionExist(BOOL& bExists)
{
    g_LogFile.Write("Does a partition exist on this machine: ");

    bExists = FALSE;

    afs_status_t nStatus;

    int nResult = ReadPartitionTable(&nStatus);
    if (!nResult)
	return nStatus;

    int nNumPartitions = 0;

    cfg_partitionEntry_t *pTable = GetPartitionTable(nNumPartitions);

    bExists = nNumPartitions > 0;
    if (bExists) {
	g_CfgData.chDeviceName = pTable->deviceName[0];
	lstrcpy(g_CfgData.szPartitionName, ((TCHAR *)A2S(pTable->partitionName)) + lstrlen(TEXT("/vicep")));
    }

    g_LogFile.WriteBoolResult(bExists);

    return 0;
}	

afs_status_t DoRootVolumesExist(BOOL& bExists)
{
    bExists = FALSE;

    afs_status_t nStatus;

    g_LogFile.Write("Do the root volumes exist: ");

    // Does root.afs exist?  If it does not, the call will fail with the VL_NOENT
    // status.  Any other error causes this function to fail.
    int nResult = vos_VLDBGet(g_hCell, 0, 0, "root.afs", &vldbRootAfsEntry, &nStatus);
    if (nResult) {
	g_CfgData.bRootAfsExists = TRUE;
        g_CfgData.nRootAfsID = vldbRootAfsEntry.volumeId[0];
    } else if (nStatus != VL_NOENT)
	return nStatus;

    // Does root.cell exist?
    nResult = vos_VLDBGet(g_hCell, 0, 0, "root.cell", &vldbRootCellEntry, &nStatus);
    if (nResult) {
	g_CfgData.bRootCellExists = TRUE;
        g_CfgData.nRootCellID = vldbRootCellEntry.volumeId[0];
    } else if (nStatus != VL_NOENT)
	return nStatus;

    bExists = g_CfgData.bRootAfsExists && g_CfgData.bRootCellExists;

    g_LogFile.WriteBoolResult(bExists);

    return 0;
}

static BOOL IsVolumeReplicated(vos_vldbEntry_t& vldbEntry)
{
    if (vldbEntry.numServers <= 1)
	return FALSE;

    for (int i = 0; i < vldbEntry.numServers; i++) {
	if ((vldbEntry.volumeSites[i].serverFlags & VOS_VLDB_READ_ONLY) ||
	     (vldbEntry.volumeSites[i].serverFlags & VOS_VLDB_NEW_REPSITE))
	    return TRUE;
    }

    return FALSE;
}

afs_status_t AreRootVolumesReplicated(BOOL& bReplicated)
{
    g_LogFile.Write("Are the root volumes replicated: ");

    // Is root.afs replicated?
    g_CfgData.bRootAfsReplicated = IsVolumeReplicated(vldbRootAfsEntry);

    // Is root.cell replicated?
    g_CfgData.bRootCellReplicated = IsVolumeReplicated(vldbRootCellEntry);

    bReplicated = g_CfgData.bRootAfsReplicated && g_CfgData.bRootCellReplicated;

    g_LogFile.WriteBoolResult(bReplicated);

    return 0;
}	

static afs_status_t IsSCSConfigured(BOOL& bConfigured)
{
    g_LogFile.Write("Is this machine a System Control Server: ");
	
    bConfigured = FALSE;

    short isUpserver, isSC, isBin;
    afs_status_t nStatus;

    int nResult = cfg_UpdateServerQueryStatus(g_hServer, &isUpserver, &isSC, &isBin, &nStatus);
    if (!nResult)
	return nStatus;

    bConfigured = isUpserver && isSC;

    g_LogFile.WriteBoolResult(bConfigured);

    return 0;
}

static afs_status_t IsSCCConfigured(BOOL& bConfigured)
{
    g_LogFile.Write("Is this machine a System Control Client: ");
	
    bConfigured = FALSE;

    short isUpclient, isSCC, isBin;
    afs_status_t nStatus;

    int nResult = cfg_UpdateClientQueryStatus(g_hServer, &isUpclient, &isSCC, &isBin, &nStatus);
    if (!nResult)
	return nStatus;

    bConfigured = isUpclient && isSCC;

    g_LogFile.WriteBoolResult(bConfigured);

    return 0;
}

static void ShowMsg(UINT uiMsgID)
{
    TCHAR szMsg[cchRESOURCE];

    GetString(szMsg, uiMsgID);
	
    pProg->SetOperation(szMsg);
}

static void NextStep(UINT uiMsgID)
{
    static int nCurStep = 1;

    if (bCancel)
	return;
	
    ShowMsg(uiMsgID);

    pProg->SetProgress(nCurStep++);

    Sleep(50);
}

static BOOL CheckConfigState(afs_status_t (*ConfigCheckFunc)(BOOL&), CONFIG_STATE& state, afs_status_t& nStatus, UINT uiMsgID)
{
    BOOL bState = FALSE;

    if (bCancel)
	return FALSE;

    NextStep(uiMsgID);

    nStatus = ConfigCheckFunc(bState);
    if (nStatus) {
	g_LogFile.WriteError("Config check failed", nStatus);
	return FALSE;
    }

    state = bState ? CS_ALREADY_CONFIGURED : CS_NULL;

    return TRUE;
}
		
static DWORD CALLBACK GetCurrentConfigState(LPPROGRESSDISPLAY ppd, LPARAM lp)
{
    afs_status_t nStatus = 0;

    ASSERT(pProg);

    g_LogFile.Write("Checking this machine's current configuration...\r\n");

    if (!IsClientConfigured(g_CfgData.bValidClientInfo, nStatus))
	return nStatus;

    if (!IsConfigInfoValid(g_CfgData.bValidServerInfo, nStatus))
	return nStatus;

    // If the server and client have good config info, and the client is in a 
    // different cell than the server, then the config routines will want to
    // reconfigure the client.  To do so they need information that we already
    // know and don't have to ask the user for.  Prefill this information here.
    if (g_CfgData.bValidClientInfo && g_CfgData.bValidServerInfo && 
        (lstrcmp(g_CfgData.szCellName, g_CfgData.szClientCellName) != 0))
    {
        lstrcpy(g_CfgData.szCellServDbHostname, g_CfgData.szHostname);
    }

    if (!GetLibHandles(&nStatus))
	return nStatus;

    if (!CheckConfigState(DoesAPartitionExist, g_CfgData.configPartition, nStatus, IDS_CHECK_PARTITION))
	return nStatus;

    if (g_CfgData.bValidServerInfo) {
	// Must check if bos server is running, and start it if it isn't.  We can't determine
	// if the services are running except by asking the bosserver.
	if (!StartBosServer(nStatus))
	    return nStatus;

	if (!CheckConfigState(IsFSConfigured, g_CfgData.configFS, nStatus, IDS_CHECK_FS_CONFIG))
	    return nStatus;

	if (!CheckConfigState(IsDBConfigured, g_CfgData.configDB, nStatus, IDS_CHECK_DB_CONFIG))
	    return nStatus;

	if (g_CfgData.configDB == CS_ALREADY_CONFIGURED) {
	    if (!AreWeLastDBServer(g_CfgData.bLastDBServer, nStatus))
		return nStatus;
	}

	if (!CheckConfigState(IsBakConfigured, g_CfgData.configBak, nStatus, IDS_CHECK_BAK_CONFIG))
	    return nStatus;

	if (!CheckConfigState(DoRootVolumesExist, g_CfgData.configRootVolumes, nStatus, IDS_CHECK_ROOT_AFS))
            return nStatus;

        g_CfgData.bRootVolumesExistanceKnown = TRUE;

	if (!CheckConfigState(AreRootVolumesReplicated, g_CfgData.configRep, nStatus, IDS_CHECK_REP))
            return nStatus;
        
        g_CfgData.bRootVolumesReplicationKnown = TRUE;

	if (!CheckConfigState(IsSCSConfigured, g_CfgData.configSCS, nStatus, IDS_CHECK_SCS))
	    return nStatus;

	if (!CheckConfigState(IsSCCConfigured, g_CfgData.configSCC, nStatus, IDS_CHECK_SCC))
	    return nStatus;
    }		

    if (!bCancel)
	pProg->SetProgress(MAX_STEPS);
	
    return 0;
}

