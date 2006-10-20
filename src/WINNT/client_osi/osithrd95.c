/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#include "osi.h"
#include "lwp.h"
/*#include "lock95.h"*/
#include <assert.h>

#include <stdio.h>

/*
thread95.c

This code implements a partial OSI-over-LWP(and IOMGR) layer for threading
and synchronization, for the AFS NT code port to Windows 9x.
*/

// It so happens that all code seen so far throws away *thread_id;
// unfortunately, callers do not send thread_id==NULL instead of getting
// a value and tossing it. :(
thread_t thrd_Create(int attributes, int stacksize, ThreadFunc func,
                     void *parm, int flags, int *thread_id, char *name)
{
  PROCESS lwp_pid;

  // Reserve priority 0 for IOMGR; we use pri 1 by default
  if (name)
    LWP_CreateProcess(func, stacksize, 1, parm, name, &lwp_pid);
  else
    LWP_CreateProcess(func, stacksize, 1, parm, "thread", &lwp_pid);

  // In principle, the "right" way to do thread_id is to create a mapping
  // between thread_id integers and LWP PROCESS names, but we're not using
  // the IDs for anything anyway, so...
  // actually, it turns out PROCESS is a pointer, so we fudge with that for now
  // just to return a nonzero value
  *thread_id = (int) lwp_pid;

  return lwp_pid;
}

thread_t thrd_Current()
{
  PROCESS pid;
  LWP_CurrentProcess(&pid);
  return pid;
}

int thrd_Close(thread_t thrd)
{
  int rc = LWP_DestroyProcess(thrd);
  if (rc == LWP_SUCCESS)
    return 0;
  else
    return -1;
}

/* The following thread-local-storage and critical-section functions are 
   not used. */
/*
DWORD thrd_Alloc(void)
{
  char **NewRock = NULL;
  static int NextTag = 0;  // Because LWP is not preemptive, we need no mutex

  NewRock = (char **) malloc (sizeof(LPVOID *));
  if (NewRock == NULL)
    return 0xFFFFFFFF;
  *NewRock = (LPVOID *) malloc(sizeof(LPVOID));
  if (*NewRock == NULL) {
    free(NewRock);
    return 0xFFFFFFFF;
  }

  if (LWP_NewRock(++NextTag,NewRock))
    return 0xFFFFFFFF;
  else
    return NextTag;
}

LPVOID thrd_GetValue(DWORD Index)
{
  char *ptr;
  if (LWP_GetRock((int) Index, &ptr)) {
    // SetLastError
    return 0;
  } else {
    return * ((LPVOID *) ptr);
  }
}

BOOL thrd_SetValue(DWORD Index, LPVOID Value) {
  char *ptr;
  if (LWP_GetRock((int) Index, &ptr)) {
    // SetLastError
    return 0;
  } else {
    * ((LPVOID *) ptr) = Value;
    return TRUE;
  }
}

#define LPCRITICAL_SECTION (struct Lock*)

#define thrd_InitCrit (Lock_Init)
#define thrd_EnterCrit (ObtainWriteLock)
#define thrd_LeaveCrit (ReleaseWriteLock)

// LWP has no formal destructor for locks.
#define thrd_DeleteCrit(x)   ;

*/


/* Since LWP is nonpreemptive, arithmetic needs no special handling. */

LONG thrd_Increment(LPLONG number)
{
  ++*number;
  return *number;
}

LONG thrd_Decrement(LPLONG number)
{
  --*number;
  return *number;
}

LONG thrd_Exchange(LPLONG number, LONG value)
{
  LONG oldval = *number;
  *number = value;
  return oldval;
}

// CreateEvent is always called with (NULL,(T/F),(T/F),NULL)
// This code will assume it and fail otherwise.
// SetLastError() is not implemented, i.e., if thrd_CreateEvent fails,
// there is no corresponding GetLastError() to pull out error codes
// at this time.
EVENT *thrd_CreateEvent(void *f, BOOL manual, BOOL startsignaled, void *g)
{
  // LWP code checks eventnames against NULL as an error condition,
  // so we start counting from 1 instead of zero.
  // It turns out that LWP uses event names as unique integer values,
  // even though these values are cast as char pointers.  We will use
  // integers, since they are never dereferenced by LWP.
  static unsigned long NextEventName = 1L;
  EVENT *event;

  // Startup stuff
  if ((f != NULL) || (g != NULL)) {
    // Panic!  This scenario is not implemented.
    assert(0);
    return NULL;
  }

  // Create an event
  if ((event=(EVENT*)malloc(sizeof(EVENT))) == NULL)
  {
    // SetLastError out of memory
    return NULL;
  }
  if (manual)
    event->state = startsignaled ? manualsignal : manualunsig;
  else
    event->state = startsignaled ? autosignal : autounsig;
  event->name = (char *) NextEventName;

  // Increment NextEventName
  ++NextEventName;

  return event;
}

