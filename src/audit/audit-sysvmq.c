/*
 * Copyright 2009, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>

/* only build on platforms that have SysV IPC support; i.e., when we
 * have sys/ipc.h */
#ifdef HAVE_SYS_IPC_H

#include <afs/param.h>

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <fcntl.h>

#include "audit-api.h"

/* solaris default is 2048 */
#define MAXMSG 2048

/* message queue size will be increased to this value
   if not already bigger */
#define MSGMNB (2*1024*1024)

static struct my_msgbuf {
    long mtype;
    char mtext[MAXMSG];
} msgbuffer;

static int mqid;

static struct mqaudit_stats {
    long all;
    long truncated;
    long lost;
} myauditstats;

static int truncated;

static void
send_msg(void)
{
    /* +1 to send the trailing '\0' in the message too so the
       receiver doesn't need to bother with it */
    if (msgsnd(mqid, &msgbuffer, strlen(msgbuffer.mtext)+1, IPC_NOWAIT) == -1) {
        myauditstats.lost++;
    } else if (truncated) {
        myauditstats.truncated++;
    }
    myauditstats.all++;
    msgbuffer.mtext[0] = 0;
    truncated = 0;
}

static void
append_msg(const char *format, ...)
{
    va_list vaList;
    int size, printed;

    size = MAXMSG - strlen(msgbuffer.mtext);

    va_start(vaList, format);
    printed = vsnprintf(&msgbuffer.mtext[strlen(msgbuffer.mtext)], size, format, vaList);
    va_end(vaList);

    /* A return value of size or more means that the output was truncated.
       If an output error is encountered, a negative value is returned. */
    if (size <= printed || printed == -1) {
        truncated = 1;
    }
}

static int
open_file(const char *fileName)
{
    int tempfd;
    struct msqid_ds msqdesc;

    msgbuffer.mtext[0] = 0;
    msgbuffer.mtype = 1;

    truncated = 0;
    myauditstats.all = 0;
    myauditstats.lost = 0;
    myauditstats.truncated = 0;

    /* try to create file for ftok if it doesn't already exist */
    tempfd = open(fileName, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if(tempfd != -1)
        close(tempfd);

    mqid = msgget(ftok(fileName, 1), S_IRUSR | S_IWUSR | IPC_CREAT);
    if (mqid == -1) {
        printf("Warning: auditlog message queue %s cannot be opened.\n", fileName);
        return 1;
    }

    /* increase message queue size */
    msgctl(mqid, IPC_STAT, &msqdesc);
    if (msqdesc.msg_qbytes < MSGMNB) {
        msqdesc.msg_qbytes = MSGMNB;
        msgctl(mqid, IPC_SET, &msqdesc);
    }

    return 0;
}

static void
print_interface_stats(FILE *out)
{
    fprintf(out, "audit statistics: %ld messages total, %ld truncated, %ld lost\n",
        myauditstats.all, myauditstats.truncated, myauditstats.lost);
}

const struct osi_audit_ops audit_sysvmq_ops = {
    &send_msg,
    &append_msg,
    &open_file,
    &print_interface_stats,
};

#endif /* HAVE_SYS_IPC_H */
