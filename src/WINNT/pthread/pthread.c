/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This file contains a skeleton pthread implementation for NT.
 * This is not intended to be a fully compliant pthread implementation
 * The purpose of this file is to only implement those functions that
 * are truly needed to support the afs code base.
 *
 * A secondary goal is to allow a "real" pthread implementation to 
 * replace this file without any modification to code that depends upon
 * this file
 *
 * The function signatures and argument types are meant to be the same
 * as their UNIX prototypes.
 * Where possible, the POSIX specified return values are used.
 * For situations where an error can occur, but no corresponding
 * POSIX error value exists, unique (within a given function) negative 
 * numbers are used for errors to avoid collsions with the errno
 * style values.
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/timeb.h>

#define PTHREAD_EXIT_EXCEPTION 0x1

/*
 * Posix threads uses static initialization for pthread_once control
 * objects, and under NT, every sophisticated synchronization primitive
 * uses procedural initialization.  This forces the use of CompareExchange
 * (aka test and set) and busy waiting for threads that compete to run
 * a pthread_once'd function.  We make these "busy" threads give up their
 * timeslice - which should cause acceptable behavior on a single processor
 * machine, but on a multiprocessor machine this could very well result
 * in busy waiting.
 */

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void)) {
    int rc = 0;

    if ((once_control != NULL) && (init_routine != NULL)) {
	if (InterlockedExchange((LPLONG)&once_control->call_started,
				(LONG) 1) == 0) {
	    (*init_routine)();
	    once_control->call_running = 0;
	} else {
	    /* use Sleep() since SwitchToThread() not available on Win95 */
	    while(once_control->call_running) Sleep(20);
	}
    } else {
	rc = EINVAL;
    }
    return rc;
}

/*
 * For now only support PTHREAD_PROCESS_PRIVATE mutexes.
 * if PTHREAD_PROCESS_SHARED are required later they can be added
 */

int pthread_mutex_init(pthread_mutex_t *mp, const pthread_mutexattr_t *attr) {
    int rc = 0;

    if ((mp != NULL) && (attr == NULL)) {
	InitializeCriticalSection(&mp->cs);
	mp->isLocked = 0;
	mp->tid = 0;
    } else {
	rc = EINVAL;
    }
    return rc;
}

/*
 * Under NT, critical sections can be locked recursively by the owning
 * thread.  This is opposite of the pthread spec, and so we keep track
 * of the thread that has locked a critical section.  If the same thread
 * tries to lock a critical section more than once we fail.
 */
int pthread_mutex_trylock(pthread_mutex_t *mp) {
    int rc = 0;

#ifdef AFS_WIN95_ENV
    /* TryEnterCriticalSection() not available on Win95, so just wait for
     * the lock.  Correct code generally can't depend on how long the
     * function takes to return, so the only code that will be broken is
     * that for which 1) the mutex *mp is obtained and never released or
     * 2) the mutex *mp is intentionally held until trylock() returns.
     * These cases are unusual and don't appear in normal (non-test) AFS
     * code; furthermore, we can reduce (but not eliminate!) the problem by
     * sneaking a look at isLocked even though we don't hold the
     * CRITICAL_SECTION in mutex *mp and are thus vulnerable to race
     * conditions.  Note that self-deadlock isn't a problem since
     * CRITICAL_SECTION objects are recursive.
     *
     * Given the very restricted usage of the pthread library on Windows 95,
     * we can live with these limitations.
     */
    if (mp != NULL) {
	if (mp->isLocked) {
	    rc = EBUSY;
	} else {
	    rc = pthread_mutex_lock(mp);
	}
    } else {
	rc = EINVAL;
    }
#else
    /* TryEnterCriticalSection() provided on other MS platforms of interest */
    if (mp != NULL) {
	if (TryEnterCriticalSection(&mp->cs)) {
	    if (mp->isLocked) {
		/* same thread tried to recursively lock, fail */
		LeaveCriticalSection(&mp->cs);
		rc = EBUSY;
	    } else {
		mp->isLocked = 1;
		mp->tid = GetCurrentThreadId();
		rc = 0;
	    }
	} else {
	    rc = EBUSY;
	}
    } else {
	rc = EINVAL;
    }
#endif /* AFS_WIN95_ENV */

    return rc;
}


int pthread_mutex_lock(pthread_mutex_t *mp) {
    int rc = 0;

    if (mp != NULL) {
	EnterCriticalSection(&mp->cs);
	if (!mp->isLocked) {
	    mp->isLocked = 1;
	    mp->tid = GetCurrentThreadId();
	} else {
	    /* 
	     * same thread tried to recursively lock this mutex.
	     * Under real POSIX, this would cause a deadlock, but NT only 
	     * supports recursive mutexes so we indicate the situation
	     * by returning EDEADLK.
	     */
	    LeaveCriticalSection(&mp->cs);
	    rc = EDEADLK;
	}
    } else {
	rc = EINVAL;
    }
	
    return rc;
}

