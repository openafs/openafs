/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* ticket caching code */

#include <afsconfig.h>
#include <afs/param.h>

#ifdef AFS_RXK5
#include "rxk5_utilafs.h"
#include "rxk5_tkt.h"
#endif

RCSID
    ("$Header$");

#include <afs/stds.h>
#include <stdio.h>
#include <afs/pthread_glock.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <rpc.h>
#include <afs/smb_iocons.h>
#include <afs/pioctl_nt.h>
#include "afs/afsrpc.h"
#include <afs/vice.h>
#include "auth.h"
#include <afs_token.h>
#include <afs/afsutil.h>

/* TBUFFERSIZE must be at least 512 larger than KTCMAXTICKETSIZE */
#define TBUFFERSIZE 12512

/* Why was TBUFFERSIZE not already sized by Unix MAXKTCTICKETLEN and
 * MAXKTCREALMLEN ? */

#define MAXPIOCTLTOKENLEN \
(3*sizeof(afs_int32)+MAXKTCTICKETLEN+sizeof(struct ClearToken)+MAXKTCREALMLEN)

#define ENOTCONN                WSAENOTCONN

/* Forward declarations for local token cache. */
static int SetLocalToken(struct ktc_principal *aserver,
			 struct ktc_token *atoken,
			 struct ktc_principal *aclient, afs_int32 flags);
static int GetLocalToken(struct ktc_principal *aserver,
			 struct ktc_token *atoken, int atokenLen,
			 struct ktc_principal *aclient);
static int ForgetLocalTokens();
static int ForgetOneLocalToken(struct ktc_principal *aserver);


static char AFSConfigKeyName[] =
    "SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters";

static char AFSGlobalKTCMutexName[] = "Global\\AFS_KTC_Mutex";
static char AFSKTCMutexName[] = "AFS_KTC_Mutex";

/*
 * Support for RPC's to send and receive session keys
 *
 * Session keys must not be sent and received in the clear.  We have no
 * way to piggyback encryption on SMB, so we use a separate RPC, using
 * packet privacy (when available).  In SetToken, the RPC is done first;
 * in GetToken, the pioctl is done first.
 */

char rpcErr[256];

void __RPC_FAR *__RPC_USER
midl_user_allocate(size_t cBytes)
{
    return ((void __RPC_FAR *)malloc(cBytes));
}

void __RPC_USER
midl_user_free(void __RPC_FAR * p)
{
    free(p);
}

/*
 * Determine the server name to be used in the RPC binding.  If it is
 * the same as the client (i.e. standalone, non-gateway), NULL can be
 * used, so it is not necessary to call gethostbyname().
 */
void
getservername(char **snp, unsigned int snSize)
{
    HKEY parmKey;
    long code;

    code =
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSConfigKeyName, 0, KEY_QUERY_VALUE,
		     &parmKey);
    if (code != ERROR_SUCCESS)
	goto nogateway;
    code = RegQueryValueEx(parmKey, "Gateway", NULL, NULL, *snp, &snSize);
    RegCloseKey(parmKey);
    if (code == ERROR_SUCCESS)
	return;
  nogateway:
    /* No gateway name in registry; use ourself */
    *snp = NULL;
}

RPC_STATUS
send_key(afs_uuid_t uuid, char sessionKey[8])
{
    RPC_STATUS status;
    char *stringBinding = NULL;
    ULONG authnLevel, authnSvc;
    char serverName[256];
    char *serverNamep = serverName;
    char encrypt[32];
    BOOL encryptionOff = FALSE;
    char protseq[32];

    /* Encryption on by default */
    if (GetEnvironmentVariable("AFS_RPC_ENCRYPT", encrypt, sizeof(encrypt)))
	if (!strcmpi(encrypt, "OFF"))
	    encryptionOff = TRUE;

    /* Protocol sequence is local by default */
    if (!GetEnvironmentVariable("AFS_RPC_PROTSEQ", protseq, sizeof(protseq)))
	strcpy(protseq, "ncalrpc");

    /* Server name */
    getservername(&serverNamep, sizeof(serverName));

    status = RpcStringBindingCompose("",	/* obj uuid */
				     protseq, serverNamep, "",	/* endpoint */
				     "",	/* protocol options */
				     &stringBinding);
    if (status != RPC_S_OK)
	goto cleanup;

    status = RpcBindingFromStringBinding(stringBinding, &hAfsHandle);
    if (status != RPC_S_OK)
	goto cleanup;

    /*
     * On Windows 95/98, we must resolve the binding before calling
     * SetAuthInfo.  On Windows NT, we don't have to resolve yet,
     * but it does no harm.
     */
    status = RpcEpResolveBinding(hAfsHandle, afsrpc_v1_0_c_ifspec);
    if (status != RPC_S_OK)
	goto cleanup;

    if (encryptionOff) {
	authnLevel = RPC_C_AUTHN_LEVEL_NONE;
	authnSvc = RPC_C_AUTHN_WINNT;
    } else {
	authnLevel = RPC_C_AUTHN_LEVEL_PKT_PRIVACY;
	authnSvc = RPC_C_AUTHN_WINNT;
    }

    status =
	RpcBindingSetAuthInfo(hAfsHandle, NULL, authnLevel, authnSvc, NULL,
			      RPC_C_AUTHZ_NONE);
    if (status != RPC_S_OK)
	goto cleanup;

    RpcTryExcept {
	status = AFSRPC_SetToken(uuid, sessionKey);
    }
    RpcExcept(1) {
	status = RpcExceptionCode();
    }
    RpcEndExcept cleanup:if (stringBinding)
	  RpcStringFree(&stringBinding);

    if (hAfsHandle != NULL)
	RpcBindingFree(&hAfsHandle);

    return status;
}

