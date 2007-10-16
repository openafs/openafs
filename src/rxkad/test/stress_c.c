/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* RX Authentication Stress test: client side code. */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/rxkad/test/stress_c.c,v 1.9.2.1 2007/04/10 18:43:45 shadow Exp $");

#include <afs/stds.h>
#include <sys/types.h>
#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#endif
#include <afs/com_err.h>
#include <afs/afsutil.h>
#include <rx/rxkad.h>
#include <afs/auth.h>
#include "stress.h"
#include "stress_internal.h"
#ifdef AFS_PTHREAD_ENV
#include <pthread.h>
#define FT_ApproxTime() (int)time(0)
#endif

extern int maxSkew;

static char *whoami;

static long
GetServer(aname)
     IN char *aname;
{
    register struct hostent *th;
    long addr;

    th = gethostbyname(aname);
    if (!th) {
	fprintf(stderr, "host %s not found\n", aname);
	return errno;
    }
    memcpy(&addr, th->h_addr, sizeof(addr));
    return addr;
}

static long
GetToken(versionP, session, ticketLenP, ticket, cell)
     OUT long *versionP;
     OUT struct ktc_encryptionKey *session;
     OUT int *ticketLenP;
     OUT char *ticket;
{
    struct ktc_principal sname;
    struct ktc_token ttoken;
    long code;

    strcpy(sname.cell, cell);
    sname.instance[0] = 0;
    strcpy(sname.name, "afs");
    code = ktc_GetToken(&sname, &ttoken, sizeof(ttoken), NULL);
    if (code)
	return code;

    *versionP = ttoken.kvno;
    *ticketLenP = ttoken.ticketLen;
    memcpy(ticket, ttoken.ticket, ttoken.ticketLen);
    memcpy(session, &ttoken.sessionKey, sizeof(struct ktc_encryptionKey));
    return 0;
}

static long
GetTicket(versionP, session, ticketLenP, ticket, cell)
     OUT long *versionP;
     OUT struct ktc_encryptionKey *session;
     OUT int *ticketLenP;
     OUT char *ticket;
{
    long code;

    /* create random session key, using key for seed to good random */
    des_init_random_number_generator(&serviceKey);
    code = des_random_key(session);
    if (code)
	return code;

    /* now create the actual ticket */
    *ticketLenP = 0;
    code =
	tkt_MakeTicket(ticket, ticketLenP, &serviceKey, RXKST_CLIENT_NAME,
		       RXKST_CLIENT_INST, cell,
		       /*start,end */ 0, 0xffffffff, session, /*host */ 0,
		       RXKST_SERVER_NAME, RXKST_SERVER_NAME);
    /* parms were buffer, ticketlen, key to seal ticket with, principal name,
     * instance and cell, start time, end time, session key to seal in ticket,
     * inet host, server name and server instance */
    if (code)
	return code;
    *versionP = serviceKeyVersion;
    return 0;
}
struct client {
    struct rx_connection *conn;
    u_long sendLen;		/* parameters for call to Copious */
    u_long recvLen;
    u_long *fastCalls;		/* number of calls to perform */
    u_long *slowCalls;
    u_long *copiousCalls;
};

static long
Copious(c, buf, buflen)
     IN struct client *c;
     IN u_char *buf;
     IN u_long buflen;
{
    long code;
    struct rx_call *call;
    long i;
    long inlen = c->sendLen;
    long outlen = c->recvLen;
    long d = 23;
    long mysum;
    long outsum;

    mysum = 0;
    for (i = 0; i < inlen; i++)
	mysum += (d++ & 0xff);

    call = rx_NewCall(c->conn);
    code = StartRXKST_Copious(call, inlen, mysum, outlen);
    if (code == 0) {
	long tlen;
	long xfer = 0;
	long n;
	d = 23;
	while (xfer < inlen) {
	    tlen = inlen - xfer;
	    if (tlen > buflen)
		tlen = buflen;
	    for (i = 0; i < tlen; i++)
		buf[i] = (d++ & 0xff);
	    n = rx_Write(call, buf, tlen);
	    if (n != tlen) {
		if (n < 0)
		    code = n;
		else
		    code = RXKST_WRITESHORT;
		break;
	    }
	    xfer += tlen;
	}
	if (code == 0) {
	    xfer = 0;
	    mysum = 0;
	    while (xfer < outlen) {
		tlen = outlen - xfer;
		if (tlen > buflen)
		    tlen = buflen;
		n = rx_Read(call, buf, tlen);
		if (n != tlen) {
		    if (n < 0)
			code = n;
		    else
			code = RXKST_READSHORT;
		    break;
		}
		for (i = 0; i < tlen; i++)
		    mysum += buf[i];
		xfer += tlen;
	    }
	}
    }
    if (code == 0)
	code = EndRXKST_Copious(call, &outsum);
    code = rx_EndCall(call, code);
    if (code)
	return code;
    if (outsum != mysum) {
	return RXKST_BADOUTPUTSUM;
    }
    return 0;
}

