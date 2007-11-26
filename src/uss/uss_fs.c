/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *	Implementation of the AFS system operations exported by the
 *	Cache Manager.
 */

/*
 * --------------------- Required definitions ---------------------
 */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "uss_fs.h"		/*Interface to this module */
#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#ifdef	AFS_SUN5_ENV
#include <sys/ioccom.h>
#endif
#include <netinet/in.h>

#include <string.h>

#include <afs/venus.h>
#include "uss_common.h"


/*
 * ---------------------- Private definitions ---------------------
 */
#undef USS_FS_DB


/*
 * ------------------------ Private globals -----------------------
 */
static struct ViceIoctl blob;	/*Param-passing area */
static struct ViceIoctl *blobP = &blob;	/*Ptr to above */


/*------------------------------------------------------------------------
 * static InAFS
 *
 * Description:
 *	Is the given pathname in AFS?
 *
 * Arguments:
 *	a_path : Pathname to examine.
 *
 * Returns:
 *	0 if the pathname is NOT in AFS,
 *	1 if it is.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
InAFS(a_path)
     register char *a_path;

{				/*InAFS */

    static char rn[] = "uss_fs:InAFS";
    register afs_int32 code;

    blob.in = NULL;
    blob.in_size = 0;
    blob.out_size = USS_FS_MAX_SIZE;
    blob.out = uss_fs_OutBuff;

    code = pioctl(a_path, VIOC_FILE_CELL_NAME, blobP, 1);
    if (code) {
	if ((errno == EINVAL) || (errno == ENOENT))
	    return (0);
    }
    return (1);

}				/*InAFS */


/*------------------------------------------------------------------------
 * static ParentAndComponent
 *
 * Description:
 *	Calculate the parent directory of the given pathname, along
 *	with the final component.
 *
 * Arguments:
 *	char *a_path         : Pathname to ancestorize.
 *	char *a_parentBuff   : Ptr to parent buffer to use.
 *	char **a_componentPP : Ptr to the final component.
 *	
 *
 * Returns:
 *	Ptr to the buffer containing the parent dir name.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static char *
ParentAndComponent(a_path, a_parentBuff, a_componentPP)
     char *a_path;
     char *a_parentBuff;
     char **a_componentPP;

{				/*ParentAndComponent */

    static char rn[] = "uss_fs:Parent";
    char *rightSlashP;

    /*
     * Copy over the original pathname, then find the location of the
     * rightmost slash.  If there is one, we chop off the string at
     * point.  Otherwise, it is a single pathname component whose
     * parent must be the current working directory.  In this case,
     * we cheat and return ``.''.
     */
    strcpy(a_parentBuff, a_path);
    rightSlashP = (char *)strrchr(a_parentBuff, '/');
    if (rightSlashP) {
	*rightSlashP = 0;
	*a_componentPP = rightSlashP + 1;
    } else {
	strcpy(a_parentBuff, ".");
	*a_componentPP = a_path;
    }

    return (a_parentBuff);

}				/*ParentAndComponent */


/*------------------------------------------------------------------------
 * static CarefulPioctl
 *
 * Description:
 *	Execute a pioctl(), but be careful to refresh the Cache Manager's
 *	volume mapping in case we get an ENODEV the first time.
 *
 * Arguments:
 *	char *a_path		  : pioctl() pathname argument.
 *	int a_opcode		  : pioctl() opcode.
 *	struct ViceIoctl *a_blobP : pioctl() blob pointer.
 *	int a_sl		  : pioctl() symlink info.
 *
 * Returns:
 *	Whatever the pioctl() returned, either the after the first
 *	call if we didn't get an ENODEV, or the results of the second
 *	call if we did.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
CarefulPioctl(a_path, a_opcode, a_blobP, a_sl)
     char *a_path;
     int a_opcode;
     struct ViceIoctl *a_blobP;
     int a_sl;

{				/*CarefulPioctl */

    static char rn[] = "uss_fs:CarefulPioctl";
    register afs_int32 code;

    /*
     * Call the pioctl() the first time, return if things worked
     * out ``normally''.
     */
#ifdef USS_FS_DB
    printf("%s: First pioctl call\n", rn);
#endif /* USS_FS_DB */
    code = pioctl(a_path, a_opcode, a_blobP, a_sl);
#ifdef USS_FS_DB
    if (code)
	printf("%s: First pioctl call fails, errno is %d\n", rn, errno);
