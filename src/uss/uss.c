/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *	Main module for the AFS user account management tool.
 */

/*
 * --------------------- Required definitions ---------------------
 */
#include <stdlib.h>
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif

#include <string.h>

#include "uss_common.h"		/*Common uss definitions, globals */
#include "uss_procs.h"		/*Main uss operations */
#include "uss_kauth.h"		/*AuthServer routines */
#include "uss_fs.h"		/*CacheManager ops */
#include <afs/cmd.h>		/*Command line parsing */
#include <afs/cellconfig.h>	/*Cell config defs */
#include <afs/kautils.h>	/*MAXKTCREALMLEN & MAXKTCNAMELEN */
#include <ubik.h>

/*
 * Label certain things which will be activated at a later time,
 * as well as certain semi-implemented features/switches which need
 * to be hidden for the general AFS 3.2 release.
 */
#define USS_FUTURE_FEATURES		1
#define USS_DONT_HIDE_SOME_FEATURES	0

/*
 * ---------------------- Exported variables ----------------------
 */
char *uss_fs_InBuff = NULL;	/*Cache Manager input  buff */
char *uss_fs_OutBuff = NULL;	/*Cache Manager output buff */

/*
 * Set up convenient tags for the command line parameter indicies.
 */

/*Add*/
#define AUP_USER	 0
#define AUP_REALNAME	 1
#define AUP_PASSWD	 2
#define AUP_PWEXPIRES    3
#define AUP_SERVER	 4	/* was 3 */
#define AUP_PART	 5	/* was 4 */
#define AUP_MNTPT	 6	/* was 5 */
#define AUP_UID		 7	/* was 6 */

/*Bulk*/
#define ABULK_FILE	 0

extern int uss_perr;
/*Delete*/
#define DUP_USER	 0
#define DUP_MNTPT	 1
#define DUP_RESTOREDIR	 2
#define DUP_SAVEVOLUME	 3
#define DUP_PWDPATH	 4
#define DUP_PWDFORMAT	 5

/*PurgeVolumes*/
#define PVP_VOLNAME	 0
#define PVP_VOLFILE	 1

/*Common ones*/
#define AUSS_TEMPLATE 	10	/* was 7 */
#define AUSS_VERBOSE  	11	/* was 8 */
#define AUSS_VAR      	12	/* was 9 */
#define AUSS_CELL     	13	/* was 10 */
#define AUSS_ADMIN      14	/* was 11 */
#define AUSS_DRYRUN     15	/* was 12 */
#define AUSS_SKIPAUTH   16	/* was 13 */
#define AUSS_OVERWRITE  17	/* was 14 */
#define AUSS_PWEXPIRES  18	/* was 15 */
#define AUSS_PIPE	19	/*  was 16 */

#undef USS_DB

static char Template[300] = "uss.template";	/*Default name */

extern FILE *yyin, *yyout;	/*YACC input & output files */
extern int doUnlog;
int uss_BulkExpires = 0;
int local_Cell = 1;

static int DoAdd();

/*-----------------------------------------------------------------------
 * static GetCommon
 *
 * Description:
 *	Read in the command line arguments common to all uss operations.
 *
 * Arguments:
 *	a_as : Ptr to the command line syntax descriptor.
 *
 * Returns:
 *	0 (always)
 *
 * Environment:
 *	May exit the program if trouble is encountered determining the
 *	cell name.  Set up as the command line parser's BeforeProc().
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
GetCommon(register struct cmd_syndesc *a_as, void *arock)
{				/*GetCommon */

    int code;			/*Result of ka_LocalCell */

    if (strcmp(a_as->name, "help") == 0)
	return;
    if (a_as->parms[AUSS_TEMPLATE].items)
	strcpy(Template, a_as->parms[AUSS_TEMPLATE].items->data);
    if (a_as->parms[AUSS_VERBOSE].items)
	uss_verbose = 1;
    else
	uss_verbose = 0;

    code = ka_CellConfig(AFSDIR_CLIENT_ETC_DIRPATH);
    if (code)
	fprintf(stderr, "%s: ** Call to ka_CellConfig() failed (code=%d)\n",
		uss_whoami, code);

    if (a_as->parms[AUSS_CELL].items) {
	char local_cell[MAXKTCREALMLEN];
	if (ka_ExpandCell
	    (a_as->parms[AUSS_CELL].items->data, uss_Cell, 0 /*local */ )) {
	    fprintf(stderr, "%s: ** Unknown or ambiguous cell name: %s\n",
		    uss_whoami, a_as->parms[AUSS_CELL].items->data);
	    exit(-1);
	}
	/*
	 * Get the local cell name
	 */
	if (ka_ExpandCell((char *)0, local_cell, 0 /*local */ )) {
	    fprintf(stderr, "Can't get local cellname\n");
	    exit(-1);
	}
	if (strcmp(uss_Cell, local_cell)) {
	    /*
	     * Not the same; not a local cell
	     */
	    local_Cell = 0;
	}
    } else {
	/*
	 * Get the local cell name
	 */
	if (ka_ExpandCell((char *)0, uss_Cell, 0 /*local */ )) {
	    fprintf(stderr, "Can't get local cellname\n");
	    exit(-1);
	}
	if (uss_verbose)
	    fprintf(stderr, "No cell specified; assuming '%s'.\n", uss_Cell);
    }

    return (0);

}				/*GetCommon */


/*-----------------------------------------------------------------------
 * static SaveRestoreInfo
 *
 * Description:
 *	Save all the information required to restore the currently
 *	parsed user account.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	0 if everything went well,
 *	1 if something went wrong in the function, or
 *	Lower-level error code if something went wrong below us.
 *
 * Environment:
 *	We need to determine and store the following new pieces of
 *	information:
 *		User's AFS ID
 *		Name of user's volume
 *		FileServer & partition hosting the volume
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
SaveRestoreInfo()
{				/*SaveRestoreInfo */

    static char rn[] = "uss:SaveRestoreInfo";	/*Routine name */
    register afs_int32 code;	/*Return code */
    afs_int32 deletedUid;	/*Uid to be nuked */

    /*
     * Translate the user name to the user ID.
     */
    code = uss_ptserver_XlateUser(uss_User, &deletedUid);
    if (code)
	return (code);
#ifdef USS_DB
    printf("%s: User '%s' translated to uid %d\n", rn, uss_User, deletedUid);
#endif /* USS_DB */
    sprintf(uss_Uid, "%d", deletedUid);

    /*
     * Pull out the name of the volume at the given mountpoint, along
     * with the name of the FileServer and partition hosting it.  This
     * also sets up all the numerical info for the above.
     */
    code = uss_vol_GetVolInfoFromMountPoint(uss_MountPoint);
    if (code)
	return (code);

    /*
     * Report back that we did fine.
     */
    return (0);

}				/*SaveRestoreInfo */


