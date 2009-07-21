/* 
 * osi_misc.c
 *
 * $Id$
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
#include <sys/types.h>
#include <sys/malloc.h>

/*
 * afs_suser() returns true if the caller is superuser, false otherwise.
 *
 * Note that it must NOT set errno.
 */

/* 
 * OpenBSD version of afs_osi_suser() by Jim Rees.
 * See osi_machdep.h for afs_suser macro. It simply calls afs_osi_suser()
 * with the creds of the current process.
 */

int
afs_osi_suser(void *credp)
{
#ifdef AFS_OBSD35_ENV
    return (suser_ucred((struct ucred *)credp) ? 0 : 1);
#else
    return (suser((struct ucred *)credp, &curproc->p_acflag) ? 0 : 1);
#endif
}

/*
 * reworked for netbsd and openbsd at 4.0/4.4
 */

#if defined(AFS_OBSD42_ENV)
/* ripped out MALLOC/FREE */

void *
osi_obsd_Alloc(size_t asize, int cansleep)
{
  void *p;
  int glocked;

  if (cansleep) {
    glocked = ISAFS_GLOCK();
    if (glocked)
      AFS_GUNLOCK();
    p = malloc(asize, M_AFSGENERIC, M_WAITOK);
    if (glocked)
      AFS_GLOCK();
  } else {
    p = malloc(asize, M_AFSGENERIC, M_NOWAIT);
  }

  return (p);
}

void
osi_obsd_Free(void *p, size_t asize)
{
  free(p, M_AFSGENERIC);
}

#else
void *
osi_obsd_Alloc(size_t asize, int cansleep)
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
osi_obsd_Free(void *p, size_t asize)
{
  FREE(p, M_AFSGENERIC);
}
#endif

#if 0 /* XXX */
/* I speculate this usage may be more correct than definitions
 * in afs_osi_alloc.c, which I discarded successfully for FreeBSD 7+,
 * and am trying to discard for NetBSD 4.x, but until tested, I'm
 * not rocking the boat.  Matt.
 */
   
void
osi_FreeLargeSpace(void *p)
{
  osi_obsd_Free(p, 0);
}

void
osi_FreeSmallSpace(void *p)
{
  osi_obsd_Free(p, 0);
}

void *
osi_AllocLargeSpace(size_t size)
{
  AFS_ASSERT_GLOCK();
  AFS_STATCNT(osi_AllocLargeSpace);
  return (osi_obsd_Alloc(size, 1));
}
#endif

int
afs_syscall_icreate(dev, near_inode, param1, param2, param3, param4, retval)
     long *retval;
     long dev, near_inode, param1, param2, param3, param4;
{
    return EINVAL;
}

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
