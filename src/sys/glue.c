/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * This file contains any necessary C glue to allow programs to communicate
 * with the AFS kernel module.  The necessary lower-level glue is defined in
 * syscall.s.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/sys/glue.c,v 1.1.2.2 2005/10/15 15:21:19 shadow Exp $");

#include <afs/afs_args.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#if defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)
#include <unistd.h>
#else
#include <stdio.h>
#endif
#ifdef AFS_SUN5_ENV
#include <fcntl.h>
#endif
#include "afssyscalls.h"

#ifdef AFS_LINUX20_ENV
int proc_afs_syscall(long syscall, long param1, long param2, long param3, 
		     long param4, int *rval) {
  struct afsprocdata syscall_data;
  int fd = open(PROC_SYSCALL_FNAME, O_RDWR);
  if(fd < 0)
      fd = open(PROC_SYSCALL_ARLA_FNAME, O_RDWR);
  if(fd < 0)
    return -1;

  syscall_data.syscall = syscall;
  syscall_data.param1 = param1;
  syscall_data.param2 = param2;
  syscall_data.param3 = param3;
  syscall_data.param4 = param4;

  *rval = ioctl(fd, VIOC_SYSCALL, &syscall_data);

  close(fd);

  return 0;
}
#endif
