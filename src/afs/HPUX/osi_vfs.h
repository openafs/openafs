/*
 * HPUX specific vfs related defines (from irix)
 */
#ifndef _HPUX_VFS_H_
#define _HPUX_VFS_H_
/*
 * Flock(3) call. (from sys/file.h)
 */
#define LOCK_SH         1	/* shared lock */
#define LOCK_EX         2	/* exclusive lock */
#define LOCK_NB         4	/* don't block when locking */
#define LOCK_UN         8	/* unlock */

#define d_fileno d_ino

#define splclock() spl7()

#endif
