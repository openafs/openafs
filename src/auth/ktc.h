/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_SRC_AUTH_KTC_H
#define AFS_SRC_AUTH_KTC_H

extern char * ktc_tkt_string(void);
extern char * ktc_tkt_string_uid(afs_uint32);
extern void ktc_set_tkt_string(char *);
extern int ktc_OldPioctl(void);

struct ktc_setTokenData;
struct ktc_tokenUnion;
extern int token_findByType(struct ktc_setTokenData *, int,
			    struct ktc_tokenUnion *);
extern struct ktc_setTokenData *token_buildTokenJar(char *);
extern int token_addToken(struct ktc_setTokenData *, struct ktc_tokenUnion *);
extern int token_replaceToken(struct ktc_setTokenData *,
			      struct ktc_tokenUnion *);
extern void token_setPag(struct ktc_setTokenData *, int);

struct ktc_token;
struct ktc_principal;
extern int token_extractRxkad(struct ktc_setTokenData *, struct ktc_token *,
			      int *, struct ktc_principal *);
#endif /* AFS_SRC_AUTH_KTC_H */
