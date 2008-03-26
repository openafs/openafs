/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *	Common definitions & global variables for the AFS user
 *	account facility.
 *
 */

#ifndef _USS_COMMON_H_
#define _USS_COMMON_H_ 1

/*
 * --------------------- Required definitions ---------------------
 */
#include <sys/param.h>		/*Ditto */
#include <stdio.h>		/*I/O stuff */
#include <afs/afsutil.h>
#include <errno.h>
#include <string.h>

/*
 * --------------------- Exported definitions ---------------------
 */

/*
 * Should we use static declarations or malloc()s?
 */

#define uss_QUIET 0
#define uss_VERBOSE 1

#define uss_MAX_ARG		  25
#define uss_MAX_ARG_SIZE	  50
#define uss_MAX_ENTRY		 400
#define uss_PATH_SIZE		AFSDIR_PATH_MAX
#define uss_MAX_SIZE		2048

#define uss_DEFAULT_PASSWORD "changeme"

/*
 * Capacities for the exported variables.
 */
#define uss_UserLen		  8
#define uss_UidLen		  8
#define uss_ServerLen	 	 64
#define uss_PartitionLen	 16
#define uss_MountPointLen	300
#define uss_RealNameLen		100
#define uss_PwdLen		 16
#define uss_PwdPathLen		300
#define uss_PwdFormatLen	100
#define uss_RestoreDirLen	300
#define uss_VolumeLen		300
#define uss_DirPoolLen		300

#if !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN_ENV) && !defined(AFS_XBSD_ENV)
extern char *sys_errlist[];
#endif

/*
 * We have to chain every directory that we make in order to be
 * able to come back in the reverse order and remove creator's
 * access to each directory.
 */
struct uss_subdir {
    struct uss_subdir *previous;	/*Back ptr */
    char *path;			/*Pathname to dir */
    char *finalACL;		/*Final ACL for dir */
};

/*
 * This is extracted from afsint.h, which can't be included because it
 * redefines some stuff from kauth.h.
 */
struct uss_VolumeStatus {
    afs_int32 Vid;
    afs_int32 ParentId;
    char Online;
    char InService;
    char Blessed;
    char NeedsSalvage;
    afs_int32 Type;
    afs_int32 MinQuota;
    afs_int32 MaxQuota;
    afs_int32 BlocksInUse;
    afs_int32 PartBlocksAvail;
    afs_int32 PartMaxBlocks;
};

typedef struct uss_VolumeStatus uss_VolumeStatus_t;


/*
 * ---------------------- Exported variables ----------------------
 */
extern char uss_User[];		/*User's account name */
extern char uss_Uid[];		/*User's uid */
extern char uss_Server[];	/*FileServer hosting user's volume */
extern char uss_Partition[];	/*FileServer partition for above */
extern char uss_MountPoint[];	/*Mountpoint for user's volume */
extern char uss_RealName[];	/*User's full name */
extern char uss_Pwd[];		/*User's password */
extern char uss_PwdPath[];	/*Current pathname to password file */
extern char uss_PwdPath_Saved[];	/*Saved pathname to password file */
extern char uss_PwdFormat[];	/*Current password entry format */
extern char uss_PwdFormat_Saved[];	/*Saved password entry format */
extern char uss_RestoreDir[];	/*Current directory for restore info */
extern char uss_RestoreDir_Saved[];	/*Saved directory for restore info */
extern char uss_Auto[];		/*Current choice of AUTO value */
extern char uss_Var[][uss_MAX_ARG_SIZE];	/*$1, $2, ... command variables */
extern int uss_VarMax;		/*Largest index in above */
extern char uss_Volume[];	/*Name of user's volume */
extern afs_int32 uss_VolumeID;	/*Numerical volume ID */
extern afs_int32 uss_ServerID;	/*Numerical server ID */
extern afs_int32 uss_PartitionID;	/*Numerical partition ID */
extern char uss_DirPool[][uss_DirPoolLen];	/*List of all acceptable subdirs */

extern int uss_NumGroups;	/*Number of $AUTO entries */
extern int uss_SaveVolume;	/*Save current user volume? */
extern int uss_SaveVolume_Saved;	/*Saved value of above */
extern int uss_DryRun;		/*Is this a dry run? */
extern int uss_Overwrite;	/*Overwrite user files? */
extern int uss_SkipKaserver;	/*Ignore calls to Kaserver */
extern int uss_OverwriteThisOne;	/*Overwrite on this pass? */
extern char uss_Administrator[];	/*Name of admin account */
extern char uss_AccountCreator[];	/*Principal running this program */
extern afs_int32 uss_DesiredUID;	/*UID to assign the user */
extern afs_int32 uss_Expires;	/*Password lifetime */
extern char uss_Cell[];		/*Cell in which account lives */
extern char uss_ConfDir[];	/*Config directory */
extern int uss_verbose;		/*Should we be verbose? */
extern int uss_ignoreFlag;	/*Ignore yyparse errors? */
extern char uss_whoami[];	/*Program name used */
extern int uss_syntax_err;	/*YACC syntax error? */
extern struct uss_subdir *uss_currentDir;	/*Current directory */


/*
 * ------------------------ Exported functions  -----------------------
 */
extern void uss_common_Init();
    /*
     * Summary:
     *    Set up various common uss variables, especially the saved
     *    ones.
     *
     * Args:
     *    None.
     *
     * Returns:
     *    Nothing.
     */

extern void uss_common_Reset();
    /*
     * Summary:
     *    Reset some common uss variables to their idle or
     *    saved states.
     *
     * Args:
     *    None.
     *
     * Returns:
     *    Nothing.
     */

extern char *uss_common_FieldCp();
    /*
     * Summary:
     *    Copy a ``field'', as terminated by the given separator, or
     *    up to a certain specified length.  Gripe if there was an
     *    overflow.
     *
     * Args:
     *    char *a_to       : Destination of the copy.
     *    char *a_from     : Source of the copy.
     *    char a_separator : Separator character to look for.
     *    int a_maxChars   : Max chars to copy.
     *    int *a_overflowP : Was there an overflow?
     *
     * Returns:
     *    Ptr to the next location to read from.
     */

#endif /* _USS_COMMON_H_ */
