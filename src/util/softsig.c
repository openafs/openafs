/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#define _POSIX_PTHREAD_SEMANTICS
#include <afs/param.h>
#include <assert.h>
#include <stdio.h>
#ifndef  AFS_NT40_ENV
#include <signal.h>
#include <unistd.h>
#else
#include <afs/procmgmt.h>
#endif
#include <pthread.h>

static pthread_t softsig_tid;
static struct {
  void (*handler) (int);
  int pending;
} softsig_sigs[NSIG];

static void *
softsig_thread (void *arg)
{
  sigset_t ss;

  sigemptyset (&ss);
  sigaddset (&ss, SIGUSR1);

  while (1) {
    void (*h) (int);
    int i, sigw;

    h = NULL;

    for (i = 0; i < NSIG; i++)
      if (softsig_sigs[i].pending) {
	softsig_sigs[i].pending = 0;
	h = softsig_sigs[i].handler;
	break;
      }

    if (i == NSIG)
      assert (0 == sigwait (&ss, &sigw));
    else if (h)
      h (i);
  }
}

void
softsig_init ()
{
  sigset_t ss, os;

  sigemptyset (&ss);
  sigaddset (&ss, SIGUSR1);

  /* Set mask right away, so we don't accidentally SIGUSR1 the
   * softsig thread and cause an exit (default action).
   */
  assert (0 == pthread_sigmask (SIG_BLOCK, &ss, &os));
  assert (0 == pthread_create (&softsig_tid, NULL, &softsig_thread, NULL));
  assert (0 == pthread_sigmask (SIG_SETMASK, &os, NULL));
}

static void
softsig_handler (int signo)
{
  softsig_sigs[signo].pending = 1;
  pthread_kill (softsig_tid, SIGUSR1);
}

void
softsig_signal (int signo, void (*handler) (int))
{
  softsig_sigs[signo].handler = handler;
  signal (signo, softsig_handler);
}

#if defined(TEST)
static void
print_foo (int signo)
{
  printf ("foo, signo = %d, tid = %d\n", signo, pthread_self ());
}

int
main ()
{
  softsig_init ();
  softsig_signal (SIGINT, print_foo);
  printf ("main is tid %d\n", pthread_self ());
  while (1)
    sleep (60);
}
#endif
