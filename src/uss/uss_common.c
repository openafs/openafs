/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *	Storage for common variables employed by the AFS user
 *	account facility.
 */

/*
 * --------------------- Required definitions ---------------------
 */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "uss_common.h"		/*Interface definitions */
#include <afs/kautils.h>	/*MAXKTCREALMLEN & MAXKTCNAMELEN */

#include <string.h>


/*
 * ---------------------- Exported variables ----------------------
 */
#define uss_AutoLen	300
#define uss_NumVars	 10

char uss_User[uss_UserLen + 1];	/*User's account name */
char uss_Uid[uss_UidLen + 1];	/*User's uid */
char uss_Server[uss_ServerLen + 1];	/*FileServer hosting user's volume */
char uss_Partition[uss_PartitionLen + 1];	/*FileServer partition for above */
char uss_MountPoint[uss_MountPointLen + 1];	/*Mountpoint for user's volume */
char uss_RealName[uss_RealNameLen + 1];	/*User's full name */
char uss_Pwd[uss_PwdLen + 1];	/*User password */
char uss_PwdPath[uss_PwdPathLen + 1];	/*Curr pathname to password file */
char uss_PwdFormat[uss_PwdFormatLen + 1];	/*Curr password entry format */
char uss_RestoreDir[uss_RestoreDirLen + 1];	/*Curr directory for restore info */
char uss_Auto[uss_AutoLen + 1];	/*Curr choice of AUTO value */
char uss_Var[uss_NumVars][uss_MAX_ARG_SIZE];	/*$1, $2, ... command variables */
int uss_VarMax = 0;		/*Largest index in above */
char uss_Volume[uss_VolumeLen + 1];	/*Name of user's volume */
afs_int32 uss_VolumeID;		/*Numerical volume ID */
afs_int32 uss_ServerID;		/*Numerical server ID */
afs_int32 uss_PartitionID;	/*Numerical partition ID */
char uss_DirPool[100][uss_DirPoolLen];	/*List of all acceptable subdirs */


/*
 * Values saved across invocations.
 */
char uss_PwdPath_Saved[uss_PwdPathLen];	/*Pathname to saved pwd file */
char uss_PwdFormat_Saved[uss_PwdFormatLen];	/*Saved pwd entry format */
char uss_RestoreDir_Saved[uss_RestoreDirLen];	/*Saved dir for restore info */

int uss_NumGroups = 0;		/*Number of $AUTO entries */
int uss_SaveVolume;		/*Save current user volume? */
int uss_SaveVolume_Saved;	/*Saved value of above */
int uss_DryRun = 0;		/*Is this a dry run? */
int uss_SkipKaserver = 0;	/*Don't talk to kaserver */
int uss_Overwrite = 0;		/*Overwrite user files? */
int uss_OverwriteThisOne;	/*Overwrite on this pass? */
char uss_Administrator[64];	/*Name of admin account */
char uss_AccountCreator[MAXKTCNAMELEN];	/*Principal running this program */
afs_int32 uss_DesiredUID;	/*Uid to assign the user */
afs_int32 uss_Expires;		/*How long between password expires */
char uss_Cell[MAXKTCREALMLEN];	/*Cell in which account lives */
char uss_ConfDir[uss_PATH_SIZE];	/*Config directory */
int uss_verbose = 0;		/*Are we being verbose? */
int uss_ignoreFlag = 0;		/*Ignore yyparse errors? */
int uss_perr = 0;
char uss_whoami[64];		/*Program name used */
int uss_syntax_err = 0;		/*YACC syntax error? */
struct uss_subdir *uss_currentDir = NULL;	/*Current directory */


/*
 * ----------------------- Private variables ----------------------
 */
static int initDone = 0;	/*Have we been initialized? */