RPC_STATUS
receive_key(afs_uuid_t uuid, char sessionKey[8])
{
    RPC_STATUS status;
    char *stringBinding = NULL;
    ULONG authnLevel, authnSvc;
    char serverName[256];
    char *serverNamep = serverName;
    char encrypt[32];
    BOOL encryptionOff = FALSE;
    char protseq[32];

    /* Encryption on by default */
    if (GetEnvironmentVariable("AFS_RPC_ENCRYPT", encrypt, sizeof(encrypt)))
	if (!strcmpi(encrypt, "OFF"))
	    encryptionOff = TRUE;

    /* Protocol sequence is local by default */
    if (!GetEnvironmentVariable("AFS_RPC_PROTSEQ", protseq, sizeof(protseq)))
	strcpy(protseq, "ncalrpc");

    /* Server name */
    getservername(&serverNamep, sizeof(serverName));

    status = RpcStringBindingCompose("",	/* obj uuid */
				     protseq, serverNamep, "",	/* endpoint */
				     "",	/* protocol options */
				     &stringBinding);
    if (status != RPC_S_OK)
	goto cleanup;

    status = RpcBindingFromStringBinding(stringBinding, &hAfsHandle);
    if (status != RPC_S_OK)
	goto cleanup;

    /*
     * On Windows 95/98, we must resolve the binding before calling
     * SetAuthInfo.  On Windows NT, we don't have to resolve yet,
     * but it does no harm.
     */
    status = RpcEpResolveBinding(hAfsHandle, afsrpc_v1_0_c_ifspec);
    if (status != RPC_S_OK)
	goto cleanup;

    if (encryptionOff) {
	authnLevel = RPC_C_AUTHN_LEVEL_NONE;
	authnSvc = RPC_C_AUTHN_WINNT;
    } else {
	authnLevel = RPC_C_AUTHN_LEVEL_PKT_PRIVACY;
	authnSvc = RPC_C_AUTHN_WINNT;
    }

    status =
	RpcBindingSetAuthInfo(hAfsHandle, NULL, authnLevel, authnSvc, NULL,
			      RPC_C_AUTHZ_NONE);
    if (status != RPC_S_OK)
	goto cleanup;

    RpcTryExcept {
	status = AFSRPC_GetToken(uuid, sessionKey);
    }
    RpcExcept(1) {
	status = RpcExceptionCode();
    }
    RpcEndExcept cleanup:if (stringBinding)
	  RpcStringFree(&stringBinding);

    if (hAfsHandle != NULL)
	RpcBindingFree(&hAfsHandle);

    return status;
}

/* new-style token interface (TODO: finish) */

int
afstoken_to_soliton(pioctl_set_token *a_token,
    int type,
    afstoken_soliton *at);
    
/* copy bits of an rxkad token into a ktc_token */
int
afstoken_to_token(pioctl_set_token *a_token,
    struct ktc_token *ttoken,
    int ttoksize,
    int *flags,
    struct ktc_principal *aclient);
    
#ifdef AFS_RXK5
/* copy bits of an rxkad token into a k5 credential */
int
afstoken_to_v5cred(pioctl_set_token *a_token, krb5_creds *v5cred);
#endif