static long
DoClient(index, rock)
     IN u_int index;
     IN opaque rock;
{
    struct client *c = (struct client *)rock;
    long code;
    int i;
    u_long n, inc_n;

    n = 95678;
    for (i = 0; i < c->fastCalls[index]; i++) {
	code = RXKST_Fast(c->conn, n, &inc_n);
	if (code)
	    return (code);
	if (n + 1 != inc_n)
	    return RXKST_INCFAILED;
	n++;
    }

    for (i = 0; i < c->slowCalls[index]; i++) {
	u_long ntime;
	u_long now;
	code = RXKST_Slow(c->conn, 1, &ntime);
	if (code)
	    return (code);
	now = FT_ApproxTime();
	if ((ntime < now - maxSkew) || (ntime > now + maxSkew))
	    return RXKST_TIMESKEW;
    }

    if (c->copiousCalls[index] > 0) {
	u_long buflen = 10000;
	u_char *buf = (u_char *) osi_Alloc(buflen);
	for (i = 0; i < c->copiousCalls[index]; i++) {
	    code = Copious(c, buf, buflen);
	    if (code)
		break;
	}
	osi_Free(buf, buflen);
	if (code)
	    return code;
    }
    return 0;
}

struct worker {
    struct worker *next;
    long exitCode;		/* is PROCESSRUNNING until exit */
    int index;
    opaque rock;
    long (*proc) ();
};

#ifdef AFS_PTHREAD_ENV
static pthread_once_t workerOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t workerLock;
static pthread_cond_t workerCV;
void
WorkerInit(void)
{
    pthread_mutex_init(&workerLock, NULL);
    pthread_cond_init(&workerCV, NULL);
}
#endif
static struct worker *workers;

static long
DoWorker(w)
     IN struct worker *w;
{
    long code;
    code = (*w->proc) (w->index, w->rock);
#ifdef AFS_PTHREAD_ENV
    pthread_mutex_lock(&workerLock);
#endif
    w->exitCode = code;
#ifdef AFS_PTHREAD_ENV
    pthread_mutex_unlock(&workerLock);
#endif
#ifdef AFS_PTHREAD_ENV
    pthread_cond_signal(&workerCV);
#else
    LWP_NoYieldSignal(&workers);
#endif
    return code;
}

#define MAX_CTHREADS 25

static long
CallSimultaneously(threads, rock, proc)
     IN u_int threads;
     IN opaque rock;
     IN long (*proc) ();
{
    long code;
    int i;
#ifdef AFS_PTHREAD_ENV
    pthread_once(&workerOnce, WorkerInit);
#endif

    workers = 0;
    for (i = 0; i < threads; i++) {
	struct worker *w;
#ifdef AFS_PTHREAD_ENV
	pthread_t pid;
#else
	PROCESS pid;
#endif
	assert(i < MAX_CTHREADS);
	w = (struct worker *)osi_Alloc(sizeof(struct worker));
	memset(w, 0, sizeof(*w));
	w->next = workers;
	workers = w;
	w->index = i;
	w->exitCode = RXKST_PROCESSRUNNING;
	w->rock = rock;
	w->proc = proc;
#ifdef AFS_PTHREAD_ENV
	{
	    pthread_attr_t tattr;

	    code = pthread_attr_init(&tattr);
	    if (code) {
		afs_com_err(whoami, code,
			"can't pthread_attr_init worker process");
		return code;
	    }

	    code =
		pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	    if (code) {
		afs_com_err(whoami, code,
			"can't pthread_attr_setdetachstate worker process");
		return code;
	    }

	    code = pthread_create(&pid, &tattr, DoWorker, (void *)w);
	}
#else
	code =
	    LWP_CreateProcess(DoWorker, 16000, LWP_NORMAL_PRIORITY,
			      (opaque) w, "Worker Process", &pid);
#endif
	if (code) {
	    afs_com_err(whoami, code, "can't create worker process");
	    return code;
	}
    }
    code = 0;			/* last non-zero code encountered */
#ifdef AFS_PTHREAD_ENV
    pthread_mutex_lock(&workerLock);
#endif
    while (workers) {
	struct worker *w, *prevW, *nextW;
	prevW = 0;
	for (w = workers; w; w = nextW) {
	    nextW = w->next;
	    if (w->exitCode != RXKST_PROCESSRUNNING) {
		if (w->exitCode) {
		    if (code == 0)
			code = w->exitCode;
		}
		if (prevW)
		    prevW->next = w->next;
		else
		    workers = w->next;
		osi_Free(w, sizeof(*w));
		continue;	/* don't bump prevW */
	    }
	    prevW = w;
	}
#ifdef AFS_PTHREAD_ENV
	if (workers)
	    pthread_cond_wait(&workerCV, &workerLock);
#else
	if (workers)
	    LWP_WaitProcess(&workers);
#endif
    }
#ifdef AFS_PTHREAD_ENV
    pthread_mutex_unlock(&workerLock);
#endif
    return code;
}

static void
DivideUpCalls(calls, threads, threadCalls)
     IN u_long calls;
     IN u_int threads;
     IN u_long threadCalls[];
{
    int i;
    for (i = 0; i < threads; i++) {
	threadCalls[i] = calls / (threads - i);
	calls -= threadCalls[i];
    }
}

