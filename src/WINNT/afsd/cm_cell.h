/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __CELL_H_ENV_
#define __CELL_H_ENV_ 1

#include "cm_server.h"

/* a cell structure */
typedef struct cm_cell {
	long cellID;			/* cell ID */
	struct cm_cell *nextp;		/* locked by cm_cellLock */
        char *namep;			/* cell name; never changes */
        cm_serverRef_t *vlServersp;     /* locked by cm_serverLock */
        osi_mutex_t mx;			/* mutex locking fields (flags) */
        long flags;			/* locked by mx */
        long timeout;                   /* if dns, time at which the server addrs expire */
} cm_cell_t;

/* These are bit flag values */
#define CM_CELLFLAG_SUID	    1	/* setuid flag; not yet used */
#define CM_CELLFLAG_DNS         2   /* cell servers are from DNS */
#define CM_CELLFLAG_VLSERVER_INVALID 4  /* cell servers are invalid */

extern void cm_InitCell(void);

extern cm_cell_t *cm_GetCell(char *namep, long flags);

extern cm_cell_t *cm_GetCell_Gen(char *namep, char *newnamep, long flags);

extern cm_cell_t *cm_FindCellByID(long cellID);

extern void cm_ChangeRankCellVLServer(cm_server_t       *tsp);

extern osi_rwlock_t cm_cellLock;

extern cm_cell_t *cm_allCellsp;

#endif /* __CELL_H_ENV_ */
