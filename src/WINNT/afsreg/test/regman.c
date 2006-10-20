/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Manual utility for testing registry access functions. Also serves
 * as a pre Configuration Manager registry configuration utility.
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <afs/param.h>
#include <afs/stds.h>
#include <afs/com_err.h>
#include <afs/cmd.h>
#include <afs/dirpath.h>

#include <WINNT/afsreg.h>
#include <WINNT/afssw.h>
#include <WINNT/vptab.h>


static char* whoami;


static int DoVptList(struct cmd_syndesc *as, char *arock)
{
    struct vpt_iter vpiter;
    struct vptab vpentry;

    if (!vpt_Start(&vpiter)) {
	while (!vpt_NextEntry(&vpiter, &vpentry)) {
	    printf("Partition: %s    Device: %s\n",
		   vpentry.vp_name, vpentry.vp_dev);
	}

	(void)vpt_Finish(&vpiter);
    }
    return 0;
}

static int DoVptAdd(struct cmd_syndesc *as, char *arock)
{
    char *vpName, *vpDev;
    struct vptab vpentry;

    vpName = as->parms[0].items->data;
    vpDev = as->parms[1].items->data;

    if (!vpt_PartitionNameValid(vpName)) {
	com_err(whoami, 0, "Partition name invalid");
	return 1;
    }

    if (!vpt_DeviceNameValid(vpDev)) {
	com_err(whoami, 0, "Device name invalid");
	return 1;
    }

    strcpy(vpentry.vp_name, vpName);
    strcpy(vpentry.vp_dev, vpDev);

    if (vpt_AddEntry(&vpentry)) {
	com_err(whoami, 0, "Unable to create vice partition table entry");
	return 1;
    }
    return 0;
}

static int DoVptDel(struct cmd_syndesc *as, char *arock)
{
    char *vpName;

    vpName = as->parms[0].items->data;

    if (!vpt_PartitionNameValid(vpName)) {
	com_err(whoami, 0, "Partition name invalid");
	return 1;
    }

    if (vpt_RemoveEntry(vpName) && errno != ENOENT) {
	com_err(whoami, 0, "Unable to remove vice partition table entry");
	return 1;
    }
    return 0;
}


static int DoDirGet(struct cmd_syndesc *as, char *arock)
{
    char *buf;

    if (afssw_GetServerInstallDir(&buf)) {
	com_err(whoami, 0,
		"Failed reading AFS install dir entry (or does not exist)");
	return 1;
    }

    printf("AFS server installation directory: %s\n", buf);

    free(buf);
    return (0);
}


static int DoDirSet(struct cmd_syndesc *as, char *arock)
{
    long status;
    HKEY key;
    char *afsPath;

    afsPath = as->parms[0].items->data;

    /* open AFS sw version key; create if does not exist */
    status = RegOpenKeyAlt(AFSREG_NULL_KEY, AFSREG_SVR_SW_VERSION_KEY,
			   KEY_WRITE, 1 /* create */, &key, NULL);

    if (status == ERROR_SUCCESS) {
	/* write AFS directory value */
	status = RegSetValueEx(key, AFSREG_SVR_SW_VERSION_DIR_VALUE,
			       0, REG_SZ, afsPath, strlen(afsPath) + 1);

	RegCloseKey(key);
    }

    if (status) {
	com_err(whoami, 0, "Unable to set AFS installation directory entry");
    }

    return (status ? 1 : 0);
}


static int DoBosCfg(struct cmd_syndesc *as, char *arock)
{
    char bosSvcPath[AFSDIR_PATH_MAX];
    SC_HANDLE scmHandle, svcHandle;

    /* determine if using specified or default service binary path */
    if (as->parms[0].items) {
	/* BOS control service binary path specified */
	sprintf(bosSvcPath, "\"%s\"", as->parms[0].items->data);
    } else {
	/* no BOS control service binary path specified; check for default */
	char *dirBuf;

	if (afssw_GetServerInstallDir(&dirBuf)) {
	    com_err(whoami, 0,
		    "binary path not specified and AFS server installation directory not set");
	    return 1;
	}
	sprintf(bosSvcPath, "\"%s%s/%s\"",
		dirBuf,
		AFSDIR_CANONICAL_SERVER_BIN_DIRPATH,
		AFSREG_SVR_SVC_IMAGENAME_DATA);
    }

    /* create BOS control service */
    scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

    if (scmHandle == NULL) {
	DWORD status = GetLastError();
	char *reason = "";

	if (status == ERROR_ACCESS_DENIED) {
	    reason = "(insufficient privilege)";
	}
	com_err(whoami, 0, "unable to connect to the SCM %s", reason);
	return 1;
    }

    svcHandle = CreateService(scmHandle,
			      AFSREG_SVR_SVC_NAME,
			      AFSREG_SVR_SVC_DISPLAYNAME_DATA,
			      STANDARD_RIGHTS_REQUIRED,
			      SERVICE_WIN32_OWN_PROCESS,
			      SERVICE_AUTO_START,
			      SERVICE_ERROR_NORMAL,
			      bosSvcPath,
			      NULL,   /* load order group */
			      NULL,   /* tag id */
			      NULL,   /* dependencies */
			      NULL,   /* service start name */
			      NULL);  /* password */

    if (svcHandle == NULL) {
	DWORD status = GetLastError();
	char *reason = "";

	if (status == ERROR_SERVICE_EXISTS || status == ERROR_DUP_NAME) {
	    reason = "(service or display name already exists)";
	}
	com_err(whoami, 0, "unable to create service %s", reason);
	CloseServiceHandle(scmHandle);
	return 1;
    }

    CloseServiceHandle(svcHandle);
    CloseServiceHandle(scmHandle);
    return (0);
}


