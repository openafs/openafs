/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <rpc.h>
#include <malloc.h>
#include "osi.h"
#include "dbrpc.h"
#include <stdio.h>
#include <assert.h>

/* the size; overrideable */
long osi_logSize = OSI_LOG_DEFAULTSIZE;

static osi_once_t osi_logOnce;

osi_log_t *osi_allLogsp;	/* all logs known; for use during panic */

unsigned long osi_logFreq;	/* 0, or frequency of high perf counter */
unsigned long osi_logTixToMicros;	/* mult. correction factor */

osi_fdOps_t osi_logFDOps = {
	osi_LogFDCreate,
        osi_LogFDGetInfo,
        osi_LogFDClose
};

/* create a new log, taking a name and a size in entries (not words) */
osi_log_t *osi_LogCreate(char *namep, long size)
{
	osi_log_t *logp;
        osi_fdType_t *typep;
        char tbuffer[256];
        LARGE_INTEGER bigFreq;
        LARGE_INTEGER bigTemp;
        LARGE_INTEGER bigJunk;
	
	if (osi_Once(&osi_logOnce)) {
		QueryPerformanceFrequency(&bigFreq);
                if (bigFreq.LowPart == 0 && bigFreq.HighPart == 0)
                	osi_logFreq = 0;
                else {
			/* turn frequency into ticks per 10 micros */
			bigTemp.LowPart = 100000;
                        bigTemp.HighPart = 0;
                        osi_logTixToMicros = 10;
                        bigFreq = LargeIntegerDivide(bigFreq, bigTemp, &bigJunk);

			/* check if resolution is too fine or to gross for this to work */
                        if (bigFreq.HighPart > 0 || bigFreq.LowPart < 8)
                        	osi_logFreq = 0;	/* too big to represent as long */
                        else
                        	osi_logFreq = bigFreq.LowPart;
                }

		/* done with init */
		osi_EndOnce(&osi_logOnce);
        }

        logp = malloc(sizeof(osi_log_t));
        memset(logp, 0, sizeof(osi_log_t));
        logp->namep = malloc(strlen(namep)+1);
        strcpy(logp->namep, namep);
        
        osi_QAdd((osi_queue_t **) &osi_allLogsp, &logp->q);

	/* compute size we'll use */
        if (size == 0) size = osi_logSize;

	/* handle init for this size */
        logp->alloc = size;
        logp->datap = malloc(size * sizeof(osi_logEntry_t));

	/* init strings array */
	logp->maxstringindex = size/10;
	logp->stringindex = 0;
	logp->stringsp = malloc((size/10) * OSI_LOG_STRINGSIZE);
 
        /* and sync */
        InitializeCriticalSection(&logp->cs);
        
	strcpy(tbuffer, "log:");
        strcat(tbuffer, namep);
	typep = osi_RegisterFDType(tbuffer, &osi_logFDOps, logp);
	if (typep) {
		/* add formatting info */
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONINT, 0,
			"Thread ID", OSI_DBRPC_HEX);
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONSTRING, 1,
			"Time (mics)", 0);
	}
	
        return logp;
}

/* we just panic'd.  Turn off all logging adding special log record
 * to all enabled logs.  Be careful not to wait for a lock.
 */
void osi_LogPanic(char *filep, long lineNumber)
{
	osi_log_t *tlp;

        for(tlp = osi_allLogsp; tlp; tlp = (osi_log_t *) osi_QNext(&tlp->q)) {
		if (!tlp->enabled) continue;

		/* otherwise, proceed */
		if (filep)
	                osi_LogAdd(tlp, "**PANIC** (file %s:%d)", (long) filep, lineNumber, 0, 0);
		else
			osi_LogAdd(tlp, "**PANIC**", 0, 0, 0, 0);
		
                /* should grab lock for this, but we're in panic, and better safe than
                 * sorry.
                 */
                tlp->enabled = 0;
        }
}

/* reset the contents of a log */
void osi_LogReset(osi_log_t *logp)
{
	if (logp) {
		EnterCriticalSection(&logp->cs);
		logp->nused = 0;
		LeaveCriticalSection(&logp->cs);
	}
}

/* free a log */
void osi_LogFree(osi_log_t *logp)
{
	if (!logp) return;

	osi_QRemove((osi_queue_t **) &osi_allLogsp, &logp->q);

	free(logp->namep);
        free(logp->datap);
	DeleteCriticalSection(&logp->cs);
        free(logp);
}

