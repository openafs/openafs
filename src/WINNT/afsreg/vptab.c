/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Functions and definitions for accessing the vice partition table */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <afs/errmap_nt.h>

#include "afsreg.h"
#include "vptab.h"

#define PARTITION_NAME_PREFIX  "/vicep"


/*
 * vpt_PartitionNameValid() -- Check partition name syntax.
 * 
 * RETURN CODES: 1 valid, 0 not valid
 */
int
vpt_PartitionNameValid(const char *vpname)
{
    int rc = 0;

    /* Only allowing full name, not any of the standard shorthands.
     * Assumes C locale.
     * NOTE: yet another incarnation of parsing code; should be consolidated.
     */
    if (!strncmp(vpname,
		 PARTITION_NAME_PREFIX, sizeof(PARTITION_NAME_PREFIX) - 1)) {
	/* prefix is correct; check suffix */
	const char *vpsuffix = vpname + (sizeof(PARTITION_NAME_PREFIX) - 1);
	size_t vpsuffixLen = strlen(vpsuffix);

	if (vpsuffixLen == 1) {
	    /* must be 'a' - 'z' (0 .. 25) */
	    if (islower(vpsuffix[0])) {
		rc = 1;
	    }
	} else if (vpsuffixLen == 2) {
	    /* must be 'aa' - 'zz' and in range (26 .. 255) */
	    if (islower(vpsuffix[0]) && islower(vpsuffix[1])) {
		int vpsuffixVal = ((vpsuffix[0] - 'a') * 26 +
				   (vpsuffix[1] - 'a') + 26);
		if (vpsuffixVal <= 255) {
		    rc = 1;
		}
	    }
	}
    }
    return rc;
}


/*
 * vpt_DeviceNameValid() -- Check device name syntax.
 * 
 * RETURN CODES: 1 valid, 0 not valid
 */
int
vpt_DeviceNameValid(const char *devname)
{
    /* assuming C locale */
    if (isupper(devname[0]) && devname[1] == ':' && devname[2] == '\0') {
	return 1;
    } else {
	return 0;
    }
}


/*
 * vpt_Start() -- Initialize iteration for reading vice partition table.
 * 
 * RETURN CODES: 0 success, -1 failed (errno set)
 */

int
vpt_Start(struct vpt_iter *iterp)  /* [out] iteration handle to initialize */
{
    long status;
    HKEY key;

    memset(iterp, 0, sizeof(*iterp));

    /* open canonical Afstab key */
    status = RegOpenKeyAlt(AFSREG_NULL_KEY, AFSREG_SVR_SVC_AFSTAB_KEY,
			   KEY_READ, 0, &key, NULL);

    if (status == ERROR_SUCCESS) {
	/* enumerate subkeys, each of which represents a vice partition */
	status = RegEnumKeyAlt(key, &iterp->vpenum);
	(void)RegCloseKey(key);
    }

    if (status) {
	errno = nterr_nt2unix(status, EIO);
	return -1;
    }
    return 0;
}


/*
 * vpt_NextEntry() -- Return next vice partition table entry.
 * 
 * RETURN CODES: 0 success, -1 no more entries (ENOENT) or failed (errno set).
 */

int
vpt_NextEntry(struct vpt_iter *iterp,   /* [in] valid iteration handle */
	      struct vptab *vptabp)    /* [out] next partiton table entry */
{
    long status;
    HKEY tabKey, vpKey;
    char *nextNamep;

    if (iterp->vpenum == NULL) {
	/* no partition entries (or invalid iteration handle) */
	errno = ENOENT;
	return -1;
    }

    /* find name of next partition entry to examine in multistring enum */
    if (iterp->last == NULL) {
	/* first call */
	nextNamep = iterp->last = iterp->vpenum;
    } else {
	/* subsequent call */
	if (*iterp->last != '\0') {
	    /* not at end of multistring; advance to next name */
	    iterp->last += strlen(iterp->last) + 1;
	}
	nextNamep = iterp->last;
    }

    if (*nextNamep == '\0') {
	/* end of multistring; no more entries */
	errno = ENOENT;
	return -1;
    }

