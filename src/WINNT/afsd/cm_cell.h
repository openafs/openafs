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
        struct cm_serverRef *vlServersp;	/* locked by cm_serverLock */
        osi_mutex_t mx;			/* mutex locking fields (flags) */
        long flags;			/* locked by mx */
} cm_cell_t;

#define CM_CELLFLAG_SUID	1	/* setuid flag; not yet used */

extern void cm_InitCell(void);

extern cm_cell_t *cm_GetCell(char *namep, long flags);

extern cm_cell_t *cm_FindCellByID(long cellID);

extern void cm_ChangeRankCellVLServer(cm_server_t       *tsp);

extern osi_rwlock_t cm_cellLock;

extern cm_cell_t *cm_allCellsp;

#endif /* __CELL_H_ENV_ */
