/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef UPDATE_GLOBAL_H
#define UPDATE_GLOBAL_H

#ifdef AFS_NT40_ENV
#include <windows.h>
#endif

#define TIMEOUT 	300	/*interval after which the files are resynch'ed */
#define MAXFNSIZE 	1024	/*max size of filenames */
#define	MAXENTRIES	20
#define UPDATEERR 	100

struct filestr {
    struct filestr *next;
    char *name;
};

#endif /* UPDATE_GLOBAL_H */