    if (strlen(nextNamep) >= VPTABSIZE_NAME) {
	/* invalid partition name entry */
	errno = EINVAL;
	return -1;
    }
    strcpy(vptabp->vp_name, nextNamep);

    /* open canonical Afstab key */
    status = RegOpenKeyAlt(AFSREG_NULL_KEY, AFSREG_SVR_SVC_AFSTAB_KEY,
			   KEY_READ, 0, &tabKey, NULL);

    if (status == ERROR_SUCCESS) {
	/* open key representing vice partition */
	status = RegOpenKeyAlt(tabKey, nextNamep,
			       KEY_READ, 0, &vpKey, NULL);

	if (status == ERROR_SUCCESS) {
	    /* read partition attributes */
	    DWORD dataType, dataSize = VPTABSIZE_DEV;

	    status = RegQueryValueEx(vpKey,
				     AFSREG_SVR_SVC_AFSTAB_DEVNAME_VALUE,
				     NULL, &dataType, vptabp->vp_dev,
				     &dataSize);

	    if (status == ERROR_SUCCESS && dataType != REG_SZ) {
		/* invalid device name type */
		status = ERROR_INVALID_PARAMETER;
	    }
	    (void)RegCloseKey(vpKey);
	}
	(void)RegCloseKey(tabKey);
    }

    if (status) {
	errno = nterr_nt2unix(status, EIO);
	return -1;
    }
    return 0;
}


/*
 * vpt_Finish() -- Terminate iteration for vice partition table.
 * 
 * RETURN CODES: 0 success, -1 failed (errno set)
 */
int
vpt_Finish(struct vpt_iter *iterp)    /* [in] iteration handle to terminate */
{
    if (iterp->vpenum) {
	free(iterp->vpenum);
    }
    memset(iterp, 0, sizeof(*iterp));
    return 0;
}


/*
 * vpt_AddEntry() -- Add or update vice partition table entry.
 *
 * RETURN CODES: 0 success, -1 failed (errno set)
 */
int
vpt_AddEntry(const struct vptab *vptabp)
{
    long status;
    HKEY tabKey, vpKey;
    const char *vpName, *vpDev;

    vpName = vptabp->vp_name;
    vpDev = vptabp->vp_dev;

    if (!vpt_PartitionNameValid(vpName) || !vpt_DeviceNameValid(vpDev)) {
	errno = EINVAL;
	return -1;
    }

    /* open canonical Afstab key; create if doesn't exist */
    status = RegOpenKeyAlt(AFSREG_NULL_KEY, AFSREG_SVR_SVC_AFSTAB_KEY,
			   KEY_WRITE, 1 /* create */, &tabKey, NULL);

    if (status == ERROR_SUCCESS) {
	/* open key representing vice partition; create if doesn't exist */
	status = RegOpenKeyAlt(tabKey, vpName,
			       KEY_WRITE, 1 /* create */, &vpKey, NULL);

	if (status == ERROR_SUCCESS) {
	    /* write partition attributes */
	    status = RegSetValueEx(vpKey, AFSREG_SVR_SVC_AFSTAB_DEVNAME_VALUE,
				   0, REG_SZ, vpDev, strlen(vpDev) + 1);

	    RegCloseKey(vpKey);
	}

	RegCloseKey(tabKey);
    }

    if (status) {
	errno = nterr_nt2unix(status, EIO);
	return -1;
    }
    return 0;
}


/*
 * vpt_RemoveEntry() -- Remove vice partition table entry.
 *
 * RETURN CODES: 0 success, -1 failed (errno set)
 */
int
vpt_RemoveEntry(const char *vpname)
{
    long status;
    HKEY tabKey;

    if (!vpt_PartitionNameValid(vpname)) {
	errno = EINVAL;
	return -1;
    }

    /* open canonical Afstab key */
    status = RegOpenKeyAlt(AFSREG_NULL_KEY, AFSREG_SVR_SVC_AFSTAB_KEY,
			   KEY_WRITE, 0, &tabKey, NULL);

    if (status == ERROR_SUCCESS) {
	/* delete key representing vice partition */
	status = RegDeleteKey(tabKey, vpname);
    }

    if (status) {
	errno = nterr_nt2unix(status, EIO);
	return -1;
    }
    return 0;
}
