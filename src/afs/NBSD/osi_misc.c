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
afs_osi_suser(afs_ucred_t *credp)
{
    int code;
/*
 *	lwp->l_acflag is gone in NBSD50. It was "Accounting" stuff.
 *	lwp->l_ru is what is listed as "accounting information" now, so this
 *	may or may not work...
 */
#ifdef AFS_NBSD50_ENV
    code = kauth_authorize_generic(credp,
				   KAUTH_GENERIC_ISSUSER,
				   &curlwp->l_ru);
#else
    code = kauth_authorize_generic(credp,
				   KAUTH_GENERIC_ISSUSER,
				   &curlwp->l_acflag);
#endif
    return (code == 0);
}

int
afs_syscall_icreate(long dev, long near_inode, long param1, long param2,
		    long param3, long param4, register_t *retval)
{
    return EINVAL;
}

int
afs_syscall_iopen(int dev, int inode, int usrmod, register_t *retval)
{
    return EINVAL;
}

int
afs_syscall_iincdec(int dev, int inode, int inode_p1, int amount)
{
    return EINVAL;
}
