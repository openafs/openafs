/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

/* AFS based headers */
#include "../afs/stds.h"
#ifdef	AFS_AIX_ENV
#include "../afs/osi_vfs.h"
#else
#ifdef	AFS_DEC_ENV
#include "../afs/gfs_vfs.h"
#include "../afs/gfs_vnode.h"
#else
#ifdef	AFS_HPUX_ENV
#include "../afs/osi_vfs.h"
#endif /* AFS_HPUX_ENV */
#endif /* AFS_DEC_ENV */
#endif /* AFS_AIX_ENV */
#if defined(AFS_SGI_ENV) || defined(AFS_LINUX20_ENV)
#include "../afs/osi_vfs.h"
#endif
#include "../afs/afs_osi.h"
#include "../rx/rx.h"
#include "../afs/lock.h"
#include "../afs/volerrors.h"
#include "../afs/voldefs.h"
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
#include "../afsint/afsint.h"
#include "../afs/exporter.h"
#include "../afsint/vldbint.h"
#include "../afs/afs.h"
#include "../afs/afs_chunkops.h"
/*#include "../afs/auxinode.h"	-- used only in afs_istuff.c */
#include "../afs/rxkad.h"
#include "../afs/prs_fs.h"
#include "../afs/dir.h"
#include "../afs/afs_axscache.h"
#include "../afs/icl.h"
#include "../afs/afs_prototypes.h"
#ifdef AFS_LINUX20_ENV
#include "../afs/osi_machdep.h"
#endif