#endif /* USS_FS_DB */
    if ((code == 0) || (code && (errno != ENODEV)))
	return (code);

    /*
     * Hmm, it's possible out volume mappings are stale.  Let's
     * bring them up to date, then try again.
     */
#ifdef USS_FS_DB
    printf("%s: First pioctl got a NODEV\n", rn);
#endif /* USS_FS_DB */
    code = uss_fs_CkBackups();
    code = pioctl(a_path, a_opcode, a_blobP, a_sl);
    return (code);

}				/*CarefulPioctl */


/*------------------------------------------------------------------------
 * EXPORTED uss_fs_GetACL
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_fs_GetACL(a_dirPath, a_aclBuff, a_aclBuffBytes)
     char *a_dirPath;
     char *a_aclBuff;
     afs_int32 a_aclBuffBytes;

{				/*uss_fs_GetACL */

    static char rn[] = "uss_fs_GetACL";	/*Routine name */
    register afs_int32 code;	/*pioctl() result */

    blob.in = NULL;
    blob.in_size = 0;
    blob.out = a_aclBuff;
    blob.out_size = a_aclBuffBytes;

#ifdef USS_FS_DB
    printf("%s: in 0x%x (%d bytes), out 0x%x, (%d bytes)\n", rn, blob.in,
	   blob.in_size, blob.out, blob.out_size);
#endif /* USS_FS_DB */

    code = CarefulPioctl(a_dirPath, VIOCGETAL, blobP, 1);

#ifdef USS_FS_DB
    if (code)
	printf("%s: pioctl() failed, errno %d\n", rn, errno);
#endif /* USS_FS_DB */

    return (code);

}				/*uss_fs_GetACL */


/*------------------------------------------------------------------------
 * EXPORTED uss_fs_SetACL
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_fs_SetACL(a_dirPath, a_aclBuff, a_aclBuffBytes)
     char *a_dirPath;
     char *a_aclBuff;
     afs_int32 a_aclBuffBytes;

{				/*uss_fs_SetACL */

    static char rn[] = "uss_fs_SetACL";	/*Routine name */
    register afs_int32 code;	/*pioctl() result */

    blob.in = a_aclBuff;
    blob.in_size = a_aclBuffBytes;
    blob.out = NULL;
    blob.out_size = 0;

#ifdef USS_FS_DB
    printf("%s: in 0x%x (%d bytes), out 0x%x, (%d bytes)\n", rn, blob.in,
	   blob.in_size, blob.out, blob.out_size);
    printf("%s: ACL value for dir '%s' is '%s'\n", rn, a_dirPath, a_aclBuff);
#endif /* USS_FS_DB */

    code = CarefulPioctl(a_dirPath, VIOCSETAL, blobP, 1);

#ifdef USS_FS_DB
    if (code)
	printf("%s: pioctl() failed, errno %d", rn, errno);
#endif /* USS_FS_DB */

    return (code);

}				/*uss_fs_SetACL */


/*------------------------------------------------------------------------
 * EXPORTED uss_fs_GetVolStat
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_fs_GetVolStat(a_mountpoint, a_volStatBuff, a_volStatBuffBytes)
     char *a_mountpoint;
     char *a_volStatBuff;
     afs_int32 a_volStatBuffBytes;

{				/*uss_fs_GetVolStat */

    static char rn[] = "uss_fs_GetVolStat";	/*Routine name */
    register afs_int32 code;	/*pioctl() result */

    blob.in = NULL;
    blob.in_size = 0;
    blob.out = a_volStatBuff;
    blob.out_size = a_volStatBuffBytes;

#ifdef USS_FS_DB
    printf("%s: in 0x%x (%d bytes), out 0x%x, (%d bytes)\n", rn, blob.in,
	   blob.in_size, blob.out, blob.out_size);
#endif /* USS_FS_DB */

    code = CarefulPioctl(a_mountpoint, VIOCGETVOLSTAT, blobP, 1);

#ifdef USS_FS_DB
    if (code)
	printf("%s: pioctl() failed, errno %d", rn, errno);
#endif /* USS_FS_DB */

    return (code);

}				/*uss_fs_GetVolStat */