/* add an element to a log */
void osi_LogAdd(osi_log_t *logp, char *formatp, long p0, long p1, long p2, long p3)
{
	osi_logEntry_t *lep;
        long ix;
        LARGE_INTEGER bigTime;

	/* handle init races */
	if (!logp) return;

	/* do this w/o locking for speed; it is obviously harmless if we're off
         * by a bit.
         */
	if (!logp->enabled) return;
        
	EnterCriticalSection(&logp->cs);
	if (logp->nused < logp->alloc) logp->nused++;
	else {
        	logp->first++;
                if (logp->first >= logp->alloc) logp->first -= logp->alloc;
        }
        ix = logp->first + logp->nused - 1;
        if (ix >= logp->alloc) ix -= logp->alloc;

        lep = logp->datap + ix;	/* ptr arith */
        lep->tid = GetCurrentThreadId();

	/* get the time, using the high res timer if available */
        if (osi_logFreq) {
		QueryPerformanceCounter(&bigTime);
		lep->micros = (bigTime.LowPart / osi_logFreq) * osi_logTixToMicros;
        }
        else lep->micros = GetCurrentTime() * 1000;

        lep->formatp = formatp;
        lep->parms[0] = p0;
        lep->parms[1] = p1;
        lep->parms[2] = p2;
        lep->parms[3] = p3;
	LeaveCriticalSection(&logp->cs);
}

void osi_LogPrint(osi_log_t *logp, HANDLE handle)
{
	char wholemsg[1000], msg[1000];
	int i, ix, ioCount;
	osi_logEntry_t *lep;

	if (!logp->enabled) return;

	EnterCriticalSection(&logp->cs);

	for (ix = logp->first, i = 0;
	     i < logp->nused;
	     i++, ix++, (ix >= logp->alloc ? ix -= logp->alloc : 0)) {
		lep = logp->datap + ix;		/* pointer arithmetic */
		sprintf(msg, lep->formatp,
			lep->parms[0], lep->parms[1],
			lep->parms[2], lep->parms[3]);
		sprintf(wholemsg, "time %d.%06d, pid %d %s\n",
			lep->micros / 1000000,
			lep->micros % 1000000,
			lep->tid, msg);
		if (!WriteFile(handle, wholemsg, strlen(wholemsg),
				&ioCount, NULL))
			break;
	}

	LeaveCriticalSection(&logp->cs);
}

char *osi_LogSaveString(osi_log_t *logp, char *s)
{
	char *saveplace = logp->stringsp[logp->stringindex];

	if (s == NULL) return NULL;

	if (strlen(s) >= OSI_LOG_STRINGSIZE)
		sprintf(saveplace, "...%s",
			s + strlen(s) - (OSI_LOG_STRINGSIZE - 4));
	else
		strcpy(saveplace, s);
	logp->stringindex++;
	if (logp->stringindex >= logp->maxstringindex)
	    logp->stringindex = 0;

	return saveplace;
}

long osi_LogFDCreate(osi_fdType_t *typep, osi_fd_t **outpp)
{
	osi_logFD_t *lfdp;
	osi_log_t *logp;
        
        lfdp = malloc(sizeof(*lfdp));
        logp = lfdp->logp = typep->rockp;	/* the log we were created for */
        EnterCriticalSection(&logp->cs);
	lfdp->nused = logp->nused;
        lfdp->first = logp->first;
        lfdp->current = 0;
	LeaveCriticalSection(&logp->cs);

	*outpp = &lfdp->fd;
        return 0;
}

long osi_LogFDGetInfo(osi_fd_t *ifd, osi_remGetInfoParms_t *outp)
{
	osi_logFD_t *lfdp;
        osi_log_t *logp;
        osi_logEntry_t *lep;
        char tbuffer[256];
        long ix;
        
        lfdp = (osi_logFD_t *) ifd;
        logp = lfdp->logp;
        
	/* see if we're done */
	if (lfdp->current >= lfdp->nused) return OSI_DBRPC_EOF;
        
	/* grab lock */
	EnterCriticalSection(&logp->cs);

        /* compute which one we want */
        ix = lfdp->first + lfdp->current;
        if (ix >= logp->alloc) ix -= logp->alloc;
        lfdp->current++;
        lep = logp->datap + ix;	/* ptr arith to current index */

	sprintf(tbuffer, lep->formatp, lep->parms[0], lep->parms[1],
        	lep->parms[2], lep->parms[3]);

	/* now copy out info */
        strcpy(outp->sdata[0], tbuffer);
        sprintf(tbuffer, "%5.6f", ((double)lep->micros)/1000000.0);
        strcpy(outp->sdata[1], tbuffer);
        outp->idata[0] = lep->tid;
        outp->scount = 2;
        outp->icount = 1;

	LeaveCriticalSection(&logp->cs);
        return 0;
}

long osi_LogFDClose(osi_fd_t *ifdp)
{
	free(ifdp);
        return 0;
}

void osi_LogEnable(osi_log_t *logp)
{
	if (logp)
		logp->enabled = 1;
}

void osi_LogDisable(osi_log_t *logp)
{
	if (logp)
		logp->enabled = 0;
}
