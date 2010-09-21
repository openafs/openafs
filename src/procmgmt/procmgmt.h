/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_PROCMGMT_H
#define OPENAFS_PROCMGMT_H


#ifdef AFS_NT40_ENV
/* Process management definitions for Windows NT */

#include <process.h>


/* -----------------  Processes  ---------------- */


typedef int pid_t;		/* process id type */

/* Wait status macros -- status returned in standard Unix format */

#define WIFEXITED(wstat)     ((int)((wstat) & 0xFF) == 0)
#define WIFSIGNALED(wstat)   ((int)((wstat) & 0xFF) != 0)
#define WEXITSTATUS(wstat)   ((int)(((wstat) >> 8) & 0xFF))
#define WTERMSIG(wstat)      ((int)((wstat) & 0xFF))
#define WCOREDUMP(wstat)     ((int)0)

#define WEXITED_ENCODE(status)     ((int)(((status) & 0xFF) << 8))
#define WSIGNALED_ENCODE(signo)    ((int)((signo) & 0xFF))

/* Wait option macros */

#define WNOHANG   0x01

/* Process related data declarations */

#ifndef PMGTEXPORT
#define PMGTEXPORT __declspec(dllimport)
#endif

#define spawnDatap pmgt_spawnData
PMGTEXPORT extern void *pmgt_spawnData;

#define spawnDataLen pmgt_spawnDataLen
PMGTEXPORT extern size_t pmgt_spawnDataLen;


/* Process related function declarations */

#define PMGT_SPAWN_DETACHED_ENV_NAME  "TransarcAfsPmgtSpawnDetached"

#define spawnprocveb(spath, sargv, senvp, sdatap, sdatalen) \
    pmgt_ProcessSpawnVEB(spath, sargv, senvp, sdatap, sdatalen)
#define spawnprocve(spath, sargv, senvp, estatus) \
    pmgt_ProcessSpawnVEB(spath, sargv, senvp, NULL, 0)
#define spawnprocv(spath, sargv, estatus) \
    pmgt_ProcessSpawnVEB(spath, sargv, NULL, NULL, 0)

extern pid_t pmgt_ProcessSpawnVEB(const char *spath, char **sargv,
				  char **senvp, void *sdatap,
				  size_t sdatalen);


#define waitpid(pid, statusP, options) \
    pmgt_ProcessWaitPid(pid, statusP, options)
#define wait(statusP) \
    pmgt_ProcessWaitPid((pid_t)-1, statusP, 0)

extern pid_t pmgt_ProcessWaitPid(pid_t pid, int *statusP, int options);






/* -----------------  Signals  ---------------- */


/* Attempt to override Microsoft (MSVC) signal definitions */
#ifndef _INC_SIGNAL
#define _INC_SIGNAL
#else
#error "Windows requires custom signal definitions; do not include signal.h."
#endif


/* Supported signals
 *     Support is provided for a subset of the standard Unix signals.
 *     Note that Windows NT (via the MSVC runtime) will NOT generate signals
 *     in response to most HW faults or terminal activity; in particular,
 *     Windows NT will NOT generate SIGILL, SIGSEGV, SIGINT, or SIGTERM.
 */


#define	SIGHUP	1		/* hangup */
#define	SIGINT	2		/* interrupt */
#define	SIGQUIT	3		/* quit */
#define	SIGILL	4		/* illegal instruction */
#define	SIGABRT 6		/* abnormal termination triggered by abort() */
#define	SIGFPE	8		/* floating point exception */
#define	SIGKILL	9		/* kill */
#define	SIGSEGV	11		/* segmentation violation */
#define	SIGTERM	15		/* software termination signal from kill */
#define	SIGUSR1	16		/* user defined signal 1 */
#define	SIGUSR2	17		/* user defined signal 2 */
#define	SIGCLD	18		/* child status change - alias for compatability */
#define	SIGCHLD	18		/* child status change */
#define	SIGTSTP 24		/* user stop requested from tty */

#define NSIG 25			/* max signal number + 1   (sig set macros presume <= 33) */


/* Signal actions */

#define SIG_ERR   ((void (__cdecl *)(int))-1)	/* signal() error value */
#define SIG_DFL   ((void (__cdecl *)(int))0)	/* default signal action */
#define SIG_IGN   ((void (__cdecl *)(int))1)	/* ignore signal */


/* Signal set type and set manipulation macros */

typedef unsigned int sigset_t;

#define sigsetbit(signo)  ((unsigned int)1 << (((signo) - 1) & (32 - 1)))

#define sigemptyset(setP)          ((*(setP) = 0) ? 0 : 0)
#define sigfillset(setP)           ((*(setP) = ~(unsigned int)0) ? 0 : 0)
#define sigaddset(setP, signo)     ((*(setP) |= sigsetbit(signo)) ? 0 : 0)
#define sigdelset(setP, signo)     ((*(setP) &= ~sigsetbit(signo)) ? 0 : 0)
#define sigismember(setP, signo)   ((*(setP) & sigsetbit(signo)) ? 1 : 0)


/* Sigaction type and related macros */

struct sigaction {
    void (__cdecl * sa_handler) (int);
    sigset_t sa_mask;
    int sa_flags;
};

#define	SA_RESETHAND   0x00000001


/* Signal related function declarations */

#define sigaction(signo, actP, oactP)  pmgt_SigactionSet(signo, actP, oactP)
extern int pmgt_SigactionSet(int signo, const struct sigaction *actionP,
			     struct sigaction *old_actionP);

#define signal(signo, dispP)  pmgt_SignalSet(signo, dispP)
extern void (__cdecl *
	     pmgt_SignalSet(int signo, void (__cdecl * dispP) (int))) (int);

#define raise(signo)  pmgt_SignalRaiseLocal(signo)
extern int pmgt_SignalRaiseLocal(int signo);

#define kill(pid, signo)  pmgt_SignalRaiseRemote(pid, signo)
extern int pmgt_SignalRaiseRemote(pid_t pid, int signo);



#else
/* Process management definitions for Unix */

#include <sys/types.h>
#include <sys/wait.h>

#include <signal.h>
#include <unistd.h>


/* -----------------  Processes  ---------------- */

#define spawnprocve(spath, sargv, senvp, estatus) \
    pmgt_ProcessSpawnVE(spath, sargv, senvp, estatus)
#define spawnprocv(spath, sargv, estatus) \
    pmgt_ProcessSpawnVE(spath, sargv, NULL, estatus)

extern pid_t pmgt_ProcessSpawnVE(const char *spath, char **sargv,
				 char **senvp, int estatus);


#endif /* AFS_NT40_ENV */

#endif /* OPENAFS_PROCMGMT_H */