int pthread_mutex_unlock(pthread_mutex_t *mp) {
    int rc = 0;

    if (mp != NULL) {
	if (mp->tid == GetCurrentThreadId()) {
	    mp->isLocked = 0;
	    mp->tid = 0;
	    LeaveCriticalSection(&mp->cs);
	} else {
	    rc = 0;
	}
    } else {
	rc = EINVAL;
    }
    return rc;
}

int pthread_mutex_destroy(pthread_mutex_t *mp) {
    int rc = 0;

    if (mp != NULL) {
	DeleteCriticalSection(&mp->cs);
    } else {
	rc = EINVAL;
    }

    return rc;
}

/*
 * keys is used to keep track of which keys are currently
 * in use by the threads library.  pthread_tsd_mutex is used
 * to protect keys.
 *
 * The bookkeeping for keys in use and destructor function/key is
 * at the library level.  Each individual thread only keeps its
 * per key data value.  This implies that the keys array and the
 * tsd array in the pthread_t structure need to always be exactly
 * the same size since the same index is used for both arrays.
 */

typedef struct {
    int inuse;
    void (*destructor)(void *);
} pthread_tsd_table_t;

static pthread_tsd_table_t keys[PTHREAD_KEYS_MAX];
static pthread_mutex_t pthread_tsd_mutex;
static pthread_once_t pthread_tsd_once = PTHREAD_ONCE_INIT;

/*
 * In order to support p_self() and p_join() under NT,
 * we have to keep our own list of active threads and provide a mapping
 * function that maps the NT thread id to our internal structure.
 * The main reason that this is necessary is that GetCurrentThread
 * returns a special constant not an actual handle to the thread.
 * This makes it impossible to write a p_self() function that works
 * with only the native NT functions.
 */

static struct rx_queue active_Q;
static struct rx_queue cache_Q;

static pthread_mutex_t active_Q_mutex;
static pthread_mutex_t cache_Q_mutex;

static pthread_once_t pthread_cache_once = PTHREAD_ONCE_INIT;
static int pthread_cache_done;

typedef struct thread {
    struct rx_queue thread_queue;
    void *rc;
    int running;
    pthread_cond_t wait_terminate;
    int waiter_count;
    int is_joinable;
    int has_been_joined;
    HANDLE t_handle;
    DWORD NT_id;
    int native_thread;
    char **tsd;
} thread_t, *thread_p;

static void create_once(void) {
    queue_Init(&active_Q);
    queue_Init(&cache_Q);
    pthread_mutex_init(&active_Q_mutex, (const pthread_mutexattr_t*)0);
    pthread_mutex_init(&cache_Q_mutex, (const pthread_mutexattr_t*)0);
    pthread_cache_done = 1;
}

static void put_thread(thread_p old) {
 
    CloseHandle(old->t_handle);
    pthread_mutex_lock(&cache_Q_mutex);
    queue_Prepend(&cache_Q, old);
    pthread_mutex_unlock(&cache_Q_mutex);
}

static thread_p get_thread() {
    thread_p new = NULL;
 
    pthread_mutex_lock(&cache_Q_mutex);
 
    if (queue_IsEmpty(&cache_Q)) {
        new = (thread_p) malloc(sizeof(thread_t));
	if (new != NULL) {
	    /*
	     * One time initialization - we assume threads put back have
	     * unlocked mutexes and condition variables with no waiters
	     *
	     * These functions cannot fail currently.
	     */
	    pthread_cond_init(&new->wait_terminate,(const pthread_condattr_t *)0);
	}
    } else {
        new = queue_First(&cache_Q, thread);
        queue_Remove(new);
    }
 
    pthread_mutex_unlock(&cache_Q_mutex);

    /* 
     * Initialization done every time we hand out a thread_t
     */

    if (new != NULL) {
	new->rc = NULL;
	new->running = 1;
	new->waiter_count = 0;
	new->has_been_joined = 0;
    }
    return new;
 
}
 
/*
 * The thread start function signature is different on NT than the pthread
 * spec so we create a tiny stub to map from one signature to the next.
 * This assumes that a void * can be stored within a DWORD.
 */

typedef struct {
    void *(*func)(void *);
    void *arg;
    char *tsd[PTHREAD_KEYS_MAX];
    thread_p me;
} pthread_create_t;

static DWORD tsd_index = 0xffffffff;
static DWORD tsd_pthread_index = 0xffffffff;
static pthread_once_t global_tsd_once = PTHREAD_ONCE_INIT;
static int tsd_done;

