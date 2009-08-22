/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_SRC_UPDATE_INTERNAL_H
#define AFS_SRC_UPDATE_INTERNAL_H

/* utils.c */

extern int AddToList(struct filestr **, char *);
extern int ZapList(struct filestr **);

/* server.c */

extern int UPDATE_FetchFile(struct rx_call *, char *);
extern int UPDATE_FetchInfo(struct rx_call *, char *);

#endif