static double
ftime()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static long
RunLoadTest(parms, conn)
     IN struct clientParms *parms;
     IN struct rx_connection *conn;
{
    long code;
    struct client c;
    u_long fastCalls[MAX_CTHREADS];
    u_long slowCalls[MAX_CTHREADS];
    u_long copiousCalls[MAX_CTHREADS];
    double start, interval;

    DivideUpCalls(parms->fastCalls, parms->threads, fastCalls);
    DivideUpCalls(parms->slowCalls, parms->threads, slowCalls);
    DivideUpCalls(parms->copiousCalls, parms->threads, copiousCalls);

    memset(&c, 0, sizeof(c));
    c.conn = conn;
    c.sendLen = parms->sendLen;
    c.recvLen = parms->recvLen;
    c.fastCalls = fastCalls;
    c.slowCalls = slowCalls;
    c.copiousCalls = copiousCalls;

    start = ftime();
    code = CallSimultaneously(parms->threads, &c, DoClient);
    if (code) {
	afs_com_err(whoami, code, "in DoClient");
	return code;
    }
    interval = ftime() - start;

    if (parms->printTiming) {
	u_long totalCalls =
	    parms->fastCalls + parms->slowCalls + parms->copiousCalls;
	int t = (interval / totalCalls) * 1000.0 + 0.5;
	if (totalCalls > 0) {
	    printf("For %d calls: %d msec/call\n", totalCalls, t);
	}
	if (parms->copiousCalls > 0) {
	    long n = parms->sendLen + parms->recvLen;
	    double kbps;
	    int b;
	    kbps = (double)(parms->copiousCalls * n) / (interval * 1000.0);
	    b = kbps + 0.5;
#if 0
	    I just cannot get printing of floats to work on the pmax !
		!!!printf("%g %d %d %d\n", (float)kbps, b);
	    printf("%g %d %d %d\n", kbps, b);
	    fprintf(stdout, "%g %d %d\n", kbps, b);
	    {
		char buf[100];
		buf[sizeof(buf) - 1] = 0;
		sprintf(buf, "%g %d %d\n", kbps, b);
		assert(buf[sizeof(buf) - 1] == 0);
		printf("%s", buf);
	    }
#endif
	    printf
		("For %d copious calls, %d send + %d recv = %d bytes each: %d kbytes/sec\n",
		 parms->copiousCalls, parms->sendLen, parms->recvLen, n, b);
#if 0
	    printf("%g\n", kbps);
#endif
	}
    }
    return 0;
}

static long
RepeatLoadTest(parms, conn)
     IN struct clientParms *parms;
     IN struct rx_connection *conn;
{
    long code;
    long count;

    if (parms->repeatInterval == 0) {
	if (parms->repeatCount == 0)
	    parms->repeatCount = 1;
    } else {
	if (parms->repeatCount == 0)
	    parms->repeatCount = 0x7fffffff;
    }

    if (parms->printTiming) {
	int types;
	types =
	    (parms->fastCalls ? 1 : 0) + (parms->slowCalls ? 1 : 0) +
	    (parms->copiousCalls ? 1 : 0);
	if (types > 1)
	    fprintf(stderr,
		    "Combined timings of several types of calls may not be meaningful.\n");
	if (types == 0)
	    /* do timings of copious calls by default */
	    parms->copiousCalls = 10;
    }

    for (count = 0; count < parms->repeatCount; count++) {
	code = RunLoadTest(parms, conn);
	if (code)
	    return code;
	if (parms->repeatInterval) {
	    u_long i = parms->repeatInterval;
	    u_long now = time(0);
	    u_long next = (now + i - 1) / i * i;	/* round up to next interval */
	    while (now < next) {
#ifdef AFS_PTHREAD_ENV
		sleep(next - now);
#else
		IOMGR_Sleep(next - now);
#endif
		now = time(0);
	    }
	}
    }
    return code;
}

/* For backward compatibility, don't try to use the CallNumber stuff unless
 * we're compiling against the new Rx. */

#ifdef rx_GetPacketCksum

struct multiChannel {
    struct rx_connection *conn;
    int done;
    long *codes;
    int changes[RX_MAXCALLS];
    long callNumbers[RX_MAXCALLS];
};
#define BIG_PRIME 1257056893	/* 0x4AED2A7d */
static u_long sequence = 0;

static long
FastCall(conn)
     IN struct rx_connection *conn;
{
    long code;
    u_long n = (sequence = sequence * BIG_PRIME + BIG_PRIME);
    u_long inc_n;

    code = RXKST_Fast(conn, n, &inc_n);
    if (code)
	return code;
    if (inc_n != n + 1)
	return RXKST_INCFAILED;
    return 0;
}

static long
UniChannelCall(index, rock)
     IN u_int index;
     IN opaque rock;
{
    struct multiChannel *mc = (struct multiChannel *)rock;
    long code;
    long callNumbers[RX_MAXCALLS];
    int unchanged;

    code = 0;
    unchanged = 1;
    while (!mc->done && unchanged) {
	int i;
	code = FastCall(mc->conn);
	if (code)
	    break;
	code = rxi_GetCallNumberVector(mc->conn, callNumbers);
	if (code)
	    break;
	unchanged = 0;
	for (i = 0; i < RX_MAXCALLS; i++) {
	    if (callNumbers[i] > mc->callNumbers[i]) {
		mc->callNumbers[i] = callNumbers[i];
		mc->changes[i]--;	/* may go negative */
	    }
	    if (mc->changes[i] > 0)
		unchanged++;
	}
    }
    mc->codes[index] = code;
    mc->done++;
    return code;
}

