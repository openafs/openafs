/*
 * Copyright (C)  1998  Transarc Corporation.  All rights reserved.
 *
 */

#ifndef TRANSARC_AFS_PIOCTL_H
#define TRANSARC_AFS_PIOCTL_H

#include <afs/param.h>
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

extern long pioctl(char *pathp, long opcode, struct ViceIoctl *blob, int follow);

#endif /* TRANSARC_AFS_PIOCTL_H */