int
ktc_SetTokenEx(pioctl_set_token *a_token)
{
#ifndef MAX_RXK5_TOKEN_LEN
#define MAX_RXK5_TOKEN_LEN 4096
#endif
    struct ViceIoctl iob;
    register afs_int32 code;
    char creds[MAX_RXK5_TOKEN_LEN];
    afs_int32 creds_len;
    XDR xdrs[1];

    xdrmem_create(xdrs, creds, MAX_RXK5_TOKEN_LEN, XDR_ENCODE);
    if (!xdr_pioctl_set_token(xdrs, a_token))
	return KTC_INVAL;
    creds_len = xdr_getpos(xdrs);
       	
    /* now setup for the pioctl */
    iob.in = creds;
    iob.in_size = creds_len;
    iob.out = creds;
    iob.out_size = creds_len;

    code = pioctl(0, VIOCSETTOK2 , &iob, 0);
    if (code == -1 && errno == EINVAL) {
	struct ktc_principal aserver[1], aclient[1];
	struct ktc_token atoken[1];
	afs_int32 flags;

	memset(aserver, 0, sizeof *aserver);
	memset(aclient, 0, sizeof *aclient);
	memset(atoken, 0, sizeof *atoken);
	code = afstoken_to_token(a_token, atoken, sizeof *atoken, &flags, aclient);
	if (code) return KTC_INVAL;
	strcpy(aserver->name, "afs");
	strcpy(aserver->cell, a_token->cell);
	return ktc_SetToken(aserver, atoken, aclient, flags);
    }

    if (code)
	return KTC_PIOCTLFAIL;

    return 0;
}

#ifdef AFS_RXK5

/*
 *	return rxk5 enctypes from kernel.
 *	return -1 on error (not supported, etc.)
 *	otherwise, return count of enctypes found.
 */

int
ktc_GetK5Enctypes(krb5_enctype *buf, int buflen)
{
    afs_int32 code;
    int i;
    char tbuffer[256], *tp, *ep;
    struct ViceIoctl blob[1];
    static char key[] = "rxk5.enctypes";

    memset(blob, 0, sizeof blob);
    blob->in = key;
    blob->in_size = sizeof key;
    blob->out = tbuffer;
    blob->out_size = sizeof tbuffer - 1;
	memset(tbuffer, 0, sizeof tbuffer);	/* XXX please valgrind */

    code = pioctl(0, VIOCGETPROP, blob, 0);
    if (code) return code;	/* "too old" */

    tbuffer[sizeof tbuffer-1] = 0;
    if (memcmp(tbuffer, key, sizeof key)) {
	errno = EDOM;		/* "rxk5 not configured" */
	return -1;
    }
    tp = tbuffer + sizeof key;
    for (i = 0; i < buflen; ++i) {
	while (*tp == ' ') ++tp;
	if (!*tp) break;
	buf[i] = strtol(tp, &ep, 0);
	if (tp == ep) {
	    errno = EDOM;	/* bad data return */
	    return -1;
	}
	tp = ep;
    }
    return i;			/* success */
}

/* Set a K5 token (internal) */
static int
SetK5Token(krb5_context context, char *cell,
	      krb5_creds *v5cred, char *username, char* smbname, afs_int32 flags)
{
    register afs_int32 code;
    pioctl_set_token a_token[1];
   
    memset(a_token, 0, sizeof *a_token); 
    a_token->flags = flags;
    a_token->cell = cell;
    a_token->username = username;
    a_token->smbname = smbname;
    code = add_afs_token_rxk5(
	    context,
	    v5cred,
	    a_token);

    if(!code)
	code = ktc_SetTokenEx(a_token);
#if 0
    a_token->cell = 0;
    xdrs->x_op = XDR_FREE;
    xdr_pioctl_set_token(xdrs, a_token);
#endif

    return code;
}

/* Set a K5 token */

afs_int32
ktc_SetK5Token(krb5_context context,
    char *cell,
    krb5_creds *v5cred,
    char *username,
    char *smbname,
    afs_int32 flags)
{
    int code;
    LOCK_GLOBAL_MUTEX;
    code = SetK5Token(context, cell, v5cred, username, smbname, flags);
    UNLOCK_GLOBAL_MUTEX;
    if (!code)
	return 0;
    if (code == -1)
	code = errno;
    else if (code == KTC_PIOCTLFAIL)
	code = errno;
    if (code == ESRCH)
	return KTC_NOCELL;
    if (code == EINVAL)
	return KTC_NOPIOCTL;
    if (code == EIO)
	return KTC_NOCM;
    return KTC_PIOCTLFAIL;
}

#endif /* AFS_RXK5 */

int
ktc_SetToken(struct ktc_principal *server, struct ktc_token *token,
	     struct ktc_principal *client, int flags)
{
    struct ViceIoctl iob;
    char tbuffer[TBUFFERSIZE];
    char *tp;
    struct ClearToken ct;
    int temp;
    int code;
    RPC_STATUS status;
    afs_uuid_t uuid;
    HANDLE ktcMutex = NULL;

    if (token->ticketLen < MINKTCTICKETLEN
	|| token->ticketLen > MAXKTCTICKETLEN)
	return KTC_INVAL;

    if (strcmp(server->name, "afs")) {
	return SetLocalToken(server, token, client, flags);
    }

    tp = tbuffer;

    /* ticket length */
    memcpy(tp, &token->ticketLen, sizeof(token->ticketLen));
    tp += sizeof(token->ticketLen);

