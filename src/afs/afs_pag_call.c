/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"
#include "rx/rx_globals.h"
#if !defined(UKERNEL) && !defined(AFS_LINUX20_ENV)
#include "net/if.h"
#ifdef AFS_SGI62_ENV
#include "h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_DARWIN60_ENV)
#include "netinet/in_var.h"
#endif
#endif /* !defined(UKERNEL) */
#ifdef AFS_LINUX22_ENV
#include "h/smp_lock.h"
#endif
#include "rmtsys.h"
#include "pagcb.h"


afs_int32 afs_termState = 0;
afs_int32 afs_gcpags = AFS_GCPAGS;
int afs_shuttingdown = 0;
int afs_cold_shutdown = 0;
int afs_resourceinit_flag = 0;
afs_int32 afs_nfs_server_addr;
struct interfaceAddr afs_cb_interface;
struct afs_osi_WaitHandle AFS_WaitHandler;
static struct rx_securityClass *srv_secobj;
static struct rx_securityClass *clt_secobj;
static struct rx_service *stats_svc;
static struct rx_service *pagcb_svc;
static struct rx_connection *rmtsys_conn;
char *afs_sysname = 0;
char *afs_sysnamelist[MAXNUMSYSNAMES];
int afs_sysnamecount = 0;
int afs_sysnamegen = 0;


void afs_Daemon(void)
{
    afs_int32 now, last10MinCheck, last60MinCheck;

    last10MinCheck = 0;
    last60MinCheck = 0;
    while (1) {
	rx_CheckPackets();
	now = osi_Time();

	if (last10MinCheck + 600 < now) {
	    afs_GCUserData(0);
	}

	if (last60MinCheck + 3600 < now) {
	    afs_int32 didany;
	    afs_GCPAGs(&didany);
	}

	now = 20000 - (osi_Time() - now);
	afs_osi_Wait(now, &AFS_WaitHandler, 0);

	if (afs_termState == AFSOP_STOP_AFS) {
#if defined(AFS_SUN5_ENV) || defined(RXK_LISTENER_ENV)
	    afs_termState = AFSOP_STOP_RXEVENT;
#else
	    afs_termState = AFSOP_STOP_COMPLETE;
#endif
	    afs_osi_Wakeup(&afs_termState);
	    return;
	}
    }
}


void afspag_Init(afs_int32 nfs_server_addr)
{
    struct clientcred ccred;
    struct rmtbulk idata, odata;
    afs_int32 code, err, addr, obuf;
    int i;

    afs_uuid_create(&afs_cb_interface.uuid);

    AFS_GLOCK();

    afs_InitStats();
    rx_Init(htons(7001));

    AFS_STATCNT(afs_ResourceInit);
    RWLOCK_INIT(&afs_xuser, "afs_xuser");
    RWLOCK_INIT(&afs_xpagcell, "afs_xpagcell");
    RWLOCK_INIT(&afs_xpagsys, "afs_xpagsys");
    RWLOCK_INIT(&afs_icl_lock, "afs_icl_lock");
#ifndef AFS_FBSD_ENV
    LOCK_INIT(&osi_fsplock, "osi_fsplock");
    LOCK_INIT(&osi_flplock, "osi_flplock");
#endif

    afs_resourceinit_flag = 1;
    afs_nfs_server_addr = nfs_server_addr;
    for (i = 0; i < MAXNUMSYSNAMES; i++)
	afs_sysnamelist[i] = afs_osi_Alloc(MAXSYSNAME);
    afs_sysname = afs_sysnamelist[0];
    strcpy(afs_sysname, SYS_NAME);
    afs_sysnamecount = 1;
    afs_sysnamegen++;

    srv_secobj = rxnull_NewServerSecurityObject();
    stats_svc = rx_NewService(0, RX_STATS_SERVICE_ID, "rpcstats", &srv_secobj,
			      1, RXSTATS_ExecuteRequest);
    pagcb_svc = rx_NewService(0, PAGCB_SERVICEID, "pagcb", &srv_secobj,
			      1, PAGCB_ExecuteRequest);
    rx_StartServer(0);

    clt_secobj = rxnull_NewClientSecurityObject();
    rmtsys_conn = rx_NewConnection(nfs_server_addr, htons(7009),
				   RMTSYS_SERVICEID, clt_secobj, 0);

#ifdef RXK_LISTENER_ENV
    afs_start_thread(rxk_Listener,       "Rx Listener");
#endif
    afs_start_thread(rx_ServerProc,      "Rx Server Thread");
    afs_start_thread(afs_rxevent_daemon, "Rx Event Daemon");
    afs_start_thread(afs_Daemon,         "AFS PAG Daemon");

    afs_icl_InitLogs();

    AFS_GUNLOCK();

    /* If it's reachable, tell the translator to nuke our creds.
     * We should be more agressive about making sure this gets done,
     * even if the translator is unreachable when we boot.
     */
    addr = obuf = err = 0;
    idata.rmtbulk_len = sizeof(addr);
    idata.rmtbulk_val = (char *)&addr;
    odata.rmtbulk_len = sizeof(obuf);
    odata.rmtbulk_val = (char *)&obuf;
    memset(&ccred, 0, sizeof(ccred));
    code = RMTSYS_Pioctl(rmtsys_conn, &ccred, NIL_PATHP, 0x4F01, 0,
                         &idata, &odata, &err);
}				/*afs_ResourceInit */