static int DoBosDel(struct cmd_syndesc *as, char *arock)
{
    int rc = 0;
    SC_HANDLE scmHandle, svcHandle;

    scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (scmHandle == NULL) {
	DWORD status = GetLastError();
	char *reason = "";

	if (status == ERROR_ACCESS_DENIED) {
	    reason = "(insufficient privilege)";
	}
	com_err(whoami, 0, "unable to connect to the SCM %s", reason);
	return 1;
    }

    svcHandle = OpenService(scmHandle,
			    AFSREG_SVR_SVC_NAME, STANDARD_RIGHTS_REQUIRED);

    if (svcHandle == NULL) {
	DWORD status = GetLastError();

	if (status != ERROR_SERVICE_DOES_NOT_EXIST) {
	    com_err(whoami, 0, "unable to open service");
	    rc = 1;
	}
	CloseServiceHandle(scmHandle);
	return rc;
    }

    if (!DeleteService(svcHandle)) {
	DWORD status = GetLastError();

	if (status != ERROR_SERVICE_MARKED_FOR_DELETE) {
	    com_err(whoami, 0, "service delete failed");
	    rc = 1;
	}
    }
    CloseServiceHandle(svcHandle);
    CloseServiceHandle(scmHandle);
    return (rc);
}

static int DoVersionGet(struct cmd_syndesc *as, char *arock)
{
    unsigned major, minor, patch;

    printf("\n");

    if (!afssw_GetClientVersion(&major, &minor, &patch)) {
	printf("Client: major = %u, minor = %u, patch = %u\n",
	       major, minor, patch);
    } else {
	printf("Client version information not available.\n");
    }

    if (!afssw_GetServerVersion(&major, &minor, &patch)) {
	printf("Server: major = %u, minor = %u, patch = %u\n",
	       major, minor, patch);
    } else {
	printf("Server version information not available.\n");
    }
    return 0;
}


static void
SetupVptCmd(void)
{
    struct cmd_syndesc	*ts;

    ts = cmd_CreateSyntax("vptlist", DoVptList, 0,
			  "list vice partition table");

    ts = cmd_CreateSyntax("vptadd", DoVptAdd, 0,
			  "add entry to vice partition table");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED, "partition name");
    cmd_AddParm(ts, "-dev", CMD_SINGLE, CMD_REQUIRED, "device name");

    ts = cmd_CreateSyntax("vptdel", DoVptDel, 0,
			  "remove entry from vice partition table");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED, "partition name");
}

static void
SetupDirCmd(void)
{
    struct cmd_syndesc	*ts;

    ts = cmd_CreateSyntax("dirget", DoDirGet, 0,
			  "display the AFS server installation directory");

    ts = cmd_CreateSyntax("dirset", DoDirSet, 0,
			  "set the AFS server installation directory");
    cmd_AddParm(ts, "-path", CMD_SINGLE, CMD_REQUIRED, "directory path");
}

static void
SetupBosCmd(void)
{
    struct cmd_syndesc	*ts;

    ts = cmd_CreateSyntax("boscfg", DoBosCfg, 0,
			  "configure the AFS BOS control service");
    cmd_AddParm(ts, "-path", CMD_SINGLE, CMD_OPTIONAL, "service binary path");

    ts = cmd_CreateSyntax("bosdel", DoBosDel, 0,
			  "delete (unconfigure) the AFS BOS control service");
}

static void
SetupVersionCmd(void)
{
    struct cmd_syndesc	*ts;

    ts = cmd_CreateSyntax("version", DoVersionGet, 0,
			  "display AFS version information");
}




int main(int argc, char *argv[])
{
    int code;

    whoami = argv[0];

    /* initialize command syntax */
    initialize_CMD_error_table();

    SetupVptCmd();
    SetupDirCmd();
    SetupBosCmd();
    SetupVersionCmd();

    /* execute command */
    code = cmd_Dispatch(argc, argv);

    return (code);
}
