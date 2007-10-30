/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#if defined(UKERNEL)
#include "afs/param.h"
#else
#include <afs/param.h>
#endif

RCSID
    ("$Header$");

#if defined(UKERNEL)
#include "afs/pthread_glock.h"
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs_usrops.h"
#include "afs/cellconfig.h"
#include "afs/pthread_glock.h"
#include "rx/xdr.h"
#include "rx/rx.h"
#include "afs/kauth.h"
#include "afs/kautils.h"
#include "afs/afsutil.h"
#else /* defined(UKERNEL) */
#include <afs/pthread_glock.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <string.h>
#include <afs/cellconfig.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include "kauth.h"
#include "kautils.h"
#include <afs/afsutil.h>
#endif /* defined(UKERNEL) */

/* This is a utility routine that many parts of kauth use but it invokes the
   afsconf package so its best to have it in a separate .o file to make the
   linker happy. */

static struct afsconf_dir *conf = 0;
static char cell_name[MAXCELLCHARS];

int
ka_CellConfig(const char *dir)
{
    int code;
#ifdef UKERNEL
    conf = afs_cdir;
    strcpy(cell_name, afs_LclCellName);
    return 0;
#else /* UKERNEL */
    LOCK_GLOBAL_MUTEX;
    if (conf)
	afsconf_Close(conf);
    conf = afsconf_Open(dir);
    if (!conf) {
	UNLOCK_GLOBAL_MUTEX;
	return KANOCELLS;
    }
    code = afsconf_GetLocalCell(conf, cell_name, sizeof(cell_name));
    UNLOCK_GLOBAL_MUTEX;
    return code;
#endif /* UKERNEL */
}

char *
ka_LocalCell(void)
{
    int code;

    LOCK_GLOBAL_MUTEX;
    if (conf) {
	UNLOCK_GLOBAL_MUTEX;
	return cell_name;
    }
#ifdef UKERNEL
    conf = afs_cdir;
    strcpy(cell_name, afs_LclCellName);
#else /* UKERNEL */
    if (conf = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH)) {
	code = afsconf_GetLocalCell(conf, cell_name, sizeof(cell_name));
/* leave conf open so we can lookup other cells */
/* afsconf_Close (conf); */
    }
    if (!conf || code) {
	printf("** Can't determine local cell name!\n");
	conf = 0;
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    }
#endif /* UKERNEL */
    UNLOCK_GLOBAL_MUTEX;
    return cell_name;
}

int
ka_ExpandCell(char *cell, char *fullCell, int *alocal)
{
    int local = 0;
    int code;
    char cellname[MAXKTCREALMLEN];
    struct afsconf_cell cellinfo;	/* storage for cell info */

    LOCK_GLOBAL_MUTEX;
    ka_LocalCell();		/* initialize things */
    if (!conf) {
	UNLOCK_GLOBAL_MUTEX;
	return KANOCELLS;
    }

    if ((cell == 0) || (strlen(cell) == 0)) {
	local = 1;
	cell = cell_name;
    } else {
	cell = lcstring(cellname, cell, sizeof(cellname));
	code = afsconf_GetCellInfo(conf, cell, 0, &cellinfo);
	if (code) {
	    UNLOCK_GLOBAL_MUTEX;
	    return KANOCELL;
	}
	cell = cellinfo.name;
    }
    if (strcmp(cell, cell_name) == 0)
	local = 1;

    if (fullCell)
	strcpy(fullCell, cell);
    if (alocal)
	*alocal = local;
    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

int
ka_CellToRealm(char *cell, char *realm, int *local)
{
    int code;

    LOCK_GLOBAL_MUTEX;
    code = ka_ExpandCell(cell, realm, local);
    ucstring(realm, realm, MAXKTCREALMLEN);
    UNLOCK_GLOBAL_MUTEX;
    return code;
}
