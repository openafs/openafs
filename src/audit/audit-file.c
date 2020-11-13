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

#include <roken.h>

#include <afs/afsutil.h>

#include "audit-api.h"

struct file_context {
    FILE *auditout;
};

static void
send_msg(void *rock, const char *message, int msglen, int truncated)
{
    struct file_context *ctx = rock;

    fwrite(message, 1, msglen, ctx->auditout);
    fputc('\n', ctx->auditout);

    fflush(ctx->auditout);
}

static int
open_file(void *rock, const char *fileName)
{
    struct file_context *ctx = rock;
    int tempfd, flags, r;
    char *oldName;

#ifndef AFS_NT40_ENV
    struct stat statbuf;

    if ((lstat(fileName, &statbuf) == 0)
        && (S_ISFIFO(statbuf.st_mode))) {
        flags = O_WRONLY | O_NONBLOCK;
    } else
#endif
    {
	r = asprintf(&oldName, "%s.old", fileName);
	if (r < 0 || oldName == NULL) {
	    printf("Warning: Unable to create backup filename. Auditing ignored\n");
	    return 1;
	}
        rk_rename(fileName, oldName);
        flags = O_WRONLY | O_TRUNC | O_CREAT;
	free(oldName);
    }
    tempfd = open(fileName, flags, 0666);
    if (tempfd > -1) {
	ctx->auditout = fdopen(tempfd, "a");
	if (!ctx->auditout) {
	    printf("Warning: fdopen failed for auditlog %s, ignored.\n", fileName);
            return 1;
        }
    } else {
        printf("Warning: auditlog %s not writable, ignored.\n", fileName);
        return 1;
    }
    return 0;
}

static void
print_interface_stats(void *rock, FILE *out)
{
    return;
}

static void *
create_interface(void)
{
    struct file_context *ctx;
    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
	printf("error allocating memory\n");
	return NULL;
    }
    return ctx;
}

static void
close_interface(void **rock)
{
    struct file_context *ctx = *rock;
    if (!ctx)
	return;

    if (ctx->auditout)
	fclose(ctx->auditout);
    free(ctx);
    *rock = NULL;
}
const struct osi_audit_ops audit_file_ops = {
    &send_msg,
    &open_file,
    &print_interface_stats,
    &create_interface,
    &close_interface,
    NULL,    /* set_option */
    NULL,    /* open_interface */
};
