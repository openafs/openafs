/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * INCLUDES ___________________________________________________________________
 *
 */
extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "resource.h"
#include <WINNT\afsapplib.h>


/*
 * DEFINITIONS _________________________________________________________________
 *
 */

/*
 * Wizard Help _________________________________________________________________
 *
 */

// Intro page
static UINT IDH_INTRO_PAGE = 101;

// Config server page
static UINT IDH_CONFIG_SERVER_PAGE = 102;

// Backup server page
static UINT IDH_BACKUP_SERVER_PAGE			= 0;
static UINT IDH_CONFIG_BACKUP_SERVER		= 1;
static UINT IDH_DONT_CONFIG_BACKUP_SERVER	= 2;

// File server page
static UINT IDH_FILE_SERVER_PAGE			= 3;
static UINT IDH_SHOULD_CONFIG_FILE_SERVER	= 4;
static UINT IDH_DONT_CONFIG_FILE_SERVER		= 5;

// Cell and Server info page
static UINT IDH_INFO_PAGE			= 6;
static UINT IDH_JOIN_EXISTING_CELL	= 7;
static UINT IDH_FIRST_SERVER		= 8;
static UINT IDH_CELL_NAME			= 9;
static UINT IDH_PRINCIPAL			= 10;
static UINT IDH_SERVER_PW			= 11;
static UINT IDH_VERIFY_PW			= 12;

// Administrative info page (when configuring first server)
static UINT IDH_FIRST_SERVER_PAGE	= 13;
static UINT IDH_ADMIN_NAME			= 14;
static UINT IDH_ADMIN_PW			= 15;
static UINT IDH_VERIFY_ADMIN_PW		= 16;
static UINT IDH_USE_NEXT_UID		= 17;
static UINT IDH_USE_THIS_UID		= 18;
static UINT IDH_AFS_UID				= 19;
static UINT IDH_AFS_UID_SPINNER		= 20;

// Administrative info page (when configuring into an existing cell)
static UINT IDH_EXISTING_CELL_PAGE	= 21;
static UINT IDH_ADMIN_NAME2			= 22;
static UINT IDH_ADMIN_PW2			= 23;
static UINT IDH_HOSTNAME			= 24;

// Database server page
static UINT IDH_DB_SERVER_PAGE	        = 100;
static UINT IDH_CONFIG_DB_SERVER		= 25;
static UINT IDH_DONT_CONFIG_DB_SERVER	= 26;
static UINT IDH_SYS_CONTROL_MACHINE		= 27;

// Partition page
static UINT IDH_PARTITION_PAGE			= 28;
static UINT IDH_CREATE_PARTITION		= 29;
static UINT IDH_DRIVE_LIST				= 30;
static UINT IDH_PARTITION_NAME			= 31;
static UINT IDH_DONT_CREATE_PARTITION	= 32;

// Root volumes page
static UINT IDH_ROOT_VOLUMES_PAGE			= 33;
static UINT IDH_CREATE_VOLUME				= 34;
static UINT IDH_DONT_CREATE_ROOT_VOLUMES	= 35;

// Replication page
static UINT IDH_REPLICATION_PAGE	= 36;
static UINT IDH_REPLICATE			= 37;
static UINT IDH_DONT_REPLICATE		= 38;

// System control page
static UINT IDH_SYS_CONTROL_PAGE		= 39;
static UINT IDH_SYS_CONTROL_SERVER		= 40;
static UINT IDH_SYS_CONTROL_CLIENT		= 41;
static UINT IDH_SYS_CONTROL_MACHINE2	= 42;
static UINT IDH_DONT_CONFIGURE			= 43;



/*
 *
 *	Arrays that map help topics to the appropriate controls___________________
 *
 */

static DWORD IDH_INTRO_CTX_HELP[] = {
    0,  0
};

static DWORD IDH_CONFIG_SERVER_CTX_HELP[] = {
    0,  0
};

static DWORD IDH_BACKUP_SERVER_CTX_HELP[] = {
	IDC_CONFIG_BACKUP_SERVER,		IDH_CONFIG_BACKUP_SERVER,
	IDC_DONT_CONFIG_BACKUP_SERVER,	IDH_DONT_CONFIG_BACKUP_SERVER,
    0,                              0
};