    /* ticket */
    memcpy(tp, token->ticket, token->ticketLen);
    tp += token->ticketLen;

    /* clear token */
    ct.AuthHandle = token->kvno;
    /*
     * Instead of sending the session key in the clear, we zero it,
     * and send it later, via RPC, encrypted.
     */
#ifndef AFS_WIN95_ENV
    /*
     * memcpy(ct.HandShakeKey, &token->sessionKey, sizeof(token->sessionKey));
     */
    memset(ct.HandShakeKey, 0, sizeof(ct.HandShakeKey));
#else
    memcpy(ct.HandShakeKey, &token->sessionKey, sizeof(token->sessionKey));
#endif
    ct.BeginTimestamp = token->startTime;
    ct.EndTimestamp = token->endTime;
    if (ct.BeginTimestamp == 0)
	ct.BeginTimestamp = 1;

    /* We don't know from Vice ID's yet */
    ct.ViceId = 37;		/* XXX */
    if (((ct.EndTimestamp - ct.BeginTimestamp) & 1) == 1)
	ct.BeginTimestamp++;	/* force lifetime to be even */

    /* size of clear token */
    temp = sizeof(struct ClearToken);
    memcpy(tp, &temp, sizeof(temp));
    tp += sizeof(temp);

    /* clear token itself */
    memcpy(tp, &ct, sizeof(ct));
    tp += sizeof(ct);

    /* flags; on NT there is no setpag flag, but there is an
     * integrated logon flag */
    temp = ((flags & AFS_SETTOK_LOGON) ? PIOCTL_LOGON : 0);
    memcpy(tp, &temp, sizeof(temp));
    tp += sizeof(temp);

    /* cell name */
    temp = (int)strlen(server->cell);
    if (temp >= MAXKTCREALMLEN)
	return KTC_INVAL;
    strcpy(tp, server->cell);
    tp += temp + 1;

    /* user name */
    temp = (int)strlen(client->name);
    if (temp >= MAXKTCNAMELEN)
	return KTC_INVAL;
    strcpy(tp, client->name);
    tp += temp + 1;

    /* we need the SMB user name to associate the tokens with in the
     * integrated logon case. */
    if (flags & AFS_SETTOK_LOGON) {
	if (client->smbname == NULL)
	    temp = 0;
	else
	    temp = (int)strlen(client->smbname);
	if (temp == 0 || temp >= MAXKTCNAMELEN)
	    return KTC_INVAL;
	strcpy(tp, client->smbname);
	tp += temp + 1;
    }

    /* uuid */
    status = UuidCreate((UUID *) & uuid);
    memcpy(tp, &uuid, sizeof(uuid));
    tp += sizeof(uuid);


#ifndef AFS_WIN95_ENV
    ktcMutex = CreateMutex(NULL, TRUE, AFSGlobalKTCMutexName);
    if (ktcMutex == NULL)
	return KTC_PIOCTLFAIL;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
	if (WaitForSingleObject(ktcMutex, INFINITE) != WAIT_OBJECT_0) {
	    CloseHandle(ktcMutex);
	    return KTC_PIOCTLFAIL;
	}
    }

    /* RPC to send session key */
    status = send_key(uuid, token->sessionKey.data);
    if (status != RPC_S_OK) {
	if (status == 1)
	    strcpy(rpcErr, "RPC failure in AFS gateway");
	else
	    DceErrorInqText(status, rpcErr);
	if (status == RPC_S_SERVER_UNAVAILABLE ||
	    status == EPT_S_NOT_REGISTERED) {
	    ReleaseMutex(ktcMutex);
	    CloseHandle(ktcMutex);
	    return KTC_NOCMRPC;
	} else {
	    ReleaseMutex(ktcMutex);
	    CloseHandle(ktcMutex);
	    return KTC_RPC;
	}
    }
#endif /* AFS_WIN95_ENV */

    /* set up for pioctl */
    iob.in = tbuffer;
    iob.in_size = (long)(tp - tbuffer);
    iob.out = tbuffer;
    iob.out_size = sizeof(tbuffer);

    code = pioctl(0, VIOCSETTOK, &iob, 0);

#ifndef AFS_WIN95_ENV
    ReleaseMutex(ktcMutex);
    CloseHandle(ktcMutex);
#endif /* AFS_WIN95_ENV */

    if (code) {
	if (code == -1) {
	    if (errno == ESRCH)
		return KTC_NOCELL;
	    else if (errno == ENODEV)
		return KTC_NOCM;
	    else if (errno == EINVAL)
		return KTC_INVAL;
	    else
		return KTC_PIOCTLFAIL;
	} else
	    return KTC_PIOCTLFAIL;
    }

    return 0;
}

/*
 *  Get AFS token at index ix, using new kernel token interface. 
 */
 