/* called with the GLOCK held */
void afspag_Shutdown(void)
{
    if (afs_shuttingdown)
	return;
    afs_shuttingdown = 1;
    afs_termState = AFSOP_STOP_RXCALLBACK;
    rx_WakeupServerProcs();
    while (afs_termState == AFSOP_STOP_RXCALLBACK)
	afs_osi_Sleep(&afs_termState);
    /* rx_ServerProc sets AFS_STOP_AFS */

    while (afs_termState == AFSOP_STOP_AFS) {
	afs_osi_CancelWait(&AFS_WaitHandler);
	afs_osi_Sleep(&afs_termState);
    }
    /* afs_Daemon sets AFS_STOP_RXEVENT */

#if defined(AFS_SUN5_ENV) || defined(RXK_LISTENER_ENV)
    while (afs_termState == AFSOP_STOP_RXEVENT)
	afs_osi_Sleep(&afs_termState);
    /* afs_rxevent_daemon sets AFSOP_STOP_RXK_LISTENER */

#if defined(RXK_LISTENER_ENV)
    afs_osi_UnmaskRxkSignals();
    osi_StopListener();
    while (afs_termState == AFSOP_STOP_RXK_LISTENER)
	afs_osi_Sleep(&afs_termState);
    /* rxk_Listener sets AFSOP_STOP_COMPLETE */
#endif
#endif
}

static void token_conversion(char *buffer, int buf_size, int in)
{
    struct ClearToken *ticket;
    afs_int32 *lptr, n;

    /* secret ticket */
    if (buf_size < 4) return;
    lptr = (afs_int32 *)buffer;
    buffer += 4; buf_size -= 4;
    if (in) {
	*lptr = ntohl(*lptr);
	n = *lptr;
    } else {
	n = *lptr;
	*lptr = htonl(*lptr);
    }
    if (n < 0 || buf_size < n) return;
    buffer += n; buf_size -= n;

    /* clear token */
    if (buf_size < 4) return;
    lptr = (afs_int32 *)buffer;
    buffer += 4; buf_size -= 4;
    if (in) {
	*lptr = ntohl(*lptr);
	n = *lptr;
    } else {
	n = *lptr;
	*lptr = htonl(*lptr);
    }
    if (n < 0 || buf_size < n) return;
    if (n >= sizeof(struct ClearToken)) {
	ticket = (struct ClearToken *)buffer;
	if (in) {
	    ticket->AuthHandle     = ntohl(ticket->AuthHandle);
	    ticket->ViceId         = ntohl(ticket->ViceId);
	    ticket->BeginTimestamp = ntohl(ticket->BeginTimestamp);
	    ticket->EndTimestamp   = ntohl(ticket->EndTimestamp);
	} else {
	    ticket->AuthHandle     = htonl(ticket->AuthHandle);
	    ticket->ViceId         = htonl(ticket->ViceId);
	    ticket->BeginTimestamp = htonl(ticket->BeginTimestamp);
	    ticket->EndTimestamp   = htonl(ticket->EndTimestamp);
	}
    }
    buffer += n; buf_size -= n;

    /* primary flag */
    if (buf_size < 4) return;
    lptr = (afs_int32 *)buffer;
    if (in) {
	*lptr = ntohl(*lptr);
    } else {
	*lptr = htonl((*lptr) & ~0x8000);
    }
    return;
}

