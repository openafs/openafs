/* Defines for abstraction layer for NT */

#ifndef THREAD_NT_H
#define THREAD_NT_H

#define thread_t HANDLE
#define ThreadFunc LPTHREAD_START_ROUTINE
#define SecurityAttrib PSECURITY_ATTRIBUTES

#define thrd_Create(security, stacksize, function, arg1, arg2, pid, name) \
       CreateThread(security, stacksize, function, arg1, arg2, pid)

#define thrd_CloseHandle(phandle) CloseHandle(phandle)

#define thrd_CreateEvent CreateEvent
#define thrd_SetEvent SetEvent
#define thrd_ResetEvent ResetEvent
#define thrd_Increment InterlockedIncrement
#define thrd_Decrement InterlockedDecrement
#define thrd_WaitForSingleObject_Event WaitForSingleObject
#define thrd_WaitForMultipleObjects_Event WaitForMultipleObjects

#define thrd_Sleep Sleep

#define Crit_Sec       CRITICAL_SECTION
#define thrd_InitCrit  InitializeCriticalSection
#define thrd_EnterCrit EnterCriticalSection
#define thrd_LeaveCrit LeaveCriticalSection
#define thrd_DeleteCrit DeleteCriticalSection

#define thrd_Current   GetCurrentThreadId

#define EVENT_HANDLE   HANDLE
#define FILE_HANDLE    HANDLE

#endif /* THREAD_NT_H */
