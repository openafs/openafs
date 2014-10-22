/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/afsutil.h>

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef AFS_NT40_ENV
#include <sys/param.h>
#include <unistd.h>
#else
#include <io.h>
#endif
#include "audit-api.h"

static FILE *auditout;

static void
send_msg(void)
{
    fprintf(auditout, "\n");
    fflush(auditout);
}

static void
append_msg(const char *format, ...)
{
    va_list vaList;

    va_start(vaList, format);
    vfprintf(auditout, format, vaList);
    va_end(vaList);
}

static int
open_file(const char *fileName)
{
    int tempfd, flags;
    char *oldName;

#ifndef AFS_NT40_ENV
    struct stat statbuf;

    if ((lstat(fileName, &statbuf) == 0)
        && (S_ISFIFO(statbuf.st_mode))) {
        flags = O_WRONLY | O_NONBLOCK;
    } else
#endif
    {
	afs_asprintf(&oldName, "%s.old", fileName);
	if (oldName == NULL) {
	    printf("Warning: Unable to create backup filename. Auditing ignored\n");
	    return 1;
	}
        renamefile(fileName, oldName);
        flags = O_WRONLY | O_TRUNC | O_CREAT;
	free(oldName);
    }
    tempfd = open(fileName, flags, 0666);
    if (tempfd > -1) {
        auditout = fdopen(tempfd, "a");
        if (!auditout) {
            printf("Warning: auditlog %s not writable, ignored.\n", fileName);
            return 1;
        }
    } else {
        printf("Warning: auditlog %s not writable, ignored.\n", fileName);
        return 1;
    }
    return 0;
}

static void
print_interface_stats(FILE *out)
{
    return;
}

const struct osi_audit_ops audit_file_ops = {
    &send_msg,
    &append_msg,
    &open_file,
    &print_interface_stats,
};