static DWORD IDH_FILE_SERVER_CTX_HELP[] = {
	IDC_SHOULD_CONFIG_FILE_SERVER,		IDH_SHOULD_CONFIG_FILE_SERVER,
	IDC_DONT_CONFIG_FILE_SERVER,		IDH_DONT_CONFIG_FILE_SERVER,
    0,                                  0
};

static DWORD IDH_CELL_AND_SERVER_INFO_CTX_HELP[] = {
	IDC_JOIN_EXISTING_CELL,	IDH_JOIN_EXISTING_CELL,
	IDC_FIRST_SERVER,		IDH_FIRST_SERVER,
	IDC_CELL_NAME,			IDH_CELL_NAME,
	IDC_PRINCIPAL,			IDH_PRINCIPAL,
	IDC_SERVER_PW,			IDH_SERVER_PW,
	IDC_VERIFY_PW,			IDH_VERIFY_PW,
    0,                      0
};

static DWORD IDH_FIRST_SERVER_CTX_HELP[] = {
	IDC_ADMIN_NAME,			IDH_ADMIN_NAME,
	IDC_ADMIN_PW,			IDH_ADMIN_PW,
	IDC_VERIFY_ADMIN_PW,	IDH_VERIFY_ADMIN_PW,
	IDC_USE_NEXT_UID,		IDH_USE_NEXT_UID,
	IDC_USE_THIS_UID,		IDH_USE_THIS_UID,
	IDC_AFS_UID,			IDH_AFS_UID,
	IDC_AFS_UID_SPINNER,	IDH_AFS_UID_SPINNER,
    0,                      0
};

static DWORD IDH_EXISTING_CELL_CTX_HELP[] = {
	IDC_ADMIN_NAME,		IDH_ADMIN_NAME2,
	IDC_ADMIN_PW,		IDH_ADMIN_PW2,
	IDC_HOSTNAME,		IDH_HOSTNAME,
    0,                  0
};

static DWORD IDH_DB_SERVER_CTX_HELP[] = {
	IDC_CONFIG_DB_SERVER,		IDH_CONFIG_DB_SERVER,
	IDC_DONT_CONFIG_DB_SERVER,	IDH_DONT_CONFIG_DB_SERVER,
	IDC_SYS_CONTROL_MACHINE,	IDH_SYS_CONTROL_MACHINE,
    0,                          0
};

static DWORD IDH_PARTITION_CTX_HELP[] = {
	IDC_CREATE_PARTITION,		IDH_CREATE_PARTITION,
	IDC_DRIVE_LIST,				IDH_DRIVE_LIST,
	IDC_PARTITION_NAME,			IDH_PARTITION_NAME,
	IDC_DONT_CREATE_PARTITION,	IDH_DONT_CREATE_PARTITION,
    0,                          0
};

static DWORD IDH_ROOT_VOLUMES_CTX_HELP[] = {
	IDC_CREATE_ROOT_VOLUMES,		IDH_CREATE_VOLUME,
	IDC_DONT_CREATE_ROOT_VOLUMES,	IDH_DONT_CREATE_ROOT_VOLUMES,
    0,                              0
};

static DWORD IDH_REPLICATION_CTX_HELP[] = {
	IDC_REPLICATE,		IDH_REPLICATE,
	IDC_DONT_REPLICATE,	IDH_DONT_REPLICATE,
    0,                  0
};

static DWORD IDH_SYS_CONTROL_CTX_HELP[] = {
	IDC_SYS_CONTROL_SERVER,		IDH_SYS_CONTROL_SERVER,
	IDC_SYS_CONTROL_CLIENT,		IDH_SYS_CONTROL_CLIENT,
	IDC_SYS_CONTROL_MACHINE,	IDH_SYS_CONTROL_MACHINE2,
	IDC_DONT_CONFIGURE,			IDH_DONT_CONFIGURE,
    0,                          0
};


/*
 *
 *	Config tool help__________________________________________________________
 *
 */

// Create partition dialog
static UINT IDH_CREATE_PARTITION_DLG	= 44;
static UINT IDH_CT_DRIVE_LIST			= 45;
static UINT IDH_CT_PARTITION_NAME		= 46;
static UINT IDH_CREATE					= 47;
static UINT IDH_CLOSE					= 48;

// Partitions property page
static UINT IDH_PARTITIONS_PAGE		= 49;
static UINT IDH_PARTITIONS_LIST		= 50;
static UINT IDH_CREATE_PARTITIONS	= 51;
static UINT IDH_SALVAGE				= 52;
static UINT IDH_REFORMAT			= 53;
static UINT IDH_REMOVE				= 54;