static long
MakeMultiChannelCall(conn, each, expectedCode, codes)
     IN struct rx_connection *conn;
     IN int each;		/* calls to make on each channel */
     IN long expectedCode;
     OUT long codes[];
{
    long code;
    int i;
    struct multiChannel mc;

    memset(&mc, 0, sizeof(mc));
    mc.conn = conn;
    for (i = 0; i < RX_MAXCALLS; i++) {
	codes[i] = RXKST_PROCESSRUNNING;
	mc.changes[i] = each;
    }
    mc.codes = codes;
    code = rxi_GetCallNumberVector(conn, mc.callNumbers);
    if (code)
	return code;
    mc.done = 0;
    code = CallSimultaneously(RX_MAXCALLS, &mc, UniChannelCall);
    if (((expectedCode == RXKST_INCFAILED) || (expectedCode == -1)) && ((code == expectedCode) || (code == -3)));	/* strange cases */
    else if (code != expectedCode) {
	afs_com_err(whoami, code,
		"problem making multichannel call, expected '%s'",
		((expectedCode == 0)
		 ? "no error" : (char *)afs_error_message(expectedCode)));
    }
    return code;
}

static long
CheckCallFailure(conn, codes, code, msg)
     IN struct rx_connection *conn;
     IN long codes[];
     IN long code;
     IN char *msg;
{
    if (code == 0) {
	fprintf(stderr, "Failed to detect %s\n", msg);
	return RXKST_NODUPLICATECALL;
    } else {
	int i;
	int okay = 1;
	int someZero = 0;
	for (i = 0; i < RX_MAXCALLS; i++)
	    if (!((codes[i] == 0) || (codes[i] == code) || (codes[i] == -3)))
		okay = 0;
	if (conn->error)
	    okay = 0;
	if (!okay) {
	    fprintf(stderr, "%s produced these errors:\n", msg);
	    for (i = 0; i < RX_MAXCALLS; i++) {
		assert(codes[i] != RXKST_PROCESSRUNNING);
		if (codes[i] == 0) {
		    someZero++;
		    fprintf(stderr, "  %d no error\n", i);
		} else
		    fprintf(stderr, "  %d %s\n", i, afs_error_message(codes[i]));
	    }
	    if (someZero) {
		char buf[100];
		sprintf(buf, "connection dead following %s", msg);
		code = FastCall(conn);
		if (code)
		    afs_com_err(whoami, code, buf);
	    }
	}
    }
    return 0;
}

#endif /* rx_GetPacketCksum */