/*------------------------------------------------------------------------
 * EXPORTED uss_fs_SetVolStat
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_fs_SetVolStat(a_mountpoint, a_volStatBuff, a_volStatBuffBytes)
     char *a_mountpoint;
     char *a_volStatBuff;
     afs_int32 a_volStatBuffBytes;

{				/*uss_fs_SetVolStat */

    static char rn[] = "uss_fs_SetVolStat";	/*Routine name */
    register afs_int32 code;	/*pioctl() result */

    blob.in = a_volStatBuff;
    blob.in_size = a_volStatBuffBytes;
    blob.out = a_volStatBuff;
    blob.out_size = USS_FS_MAX_SIZE;

#ifdef USS_FS_DB
    printf("%s: in 0x%x (%d bytes), out 0x%x, (%d bytes)\n", rn, blob.in,
	   blob.in_size, blob.out, blob.out_size);
#endif /* USS_FS_DB */

    code = CarefulPioctl(a_mountpoint, VIOCSETVOLSTAT, blobP, 1);

#ifdef USS_FS_DB
    if (code)
	printf("%s: pioctl() failed, errno %d", rn, errno);
#endif /* USS_FS_DB */

    return (code);

}				/*uss_fs_SetVolStat */


/*------------------------------------------------------------------------
 * EXPORTED uss_fs_CkBackups
 *
 * Environment:
 *	We are NOT careful here, since it's OK to get ENODEVs.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_fs_CkBackups()
{				/*uss_fs_CkBackups */

    static char rn[] = "uss_fs_CkBackups";	/*Routine name */
    register afs_int32 code;	/*pioctl() result */

    blob.in = NULL;
    blob.in_size = 0;
    blob.out = NULL;
    blob.out_size = 0;

#ifdef USS_FS_DB
    printf("%s: in 0x%x (%d bytes), out 0x%x, (%d bytes)\n", rn, blob.in,
	   blob.in_size, blob.out, blob.out_size);
#endif /* USS_FS_DB */

    code = pioctl(NULL,		/*No pathname needed here */
		  VIOCCKBACK,	/*CheckBackups */
		  &blob,	/*Params */
		  1);		/*Symlink disposition */
#ifdef USS_FS_DB
    if (code)
	printf("%s: pioctl() failed, errno %d", rn, errno);
#endif /* USS_FS_DB */

    return (code);

}				/*uss_fs_CkBackups */


/*------------------------------------------------------------------------
 * EXPORTED uss_fs_MkMountPoint
 *
 * Environment:
 *	Uses uss_fs_OutBuff to construct the mountpoint contents.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_fs_MkMountPoint(a_volname, a_cellname, a_rw, a_mountpoint)
     char *a_volname;
     char *a_cellname;
     afs_int32 a_rw;
     char *a_mountpoint;

{				/*uss_fs_MkMountPoint */
    extern int local_Cell;
    static char rn[] = "uss_fs_MkMountPoint";	/*Routine name */
    register afs_int32 code;	/*pioctl() result */
    char *tp;			/*Temporary */

#ifdef USS_FS_DB
    printf
	("%s: a_volname='%s', a_cellname='%s', a_rw=%d, a_mountpoint='%s'\n",
	 rn, a_volname, a_cellname, a_rw, a_mountpoint);
#endif /* USS_FS_DB */

    /*
     * Make sure the parent directory is in AFS.
     */
    if (!InAFS(ParentAndComponent(a_mountpoint, uss_fs_OutBuff, &tp))) {
	printf("%s: Mountpoints must be created within AFS\n", rn);
	return (-1);
    }

    /*
     * Build the contents of the mountpoint we'll create.  It's safe to
     * use the uss_fs_OutBuff for this construction.  Note: the last
     * char, by convention, is a dot.
     */
    if (local_Cell) {
	sprintf(uss_fs_OutBuff, "%s%s.", (a_rw ? "%" : "#"), a_volname);
    } else {
	sprintf(uss_fs_OutBuff, "%s%s:%s.", (a_rw ? "%" : "#"), a_cellname,
		a_volname);
    }

    /*
     * Now, create the symlink with the above value.
     */
    code = symlink(uss_fs_OutBuff, a_mountpoint);
    if (code) {
#ifdef USS_FS_DB
	printf("%s: Mountpoint creation (symlink) failed, errno is %d\n", rn,
	       errno);
#endif /* USS_FS_DB */
	return (-1);
    }
    return 0;
}				/*uss_fs_MkMountPoint */


