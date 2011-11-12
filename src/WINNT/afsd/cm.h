/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_WINNT_AFSD_CM_H
#define OPENAFS_WINNT_AFSD_CM_H 1

/* We use pthreads in the cache manager (not LWP) */
#ifndef AFS_PTHREAD_ENV
#define AFS_PTHREAD_ENV 1
#endif

#include <rx/rx.h>
#include <afs/vldbint.h>
#include <afs/afsint.h>
#include <afs/cm_error.h>

#define CM_DEFAULT_CALLBACKPORT         7001

/* common flags to many procedures */
#define CM_FLAG_CREATE		1		/* create entry */
#define CM_FLAG_CASEFOLD	2		/* fold case in namei, lookup, etc. */
#define CM_FLAG_EXCLUSIVE	4		/* create exclusive */
#define CM_FLAG_FOLLOW		8		/* follow symlinks, even at the end (namei) */
#define CM_FLAG_8DOT3		0x10		/* restrict to 8.3 name */
#define CM_FLAG_NOMOUNTCHASE	0x20		/* don't follow mount points */
#define CM_FLAG_DIRSEARCH	0x40		/* for directory search */
#define CM_FLAG_CHECKPATH	0x80		/* Path instead of File */
#define CM_FLAG_NOPROBE         0x100           /* For use with cm_GetCellxxx - do not probe server status */
#define CM_FLAG_DFS_REFERRAL    0x200           /* The request is a DFS Referral - the last char of the lookup name may be missing */

/* Used by cm_FollowMountPoint and cm_FindVolumeByName */
/* And as an index in cm_volume_t */
#define RWVOL	0
#define ROVOL	1
#define BACKVOL	2

#define LOCK_HIERARCHY_IGNORE                    0

#define LOCK_HIERARCHY_SMB_STARTED              20
#define LOCK_HIERARCHY_SMB_LISTENER             30
#define LOCK_HIERARCHY_SMB_DIRWATCH             40
#define LOCK_HIERARCHY_SMB_GLOBAL               50
#define LOCK_HIERARCHY_SMB_DIRSEARCH            60
#define LOCK_HIERARCHY_SMB_FID                  70
#define LOCK_HIERARCHY_SMB_TID                  80
#define LOCK_HIERARCHY_SMB_UID                  90
#define LOCK_HIERARCHY_SMB_RAWBUF              100
#define LOCK_HIERARCHY_SMB_RCT_GLOBAL          110
#define LOCK_HIERARCHY_SMB_VC                  120
#define LOCK_HIERARCHY_SMB_MONITOR             125


#define LOCK_HIERARCHY_SCACHE_DIRLOCK          500
#define LOCK_HIERARCHY_DAEMON_GLOBAL           510
#define LOCK_HIERARCHY_SMB_USERNAME            520
#define LOCK_HIERARCHY_SCACHE_BUFCREATE        530
#define LOCK_HIERARCHY_BUFFER                  540
#define LOCK_HIERARCHY_SCACHE                  550
#define LOCK_HIERARCHY_BUF_GLOBAL              560
#define LOCK_HIERARCHY_VOLUME                  570
#define LOCK_HIERARCHY_USER                    580
#define LOCK_HIERARCHY_SCACHE_GLOBAL           590
#define LOCK_HIERARCHY_CONN_GLOBAL             600
#define LOCK_HIERARCHY_CELL                    620
#define LOCK_HIERARCHY_CELL_GLOBAL             630
#define LOCK_HIERARCHY_SERVER                  640
#define LOCK_HIERARCHY_CALLBACK_GLOBAL         645
#define LOCK_HIERARCHY_SERVER_GLOBAL           650
#define LOCK_HIERARCHY_CONN                    660
#define LOCK_HIERARCHY_VOLUME_GLOBAL           670
#define LOCK_HIERARCHY_DNLC_GLOBAL             690
#define LOCK_HIERARCHY_FREELANCE_GLOBAL        700
#define LOCK_HIERARCHY_UTILS_GLOBAL            710
#define LOCK_HIERARCHY_OTHER_GLOBAL            720
#define LOCK_HIERARCHY_ACL_GLOBAL              730
#define LOCK_HIERARCHY_USER_GLOBAL             740
#define LOCK_HIERARCHY_AFSDBSBMT_GLOBAL       1000
#define LOCK_HIERARCHY_TOKEN_EVENT_GLOBAL     2000
#define LOCK_HIERARCHY_SYSCFG_GLOBAL          3000
#endif /*  OPENAFS_WINNT_AFSD_CM_H */