static void tsd_once(void) {
    while(tsd_index == 0xffffffff) {
	tsd_index = TlsAlloc();
    }
    while(tsd_pthread_index == 0xffffffff) {
	tsd_pthread_index = TlsAlloc();
    }
    tsd_done = 1;
}

static void tsd_free_all(char *tsd[PTHREAD_KEYS_MAX]) {
    int call_more_destructors = 0;
    do {
	int i;
	void *value;
	void (*destructor)(void *);
	call_more_destructors = 0;
	for(i=0;i<PTHREAD_KEYS_MAX;i++) {
	    if (tsd[i] != NULL) {
		destructor = keys[i].destructor;
		value = (void *)tsd[i];
		tsd[i] = NULL;
		if (destructor != NULL) {
		    (destructor)(value);
		    /*
		     * A side-effect of calling a destructor function is that
		     * more thread specific may be created for this thread.
		     * If we call a destructor, we must recycle through the
		     * entire list again and run any new destructors.
		     */
		    call_more_destructors = 1;
		}
	    }
	}
    } while(call_more_destructors);
}

static DWORD WINAPI afs_pthread_create_stub(LPVOID param) {
    pthread_create_t *t = (pthread_create_t *) param;
    void *rc;

    /* 
     * Initialize thread specific storage structures.
     */

    memset(t->tsd, 0, (sizeof(char *) * PTHREAD_KEYS_MAX));
    (tsd_done || pthread_once(&global_tsd_once, tsd_once));
    TlsSetValue(tsd_index, (LPVOID) (t->tsd));
    TlsSetValue(tsd_pthread_index, (LPVOID) (t->me));

    /*
     * Call the function the user passed to pthread_create and catch the
     * pthread exit exception if it is raised.
     */

    __try {
	rc = (*(t->func))(t->arg);
    } __except(GetExceptionCode() == PTHREAD_EXIT_EXCEPTION) {
	rc = t->me->rc; /* rc is set at pthread_exit */
    }

    /*
     * Cycle through the thread specific data for this thread and
     * call the destructor function for each non-NULL datum
     */

    tsd_free_all (t->tsd);
    t->me->tsd = NULL;

    /*
     * If we are joinable, signal any waiters.
     */

    pthread_mutex_lock(&active_Q_mutex);
    if (t->me->is_joinable) {
	t->me->running = 0;
	t->me->rc = rc;
	if (t->me->waiter_count) {
	    pthread_cond_broadcast(&t->me->wait_terminate);
	}
    } else {
	queue_Remove(t->me);
	put_thread(t->me);
    }
    pthread_mutex_unlock(&active_Q_mutex);

    free(t);
    return 0;
}

/*
 * If a pthread function is called on a thread which was not created by
 * pthread_create(), that thread will have an entry added to the active_Q
 * by pthread_self(). When the thread terminates, we need to know
 * about it, so that we can perform cleanup. A dedicated thread is therefore
 * maintained, which watches for any thread marked "native_thread==1"
 * in the active_Q to terminate. The thread spends most of its time sleeping:
 * it can be signalled by a dedicated event in order to alert it to the
 * presense of a new thread to watch, or will wake up automatically when
 * a native thread terminates.
 */

static DWORD terminate_thread_id = 0;
static HANDLE terminate_thread_handle = INVALID_HANDLE_VALUE;
static HANDLE terminate_thread_wakeup_event = INVALID_HANDLE_VALUE;
static HANDLE *terminate_thread_wakeup_list = NULL;
static size_t terminate_thread_wakeup_list_size = 0;

