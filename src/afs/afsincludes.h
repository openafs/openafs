/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_INCLUDES_H
#define AFS_INCLUDES_H 1


#include <afsconfig.h>

#ifdef UKERNEL
#include <UKERNEL/afsincludes.h>
#else

/* AFS based headers */
#include "afs/stds.h"
#ifdef	AFS_AIX_ENV
#include "osi_vfs.h"
#elif defined(AFS_HPUX_ENV) || defined(AFS_NBSD_ENV)
#include "osi_vfs.h"
#endif
#if defined(AFS_SGI_ENV) || defined(AFS_LINUX20_ENV)
#include "osi_vfs.h"
#endif
#include "rx/rx.h"
#include "afs/afs_osi.h"
#include "afs/lock.h"
#include "afs/volerrors.h"
#include "afs/voldefs.h"
#ifdef AFS_LINUX20_ENV
#ifdef TRUE
#undef TRUE
#undef FALSE
#endif
#endif
#ifdef AFS_LINUX20_ENV
#undef __NFDBITS
#undef __FDMASK
#endif
#include "afsint.h"
#include "afs/exporter.h"
#include "vldbint.h"
#include "afs/afs.h"
#include "afs/afs_chunkops.h"
/*#include "afs/auxinode.h"	-- used only in afs_istuff.c */
#include "rx/rxkad.h"
#include "afs/prs_fs.h"
#include "afs/dir.h"
#include "afs/afs_axscache.h"
#include "afs/icl.h"
#include "afs/afs_stats.h"
#include "afs/afs_prototypes.h"
#include "afs/discon.h"
#if defined(AFS_LINUX20_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
#include "osi_machdep.h"
#endif

#endif

#endif /* AFS_INCLUDES_H */