static long
RunCallTest(parms, host, sc, si)
     IN struct clientParms *parms;
     IN long host;
     IN struct rx_securityClass *sc;
     IN long si;
{
    long code;

#ifndef rx_GetPacketCksum

    code = RXKST_BADARGS;
    afs_com_err(whoami, code,
	    "Older versions of Rx don't support Get/Set callNumber Vector procedures: can't run this CallTest");
    return code;

#else

    int i, ch;
    struct rx_connection *conn;
    long firstCall;
    long callNumbers[RX_MAXCALLS];
    long codes[RX_MAXCALLS];
    long retCode = 0;		/* ret. if nothing fatal goes wrong */

    conn =
	rx_NewConnection(host, htons(RXKST_SERVICEPORT), RXKST_SERVICEID, sc,
			 si);
    if (!conn)
	return RXKST_NEWCONNFAILED;

    /* First check the basic behaviour of call number handling */

    code = rxi_GetCallNumberVector(conn, callNumbers);
    if (code)
	return code;
    for (i = 0; i < RX_MAXCALLS; i++) {
	if (callNumbers[i] != 0) {
	    fprintf(stderr,
		    "Connection's initial call numbers not zero. call[%d] = %d\n",
		    i, callNumbers[i]);
	    return RXKST_BADCALLNUMBERS;
	}
    }
    code = FastCall(conn);
    if (code)
	return code;
    code = rxi_GetCallNumberVector(conn, callNumbers);
    if (code)
	return code;
    firstCall = callNumbers[0];
    code = FastCall(conn);
    if (code)
	return code;
    code = rxi_GetCallNumberVector(conn, callNumbers);
    if (code)
	return code;
    if ((callNumbers[0] != firstCall + 1)
	&& ((firstCall == 1) || (firstCall == 2))) {
	/* The call number after the first call should be one or, more likely,
	 * two (if the call is still DALLYing).  Between first and second call,
	 * the call number should have incremented by one. */
	fprintf(stderr,
		"Connection's first channel call number not one. call[%d] = %d\n",
		0, callNumbers[0]);
	return RXKST_BADCALLNUMBERS;
    }
    for (i = 1; i < RX_MAXCALLS; i++) {
	if (callNumbers[i] != 0) {
	    fprintf(stderr,
		    "Connection's other channel call numbers not zero. call[%d] = %d\n",
		    i, callNumbers[i]);
	    return RXKST_BADCALLNUMBERS;
	}
    }
    code = MakeMultiChannelCall(conn, 1, 0, codes);
    if (code)
	return code;

    /* Now try to resend a call that's already been executed by finding a
     * non-zero call number on a channel other than zero and decrementing it by
     * one.  This should appear to the server as a retransmitted call.  Since
     * this is behaving as a broken client different strange behaviors may be
     * exhibited by different servers.  If the response packet to the original
     * call is discarded by the time the "retransmitted" call arrives (perhaps
     * due to high server or client load) there is no way for the server to
     * respond at all.  Further, it seems, that under some cases the connection
     * will be kept alive indefinitely even though the server has discarded the
     * "retransmitted" call and is making no effort to reexecute the call.  To
     * handle these, accept either a timeout (-1) or and INCFAILED error here,
     * also set the connenction HardDeadTime to punt after a reasonable
     * interval. */

    /* short dead time since may we expect some trouble */
    rx_SetConnHardDeadTime(conn, 30);
    code = rxi_GetCallNumberVector(conn, callNumbers);
    if (code)
	return code;
    for (ch = 1; ch < RX_MAXCALLS; ch++)
	if (callNumbers[ch] > 1) {
	    callNumbers[ch]--;
	    code = rxi_SetCallNumberVector(conn, callNumbers);
	    if (code)
		return code;
	    break;
	}
    if (ch >= RX_MAXCALLS)	/* didn't find any? all DALLYing? */
	return RXKST_BADCALLNUMBERS;
    code = MakeMultiChannelCall(conn, 1, RXKST_INCFAILED, codes);
    code = CheckCallFailure(conn, codes, code, "retransmitted call");
    if (code && !retCode)
	retCode = code;

    /* Get a fresh connection, becasue if the above failed as it should the
     * connection is dead. */
    rx_DestroyConnection(conn);
    conn =
	rx_NewConnection(host, htons(RXKST_SERVICEPORT), RXKST_SERVICEID, sc,
			 si);
    if (!conn)
	return RXKST_NEWCONNFAILED;

    /* Similarly, but decrement call number by two which should be completely
     * unmistakeable as a broken or malicious client. */

    /* short dead time since may we expect some trouble */
    rx_SetConnHardDeadTime(conn, 30);
    code = MakeMultiChannelCall(conn, 2, 0, codes);
    if (code)
	return code;
    code = rxi_GetCallNumberVector(conn, callNumbers);
    if (code)
	return code;
    for (ch = 1; ch < RX_MAXCALLS; ch++)
	if (callNumbers[ch] > 2) {
	    callNumbers[ch] -= 2;
	    code = rxi_SetCallNumberVector(conn, callNumbers);
	    break;
	}
    if (ch >= RX_MAXCALLS)	/* didn't find any? all DALLYing? */
	return RXKST_BADCALLNUMBERS;
    code = MakeMultiChannelCall(conn, 1, -1, codes);
    code = CheckCallFailure(conn, codes, code, "duplicate call");
    if (code && !retCode)
	retCode = code;

    rx_DestroyConnection(conn);
    conn =
	rx_NewConnection(host, htons(RXKST_SERVICEPORT), RXKST_SERVICEID, sc,
			 si);
    if (!conn)
	return RXKST_NEWCONNFAILED;

    /* Next, without waiting for the server to discard its state, we will check
     * to see if the Challenge/Response protocol correctly informs the server
     * of the client's callNumber state.  We do this by artificially increasing
     * the call numbers of a new connection for all channels beyond zero,
     * making a call on channel zero, then resetting the call number for the
     * unused channels back to zero, then making calls on all channels. */

    code = rxi_GetCallNumberVector(conn, callNumbers);
    if (code)
	return code;
    for (i = 0; i < RX_MAXCALLS; i++) {
	if (callNumbers[i] != 0)
	    return RXKST_BADCALLNUMBERS;
	callNumbers[i] = 51;	/* an arbitrary value... */
    }
    code = rxi_SetCallNumberVector(conn, callNumbers);
    if (code)
	return code;
    code = FastCall(conn);	/* use channel 0 */
    if (code)
	return code;
    code = rxi_GetCallNumberVector(conn, callNumbers);
    if (code)
	return code;
    if (callNumbers[0] != 52)
	return RXKST_BADCALLNUMBERS;
    for (i = 1; i < RX_MAXCALLS; i++) {
	if (callNumbers[i] != 51)
	    return RXKST_BADCALLNUMBERS;
	callNumbers[i] = 37;	/* back up a ways */
    }
    code = rxi_SetCallNumberVector(conn, callNumbers);
    if (code)
	return code;
    /* now try calls on all channels... */
    code = MakeMultiChannelCall(conn, 1, -1, codes);
    code =
	CheckCallFailure(conn, codes, code, "alternate channel call replay");
    if (code && !retCode)
	retCode = code;

    rx_DestroyConnection(conn);
    return retCode;

#endif /* rx_GetPacketCksum */

}

#ifdef rx_GetPacketCksum

static struct {
    int op;
    u_long epoch;		/* connection to attack */
    u_long cid;
    int client;			/* TRUE => client side */
    u_long newEpoch;		/* conn to direct challenges to */
    u_long newCid;
    u_long counts[RX_N_PACKET_TYPES];
} incomingOps;
#define IO_NOOP			0
#define IO_COUNT		1
#define IO_REDIRECTCHALLENGE	2

