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

#ifndef RXK5_TKT_H
#define RXK5_TKT_H

#ifndef KERNEL
#ifdef AFS_NT40_ENV
#include <afs/auth.h>
#else
#include "auth.h" /* ktc_token */
#endif /* !NT */
#else
#include "afs_token.h"
#endif /* !KERNEL */

#ifdef AFS_RXK5
/* In-kernel creds */ 
typedef struct _rxk5_creds
{
    krb5_creds k5creds[1];
} rxk5_creds;

void rxk5_free_creds(
	krb5_context k5context, 
	rxk5_creds *creds);

/*
 * Does what afs_osi_FreeStr(x) does, but a macro
 */
#define rxk5_free_str(x) \
 do { \
	 int s; \
		 s = strlen(x) + 1;		\
		 afs_osi_Free(x, s);		\
 } while (0) \

#endif
/* Interoperable credentials stuff */

#ifdef AFS_RXK5
/*
 * Format new-style afs_token using kerberos 5 credentials (rxk5), 
 * caller frees returned memory (of size bufsize).
 */
int
add_afs_token_rxk5(
	krb5_context context,
	krb5_creds *creds,
	pioctl_set_token *a_token /* out */);
#endif

#ifdef AFS_RXK5
/* 
 * Converts afs_token structure to an rxk5_creds structure, which is returned
 * in creds.  Caller must free.
 */
int afs_token_to_rxk5_creds(
	pioctl_set_token *a_token, 
	rxk5_creds **creds);
	
/* 
 * Converts rxk5_principal structure to a native krb5_creds structure, which is returned
 * in k5_princ.  Caller must free.
 */	
void rxk5_principal_to_krb5_principal(krb5_principal *k5_princ, 
	k5_principal *rxk5_princ);

int
krb5_principal_to_rxk5_principal(krb5_principal princ,
                                 k5_principal *k5_princ);


/*
 * Format new-style afs_token using kerberos 5 credentials (rxk5), 
 * caller frees returned memory (of size bufsize).
 */
int
add_afs_token_rxk5(krb5_context context,
                   krb5_creds *creds,
                   pioctl_set_token *a_token);

/* 
 * Converts afs_token structure to an rxk5_creds structure, which is returned
 * in creds.  Caller must free.
 */
int afs_token_to_rxk5_creds(pioctl_set_token *a_token, 
                            rxk5_creds **creds);

/* copy bits of an rxkad token into a k5 credential */
int
afstoken_to_v5cred(pioctl_set_token *a_token, krb5_creds *v5cred);



#endif

#endif /* RXK5_TKT_H */
