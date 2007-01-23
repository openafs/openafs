#ifndef AFS_FAKE_H
#define AFS_FAKE_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

/* Begin Type Cheats */

typedef int afs_int32; /* Is EiC long != int? */
typedef unsigned long osi_timeval_t;

#define afs_warn printf

#define afs_osi_Alloc osi_Alloc
#define osi_Alloc malloc

#define afs_osi_FreeStr(x) \
    osi_Free((x), strlen(x))

#define afs_osi_Free osi_Free
#define osi_Free(x,y) \
	free((x))

#define afs_strdup strdup
#define afs_strcat strcat
#define afs_strcasecmp strcasecmp


/* End Type Cheats */

/* Begin lock.h fragment */

struct afs_lock {
    unsigned char wait_states;	/* type of lockers waiting */
    unsigned char excl_locked;	/* anyone have boosted, shared or write lock? */
    unsigned short readers_reading;	/* # readers actually with read locks */
    unsigned short num_waiting;	/* probably need this soon */
    unsigned short spare;	/* not used now */
    osi_timeval_t time_waiting;	/* for statistics gathering */
#if defined(INSTRUMENT_LOCKS)
    /* the following are useful for debugging 
     ** the field 'src_indicator' is updated only by ObtainLock() and
     ** only for writes/shared  locks. Hence, it indictes where in the
     ** source code the shared/write lock was set.
     */
    unsigned int pid_last_reader;	/* proceess id of last reader */
    unsigned int pid_writer;	/* process id of writer, else 0 */
    unsigned int src_indicator;	/* third param to ObtainLock() */
#endif				/* INSTRUMENT_LOCKS */
};
typedef struct afs_lock afs_lock_t;
typedef struct afs_lock afs_rwlock_t;

#define ObtainWriteLock(lock, mode) \
{ }
 
#define ReleaseWriteLock(lock) \
{ }

#define RWLOCK_INIT(lock, str) \
{    printf("Initializing lock %s\n", str); }

/* End lock.h fragment */

/* Begin AFS Q Pkg (afs.h) */

/*
  * Operations on circular queues implemented with pointers.  Note: these queue
  * objects are always located at the beginning of the structures they are linking.
  */

struct afs_q {
    struct afs_q *next;
    struct afs_q *prev;
};

#define	QInit(q)    ((q)->prev = (q)->next = (q))
#define	QAdd(q,e)   ((e)->next = (q)->next, (e)->prev = (q), \
			(q)->next->prev = (e), (q)->next = (e))
#define	QRemove(e)  ((e)->next->prev = (e)->prev, (e)->prev->next = (e)->next)
#define	QNext(e)    ((e)->next)
#define QPrev(e)    ((e)->prev)
#define QEmpty(q)   ((q)->prev == (q))
/* this one takes q1 and sticks it on the end of q2 - that is, the other end, not the end
 * that things are added onto.  q1 shouldn't be empty, it's silly */
#define QCat(q1,q2) ((q2)->prev->next = (q1)->next, (q1)->next->prev=(q2)->prev, (q1)->prev->next=(q2), (q2)->prev=(q1)->prev, (q1)->prev=(q1)->next=(q1))
/*
 * Do lots of address arithmetic to go from vlruq to the base of the vcache
 * structure.  Don't move struct vnode, since we think of a struct vcache as
 * a specialization of a struct vnode
 */
#define	QTOV(e)	    ((struct vcache *)(((char *) (e)) - (((char *)(&(((struct vcache *)(e))->vlruq))) - ((char *)(e)))))
#define	QTOC(e)	    ((struct cell *)((char *) (e)))

#endif /* AFS_FAKE_H */

