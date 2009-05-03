/*
 * Copyright (c) 2005, 2006
 * The Linux Box Corporation
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the Linux Box
 * Corporation is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * Linux Box Corporation is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the Linux Box Corporation as to its fitness for any
 * purpose, and without warranty by the Linux Box Corporation
 * of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the Linux Box Corporation shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#include <afsconfig.h>
#if defined(KERNEL)
# include "afs/param.h"
# include "afs/sysincludes.h"
# include "afsincludes.h"
# include "afs_stats.h"
# include "auth.h"
# include "afs_token.h"
#else	/* !KERNEL */
#define afs_osi_Alloc(n)	malloc(n)
#define afs_osi_Free(p,n)	free(p)
#define afs_strdup(p) strdup(p)
# include <afs/afsutil.h>
# include "cellconfig.h"
# include "auth.p.h"
# include <stdlib.h>
# include <stdarg.h>
# include <string.h>
# include <stdio.h>
# include <sys/types.h>
# include <sys/stat.h>
#  ifndef AFS_NT40_ENV
#    include <syslog.h>
#    include <unistd.h>
#  endif
# include <errno.h>
# include <afs/afs_token.h>
#endif	/* !KERNEL */
#include "afs_token_protos.h"
#include "rx/rx.h"

struct genericlist {
    int len;
    char *val;
};

void *
afs_addlenval(void *arp, void *ptr, int size)
{
    struct genericlist *rp = arp;
    int atstart, os;
    char *newval, *oldval, *result;

    if ((atstart = (size < 0)))
	size = -size;
    os = size*rp->len;
    newval = afs_osi_Alloc(os + size);
    if (!newval) return 0;
    if (atstart) {
	memcpy(result = newval, ptr, size);
	if (os)
	    memcpy(newval+size, rp->val, os);
    } else {
	if (os)
	    memcpy(newval, rp->val, os);
	memcpy(result = newval + os,
	    ptr, size);
    }
    oldval = rp->val;
    rp->val = newval;
    ++rp->len;
    if (os)
	afs_osi_Free(oldval, os);
    return result;
}

int
add_afs_token_soliton(pioctl_set_token *a_token,
    afstoken_soliton *at)
{
#define WORKSIZE 16384
    char *temp = 0;
    token_opaque x[1];
    int code;
    XDR xdrs[1];

    memset(x, 0, sizeof *x);
    temp = afs_osi_Alloc(WORKSIZE);
    if (!temp) {
	code = ENOMEM;
	goto Failed;
    }
    xdrmem_create(xdrs, temp, WORKSIZE, XDR_ENCODE);
    if (!xdr_afstoken_soliton(xdrs, at)) {
	code = EINVAL;
	goto Failed;
    }
    x->token_opaque_len = xdr_getpos(xdrs);
    x->token_opaque_val = afs_osi_Alloc(x->token_opaque_len);
    if (!x->token_opaque_val) {
	code = ENOMEM;
	goto Failed;
    }
    memcpy(x->token_opaque_val, temp, x->token_opaque_len);
    if (afs_addlenval(&a_token->tokens, x, sizeof *x)) {
	code = 0;
	x->token_opaque_val = 0;
    } else code = ENOMEM;
Failed:
    if (x->token_opaque_val)
	afs_osi_Free(x->token_opaque_val, x->token_opaque_len);
    if (temp) afs_osi_Free(temp, WORKSIZE);
    return code;
}

#if defined(KERNEL) || defined(AFS_NT40_ENV)
/*
 * Format new-style afs_token using rxkad credentials 
 * as stored in the cache manager.  Caller frees returned memory 
 * (of size bufsize).
 */
