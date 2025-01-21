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
#include <afs/stds.h>

#include <roken.h>

#include <ctype.h>

#include <afs/opr.h>
#include <afs/pthread_glock.h>
#include <rpc.h>
#include <afs/smb_iocons.h>
#include <afs/pioctl_nt.h>
#include "afs/afsrpc.h"
#include <afs/vice.h>
#include "auth.h"
#include <afs/afsutil.h>
#include "token.h"

/* TBUFFERSIZE must be at least 512 larger than KTCMAXTICKETSIZE */
#define TBUFFERSIZE 12512

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

#define MAXPIOCTLTOKENLEN \
(3*sizeof(afs_int32)+MAXKTCTICKETLEN+sizeof(struct ClearToken)+MAXKTCREALMLEN)

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
    return malloc(cBytes);
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
	if (!_stricmp(encrypt, "OFF"))
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
	if (!_stricmp(encrypt, "OFF"))
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

int
ktc_SetToken(struct ktc_principal *server, struct ktc_token *token,
	     struct ktc_principal *client, int flags)
{
    struct ViceIoctl iob;
    char tbuffer[TBUFFERSIZE];
    int len;
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
    len = sizeof(token->ticketLen);

    /* ticket */
    if (len + token->ticketLen > TBUFFERSIZE)
        return KTC_INVAL;
    memcpy(tp, token->ticket, token->ticketLen);
    tp += token->ticketLen;
    len += token->ticketLen;

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
    if (len + sizeof(temp) > TBUFFERSIZE)
        return KTC_INVAL;
    temp = sizeof(struct ClearToken);
    memcpy(tp, &temp, sizeof(temp));
    tp += sizeof(temp);
    len += sizeof(temp);

    /* clear token itself */
    if (len + sizeof(ct) > TBUFFERSIZE)
        return KTC_INVAL;
    memcpy(tp, &ct, sizeof(ct));
    tp += sizeof(ct);
    len += sizeof(ct);

    /* flags; on NT there is no setpag flag, but there is an
     * integrated logon flag */
    if (len + sizeof(temp) > TBUFFERSIZE)
        return KTC_INVAL;
    temp = ((flags & AFS_SETTOK_LOGON) ? PIOCTL_LOGON : 0);
    memcpy(tp, &temp, sizeof(temp));
    tp += sizeof(temp);
    len += sizeof(temp);

    /* cell name */
    temp = (int)strlen(server->cell) + 1;
    if (len + temp > TBUFFERSIZE ||
        temp > MAXKTCREALMLEN)
	return KTC_INVAL;
    strcpy(tp, server->cell);
    tp += temp;
    len += temp;

    /* user name */
    temp = (int)strlen(client->name) + 1;
    if (len + temp > TBUFFERSIZE ||
        temp > MAXKTCNAMELEN)
	return KTC_INVAL;
    strcpy(tp, client->name);
    tp += temp;
    len += temp;

    /* we need the SMB user name to associate the tokens with in the
     * integrated logon case. */
    if (flags & AFS_SETTOK_LOGON) {
	if (client->smbname == NULL)
	    temp = 1;
	else
	    temp = (int)strlen(client->smbname) + 1;
	if (temp == 1 ||
            len + temp > TBUFFERSIZE ||
            temp > MAXKTCNAMELEN)
	    return KTC_INVAL;
	strcpy(tp, client->smbname);
	tp += temp;
        len += temp;
    }

    /* uuid */
    if (len + sizeof(uuid) > TBUFFERSIZE)
        return KTC_INVAL;
    status = UuidCreate((UUID *) & uuid);
    memcpy(tp, &uuid, sizeof(uuid));
    tp += sizeof(uuid);
    len += sizeof(uuid);

    ktcMutex = CreateMutex(NULL, TRUE, AFSGlobalKTCMutexName);
    if (ktcMutex == NULL)
	return KTC_TOKEN_MUTEX_FAIL;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
	if (WaitForSingleObject(ktcMutex, INFINITE) != WAIT_OBJECT_0) {
	    CloseHandle(ktcMutex);
            return KTC_TOKEN_MUTEX_FAIL;
	}
    }

    /* RPC to send session key */
    status = send_key(uuid, token->sessionKey.data);
    if (status != RPC_S_OK) {
	if (status == 1)
	    strcpy(rpcErr, "RPC failure in AFS gateway");
	else
	    DceErrorInqText(status, rpcErr);

        ReleaseMutex(ktcMutex);
        CloseHandle(ktcMutex);

        if (status == RPC_S_SERVER_UNAVAILABLE ||
	    status == EPT_S_NOT_REGISTERED) {
	    return KTC_NOCMRPC;
	} else {
	    return KTC_RPC;
	}
    }

    /* set up for pioctl */
    iob.in = tbuffer;
    iob.in_size = (long)(tp - tbuffer);
    iob.out = tbuffer;
    iob.out_size = sizeof(tbuffer);

    code = pioctl(0, VIOCSETTOK, &iob, 0);

    ReleaseMutex(ktcMutex);
    CloseHandle(ktcMutex);

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