static int
HandleIncoming(p, addr)
     INOUT struct rx_packet *p;
     INOUT struct sockaddr_in *addr;
{
    int client;			/* packet sent by client */
    u_char type;		/* packet type */

    if (incomingOps.op == IO_NOOP)
	return 0;

    client = ((p->header.flags & RX_CLIENT_INITIATED) != RX_CLIENT_INITIATED);
    if ((p->header.epoch != incomingOps.epoch)
	|| ((p->header.cid ^ incomingOps.cid) & RX_CIDMASK)
	|| (client != incomingOps.client))
	return 0;
    type = p->header.type;
    if ((type <= 0) || (type >= RX_N_PACKET_TYPES))
	type = 0;
    incomingOps.counts[type]++;

    switch (incomingOps.op) {
    case IO_NOOP:
    case IO_COUNT:
	break;

    case IO_REDIRECTCHALLENGE:
	if (p->header.type != RX_PACKET_TYPE_CHALLENGE)
	    break;
	p->header.epoch = incomingOps.newEpoch;
	p->header.cid = incomingOps.newCid;
	/* Now set up to watch for the corresponding challenge. */
	incomingOps.epoch = incomingOps.newEpoch;
	incomingOps.cid = incomingOps.newCid;
	incomingOps.op = IO_COUNT;
	break;

    default:
	fprintf(stderr, "Unknown incoming op %d\n", incomingOps.op);
	break;
    }
    return 0;
}

static struct {
    int op;
    u_long epoch;		/* connection to attack */
    u_long cid;
    int client;			/* TRUE => client side */
    u_long counts[RX_N_PACKET_TYPES];
} outgoingOps;
#define OO_NOOP		0
#define OO_COUNT	1
#define OO_ZEROCKSUM	2
#define OO_MUNGCKSUM	3

static int
HandleOutgoing(p, addr)
     INOUT struct rx_packet *p;
     INOUT struct sockaddr_in *addr;
{
    int client;			/* packet sent by client */
    u_char type;		/* packet type */

    if (outgoingOps.op == OO_NOOP)
	return 0;

    client = ((p->header.flags & RX_CLIENT_INITIATED) == RX_CLIENT_INITIATED);
    if ((p->header.epoch != outgoingOps.epoch)
	|| ((p->header.cid ^ outgoingOps.cid) & RX_CIDMASK)
	|| (client != outgoingOps.client))
	return 0;
    type = p->header.type;
    if ((type <= 0) || (type >= RX_N_PACKET_TYPES))
	type = 0;
    outgoingOps.counts[type]++;

    switch (outgoingOps.op) {
    case OO_NOOP:
    case OO_COUNT:
	/* counting always happens above if not noop */
	break;

    case OO_ZEROCKSUM:
	if (p->header.type != RX_PACKET_TYPE_DATA)
	    break;
	if (rx_GetPacketCksum(p) == 0) {
	    /* probably, a retransmitted packet */
	    fprintf(stderr, "Packet cksum already zero\n");
	    break;
	}
	rx_SetPacketCksum(p, 0);
	break;

    case OO_MUNGCKSUM:{
	    u_short cksum;
	    if (p->header.type != RX_PACKET_TYPE_DATA)
		break;
	    cksum = rx_GetPacketCksum(p);
	    if (cksum == 0) {
		fprintf(stderr, "Packet cksum already zero\n");
		break;
	    }
	    rx_SetPacketCksum(p, cksum ^ 8);
	    break;
	}
    default:
	fprintf(stderr, "Unknown outgoing op %d\n", outgoingOps.op);
	break;
    }
    return 0;
}

#ifdef AFS_PTHREAD_ENV
static pthread_once_t slowCallOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t slowCallLock;
static pthread_cond_t slowCallCV;
void
SlowCallInit(void)
{
    pthread_mutex_init(&slowCallLock, NULL);
    pthread_cond_init(&slowCallCV, NULL);
}
#endif
static long slowCallCode;
static long
SlowCall(conn)
     IN opaque conn;
{
    u_long ntime;
    u_long now;
    long temp_rc;

#ifdef AFS_PTHREAD_ENV
    pthread_mutex_lock(&slowCallLock);
#endif
    slowCallCode = RXKST_PROCESSRUNNING;
#ifdef AFS_PTHREAD_ENV
    pthread_cond_signal(&slowCallCV);
#else
    LWP_NoYieldSignal(&slowCallCode);
#endif
    slowCallCode = RXKST_Slow(conn, 1, &ntime);
    if (!slowCallCode) {
	now = FT_ApproxTime();
	if ((ntime < now - maxSkew) || (ntime > now + maxSkew))
	    slowCallCode = RXKST_TIMESKEW;
    }
    temp_rc = slowCallCode;
#ifdef AFS_PTHREAD_ENV
    pthread_cond_signal(&slowCallCV);
    pthread_mutex_unlock(&slowCallLock);
#else
    LWP_NoYieldSignal(&slowCallCode);
#endif
    return temp_rc;
}

#endif /* rx_GetPacketCksum */

