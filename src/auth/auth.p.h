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