/*------------------------------------------------------------------------
 * EXPORTED uss_fs_RmMountPoint
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
uss_fs_RmMountPoint(a_mountpoint)
     char *a_mountpoint;

{				/*uss_fs_RmMountPoint */

    static char rn[] = "uss_fs_RmMountPoint";	/*Routine name */
    register afs_int32 code;	/*pioctl() result */
    char *parentDirP;		/*Ptr to parent */
    char *componentP;		/*Ptr to last component */

    /*
     * Get the parent & final component names.
     */
    parentDirP = ParentAndComponent(a_mountpoint, uss_fs_InBuff, &componentP);

    blob.in = componentP;
    blob.in_size = strlen(componentP) + 1;
    blob.out = uss_fs_OutBuff;
    blob.out_size = USS_FS_MAX_SIZE;

#ifdef USS_FS_DB
    printf("%s: AFS_STAT_MT_PT, in 0x%x (%d bytes), out 0x%x, (%d bytes)\n",
	   rn, blob.in, blob.in_size, blob.out, blob.out_size);
#endif /* USS_FS_DB */

    code = CarefulPioctl(parentDirP, VIOC_AFS_STAT_MT_PT, blobP, 1);
    if (code) {
#ifdef USS_FS_DB
	printf("%s: STAT_MT_PT pioctl() failed, errno %d", rn, errno);
#endif /* USS_FS_DB */
	if (errno == EINVAL)
	    printf("%s: '%s' is not a mountpoint\n", rn, a_mountpoint);
	return (code);
    }

    /*
     * Now that we know we have a proper mountpoint, nuke it.
     */
    blob.in = componentP;
    blob.in_size = strlen(componentP) + 1;
    blob.out = NULL;
    blob.out_size = 0;

    if (!uss_DryRun) {
#ifdef USS_FS_DB
	printf
	    ("%s: AFS_DELETE_MT_PT, in 0x%x (%d bytes), out 0x%x, (%d bytes)\n",
	     rn, blob.in, blob.in_size, blob.out, blob.out_size);
#endif /* USS_FS_DB */

	code = pioctl(parentDirP, VIOC_AFS_DELETE_MT_PT, blobP, 1);
	if (code) {
#ifdef USS_FS_DB
	    printf("%s: DELETE_MT_PT pioctl() failed, errno %d", rn, errno);
#endif /* USS_FS_DB */
	}
    } else
	printf("\t[Dry run - mount point '%s' NOT removed]\n", componentP);

    return (code);

}				/*uss_fs_RmMountPoint */


#include <afs/auth.h>
struct tokenInfo {
    struct ktc_token token;
    struct ktc_principal service;
    struct ktc_principal client;
    int deleted;
};

/*
 * Build a list of tokens, delete the bad ones (the ones to remove from the 
 * permissions list,) destroy all tokens, and then re-register the good ones.
 * Ugly, but it works.
 */
uss_fs_UnlogToken(celln)
     char *celln;
{
    unsigned count = 0, index, index2;
    afs_int32 code = 0, cnt = 0;
    struct ktc_principal serviceName;
    struct tokenInfo *tokenInfoP, *tp;

    do {
	code = ktc_ListTokens(count, &count, &serviceName);
	cnt++;
    } while (!code);
    count = cnt - 1;
    tokenInfoP =
	(struct tokenInfo *)malloc((sizeof(struct tokenInfo) * count));
    for (code = index = index2 = 0; (!code) && (index < count); index++) {
	tp = tokenInfoP + index;
	code = ktc_ListTokens(index2, &index2, &tp->service);
	if (!code) {
	    code =
		ktc_GetToken(&tp->service, &tp->token,
			     sizeof(struct ktc_token), &tp->client);
	    if (!code) {
		tp->deleted = (!strcmp(celln, tp->client.cell) ? 1 : 0);
		if (tp->deleted)
		    cnt = 1;
	    }
	}
    }
    if (code = ktc_ForgetAllTokens()) {
	printf("uss_fs_UnlogToken: could not discard tickets, code %d\n",
	       code);
	exit(1);
    }
    for (code = index = 0; index < count; index++) {
	tp = tokenInfoP + index;
	if (!(tp->deleted)) {
	    code = ktc_SetToken(&tp->service, &tp->token, &tp->client, 0);
	    if (code) {
		printf
		    ("uss_fs_UnlogToken: Couldn't re-register token, code = %d\n",
		     code);
	    }
	}
    }
    return 0;
}
