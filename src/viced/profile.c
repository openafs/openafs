/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* profile.c -- routines to deal with profiling.   2 June 1986 */

#include <afsconfig.h>
#include <afs/param.h>


#include <stdio.h>
#include <sys/file.h>
#include <signal.h>

#if defined(sun) || defined(mac2)
#define PROFSTART	0x8000
#endif
#ifdef vax
#define PROFSTART	2
#endif
#ifdef	mips
#define	PROFSTART	2	/* Verify This! */
#endif
#ifdef	NeXT
#define	PROFSTART	0x2000
#endif
#ifdef	__alpha
#define	PROFSTART	0
#endif
#define		SCALE_1_TO_1	0x10000L

#ifdef	AFS_OSF_ENV
extern void *malloc();
#else
extern char *malloc();
#endif
extern int etext;
void AllocProfBuf();

static char *profBuf = NULL;
static int profBufSize;
static int profSig = -1;
static int profiling = 0;
static struct {
    afs_int32 startpc;
    afs_int32 endpc;
    afs_int32 count;
} profileHeader;

void
StartProfiling()
{
#if !defined (AFS_AIX_ENV) && !defined (AFS_HPUX_ENV)
/* Soon should handle aix profiling */
    AllocProfBuf();
    memset(profBuf, 0, profBufSize);
    /* the following code is to replace the monitor call below  */
    /*  monitor (PROFSTART, &etext, profBuf, profBufSize, 0); */
    profileHeader.startpc = PROFSTART;
    profileHeader.endpc = (afs_int32) & etext;
    profileHeader.count = 0;
    profil(profBuf, profBufSize, PROFSTART, SCALE_1_TO_1);
    profiling = 1;
    return;
#endif
}

EndProfiling()
{
#if !defined (AFS_AIX_ENV) && !defined (AFS_HPUX_ENV)
/* Soon should handle aix profiling */
    int f;

    /* monitor(0); */
    profil(0, 0, 0, 0);
    profiling = 0;
    f = open("mon.out", O_CREAT | O_TRUNC | O_WRONLY, 0666);
    if (f <= 0)
	return;
    write(f, &profileHeader, sizeof(profileHeader));
    write(f, profBuf, profBufSize);
    close(f);
#endif
}

/* Allocate the profiling buffer */
void
AllocProfBuf()
{
#if !defined (AFS_AIX_ENV) && !defined (AFS_HPUX_ENV)
/* Soon should handle aix profiling */
    if (profBuf != NULL)
	return;
    profBufSize = (int)&etext - PROFSTART;
    profBuf = (char *)malloc(profBufSize);
    if (profBuf == NULL) {
	fprintf(stderr, "Couldn't get profiling buffer.\n");
	return;
    }
#endif
}
int ProfileToggle();

/* Arrange to start and stop profiling when signo is sent. */
void
ProfileSig(int signo)
{
#if !defined (AFS_AIX_ENV) && !defined (AFS_HPUX_ENV)
/* Soon should handle aix profiling */
    profSig = signo;
    if (signo >= 0) {
	AllocProfBuf();
	signal(signo, ProfileToggle);
    } else
	signal(profSig, SIG_DFL);
#endif
}

/* Toggle profiling.  Can be called as a signal handler. */
ProfileToggle()
{
#if !defined (AFS_AIX_ENV) && !defined (AFS_HPUX_ENV)
/* Soon should handle aix profiling */
    static char *onMsg = "Profiling turned on.\n";
    static char *offMsg = "Profiling turned off; mon.out written.\n";

    if (profiling) {
	EndProfiling();
	write(fileno(stdout), offMsg, strlen(offMsg));
    } else {
	StartProfiling();
	write(fileno(stdout), onMsg, strlen(onMsg));
    }
#endif
}