static long
RunHijackTest(parms, host, sc, si)
     IN struct clientParms *parms;
     IN long host;
     IN struct rx_securityClass *sc;
     IN long si;
{

#ifndef rx_GetPacketCksum

    code = RXKST_BADARGS;
    afs_com_err(whoami, code,
	    "Older versions of Rx don't export packet tracing routines: can't run this HijackTest");
    return code;

#else

    extern int (*rx_justReceived) ();
    extern int (*rx_almostSent) ();

    long code;
    struct rx_connection *conn = 0;
    struct rx_connection *otherConn = 0;
#ifdef AFS_PTHREAD_ENV
    pthread_t pid;
#else
    PROCESS pid;
#endif
    int nResp;			/* otherConn responses seen */
    long tmp_rc;

#ifdef AFS_PTHREAD_ENV
    pthread_once(&slowCallOnce, SlowCallInit);
#endif
    rx_justReceived = HandleIncoming;
    rx_almostSent = HandleOutgoing;

    incomingOps.op = IO_NOOP;
    outgoingOps.op = OO_NOOP;

#define HIJACK_CONN(conn) \
    {   if (conn) rx_DestroyConnection (conn); \
	(conn) = rx_NewConnection(host, htons(RXKST_SERVICEPORT), \
				RXKST_SERVICEID, sc, si); \
	if (!(conn)) return RXKST_NEWCONNFAILED; \
	outgoingOps.client = 1; \
	outgoingOps.epoch = (conn)->epoch; \
	outgoingOps.cid = (conn)->cid; }

    HIJACK_CONN(conn);

    /* First try switching from no packet cksum to sending packet cksum between
     * calls, and see if server complains. */

    outgoingOps.op = OO_ZEROCKSUM;
    code = FastCall(conn);
    if (code) {
	afs_com_err(whoami, code, "doing FastCall with ZEROCKSUM");
	return code;
    }
    /* The server thinks we're an old style client.  Now start sending cksums.
     * Server shouldn't care. */
    outgoingOps.op = OO_NOOP;
    code = FastCall(conn);
    if (code) {
	afs_com_err(whoami, code, "doing FastCall with non-ZEROCKSUM");
	return code;
    }
    /* The server now thinks we're a new style client, we can't go back now. */
    outgoingOps.op = OO_ZEROCKSUM;
    code = FastCall(conn);
    if (code == 0)
	code = RXKST_NOBADCKSUM;
    if (code != RXKADSEALEDINCON) {
	afs_com_err(whoami, code, "doing FastCall with ZEROCKSUM");
	return code;
    } else if (!conn->error) {
	code = RXKST_NOCONNERROR;
	afs_com_err(whoami, code, "doing FastCall with ZEROCKSUM");
	return code;
    } else
	code = 0;

    HIJACK_CONN(conn);

    /* Now try modifying packet cksum to see if server complains. */

    outgoingOps.op = OO_MUNGCKSUM;
    code = FastCall(conn);
    if (code == 0)
	code = RXKST_NOBADCKSUM;
    if (code != RXKADSEALEDINCON) {
	afs_com_err(whoami, code, "doing FastCall with ZEROCKSUM");
	return code;
    } else if (!conn->error) {
	code = RXKST_NOCONNERROR;
	afs_com_err(whoami, code, "doing FastCall with ZEROCKSUM");
	return code;
    } else
	code = 0;

    /* Now make two connection and direct the first challenge on one connection
     * to the other connection to see if it generates a response.  The
     * retransmitted challenge should allow the call on the first connection to
     * complete correctly.  Part one is to attack a new connection, then attack
     * it after it has made a call.  Part three, just for comparison, attacks a
     * otherConn while it is making a slow call (and thus has an active call).
     * Against this attack we have no defense so we expect a challenge in this
     * case, which the server will discard. */

#define RedirectChallenge(conn,otherConn)	\
    (incomingOps.epoch = (conn)->epoch,		\
     incomingOps.cid = (conn)->cid,		\
     incomingOps.client = 1,			\
     incomingOps.newEpoch = (otherConn)->epoch,	\
     incomingOps.newCid = (otherConn)->cid,	\
     incomingOps.op = IO_REDIRECTCHALLENGE,	\
     outgoingOps.epoch = (otherConn)->epoch,	\
     outgoingOps.cid = (otherConn)->cid,	\
     outgoingOps.client = 1,			\
     outgoingOps.op = OO_COUNT,			\
     outgoingOps.counts[RX_PACKET_TYPE_RESPONSE] = 0)

    HIJACK_CONN(conn);
    HIJACK_CONN(otherConn)
	RedirectChallenge(conn, otherConn);

    code = FastCall(conn);
    if (code)
	return code;
    assert(incomingOps.op == IO_COUNT);	/* redirect code was triggered */
    if (outgoingOps.counts[RX_PACKET_TYPE_RESPONSE] > 0) {
      oracle:
	code = RXKST_CHALLENGEORACLE;
	afs_com_err(whoami, code, "misdirecting challenge");
	return code;
    }
    code = FastCall(otherConn);	/* generate some activity here */
    if (code)
	return code;
    nResp = outgoingOps.counts[RX_PACKET_TYPE_RESPONSE];
    assert(nResp >= 1);
    code = FastCall(conn);
    if (code)
	return code;
    if (outgoingOps.counts[RX_PACKET_TYPE_RESPONSE] > nResp)
	goto oracle;

    HIJACK_CONN(conn);
    RedirectChallenge(conn, otherConn);
    /* otherConn was authenticated during part one */
    code = FastCall(conn);
    if (code)
	return code;
    assert(incomingOps.op == IO_COUNT);	/* redirect code was triggered */
    if (outgoingOps.counts[RX_PACKET_TYPE_RESPONSE] != 0)
	goto oracle;

    HIJACK_CONN(conn);
    RedirectChallenge(conn, otherConn);
    /* otherConn is still authenticated */
    slowCallCode = RXKST_PROCESSCREATED;
#ifdef AFS_PTHREAD_ENV
    {
	pthread_attr_t tattr;

	code = pthread_attr_init(&tattr);
	if (code) {
	    afs_com_err(whoami, code,
		    "can't pthread_attr_init slow call process");
	    return code;
	}

	code = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	if (code) {
	    afs_com_err(whoami, code,
		    "can't pthread_attr_setdetachstate slow call process");
	    return code;
	}

	code = pthread_create(&pid, &tattr, SlowCall, (void *)otherConn);
    }
#else
    code =
	LWP_CreateProcess(SlowCall, 16000, LWP_NORMAL_PRIORITY,
			  (opaque) otherConn, "Slow Call Process", &pid);
#endif
    if (code) {
	afs_com_err(whoami, code, "can't create slow call process");
	return code;
    }
#ifdef AFS_PTHREAD_ENV
    pthread_mutex_lock(&slowCallLock);
    while (slowCallCode == RXKST_PROCESSCREATED)
	pthread_cond_wait(&slowCallCV, &slowCallLock);
#else
    while (slowCallCode == RXKST_PROCESSCREATED)
	LWP_WaitProcess(&slowCallCode);	/* wait for process start */
#endif
    if (slowCallCode != RXKST_PROCESSRUNNING) {
	tmp_rc = slowCallCode;
#ifdef AFS_PTHREAD_ENV
	pthread_mutex_unlock(&slowCallLock);
#endif
	return tmp_rc;		/* make sure didn't fail immediately */
    }
    assert(incomingOps.op == IO_REDIRECTCHALLENGE);
    code = FastCall(conn);
    if (code)
	return code;
    assert(incomingOps.op == IO_COUNT);	/* redirect code was triggered */
#ifdef AFS_PTHREAD_ENV
    while (slowCallCode == RXKST_PROCESSRUNNING)
	pthread_cond_wait(&slowCallCV, &slowCallLock);
    pthread_mutex_unlock(&slowCallLock);
#else
    while (slowCallCode == RXKST_PROCESSRUNNING)
	LWP_WaitProcess(&slowCallCode);	/* wait for process finish */
#endif
    if (outgoingOps.counts[RX_PACKET_TYPE_RESPONSE] != 1)
	goto oracle;

    rx_justReceived = 0;
    rx_almostSent = 0;
    rx_DestroyConnection(otherConn);
    rx_DestroyConnection(conn);
    return code;

#endif /* rx_GetPacketCksum */

}

