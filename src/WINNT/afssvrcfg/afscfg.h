/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	_H_AFSCFG_H_
#define _H_AFSCFG_H_


extern "C" {
#include <afs\afs_cfgAdmin.h>
#include <afs\afs_utilAdmin.h>
}
#include <WINNT\afsapplib.h>
#include <tchar.h>
#include <crtdbg.h>
#include "hourglass.h"
#include "toolbox.h"
#include "logfile.h"
#include "char_conv.h"
#include "validation.h"


#define	ASSERT(x)		_ASSERTE((x))

#define	LOG_FILE_NAME	"afs_server_config_log.txt"


/* Enum for the steps in the wizard.  Used to index into the array of steps. */
enum StateID {
   sidSTEP_ONE,
   sidSTEP_TWO,
   sidSTEP_THREE,
   sidSTEP_FOUR,
   sidSTEP_FIVE,
   sidSTEP_SIX,
   sidSTEP_SEVEN,
   sidSTEP_EIGHT,
   sidSTEP_NINE,
   sidSTEP_TEN,
   sidSTEP_ELEVEN,
   sidSTEP_TWELVE
};


// These are the possible states for a configuration step.
// The first 4 are mutually exclusive.  The last one can
// be combined with any of the others, though it doesn't
// mean anything to combine it with CS_NULL.
#define	CS_NULL					0		// NULL state
#define	CS_DONT_CONFIGURE		1		// Do not perform the configuration step
#define	CS_CONFIGURE			2		// Perform the configuration step
#define	CS_ALREADY_CONFIGURED	3		// The step has already been performed, so we don't 
										// need to do it.
#define CS_UNCONFIGURE			4		// Unconfigure the step
#define CS_DISABLED				32		// The step cannot be performed because it is dependent
										// on some other step that is not to be performed and 
										// has not already been performed.

// I know I could have used an enum for this, but then tacking on the CS_DISABLED
// flag to the enum would have been a bit sleazy.
typedef int CONFIG_STATE;

// Timeout values
#define BOSSERVER_START_TIMEOUT     2 * 60      // 2 minutes in seconds
#define BOSSERVER_STOP_TIMEOUT      30 * 60     // 30 minutes in seconds

// Sizes for various arrays
#define MAX_PARTITION_NAME_LEN		8	//cfg_maxPartitionNameLen
#define MAX_MACHINE_NAME_LEN		128 //AFS_MAX_SERVER_NAME_LEN
#define MAX_CELL_NAME_LEN			64
#define MAX_SERVER_PW_LEN			32
#define MAX_ADMIN_NAME_LEN			256
#define MAX_ADMIN_PW_LEN			32
#define MAX_UID_LEN					12


/*
 *	This sturcture contains all of the information needed to configure
 *	a machine.
 */
struct CFG_DATA {
	BOOL bWizard;
	
	CONFIG_STATE configFS;			// File server
	CONFIG_STATE configDB;			// Database server
	CONFIG_STATE configBak;			// Backup server
	CONFIG_STATE configPartition;	// Create a partition
	CONFIG_STATE configRootVolumes;	// Create root.afs and root.cell
	CONFIG_STATE configRep;			// Replicate root.afs and root.cell
	CONFIG_STATE configSCS;			// System Control server
	CONFIG_STATE configSCC;			// System Control client

	BOOL bLastDBServer;

    UINT nRootAfsID;				// Volume ID of root.afs
    UINT nRootCellID;				// Volume ID of root.cell

	BOOL bRootAfsExists;
	BOOL bRootCellExists;
 
    // If we cannot determine if the root volumes exist, then this 
    // variable will be FALSE.
    BOOL bRootVolumesExistanceKnown;
	
    BOOL bRootAfsReplicated;
	BOOL bRootCellReplicated;
    BOOL bRootVolumesReplicationKnown;

	// Partition info
	TCHAR chDeviceName;				// Just the drive to use
	TCHAR szPartitionName[MAX_PARTITION_NAME_LEN + 1];

