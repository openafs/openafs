/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _PARAM_I386_DJGPP_H_
#define _PARAM_I386_DJGPP_H_

#define AFS_NONFSTRANS
#define AFS_DJGPP_ENV         /* win95 env. */

#define AFS_MOUNT_AFS "afs"	/* The name of the filesystem type. */
#define AFS_SYSCALL 137
#include <afs/afs_sysnames.h>

#define AFS_USERSPACE_IP_ADDR 1
#define RXK_LISTENER_ENV 1
#define AFS_GCPAGS		0       /* if nonzero, garbage collect PAGs */


/* Machine / Operating system information */
#define SYS_NAME	"i386_win9x"
#define SYS_NAME_ID	SYS_NAME_ID_i386_win9x
#define AFSLITTLE_ENDIAN    1
#define AFS_HAVE_FFS            1       /* Use system's ffs. */
#define AFS_HAVE_STATVFS	0	/* System doesn't support statvfs */
#define AFS_VM_RDWR_ENV	1	/* read/write implemented via VM */

#endif /* _PARAM_I386_DJGPP_H_ */
