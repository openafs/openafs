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
#else	/* !KERNEL */
#define afs_osi_Alloc(n)	malloc(n)
#define afs_osi_Free(p,n)	free(p)
#define afs_strdup(p) strdup(p)
# include <afs/afsutil.h>
# include "cellconfig.h"
# include "auth.p.h"
# include <stdlib.h>
# include <syslog.h>
# include <stdarg.h>
# include <string.h>
# include <stdio.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
# include <errno.h>
#endif	/* !KERNEL */
#include "rx/rx.h"
#include "afs/afs_token.h"


#ifndef KERNEL
/*
 * Format new-style afs_token using rxkad credentials, 
 * caller frees returned memory (of size bufsize).
 */
int make_afs_token_rxkad(
	char *cell,
	afs_int32 viceid,
	struct ktc_token *k_token,
	afs_int32 primary_flag,
	afs_token **a_token /* out */)
{
	rxkad_token *kad_token;

	(*a_token) = (afs_token*) afs_osi_Alloc(sizeof(afs_token));
	memset((*a_token), 0, sizeof(afs_token)); /* skip? */

	(*a_token)->nextcellnumber = 0;
	(*a_token)->cell = afs_strdup(cell);
	(*a_token)->cu->cu_type = CU_KAD;

	kad_token = &((*a_token)->cu->cu_u.cu_kad);

	kad_token->primary_flag = primary_flag;
	kad_token->cell_name = afs_strdup(cell);
	kad_token->ticket.ticket_len = k_token->ticketLen;
	kad_token->ticket.ticket_val = afs_osi_Alloc(kad_token->ticket.ticket_len);
	memcpy(kad_token->ticket.ticket_val, k_token->ticket, 
	       kad_token->ticket.ticket_len);
	kad_token->token.kvno = k_token->kvno;
	memcpy(kad_token->token.m_key, &(k_token->sessionKey), 8);
	kad_token->token.viceid = viceid;
	kad_token->token.begintime = k_token->startTime;
	kad_token->token.endtime = k_token->endTime;

	return 0;
}
#else	/* KERNEL */

/*
 * Format new-style afs_token using rxkad credentials 
 * as stored in the cache manager.  Caller frees returned memory 
 * (of size bufsize).
 */
int make_afs_token_rxkad_k(
	char *cell,
	n_clear_token *pct,
	char* stp,
	afs_int32 stLen,
	afs_int32 primary_flag,
	afs_token **a_token /* out */)
{
	rxkad_token *kad_token;

	(*a_token) = (afs_token*) afs_osi_Alloc(sizeof(afs_token));
	memset((*a_token), 0, sizeof(afs_token)); /* skip? */

	(*a_token)->nextcellnumber = 0;
	(*a_token)->cell = afs_strdup(cell);
	(*a_token)->cu->cu_type = CU_KAD;

	kad_token = &((*a_token)->cu->cu_u.cu_kad);

	kad_token->primary_flag = primary_flag;
	kad_token->cell_name = afs_strdup(cell);
	kad_token->ticket.ticket_len = stLen;
	kad_token->ticket.ticket_val = afs_osi_Alloc(kad_token->ticket.ticket_len);
	memcpy(kad_token->ticket.ticket_val, stp, kad_token->ticket.ticket_len);
	kad_token->token.kvno = pct->kvno;
	memcpy(kad_token->token.m_key, pct->m_key, 8);
	kad_token->token.viceid = pct->viceid;
	kad_token->token.begintime = pct->begintime ;
	kad_token->token.endtime = pct->endtime ;

	return 0;
}
#endif	/* KERNEL */

/* XXX need a better home for the following 3... */

/*
 * Convert afs_token to XDR-encoded token stream, which is returned
 * in buf (at most of size bufsize).  Caller must pass a sufficiently
 * large buffer.
 */
int
encode_afs_token(afs_token *a_token,
    void *buf /* in */,
    int *bufsize /* inout */)
{
    XDR xdrs[1];
    int r = -1;

    /* XDR encode afs_token into xdr_buf */
    xdrmem_create(xdrs, buf, *bufsize, XDR_ENCODE);
    if (!xdr_afs_token(xdrs, a_token))
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
 * in a_token.	Caller must free.
 */
int
parse_afs_token(void* token_buf, 
    int token_size, 
    afs_token **a_token)
{
    XDR xdrs[1];

    *a_token = afs_osi_Alloc(sizeof(afs_token));
    if(!*a_token)
	return ENOMEM;
    memset(*a_token, 0, sizeof(afs_token)); /* not optional */

    /* XDR decode token_buf into a_token */
    xdrmem_create(xdrs, token_buf, token_size, XDR_DECODE);
    if (!xdr_afs_token(xdrs, *a_token)) {
	return -1;
    }
    return 0;
}

/* 
 * Free afs_token variant using XDR logic 
*/
int
free_afs_token(afs_token *a_token)
{
    XDR xdrs[1];
    xdrs->x_op = XDR_FREE;
    if (!xdr_afs_token(xdrs, a_token)) {
	return 1;
    }
    return 0;
}
