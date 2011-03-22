/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSD_AFSD_H
#define AFSD_AFSD_H

#ifdef IGNORE_SOME_GCC_WARNINGS
# pragma GCC diagnostic warning "-Wstrict-prototypes"
#endif

extern int afsd_debug;
extern int afsd_verbose;
extern char afsd_cacheMountDir[];

void afsd_init(void);
int afsd_parse(int argc, char **argv);
int afsd_run(void);

/* a function that is called from afsd_fork in a new process/thread */
typedef void* (*afsd_callback_func) (void *rock);

/* afsd.c expects these to be implemented; it does not implement them itself! */
void afsd_mount_afs(const char *rn, const char *mountdir);
int afsd_check_mount(const char *rn, const char *mountdir);
void afsd_set_rx_rtpri(void);
void afsd_set_afsd_rtpri(void);
int afsd_call_syscall();
int afsd_fork(int wait, afsd_callback_func cbf, void *rock);
int afsd_daemon(int nochdir, int noclose);

#endif /* AFSD_AFSD_H */