int add_afs_token_rxkad_k(struct ClearToken *pct,
    char* stp,
    afs_int32 stLen,
    afs_int32 primary_flag,
    pioctl_set_token *a_token /* out */)
{
    afstoken_soliton at[1];
    token_rxkad *kad_token = &at->afstoken_soliton_u.at_kad;

    memset(at, 0, sizeof *at);
    at->at_type = AFSTOKEN_UNION_KAD;

    kad_token->rk_primary_flag = primary_flag;
    kad_token->rk_ticket.rk_ticket_len = stLen;
    kad_token->rk_ticket.rk_ticket_val = afs_osi_Alloc(kad_token->rk_ticket.rk_ticket_len);
    memcpy(kad_token->rk_ticket.rk_ticket_val, stp, kad_token->rk_ticket.rk_ticket_len);
    kad_token->rk_kvno = pct->AuthHandle;
    memcpy(kad_token->rk_key, pct->HandShakeKey, 8);
    kad_token->rk_viceid = pct->ViceId;
    kad_token->rk_begintime = pct->BeginTimestamp ;
    kad_token->rk_endtime = pct->EndTimestamp ;

    return add_afs_token_soliton(a_token, at);
}
#else	/* !KERNEL && !AFS_NT40_ENV */
/*
 * Format new-style afs_token using rxkad credentials, 
 * caller frees returned memory (of size bufsize).
 */
int add_afs_token_rxkad(afs_int32 viceid,
    struct ktc_token *k_token,
    afs_int32 primary_flag,
    pioctl_set_token *a_token /* out */)
{
    afstoken_soliton at[1];
    token_rxkad *kad_token = &at->afstoken_soliton_u.at_kad;

    memset(at, 0, sizeof *at);
    at->at_type = AFSTOKEN_UNION_KAD;

    kad_token->rk_primary_flag = primary_flag;
    kad_token->rk_ticket.rk_ticket_len = k_token->ticketLen;
    kad_token->rk_ticket.rk_ticket_val = afs_osi_Alloc(kad_token->rk_ticket.rk_ticket_len);
    memcpy(kad_token->rk_ticket.rk_ticket_val, k_token->ticket, 
	   kad_token->rk_ticket.rk_ticket_len);
    kad_token->rk_kvno = k_token->kvno;
    memcpy(kad_token->rk_key, &(k_token->sessionKey), 8);
    kad_token->rk_viceid = viceid;
    kad_token->rk_begintime = k_token->startTime;
    kad_token->rk_endtime = k_token->endTime;

    return add_afs_token_soliton(a_token, at);
}
#endif	/* KERNEL || AFS_NT40_ENV */

/* XXX need a better home for the following 3... */

/*
 * Convert afs_token to XDR-encoded token stream, which is returned
 * in buf (at most of size bufsize).  Caller must pass a sufficiently
 * large buffer.
 */
int
encode_afs_token(pioctl_set_token *a_token,
    void *buf /* in */,
    int *bufsize /* inout */)
{
    XDR xdrs[1];
    int r = -1;

    /* XDR encode afs_token into xdr_buf */
    xdrmem_create(xdrs, buf, *bufsize, XDR_ENCODE);
    if (!xdr_pioctl_set_token(xdrs, a_token))
	goto Done;

    /* and return a copy from the free store one */
    *bufsize = xdr_getpos(xdrs);
    r = 0;
Done:
    if (r) *bufsize = 0;
    return r;
}

/*
 * Convert XDR-encoded token stream to an afs_token, which is returned
 * in a_token.	Caller must free token contents.
 */
int
parse_afs_token(void* token_buf, 
    int token_size, 
    pioctl_set_token *a_token)
{
    XDR xdrs[1];
    int len;

    memset(a_token, 0, sizeof *a_token); /* not optional */

    /* XDR decode token_buf into a_token */
    xdrmem_create(xdrs, token_buf, token_size, XDR_DECODE);
    if (!xdr_int(xdrs, &len)) {
	return -1;
    }
    if (len-4 > (unsigned) token_size) {
	return -1;
    }
	/* XXX MSVC wont do arithmetic on a void pointer */
    xdrmem_create(xdrs, ((char*)token_buf)+4, len-4, XDR_DECODE);
    if (!xdr_pioctl_set_token(xdrs, a_token)) {
	return -1;
    }
    return 0;
}

/* 
 * Free afs_token variant using XDR logic 
*/
int
free_afs_token(pioctl_set_token *a_token)
{
    XDR xdrs[1];
    xdrs->x_op = XDR_FREE;
    if (!xdr_pioctl_set_token(xdrs, a_token)) {
	return 1;
    }
    return 0;
}

