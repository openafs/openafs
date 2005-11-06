/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Alloc - Memory instrumentation apparatus
 *
 */

#ifndef ALLOC_H
#define ALLOC_H

/*
 * The first step in instrumenting memory management code is to replace
 * your allocation and deallocation statements, and augment "new" and "delete"
 * commands. Once this instrumentation is in place, your application can
 * call the ShowMemoryManager() routine, which provides (on a managed
 * background thread) an interactive window showing current memory usage.
 *
 * EXAMPLES ___________________________________________________________________
 *
 * The easiest memory allocation method is:
 *
 *    LPTSTR psz = Allocate (sizeof(TCHAR) * 256);
 *    Free (psz);
 *
 * Internally, Allocate() uses GlobalAlloc() to actually allocate the
 * requested chunk of memory (plus additional space at the end of the chunk,
 * for a trailing signature). Free() calls GlobalFree() to release the chunk,
 * after validating the trailing signature. The two routines also allow
 * the memory manager to keep track of where memory is allocated and freed.
 *
 * You can construct and destroy C++ objects in the same way:
 *
 *    LPTSTR psz = New (TCHAR[256]);
 *    Delete(psz);
 *
 * For complex constructors, you may need to use the New2 macro:
 *
 *    OBJECT *pObject = New2 (OBJECT,(15,242));
 *    Delete(pObject);
 *
 */

/*
 * DEFINITIONS ________________________________________________________________
 *
 * You can turn off instrumentation for a particular source file by
 * undefining DEBUG for that file. If you want -DDEBUG but still don't
 * want instrumentation, you can explicitly define NO_DEBUG_ALLOC.
 *
 */

#ifndef DEBUG
#ifndef NO_DEBUG_ALLOC
#define NO_DEBUG_ALLOC
#endif
#endif

/*
 * If you put this package as part of a .DLL, that .DLL will need to
 * add these functions to its .DEF file:
 *
 *    ShowMemoryManager
 *    WhileMemoryManagerShowing
 *    MemMgr_AllocateMemory
 *    MemMgr_FreeMemory
 *    MemMgr_TrackNew
 *    MemMgr_TrackDelete
 *
 * You can change MEMMGR_CALLCONV in your .DLL to be _declspec(dllexport),
 * and in your other packages to _declspec(dllimport), to ensure that all
 * modules use the same instance of this package.
 *
 */

#ifndef MEMMGR_CALLCONV
#define MEMMGR_CALLCONV _cdecl
#endif

#ifndef EXPORTED
#define EXPORTED __declspec(dllexport)
#endif

/*
 * MACROS _____________________________________________________________________
 *
 * These definitions hide or expose instrumentation based on your
 * compilation flags.
 *
 */

#ifdef NO_DEBUG_ALLOC

#define Allocate(_s)   ((PVOID)GlobalAlloc(GMEM_FIXED,_s))
#define Free(_p)       GlobalFree((HGLOBAL)(_p))
#define New(_t)        new _t
#define New2(_t,_a)    new _t _a
#define Delete(_p)     delete _p

#else /* NO_DEBUG_ALLOC */

#define Allocate(_s)   MemMgr_AllocateMemory (_s, #_s, __FILE__, __LINE__)
#define Free(_p)       MemMgr_FreeMemory (_p, __FILE__, __LINE__)
#define New(_t)        (_t*)MemMgr_TrackNew (new _t, sizeof(_t), #_t, __FILE__, __LINE__)
#define New2(_t,_a)    (_t*)MemMgr_TrackNew (new _t _a, sizeof(_t), #_t, __FILE__, __LINE__)
#define Delete(_p)     do { MemMgr_TrackDelete (_p, __FILE__, __LINE__); delete _p; } while(0)

#endif /* NO_DEBUG_ALLOC */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

#ifndef DEBUG

#define ShowMemoryManager()
#define WhileMemoryManagerShowing()
#define IsMemoryManagerMessage(_pm) FALSE

#else /* DEBUG */

EXPORTED void MEMMGR_CALLCONV ShowMemoryManager (void);
EXPORTED void MEMMGR_CALLCONV WhileMemoryManagerShowing (void);
EXPORTED BOOL MEMMGR_CALLCONV IsMemoryManagerMessage (MSG *pMsg);

#ifndef NO_DEBUG_ALLOC

EXPORTED PVOID MEMMGR_CALLCONV MemMgr_AllocateMemory (size_t cb, LPSTR pszExpr, LPSTR pszFile, DWORD dwLine);
EXPORTED void MEMMGR_CALLCONV MemMgr_FreeMemory (PVOID pData, LPSTR pszFile, DWORD dwLine);

EXPORTED PVOID MEMMGR_CALLCONV MemMgr_TrackNew (PVOID pData, size_t cb, LPSTR pszExpr, LPSTR pszFile, DWORD dwLine);
EXPORTED void MEMMGR_CALLCONV MemMgr_TrackDelete (PVOID pData, LPSTR pszFile, DWORD dwLine);

#endif /* NO_DEBUG_ALLOC */

#endif /* DEBUG */


#endif /* ALLOC_H */