static DWORD WINAPI terminate_thread_routine(LPVOID param) {
    thread_p cur, next;
    DWORD native_thread_count;
    int should_terminate;
    int terminate_thread_wakeup_list_index;

    for (;;) {
	/*
	 * Grab the active_Q_mutex, and while we hold it, scan the active_Q
	 * to see how many native threads we need to watch. If we don't need
	 * to watch any, we can stop this watcher thread entirely (or not);
	 * if we do need to watch some, fill the terminate_thread_wakeup_list
	 * array and go to sleep.
	 */
	cur = NULL;
	next = NULL;
	native_thread_count = 0;
	should_terminate = FALSE;
	pthread_mutex_lock(&active_Q_mutex);

	for(queue_Scan(&active_Q, cur, next, thread)) {
	    if (cur->native_thread)
	    	++native_thread_count;
	}

	/*
	 * At this point we could decide to terminate this watcher thread
	 * whenever there are no longer any native threads to watch--however,
	 * since thread creation is a time-consuming thing, and since this
	 * thread spends all its time sleeping anyway, there's no real
	 * compelling reason to do so. Thus, the following statement is
	 * commented out:
	 *
	 * if (!native_thread_count) {
	 *    should_terminate = TRUE;
	 * }
	 *
	 * Restore the snippet above to cause this watcher thread to only
	 * live whenever there are native threads to watch.
	 *
	 */

	/*
	 * Make sure that our wakeup_list array is large enough to contain
	 * the handles of all the native threads /and/ to contain an
	 * entry for our wakeup_event (in case another native thread comes
	 * along).
	 */
	if (terminate_thread_wakeup_list_size < (1+native_thread_count)) {
	    if (terminate_thread_wakeup_list)
		free (terminate_thread_wakeup_list);
	    terminate_thread_wakeup_list = (HANDLE*)malloc (sizeof(HANDLE) *
				(1+native_thread_count));
	    if (terminate_thread_wakeup_list == NULL) {
		should_terminate = TRUE;
	    } else {
		terminate_thread_wakeup_list_size = 1+native_thread_count;
	    }
	}

	if (should_terminate) {
	    /*
	     * Here, we've decided to terminate this watcher thread.
	     * Free our wakeup event and wakeup list, then release the
	     * active_Q_mutex and break this loop.
	     */
	    if (terminate_thread_wakeup_list)
		free (terminate_thread_wakeup_list);
	    CloseHandle (terminate_thread_wakeup_event);
	    terminate_thread_id = 0;
	    terminate_thread_handle = INVALID_HANDLE_VALUE;
	    terminate_thread_wakeup_event = INVALID_HANDLE_VALUE;
	    terminate_thread_wakeup_list = NULL;
	    terminate_thread_wakeup_list_size = 0;
	    pthread_mutex_unlock(&active_Q_mutex);
	    break;
	} else {
	    /*
	     * Here, we've decided to wait for native threads et al.
	     * Fill out the wakeup_list.
	     */
	    memset(terminate_thread_wakeup_list, 0x00, (sizeof(HANDLE) * 
				(1+native_thread_count)));

	    terminate_thread_wakeup_list[0] = terminate_thread_wakeup_event;
	    terminate_thread_wakeup_list_index = 1;

	    cur = NULL;
	    next = NULL;
	    for(queue_Scan(&active_Q, cur, next, thread)) {
		if (cur->native_thread) {
		    terminate_thread_wakeup_list[terminate_thread_wakeup_list_index]
			= cur->t_handle;
		    ++terminate_thread_wakeup_list_index;
		}
	    }

	    ResetEvent (terminate_thread_wakeup_event);
	}

	pthread_mutex_unlock(&active_Q_mutex);

	/*
	 * Time to sleep. We'll wake up if either of the following happen:
	 * 1) Someone sets the terminate_thread_wakeup_event (this will
	 *    happen if another native thread gets added to the active_Q)
	 * 2) One or more of the native threads terminate
	 */
	terminate_thread_wakeup_list_index = WaitForMultipleObjects(
					1+native_thread_count,
					terminate_thread_wakeup_list,
					FALSE,
					INFINITE);

	/*
	 * If we awoke from sleep because an event other than
	 * terminate_thread_wakeup_event was triggered, it means the
	 * specified thread has terminated. (If more than one thread
	 * terminated, we'll handle this first one and loop around--
	 * the event's handle will still be triggered, so we just won't
	 * block at all when we sleep next time around.)
	 */
	if (terminate_thread_wakeup_list_index > 0) {
	    pthread_mutex_lock(&active_Q_mutex);

	    cur = NULL;
	    next = NULL;
	    for(queue_Scan(&active_Q, cur, next, thread)) {
		if (cur->t_handle == terminate_thread_wakeup_list[ terminate_thread_wakeup_list_index ])
		    break;
	    }

	    if(cur != NULL) {
		/*
		 * Cycle through the thread specific data for the specified
		 * thread and call the destructor function for each non-NULL
		 * datum. Then remove the thread_t from active_Q and put it
		 * back on cache_Q for possible later re-use.
		 */
		if(cur->tsd != NULL) {
		    tsd_free_all(cur->tsd);
                    free(cur->tsd);
		    cur->tsd = NULL;
		}
		queue_Remove(cur);
		put_thread(cur);
	    }

	    pthread_mutex_unlock(&active_Q_mutex);
	}
    }
    return 0;
}


static void pthread_sync_terminate_thread(void) {
    (pthread_cache_done || pthread_once(&pthread_cache_once, create_once));

    if (terminate_thread_handle == INVALID_HANDLE_VALUE) {
        CHAR eventName[MAX_PATH];
        static eventCount = 0;
        sprintf(eventName, "terminate_thread_wakeup_event %d::%d", _getpid(), eventCount++);
        terminate_thread_wakeup_event = CreateEvent((LPSECURITY_ATTRIBUTES) 0,
                                                     TRUE, FALSE, (LPCTSTR) eventName);
        terminate_thread_handle = CreateThread((LPSECURITY_ATTRIBUTES) 0, 0, 
                                                terminate_thread_routine, (LPVOID) 0, 0, 
                                                &terminate_thread_id);
    } else {
    	SetEvent (terminate_thread_wakeup_event);
    }
}