BOOL thrd_SetEvent(EVENT *event)
{
  if (event==NULL)
    return FALSE;
  if (AUTOMATIC(event))
    event->state = autosignal;
  else
    event->state = manualsignal;
  LWP_SignalProcess(event->name);
  return TRUE;
}

BOOL thrd_ResetEvent(EVENT *event)
{
  if (event==NULL)
    return FALSE;
  if (AUTOMATIC(event))
    event->state = autounsig;
  else
    event->state = manualunsig;
  return TRUE;
}

// It appears there is a slight difference in the two wait schemes.
// Win32's WaitForSingleObject returns only when the wait is finished;
// LWP's WaitProcess may return randomly, and so requires while() wrapping
// (a little busywaiting).
DWORD thrd_WaitForSingleObject_Event(EVENT *event, DWORD timeoutms)
{
  if (timeoutms != INFINITE) {
    // Panic!
    assert(0);
    return WAIT_FAILED;
  }
  if (event == NULL) {
    return WAIT_FAILED;
  }
  while (!SIGNALED(event))
    LWP_WaitProcess(event->name);
  if (AUTOMATIC(event))
    event->state = autounsig;
  return WAIT_OBJECT_0;
}

DWORD thrd_WaitForMultipleObjects_Event(DWORD count, EVENT* events[],
					BOOL waitforall, DWORD timeoutms)
{
  if ((timeoutms != INFINITE) || waitforall) {
    // Panic!  This functionality not implemented.
    assert(0);
    return WAIT_FAILED;
  }
  if (events == NULL) {
    return WAIT_FAILED;
  }

  // Do the actual wait
  {
    // Construct the list of LWP events to wait on
    char *names[count+1];
    int i;
    for (i=0;i<count;++i) {
      if (SIGNALED(events[i])) {
	// We're done; one of the events is signaled.
	if (AUTOMATIC(events[i]))
	  events[i]->state = autounsig;
	return (WAIT_OBJECT_0 + i);
      }
      names[i] = events[i]->name;
    }
    names[count] = NULL;
    
    // Do the wait for something to signal
    while (1) {
      LWP_MwaitProcess(1,names);
      // Find who got signalled: MwaitProcess doesn't tell us.
      for (i=0; i<count; ++i) {
	if (SIGNALED(events[i])) {
	  if (AUTOMATIC(events[i]))
	    events[i]->state = autounsig;
	  return WAIT_OBJECT_0 + i;
	}
      }
    }
    // not reached
    assert(0);
  }
  // not reached
  assert(0);
}

int osi_Once(osi_once_t *argp)
{
     long i;

     lock_ObtainMutex(&argp->atomic);

     if (argp->done == 0) {
          argp->done = 1;
          return 1;
     }

     /* otherwise we've already been initialized, so clear lock and return */
     lock_ReleaseMutex(&argp->atomic);
     return 0;
}

void osi_EndOnce(osi_once_t *argp)
{
     lock_ReleaseMutex(&argp->atomic);
}

int osi_TestOnce(osi_once_t *argp)
{
     long localDone;
     long i;

     lock_ObtainMutex(&argp->atomic);

     localDone = argp->done;

     /* drop interlock */
     lock_ReleaseMutex(&argp->atomic);

     return (localDone? 0 : 1);
}

void osi_panic(char *s, char *f, long l)
{
  fprintf(stderr, "Fatal error: %s at %s:%d\n", s, f, l);
  exit(1);
}

/* return true iff x is prime */
int osi_IsPrime(unsigned long x)
{
        unsigned long c;
        
        /* even numbers aren't prime */
        if ((x & 1) == 0 && x != 2) return 0;

        for(c = 3; c<x; c += 2) {
                /* see if x is divisible by c */
                if ((x % c) == 0) return 0;     /* yup, it ain't prime */
                
                /* see if we've gone far enough; only have to compute until
                 * square root of x.
                 */
                if (c*c > x) return 1;
        }

        /* probably never get here */
        return 1;
}

/* return first prime number less than or equal to x */
unsigned long osi_PrimeLessThan(unsigned long x) {
        unsigned long c;
        
        for(c = x; c > 1; c--) {
                if (osi_IsPrime(c)) return c;
        }

        /* ever reached? */
        return 1;
}