/*-----------------------------------------------------------------------
 * static DoDelete
 *
 * Description:
 *	With everything properly inserted into the common variables,
 *	delete the specified user account.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	0 if everything went well,
 *	1 if something went wrong in the function, or
 *	Lower-level error code if something went wrong below us.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
DoDelete()
{				/*DoDelete */

    int code;			/*Return code */

    /*
     * Make sure the user name is a lega one.
     */
    code = uss_kauth_CheckUserName();
    if (code)
	return (code);

    /*
     * Store all the info about the account before actually doing
     * anything.
     */
    code = SaveRestoreInfo();
    if (code)
	return (code);

    if ((uss_VolumeID != 0) && (uss_MountPoint[0] != '\0')) {
	/*
	 * Unmount the user's volume from the file system.
	 */
	if (uss_verbose) {
	    fprintf(stderr,
		    "Unmounting volume '%s' (ID %u) mounted at '%s'\n",
		    uss_Volume, uss_VolumeID, uss_MountPoint);
	}

	code = uss_fs_RmMountPoint(uss_MountPoint);
	if (code) {
	    if (uss_verbose)
		fprintf(stderr, "%s: Can't remove mountpoint '%s'\n",
			uss_whoami, uss_MountPoint);
	    return (code);	/* Must return - we may have incorrect volume */
	}
    }

    /*
     * If our caller has elected to delete the user's volume now,
     * then do so.
     */
    if (!uss_SaveVolume && (uss_VolumeID != 0)) {
	if (uss_verbose) {
	    fprintf(stderr, "Deleting volume '%s' (ID %u)\n", uss_Volume,
		    uss_VolumeID);
	}

	code =
	    uss_vol_DeleteVol(uss_Volume, uss_VolumeID, uss_Server,
			      uss_ServerID, uss_Partition, uss_PartitionID);
	if (code) {
	    if (uss_verbose)
		fprintf(stderr, "%s: Can't delete volume '%s' (ID %u)\n",
			uss_whoami, uss_Volume, uss_VolumeID);
	    return (code);
	}
    } else if (uss_verbose && (uss_MountPoint[0] != '\0'))
	printf("%s: Warning: Not attempting to delete volume at '%s'\n",
	       uss_whoami, uss_MountPoint);

    /*
     * Get rid of the user's authentication entry.
     */
    code = uss_kauth_DelUser(uss_User);
    if (code)
	return (code);

    /*
     * Finally, remove the user's AFS ID from the Protection DB and
     * return that result.
     */
    code = uss_ptserver_DelUser(uss_User);
    return (code);

}				/*DoDelete */


/*-----------------------------------------------------------------------
 * static DelUser
 *
 * Description:
 *	Process the given (non-bulk) delete command.
 *
 * Arguments:
 *	a_as   : Ptr to the command line syntax descriptor.
 *	a_rock : Ptr to the rock passed in.
 *
 * Returns:
 *	0 if everything went well,
 *	1 if something went wrong in the function, or
 *	Lower-level error code if something went wrong below us.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
DelUser(struct cmd_syndesc *a_as, void *a_rock)
{				/*DelUser */

    int code;

    /*
     * Before we do anything else, make sure we initialize the
     * global field settings.
     */
    uss_common_Reset();

    /*
     * Pull out the fields as passed in by the caller on the command
     * line.
     */
    strcpy(uss_User, a_as->parms[DUP_USER].items->data);
    if (a_as->parms[DUP_MNTPT].items)
	strcpy(uss_MountPoint, a_as->parms[DUP_MNTPT].items->data);
#if USS_FUTURE_FEATURES
#if USS_DONT_HIDE_SOME_FEATURES
    strcpy(uss_RestoreDir, a_as->parms[DUP_RESTOREDIR].items->data);
#endif /* USS_DONT_HIDE_SOME_FEATURES */
#endif /* USS_FUTURE_FEATURES */

/*
    if (a_as->parms[DUP_SAVEVOLUME].items)
	uss_SaveVolume = 1;
*/

    if (a_as->parms[2].items) {
	uss_SaveVolume = 1;
    }
#if USS_FUTURE_FEATURES
#if USS_DONT_HIDE_SOME_FEATURES
    if (a_as->parms[DUP_PWDPATH].items)
	strcpy(uss_PwdPath, a_as->parms[DUP_PWDPATH].items->data);
    if (a_as->parms[DUP_PWDFORMAT].items)
	strcpy(uss_PwdFormat, a_as->parms[DUP_PWDFORMAT].items->data);
#endif /* USS_DONT_HIDE_SOME_FEATURES */
#endif /* USS_FUTURE_FEATURES */

    if (a_as->parms[AUSS_DRYRUN].items)
	uss_DryRun = 1;
    if (a_as->parms[AUSS_SKIPAUTH].items)
	uss_SkipKaserver = 1;
    if (a_as->parms[AUSS_ADMIN].items) {
	strcpy(uss_Administrator, a_as->parms[AUSS_ADMIN].items->data);
	/*      fprintf(stderr, "debugging: uss_Administrator set to '%s'\n",
	 * uss_Administrator); */
    } else {
	/*      fprintf(stderr, "debugging: No administrator value given\n"); */
	uss_Administrator[0] = '\0';
    }

    /*
     * Initialize uss_AccountCreator().
     */
    code = uss_kauth_InitAccountCreator();
    if (code)
	return (code);

    /*
     * Now that the command line arguments are parsed and properly stored,
     * go for it!
     */

    return (DoDelete());

}				/*DelUser */


/*-----------------------------------------------------------------------
 * static PurgeVolumes
 *
 * Description:
 *	Purge the given volume(s).
 *
 * Arguments:
 *	a_as   : Ptr to the command line syntax descriptor.
 *	a_rock : Ptr to the rock passed in.
 *
 * Returns:
 *	0 if everything went well,
 *	1 if something went wrong in the function, or
 *	Lower-level error code if something went wrong below us.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
PurgeVolumes(struct cmd_syndesc *a_as, void *a_rock)
{				/*PurgeVolumes */

    fprintf(stderr, "Sorry, purgevolumes has not yet been implemented.\n");
    return (0);

}				/*PurgeVolumes */


/*-----------------------------------------------------------------------
 * static RestoreUser
 *
 * Description:
 *	Process the given delete command.
 *
 * Arguments:
 *	a_as   : Ptr to the command line syntax descriptor.
 *	a_rock : Ptr to the rock passed in.
 *
 * Returns:
 *	0 if everything went well,
 *	1 if something went wrong in the function, or
 *	Lower-level error code if something went wrong below us.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
RestoreUser(struct cmd_syndesc *a_as, void *a_rock)
{				/*RestoreUser */

    fprintf(stderr, "Sorry, restoreuser has not yet been implemented.\n");
    return (0);

}				/*RestoreUser */


/*-----------------------------------------------------------------------
 * static DoBulkAddLine
 *
 * Description:
 *	Process the given bulk add command.
 *
 * Arguments:
 *	a_buf : Ptr to the buffer holding the bulk add command.
 *	a_tp  : Ptr to the first char past the opcode.
 *
 * Returns:
 *	0  if everything went well,
 *	-1 if something went wrong in the function, or
 *	Lower-level error code if something went wrong below us.
 *
 * Environment:
 *	The required fields are:
 *		-user
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
DoBulkAddLine(a_buf, a_tp)
     char *a_buf;
     char *a_tp;

{				/*DoBulkAddLine */

    register int i;		/*Loop variable */
    static char rn[] = "DoBulkAddLine";	/*Routine name */
    int overflow;		/*Overflow in field copy? */

#ifdef USS_DB
    printf("%s: Command buffer left to parse: '%s'\n", rn, a_tp);