int
ktc_SetTokenEx(struct ktc_setTokenData *token)
{
    /* Not yet implemented */
    return KTC_PIOCTLFAIL;
}

int
ktc_GetToken(struct ktc_principal *server, struct ktc_token *token,
	     int tokenLen, struct ktc_principal *client)
{
    struct ViceIoctl iob;
    char tbuffer[TBUFFERSIZE];
    size_t len;
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
    len = strlen(server->cell) + 1;
    strcpy(tp, server->cell);
    tp += len;

    /* uuid */
    status = UuidCreate((UUID *) & uuid);
    memcpy(tp, &uuid, sizeof(uuid));
    tp += sizeof(uuid);
    len += sizeof(uuid);

    iob.in = tbuffer;
    iob.in_size = (long)(tp - tbuffer);
    iob.out = tbuffer;
    iob.out_size = sizeof(tbuffer);

    ktcMutex = CreateMutex(NULL, TRUE, AFSGlobalKTCMutexName);
    if (ktcMutex == NULL)
	return KTC_TOKEN_MUTEX_FAIL;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
	if (WaitForSingleObject(ktcMutex, INFINITE) != WAIT_OBJECT_0) {
	    CloseHandle(ktcMutex);
            return KTC_TOKEN_MUTEX_FAIL;
	}
    }

    code = pioctl(0, VIOCNEWGETTOK, &iob, 0);
    if (code) {
	ReleaseMutex(ktcMutex);
	CloseHandle(ktcMutex);

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

    /* RPC to receive session key */
    status = receive_key(uuid, token->sessionKey.data);

    ReleaseMutex(ktcMutex);
    CloseHandle(ktcMutex);

    if (status != RPC_S_OK) {
        if (status == 1)
            strcpy(rpcErr, "RPC failure in AFS gateway");
        else
            DceErrorInqText(status, rpcErr);

        if (status == RPC_S_SERVER_UNAVAILABLE ||
            status == EPT_S_NOT_REGISTERED)
            return KTC_NOCMRPC;
        else
            return KTC_RPC;
    }

    cp = tbuffer;

    /* ticket length */
    memcpy(&ticketLen, cp, sizeof(ticketLen));
    cp += sizeof(ticketLen);
    len = sizeof(ticketLen);

    /* remember where ticket is and skip over it */
    if (len + ticketLen > TBUFFERSIZE ||
        len + ticketLen > iob.out_size)
        return KTC_ERROR;
    ticketP = cp;
    cp += ticketLen;
    len += ticketLen;

    /* size of clear token */
    if (len + sizeof(temp) > TBUFFERSIZE ||
        len + sizeof(temp) > iob.out_size)
        return KTC_ERROR;
    memcpy(&temp, cp, sizeof(temp));
    cp += sizeof(temp);
    len += sizeof(temp);
    if (temp != sizeof(ct))
	return KTC_ERROR;

    /* clear token */
    if (len + temp > TBUFFERSIZE ||
        len + temp > iob.out_size)
        return KTC_ERROR;
    memcpy(&ct, cp, temp);
    cp += temp;
    len += temp;

    /* skip over primary flag */
    if (len + sizeof(temp) > TBUFFERSIZE ||
        len + sizeof(temp) > iob.out_size)
        return KTC_ERROR;
    cp += sizeof(temp);
    len += sizeof(temp);

    /* remember cell name and skip over it */
    cellName = cp;
    cellNameSize = (int)strlen(cp);
    if (len + cellNameSize + 1 > TBUFFERSIZE ||
        len + cellNameSize + 1 > iob.out_size)
        return KTC_ERROR;
    cp += cellNameSize + 1;
    len += cellNameSize + 1;

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

/*!
 * Get a token, given the cell that we need to get information for
 *
 * @param cellName
 * 	The name of the cell we're getting the token for - if NULL, we'll
 * 	get information for the primary cell
 */
int
ktc_GetTokenEx(char *cellName, struct ktc_setTokenData **tokenSet) {
    struct ViceIoctl iob;
    char tbuffer[MAXPIOCTLTOKENLEN];
    char *tp;
    afs_int32 code;
    XDR xdrs;
    HANDLE ktcMutex = NULL;

    tp = tbuffer;

    /* If we have a cellName, write it out here */
    if (cellName) {
	memcpy(tp, cellName, strlen(cellName) +1);
	tp += strlen(cellName)+1;
    }

    iob.in = tbuffer;
    iob.in_size = tp - tbuffer;
    iob.out = tbuffer;
    iob.out_size = sizeof(tbuffer);

    ktcMutex = CreateMutex(NULL, TRUE, AFSGlobalKTCMutexName);
    if (ktcMutex == NULL)
	return KTC_TOKEN_MUTEX_FAIL;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
	if (WaitForSingleObject(ktcMutex, INFINITE) != WAIT_OBJECT_0) {
	    CloseHandle(ktcMutex);
            return KTC_TOKEN_MUTEX_FAIL;
	}
    }

    code = -1;   /* VIOC_GETTOK2 not yet implemented */
    errno = EINVAL;

    ReleaseMutex(ktcMutex);
    CloseHandle(ktcMutex);

    /* If we can't use the new pioctl, the fall back to the old one. We then
     * need to convert the rxkad token we get back into the new format
     */
    if (code == -1 && errno == EINVAL) {
	struct ktc_principal server;
	struct ktc_principal client;
	struct ktc_tokenUnion token;
	struct ktc_token *ktcToken; /* too huge for the stack */

	memset(&server, 0, sizeof(server));
	ktcToken = malloc(sizeof(struct ktc_token));
	if (ktcToken == NULL)
	    return ENOMEM;
	memset(ktcToken, 0, sizeof(struct ktc_token));

	strcpy(server.name, "afs");
	strcpy(server.cell, cellName);
	code = ktc_GetToken(&server, ktcToken, sizeof(struct ktc_token),
			    &client);
	if (code == 0) {
	    *tokenSet = token_buildTokenJar(cellName);
	    token.at_type = AFSTOKEN_UNION_KAD;
	    token.ktc_tokenUnion_u.at_kad.rk_kvno = ktcToken->kvno;
	    memcpy(token.ktc_tokenUnion_u.at_kad.rk_key,
		   ktcToken->sessionKey.data, 8);

	    token.ktc_tokenUnion_u.at_kad.rk_begintime = ktcToken->startTime;
	    token.ktc_tokenUnion_u.at_kad.rk_endtime   = ktcToken->endTime;
	    token.ktc_tokenUnion_u.at_kad.rk_ticket.rk_ticket_len
		= ktcToken->ticketLen;
	    token.ktc_tokenUnion_u.at_kad.rk_ticket.rk_ticket_val
		= ktcToken->ticket;

	    token_addToken(*tokenSet, &token);

	    memset(ktcToken, 0, sizeof(struct ktc_token));
	}
	free(ktcToken);
	return code;
    }
    if (code)
	return KTC_PIOCTLFAIL;

    *tokenSet = malloc(sizeof(struct ktc_setTokenData));
    if (*tokenSet == NULL)
	return ENOMEM;
    memset(*tokenSet, 0, sizeof(struct ktc_setTokenData));

    xdrmem_create(&xdrs, iob.out, iob.out_size, XDR_DECODE);
    if (!xdr_ktc_setTokenData(&xdrs, *tokenSet)) {
	free(*tokenSet);
	*tokenSet = NULL;
	xdr_destroy(&xdrs);
	return EINVAL;
    }
    xdr_destroy(&xdrs);
    return 0;
}

