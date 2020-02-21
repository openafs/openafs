/*
 * Copyright (c) 2010 Your File System Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Soft signals
 *
 * This is a pthread compatible signal handling system. It doesn't have any
 * restrictions on the code which can be called from a signal handler, as handlers
 * are processed in a dedicated thread, rather than as part of the async signal
 * handling code.
 *
 * Applications wishing to use this system must call opr_softsig_Init _before_
 * starting any other threads. After this, all signal handlers must be registered
 * using opr_softsig_Register.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <opr.h>
#include <roken.h>

#include <pthread.h>

static struct {
    void (*handler) (int);
} handlers[NSIG];

static void
softsigSignalSet(sigset_t *set)
{
    sigfillset(set);
    sigdelset(set, SIGKILL);
    sigdelset(set, SIGSTOP);
    sigdelset(set, SIGCONT);
    sigdelset(set, SIGABRT);
    sigdelset(set, SIGBUS);
    sigdelset(set, SIGILL);
    sigdelset(set, SIGPIPE);
    sigdelset(set, SIGSEGV);
    sigdelset(set, SIGTRAP);
}

static void *
signalHandler(void *arg)
{
    int receivedSignal;
    sigset_t set;

    softsigSignalSet(&set);
    while (1) {
	opr_Verify(sigwait(&set, &receivedSignal) == 0);
	opr_Verify(sigismember(&set, receivedSignal) == 1);
	if (handlers[receivedSignal].handler != NULL) {
	    handlers[receivedSignal].handler(receivedSignal);
	}
    }
    AFS_UNREACHED(return(NULL));
}

static void
ExitHandler(int signal)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, signal);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
    raise(signal);

    /* Should be unreachable. */
    exit(signal);
}

static void
StopHandler(int signal)
{
    kill(getpid(), SIGSTOP);
}

/*!
 * Register a soft signal handler
 *
 * Soft signal handlers may only be registered for async signals.
 *
 * @param[in] sig
 *      The signal to register a handler for.
 * @param[in] handler
 *      The handler function to register, or NULL, to clear a signal handler.
 *
 * @returns
 *      EINVAL if the signal given isn't one for which we can register a soft
 *      handler.
 */

int
opr_softsig_Register(int sig, void (*handler)(int))
{
    sigset_t set;

    softsigSignalSet(&set);

    /* Check that the supplied signal is handled by softsig. */
    if (sigismember(&set, sig)) {
	handlers[sig].handler = handler;
	return 0;
    }

    return EINVAL;
}

/*!
 * Initialise the soft signal system
 *
 * This call initialises the soft signal system. It provides default handlers for
 * SIGINT and SIGTSTP which preserve the operating system behaviour (terminating
 * and stopping the process, respectively).
 *
 * opr_softsig_Init() must be called before any threads are created, as it sets
 * up a global signal mask on the parent process that is then inherited by all
 * children.
 */

int
opr_softsig_Init(void)
{
    sigset_t set;
    pthread_t handlerThread;

    /* Block all signals in the main thread, and in any threads which are created
     * after us. Only the signal handler thread will receive signals. */
    softsigSignalSet(&set);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    /* Register a few handlers so that we keep the usual behaviour for CTRL-C and
     * CTRL-Z, unless the application replaces them. */
    opr_Verify(opr_softsig_Register(SIGINT, ExitHandler) == 0);
    opr_Verify(opr_softsig_Register(SIGTERM, ExitHandler) == 0);
    opr_Verify(opr_softsig_Register(SIGQUIT, ExitHandler) == 0);
    opr_Verify(opr_softsig_Register(SIGTSTP, StopHandler) == 0);

    /*
     * Some of our callers do actually specify a SIGFPE handler, but make sure
     * the default SIGFPE behavior does actually terminate the process, in case
     * we get a real FPE.
     */
    opr_Verify(opr_softsig_Register(SIGFPE, ExitHandler) == 0);

    /* Create a signal handler thread which will respond to any incoming signals
     * for us. */
    opr_Verify(pthread_create(&handlerThread, NULL, signalHandler, NULL) == 0);
    opr_Verify(pthread_detach(handlerThread) == 0);

    return 0;
}
