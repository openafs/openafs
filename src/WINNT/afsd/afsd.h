/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __AFSD_H_ENV__
#define __AFSD_H_ENV__ 1

#include <afs/param.h>

#ifndef DJGPP
BOOL InitClass(HANDLE);
BOOL InitInstance(HANDLE, int);

LONG APIENTRY MainWndProc(HWND, unsigned int, unsigned int, long);
BOOL APIENTRY About(HWND, unsigned int, unsigned int, long);
#endif /* !DJGPP */

#ifndef DJGPP
#include <nb30.h>
#else /* DJGPP */
#include <sys/farptr.h>
#include <go32.h>
#include "dosdefs95.h"
#include "largeint95.h"
#endif /* !DJGPP */

#include "afsdicon.h"

#include "cm.h"

#include "krb.h"
#include "krb_prot.h"
#include <crypt.h>
#include <afs/prs_fs.h>

#include <osi.h>
#include "cm_user.h"
#include "cm_callback.h"
#ifdef DISKCACHE95
#include "cm_diskcache95.h"
#endif /* DISKCACHE95 */
#include "cm_conn.h"
#include "cm_aclent.h"
#include "cm_cell.h"
#include "cm_config.h"
#include "cm_server.h"
#include "cm_volume.h"
#include "cm_scache.h"
#include "cm_dcache.h"
#include "cm_access.h"
#include "cm_vnodeops.h"
#include "cm_dir.h"
#include "cm_utils.h"
#include "cm_daemon.h"
#include "cm_ioctl.h"
#include "cm_dnlc.h"
#include "cm_buf.h"
#ifdef DJGPP
#include "afs/afsmsg95.h"
#endif

#include <afs/vldbint.h>
#include <afs/afsint.h>

#define AFS_DAEMON_SERVICE_NAME "TransarcAFSDaemon"
#define AFS_DAEMON_EVENT_NAME "AFS Client"

void afs_exit();

/* globals from the base afsd */

extern int cm_logChunkSize;
extern int cm_chunkSize;

extern cm_volume_t *cm_rootVolumep;

extern cm_cell_t *cm_rootCellp;

extern cm_fid_t cm_rootFid;

extern cm_scache_t *cm_rootSCachep;

extern osi_log_t *afsd_logp;

extern char cm_mountRoot[];
extern DWORD cm_mountRootLen;

extern char cm_CachePath[];

extern BOOL isGateway;

extern BOOL reportSessionStartups;

#endif /* AFSD_H_ENV */
