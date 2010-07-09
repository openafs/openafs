/*
 * osi_misc.c
 *
 * $Id: osi_misc.c,v 1.4 2003/10/09 16:13:16 rees Exp $
 */

/*
copyright 2002
the regents of the university of michigan
all rights reserved

permission is granted to use, copy, create derivative works
and redistribute this software and such derivative works
for any purpose, so long as the name of the university of
michigan is not used in any advertising or publicity
pertaining to the use or distribution of this software
without specific, written prior authorization.  if the
above copyright notice or any other identification of the
university of michigan is included in any copy of any
portion of this software, then the disclaimer below must
also be included.

this software is provided as is, without representation
from the university of michigan as to its fitness for any
purpose, and without warranty by the university of
michigan of any kind, either express or implied, including
without limitation the implied warranties of
merchantability and fitness for a particular purpose. the
regents of the university of michigan shall not be liable
for any damages, including special, indirect, incidental, or
consequential damages, with respect to any claim arising
out of or in connection with the use of the software, even
if it has been or is hereafter advised of the possibility of
such damages.
*/

/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"



#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afs/afsincludes.h"	/* Afs-based standard headers */

/*
 * afs_suser() returns true if the caller is superuser, false otherwise.
 *
 * Note that it must NOT set errno.
 */

/*
 * Modern NetBSD version of afs_osi_suser().  For cognate code calling
 * traditional BSD suser, see OBSD/osi_misc.c.
 */
int
afs_osi_suser(void *credp)
{
    int code;
    code = kauth_authorize_generic(credp,
				   KAUTH_GENERIC_ISSUSER,
				   &curlwp->l_acflag);
    return (code == 0);
}

/*
 * Support Alloc_NoSleep.  This should propagate back to OBSD.
 * Matt.
 */
void *
osi_nbsd_Alloc(size_t asize, int cansleep)
{
    void *p;
    int glocked;

    if (cansleep) {
	glocked = ISAFS_GLOCK();
	if (glocked)
	    AFS_GUNLOCK();
	MALLOC(p, void *, asize, M_AFSGENERIC, M_WAITOK);
	if (glocked)
	    AFS_GLOCK();
    } else {
	MALLOC(p, void *, asize, M_AFSGENERIC, M_NOWAIT);
    }

    return (p);
}

void
osi_nbsd_Free(void *p, size_t asize)
{
    FREE(p, M_AFSGENERIC);
}

inline void *
afs_osi_Alloc(size_t asize) {
    return (osi_nbsd_Alloc(asize, 1));
}

inline void *
afs_osi_Alloc_NoSleep(size_t asize) {
    return (osi_nbsd_Alloc(asize, 0));
}

inline void
afs_osi_Free(void *buf, size_t asize) {
    osi_nbsd_Free(buf, asize);
}

inline void
afs_osi_FreeStr(char *x)
{
    afs_osi_Free(x, strlen(x) + 1);
}

/* XXXX OpenBSD avoids space pool, presumably Rees believed the kernel
 * allocator did as well or better */
#if 0
void
osi_FreeLargeSpace(void *p)
{
    osi_nbsd_Free(p, 0);
}

/* XXXX OpenBSD avoids space pool, presumably Rees believed the kernel
 * allocator did as well or better */
#if 0
void
osi_FreeSmallSpace(void *p)
{
    osi_nbsd_Free(p, 0);
}

void *
osi_AllocLargeSpace(size_t size)
{
    AFS_ASSERT_GLOCK();
    AFS_STATCNT(osi_AllocLargeSpace);
    return (osi_nbsd_Alloc(size, 1));
}

void *
osi_AllocSmallSpace(size_t size)
{
    AFS_ASSERT_GLOCK();
    AFS_STATCNT(osi_AllocSmallSpace);
    return (osi_nbsd_Alloc(size, 1));
}

#endif /* Space undef */

int
afs_syscall_icreate(dev, near_inode, param1, param2, param3, param4, retval)
    long *retval;
    long dev, near_inode, param1, param2, param3, param4;
{
    return EINVAL;
}

#endif /* Space undef */

int
afs_syscall_iopen(dev, inode, usrmod, retval)
    long *retval;
    int dev, inode, usrmod;
{
    return EINVAL;
}

int
afs_syscall_iincdec(dev, inode, inode_p1, amount)
     int dev, inode, inode_p1, amount;
{
    return EINVAL;
}

inline gid_t
osi_crgroupbyid(afs_ucred_t *acred, int gindex)
{
    struct kauth_cred *cr = acred;
    return (cr->cr_groups[gindex]);
}

/*
 * just calls kern_time.c:settime()
 */
void
afs_osi_SetTime(osi_timeval_t *atv)
{
#if 0
    printf("afs attempted to set clock; use \"afsd -nosettime\"\n");
#else
    struct timespec ts;
    AFS_GUNLOCK();
    ts.tv_sec = atv->tv_sec;
    ts.tv_nsec = atv->tv_usec * 1000;
    settime(osi_curproc()->l_proc, &ts); /* really takes a process */
    AFS_GLOCK();
#endif
}