int
ktc_ListTokens(int cellNum, int *cellNumP, struct ktc_principal *server)
{
    struct ViceIoctl iob;
    char tbuffer[TBUFFERSIZE];
    int len;
    char *tp, *cp;
    int newIter, ticketLen, temp;
    int code;
    HANDLE ktcMutex = NULL;

    ktcMutex = CreateMutex(NULL, TRUE, AFSGlobalKTCMutexName);
    if (ktcMutex == NULL)
	return KTC_TOKEN_MUTEX_FAIL;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
	if (WaitForSingleObject(ktcMutex, INFINITE) != WAIT_OBJECT_0) {
	    CloseHandle(ktcMutex);
            return KTC_TOKEN_MUTEX_FAIL;
	}
    }

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

    ReleaseMutex(ktcMutex);
    CloseHandle(ktcMutex);

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
    len = sizeof(newIter);

    /* ticket length */
    if (len + sizeof(ticketLen) > TBUFFERSIZE ||
        len + sizeof(ticketLen) > iob.out_size)
        return KTC_ERROR;
    memcpy(&ticketLen, cp, sizeof(ticketLen));
    cp += sizeof(ticketLen);
    len += sizeof(ticketLen);

    /* skip over ticket */
    cp += ticketLen;
    len += ticketLen;

    /* clear token size */
    if (len + sizeof(temp) > TBUFFERSIZE ||
        len + sizeof(temp) > iob.out_size)
        return KTC_ERROR;
    memcpy(&temp, cp, sizeof(temp));
    cp += sizeof(temp);
    len += sizeof(temp);
    if (temp != sizeof(struct ClearToken))
	return KTC_ERROR;

    /* skip over clear token */
    cp += sizeof(struct ClearToken);
    len += sizeof(struct ClearToken);

    /* skip over primary flag */
    cp += sizeof(temp);
    len += sizeof(temp);
    if (len > TBUFFERSIZE ||
        len > iob.out_size)
        return KTC_ERROR;

    /* cell name is here */

    /* set return values */
    if (len + temp > TBUFFERSIZE ||
        temp > MAXKTCREALMLEN)
        return KTC_ERROR;
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
    ktcMutex = CreateMutex(NULL, TRUE, AFSGlobalKTCMutexName);
    if (ktcMutex == NULL)
	return KTC_TOKEN_MUTEX_FAIL;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
	if (WaitForSingleObject(ktcMutex, INFINITE) != WAIT_OBJECT_0) {
	    CloseHandle(ktcMutex);
            return KTC_TOKEN_MUTEX_FAIL;
	}
    }

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
    ReleaseMutex(ktcMutex);
    CloseHandle(ktcMutex);

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

    ktcMutex = CreateMutex(NULL, TRUE, AFSGlobalKTCMutexName);
    if (ktcMutex == NULL)
	return KTC_TOKEN_MUTEX_FAIL;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
	if (WaitForSingleObject(ktcMutex, INFINITE) != WAIT_OBJECT_0) {
	    CloseHandle(ktcMutex);
            return KTC_TOKEN_MUTEX_FAIL;
	}
    }

    /* do pioctl */
    iob.in = tbuffer;
    iob.in_size = 0;
    iob.out = tbuffer;
    iob.out_size = sizeof(tbuffer);

    code = pioctl(0, VIOCDELALLTOK, &iob, 0);
    ReleaseMutex(ktcMutex);
    CloseHandle(ktcMutex);

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
		   opr_min(atokenLen, sizeof(struct ktc_token)));
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