long
rxkst_StartClient(parms)
     IN struct clientParms *parms;
{
    long code;
    long host;
    long scIndex;
    struct rx_securityClass *sc;

    whoami = parms->whoami;	/* set this global variable */

    host = GetServer(parms->server);

    if (parms->authentication >= 0) {
	long kvno;
	char ticket[MAXKTCTICKETLEN];
	int ticketLen;
	struct ktc_encryptionKey Ksession;

	if (parms->useTokens)
	    code =
		GetToken(&kvno, &Ksession, &ticketLen, ticket, parms->cell);
	else
	    code =
		GetTicket(&kvno, &Ksession, &ticketLen, ticket, parms->cell);
	if (code)
	    return code;

	/* next, we have ticket, kvno and session key, authenticate the conn */
	sc = (struct rx_securityClass *)
	    rxkad_NewClientSecurityObject(parms->authentication, &Ksession,
					  kvno, ticketLen, ticket);
	assert(sc);
	scIndex = 2;		/* kerberos security index */
    } else {
	/* unauthenticated connection */
	sc = rxnull_NewClientSecurityObject();
	assert(sc);
	scIndex = 0;		/* null security index */
    }

    code = 0;
    if (!code && parms->callTest) {
	code = RunCallTest(parms, host, sc, scIndex);
    }
    if (!code && parms->hijackTest) {
	code = RunHijackTest(parms, host, sc, scIndex);
    }
    if (!code
	&& (parms->printTiming || parms->fastCalls || parms->slowCalls
	    || parms->copiousCalls)) {
	struct rx_connection *conn;
	conn =
	    rx_NewConnection(host, htons(RXKST_SERVICEPORT), RXKST_SERVICEID,
			     sc, scIndex);
	if (conn) {
	    code = RepeatLoadTest(parms, conn);
	    rx_DestroyConnection(conn);
	} else
	    code = RXKST_NEWCONNFAILED;
    }
    if (!code && parms->stopServer) {
	struct rx_connection *conn;
	conn =
	    rx_NewConnection(host, htons(RXKST_SERVICEPORT), RXKST_SERVICEID,
			     sc, scIndex);
	if (conn) {
	    code = RXKST_Kill(conn);
	    if (code) {
		afs_com_err(whoami, code, "trying to stop server");
	    }
	    rx_DestroyConnection(conn);
	} else
	    code = RXKST_NEWCONNFAILED;
    }

    if (parms->printStats) {
	rx_PrintStats(stdout);
#if 0
	/* use rxdebug style iteration here */
	rx_PrintPeerStats(stdout, rx_PeerOf(conn));
#endif
    }

    rxs_Release(sc);
    rx_Finalize();
    if (code) {
	afs_com_err(parms->whoami, code, "test fails");
	exit(13);
    } else {
	printf("Test Okay\n");
	if (!parms->noExit)
	    exit(0);
    }
    return 0;
}
