/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_AFS_PIOCTL_H
#define OPENAFS_AFS_PIOCTL_H

/* define the basic DeviceIoControl structure for communicating with the
 * cache manager.
 */

/* looks like pioctl block from the Unix world */
typedef struct ViceIoctl {
    long in_size;
    long out_size;
    void *in;
    void *out;
} viceIoctl_t;

/* Fake error code since NT errno.h doesn't define it */
#include <afs/errmap_nt.h>

extern int pioctl(char *pathp, afs_int32 opcode, struct ViceIoctl *blob,
		  afs_int32 follow);

extern int pioctl_utf8(char *pathp, afs_int32 opcode, struct ViceIoctl *blob,
                       afs_int32 follow);

#endif /* OPENAFS_AFS_PIOCTL_H */
