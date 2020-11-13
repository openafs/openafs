/*
 * Copyright 2009, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

/* only build on platforms that have SysV IPC support; i.e., when we
 * have sys/ipc.h */
#ifdef HAVE_SYS_IPC_H

#include <sys/ipc.h>
#include <sys/msg.h>

#include "audit-api.h"

/* message queue size will be increased to this value
   if not already bigger */
#define MSGMNB (2*1024*1024)

/* msgsnd requires a structure with the following layout. See manpage for
 * msgsnd() for the details */
struct my_msgbuf {
    long mtype;
    char mtext[OSI_AUDIT_MAXMSG];
};

struct mqaudit_stats {
    long all;
    long truncated;
    long lost;
};

struct sysvmq_context {
    int mqid;
    struct my_msgbuf msgbuffer;
    struct mqaudit_stats myauditstats;
};

static void
send_msg(void *rock, const char *message, int msglen, int truncated)
{
    struct sysvmq_context *ctx = rock;

    if (msglen >= OSI_AUDIT_MAXMSG) {
	truncated = 1;
	msglen = OSI_AUDIT_MAXMSG - 1;
    }

    /* Copy the data into the msgsnd structure */
    memcpy(&ctx->msgbuffer.mtext, message, msglen+1);
    ctx->msgbuffer.mtext[msglen] = '\0';

    /* Include the trailing zero in the msgsnd */
    if (msgsnd(ctx->mqid, &ctx->msgbuffer, msglen+1, IPC_NOWAIT) == -1) {
	ctx->myauditstats.lost++;
    } else if (truncated) {
	ctx->myauditstats.truncated++;
    }
    ctx->myauditstats.all++;
}

static int
open_file(void *rock, const char *fileName)
{
    struct sysvmq_context *ctx = rock;
    int tempfd;
    struct msqid_ds msqdesc;

    ctx->msgbuffer.mtext[0] = 0;
    ctx->msgbuffer.mtype = 1;

    ctx->myauditstats.all = 0;
    ctx->myauditstats.lost = 0;
    ctx->myauditstats.truncated = 0;

    /* try to create file for ftok if it doesn't already exist */
    tempfd = open(fileName, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if(tempfd != -1)
        close(tempfd);

    ctx->mqid = msgget(ftok(fileName, 1), S_IRUSR | S_IWUSR | IPC_CREAT);
    if (ctx->mqid == -1) {
        printf("Warning: auditlog message queue %s cannot be opened.\n", fileName);
        return 1;
    }

    /* increase message queue size */
    msgctl(ctx->mqid, IPC_STAT, &msqdesc);
    if (msqdesc.msg_qbytes < MSGMNB) {
        msqdesc.msg_qbytes = MSGMNB;
	msgctl(ctx->mqid, IPC_SET, &msqdesc);
    }

    return 0;
}

static void
print_interface_stats(void *rock, FILE *out)
{
    struct sysvmq_context *ctx = rock;

    fprintf(out, "audit statistics: %ld messages total, %ld truncated, %ld lost\n",
	ctx->myauditstats.all, ctx->myauditstats.truncated, ctx->myauditstats.lost);
}

static void *
create_interface(void)
{
    struct sysvmq_context *ctx;

    ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
	printf("error allocating memory\n");
	return NULL;
    }
    return ctx;
}

static void
close_interface(void **rock)
{
    struct sysvmq_context *ctx = *rock;
    if (ctx == NULL)
	return;

    free(ctx);
    *rock = NULL;
}

const struct osi_audit_ops audit_sysvmq_ops = {
    &send_msg,
    &open_file,
    &print_interface_stats,
    &create_interface,
    &close_interface,
    NULL,     /* set_option */
    NULL,     /* open_interface */
};

#endif /* HAVE_SYS_IPC_H */