#endif /* USS_DB */
    uss_Expires = uss_BulkExpires;

    /*
     * Pull out all the fields.
     */
    a_tp = uss_common_FieldCp(uss_User, a_tp, ':', uss_UserLen, &overflow);
    if (overflow) {
	fprintf(stderr,
		"%s: * User field in add cmd too long (max is %d chars; truncated value is '%s')\n",
		uss_whoami, uss_UserLen, uss_User);
	return (-1);
    }
    if ((*a_tp == '\0') || (*a_tp == '\n')) {
	fprintf(stderr,
		"%s: * The user field must appear in a bulk add command.\n",
		uss_whoami);
	return (-1);
    }

    a_tp =
	uss_common_FieldCp(uss_RealName, a_tp, ':', uss_RealNameLen,
			   &overflow);
    if (overflow) {
	fprintf(stderr,
		"%s: * Real name field in add cmd too long (max is %d chars; truncated value is '%s')\n",
		uss_whoami, uss_RealNameLen, uss_RealName);
	return (-1);
    }
    if (uss_RealName[0] == '\0') {
	/*
	 * The user's real name has not been supplied.  As a
	 * default, we use the account name.
	 */
	sprintf(uss_RealName, "%s", uss_User);
	if (uss_verbose)
	    fprintf(stderr, "%s: Using default real name, '%s'\n", uss_whoami,
		    uss_User);
    }
    /*Use default full name */
    a_tp = uss_common_FieldCp(uss_Pwd, a_tp, ':', uss_PwdLen, &overflow);
    if (overflow) {
	fprintf(stderr,
		"%s: * Password field in add cmd too long (max is %d chars; truncated value is '%s')\n",
		uss_whoami, uss_PwdLen, uss_Pwd);
	return (-1);
    }
    if (uss_Pwd[0] == '\0') {
	/*
	 * The user's password has not been provided.  Use
	 * the default.
	 */
	sprintf(uss_Pwd, "%s", uss_DEFAULT_PASSWORD);
	if (uss_verbose)
	    fprintf(stderr, "%s: Using default password, '%s'\n", uss_whoami,
		    uss_Pwd);
    }				/*Use default password */
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	goto DoBulkAddLine_ParsingDone;


    {
	char temp[10];
	a_tp = uss_common_FieldCp(temp, a_tp, ':', 9, &overflow);
	if (overflow) {
	    fprintf(stderr,
		    "%s: * Password expiration time is longer than %d characters, ignoring...\n",
		    uss_whoami, 9);
	}
	if (temp[0] == '\0') {
	    /* Expiration time not specified.  Use default */
	    if (uss_verbose)
		fprintf(stderr, "%s: Using default expiration time, '%d'\n",
			uss_whoami, uss_Expires);
	} else {
	    int te;
	    te = atoi(temp);
	    if (te < 0 || te > 254) {
		fprintf(stderr,
			"%s: * Password Expiration must be in [0..254] days, using default %d\n",
			uss_whoami, uss_Expires);
	    } else
		uss_Expires = te;
	}

	if ((*a_tp == '\0') || (*a_tp == '\n'))
	    goto DoBulkAddLine_ParsingDone;
    }


    a_tp =
	uss_common_FieldCp(uss_Server, a_tp, ':', uss_ServerLen, &overflow);
    if (overflow) {
	fprintf(stderr,
		"%s: * Server field in add cmd too long (max is %d chars; truncated value is '%s')\n",
		uss_whoami, uss_ServerLen, uss_Server);
	return (-1);
    }
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	goto DoBulkAddLine_ParsingDone;

    a_tp =
	uss_common_FieldCp(uss_Partition, a_tp, ':', uss_PartitionLen,
			   &overflow);
    if (overflow) {
	fprintf(stderr,
		"%s: * Partition field in add cmd too long (max is %d chars; truncated value is '%s')\n",
		uss_whoami, uss_PartitionLen, uss_Partition);
	return (-1);
    }
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	goto DoBulkAddLine_ParsingDone;

    a_tp =
	uss_common_FieldCp(uss_MountPoint, a_tp, ':', uss_MountPointLen,
			   &overflow);
    if (overflow) {
	fprintf(stderr,
		"%s: * Mountpoint field in add cmd too long (max is %d chars; truncated value is '%s')\n",
		uss_whoami, uss_MountPointLen, uss_MountPoint);
	return (-1);
    }
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	goto DoBulkAddLine_ParsingDone;

    a_tp = uss_common_FieldCp(uss_Uid, a_tp, ':', uss_UidLen, &overflow);
    if (overflow) {
	fprintf(stderr,
		"%s: * UID field in add cmd too long (max is %d chars; truncated value is '%s')\n",
		uss_whoami, uss_UidLen, uss_Uid);
	return (-1);
    }
    uss_DesiredUID = atoi(uss_Uid);
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	goto DoBulkAddLine_ParsingDone;

    for (uss_VarMax = 1; uss_VarMax < 10; uss_VarMax++) {
	a_tp =
	    uss_common_FieldCp(uss_Var[uss_VarMax], a_tp, ':',
			       uss_MAX_ARG_SIZE, &overflow);
	if (overflow) {
	    fprintf(stderr,
		    "%s: * Variable %d field in add cmd too long (max is %d chars; truncated value is '%s')\n",
		    uss_whoami, uss_VarMax, uss_MAX_ARG_SIZE,
		    uss_Var[uss_VarMax]);
	    return (-1);
	}
	if ((*a_tp == '\0') || (*a_tp == '\n'))
	    goto DoBulkAddLine_ParsingDone;
    }

  DoBulkAddLine_ParsingDone:
    /*
     * If there's anything left on the line, we ignore it.  Announce
     * the bulk add parameters we've parsed or filled in if we're
     * being verbose, then go for it.
     */
    if (uss_verbose) {
	fprintf(stderr,
		"\nAdding user '%s' ('%s'), password='%s' on server '%s', partition '%s', home directory='%s'",
		uss_User, uss_RealName, uss_Pwd,
		(uss_Server[0] != '\0' ? uss_Server : "<default>"),
		(uss_Partition[0] != '\0' ? uss_Partition : "<default>"),
		(uss_MountPoint[0] != '\0' ? uss_MountPoint : "<default>"));
	if (uss_DesiredUID)
	    fprintf(stderr, ", uid preset to %d\n", uss_DesiredUID);
	else
	    fprintf(stderr, ", no preset uid\n");
	for (i = 1; i <= uss_VarMax; i++) {
	    if (uss_Var[i][0] != '\0')
		fprintf(stderr, "$%1d='%s' ", i, uss_Var[i]);
	}
	if (uss_VarMax > 0)
	    fprintf(stderr, "\n");
    }

    /*Verbose status of add command */
    /*
     * Now do the real work.
     */
    return (DoAdd());

}				/*DoBulkAddLine */


/*-----------------------------------------------------------------------
 * static DoBulkDeleteLine
 *
 * Description:
 *	Process the given bulk delete command.
 *
 * Arguments:
 *	a_buf : Ptr to the buffer holding the bulk delete command.
 *	a_tp  : Ptr to the first char past the opcode.
 *
 * Returns:
 *	0  if everything went well,
 *	-1 if something went wrong in the function, or
 *	Lower-level error code if something went wrong below us.
 *
 * Environment:
 *	The required fields are:
 *		-user, -mountpoint, -restoredir
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
DoBulkDeleteLine(a_buf, a_tp)
     char *a_buf;
     char *a_tp;

{				/*DoBulkDeleteLine */

    char volField[32];		/*Value of optional vol disposition field */
    int overflow;		/*Was there an overflow in field copying? */

    /*
     * Pull out all the fields.
     */
    a_tp = uss_common_FieldCp(uss_User, a_tp, ':', uss_UserLen, &overflow);
    if (overflow) {
	fprintf(stderr,
		"%s: * User field in delete cmd too long (max is %d chars; truncated value is '%s')\n",
		uss_whoami, uss_UserLen, uss_User);
	return (-1);
    }
    if ((uss_User[0] == '\0') || (*a_tp == '\0') || (*a_tp == '\n'))
	goto Delete_MissingRequiredParam;

    a_tp =
	uss_common_FieldCp(uss_MountPoint, a_tp, ':', uss_MountPointLen,
			   &overflow);
    if (overflow) {
	fprintf(stderr,
		"%s: * Mountpoint field in delete cmd too long (max is %d chars; truncated value is '%s')\n",
		uss_whoami, uss_MountPointLen, uss_MountPoint);
	return (-1);
    }
#if USS_FUTURE_FEATURES
#if USS_DONT_HIDE_SOME_FEATURES
    if ((uss_MountPoint[0] == '\0') || (*a_tp == '\0') || (*a_tp == '\n'))
	goto Delete_MissingRequiredParam;
#endif /* USS_DONT_HIDE_SOME_FEATURES */
#else
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	goto Delete_ParsingDone;
#endif /* USS_FUTURE_FEATURES */