// Services page
static UINT IDH_SERVICES_PAGE	= 55;
static UINT IDH_FS_SERVICE		= 56;
static UINT IDH_DB_SERVICE		= 57;
static UINT IDH_DB_DETAILS		= 58;
static UINT IDH_BK_SERVICE		= 59;
static UINT IDH_SCS				= 60;
static UINT IDH_SCC				= 61;
static UINT IDH_SC_MACHINE		= 62;

// Admin info (and other server) page
static UINT IDH_ADMIN_INFO_DLG	= 63;
static UINT IDH_CT_ADMIN_NAME	= 64;
static UINT IDH_CT_ADMIN_PW		= 65;
static UINT IDH_CT_HOSTNAME		= 66;
static UINT IDH_CONFIG			= 67;

// Salvage dialog
static UINT IDH_SALVAGE_DLG             = 68;
static UINT IDH_SERVER                  = 69;
static UINT IDH_PARTITION               = 70;
static UINT IDH_VOLUME                  = 71;
static UINT IDH_VOLUME_NAME             = 72;
static UINT IDH_ADVANCED                = 73;
static UINT IDH_LOG_FILE                = 74;
static UINT IDH_NUM_PROCESSES           = 75;
static UINT IDH_TEMP_DIR                = 76;
static UINT IDH_NUM_PROCESSES_CHECKBOX  = 77;

// Salvage results dialog
static UINT IDH_SALVAGE_RESULTS_DLG     = 80;
static UINT IDH_LOG                     = 81;
static UINT IDH_CLOSE_LOG               = 82;


/*
 *
 *	Arrays that map help topics to the appropriate controls___________________
 *
*/

static DWORD IDH_CREATE_PARTITION_CTX_HELP[] = {
	IDC_DRIVE_LIST,			IDH_DRIVE_LIST,
	IDC_PARTITION_NAME,		IDH_CT_PARTITION_NAME,
	IDC_CREATE,				IDH_CREATE,
	IDC_CLOSE,				IDH_CLOSE,
    0,                      0
};

static DWORD IDH_PARTITIONS_CTX_HELP[] = {
	IDC_PARTITION_LIST,		IDH_PARTITIONS_LIST,
	IDC_CREATE_PARTITIONS,	IDH_CREATE_PARTITIONS,
	IDC_SALVAGE,			IDH_SALVAGE,
	IDC_REFORMAT,			IDH_REFORMAT,
	IDC_REMOVE,				IDH_REMOVE,
    0,                      0
};

static DWORD IDH_SERVICES_CTX_HELP[] = {
	IDC_FS_SERVICE,		IDH_FS_SERVICE,
	IDC_DB_SERVICE,		IDH_DB_SERVICE,
	IDC_DB_DETAILS,		IDH_DB_DETAILS,
	IDC_BK_SERVICE,		IDH_BK_SERVICE,
	IDC_SCS,			IDH_SCS,
	IDC_SCC,			IDH_SCC,
	IDC_SC_MACHINE,		IDH_SC_MACHINE,
    0,                  0
};

static DWORD IDH_ADMIN_INFO_CTX_HELP[] = {
	IDC_ADMIN_NAME,		IDH_CT_ADMIN_NAME,
	IDC_ADMIN_PW,		IDH_CT_ADMIN_PW,
	IDC_HOSTNAME,		IDH_CT_HOSTNAME,
	IDOK,				IDH_CONFIG,
    0,                  0
};

static DWORD IDH_SALVAGE_CTX_HELP[] = {
    IDC_SERVER,                 IDH_SERVER,
    IDC_PARTITION,              IDH_PARTITION,
    IDC_VOLUME,                 IDH_VOLUME,
    IDC_VOLUME_NAME,            IDH_VOLUME_NAME,
    IDC_ADVANCED,               IDH_ADVANCED,
    IDC_LOG_FILE,               IDH_LOG_FILE,
    IDC_NUM_PROCESSES,          IDH_NUM_PROCESSES,
    IDC_TEMP_DIR,               IDH_TEMP_DIR,
    IDC_NUM_PROCESSES_CHECKBOX, IDH_NUM_PROCESSES_CHECKBOX,
    0,                          0
};

static DWORD IDH_SALVAGE_RESULTS_CTX_HELP[] = {
    IDC_LOG,                    IDH_LOG,
    IDC_CLOSE,                  IDH_CLOSE_LOG,
    0,                          0
};


