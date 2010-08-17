/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_LIBADMIN_TEST_COMMON_H_
#define OPENAFS_LIBADMIN_TEST_COMMON_H_

#define ERR_EXT(string) \
    fprintf(stderr, "%s\n", string);\
    exit(1);

#define ERR_ST_EXT(string, st) \
    { \
    const char *errstr = "unknown error"; \
    util_AdminErrorCodeTranslate(st, 0, &errstr, (afs_status_p) 0); \
    fprintf(stderr, "%s (%s - %d)\n", string, errstr, st);\
    exit(1); \
    }

/*
 * Convenience enum for indexing to common parameters
 */

typedef enum {
    USER_PARAM = 12,
    PASSWORD_PARAM,
    AUTHCELL_PARAM,
    EXECCELL_PARAM,
    NOAUTH_PARAM
} CommonParm_t;

extern void
  SetupCommonCmdArgs(struct cmd_syndesc *as);

extern void *cellHandle;
extern void *tokenHandle;

#endif /* OPENAFS_LIBADMIN_TEST_COMMON_H_ */