#if USS_FUTURE_FEATURES
#if USS_DONT_HIDE_SOME_FEATURES
    a_tp =
	uss_common_FieldCp(uss_RestoreDir, a_tp, ':', uss_RestoreDirLen,
			   &overflow);
    if (overflow) {
	fprintf(stderr,
		"%s: * RestoreDir field in delete cmd too long (max is %d chars; truncated value is '%s')\n",
		uss_whoami, uss_RestoreDirLen, uss_RestoreDir);
	return (-1);
    }
    if (uss_RestoreDir[0] == '\0')
	goto Delete_MissingRequiredParam;
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	goto Delete_ParsingDone;

    a_tp =
	uss_common_FieldCp(uss_PwdPath, a_tp, ':', uss_PwdPathLen, &overflow);
    if (overflow) {
	fprintf(stderr,
		"%s: * Password path field in delete cmd too long (max is %d chars; truncated value is '%s')\n",
		uss_whoami, uss_PwdPathLen, uss_PwdPath);
	return (-1);
    }
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	goto Delete_ParsingDone;

    a_tp =
	uss_common_FieldCp(uss_PwdFormat, a_tp, ':', uss_PwdFormatLen,
			   &overflow);
    if (overflow) {
	fprintf(stderr,
		"%s: * Password format field in delete cmd too long (max is %d chars; truncated value is '%s')\n",
		uss_whoami, uss_PwdFormatLen, uss_PwdFormat);
	return (-1);
    }
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	goto Delete_ParsingDone;
#endif /* USS_DONT_HIDE_SOME_FEATURES */
#endif /* USS_FUTURE_FEATURES */

    a_tp = uss_common_FieldCp(volField, a_tp, ':', 32, &overflow);
    if (overflow) {
	fprintf(stderr,
		"%s: * Volume save/del field in delete cmd too long (max is 32 chars; truncated value is '%s')\n",
		uss_whoami, volField);
	return (-1);
    }
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	goto Delete_ParsingDone;

    if (strcmp(volField, "delvolume") == 0)
	uss_SaveVolume = 0;
    else
	uss_SaveVolume = 1;

  Delete_ParsingDone:
    /*
     * If there's anything left on the line, we ignore it.  Announce
     * the bulk delete parameters we've parsed if we're being verbose,
     * then go for it.
     */
    if (uss_verbose) {
#if USS_FUTURE_FEATURES
#if USS_DONT_HIDE_SOME_FEATURES
	fprintf(stderr,
		"\nDeleting user '%s' mounted at '%s', restoredir='%s', pwd path='%s', pwd format='%s'",
		uss_User, uss_MountPoint, uss_RestoreDir, uss_PwdPath,
		uss_PwdFormat);
#endif /* USS_DONT_HIDE_SOME_FEATURES */
#else
	fprintf(stderr, "\nDeleting user '%s' mounted at '%s'", uss_User,
		uss_MountPoint);
#endif /* USS_FUTURE_FEATURES */
	if (uss_SaveVolume)
	    fprintf(stderr, ", saving user's volume\n");
	else
	    fprintf(stderr, ", deleting user's volume\n");
    }

    /*Verbose status of delete command */
    /*
     * Now do the real work.
     */
    return (DoDelete());

  Delete_MissingRequiredParam:
    fprintf(stderr,
	    "%s: * All of the user, mountpoint, and restoredir fields must appear in a bulk delete command line.\n",
	    uss_whoami);
    return (-1);

}				/*DoBulkDeleteLine */

#if USS_FUTURE_FEATURES
#if USS_DONT_HIDE_SOME_FEATURES
/*-----------------------------------------------------------------------
 * static DoBulkPurgeVolumeLine
 *
 * Description:
 *	Process the given bulk add command.
 *
 * Arguments:
 *	a_buf : Ptr to the buffer holding the bulk add command.
 *	a_tp  : Ptr to the first char past the opcode.
 *
 * Returns:
 *	0  if everything went well,
 *	-1 if something went wrong in the function, or
 *	Lower-level error code if something went wrong below us.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
DoBulkPurgeVolumeLine(a_buf, a_tp)
     char *a_buf;
     char *a_tp;

{				/*DoBulkPurgeVolumeLine */

    register int i;		/*Loop variable */
    int overflow;		/*Did a field copy overflow happen? */

    /*
     * Pull out all the fields.
     */
    a_tp = uss_common_FieldCp(uss_User, a_tp, ':', uss_UserLen, &overflow);
    if (overflow) {
	fprintf(stderr,
		"%s: * User field in purgevolume cmd too long (max is %d chars; truncated value is '%s')\n",
		uss_whoami, uss_UserLen, uss_User);
	return (-1);
    }
    if ((*a_tp == '\0') || (*a_tp == '\n')) {
	fprintf(stderr,
		"%s: * The user field must appear in a bulk add command.\n",
		uss_whoami);
	return (-1);
    }

    a_tp =
	uss_common_FieldCp(uss_RealName, a_tp, ':', uss_RealNameLen,
			   &overflow);
    if (overflow) {
	fprintf(stderr,
		"%s: * Real name field in purgevolume cmd too long (max is %d chars; truncated value is '%s')\n",
		uss_whoami, uss_RealNameLen, uss_RealName);
	return (-1);
    }
    if (uss_RealName[0] == '\0') {
	/*
	 * The user's real name has not been supplied.  As a
	 * default, we use the account name.
	 */
	sprintf(uss_RealName, "%s", uss_User);
	if (uss_verbose)
	    fprintf(stderr, "%s: Using default real name, '%s'\n", uss_whoami,
		    uss_User);
    }
    /*Use default full name */
    a_tp = uss_common_FieldCp(uss_Pwd, a_tp, ':', uss_PwdLen, &overflow);
    if (overflow) {
	fprintf(stderr,
		"%s: * Password field in purgevolume cmd too long (max is %d chars; truncated value is '%s')\n",
		uss_whoami, uss_PwdLen, uss_Pwd);
	return (-1);
    }
    if (uss_Pwd[0] == '\0') {
	/*
	 * The user's password has not been provided.  Use
	 * the default.
	 */
	sprintf(uss_Pwd, "%s", uss_DEFAULT_PASSWORD);
	if (uss_verbose)
	    fprintf(stderr, "%s: Using default password, '%s'\n", uss_whoami,
		    uss_Pwd);
    }				/*Use default password */
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	return (0);

    a_tp =
	uss_common_FieldCp(uss_Server, a_tp, ':', uss_ServerLen, &overflow);
    if (overflow) {
	fprintf(stderr, "%s: * Server field too long (max is %d chars)\n",
		uss_whoami, uss_ServerLen);
	return (-1);
    }
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	return (0);

    a_tp =
	uss_common_FieldCp(uss_Partition, a_tp, ':', uss_PartitionLen,
			   &overflow);
    if (overflow) {
	fprintf(stderr, "%s: * Partition field too long (max is %d chars)\n",
		uss_whoami, uss_PartitionLen);
	return (-1);
    }
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	return (0);

    a_tp =
	uss_common_FieldCp(uss_MountPoint, a_tp, ':', uss_MountPointLen,
			   &overflow);
    if (overflow) {
	fprintf(stderr, "%s: * Mountpoint field too long (max is %d chars)\n",
		uss_whoami, uss_MountPointLen);
	return (-1);
    }
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	return (0);

    a_tp = uss_common_FieldCp(uss_Uid, a_tp, ':', uss_UidLen, &overflow);
    if (overflow) {
	fprintf(stderr, "%s: * UID field too long (max is %d chars)\n",
		uss_whoami, uss_UidLen);
	return (-1);
    }
    uss_DesiredUID = atoi(uss_Uid);
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	return (0);

    for (uss_VarMax = 1; uss_VarMax < 10; uss_VarMax++) {
	a_tp =
	    uss_common_FieldCp(uss_Var[uss_VarMax], a_tp, ':',
			       uss_MAX_ARG_SIZE, &overflow);
	if (overflow) {
	    fprintf(stderr,
		    "%s: * Variable %d field too long (max is %d chars)\n",
		    uss_whoami, uss_VarMax, uss_MAX_ARG_SIZE);
	    return (-1);
	}
	if ((*a_tp == '\0') || (*a_tp == '\n'))
	    return (0);
    }

    /*
     * If there's anything left on the line, we ignore it.  Announce
     * the bulk add parameters we've parsed or filled in if we're
     * being verbose, then go for it.
     */
    if (uss_verbose) {
	fprintf(stderr,
		"\nAdding user '%s' ('%s'), password='%s' on server '%s', partition '%s', home directory='%s'",
		uss_User, uss_RealName, uss_Pwd,
		(uss_Server[0] != '\0' ? uss_Server : "<default>"),
		(uss_Partition[0] != '\0' ? uss_Partition : "<default>"),
		(uss_MountPoint[0] != '\0' ? uss_MountPoint : "<default>"));
	if (uss_DesiredUID)
	    fprintf(stderr, ", uid preset to %d\n", uss_DesiredUID);
	else
	    fprintf(stderr, ", no preset uid\n");
	for (i = 1; i <= uss_VarMax; i++) {
	    if (uss_Var[i][0] != '\0')
		fprintf(stderr, "$%1d='%s' ", i, uss_Var[i]);
	}
	if (uss_VarMax > 0)
	    fprintf(stderr, "\n");
    }

    /*Verbose status of add command */
    /*
     * Now do the real work.
     */
    return (DoAdd());

}				/*DoBulkPurgeVolumeLine */
#endif /* USS_DONT_HIDE_SOME_FEATURES */
#endif /* USS_FUTURE_FEATURES */

