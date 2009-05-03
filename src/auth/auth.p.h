/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __AUTH_AFS_INCL_
#define	__AUTH_AFS_INCL_    1

#include <rx/rxkad.h>		/* to get ticket parameters/contents */
#include "afs_token.h"

/* super-user pincipal used by servers when talking to other servers */
#define AUTH_SUPERUSER        "afs"

struct ktc_token {
    afs_int32 startTime;
    afs_int32 endTime;
    struct ktc_encryptionKey sessionKey;
    short kvno;			/* XXX UNALIGNED */
    int ticketLen;
    char ticket[MAXKTCTICKETLEN];
};

#ifndef KERNEL
#include "afs_token_protos.h"

void afs_get_tokens_type_msg(pioctl_set_token *, char *, int);
#endif
int afstoken_to_soliton(pioctl_set_token *, int, afstoken_soliton *);
int afstoken_to_token(pioctl_set_token *, struct ktc_token *, int, int *, struct ktc_principal *);
#ifdef RXK5_UTILAFS_H
int afstoken_to_v5cred(pioctl_set_token *, krb5_creds *);
#endif

int ktc_SetTokenEx(pioctl_set_token *);
int ktc_GetTokenEx(afs_int32, const char *, pioctl_set_token *);
#ifdef RXK5_UTILAFS_H
int ktc_GetK5Enctypes(krb5_enctype *, int);
afs_int32 ktc_SetK5Token(krb5_context, char *, krb5_creds *,
#ifdef AFS_NT40_ENV
    char *, char*, /* XXX username, smbname */
#endif
  afs_int32);
#endif
int ktc_SetToken(struct ktc_principal *, struct ktc_token *,
    struct ktc_principal *, afs_int32);
int ktc_GetToken(struct ktc_principal *, struct ktc_token *,
    int, struct ktc_principal *);
int ktc_ListTokens(int, int *, struct ktc_principal *);
int ktc_ForgetToken(struct ktc_principal *);
int ktc_ForgetAllTokens(void);

#ifdef AFS_NT40_ENV

/* Flags for the flag word sent along with a token */
#define PIOCTL_LOGON		0x1	/* invoked from integrated logon */

#endif /* AFS_NT40_ENV */

/* Flags for ktc_SetToken() */
#define AFS_SETTOK_SETPAG	0x1
#define AFS_SETTOK_LOGON	0x2	/* invoked from integrated logon */

#endif /* __AUTH_AFS_INCL_ */