int
ktc_GetTokenEx(afs_int32 index, const char *cell,
    pioctl_set_token *a_token)
{
    struct ViceIoctl iob;
    char tbuffer[MAXPIOCTLTOKENLEN];
    afs_int32 code;
    register char *tp;
    afstoken_soliton at[1];
    XDR xdrs[1];

    LOCK_GLOBAL_MUTEX;
  
    memset(a_token, 0, sizeof *a_token); 
    memset(at, 0, sizeof *at); 
    if (cell) {
	int len;

	len = strlen(cell) + 1;
	tp = tbuffer;
	memcpy(tp, (char*)&index, sizeof(afs_int32));
	tp += sizeof(afs_int32);
	memcpy(tp, cell, len);
	tp += len;
	iob.in = tbuffer;
	iob.in_size = tp - tbuffer;
    } else {
	iob.in = (char *)&index;
	iob.in_size = sizeof(afs_int32);
    }
    iob.out = tbuffer;
    iob.out_size = sizeof(tbuffer);

    code = pioctl(0, VIOCGETTOK2 , &iob, 0);

    if (code == -1 && errno == EINVAL) {
	char *stp, *cellp;		/* secret token ptr */
	afs_int32 temp, primflag;
	int tktLen;			/* server ticket length */
	struct ClearToken ct;

	/* new interace isn't in kernel?  fall back to old */
	iob.in = (char *)&index;
	iob.in_size = sizeof(afs_int32);
	for (;;) {
	    code = pioctl(0, VIOCGETTOK, &iob, 0);
	    if (code) goto Failed;
	    /* token retrieved; parse buffer */
	    tp = tbuffer;

	    /* get ticket length */
	    memcpy(&temp, tp, sizeof(afs_int32));
	    tktLen = temp;
	    tp += sizeof(afs_int32);

	    /* remember where ticket is and skip over it */
	    stp = tp;
	    tp += tktLen;

	    /* get size of clear token and verify */
	    memcpy(&temp, tp, sizeof(afs_int32));
	    if (temp != sizeof(struct ClearToken)) {
		code = KTC_ERROR;
		goto Done;
	    }
	    tp += sizeof(afs_int32);

	    /* copy clear token */
	    memcpy(&ct, tp, temp);
	    tp += temp;

	    /* copy primary flag */
	    memcpy(&primflag, tp, sizeof(afs_int32));
	    tp += sizeof(afs_int32);

	    /* remember where cell name is */
	    cellp = tp;
	    if (!cell || !strcmp(cellp, cell))
		break;
	    if (++index >= 200) {
		code = KTC_PIOCTLFAIL;
		goto Done;
	    }
	}

	/* set return values */
	/* got token for cell; check that it will fit */
	if (tktLen > (unsigned) MAXKTCTICKETLEN) {
	    code = KTC_TOOBIG;
	    goto Done;
	}
	code = ENOMEM;
	memset(a_token, 0, sizeof *a_token);
	if (!(a_token->cell = strdup(cellp)))
	    goto Done;
	at->at_type = AFSTOKEN_UNION_KAD;
	at->afstoken_soliton_u.at_kad.rk_primary_flag = primflag;
	if (!(at->afstoken_soliton_u.at_kad.rk_ticket.rk_ticket_val = malloc(tktLen)))
	    goto Done;
	at->afstoken_soliton_u.at_kad.rk_ticket.rk_ticket_len = tktLen;
	memcpy(at->afstoken_soliton_u.at_kad.rk_ticket.rk_ticket_val, stp, tktLen);
	at->afstoken_soliton_u.at_kad.rk_kvno = ct.AuthHandle;
	at->afstoken_soliton_u.at_kad.rk_viceid = ct.ViceId;
	memcpy(at->afstoken_soliton_u.at_kad.rk_key, ct.HandShakeKey, 8);
	at->afstoken_soliton_u.at_kad.rk_begintime = ct.BeginTimestamp;
	at->afstoken_soliton_u.at_kad.rk_endtime = ct.EndTimestamp;
	code = add_afs_token_soliton(a_token, at);
	goto Done;
    }

    if (code) {
Failed:
	/* failed to retrieve specified token */
	if (code < 0) switch(code = errno) {
	case EDOM:
	case ENOTCONN:
	    code = KTC_NOENT;
	    break;
	case EIO:
	    code = KTC_NOCM;
	    break;
	}
    } else {
	    /* now we're cookin with gas */		
	    code = parse_afs_token(iob.out, iob.out_size, a_token);		
    }
Done:
    UNLOCK_GLOBAL_MUTEX;
    xdrs->x_op = XDR_FREE;
    xdr_afstoken_soliton(xdrs, at);
    return code;
}

