/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#ifndef _OSI_LOG_H__
#define _OSI_LOG_H__ 1

#include "osisleep.h"
#include "osibasel.h"
#include "osistatl.h"
#include "osifd.h"
#include "osiqueue.h"

#define OSI_LOG_DEFAULTSIZE	1000
#define OSI_LOG_STRINGSIZE	30
#define OSI_LOG_MAXPARMS	4	/* max # of int parms */

typedef struct osi_logEntry {
	long tid;			/* thread ID */
        unsigned long micros;		/* microsecond-based time stamp */
        char *formatp;			/* format string */
        long parms[OSI_LOG_MAXPARMS];	/* parms */
} osi_logEntry_t;

typedef struct osi_log {
	osi_queue_t q;			/* queue of all logs */
	char *namep;			/* name */
	long alloc;			/* allocated size */
        long nused;			/* number currently in use */
        long first;			/* index of first entry */
        CRITICAL_SECTION cs;		/* use this, rather than a higher-level
					 * lock, so we can log stuff from
					 * osi lock pkg */
        osi_logEntry_t *datap;		/* data for the log */
	int stringindex;		/* where to put new strings */
	int maxstringindex;		/* size of string array */
	char (*stringsp)[OSI_LOG_STRINGSIZE];	/* string array */
	int enabled;			/* true if enabled */
} osi_log_t;

typedef struct osi_logFD {
	osi_fd_t fd;			/* FD header */
	osi_log_t *logp;		/* logp */
        long first;			/* first index at time we started */
        long nused;			/* nused at tiem we started */
        long current;			/* counter we're at */
} osi_logFD_t;

extern long osi_logSize;

extern osi_log_t *osi_LogCreate(char *, long);

extern void osi_LogFree(osi_log_t *);

extern void osi_LogAdd(osi_log_t *, char *, long, long, long, long);

extern void osi_LogReset(osi_log_t *);

extern long osi_LogFDCreate(osi_fdType_t *, osi_fd_t **);

extern long osi_LogFDGetInfo(osi_fd_t *, osi_remGetInfoParms_t *);

extern long osi_LogFDClose(osi_fd_t *);

extern void osi_LogEnable(osi_log_t *);

extern void osi_LogDisable(osi_log_t *);

extern void osi_LogPanic(char *filep, long line);

extern void osi_LogPrint(osi_log_t *logp, HANDLE handle);

extern char *osi_LogSaveString(osi_log_t *logp, char *s);

/* define macros */
#define osi_Log0(l,f)		osi_LogAdd((l), (f), 0, 0, 0, 0)
#define osi_Log1(l,f,a)		osi_LogAdd((l), (f), (long) (a), 0, 0, 0)
#define osi_Log2(l,f,a,b)	osi_LogAdd((l), (f), (long) (a), (long) (b), 0, 0)
#define osi_Log3(l,f,a,b,c)	osi_LogAdd((l), (f), (long) (a), (long) (b), (long) (c), 0)
#define osi_Log4(l,f,a,b,c,d)	osi_LogAdd((l), (f), (long) (a), (long) (b), (long) (c), (long) (d))

#endif /*  _OSI_LOG_H__ */