#if USS_FUTURE_FEATURES
#if USS_DONT_HIDE_SOME_FEATURES
/*-----------------------------------------------------------------------
 * static DoBulkRestoreLine
 *
 * Description:
 *	Process the given bulk add command.
 *
 * Arguments:
 *	a_buf : Ptr to the buffer holding the bulk add command.
 *	a_tp  : Ptr to the first char past the opcode.
 *
 * Returns:
 *	0  if everything went well,
 *	-1 if something went wrong in the function, or
 *	Lower-level error code if something went wrong below us.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
DoBulkRestoreLine(a_buf, a_tp)
     char *a_buf;
     char *a_tp;

{				/*DoBulkRestoreLine */

    register int i;		/*Loop variable */
    int overflow;		/*Overflow occur on field copy? */

    /*
     * Pull out all the fields.
     */
    a_tp = uss_common_FieldCp(uss_User, a_tp, ':', uss_UserLen, &overflow);
    if (overflow) {
	fprintf(stderr, "%s: * User field too long (max is %d chars)\n",
		uss_whoami, uss_UserLen);
	return (-1);
    }
    if ((*a_tp == '\0') || (*a_tp == '\n')) {
	fprintf(stderr,
		"%s: * The user field must appear in a bulk add command.\n",
		uss_whoami);
	return (-1);
    }

    a_tp =
	uss_common_FieldCp(uss_RealName, a_tp, ':', uss_RealNameLen,
			   &overflow);
    if (overflow) {
	fprintf(stderr, "%s: * Real name field too long (max is %d chars)\n",
		uss_whoami, uss_RealNameLen);
	return (-1);
    }
    if (uss_RealName[0] == '\0') {
	/*
	 * The user's real name has not been supplied.  As a
	 * default, we use the account name.
	 */
	sprintf(uss_RealName, "%s", uss_User);
	if (uss_verbose)
	    fprintf(stderr, "%s: Using default real name, '%s'\n", uss_whoami,
		    uss_User);
    }
    /*Use default full name */
    a_tp = uss_common_FieldCp(uss_Pwd, a_tp, ':', uss_PwdLen, &overflow);
    if (overflow) {
	fprintf(stderr, "%s: * Password field too long (max is %d chars)\n",
		uss_whoami, uss_PwdLen);
	return (-1);
    }
    if (uss_Pwd[0] == '\0') {
	/*
	 * The user's password has not been provided.  Use
	 * the default.
	 */
	sprintf(uss_Pwd, "%s", uss_DEFAULT_PASSWORD);
	if (uss_verbose)
	    fprintf(stderr, "%s: Using default password, '%s'\n", uss_whoami,
		    uss_Pwd);
    }				/*Use default password */
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	return (0);

    a_tp =
	uss_common_FieldCp(uss_Server, a_tp, ':', uss_ServerLen, &overflow);
    if (overflow) {
	fprintf(stderr, "%s: * Server field too long (max is %d chars)\n",
		uss_whoami, uss_ServerLen);
	return (-1);
    }
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	return (0);

    a_tp =
	uss_common_FieldCp(uss_Partition, a_tp, ':', uss_PartitionLen,
			   &overflow);
    if (overflow) {
	fprintf(stderr, "%s: * Partition field too long (max is %d chars)\n",
		uss_whoami, uss_PartitionLen);
	return (-1);
    }
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	return (0);

    a_tp =
	uss_common_FieldCp(uss_MountPoint, a_tp, ':', uss_MountPointLen,
			   &overflow);
    if (overflow) {
	fprintf(stderr, "%s: * mountpoint field too long (max is %d chars)\n",
		uss_whoami, uss_MountPointLen);
	return (-1);
    }
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	return (0);

    a_tp = uss_common_FieldCp(uss_Uid, a_tp, ':', uss_UidLen, &overflow);
    if (overflow) {
	fprintf(stderr, "%s: * UID field too long (max is %d chars)\n",
		uss_whoami, uss_UidLen);
	return (-1);
    }
    uss_DesiredUID = atoi(uss_Uid);
    if ((*a_tp == '\0') || (*a_tp == '\n'))
	return (0);

    for (uss_VarMax = 1; uss_VarMax < 10; uss_VarMax++) {
	a_tp =
	    uss_common_FieldCp(uss_Var[uss_VarMax], a_tp, ':',
			       uss_MAX_ARG_SIZE, &overflow);
	if (overflow) {
	    fprintf(stderr,
		    "%s: * Variable %d field too long (max is %d chars)\n",
		    uss_whoami, uss_VarMax, uss_MAX_ARG_SIZE);
	    return (-1);
	}
	if ((*a_tp == '\0') || (*a_tp == '\n'))
	    return (0);
    }

    /*
     * If there's anything left on the line, we ignore it.  Announce
     * the bulk add parameters we've parsed or filled in if we're
     * being verbose, then go for it.
     */
    if (uss_verbose) {
	fprintf(stderr,
		"\nAdding user '%s' ('%s'), password='%s' on server '%s', partition '%s', home directory='%s'",
		uss_User, uss_RealName, uss_Pwd,
		(uss_Server[0] != '\0' ? uss_Server : "<default>"),
		(uss_Partition[0] != '\0' ? uss_Partition : "<default>"),
		(uss_MountPoint[0] != '\0' ? uss_MountPoint : "<default>"));
	if (uss_DesiredUID)
	    fprintf(stderr, ", uid preset to %d\n", uss_DesiredUID);
	else
	    fprintf(stderr, ", no preset uid\n");
	for (i = 1; i <= uss_VarMax; i++) {
	    if (uss_Var[i][0] != '\0')
		fprintf(stderr, "$%1d='%s' ", i, uss_Var[i]);
	}
	if (uss_VarMax > 0)
	    fprintf(stderr, "\n");
    }

    /*Verbose status of add command */
    /*
     * Now do the real work.
     */
    return (DoRestore());

}				/*DoBulkRestoreLine */
#endif /* USS_DONT_HIDE_SOME_FEATURES */
#endif /* USS_FUTURE_FEATURES */


/*-----------------------------------------------------------------------
 * static DoBulkExecLine
 *
 * Description:
 *	Process the given bulk exec command.
 *
 * Arguments:
 *	a_buf : Ptr to the buffer holding the bulk exec command.
 *	a_tp  : Ptr to the first char past the opcode.
 *
 * Returns:
 *	0  if everything went well,
 *	-1 if something went wrong in the function, or
 *	Lower-level error code if something went wrong below us.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
DoBulkExecLine(a_buf, a_tp)
     char *a_buf;
     char *a_tp;

{				/*DoBulkExecLine */

    register afs_int32 code;	/*Return code */

    /*
     * Really, uss_procs_Exec does all the work for us!
     */
    code = uss_procs_Exec(a_tp);
    return (code);

}				/*DoBulkExecLine */


