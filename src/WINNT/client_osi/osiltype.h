/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#ifndef _OSILTYPE_H_ENV_
#define _OSILTYPE_H_ENV_ 1

/* number of dynamic lock types we permit */
#define OSI_NLOCKTYPES		32	/* should be enough */

/* the set of procedures that we subclass to make a new lock
 * implementation.
 */
typedef struct osi_lockOps {
	void (*ObtainReadProc)(struct osi_rwlock *);
	void (*ObtainWriteProc)(struct osi_rwlock *);
	void (*ReleaseReadProc)(struct osi_rwlock *);
	void (*ReleaseWriteProc)(struct osi_rwlock *);
	void (*ObtainMutexProc)(struct osi_mutex *);
	void (*ReleaseMutexProc)(struct osi_mutex *);
	int (*TryReadProc)(struct osi_rwlock *);
	int (*TryWriteProc)(struct osi_rwlock *);
	int (*TryMutexProc)(struct osi_mutex *);
	void (*SleepRProc)(LONG_PTR, struct osi_rwlock *);
	void (*SleepWProc)(LONG_PTR, struct osi_rwlock *);
	void (*SleepMProc)(LONG_PTR, struct osi_mutex *);
	void (*InitializeMutexProc)(struct osi_mutex *, char *);
	void (*InitializeRWLockProc)(struct osi_rwlock *, char *);
	void (*FinalizeMutexProc)(struct osi_mutex *);
	void (*FinalizeRWLockProc)(struct osi_rwlock *);
        void (*ConvertWToRProc)(struct osi_rwlock *);
        int (*GetRWLockState)(struct osi_rwlock *);
        int (*GetMutexState)(struct osi_mutex *);
} osi_lockOps_t;

/* operation vectors for lock ops */
extern osi_lockOps_t *osi_lockOps[OSI_NLOCKTYPES];

extern int osi_lockTypeDefault;

/* external procedures */
void osi_LockTypeAdd(osi_lockOps_t *, char *, int *);

int osi_LockTypeSetDefault(char *);

/* bits for GetRWLockInfo and GetMutexInfo return values */
#define OSI_MUTEX_HELD		1		/* mutex is held */
#define OSI_RWLOCK_READHELD	1		/* locked for read */
#define OSI_RWLOCK_WRITEHELD	2		/* locked for write */

#endif /* _OSILTYPE_H_ENV_ */