static void FetchVolumeStatus_conversion(char *buffer, int buf_size, int in)
{
    AFSFetchVolumeStatus *status = (AFSFetchVolumeStatus *)buffer;

    if (buf_size < sizeof(AFSFetchVolumeStatus))
	return;
    if (in) {
	status->Vid              = ntohl(status->Vid);
	status->ParentId         = ntohl(status->ParentId);
	status->Type             = ntohl(status->Type);
	status->MinQuota         = ntohl(status->MinQuota);
	status->MaxQuota         = ntohl(status->MaxQuota);
	status->BlocksInUse      = ntohl(status->BlocksInUse);
	status->PartBlocksAvail  = ntohl(status->PartBlocksAvail);
	status->PartMaxBlocks    = ntohl(status->PartMaxBlocks);
    } else {
	status->Vid              = htonl(status->Vid);
	status->ParentId         = htonl(status->ParentId);
	status->Type             = htonl(status->Type);
	status->MinQuota         = htonl(status->MinQuota);
	status->MaxQuota         = htonl(status->MaxQuota);
	status->BlocksInUse      = htonl(status->BlocksInUse);
	status->PartBlocksAvail  = htonl(status->PartBlocksAvail);
	status->PartMaxBlocks    = htonl(status->PartMaxBlocks);
    }
}

static void inparam_conversion(int cmd, char *buffer, int buf_size, int in)
{
    afs_int32 *lptr = (afs_int32 *)buffer;

    switch (cmd & 0xffff) {
	case (0x5600 |  3): /* VIOCSETTOK */
	    token_conversion(buffer, buf_size, in);
	    return;

	case (0x5600 |  5): /* VIOCSETVOLSTAT */
	    FetchVolumeStatus_conversion(buffer, buf_size, in);
	    return;

	case (0x5600 |  8): /* VIOCGETTOK */
	case (0x5600 | 10): /* VIOCCKSERV */
	case (0x5600 | 20): /* VIOCACCESS */
	case (0x5600 | 24): /* VIOCSETCACHESIZE */
	case (0x5600 | 27): /* VIOCGETCELL */
	case (0x5600 | 32): /* VIOC_AFS_MARINER_HOST */
	case (0x5600 | 34): /* VIOC_VENUSLOG */
	case (0x5600 | 38): /* VIOC_AFS_SYSNAME */
	case (0x5600 | 39): /* VIOC_EXPORTAFS */
	    /* one 32-bit integer */
	    if (buf_size >= 4) {
		if (in) lptr[0] = ntohl(lptr[0]);
		else    lptr[0] = htonl(lptr[0]);
	    }
	    return;

	case (0x5600 | 36): /* VIOCSETCELLSTATUS */
	    /* two 32-bit integers */
	    if (buf_size >= 4) {
		if (in) lptr[0] = ntohl(lptr[0]);
		else    lptr[0] = htonl(lptr[0]);
	    }
	    if (buf_size >= 8) {
		if (in) lptr[1] = ntohl(lptr[1]);
		else    lptr[1] = htonl(lptr[1]);
	    }
	    return;
    }
}