int
ktc_GetToken(struct ktc_principal *server, struct ktc_token *token,
	     int tokenLen, struct ktc_principal *client)
{
    struct ViceIoctl iob;
    char tbuffer[TBUFFERSIZE];
    char *tp, *cp;
    char *ticketP;
    int ticketLen, temp;
    struct ClearToken ct;
    char *cellName;
    int cellNameSize;
    int maxLen;
    int code;
    RPC_STATUS status;
    afs_uuid_t uuid;
    HANDLE ktcMutex = NULL;

    tp = tbuffer;

    /* check to see if the user is requesting tokens for a principal
     * other than afs.  If so, check the local token cache.
     */
    if (strcmp(server->name, "afs")) {
	return GetLocalToken(server, token, tokenLen, client);
    }

    /* cell name */
    strcpy(tp, server->cell);
    tp += strlen(server->cell) + 1;

    /* uuid */
    status = UuidCreate((UUID *) & uuid);
    memcpy(tp, &uuid, sizeof(uuid));
    tp += sizeof(uuid);

    iob.in = tbuffer;
    iob.in_size = (long)(tp - tbuffer);
    iob.out = tbuffer;
    iob.out_size = sizeof(tbuffer);

#ifndef AFS_WIN95_ENV
    ktcMutex = CreateMutex(NULL, TRUE, AFSGlobalKTCMutexName);
    if (ktcMutex == NULL)
	return KTC_PIOCTLFAIL;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
	if (WaitForSingleObject(ktcMutex, INFINITE) != WAIT_OBJECT_0) {
	    CloseHandle(ktcMutex);
	    return KTC_PIOCTLFAIL;
	}
    }
#endif /* AFS_WIN95_ENV */

    code = pioctl(0, VIOCNEWGETTOK, &iob, 0);
    if (code) {
#ifndef AFS_WIN95_ENV
	ReleaseMutex(ktcMutex);
	CloseHandle(ktcMutex);
#endif /* AFS_WIN95_ENV */
	if (code == -1) {
	    if (errno == ESRCH)
		return KTC_NOCELL;
	    else if (errno == ENODEV)
		return KTC_NOCM;
	    else if (errno == EINVAL)
		return KTC_INVAL;
	    else if (errno == EDOM)
		return KTC_NOENT;
	    else
		return KTC_PIOCTLFAIL;
	} else
	    return KTC_PIOCTLFAIL;
    }
#ifndef AFS_WIN95_ENV
    /* get rid of RPC for win95 build */
    /* RPC to receive session key */
    status = receive_key(uuid, token->sessionKey.data);

    ReleaseMutex(ktcMutex);
    CloseHandle(ktcMutex);

    if (status != RPC_S_OK) {
	if (status == 1)
	    strcpy(rpcErr, "RPC failure in AFS gateway");
	else
	    DceErrorInqText(status, rpcErr);
	if (status == RPC_S_SERVER_UNAVAILABLE
	    || status == EPT_S_NOT_REGISTERED)
	    return KTC_NOCMRPC;
	else
	    return KTC_RPC;
    }
#endif /* AFS_WIN95_ENV */

    cp = tbuffer;

    /* ticket length */
    memcpy(&ticketLen, cp, sizeof(ticketLen));
    cp += sizeof(ticketLen);

    /* remember where ticket is and skip over it */
    ticketP = cp;
    cp += ticketLen;

    /* size of clear token */
    memcpy(&temp, cp, sizeof(temp));
    cp += sizeof(temp);
    if (temp != sizeof(ct))
	return KTC_ERROR;

    /* clear token */
    memcpy(&ct, cp, temp);
    cp += temp;

    /* skip over primary flag */
    cp += sizeof(temp);

    /* remember cell name and skip over it */
    cellName = cp;
    cellNameSize = (int)strlen(cp);
    cp += cellNameSize + 1;

    /* user name is here */

    /* check that ticket will fit 
     * this compares the size of the ktc_token allocated by the app
     * which might be smaller than the current definition of MAXKTCTICKETLEN
     */
    maxLen = tokenLen - sizeof(struct ktc_token) + MAXKTCTICKETLEN;
    if (maxLen < ticketLen)
	return KTC_TOOBIG;

    /* set return values */
    memcpy(token->ticket, ticketP, ticketLen);
    token->startTime = ct.BeginTimestamp;
    token->endTime = ct.EndTimestamp;
    if (ct.AuthHandle == -1)
	ct.AuthHandle = 999;
    token->kvno = ct.AuthHandle;
#ifndef AFS_WIN95_ENV
    /*
     * Session key has already been set via RPC
     */
#else
    memcpy(&token->sessionKey, ct.HandShakeKey, sizeof(ct.HandShakeKey));
#endif /* AFS_WIN95_ENV */
    token->ticketLen = ticketLen;
    if (client) {
	strcpy(client->name, cp);
	client->instance[0] = '\0';
	strcpy(client->cell, cellName);
    }

    return 0;
}

