/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_PMGTPRIVATE_H
#define OPENAFS_PMGTPRIVATE_H

/* Private process management definitions and declarations that we don't
 * want to export in the public procmgmt.h.
 */

#ifdef AFS_NT40_ENV
/* Private process management definitions and declarations for Windows NT */

/* Macros to encode/test/decode process exit status to indicate a signal */
#define PMGT_SIGSTATUS_ENCODE(signo)  \
    (0x0AF50000 | (UINT)(((signo) & 0xFF) << 8))

#define PMGT_IS_SIGSTATUS(status) \
    ((((status) & 0xFF) == 0) && (((status) & 0xFFFF0000) == 0x0AF50000))

#define PMGT_SIGSTATUS_DECODE(status) \
    ((int)(((status) >> 8) & 0xFF))


extern int pmgt_SignalRaiseLocalByName(const char *signo, int *libSigno);
extern int pmgt_RedirectNativeSignals(void);
extern int pmgt_RestoreNativeSignals(void);

#else
/* Private process management definitions and declarations for Unix */

#endif /* AFS_NT40_ENV */

#endif /* OPENAFS_PMGTPRIVATE_H */