/*-----------------------------------------------------------------------
 * static HandleBulk
 *
 * Description:
 *	Process the given bulk command.
 *
 * Arguments:
 *	a_as   : Ptr to the command line syntax descriptor.
 *	a_rock : Ptr to the rock passed in.
 *
 * Returns:
 *	0 if everything went well,
 *	1 if something went wrong in the function, or
 *	Lower-level error code if something went wrong below us.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/
extern int Pipe;
static int
HandleBulk(register struct cmd_syndesc *a_as, void *a_rock)
{				/*HandleBulk */

#define USS_BULK_CMD_CHARS	 128
#define USS_BULK_BUF_CHARS	1024

    char cmd[USS_BULK_CMD_CHARS], buf[USS_BULK_BUF_CHARS];
    FILE *infile;
    char *tp;
    int overflow;
    int code;

    int line_no = 0;
    int error;
    char tbuf[USS_BULK_BUF_CHARS];

    /*
     * Open up the bulk file, croak if we can't.
     */
    if ((infile = fopen(a_as->parms[ABULK_FILE].items->data, "r")) == NULL) {
	fprintf(stderr, "%s: * Failed to open input file %s\n", uss_whoami,
		a_as->parms[ABULK_FILE].items->data);
	return (-1);
    }

    /*
     * Pull out the other fields as passed in by the caller on the
     * command line.
     */
    if (a_as->parms[AUSS_DRYRUN].items)
	uss_DryRun = 1;
    if (a_as->parms[AUSS_SKIPAUTH].items)
	uss_SkipKaserver = 1;
    if (a_as->parms[AUSS_OVERWRITE].items)
	uss_Overwrite = 1;
    if (a_as->parms[AUSS_PIPE].items)
	Pipe = 1;
    if (a_as->parms[AUSS_ADMIN].items)
	strcpy(uss_Administrator, a_as->parms[AUSS_ADMIN].items->data);
    else
	uss_Administrator[0] = '\0';

    if (a_as->parms[AUSS_PWEXPIRES].items) {
	uss_BulkExpires = atoi(a_as->parms[AUSS_PWEXPIRES].items->data);
	if (uss_BulkExpires < 0 || uss_BulkExpires > 254) {
	    fprintf(stderr,
		    "%s: Password Expiration must be in [0..255] days\n",
		    uss_whoami);
	    return (-1);
	}
    } else
	uss_BulkExpires = 0;

    /*
     * Initialize uss_AccountCreator().
     */
    code = uss_kauth_InitAccountCreator();
    if (code)
	return (code);

    /*
     * Process all the lines in the bulk command file.
     */
    uss_VarMax = 0;		/*No uss vars picked up yet */
    while (fgets(buf, sizeof(buf), infile) != NULL) {

	/* skip blank line */

	if (buf[0] == '\n')
	    continue;

	/* After executing the line, print the line and the result */
	if (line_no) {
	    if (error == UNOQUORUM) {
		IOMGR_Sleep(1);
	    }

	    if (!error)
		error = uss_perr;
	    printf("LINE %d %s %s", line_no, (error ? "FAIL" : "SUCCESS"),
		   tbuf);
	    fflush(stdout);
	}

	/*
	 * Reset the common variables for each command line
	 * processed.
	 */
	uss_common_Reset();

	strncpy(tbuf, buf, USS_BULK_BUF_CHARS-1);

	/*
	 * First line of file = line 1.
	 */

	++line_no;

	/*
	 * Get the opcode and act upon it.
	 */
	tp = uss_common_FieldCp(cmd, buf, ' ', USS_BULK_CMD_CHARS, &overflow);
	if (overflow) {
	    fprintf(stderr,
		    "%s: * Bulk opcode field too long (max is %d chars)\n",
		    uss_whoami, USS_BULK_CMD_CHARS);

	    error = -1;
	    continue;
/*
	    return(-1);
*/
	}
	if (strcmp(cmd, "add") == 0) {
	    error = DoBulkAddLine(buf, tp);
	    continue;
	}
	if (strcmp(cmd, "delete") == 0) {
	    error = DoBulkDeleteLine(buf, tp);
	    continue;
	}
	if (strcmp(cmd, "delvolume") == 0) {
	    uss_SaveVolume = 0;
	    error = 0;
	    continue;
	}
#if USS_FUTURE_FEATURES
#if USS_DONT_HIDE_SOME_FEATURES
	if (strcmp(cmd, "purgevolume") == 0) {
	    error = DoBulkPurgeVolumeLine(buf, tp);
	    continue;
	}
	if (strcmp(cmd, "pwdformat") == 0) {
	    /*Set the password format here */
	    continue;
	}
	if (strcmp(cmd, "pwdpath") == 0) {
	    /*Set the password path here */
	    continue;
	}
	if (strcmp(cmd, "restore") == 0) {
	    error = DoBulkRestoreLine(buf, tp);
	    continue;
	}
#endif /* USS_DONT_HIDE_SOME_FEATURES */
#endif /* USS_FUTURE_FEATURES */
	if (strcmp(cmd, "savevolume") == 0) {
	    /*Set the savevolume flag here */
	    continue;
	}
	if (strcmp(cmd, "exec") == 0) {
	    error = DoBulkExecLine(buf, tp);
	    continue;
	}

	/*
	 * If none of the valid opcodes match, see if the line is either
	 * a comment of a blank.  If so, just ignore it.  Otherwise, we
	 * have a problem.
	 */
	if ((cmd[0] != '#') && (cmd[0] != '\0')) {
	    fprintf(stderr,
		    "%s: ** Unrecognized command ('%s') in bulk file\n",
		    uss_whoami, cmd);

	    error = -1;
	    continue;

/*
	    return(-1);
*/
	}			/*Bad bulk line */
    }				/*Process a line in the bulk file */

    /* Last line. */
    if (line_no) {
	if (!error)
	    error = uss_perr;
	printf("LINE %d %s %s", line_no, (error ? "FAIL" : "SUCCESS"), tbuf);
	fflush(stdout);
    }

    return (0);
}				/*HandleBulk */


/*-----------------------------------------------------------------------
 * static AddUser
 *
 * Description:
 *	Process the given (non-bulk) add command.
 *
 * Arguments:
 *	a_as   : Ptr to the command line syntax descriptor.
 *	a_rock : Ptr to the rock passed in.
 *
 * Returns:
 *	0 if everything went well,
 *	1 if something went wrong in the function, or
 *	Lower-level error code if something went wrong below us.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
AddUser(register struct cmd_syndesc *a_as, void *a_rock)
{				/*AddUser */

    int i;
    register struct cmd_item *ti;
    int code;

    /*
     * Before we do anything else, make sure we initialize the
     * global field settings.
     */
    uss_common_Reset();

    /*
     * Pull out the fields as passed in by the caller on the command
     * line.
     */
    strcpy(uss_User, a_as->parms[AUP_USER].items->data);
    if (a_as->parms[AUP_REALNAME].items)
	strcpy(uss_RealName, a_as->parms[AUP_REALNAME].items->data);
    else
	strcpy(uss_RealName, uss_User);
    if (a_as->parms[AUP_PASSWD].items)
	strcpy(uss_Pwd, a_as->parms[AUP_PASSWD].items->data);
    else
	strcpy(uss_Pwd, uss_DEFAULT_PASSWORD);
    if (a_as->parms[AUP_SERVER].items)
	strcpy(uss_Server, a_as->parms[AUP_SERVER].items->data);
    if (a_as->parms[AUP_PART].items)
	strcpy(uss_Partition, a_as->parms[AUP_PART].items->data);
    if (a_as->parms[AUP_MNTPT].items)
	strcpy(uss_MountPoint, a_as->parms[AUP_MNTPT].items->data);
    if (a_as->parms[AUP_UID].items)
	uss_DesiredUID = atoi(a_as->parms[AUP_UID].items->data);
    else
	uss_DesiredUID = 0;
    if (a_as->parms[AUP_PWEXPIRES].items) {
	uss_Expires = atoi(a_as->parms[AUP_PWEXPIRES].items->data);
	if (uss_Expires < 0 || uss_Expires > 254) {
	    fprintf(stderr,
		    "%s: Password Expiration must be in [0..255] days\n",
		    uss_whoami);
	    return (-1);
	}
    } else
	uss_Expires = 0;

    if (a_as->parms[AUSS_DRYRUN].items)
	uss_DryRun = 1;
    if (a_as->parms[AUSS_SKIPAUTH].items)
	uss_SkipKaserver = 1;
    if (a_as->parms[AUSS_OVERWRITE].items)
	uss_Overwrite = 1;
    if (a_as->parms[AUSS_ADMIN].items) {
	strcpy(uss_Administrator, a_as->parms[AUSS_ADMIN].items->data);
	/*      fprintf(stderr, "debugging: uss_Administrator set to '%s'\n",
	 * uss_Administrator); */
    } else {
	/*      fprintf(stderr, "debugging: No administrator value given\n"); */
	uss_Administrator[0] = '\0';
    }

    if (a_as->parms[AUSS_VAR].items) {
	for (ti = a_as->parms[AUSS_VAR].items; ti; ti = ti->next) {
	    i = atoi(ti->data);
	    if (i < 0 || i > 9 || (i == 0 && *ti->data != '0')) {
		fprintf(stderr,
			"%s: Bad -var format: must be '0 val0 1 val1 ... 9 val9'\n",
			uss_whoami);
		return (-1);
	    }
	    ti = ti->next;
	    if (!ti) {
		fprintf(stderr,
			"%s: -var values must appear in pairs: 'Num val'\n",
			uss_whoami);
		return (-1);
	    }
	    strcpy(uss_Var[i], ti->data);
	    if (i > uss_VarMax)
		uss_VarMax = i;
	}			/*Remember each VAR item */
    }

    /*VAR items exist */
    /*
     * Initialize uss_AccountCreator().
     */
    code = uss_kauth_InitAccountCreator();
    if (code)
	return (code);

    /*
     * Now that the command line arguments are parsed and properly stored,
     * go for it!
     */
    return (DoAdd());

}				/*AddUser */