/*!
 * An iterator which can list all cells with tokens in the cache
 *
 * This function may be used to list the names of all cells for which
 * tokens exist in the current cache. The first time that it is called,
 * prevIndex should be set to 0. On all subsequent calls, prevIndex
 * should be set to the value returned in newIndex by the last call
 * to the function. Note that there is no guarantee that the index value
 * is monotonically increasing.
 *
 * @param prevIndex
 * 	The index returned by the last call, or 0 if this is the first
 * 	call in an iteration
 * @param newIndex
 * 	A pointer to an int which, upon return, will hold the next value
 * 	to be used.
 * @param cellName
 * 	A pointer to a char * which, upon return, will hold a cellname.
 * 	This must be freed by the caller using free()
 */

int
ktc_ListTokensEx(int prevIndex, int *newIndex, char **cellName) {
    struct ViceIoctl iob;
    char tbuffer[MAXPIOCTLTOKENLEN];
    afs_int32 code;
    afs_int32 index;
    struct ktc_setTokenData tokenSet;
    XDR xdrs;
    HANDLE ktcMutex = NULL;

    memset(&tokenSet, 0, sizeof(tokenSet));

    *cellName = NULL;
    *newIndex = prevIndex;

    index = prevIndex;

    while (index<100) { /* Safety, incase of pioctl failure */
	memset(tbuffer, 0, sizeof(tbuffer));
	iob.in = tbuffer;
	memcpy(tbuffer, &index, sizeof(afs_int32));
	iob.in_size = sizeof(afs_int32);
	iob.out = tbuffer;
	iob.out_size = sizeof(tbuffer);

	code = -1;      /* VIOC_GETTOK2 not yet implemented */
	errno = EINVAL;

	/* Can't use new pioctl, so must use old one */
	if (code == -1 && errno == EINVAL) {
	    struct ktc_principal server;

	    code = ktc_ListTokens(index, newIndex, &server);
	    if (code == 0)
		*cellName = strdup(server.cell);
	    return code;
	}

	if (code == 0) {
	    /* Got a token from the pioctl. Now we throw it away,
	     * so we can return just a cellname. This is rather wasteful,
	     * but it's what the old API does. Ho hum.  */

	    xdrmem_create(&xdrs, iob.out, iob.out_size, XDR_DECODE);
	    if (!xdr_ktc_setTokenData(&xdrs, &tokenSet)) {
		xdr_destroy(&xdrs);
		return EINVAL;
	    }
	    xdr_destroy(&xdrs);
	    *cellName = strdup(tokenSet.cell);
	    token_FreeSetContents(&tokenSet);
	    *newIndex = index + 1;
	    return 0;
	}
	index++;
    }
    return KTC_PIOCTLFAIL;
}

