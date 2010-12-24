/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* RX Authentication Stress test: server side code. */

#include <afsconfig.h>
#include <afs/param.h>


#include <afs/stds.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <rx/rx_null.h>

#include <rx/rxkad.h>

#include <afs/keys.h>
#include <afs/cellconfig.h>

#include "stress.h"
#include "stress_internal.h"

struct ktc_encryptionKey serviceKey =
    { { 0x45, 0xe3, 0x3d, 0x16, 0x29, 0x64, 0x8a, 0x8f } };
long serviceKeyVersion = 7;

static int
GetKey(void *rock, int kvno, struct ktc_encryptionKey *key)
{
    struct serverParms *parms = (struct serverParms *)rock;
    struct afsconf_keys tstr;
    afs_int32 code;
    int fd;

    fprintf(stderr, "GetKey called for kvno %d\n", kvno);
    if (!parms->keyfile) {
	memcpy(key, &serviceKey, sizeof(*key));
	return 0;
    }

    /* the rest of this function borrows heavily from auth/cellconfig.c */
    fd = open(parms->keyfile, O_RDONLY);
    if (fd < 0) {
	return AFSCONF_FAILURE;
    }
    code = read(fd, &tstr, sizeof(struct afsconf_keys));
    close(fd);
    if (code < sizeof(afs_int32)) {
	return AFSCONF_FAILURE;
    }

    /* convert key structure to host order */
    tstr.nkeys = ntohl(tstr.nkeys);
    for (fd = 0; fd < tstr.nkeys; fd++) {
	if (kvno == ntohl(tstr.key[fd].kvno)) {
	    memcpy(key, tstr.key[fd].key, sizeof(*key));
	    return 0;
	}
    }

    return AFSCONF_NOTFOUND;
}

static int minAuth;

void *
rxkst_StartServer(void * rock)
{
    struct serverParms *parms = rock;
    extern int rx_stackSize;
    struct rx_service *tservice;
    struct rx_securityClass *sc[3];
    rxkad_level minLevel;

    minAuth = parms->authentication;
    if (minAuth == -1)
	minLevel = rxkad_clear;
    else
	minLevel = minAuth;

    sc[0] = rxnull_NewServerSecurityObject();
    sc[1] = 0;			/* no rxvab anymore */
    sc[2] = rxkad_NewServerSecurityObject(minLevel, (void *)parms, GetKey, 0);
    tservice =
	rx_NewService(htons(RXKST_SERVICEPORT), RXKST_SERVICEID,
		      "stress test", sc, 3, RXKST_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	fprintf(stderr, "Could not create stress test rx service\n");
	exit(3);
    }
    rx_SetMinProcs(tservice, parms->threads);
    rx_SetMaxProcs(tservice, parms->threads);
    rx_SetStackSize(tservice, 10000);

    rx_StartServer( /*donate me */ 1);	/* start handling req. of all types */
    /* never reached */
    return 0;
}

static char test_client_name[MAXKTCNAMELEN];
static char test_client_inst[MAXKTCNAMELEN];
static char test_client_cell[MAXKTCREALMLEN];
static int got_client_id = 0;
static long
CheckAuth(struct rx_call *call)
{
    long code;
    int si;
    rxkad_level level;
    char name[MAXKTCNAMELEN];
    char inst[MAXKTCNAMELEN];
    char cell[MAXKTCREALMLEN];
    int kvno;
    unsigned int expiration;		/* checked by Security Module */

    si = rx_SecurityClassOf(rx_ConnectionOf(call));
    if (si == 1) {
	printf("No support for VAB security module.\n");
	return -1;
    } else if (si == 0) {
	if (minAuth > -1)
	    return RXKST_UNAUTH;
	else
	    return 0;
    } else if (si != 2) {
	fprintf(stderr, "Unknown security index %d\n", si);
	return -1;
    }

    code =
	rxkad_GetServerInfo(rx_ConnectionOf(call), &level, &expiration, name,
			    inst, cell, &kvno);
    if (code)
	return code;
    if (minAuth > level)
	return -1;
    fprintf(stderr, "Test client is %s.%s@%s\n", name, inst, cell);
    if (got_client_id) {
	if (strcmp(name, test_client_name))
	    return RXKST_BADCLIENT;
	if (strcmp(inst, test_client_inst))
	    return RXKST_BADCLIENT;
	if (strcmp(cell, test_client_cell))
	    return RXKST_BADCLIENT;
    } else {
	strcpy(test_client_name, name);
	strcpy(test_client_inst, inst);
	strcpy(test_client_cell, cell);
	got_client_id = 1;
    }
    return 0;
}