/*
 *
 *	Help for both__________________________________________________________
 *
 */

// Get 3.4 server afs principal password dialog
static UINT IDH_GET_PW_DLG      = 78;
static UINT IDH_PW              = 79;


/*
 *
 *	Arrays that map help topics to the appropriate controls___________________
 *
*/

static DWORD IDH_GET_PW_CTX_HELP[] = {
    IDC_PW,     IDH_PW,
    0,          0
};




/*
 * EXPORTED FUNCTIONS _________________________________________________________
 *
 */
void RegisterConfigToolHelp()
{
	AfsAppLib_RegisterHelp(IDD_CREATE_PARTITION, IDH_CREATE_PARTITION_CTX_HELP, IDH_CREATE_PARTITION_DLG);
	AfsAppLib_RegisterHelp(IDD_PARTITIONS_PAGE, IDH_PARTITIONS_CTX_HELP, IDH_PARTITIONS_PAGE);
	AfsAppLib_RegisterHelp(IDD_SERVICES_PAGE, IDH_SERVICES_CTX_HELP, IDH_SERVICES_PAGE);
	AfsAppLib_RegisterHelp(IDD_ADMIN_INFO, IDH_ADMIN_INFO_CTX_HELP, IDH_ADMIN_INFO_DLG);
   	AfsAppLib_RegisterHelp(IDD_GET_PW, IDH_GET_PW_CTX_HELP, IDH_GET_PW_DLG);
   	AfsAppLib_RegisterHelp(IDD_SALVAGE, IDH_SALVAGE_CTX_HELP, IDH_SALVAGE_DLG);
   	AfsAppLib_RegisterHelp(IDD_SALVAGE_RESULTS, IDH_SALVAGE_RESULTS_CTX_HELP, IDH_SALVAGE_RESULTS_DLG);
}

void RegisterWizardHelp()
{
	AfsAppLib_RegisterHelp(IDD_INTRO_PAGE, IDH_INTRO_CTX_HELP, IDH_INTRO_PAGE);	
    AfsAppLib_RegisterHelp(IDD_INFO_PAGE, IDH_CELL_AND_SERVER_INFO_CTX_HELP, IDH_INFO_PAGE);
	AfsAppLib_RegisterHelp(IDD_INFO_PAGE2_FIRST_SERVER, IDH_FIRST_SERVER_CTX_HELP, IDH_FIRST_SERVER_PAGE);
	AfsAppLib_RegisterHelp(IDD_INFO_PAGE2_NOT_FIRST_SERVER, IDH_EXISTING_CELL_CTX_HELP, IDH_EXISTING_CELL_PAGE);
	AfsAppLib_RegisterHelp(IDD_FILE_SERVER_PAGE, IDH_FILE_SERVER_CTX_HELP, IDH_FILE_SERVER_PAGE);
	AfsAppLib_RegisterHelp(IDD_DB_SERVER_PAGE, IDH_DB_SERVER_CTX_HELP, IDH_DB_SERVER_PAGE);
	AfsAppLib_RegisterHelp(IDD_BACKUP_SERVER_PAGE, IDH_BACKUP_SERVER_CTX_HELP, IDH_BACKUP_SERVER_PAGE);
	AfsAppLib_RegisterHelp(IDD_PARTITION_PAGE, IDH_PARTITION_CTX_HELP, IDH_PARTITION_PAGE);
	AfsAppLib_RegisterHelp(IDD_ROOT_VOLUMES_PAGE, IDH_ROOT_VOLUMES_CTX_HELP, IDH_ROOT_VOLUMES_PAGE);
	AfsAppLib_RegisterHelp(IDD_REPLICATION_PAGE, IDH_REPLICATION_CTX_HELP, IDH_REPLICATION_PAGE);
	AfsAppLib_RegisterHelp(IDD_SYS_CONTROL_PAGE, IDH_SYS_CONTROL_CTX_HELP, IDH_SYS_CONTROL_PAGE);
   	AfsAppLib_RegisterHelp(IDD_GET_PW, IDH_GET_PW_CTX_HELP, IDH_GET_PW_DLG);
    AfsAppLib_RegisterHelp(IDD_CONFIG_SERVER_PAGE, IDH_CONFIG_SERVER_CTX_HELP, IDH_CONFIG_SERVER_PAGE);	
}