/*------------------------------------------------------------------------
 * EXPORTED uss_common_Init
 *
 * Description:
 *	Set up various common uss variables, especially the saved ones.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	We only allow an initialization once.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
uss_common_Init()
{				/*uss_common_Init */

    extern int line;		/*Current template line */

    if (initDone)
	return;

    strcpy(uss_ConfDir, AFSDIR_CLIENT_ETC_DIRPATH);
    uss_PwdPath_Saved[0] = '\0';
    uss_PwdFormat_Saved[0] = '\0';
    uss_RestoreDir_Saved[0] = '\0';
    uss_SaveVolume_Saved = 0;
    uss_syntax_err = 0;
    line = 1;

}				/*uss_common_Init */


/*------------------------------------------------------------------------
 * EXPORTED uss_common_Reset
 *
 * Description:
 *	Resets some of the common variables to their idle or
 *	saved states.
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
uss_common_Reset()
{				/*uss_common_Reset */

    extern int line;		/*Current template line */
    int i;			/*Loop variable */

    uss_ignoreFlag = 0;
    uss_NumGroups = 0;
    uss_perr = 0;
    uss_currentDir = NULL;
    uss_DesiredUID = 0;
    uss_VarMax = 0;
    uss_User[0] = '\0';
    uss_Uid[0] = '\0';
    uss_Server[0] = '\0';
    uss_Partition[0] = '\0';
    uss_MountPoint[0] = '\0';
    uss_RealName[0] = '\0';
    sprintf(uss_Pwd, "%s", uss_DEFAULT_PASSWORD);
    strcpy(uss_PwdPath, uss_PwdPath_Saved);
    strcpy(uss_PwdFormat, uss_PwdFormat_Saved);
    strcpy(uss_RestoreDir, uss_RestoreDir_Saved);
    uss_SaveVolume = uss_SaveVolume_Saved;
    uss_Auto[0] = '\0';
    for (i = 1; i < 10; i++)
	uss_Var[i][0] = '\0';

    /*
     * Reset the template line you think you're reading from, and
     * remember you haven't seen any template file parsing errors
     * on this run yet.
     */
    line = 1;
    uss_syntax_err = 0;

}				/*uss_common_Reset */


/*------------------------------------------------------------------------
 * EXPORTED uss_common_FieldCp
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

char *
uss_common_FieldCp(a_to, a_from, a_separator, a_maxChars, a_overflowP)
     char *a_to;
     char *a_from;
     char a_separator;
     int a_maxChars;
     int *a_overflowP;

{				/*uss_common_FieldCp */

    register int chars_read;	/*Number of chars read so far */

    chars_read = 0;
    *a_overflowP = 0;
    /*
     * It's OK to read in the separator/null after reading a_maxChars,
     * so we need to bump a_maxChars by one.
     */
    a_maxChars++;

    while (*a_from != a_separator && *a_from != '\0' && *a_from != '\n') {
	*a_to++ = *a_from++;
	if (++chars_read >= a_maxChars) {
	    /*
	     * Bummer, we've overflowed.  Remember the sad fact, and
	     * move the destination pointer back over the extra
	     * character.  Also, skip over any garbage sitting after
	     * the characters that won't fit in to the given field.
	     */
	    *a_overflowP = 1;
	    a_to--;
#if 0
	    printf("*** Skipping overflow char '%c'\n", *(a_from - 1));
#endif /* 0 */
	    while (*a_from != a_separator && *a_from != '\0'
		   && *a_from != '\n') {
#if 0
		printf("*** Skipping overflow char '%c'\n", *a_from);
#endif /* 0 */
		a_from++;
	    }			/*Skip over excess chars */
	    break;
	}			/*Found overflow */
    }				/*Read til end of field */

    /*
     * Make sure we return a null-terminated string.
     */
    *a_to = '\0';

    /*
     * Collapse any number of blanks, should we have ended up pointing
     * to one.
     */
    if (a_separator == ' ')
	while ((*a_from == ' ') && (*(a_from + 1) == ' '))
	    a_from++;

    /*
     * Return the position of the next non-separator char or
     * the position of the null, if that's what was left.
     */
    if (*a_from == '\0')
	return (a_from);
    else
	return (++a_from);

}				/*uss_common_FieldCp */