/*
 * Only support the detached attribute specifier for pthread_create.
 * Under NT, thread stacks grow automatically as needed.
 */

int pthread_create(pthread_t *tid, const pthread_attr_t *attr, void *(*func)(void *), void *arg) {
    int rc = 0;
    pthread_create_t *t = NULL;

    (pthread_cache_done || pthread_once(&pthread_cache_once, create_once));

    if ((tid != NULL) && (func != NULL)) {
	if ((t = (pthread_create_t *) malloc(sizeof(pthread_create_t))) &&
	    (t->me = get_thread()) ) {
	    t->func = func;
	    t->arg = arg;
	    *tid = (pthread_t) t->me;
	    if (attr != NULL) {
		t->me->is_joinable = attr->is_joinable;
	    } else {
		t->me->is_joinable = PTHREAD_CREATE_JOINABLE;
	    }
	    t->me->native_thread = 0;
	    t->me->tsd = t->tsd;
	    /*
	     * At the point (before we actually create the thread)
	     * we need to add our entry to the active queue.  This ensures
	     * us that other threads who may run after this thread returns
	     * will find an entry for the create thread regardless of
	     * whether the newly created thread has run or not.
	     * In the event the thread create fails, we will have temporarily
	     * added an entry to the list that was never valid, but we
	     * (i.e. the thread that is calling thread_create) are the
	     * only one who could possibly know about the bogus entry
	     * since we hold the active_Q_mutex.
	     */
	    pthread_mutex_lock(&active_Q_mutex);
	    queue_Prepend(&active_Q, t->me);
	    t->me->t_handle = CreateThread((LPSECURITY_ATTRIBUTES) 0, 0, 
		                afs_pthread_create_stub, (LPVOID) t, 0, 
			        &t->me->NT_id);
	    if (t->me->t_handle == 0) {
		/* 
		 * we only free t if the thread wasn't created, otherwise
		 * it's free'd by the new thread.
		 */
		queue_Remove(t->me);
		put_thread(t->me);
		free(t);
		rc = EAGAIN;
	    }
	    pthread_mutex_unlock(&active_Q_mutex);
	} else {
	    if (t != NULL) {
		free(t);
	    }
	    rc = ENOMEM;
	}
    } else {
	rc = EINVAL;
    }
    return rc;
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    int rc = 0;

    /*
     * Only support default attribute -> must pass a NULL pointer for
     * attr parameter.
     */
    if ((attr == NULL) && (cond != NULL)) {
	InitializeCriticalSection(&cond->cs);
	queue_Init(&cond->waiting_threads);
    } else {
	rc = EINVAL;
    }

    return rc;
}

/*
 * In order to optimize the performance of condition variables,
 * we maintain a pool of cond_waiter_t's that have been dynamically
 * allocated.  There is no attempt made to garbage collect these -
 * once they have been created, they stay in the cache for the life
 * of the process.
 */
 
static struct rx_queue waiter_cache;
static CRITICAL_SECTION waiter_cache_cs;
static int waiter_cache_init;
static pthread_once_t waiter_cache_once = PTHREAD_ONCE_INIT;
 
static void init_waiter_cache(void) {
    InitializeCriticalSection(&waiter_cache_cs);
    waiter_cache_init = 1;
    queue_Init(&waiter_cache);
}
 
static cond_waiters_t *get_waiter() {
    cond_waiters_t *new = NULL;
 
    (waiter_cache_init || pthread_once(&waiter_cache_once, init_waiter_cache));
 
    EnterCriticalSection(&waiter_cache_cs);
 
    if (queue_IsEmpty(&waiter_cache)) {
        new = (cond_waiters_t *) malloc(sizeof(cond_waiters_t));
	if (new != NULL) {
        CHAR eventName[MAX_PATH];
        static eventCount = 0;
        sprintf(eventName, "cond_waiters_t %d::%d", _getpid(), eventCount++);
        new->event = CreateEvent((LPSECURITY_ATTRIBUTES) 0, FALSE,
                                  FALSE, (LPCTSTR) eventName);
	    if (new->event == NULL) {
		free(new);
		new = NULL;
	    }
	}
    } else {
        new = queue_First(&waiter_cache, cond_waiter);
        queue_Remove(new);
    }
 
    LeaveCriticalSection(&waiter_cache_cs);
    return new;
 
}
 