int
ktc_ListTokens(int cellNum, int *cellNumP, struct ktc_principal *server)
{
    struct ViceIoctl iob;
    char tbuffer[TBUFFERSIZE];
    char *tp, *cp;
    int newIter, ticketLen, temp;
    int code;
    HANDLE ktcMutex = NULL;

#ifndef AFS_WIN95_ENV
    ktcMutex = CreateMutex(NULL, TRUE, AFSGlobalKTCMutexName);
    if (ktcMutex == NULL)
	return KTC_PIOCTLFAIL;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
	if (WaitForSingleObject(ktcMutex, INFINITE) != WAIT_OBJECT_0) {
	    CloseHandle(ktcMutex);
	    return KTC_PIOCTLFAIL;
	}
    }
#endif /* AFS_WIN95_ENV */

    tp = tbuffer;

    /* iterator */
    memcpy(tp, &cellNum, sizeof(cellNum));
    tp += sizeof(cellNum);

    /* do pioctl */
    iob.in = tbuffer;
    iob.in_size = (long)(tp - tbuffer);
    iob.out = tbuffer;
    iob.out_size = sizeof(tbuffer);

    code = pioctl(0, VIOCGETTOK, &iob, 0);

#ifndef AFS_WIN95_ENV
    ReleaseMutex(ktcMutex);
    CloseHandle(ktcMutex);
#endif /* AFS_WIN95_ENV */

    if (code) {
	if (code == -1) {
	    if (errno == ESRCH)
		return KTC_NOCELL;
	    else if (errno == ENODEV)
		return KTC_NOCM;
	    else if (errno == EINVAL)
		return KTC_INVAL;
	    else if (errno == EDOM)
		return KTC_NOENT;
	    else
		return KTC_PIOCTLFAIL;
	} else
	    return KTC_PIOCTLFAIL;
    }

    cp = tbuffer;

    /* new iterator */
    memcpy(&newIter, cp, sizeof(newIter));
    cp += sizeof(newIter);

    /* ticket length */
    memcpy(&ticketLen, cp, sizeof(ticketLen));
    cp += sizeof(ticketLen);

    /* skip over ticket */
    cp += ticketLen;

    /* clear token size */
    memcpy(&temp, cp, sizeof(temp));
    cp += sizeof(temp);
    if (temp != sizeof(struct ClearToken))
	return KTC_ERROR;

    /* skip over clear token */
    cp += sizeof(struct ClearToken);

    /* skip over primary flag */
    cp += sizeof(temp);

    /* cell name is here */

    /* set return values */
    strcpy(server->cell, cp);
    server->instance[0] = '\0';
    strcpy(server->name, "afs");

    *cellNumP = newIter;
    return 0;
}

int
ktc_ForgetToken(struct ktc_principal *server)
{
    struct ViceIoctl iob;
    char tbuffer[TBUFFERSIZE];
    char *tp;
    int code;
    HANDLE ktcMutex = NULL;

    if (strcmp(server->name, "afs")) {
	return ForgetOneLocalToken(server);
    }
#ifndef AFS_WIN95_ENV
    ktcMutex = CreateMutex(NULL, TRUE, AFSGlobalKTCMutexName);
    if (ktcMutex == NULL)
	return KTC_PIOCTLFAIL;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
	if (WaitForSingleObject(ktcMutex, INFINITE) != WAIT_OBJECT_0) {
	    CloseHandle(ktcMutex);
	    return KTC_PIOCTLFAIL;
	}
    }
#endif /* AFS_WIN95_ENV */

    tp = tbuffer;

    /* cell name */
    strcpy(tp, server->cell);
    tp += strlen(tp) + 1;

    /* do pioctl */
    iob.in = tbuffer;
    iob.in_size = (long)(tp - tbuffer);
    iob.out = tbuffer;
    iob.out_size = sizeof(tbuffer);

    code = pioctl(0, VIOCDELTOK, &iob, 0);
#ifndef AFS_WIN95_ENV
    ReleaseMutex(ktcMutex);
    CloseHandle(ktcMutex);
#endif /* AFS_WIN95_ENV */
    if (code) {
	if (code == -1) {
	    if (errno == ESRCH)
		return KTC_NOCELL;
	    else if (errno == EDOM)
		return KTC_NOENT;
	    else if (errno == ENODEV)
		return KTC_NOCM;
	    else
		return KTC_PIOCTLFAIL;
	} else
	    return KTC_PIOCTLFAIL;
    }
    return 0;
}

int
ktc_ForgetAllTokens()
{
    struct ViceIoctl iob;
    char tbuffer[TBUFFERSIZE];
    int code;
    HANDLE ktcMutex = NULL;

    (void)ForgetLocalTokens();

#ifndef AFS_WIN95_ENV
    ktcMutex = CreateMutex(NULL, TRUE, AFSGlobalKTCMutexName);
    if (ktcMutex == NULL)
	return KTC_PIOCTLFAIL;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
	if (WaitForSingleObject(ktcMutex, INFINITE) != WAIT_OBJECT_0) {
	    CloseHandle(ktcMutex);
	    return KTC_PIOCTLFAIL;
	}
    }
