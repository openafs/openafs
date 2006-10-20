#ifndef THRD95_H
#define THRD95_H

#include <lwp.h>
#include <lock.h>
#include <assert.h>
#include <stdio.h>

/* Since we don't have windows.h to define booleans, we do it ourselves */
#ifndef TRUE
 #define TRUE (1)
#endif
#ifndef FALSE
 #define FALSE (0)
#endif

#define osi_rwlock_t struct Lock
#define osi_mutex_t struct Lock

#define osi_Init() { PROCESS pid; LWP_InitializeProcessSupport(1,&pid); IOMGR_Initialize(); }

/*
#define lock_InitializeMutex(a, b) Lock_Init(a)
#define lock_InitializeRWLock(a, b) Lock_Init(a)
*/

#define lock_InitializeRWLock(a, b) Lock_Init(a)
#define lock_InitializeMutex(a, b) lock_InitializeRWLock(a,b)

#define lock_ObtainWrite(a) ObtainWriteLock(a)
#define lock_ObtainRead(a) ObtainReadLock(a)
#define lock_ObtainMutex(a) lock_ObtainWrite(a)
#define lock_ReleaseRead(a) ReleaseReadLock(a)
#define lock_ReleaseWrite(a) ReleaseWriteLock(a)
#define lock_ReleaseMutex(a) lock_ReleaseWrite(a)

#define lock_FinalizeRWLock(a) /* */
#define lock_FinalizeMutex(a) lock_FinalizeRWLock(a)

/*
#define lock_TryWrite(a) if (!WriteLocked(a)) ObtainWriteLock(a)
#define lock_TryRead(a) if (!WriteLocked(a)) ObtainReadLock(a)
#define lock_TryMutex(a) if (!WriteLocked(a)) ObtainWriteLock(a)
*/
#define lock_TryWrite(a) ( ((CheckLock(a)) == 0) ? (({ObtainWriteLock(a);}), 1) : 0 )
#define lock_TryRead(a) ( ((CheckLock(a)) > -1) ? (({ObtainReadLock(a);}), 1) : 0 )
#define lock_TryMutex(a) lock_TryWrite(a)
#define lock_GetMutexState(a) CheckLock(a)
#define lock_AssertMutex(a) assert(lock_GetMutexState(a) == -1)
#define lock_AssertWrite(a) assert(lock_GetMutexState(a) == -1)

#define Crit_Sec osi_mutex_t
#define thrd_InitCrit(a) lock_InitializeMutex(a, "cs")
#define thrd_EnterCrit lock_ObtainMutex
#define thrd_LeaveCrit lock_ReleaseMutex
#define thrd_DeleteCrit(a)

// Does LWP take a char* and read it as ASCIIZ, or as a pointer?
// ASCIIZ, perhaps?
/*
#define osi_Sleep(a) { char buf[sizeof(long)+1]; memcpy(buf,&a,sizeof(long)); buf[sizeof(long)]='\0'; LWP_WaitProcess(buf); }
*/
// Actually, pointer (from reading LWP source)
#define osi_Sleep(a) LWP_WaitProcess(a);

#define __do_LWP_sleep(v,l,f) { f(l); osi_Sleep(v); }
#define osi_SleepM(a,b) __do_LWP_sleep(a,b,lock_ReleaseMutex)
#define osi_SleepR(a,b) __do_LWP_sleep(a,b,lock_ReleaseRead)
#define osi_SleepW(a,b) __do_LWP_sleep(a,b,lock_ReleaseWrite)

/*
#define osi_Wakeup(a) { char buf[sizeof(long)+1]; memcpy(buf,&a,sizeof(long)); buf[sizeof(long)]='\0'; LWP_SignalProcess(buf); }
*/
#define osi_Wakeup(a) LWP_SignalProcess(a)

/*
#define thrd_Sleep(a) IOMGR_Select(0, 0, 0, 0, a)
*/
#define thrd_Sleep(a) IOMGR_Sleep((a)/1000)

#define thrd_Yield LWP_DispatchProcess

/* For thread95.c */
#define __EVENT_NAME_LENGTH 10
typedef struct __event {
  enum { manualunsig = 0, autounsig = 2, 
		 autosignal = 3, manualsignal = 1 } state;
  char *name; // "Name" of corresponding LWP events
  //  char name[__EVENT_NAME_LENGTH+1];  // Name of corresponding LWP events
} EVENT;
#define SIGNALED(event) (event->state & 0x01)
#define AUTOMATIC(event) (event->state & 0x02)

typedef PROCESS thread_t;
typedef int (*ThreadFunc)(void *);
typedef int SecurityAttrib;

thread_t thrd_Create(int attributes, int stacksize, ThreadFunc func,
                     void *parm, int flags, int *thread_id, char *name);
#define thrd_Exit(rc) return(rc)
thread_t thrd_Current(void);
int thrd_Close(thread_t thrd);
#define thrd_CloseHandle(h)

typedef long int LONG;
typedef LONG *LPLONG;
LONG thrd_Increment(LPLONG number);
LONG thrd_Decrement(LPLONG number);
LONG thrd_Exchange(LPLONG number, LONG value);

typedef enum { False=0, True=1 } BOOL;
typedef EVENT *EVENT_HANDLE;  /* Absorbs Win32 type HANDLE when used for synch */
EVENT *thrd_CreateEvent(void *f, BOOL manual, BOOL startsignaled, void *g);
BOOL thrd_SetEvent(EVENT *event);
BOOL thrd_ResetEvent(EVENT *event);

typedef unsigned long DWORD;
#define INFINITE (0xFFFFFFFF)  // infinite timeout for waits
#define WAIT_FAILED (0xFFFFFFFF)
#define WAIT_OBJECT_0 (0L)
#define CONST const
DWORD thrd_WaitForSingleObject_Event(EVENT *event, DWORD timeoutms);
DWORD thrd_WaitForMultipleObjects_Event(DWORD count, EVENT* events[],
					BOOL waitforall, DWORD timeoutms);

#define osi_Time gettime_ms

typedef FILE *FILE_HANDLE;

#endif  /* #ifndef THRD95_H */