static void outparam_conversion(int cmd, char *buffer, int buf_size, int in)
{
    afs_int32 *lptr = (afs_int32 *)buffer;
    int i;

    switch (cmd & 0xffff) {
	case (0x5600 |  4): /* VIOCGETVOLSTAT */
	case (0x5600 |  5): /* VIOCSETVOLSTAT */
	    FetchVolumeStatus_conversion(buffer, buf_size, in);
	    return;

	case (0x5600 |  8): /* VIOCGETTOK */
	    token_conversion(buffer, buf_size, in);
	    return;

	case (0x5600 | 12): /* VIOCCKCONN */
	case (0x5600 | 32): /* VIOC_AFS_MARINER_HOST */
	case (0x5600 | 34): /* VIOC_VENUSLOG */
	case (0x5600 | 35): /* VIOC_GETCELLSTATUS */
	case (0x5600 | 38): /* VIOC_AFS_SYSNAME */
	case (0x5600 | 39): /* VIOC_EXPORTAFS */
	    /* one 32-bit integer */
	    if (buf_size >= 4) {
		if (in) lptr[0] = ntohl(lptr[0]);
		else    lptr[0] = htonl(lptr[0]);
	    }
	    return;

	case (0x5600 | 40): /* VIOCGETCACHEPARMS */
	    /* sixteen 32-bit integers */
	    for (i = 0; i < 16 && buf_size >= 4; i++) {
		if (in) lptr[i] = ntohl(lptr[i]);
		else    lptr[i] = htonl(lptr[i]);
		buf_size -= 4;
	    }
	    return;
    }
}


/* called with the GLOCK held */
int
#ifdef	AFS_SUN5_ENV
afs_syscall_pioctl(path, com, cmarg, follow, rvp, credp)
     rval_t *rvp;
     struct AFS_UCRED *credp;
#else
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
afs_syscall_pioctl(path, com, cmarg, follow, credp)
     struct AFS_UCRED *credp;