static void put_waiter(cond_waiters_t *old) {
 
    (waiter_cache_init || pthread_once(&waiter_cache_once, init_waiter_cache));
 
    EnterCriticalSection(&waiter_cache_cs);
    queue_Prepend(&waiter_cache, old);
    LeaveCriticalSection(&waiter_cache_cs);
}

static int cond_wait_internal(pthread_cond_t *cond, pthread_mutex_t *mutex, const DWORD time) {
    int rc=0;
    cond_waiters_t *my_entry = get_waiter();
    cond_waiters_t *cur, *next;
    int hasnt_been_signalled=0;

    if ((cond != NULL) && (mutex != NULL) && (my_entry != NULL)) {
	EnterCriticalSection(&cond->cs);
	queue_Append(&cond->waiting_threads, my_entry);
	LeaveCriticalSection(&cond->cs);

	if (!pthread_mutex_unlock(mutex)) {
	    switch(WaitForSingleObject(my_entry->event, time)) {
		case WAIT_FAILED:
		    rc = -1;
		    break;
		case WAIT_TIMEOUT:
		    rc = ETIME;
		    /*
		     * This is a royal pain.  We've timed out waiting
		     * for the signal, but between the time out and here
		     * it is possible that we were actually signalled by 
		     * another thread.  So we grab the condition lock
		     * and scan the waiting thread queue to see if we are
		     * still there.  If we are, we just remove ourselves.
		     *
		     * If we are no longer listed in the waiter queue,
		     * it means that we were signalled after the time
		     * out occurred and so we have to do another wait
		     * WHICH HAS TO SUCCEED!  In this case, we reset
		     * rc to indicate that we were signalled.
		     *
		     * We have to wait or otherwise, the event
		     * would be cached in the signalled state, which
		     * is wrong.  It might be more efficient to just
		     * close and reopen the event.
		     */
		    EnterCriticalSection(&cond->cs);
		    for(queue_Scan(&cond->waiting_threads, cur,
				   next, cond_waiter)) {
			if (cur == my_entry) {
			    hasnt_been_signalled = 1;
			    break;
			}
		    }
		    if (hasnt_been_signalled) {
			queue_Remove(cur);
		    } else {
			rc = 0;
			if (ResetEvent(my_entry->event)) {
			    if (pthread_mutex_lock(mutex)) {
				rc = -5;
			    }
			} else {
			    rc = -6;
			}
		    }
		    LeaveCriticalSection(&cond->cs);
		    break;
		case WAIT_ABANDONED:
		    rc = -2;
		    break;
		case WAIT_OBJECT_0:
		    if (pthread_mutex_lock(mutex)) {
			rc = -3;
		    }
		    break;
		default:
		    rc = -4;
		    break;
	    }
	} else {
	    rc = EINVAL;
	}
    } else {
	rc = EINVAL;
    }

    if (my_entry != NULL) {
	put_waiter(my_entry);
    }

    return rc;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    int rc = 0;

    rc = cond_wait_internal(cond, mutex, INFINITE);
    return rc;
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
    int rc = 0;
    struct _timeb now, then;
    afs_uint32 n_milli, t_milli;

    if (abstime->tv_nsec < 1000000000) {

	/*
	 * pthread timedwait uses an absolute time, NT uses relative so
	 * we convert here.  The millitm field in the timeb struct is
	 * unsigned, but we need to do subtraction preserving the sign, 
	 * so we copy the fields into temporary variables.
	 *
	 * WARNING:
	 * In NT 4.0 SP3, WaitForSingleObject can occassionally timeout
	 * earlier than requested.  Therefore, our pthread_cond_timedwait
	 * can also return early.
	 */

	_ftime(&now);
	n_milli = now.millitm;
	then.time = abstime->tv_sec;
	t_milli = abstime->tv_nsec/1000000;

	if((then.time > now.time || 
	   (then.time == now.time && t_milli > n_milli))) {
	    if((t_milli -= n_milli) < 0) {
		t_milli += 1000;
		then.time--;
	    }
	    then.time -= now.time;

	    if ((then.time + (clock() / CLOCKS_PER_SEC)) <= 50000000) {
		/*
		 * Under NT, we can only wait for milliseconds, so we
		 * round up the wait time here.
		 */
		rc = cond_wait_internal(cond, mutex, 
				 (DWORD)((then.time * 1000) + (t_milli)));
	    } else {
		rc = EINVAL;
	    }
	} else {
	    rc = ETIME;
	}
    } else {
	rc = EINVAL;
    }

    return rc;
}