#endif /* AFS_WIN95_ENV */

    /* do pioctl */
    iob.in = tbuffer;
    iob.in_size = 0;
    iob.out = tbuffer;
    iob.out_size = sizeof(tbuffer);

    code = pioctl(0, VIOCDELALLTOK, &iob, 0);
#ifndef AFS_WIN95_ENV
    ReleaseMutex(ktcMutex);
    CloseHandle(ktcMutex);
#endif /* AFS_WIN95_ENV */
    if (code) {
	if (code == -1) {
	    if (errno == ENODEV)
		return KTC_NOCM;
	    else
		return KTC_PIOCTLFAIL;
	} else
	    return KTC_PIOCTLFAIL;
    }
    return 0;
}

int
ktc_OldPioctl()
{
    return 1;
}


#define MAXLOCALTOKENS 4

static struct {
    int valid;
    struct ktc_principal server;
    struct ktc_principal client;
    struct ktc_token token;
} local_tokens[MAXLOCALTOKENS] = {
0};

static int
SetLocalToken(struct ktc_principal *aserver, struct ktc_token *atoken,
	      struct ktc_principal *aclient, afs_int32 flags)
{
    int found = -1;
    int i;

    LOCK_GLOBAL_MUTEX;
    for (i = 0; i < MAXLOCALTOKENS; i++)
	if (local_tokens[i].valid) {
	    if ((strcmp(local_tokens[i].server.name, aserver->name) == 0)
		&& (strcmp(local_tokens[i].server.instance, aserver->instance)
		    == 0)
		&& (strcmp(local_tokens[i].server.cell, aserver->cell) == 0)) {
		found = i;	/* replace existing entry */
		break;
	    }
	} else if (found == -1)
	    found = i;		/* remember empty slot but keep looking for a match */
    if (found == -1) {
	UNLOCK_GLOBAL_MUTEX;
	return KTC_NOENT;
    }
    memcpy(&local_tokens[found].token, atoken, sizeof(struct ktc_token));
    memcpy(&local_tokens[found].server, aserver,
	   sizeof(struct ktc_principal));
    memcpy(&local_tokens[found].client, aclient,
	   sizeof(struct ktc_principal));
    local_tokens[found].valid = 1;
    UNLOCK_GLOBAL_MUTEX;
    return 0;
}


static int
GetLocalToken(struct ktc_principal *aserver, struct ktc_token *atoken,
	      int atokenLen, struct ktc_principal *aclient)
{
    int i;

    LOCK_GLOBAL_MUTEX;
    for (i = 0; i < MAXLOCALTOKENS; i++)
	if (local_tokens[i].valid
	    && (strcmp(local_tokens[i].server.name, aserver->name) == 0)
	    && (strcmp(local_tokens[i].server.instance, aserver->instance) ==
		0)
	    && (strcmp(local_tokens[i].server.cell, aserver->cell) == 0)) {
	    memcpy(atoken, &local_tokens[i].token,
		   min(atokenLen, sizeof(struct ktc_token)));
	    memcpy(aclient, &local_tokens[i].client,
		   sizeof(struct ktc_principal));
	    UNLOCK_GLOBAL_MUTEX;
	    return 0;
	}
    UNLOCK_GLOBAL_MUTEX;
    return KTC_NOENT;
}


static int
ForgetLocalTokens()
{
    int i;

    LOCK_GLOBAL_MUTEX;
    for (i = 0; i < MAXLOCALTOKENS; i++) {
	local_tokens[i].valid = 0;
	memset(&local_tokens[i].token.sessionKey, 0,
	       sizeof(struct ktc_encryptionKey));
    }
    UNLOCK_GLOBAL_MUTEX;
    return 0;
}


static int
ForgetOneLocalToken(struct ktc_principal *aserver)
{
    int i;

    LOCK_GLOBAL_MUTEX;
    for (i = 0; i < MAXLOCALTOKENS; i++) {
	if (local_tokens[i].valid
	    && (strcmp(local_tokens[i].server.name, aserver->name) == 0)
	    && (strcmp(local_tokens[i].server.instance, aserver->instance) ==
		0)
	    && (strcmp(local_tokens[i].server.cell, aserver->cell) == 0)) {
	    local_tokens[i].valid = 0;
	    memset(&local_tokens[i].token.sessionKey, 0,
		   sizeof(struct ktc_encryptionKey));
	    UNLOCK_GLOBAL_MUTEX;
	    return 0;
	}
    }
    UNLOCK_GLOBAL_MUTEX;
    return KTC_NOENT;
}
