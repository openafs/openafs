/*
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 */

#ifndef _OSI_LOG_H__
#define _OSI_LOG_H__ 1

#include "osi.h"
#ifndef DJGPP
#include "osisleep.h"
#include "osibasel.h"
#include "osistatl.h"
#endif /* !DJGPP */
#include "osifd.h"
#include "osiqueue.h"

#define OSI_LOG_DEFAULTSIZE	1000
#define OSI_LOG_STRINGSIZE	128
#define OSI_LOG_MAXPARMS	4	/* max # of int parms */

typedef struct osi_logEntry {
	size_t tid;			/* thread ID */
        unsigned long micros;		/* microsecond-based time stamp */
        char *formatp;			/* format string */
        long parms[OSI_LOG_MAXPARMS];	/* parms */
} osi_logEntry_t;

typedef struct osi_log {
	osi_queue_t q;			/* queue of all logs */
	char *namep;			/* name */
	size_t alloc;			/* allocated size */
        long nused;			/* number currently in use */
        long first;			/* index of first entry */
        Crit_Sec cs;			/* use this, rather than a higher-level
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

extern osi_log_t *osi_LogCreate(char *, size_t);

extern void osi_LogFree(osi_log_t *);

extern void osi_LogAdd(osi_log_t *, char *, size_t, size_t, size_t, size_t);

extern void osi_LogReset(osi_log_t *);

extern long osi_LogFDCreate(osi_fdType_t *, osi_fd_t **);

#ifndef DJGPP
extern long osi_LogFDGetInfo(osi_fd_t *, osi_remGetInfoParms_t *);
#endif

extern long osi_LogFDClose(osi_fd_t *);

extern void osi_LogEnable(osi_log_t *);

extern void osi_LogDisable(osi_log_t *);

extern void osi_LogPanic(char *filep, size_t line);

extern void osi_LogPrint(osi_log_t *logp, FILE_HANDLE handle);

extern char *osi_LogSaveString(osi_log_t *logp, char *s);
extern void osi_InitTraceOption();
extern void osi_LogEvent0(char *a,char *b);
extern void osi_LogEvent(char *a,char *b,char *c,...);
extern char *osi_HexifyString(char *s);

/* define macros */
#define osi_Log0(l,f)		if ((l) && (l)->enabled) osi_LogAdd((l), (f), 0, 0, 0, 0)
#define osi_Log1(l,f,a)		if ((l) && (l)->enabled) osi_LogAdd((l), (f), (size_t) (a), 0, 0, 0)
#define osi_Log2(l,f,a,b)	if ((l) && (l)->enabled) osi_LogAdd((l), (f), (size_t) (a), (size_t) (b), 0, 0)
#define osi_Log3(l,f,a,b,c)	if ((l) && (l)->enabled) osi_LogAdd((l), (f), (size_t) (a), (size_t) (b), (size_t) (c), 0)
#define osi_Log4(l,f,a,b,c,d)	if ((l) && (l)->enabled) osi_LogAdd((l), (f), (size_t) (a), (size_t) (b), (size_t) (c), (size_t) (d))
#define osi_Log5(l,f,a,b,c,d,e)	if ((l) && (l)->enabled) osi_LogAdd((l), (f), (size_t) (a), (size_t) (b), (size_t) (c), (size_t) (d), (size_t) (e))

#ifdef DEBUG_VERBOSE
#define DEBUG_EVENT1(a,b,c) {HANDLE h; char *ptbuf[1],buf[132];\
	h = RegisterEventSource(NULL, a);\
	sprintf(buf, b,c);\
	ptbuf[0] = buf;\
	ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, (const char **)ptbuf, NULL);\
	DeregisterEventSource(h);}
#define DEBUG_EVENT0(a) {HANDLE h; char *ptbuf[1];\
	h = RegisterEventSource(NULL, a);\
	ptbuf[0] = "";\
	ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0,(const char **) ptbuf, NULL);\
	DeregisterEventSource(h);}
#define DEBUG_EVENT2(a,b,c,d) {HANDLE h; char *ptbuf[1],buf[132];\
	h = RegisterEventSource(NULL, a);\
	sprintf(buf, b,c,d);\
	ptbuf[0] = buf;\
	ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0,(const char **) ptbuf, NULL);\
	DeregisterEventSource(h);}
#define DEBUG_EVENT3(a,b,c,d,e) {HANDLE h; char *ptbuf[1],buf[132];\
	h = RegisterEventSource(NULL, a);\
	sprintf(buf, b,c,d,e);\
	ptbuf[0] = buf;\
	ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0,(const char **)ptbuf, NULL);\
	DeregisterEventSource(h);}
#define DEBUG_EVENT4(a,b,c,d,e,f) {HANDLE h; char *ptbuf[1],buf[132];\
	h = RegisterEventSource(NULL, a);\
	sprintf(buf, b,c,d,e,f);\
	ptbuf[0] = buf;\
	ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0,(const char **) ptbuf, NULL);\
	DeregisterEventSource(h);}
#define DEBUG_EVENT5(a,b,c,d,e,f,g) {HANDLE h; char *ptbuf[1],buf[132];\
	h = RegisterEventSource(NULL, a);\
	sprintf(buf, b,c,d,e,f,g);\
	ptbuf[0] = buf;\
	ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0,(const char **) ptbuf, NULL);\
	DeregisterEventSource(h);}
#define DEBUG_EVENT6(a,b,c,d,e,f,g,h) {HANDLE h; char *ptbuf[1],buf[132];\
	h = RegisterEventSource(NULL, a);\
	sprintf(buf,b,c,d,e,f,g,h);\
	ptbuf[0] = buf;\
	ReportEvent(h, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0,(const char **) ptbuf, NULL);\
	DeregisterEventSource(h);}
#else
#define DEBUG_EVENT0(a)
#define DEBUG_EVENT1(a,b,c)
#define DEBUG_EVENT2(a,b,c,d)
#define DEBUG_EVENT3(a,b,c,d,e)
#define DEBUG_EVENT4(a,b,c,d,e,f)
#define DEBUG_EVENT5(a,b,c,d,e,f,g)
#define DEBUG_EVENT6(a,b,c,d,e,f,g,h)
#endif

#endif /*  _OSI_LOG_H__ */
