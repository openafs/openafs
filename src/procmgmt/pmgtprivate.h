/* Copyright (C) 1998  Transarc Corporation.  All Rights Reserved.
 *
 */

#ifndef TRANSARC_PMGTPRIVATE_H
#define TRANSARC_PMGTPRIVATE_H

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

#else
/* Private process management definitions and declarations for Unix */

#endif /* AFS_NT40_ENV */

#endif /* TRANSARC_PMGTPRIVATE_H */