#else
afs_syscall_pioctl(path, com, cmarg, follow)
#endif
#endif
     char *path;
     unsigned int com;
     caddr_t cmarg;
     int follow;
{
#ifdef	AFS_AIX41_ENV
    struct ucred *credp = crref();	/* don't free until done! */
#endif
#ifdef AFS_LINUX22_ENV
    cred_t *credp = crref();	/* don't free until done! */
#endif
    struct afs_ioctl data;
    struct clientcred ccred;
    struct rmtbulk idata, odata;
    short in_size, out_size;
    afs_int32 code = 0, pag, err;
    gid_t g0, g1;
    char *abspath, *pathbuf = 0;

    AFS_STATCNT(afs_syscall_pioctl);
    if (follow)
	follow = 1;		/* compat. with old venus */
    code = copyin_afs_ioctl(cmarg, &data);
    if (code) goto out;

    if ((com & 0xff) == 90) {
	/* PSetClientContext, in any space */
	code = EINVAL;
	goto out;
    }

    /* Special handling for a few pioctls */
    switch (com & 0xffff) {
	case (0x5600 |  3): /* VIOCSETTOK */
	    code = afspag_PSetTokens(data.in, data.in_size, &credp);
	    if (code) goto out;
	    break;

	case (0x5600 |  9): /* VIOCUNLOG */
	case (0x5600 | 21): /* VIOCUNPAG */
	    code = afspag_PUnlog(data.in, data.in_size, &credp);
	    if (code) goto out;
	    break;

	case (0x5600 | 38): /* VIOC_AFS_SYSNAME */
	    code = afspag_PSetSysName(data.in, data.in_size, &credp);
	    if (code) goto out;
	    break;
    }

    /* Set up credentials */
    memset(&ccred, 0, sizeof(ccred));
    pag = PagInCred(credp);
    ccred.uid = credp->cr_uid;
    if (pag != NOPAG) {
	 afs_get_groups_from_pag(pag, &g0, &g1);
	 ccred.group0 = g0;
	 ccred.group1 = g1;
    }

    /*
     * Copy the path and convert to absolute, if one was given.
     * NB: We can only use osI_AllocLargeSpace here as long as
     * RMTSYS_MAXPATHLEN is less than AFS_LRALLOCSIZ.
     */
    if (path) {
	pathbuf = osi_AllocLargeSpace(RMTSYS_MAXPATHLEN);
	if (!pathbuf) {
	    code = ENOMEM;
	    goto out;
	}
	code = osi_abspath(path, pathbuf, RMTSYS_MAXPATHLEN, 0, &abspath);
	if (code)
	    goto out_path;
    } else {
	abspath = NIL_PATHP;
    }

    /* Allocate, copy, and convert incoming data */
    idata.rmtbulk_len = in_size = data.in_size;
    if (in_size  < 0 || in_size  > MAXBUFFERLEN) {
	code = EINVAL;
	goto out_path;
    }
    if (in_size > AFS_LRALLOCSIZ)
	 idata.rmtbulk_val = osi_Alloc(in_size);
    else
	 idata.rmtbulk_val = osi_AllocLargeSpace(AFS_LRALLOCSIZ);
    if (!idata.rmtbulk_val) {
	code = ENOMEM;
	goto out_path;
    }
    if (in_size) {
	AFS_COPYIN(data.in, idata.rmtbulk_val, in_size, code);
	if (code)
	    goto out_idata;
	inparam_conversion(com, idata.rmtbulk_val, in_size, 0);
    }

    /* Allocate space for outgoing data */
    odata.rmtbulk_len = out_size = data.out_size;
    if (out_size < 0 || out_size > MAXBUFFERLEN) {
	code = EINVAL;
	goto out_idata;
    }
    if (out_size > AFS_LRALLOCSIZ)
	 odata.rmtbulk_val = osi_Alloc(out_size);
    else
	 odata.rmtbulk_val = osi_AllocLargeSpace(AFS_LRALLOCSIZ);
    if (!odata.rmtbulk_val) {
	code = ENOMEM;
	goto out_idata;
    }

    AFS_GUNLOCK();
    code = RMTSYS_Pioctl(rmtsys_conn, &ccred, abspath, com, follow,
			 &idata, &odata, &err);
    AFS_GLOCK();
    if (code)
	goto out_odata;

    /* Convert and copy out the result */
    if (odata.rmtbulk_len > out_size) {
	code = E2BIG;
	goto out_odata;
    }
    if (odata.rmtbulk_len) {
	outparam_conversion(com, odata.rmtbulk_val, odata.rmtbulk_len, 1);
	AFS_COPYOUT(odata.rmtbulk_val, data.out, odata.rmtbulk_len, code);
    }
    if (!code)
	code = err;

out_odata:
    if (out_size > AFS_LRALLOCSIZ)
	osi_Free(odata.rmtbulk_val, out_size);
    else
	osi_FreeLargeSpace(odata.rmtbulk_val);

out_idata:
    if (in_size > AFS_LRALLOCSIZ)
	osi_Free(idata.rmtbulk_val, in_size);
    else
	osi_FreeLargeSpace(idata.rmtbulk_val);

out_path:
    if (path)
	osi_FreeLargeSpace(pathbuf);

out:
#if defined(AFS_LINUX22_ENV) || defined(AFS_AIX41_ENV)
    crfree(credp);
#endif
#if defined(KERNEL_HAVE_UERROR)
    if (!getuerror())
	setuerror(code);
    return (getuerror());
#else
    return (code);
#endif
}


int
afs_syscall_call(parm, parm2, parm3, parm4, parm5, parm6)
     long parm, parm2, parm3, parm4, parm5, parm6;
{
    /* superusers may shut us down, as with afsd --shutdown */
#ifdef AFS_SUN5_ENV
    if (parm == AFSOP_SHUTDOWN && afs_suser(CRED()))
#else
    if (parm == AFSOP_SHUTDOWN && afs_suser(NULL))
#endif
    {
	AFS_GLOCK();
	afspag_Shutdown();
	AFS_GUNLOCK();
	return 0;
    }

    /* otherwise, we don't support afs_syscall_call, period */
#if defined(KERNEL_HAVE_UERROR)
    setuerror(EPERM);
#endif
    return EPERM;
}