/*-----------------------------------------------------------------------
 * static DoAdd
 *
 * Description:
 *	Create the desired user account, having parsed the add command
 *	from either the command line or a bulk file.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	0 if everything went well,
 *	1 if something went wrong in the function, or
 *	Lower-level error code if something went wrong below us.
 *
 * Environment:
 *	All values needed have been put in the common variables.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
DoAdd()
{				/*DoAdd */

    int code;			/*Return code */

    /*
     * Make sure the user name is legal.
     */
    code = uss_kauth_CheckUserName();
    if (code)
	return (code);

    /*
     * This time around, we start off assuming the global value of the
     * -overwrite flag.
     */
    uss_OverwriteThisOne = uss_Overwrite;

    /*
     * Open up the template file before doing any real processing,
     * so we can quit early should it not be found.
     */
    if (yyin == NULL) {
	if ((yyin = uss_procs_FindAndOpen(Template)) == NULL) {
	    fprintf(stderr, "%s: ** Can't open template file '%s'\n",
		    uss_whoami, Template);
	    return (-1);
	}
	yyout = fopen("/dev/null", "w");
    } else
	rewind(yyin);

    /*
     * Add the new user to the Protection DB.
     */
    code = uss_ptserver_AddUser(uss_User, uss_Uid);
    if (code) {
	fprintf(stderr, "%s: Failed to add user '%s' to the Protection DB\n",
		uss_whoami, uss_User);
	return (code);
    }

    /*
     * Add the new user to the Authentication DB.
     */
    code = uss_kauth_AddUser(uss_User, uss_Pwd);
    if (code) {
	fprintf(stderr, "%s: Can't add user '%s' to the Authentication DB\n",
		uss_whoami, uss_User);
	return (code);
    }

    /*
     * Process the items covered by the template file.
     */
    if (yyparse() && (!uss_ignoreFlag))
	exit(-1);

    /*
     * Finally, clean up after ourselves, removing the uss_AccountCreator
     * from various of the new user's ACLs.
     */
    return (uss_acl_CleanUp());

}				/*DoAdd */


#if USS_FUTURE_FEATURES
#if USS_DONT_HIDE_SOME_FEATURES
/*-----------------------------------------------------------------------
 * static DoRestore
 *
 * Description:
 *	Perform the parsed restore command.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	0 if everything went well,
 *	1 if something went wrong in the function, or
 *	Lower-level error code if something went wrong below us.
 *
 * Environment:
 *	All values needed have been put in the common variables.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
DoRestore()
{				/*DoRestore */

    return (0);

}				/*DoRestore */
#endif /* USS_DONT_HIDE_SOME_FEATURES */
#endif /* USS_FUTURE_FEATURES */


/*-----------------------------------------------------------------------
 * static InitETTables
 *
 * Description:
 *	Set up the error code tables for the various modules we use.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
InitETTables()
{				/*InitETTables */


    /*
     * In order to get error code -> error message translations to work,
     * we have to initialize all error tables.
     */
    initialize_CMD_error_table();
    initialize_RXK_error_table();
    initialize_KTC_error_table();
    initialize_KA_error_table();
    initialize_ACFG_error_table();
    initialize_VL_error_table();
    initialize_PT_error_table();
    initialize_U_error_table();

}				/*InitETTables */


int
osi_audit()
{
/* this sucks but it works for now.
*/
    return 0;
}

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char *argv[];