#if !defined(KERNEL) || defined(UKERNEL)
/* enumerate tokens for use in an unrecognized token types message. */
void
afs_get_tokens_type_msg(pioctl_set_token *atoken, char *msg, int msglen)
{
    int i, t;
    XDR xdrs[1];
    int l;
    snprintf(msg, msglen, "%d tokens:", atoken->tokens.tokens_len);
    l = strlen(msg);
    msglen -= l;
    msg += l;
    for (i = 0; i < atoken->tokens.tokens_len; ++i) {
	xdrmem_create(xdrs,
	    atoken->tokens.tokens_val[i].token_opaque_val,
	    atoken->tokens.tokens_val[i].token_opaque_len,
	    XDR_DECODE);
	if (!xdr_int(xdrs, &t)) {
	    snprintf(msg, msglen, " %d", t);
	    msglen -= l;
	    msg += l;
	    if (!l) break;
	}
    }
}
#endif

int
afstoken_to_soliton(pioctl_set_token *a_token,
    int type,
    afstoken_soliton *at)
{
    XDR xdrs[1];
    int i, t, code;

    code = EINVAL;
    memset(at, 0, sizeof *at);
    for (i = 0; i < a_token->tokens.tokens_len; ++i) {
	xdrmem_create(xdrs,
		a_token->tokens.tokens_val[i].token_opaque_val,
		a_token->tokens.tokens_val[i].token_opaque_len,
		XDR_DECODE);
	if (!xdr_int(xdrs, &t)) continue;
	if (t != type) continue;
	if (!xdr_setpos(xdrs, 0)) continue;
	if (!xdr_afstoken_soliton(xdrs, at) || at->at_type != type) {
	    goto Failed;
	}
	code = 0;
	break;
    }
Failed:
    return code;
}

/* copy bits of an rxkad token into a ktc_token */
int
afstoken_to_token(pioctl_set_token *a_token,
    struct ktc_token *ttoken,
    int ttoksize,
    int *flags,
    struct ktc_principal *aclient)
{
    afstoken_soliton at[1];
    XDR xdrs[1];
    int code;

    code = afstoken_to_soliton(a_token, AFSTOKEN_UNION_KAD, at);
    if (code) goto Failed;

    ttoken->kvno = at->afstoken_soliton_u.at_kad.rk_kvno;
    memcpy(ttoken->sessionKey.data,
	at->afstoken_soliton_u.at_kad.rk_key,
	8);
    ttoken->startTime=at->afstoken_soliton_u.at_kad.rk_begintime;
    ttoken->endTime=at->afstoken_soliton_u.at_kad.rk_endtime;
    ttoken->ticketLen=at->afstoken_soliton_u.at_kad.rk_ticket.rk_ticket_len;
    if (aclient) {
	aclient->cell[MAXKTCREALMLEN-1] = 0;
	strncpy(aclient->cell, a_token->cell, MAXKTCREALMLEN-1);
    }
    if (ttoken->ticketLen >
	    (unsigned) (ttoksize - (sizeof *ttoken - MAXKTCTICKETLEN))) {
	return E2BIG;
    }
    memcpy(ttoken->ticket,
	at->afstoken_soliton_u.at_kad.rk_ticket.rk_ticket_val,
	ttoken->ticketLen);
    if (flags)
	*flags = at->afstoken_soliton_u.at_kad.rk_primary_flag & ~0x8000;
    if (!aclient)
	;
    else if ((ttoken->kvno == 999) ||	/* old style bcrypt ticket */
	(ttoken->startTime &&	/* new w/ prserver lookup */
	 (((ttoken->endTime - ttoken->startTime) & 1) == 1))) {
	sprintf(aclient->name, "AFS ID %d", at->afstoken_soliton_u.at_kad.rk_viceid);
    } else {
	sprintf(aclient->name, "Unix UID %d", at->afstoken_soliton_u.at_kad.rk_viceid);
    }
Failed:
    xdrs->x_op = XDR_FREE;
    xdr_afstoken_soliton(xdrs, at);
    return code;
}