int pthread_cond_signal(pthread_cond_t *cond) {
    int rc = 0;
    cond_waiters_t *release_thread;

    if (cond != NULL) {
	EnterCriticalSection(&cond->cs);

	/*
	 * remove the first waiting thread from the queue
	 * and resume his execution
	 */
	if (queue_IsNotEmpty(&cond->waiting_threads)) {
	    release_thread = queue_First(&cond->waiting_threads,
					 cond_waiter);
	    queue_Remove(release_thread);
	    if (!SetEvent(release_thread->event)) {
		rc = -1;
	    }
	}

	LeaveCriticalSection(&cond->cs);
    } else {
	rc = EINVAL;
    }

    return rc;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
    int rc = 0;
    cond_waiters_t *release_thread, *next_thread;

    if(cond != NULL) {
	EnterCriticalSection(&cond->cs);

	/*
	 * Empty the waiting_threads queue. 
	 */
	if (queue_IsNotEmpty(&cond->waiting_threads)) {
	    for(queue_Scan(&cond->waiting_threads, release_thread,
			   next_thread, cond_waiter)) {
		queue_Remove(release_thread);
		if (!SetEvent(release_thread->event)) {
		    rc = -1;
		}
	    }
	}

	LeaveCriticalSection(&cond->cs);
    } else {
	rc = EINVAL;
    }

    return rc;
}

int pthread_cond_destroy(pthread_cond_t *cond) {
    int rc = 0;

    if (cond != NULL) {
	DeleteCriticalSection(&cond->cs);
    } else {
	rc = EINVAL;
    }
	
    /*
     * A previous version of this file had code to check the waiter
     * queue and empty it here.  This has been removed in the hopes
     * that it will aid in debugging.
     */

    return rc;
}

int pthread_join(pthread_t target_thread, void **status) {
    int rc = 0;
    thread_p me, target;
    thread_p cur, next;

    target = (thread_p) target_thread;
    me = (thread_p) pthread_self();

    if (me != target) {
	/*
	 * Check to see that the target thread is joinable and hasn't
	 * already been joined.
	 */

	pthread_mutex_lock(&active_Q_mutex);

	for(queue_Scan(&active_Q, cur, next, thread)) {
	    if (target == cur) break;
	}

	if (target == cur) {
	    if ((!target->is_joinable) || (target->has_been_joined)) {
		rc = ESRCH;
	    }
	} else {
	    rc = ESRCH;
	}

	if (rc) {
	    pthread_mutex_unlock(&active_Q_mutex);
	    return rc;
	}

	target->waiter_count++;
	while(target->running) {
	    pthread_cond_wait(&target->wait_terminate, &active_Q_mutex);
	}

	/*
	 * Only one waiter gets the status and is allowed to join, all the
	 * others get an error.
	 */

	if (target->has_been_joined) {
	    rc = ESRCH;
	} else {
	    target->has_been_joined = 1;
	    if (status) {
		*status = target->rc;
	    }
	}

	/*
	 * If we're the last waiter it is our responsibility to remove
	 * this entry from the terminated list and put it back in the
	 * cache.
	 */

	target->waiter_count--;
	if (target->waiter_count == 0) {
	    queue_Remove(target);
	    pthread_mutex_unlock(&active_Q_mutex);
	    put_thread(target);
	} else {
	    pthread_mutex_unlock(&active_Q_mutex);
	}
    } else {
	rc = EDEADLK;
    }

    return rc;
}

/*
 * Note that we can't return an error from pthread_getspecific so
 * we return a NULL pointer instead.
 */

void *pthread_getspecific(pthread_key_t key) {
    void *rc = NULL;
    char **tsd = TlsGetValue(tsd_index);

    if (tsd == NULL)
        return NULL;

    if ((key > -1) && (key < PTHREAD_KEYS_MAX )) {
	rc = (void *) *(tsd + key);
    }

    return rc;
}

static int p_tsd_done;

static void pthread_tsd_init(void) {
    pthread_mutex_init(&pthread_tsd_mutex, (const pthread_mutexattr_t*)0);
    p_tsd_done = 1;
}

int pthread_key_create(pthread_key_t *keyp, void (*destructor)(void *value)) {
    int rc = 0;
    int i;

    if (p_tsd_done || (!pthread_once(&pthread_tsd_once, pthread_tsd_init))) {
	if (!pthread_mutex_lock(&pthread_tsd_mutex)) {
	    for(i=0;i<PTHREAD_KEYS_MAX;i++) {
		if (!keys[i].inuse) break;
	    }

	    if (!keys[i].inuse) {
		keys[i].inuse = 1;
		keys[i].destructor = destructor;
		*keyp = i;
	    } else {
		rc = EAGAIN;
	    }
	    pthread_mutex_unlock(&pthread_tsd_mutex);
	} else {
	    rc = -1;
	}
    } else {
	rc = -2;
    }

    return rc;
}