{				/*Main routine */

    register struct cmd_syndesc *cs;	/*Command line syntax descriptor */
    register afs_int32 code;	/*Return code */

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a 
     * crash (i.e. core is generated) we can include the user's data section 
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGABRT, &nsa, NULL);
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    strcpy(uss_whoami, argv[0]);
    yyin = (FILE *) NULL;

    uss_fs_InBuff = (char *)malloc(USS_FS_MAX_SIZE);	/*Cache Manager input buff */
    uss_fs_OutBuff = (char *)malloc(USS_FS_MAX_SIZE);	/*Cache Manager output buff */
    if (!uss_fs_InBuff || !uss_fs_OutBuff) {
	fprintf(stderr, "%s: Can't malloc in/out buffers\n", uss_whoami);
	exit(-1);
    }

    /* ----------------------------- add ----------------------------- */

    cs = cmd_CreateSyntax("add", AddUser, NULL, "create a new user account");
    cmd_AddParm(cs, "-user", CMD_SINGLE, 0, "login name");
    cmd_AddParm(cs, "-realname", CMD_SINGLE, CMD_OPTIONAL,
		"full name in quotes");
    cmd_AddParm(cs, "-pass", CMD_SINGLE, CMD_OPTIONAL, "initial password");
    /* new parm */
    cmd_AddParm(cs, "-pwexpires", CMD_SINGLE, CMD_OPTIONAL,
		"password expires in [0..254] days (0 => never)");
    cmd_AddParm(cs, "-server", CMD_SINGLE, CMD_OPTIONAL,
		"FileServer for home volume");
    cmd_AddParm(cs, "-partition", CMD_SINGLE, CMD_OPTIONAL,
		"FileServer's disk partition for home volume");
    cmd_AddParm(cs, "-mount", CMD_SINGLE, CMD_OPTIONAL,
		"home directory mount point");
    cmd_AddParm(cs, "-uid", CMD_SINGLE, CMD_OPTIONAL,
		"uid to assign the user");
    cmd_Seek(cs, AUSS_TEMPLATE);
    cmd_AddParm(cs, "-template", CMD_SINGLE, CMD_OPTIONAL,
		"pathname of template file");
    cmd_AddParm(cs, "-verbose", CMD_FLAG, CMD_OPTIONAL, "verbose operation");
    cmd_AddParm(cs, "-var", CMD_LIST, CMD_OPTIONAL | CMD_EXPANDS,
		"auxiliary argument pairs (Num val)");
    cmd_AddParm(cs, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(cs, "-admin", CMD_SINGLE, CMD_OPTIONAL,
		"administrator to authenticate");
    cmd_AddParm(cs, "-dryrun", CMD_FLAG, CMD_OPTIONAL,
		"only list what would be done, don't do it");
    cmd_AddParm(cs, "-skipauth", CMD_FLAG, CMD_OPTIONAL,
		"ignore all contact with the authentication server (kaserver)");
    cmd_AddParm(cs, "-overwrite", CMD_FLAG, CMD_OPTIONAL,
		"Overwrite pre-existing files in user home directory tree");


    /* ---------------------------- bulk ----------------------------- */

    cs = cmd_CreateSyntax("bulk", HandleBulk, NULL, "bulk input mode");
    cmd_AddParm(cs, "-file", CMD_SINGLE, 0, "bulk input file");
    cmd_Seek(cs, AUSS_TEMPLATE);
    cmd_AddParm(cs, "-template", CMD_SINGLE, CMD_OPTIONAL,
		"pathname of template file");
    cmd_AddParm(cs, "-verbose", CMD_FLAG, CMD_OPTIONAL, "verbose operation");
    cmd_Seek(cs, AUSS_CELL);
    cmd_AddParm(cs, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(cs, "-admin", CMD_SINGLE, CMD_OPTIONAL,
		"administrator to authenticate");
    cmd_AddParm(cs, "-dryrun", CMD_FLAG, CMD_OPTIONAL,
		"only list what would be done, don't do it");
    cmd_AddParm(cs, "-skipauth", CMD_FLAG, CMD_OPTIONAL,
		"ignore all contact with the authentication server (kaserver)");
    cmd_AddParm(cs, "-overwrite", CMD_FLAG, CMD_OPTIONAL,
		"Overwrite pre-existing files in user home directory tree");
    cmd_Seek(cs, AUSS_PWEXPIRES);
    cmd_AddParm(cs, "-pwexpires", CMD_SINGLE, CMD_OPTIONAL,
		"password expires in [0..254] days (0 => never)");
    cmd_Seek(cs, AUSS_PIPE);
    cmd_AddParm(cs, "-pipe", CMD_FLAG, CMD_OPTIONAL,
		"don't prompt for passwd; get it from standard input");

    /* ---------------------------- delete --------------------------- */

    cs = cmd_CreateSyntax("delete", DelUser, NULL, "delete a user account");
    cmd_AddParm(cs, "-user", CMD_SINGLE, 0, "login name");
    cmd_AddParm(cs, "-mountpoint", CMD_SINGLE, CMD_OPTIONAL,
		"mountpoint for user's volume");
#if USS_FUTURE_FEATURES
#if USS_DONT_HIDE_SOME_FEATURES
    cmd_AddParm(cs, "-restoredir", CMD_SINGLE, 0,
		"directory where restore info is to be placed");
#endif /* USS_DONT_HIDE_SOME_FEATURES */
#endif /* USS_FUTURE_FEATURES */
    cmd_AddParm(cs, "-savevolume", CMD_FLAG, CMD_OPTIONAL,
		"don't destroy the user's volume");
#if USS_FUTURE_FEATURES
#if USS_DONT_HIDE_SOME_FEATURES
    cmd_AddParm(cs, "-pwdpath", CMD_SINGLE, CMD_OPTIONAL,
		"pathname to the password file");
    cmd_AddParm(cs, "-pwdformat", CMD_SINGLE, CMD_OPTIONAL,
		"password entry format");
#endif /* USS_DONT_HIDE_SOME_FEATURES */
#endif /* USS_FUTURE_FEATURES */
    cmd_Seek(cs, AUSS_VERBOSE);
    cmd_AddParm(cs, "-verbose", CMD_FLAG, CMD_OPTIONAL, "verbose operation");
    cmd_Seek(cs, AUSS_CELL);
    cmd_AddParm(cs, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(cs, "-admin", CMD_SINGLE, CMD_OPTIONAL,
		"administrator to authenticate");
    cmd_AddParm(cs, "-dryrun", CMD_FLAG, CMD_OPTIONAL,
		"only list what would be done, don't do it");
    cmd_AddParm(cs, "-skipauth", CMD_FLAG, CMD_OPTIONAL,
		"ignore all contact with the authentication server (kaserver)");
#if USS_FUTURE_FEATURES
#if USS_DONT_HIDE_SOME_FEATURES
    /* ------------------------- purgevolumes ------------------------ */

    cs = cmd_CreateSyntax("purgevolumes", PurgeVolumes, NULL,
			  "destroy a deleted user's volume");
    cmd_AddParm(cs, "-volname", CMD_LIST, CMD_OPTIONAL | CMD_EXPANDS,
		"Name(s) of volume(s) to destroy");
    cmd_AddParm(cs, "-volfile", CMD_SINGLE, CMD_OPTIONAL,
		"pathname to volume purge file");
    cmd_Seek(cs, AUSS_VERBOSE);
    cmd_AddParm(cs, "-verbose", CMD_FLAG, CMD_OPTIONAL, "verbose operation");
    cmd_Seek(cs, AUSS_CELL);
    cmd_AddParm(cs, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(cs, "-admin", CMD_SINGLE, CMD_OPTIONAL,
		"administrator to authenticate");
    cmd_AddParm(cs, "-dryrun", CMD_FLAG, CMD_OPTIONAL,
		"only list what would be done, don't do it");
    cmd_AddParm(cs, "-skipauth", CMD_FLAG, CMD_OPTIONAL,
		"ignore all contact with the authentication server (kaserver)");
#endif /* USS_DONT_HIDE_SOME_FEATURES */
#endif /* USS_FUTURE_FEATURES */

#if USS_FUTURE_FEATURES
#if USS_DONT_HIDE_SOME_FEATURES
    /* ---------------------------- restore -------------------------- */

    cs = cmd_CreateSyntax("restore", RestoreUser, NULL,
			  "restore a deleted user account");
    cmd_AddParm(cs, "-user", CMD_SINGLE, 0, "login name to restore");
    cmd_AddParm(cs, "-uid", CMD_SINGLE, 0, "user id number");
    cmd_AddParm(cs, "-mount", CMD_SINGLE, 0, "mountpoint for user's volume");
    cmd_AddParm(cs, "-volname", CMD_SINGLE, 0, "name of user's volume");
    cmd_AddParm(cs, "-realname", CMD_SINGLE, CMD_OPTIONAL,
		"user's full name");
    cmd_AddParm(cs, "-server", CMD_SINGLE, CMD_OPTIONAL,
		"FileServer to host user's volume");
    cmd_AddParm(cs, "-partition", CMD_SINGLE, CMD_OPTIONAL,
		"FileServer partition to host user's volume");
    cmd_AddParm(cs, "-pwdpath", CMD_SINGLE, CMD_OPTIONAL,
		"pathname to the password file");
    cmd_AddParm(cs, "-pwdformat", CMD_SINGLE, CMD_OPTIONAL,
		"password entry format");
    cmd_Seek(cs, AUSS_VERBOSE);
    cmd_AddParm(cs, "-verbose", CMD_FLAG, CMD_OPTIONAL, "verbose operation");
    cmd_Seek(cs, AUSS_CELL);
    cmd_AddParm(cs, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(cs, "-admin", CMD_SINGLE, CMD_OPTIONAL,
		"administrator to authenticate");
    cmd_AddParm(cs, "-dryrun", CMD_FLAG, CMD_OPTIONAL,
		"only list what would be done, don't do it");
    cmd_AddParm(cs, "-skipauth", CMD_FLAG, CMD_OPTIONAL,
		"ignore all contact with the authentication server (kaserver)");
#endif /* USS_DONT_HIDE_SOME_FEATURES */
#endif /* USS_FUTURE_FEATURES */

    /*
     * Set up all the error code translation tables, initialize the
     * command variables, and set up to parse the common command line
     * parameters.
     */
    InitETTables();
    uss_common_Init();
    cmd_SetBeforeProc(GetCommon, NULL);

    /*
     * Execute the parsed command.
     */
    code = cmd_Dispatch(argc, argv);
#if 0
    if (code) {
	fprintf(stderr, "%s: Call to cmd_Dispatch() failed; code is %d\n",
		uss_whoami, code);
	exit(-1);
    }
#endif /* 0 */
    if (doUnlog) {
	code = uss_fs_UnlogToken(uss_Cell);
    }
}				/*Main routine */
