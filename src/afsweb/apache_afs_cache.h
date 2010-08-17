/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _APACHE_AFS_CACHE_H_INCLUDED_
#define _APACHE_AFS_CACHE_H_INCLUDED_

#include <stdio.h>
#include "nsafs.h"

#ifdef AIX
#include <sys/types.h>
#include <net/nh.h>
#endif

#ifndef MAX
#define MAX(A,B)		((A)>(B)?(A):(B))
#endif /* !MAX */
#ifndef MIN
#define MIN(A,B)		((A)<(B)?(A):(B))
#endif /* !MAX */

/*
 * Macros to manipulate doubly linked lists
 */

#define DLL_INIT_LIST(_HEAD, _TAIL) \
    { _HEAD = NULL ; _TAIL = NULL; }

#define DLL_INSERT_TAIL(_ELEM, _HEAD, _TAIL, _NEXT, _PREV) \
{ \
    if (_HEAD == NULL) { \
        _ELEM->_NEXT = NULL; \
        _ELEM->_PREV = NULL; \
        _HEAD = _ELEM; \
        _TAIL = _ELEM; \
    } else { \
        _ELEM->_NEXT = NULL; \
        _ELEM->_PREV = _TAIL; \
        _TAIL->_NEXT = _ELEM; \
        _TAIL = _ELEM; \
    } \
}

#define DLL_DELETE(_ELEM, _HEAD, _TAIL, _NEXT, _PREV) \
{ \
    if (_ELEM->_NEXT == NULL) { \
				  _TAIL = _ELEM->_PREV; \
    } else { \
	       _ELEM->_NEXT->_PREV = _ELEM->_PREV; \
	   } \
    if (_ELEM->_PREV == NULL) { \
				  _HEAD = _ELEM->_NEXT; \
    } else { \
	       _ELEM->_PREV->_NEXT = _ELEM->_NEXT; \
	   } \
    _ELEM->_NEXT = NULL; \
    _ELEM->_PREV = NULL; \
}


#define WEBLOG_BUFFER_SIZE	4096	/* Send/Receive buffer size */
#define WEBLOG_MAX_PATH		1024	/* Maximum path length */
#define WEBLOG_USERNAME_MAX	64	/* Maximum username length */
#define WEBLOG_CELLNAME_MAX	64	/* Maximum password length */
#define WEBLOG_PASSWORD_MAX     64
#define WEBLOG_LOGIN_HASH_SIZE	1024	/* MUST be power of two */
#define TEN_MINUTES		600	/* 10 minutes = 600 seconds */

#define MAXBUFF                 1024	/* CHECK THIS - size of token */
/*
 * Structure user to store entries in AFS login cache
 */
struct weblog_login {
    afs_uint32 expiration;
    char token[MAXBUFF];
    int tokenLen;
    char username[WEBLOG_USERNAME_MAX];
    char cellname[WEBLOG_CELLNAME_MAX];
    char cksum[SHA_HASH_BYTES];
    struct weblog_login *next;
    struct weblog_login *prev;
};



extern int weblog_login_hash(char *name, char *cell);
extern void weblog_login_checksum(char *user, char *cell, char *passwd,
				  char *cksum);
extern int weblog_login_lookup(char *user, char *cell, char *cksum,
			       char *token);
extern int weblog_login_store(char *user, char *cell, char *cksum,
			      char *token, int tokenLen,
			      afs_uint32 expiration);
extern int getTokenLen(char *token);

#endif /* _APACHE_AFS_CACHE_H_INCLUDED_ */