int pthread_key_delete(pthread_key_t key) {
    int rc = 0;

    if (p_tsd_done || (!pthread_once(&pthread_tsd_once, pthread_tsd_init))) {
	if ((key > -1) && (key < PTHREAD_KEYS_MAX )) {
	    if (!pthread_mutex_lock(&pthread_tsd_mutex)) {
		keys[key].inuse = 0;
		keys[key].destructor = NULL;
		pthread_mutex_unlock(&pthread_tsd_mutex);
	    } else {
		rc = -1;
	    }
	} else {
	    rc = EINVAL;
	}
    } else {
	rc = -2;
    }

    return rc;
}

int pthread_setspecific(pthread_key_t key, const void *value) {
    int rc = 0;
    char **tsd;

    /* make sure all thread-local storage has been allocated */
    pthread_self();

    if (p_tsd_done || (!pthread_once(&pthread_tsd_once, pthread_tsd_init))) {
	if ((key > -1) && (key < PTHREAD_KEYS_MAX )) {
	    if (!pthread_mutex_lock(&pthread_tsd_mutex)) {
		if (keys[key].inuse) {
		    tsd = TlsGetValue(tsd_index);
		    *(tsd + key) = (char *) value;
		} else {
		    rc = EINVAL;
		}
		pthread_mutex_unlock(&pthread_tsd_mutex);
	    } else {
		rc = -1;
	    }
	} else {
	    rc = EINVAL;
	}
    } else {
      rc = -2;
    }

    return rc;
}

pthread_t pthread_self(void) {
    thread_p cur;
    DWORD my_id = GetCurrentThreadId();

    (pthread_cache_done || pthread_once(&pthread_cache_once, create_once));
    (tsd_done || pthread_once(&global_tsd_once, tsd_once));

    pthread_mutex_lock(&active_Q_mutex);

    cur = TlsGetValue (tsd_pthread_index);

    if(!cur) {
	/*
	 * This thread's ID was not found in our list of pthread-API client
	 * threads (e.g., those threads created via pthread_create). Create
	 * an entry for it.
	 */
	if ((cur = get_thread()) != NULL) {
	    cur->is_joinable = 0;
	    cur->NT_id = my_id;
	    cur->native_thread = 1;
	    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
             GetCurrentProcess(), &cur->t_handle, 0,
             TRUE, DUPLICATE_SAME_ACCESS);

	    /*
	     * We'll also need a place to store key data for this thread
	     */
	    if ((cur->tsd = malloc(sizeof(char*) * PTHREAD_KEYS_MAX)) != NULL) {
		memset(cur->tsd, 0, (sizeof(char*) * PTHREAD_KEYS_MAX));
	    }
	    TlsSetValue(tsd_index, (LPVOID)cur->tsd);
	    TlsSetValue(tsd_pthread_index, (LPVOID)cur);

	    /*
	     * The thread_t structure is complete; add it to the active_Q
	     */
	    queue_Prepend(&active_Q, cur);

	    /*
	     * We were able to successfully insert a new entry into the
	     * active_Q; however, when this thread terminates, we will need
	     * to know about it. The pthread_sync_terminate_thread() routine
	     * will make sure there is a dedicated thread waiting for any
	     * native-thread entries in the active_Q to terminate.
	     */
	    pthread_sync_terminate_thread();
	}
    }

    pthread_mutex_unlock(&active_Q_mutex);

    return (void *) cur;
}

int pthread_equal(pthread_t t1, pthread_t t2) {
    return (t1 == t2);
}

int pthread_attr_destroy(pthread_attr_t *attr) {
    int rc = 0;

    return rc;
}

int pthread_attr_init(pthread_attr_t *attr) {
    int rc = 0;

    if (attr != NULL) {
	attr->is_joinable = PTHREAD_CREATE_JOINABLE;
    } else {
	rc = EINVAL;
    }

    return rc;
}

int pthread_attr_getdetachstate(pthread_attr_t *attr, int *detachstate) {
    int rc = 0;

    if ((attr != NULL) && (detachstate != NULL)) {
	    *detachstate = attr->is_joinable;
    } else {
	rc = EINVAL;
    }
    return rc;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate) {
    int rc = 0;

    if ((attr != NULL) && ((detachstate == PTHREAD_CREATE_JOINABLE) ||
	(detachstate == PTHREAD_CREATE_DETACHED))) {
	attr->is_joinable = detachstate;
    } else {
	rc = EINVAL;
    }
    return rc;
}

void pthread_exit(void *status) {
    thread_p me = (thread_p) pthread_self();

    /*
     * Support pthread_exit for thread's created by calling pthread_create
     * only.  Do this by using an exception that will transfer control
     * back to afs_pthread_create_stub.  Store away our status before
     * returning.
     *
     * If this turns out to be a native thread, the exception will be
     * unhandled and the process will terminate.
     */

    me->rc = status;
    RaiseException(PTHREAD_EXIT_EXCEPTION, 0, 0, NULL);

}