	// Sys control machine if configuring as a system control client and also to
	// use to propagate CellServDB, if configuring a db server
	TCHAR szSysControlMachine[MAX_MACHINE_NAME_LEN + 1];

	// Server and Cell information
	TCHAR szCellName[MAX_CELL_NAME_LEN + 1];
	TCHAR szServerPW[MAX_SERVER_PW_LEN + 1];
	TCHAR szAdminName[MAX_ADMIN_NAME_LEN + 1];
	TCHAR szAdminPW[MAX_ADMIN_PW_LEN + 1];
	TCHAR szAdminUID[MAX_UID_LEN + 1];
	BOOL bUseNextUid;
	BOOL bFirstServer;
	BOOL bAuthServerRunning;		// Only used when bFirstServer is true
	BOOL bAdminPrincipalCreated;	// Only used when bFirstServer is true

	// This machine's local name
	TCHAR szLocalName[MAX_MACHINE_NAME_LEN + 1];

	// This machine's hostname
	TCHAR szHostname[MAX_MACHINE_NAME_LEN + 1];

	// Hostname of machine to get CellServDB from
	TCHAR szCellServDbHostname[MAX_MACHINE_NAME_LEN + 1];

	// Is the AFS Client configured?
	BOOL bValidClientInfo;
	TCHAR szClientCellName[MAX_CELL_NAME_LEN + 1];
	unsigned nClientVersion;

	// Does this machine have valid config info?
	BOOL bValidServerInfo;

    // Can we reuse the admin name and password?
    // For use by config manager - if false we will prompt user to login.
    BOOL bReuseAdminInfo;

    // Handle of the thread performing a salvage
    HANDLE hSalvageThread;
    TCHAR szSalvageLogFileName[_MAX_PATH];
};


// Must include this after the definition of CONFIG_STATE;
#include "cfg_utils.h"


// These are defined in afscfg.cpp
extern LPWIZARD g_pWiz;			// The wizard
extern LPPROPSHEET g_pSheet;	// The config tool property sheet
extern CFG_DATA g_CfgData;		// The config data

extern void *g_hToken;			// Current token obtained by this app
extern void *g_hCell;			// Handle to the cell we are configuring into
extern void *g_hClient;			// Client's config library handle
extern void *g_hServer;			// Server's config library handle

extern size_t g_nNumStates;
extern UINT g_StateDesc[];		// Res ID's of descriptions of each wizard step

extern LOGFILE g_LogFile;		// Our log file

BOOL CALLBACK WizStep_Common_DlgProc (HWND hRHS, UINT msg, WPARAM wp, LPARAM lp);
BOOL QueryCancelWiz();
BOOL GetLibHandles(afs_status_t *pStatus);
BOOL GetHandles(HWND hParentDlg);   // More convenient version if you only need to know if it succeeded


/*
 *	Accessor functions for g_CfgData__________________________________________
 */

// TCHAR versions
TCHAR GetDeviceName();
LPTSTR GetPartitionName();
LPTSTR GetHostName();
LPTSTR GetSysControlMachine();
LPTSTR GetCellName();
LPTSTR GetServerPW();
LPTSTR GetAdminName();
LPTSTR GetAdminPW();
LPTSTR GetAdminUID();
LPTSTR GetLocalName();
LPTSTR GetHostname();
LPTSTR GetCellServDbHostname();
LPTSTR GetClientCellName();
LPTSTR GetClientNetbiosName();
LPTSTR GetSalvageLogFileName();

// char versions
char GetDeviceNameA();
char *GetPartitionNameA();
char *GetSysControlMachineA();
char *GetCellNameA();
char *GetServerPWA();
char *GetAdminNameA();
char *GetAdminPWA();
char *GetAdminUIDA();
char *GetLocalNameA();
char *GetHostnameA();
char *GetCellServDbHostnameA();
char *GetClientCellNameA();
char *GetClientNetbiosNameA();
char *GetSalvageLogFileNameA();

#endif	// _H_AFSCFG_H_