/* Stop the server.  There isn't a graceful way to do this so just exit. */

afs_int32
SRXKST_Kill(struct rx_call *call)
{
    long code;
    code = CheckAuth(call);
    if (code)
	return code;

    /* This is tricky, but since we're never going to return, we end the call
     * here, then rx_Finalize should push out the response/ack. */
    rx_EndCall(call, 0);
    rx_Finalize();

    printf("Server halted by RPC request.\n");
    exit(0);
    return 0;
}

afs_int32
SRXKST_Fast(struct rx_call *call, u_long n, u_long *inc_nP)
{
    *inc_nP = n + 1;
    return 0;
}

afs_int32
SRXKST_Slow(struct rx_call *call, u_long tag, u_long *nowP)
{
    long code;
    time_t now;
    code = CheckAuth(call);
    if (code)
	return code;
#ifdef AFS_PTHREAD_ENV
    sleep(1);
#else
    IOMGR_Sleep(1);
#endif
    time(&now);
    *nowP = now;
    return 0;
}

#define COPBUFSIZE 10000
static struct buflist {
    struct buflist *next;
} *buflist = 0;
static int bufAllocs = 0;

static char *
GetBuffer(void)
{
    char *ret;
    if (buflist) {
	ret = (char *) buflist;
	buflist = buflist->next;
    } else {
	ret = osi_Alloc(COPBUFSIZE);
	bufAllocs++;
    }
    return ret;
}

static void
PutBuffer(char *b)
{
    struct buflist *bl = (struct buflist *)b;
    bl->next = buflist;
    buflist = bl;
}

afs_int32
SRXKST_Copious(struct rx_call *call, u_long inlen, u_long insum,
	       u_long outlen, u_long *outsum)
{
    long code;
    long mysum;
    char *buf;
    int i;
    long b;
    long bytesTransfered;
    long n;

    code = CheckAuth(call);
    if (code)
	return code;
    buf = GetBuffer();
    mysum = 0;
    bytesTransfered = 0;
    while (bytesTransfered < inlen) {
	u_long tlen;		/* how much more to do */
	tlen = inlen - bytesTransfered;
	if (tlen > COPBUFSIZE)
	    tlen = COPBUFSIZE;
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
	bytesTransfered += tlen;
    }
    if (code)
	goto done;
    if (mysum != insum) {
	code = RXKST_BADINPUTSUM;
	goto done;
    }
#define BIG_PRIME 1257056893	/* 0x4AED2A7d */
#if 0
#define NextByte() ((b>24 ? ((seed = seed*BIG_PRIME + BIG_PRIME),b=0) : 0), \
		    (b +=8), ((seed >> (b-8))&0xff))
#else
#define NextByte() (b+=3)
#endif
    b = 32;

    mysum = 0;
    bytesTransfered = 0;
    while (bytesTransfered < outlen) {
	u_long tlen;		/* how much more to do */
	tlen = outlen - bytesTransfered;
	if (tlen > COPBUFSIZE)
	    tlen = COPBUFSIZE;
	for (i = 0; i < tlen; i++)
	    mysum += (buf[i] = NextByte());
	n = rx_Write(call, buf, tlen);
	if (n != tlen) {
	    if (n < 0)
		code = n;
	    else
		code = RXKST_WRITESHORT;
	    break;
	}
	bytesTransfered += tlen;
    }
  done:
    PutBuffer(buf);
    if (code)
	return code;
    *outsum = mysum;
    return 0;
}
