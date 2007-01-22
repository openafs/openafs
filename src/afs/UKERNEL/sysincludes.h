/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __AFS_SYSINCLUDESH__
#define __AFS_SYSINCLUDESH__ 1

#include  <stdio.h>
#if !defined(AFS_USR_DARWIN_ENV) && !defined(AFS_USR_FBSD_ENV)	/* must be included after KERNEL undef'd */
#include  <errno.h>
#endif
#include  <stdlib.h>
#include  <string.h>
#include  <unistd.h>
#include  <dirent.h>
#include  <limits.h>
#include  <assert.h>
#include  <stdarg.h>
#if !defined(AFS_USR_DARWIN_ENV) && !defined(AFS_USR_FBSD_ENV) /* must be included after KERNEL undef'd */
#include <setjmp.h>
#endif

#ifdef AFS_USR_SUN5_ENV
#include  <signal.h>
#include  <sys/param.h>
#include  <sys/types.h>
#include  <sys/socket.h>
#include  <net/if.h>
#include  <sys/sockio.h>
#include  <sys/file.h>
#include  <sys/stat.h>
#include  <sys/fcntl.h>
#include  <netinet/in.h>
#include  <netdb.h>
#include  <arpa/inet.h>
#ifdef OSI_SUN57_ENV
/* 
 * XXX putrid hack becuase 
 * of the awful #define mess down below
 */
#include  <libdevinfo.h>
#endif /* OSI_SUN57_ENV */
#endif /* AFS_USR_SUN5_ENV */


#ifdef AFS_USR_AIX_ENV
#include  <sys/param.h>
#include  <sys/types.h>
#include  <sys/socket.h>
#include  <net/if.h>
#include  <fcntl.h>
#include  <netinet/in.h>
#include  <sys/stropts.h>
#include  <netdb.h>
#include  <sys/timers.h>
#include  <arpa/inet.h>
#endif /* AFS_USR_AIX_ENV */

#ifdef AFS_USR_SGI_ENV
#include  <sys/param.h>
#include  <sys/types.h>
#include  <sys/socket.h>
#include  <net/if.h>
#include  <sys/sockio.h>
#include  <sys/file.h>
#include  <sys/stat.h>
#include  <sys/fcntl.h>
#include  <netinet/in.h>
#include  <netdb.h>
#include  <arpa/inet.h>
#endif /* AFS_USR_SGI_ENV */

#ifdef AFS_USR_HPUX_ENV
#include  <sys/param.h>
#include  <sys/types.h>
#include  <sys/socket.h>
#include  <net/if.h>
#include  <sys/file.h>
#include  <sys/stat.h>
#include  <sys/fcntl.h>
#include  <netinet/in.h>
#include  <netdb.h>
#include  <arpa/inet.h>
#endif /* AFS_USR_HPUX_ENV */

#ifdef AFS_USR_OSF_ENV
#ifdef KERNEL
#undef KERNEL
#define AFS_USR_UNDEF_KERNEL_ENV 1
#endif
#include  <sys/param.h>
#include  <sys/types.h>
#include  <sys/socket.h>
#include  <net/if.h>
#include  <sys/file.h>
#include  <sys/ioctl.h>
#include  <sys/stat.h>
#include  <sys/fcntl.h>
#include  <netinet/in.h>
#include  <netdb.h>
#include  <arpa/inet.h>
#endif /* AFS_USR_OSF_ENV */

#ifdef AFS_USR_LINUX22_ENV
#include  <sys/ioctl.h>		/* _IOW() */
#include  <sys/uio.h>		/* struct iovec */
#include  <sys/time.h>		/* struct timeval */
#include  <sys/param.h>
#include  <sys/types.h>
#include  <sys/socket.h>
#include  <net/if.h>
#include  <sys/file.h>
#include  <sys/stat.h>
#include  <sys/fcntl.h>
#include  <netinet/in.h>
#include  <netdb.h>
#include  <arpa/inet.h>
#define FREAD			0x0001
#endif /* AFS_USR_LINUX22_ENV */

#if defined(AFS_USR_DARWIN_ENV) || defined(AFS_USR_FBSD_ENV)
#ifdef KERNEL
#undef KERNEL
#define AFS_USR_UNDEF_KERNEL_ENV 1
#endif
#include  <errno.h>
#include  <setjmp.h>
#include  <sys/param.h>
#include  <sys/types.h>
#include  <sys/socket.h>
#include  <net/if.h>
#include  <sys/file.h>
#include  <sys/ioctl.h>
#include  <sys/stat.h>
#include  <sys/fcntl.h>
#include  <sys/uio.h>
#include  <netinet/in.h>
#include  <netdb.h>
#include  <arpa/inet.h>
#ifndef O_SYNC
#define O_SYNC O_FSYNC
#endif
#endif /* AFS_USR_DARWIN_ENV || AFS_USR_FBSD_ENV */

#ifdef AFS_AFSDB_ENV
#include <arpa/nameser.h>
#include <resolv.h>
#endif /* AFS_AFSDB_ENV */

/* glibc 2.2 has pthread_attr_setstacksize */
#if (defined(AFS_LINUX22_ENV) && !defined(AFS_USR_LINUX22_ENV)) || (defined(AFS_USR_LINUX22_ENV) && (__GLIBC_MINOR__ < 2))
#define pthread_attr_setstacksize(a,b) 0
#endif

#include  <sys/stat.h>		/* afs_usrops.h uses struct stat in prototypes */

#ifdef NETSCAPE_NSAPI

#include  <nsapi.h>

#else /* NETSCAPE_NSAPI */

#include  <pthread.h>

#endif /* NETSCAPE_NSAPI */

#ifdef AFS_USR_UNDEF_KERNEL_ENV
#undef AFS_USR_UNDEF_KERNEL_ENV
#define KERNEL 1
#endif

/*
 * User space versions of kernel data structures.
 */

#ifndef MAXNAMLEN
#define MAXNAMLEN		512
#endif

/*
 * This file contains data types and definitions for running
 * the AFS client in user space. Kernel data structures
 * are renamed from XXXX to usr_XXXX.
 *
 * this is absolutely completely wretched and needs
 * to be rearchitected
 */

#ifdef UKERNEL

#ifdef AFS_USR_SGI_ENV
#undef socket
#endif /* AFS_USR_SGI_ENV */

#if defined(AFS_USR_DARWIN_ENV) || defined(AFS_USR_FBSD_ENV)
#undef if_mtu
#undef if_metric
#endif

#define mount			usr_mount
#define fs			usr_fs
#define uio			usr_uio
#define fileops			usr_fileops
#define vnodeops		usr_vnodeops
#define vnode			usr_vnode
#define inode			usr_inode
#define whymountroot_t		usr_whymountroot_t
#define vfsops			usr_vfsops
#define vfs			usr_vfs
#define vattr			usr_vattr
#define buf			usr_buf
#define statfs			usr_statfs
#define ucred			usr_ucred
#define user			usr_user
#define proc			usr_proc
#define file			usr_file
#define dirent			usr_dirent
#define flock			usr_flock
#define fid			usr_fid
#define sysent			usr_sysent
#define in_ifaddr		usr_in_ifaddr
#define ifaddr			usr_ifaddr
#define ifnet			usr_ifnet
#define socket			usr_socket
#define crget			usr_crget
#define crcopy			usr_crcopy
#define crhold			usr_crhold
#define crfree			usr_crfree
#define vtype_t			usr_vtype_t
#define vcexcl			usr_vcexcl
#define m_free			usr_m_free
#define m_freem			usr_m_freem
#define m_adj			usr_m_adj
#define m_pullup		usr_m_pullup
#define uiomove			usr_uiomove
#define EXCL			usr_EXCL
#define NONEXCL			usr_NONEXCL
#define uio_rw			usr_uio_rw
#ifdef ino_t
#undef ino_t
#endif
#define ino_t			usr_ino_t
#define offset_t		usr_offset_t
#define getpid()		usr_getpid()
#define setpag(A,B,C,D)		usr_setpag((A),(B),(C),(D))
#ifdef pid_t
#undef pid_t
#endif
#define pid_t			int

enum usr_vcexcl { usr_NONEXCL, usr_EXCL };
typedef long offset_t;
#ifdef AFS_USR_OSF_ENV
typedef int usr_ino_t;
#else /* AFS_USR_OSF_ENV */
typedef long usr_ino_t;
#endif /* AFS_USR_OSF_ENV */

#if defined(AFS_USR_AIX_ENV) || defined(AFS_USR_SGI_ENV)
#define SYS_setgroups		101
#endif

#define ioctl()			usr_ioctl()

#define label_t			jmp_buf

#ifdef VFSTOM
#undef VFSTOM
#endif

#define	VFSTOM(VP)		((struct usr_mount *)(VP)->vfs_mount)

#ifdef VINACT
#undef VINACT
#endif
#ifdef VLOCK
#undef VLOCK
#endif
#ifdef VNOMAP
#undef VNOMAP
#endif
#ifdef VROOT
#undef VROOT
#endif
#ifdef VSHARE
#undef VSHARE
#endif
#ifdef VTEXT
#undef VTEXT
#endif
#ifdef VWAIT
#undef VWAIT
#endif
#ifdef VWASMAP
#undef VWASMAP
#endif
#ifdef VXLOCK
#undef VXLOCK
#endif

#define VINACT			0x0001
#define VLOCK			0x0002
#define VNOMAP			0x0004
#define VROOT			0x0008
#define VSHARE			0x0010
#define VTEXT			0x0020
#define VWAIT			0x0040
#define VWASMAP			0x0080
#define VXLOCK			0x0100

#ifdef VNON
#undef VNON
#endif
#ifdef VREG
#undef VREG
#endif
#ifdef VDIR
#undef VDIR
#endif
#ifdef VBLK
#undef VBLK
#endif
#ifdef VCHR
#undef VCHR
#endif
#ifdef VLNK
#undef VLNK
#endif
#ifdef VFIFO
#undef VFIFO
#endif
#ifdef VDOOR
#undef VDOOR
#endif
#ifdef VBAD
#undef VBAD
#endif
#ifdef VSOCK
#undef VSOCK
#endif

#define VNON			0
#define VREG			1
#define VDIR			2
#define VBLK			3
#define VCHR			4
#define VLNK			5
#define VFIFO			6
#define VDOOR			7
#define VBAD			8
#define VSOCK			9

typedef int usr_vtype_t;

#ifdef VOP_RDWR
#undef VOP_RDWR
#endif

#define VOP_RDWR		afs_osi_VOP_RDWR

#ifdef NDADDR
#undef NDADDR
#endif
#ifdef NIADDR
#undef NIADDR
#endif

#define NDADDR			12
#define NIADDR			3

#ifdef DTYPE_VNODE
#undef DTYPE_VNODE
#endif

#define DTYPE_VNODE		1

#ifdef IUPD
#undef IUPD
#endif
#ifdef IACC
#undef IACC
#endif
#ifdef IMOD
#undef IMOD
#endif
#ifdef ICHG
#undef ICHG
#endif
#ifdef INOACC
#undef INOACC
#endif
#ifdef IMODTIME
#undef IMODTIME
#endif
#ifdef IREF
#undef IREF
#endif
#ifdef ISYNC
#undef ISYNC
#endif
#ifdef IFASTSYMLNK
#undef IFASTSYMLNK
#endif
#ifdef IMODACC
#undef IMODACC
#endif
#ifdef IATTCHG
#undef IATTCHG
#endif
#ifdef IBDWRITE
#undef IBDWRITE
#endif
#ifdef IBAD
#undef IBAD
#endif
#ifdef IDEL
#undef IDEL
#endif

#define IUPD			0x0001
#define IACC			0x0002
#define IMOD			0x0004
#define ICHG			0x0008
#define INOACC			0x0010
#define IMODTIME		0x0020
#define IREF			0x0040
#define ISYNC			0x0080
#define IFASTSYMLNK		0x0100
#define IMODACC			0x0200
#define IATTCHG			0x0400
#define IBDWRITE		0x0800
#define IBAD			0x1000
#define IDEL			0x2000

#ifdef IFMT
#undef IFMT
#endif
#ifdef IFIFO
#undef IFIFO
#endif
#ifdef IFCHR
#undef IFCHR
#endif
#ifdef IFDIR
#undef IFDIR
#endif
#ifdef IFBLK
#undef IFBLK
#endif
#ifdef IFREG
#undef IFREG
#endif
#ifdef IFLNK
#undef IFLNK
#endif
#ifdef IFSHAD
#undef IFSHAD
#endif
#ifdef IFSOCK
#undef IFSOCK
#endif

#define IFMT			0170000
#define IFIFO			0010000
#define IFCHR			0020000
#define IFDIR			0040000
#define IFBLK			0060000
#define IFREG			0100000
#define IFLNK			0120000
#define IFSHAD			0130000
#define IFSOCK			0140000

#ifdef ISUID
#undef ISUID
#endif
#ifdef ISGID
#undef ISGID
#endif
#ifdef ISVTX
#undef ISVTX
#endif
#ifdef IREAD
#undef IREAD
#endif
#ifdef IWRITE
#undef IWRITE
#endif
#ifdef IEXEC
#undef IEXEC
#endif

#define ISUID			04000
#define ISGID			02000
#define ISVTX			01000
#define IREAD			0400
#define IWRITE			0200
#define IEXEC			0100

#ifdef I_SYNC
#undef I_SYNC
#endif
#ifdef I_DSYNC
#undef I_DSYNC
#endif
#ifdef I_ASYNC
#undef I_ASYNC
#endif

#define I_SYNC			1
#define I_DSYNC			2
#define I_ASYNC			0

#ifdef I_FREE
#undef I_FREE
#endif
#ifdef I_DIR
#undef I_DIR
#endif
#ifdef I_IBLK
#undef I_IBLK
#endif
#ifdef I_CHEAP
#undef I_CHEAP
#endif
#ifdef I_SHAD
#undef I_SHAD
#endif
#ifdef I_QUOTA
#undef I_QUOTA
#endif

#define I_FREE			0x00000001
#define I_DIR			0x00000002
#define I_IBLK			0x00000004
#define I_CHEAP			0x00000008
#define I_SHAD			0x00000010
#define I_QUOTA			0x00000020

#ifdef VTOI
#undef VTOI
#endif
#ifdef ITOV
#undef ITOV
#endif

#define	VTOI(VP)	((struct usr_inode *)(VP)->v_data)
#define	ITOV(IP)	((struct usr_vnode *)&(IP)->i_vnode)

#ifdef VN_HOLD
#undef VN_HOLD
#endif
#ifdef VN_RELE
#undef VN_RELE
#endif

#ifdef ROOT_INIT
#undef ROOT_INIT
#endif
#ifdef ROOT_REMOUNT
#undef ROOT_REMOUNT
#endif
#ifdef ROOT_UNMOUNT
#undef ROOT_UNMOUNT
#endif
#ifdef ROOT_FRONTMOUNT
#undef ROOT_FRONTMOUNT
#endif
#ifdef ROOT_BACKMOUNT
#undef ROOT_BACKMOUNT
#endif

#define ROOT_INIT			0x0001
#define ROOT_REMOUNT			0X0002
#define ROOT_UNMOUNT			0x0003
#define ROOT_FRONTMOUNT			0x0004
#define ROOT_BACKMOUNT			0x0005

#ifdef	MAXFIDSZ
#undef	MAXFIDSZ
#endif

#define	MAXFIDSZ			64

#ifdef FSTYPSZ
#undef FSTYPSZ
#endif

#define FSTYPSZ				16

#ifdef	VFS_MOUNT
#undef	VFS_MOUNT
#endif
#ifdef	VFS_UNMOUNT
#undef	VFS_UNMOUNT
#endif
#ifdef	VFS_ROOT
#undef	VFS_ROOT
#endif
#ifdef	VFS_STATFS
#undef	VFS_STATFFS
#endif
#ifdef	VFS_SYNC
#undef	VFS_SYNC
#endif
#ifdef	VFS_VGET
#undef	VFS_VGET
#endif
#ifdef	VFS_MOUNTROOT
#undef	VFS_MOUNTROOT
#endif
#ifdef	VFS_SWAPVP
#undef	VFS_SWAPVP
#endif
#ifdef	VFS_MOUNT
#undef	VFS_MOUNT
#endif

#define	VFS_STATFS(vfsp, sp)	((sp)->f_bsize=4096, 0)

#ifdef FAPPEND
#undef FAPPEND
#endif
#ifdef FSYNC
#undef FSYNC
#endif
#ifdef FTRUNC
#undef FTRUNC
#endif
#ifdef FWRITE
#undef FWRITE
#endif
#ifdef IO_APPEND
#undef IO_APPEND
#endif
#ifdef IO_SYNC
#undef IO_SYNC
#endif

#define FAPPEND			0x0100
#define IO_APPEND		FAPPEND
#define FSYNC			0x0200
#define IO_SYNC			FSYNC
#define FTRUNC			0x0400
#define FWRITE			0x0800

#ifdef F_GETLK
#undef F_GETLK
#endif
#ifdef F_RDLCK
#undef F_RDLCK
#endif
#ifdef F_SETLK
#undef F_SETLK
#endif
#ifdef F_SETLKW
#undef F_SETLKW
#endif
#ifdef F_UNLCK
#undef F_UNLCK
#endif
#ifdef F_WRLCK
#undef F_WRLCK
#endif

#define F_GETLK			0x0001
#define F_RDLCK			0x0002
#define F_SETLK			0x0003
#define F_SETLKW		0x0004
#define F_UNLCK			0x0005
#define F_WRLCK			0x0006

#ifdef LOCK_SH
#undef LOCK_SH
#endif
#ifdef LOCK_EX
#undef LOCK_EX
#endif
#ifdef LOCK_NB
#undef LOCK_NB
#endif
#ifdef LOCK_UN
#undef LOCK_UN
#endif

#define LOCK_SH			F_RDLCK
#define LOCK_UN			F_UNLCK
#define LOCK_EX			F_WRLCK
#define LOCK_NB			0x0007

#ifdef FEXLOCK
#undef FEXLOCK
#endif
#ifdef FSHLOCK
#undef FSHLOCK
#endif

#define FEXLOCK			F_WRLCK
#define FSHLOCK			F_RDLCK

#ifdef SSYS
#undef SSYS
#endif

#define SSYS			0x0001

enum usr_uio_rw { USR_UIO_READ, USR_UIO_WRITE };

#ifdef UIO_READ
#undef UIO_READ
#endif
#ifdef UIO_WRITE
#undef UIO_WRITE
#endif

#define UIO_READ		0x0000
#define UIO_WRITE		0x0001

#ifdef UIO_USERSPACE
#undef UIO_USERSPACE
#endif
#ifdef UIO_SYSSPACE
#undef UIO_SYSSPACE
#endif

#define UIO_USERSPACE		0x0000
#define UIO_SYSSPACE		0x0001

#ifdef B_AGE
#undef B_AGE
#endif
#ifdef B_ASYNC
#undef B_ASYNC
#endif
#ifdef B_DELWRI
#undef B_DELWRI
#endif
#ifdef B_DIRTY
#undef B_DIRTY
#endif
#ifdef B_DONE
#undef B_DONE
#endif
#ifdef B_ERROR
#undef B_ERROR
#endif
#ifdef B_FREE
#undef B_FREE
#endif
#ifdef B_NOCACHE
#undef B_NOCACHE
#endif
#ifdef B_PFSTORE
#undef B_PFSTORE
#endif
#ifdef B_READ
#undef B_READ
#endif
#ifdef B_UBC
#undef B_UBC
#endif
#ifdef B_WANTED
#undef B_WANTED
#endif
#ifdef B_WRITE
#undef B_WRITE
#endif

#define B_AGE			0x0001
#define B_ASYNC			0x0002
#define B_DELWRI		0x0004
#define B_DIRTY			0x0008
#define B_DONE			0x0010
#define B_ERROR			0x0020
#define B_FREE			0x0040
#define B_NOCACHE		0x0080
#define B_PFSTORE		0x0100
#define B_READ			0x0200
#define B_UBC			0x0400
#define B_WANTED		0x0800
#define B_WRITE			0x1000

#ifdef MFREE
#undef MFREE
#endif
#ifdef MINUSE
#undef MINUSE
#endif
#ifdef MINTER
#undef MINTER
#endif
#ifdef MUPDATE
#undef MUPDATE
#endif

#define	MFREE			0
#define	MINUSE			1
#define	MINTER			2
#define	MUPDATE			4

#ifdef MSIZE
#undef MSIZE
#endif
#ifdef MMAXOFF
#undef MMAXOFF
#endif

#define MSIZE			16384
#define MMAXOFF			16384

#ifdef IA_SIN
#undef IA_SIN
#endif

#define   IA_SIN(IA)		(&(IA)->ia_addr)

#ifdef mtod
#undef mtod
#endif
#ifdef dtom
#undef dtom
#endif
#ifdef mtocl
#undef mtocl
#endif

#define mtod(m,t)       ((t)((m)->m_data))

#ifdef NBPG
#undef NBPG
#endif
#define NBPG			4096

#define panic(S)		do{fprintf(stderr, S);assert(0);}while(0)
#define abort()			assert(0)
#define usr_assert(A)		assert(A)

#ifdef NETSCAPE_NSAPI

/*
 * All CONDVARs created with the same CRITICAL end up being the
 * same CONDVAR, not a new one. If we want to use more than
 * one usr_cond_t with the same usr_mutex_t, then we need a CRITICAL
 * for each CONDVAR, otherwise we cannot know which thread we are
 * waking when we do the signal.
 */
typedef struct {
    int waiters;
    CRITICAL lock;
    CONDVAR cond;
} usr_cond_t;

#define usr_mutex_t		CRITICAL
#define usr_thread_t		SYS_THREAD
#define usr_key_t		int

#define usr_mutex_init(A)	(*(A)=crit_init(), 0)
#define usr_mutex_destroy(A)	(crit_terminate(*(A)), 0)
#define usr_mutex_lock(A)	crit_enter(*(A))
#define usr_mutex_trylock(A)	(crit_enter(*(A)),1)
#define usr_mutex_unlock(A)	crit_exit(*(A))

#define usr_cond_init(A)	\
     ((A)->waiters = 0,		\
      (A)->lock = crit_init(),	\
      (A)->cond = condvar_init((A)->lock), 0)

#define usr_cond_destroy(A)	\
    (condvar_terminate((A)->cond), \
     crit_terminate((A)->lock), 0)

#define usr_cond_signal(A)	\
{				\
    crit_enter((A)->lock);	\
    if ((A)->waiters != 0) {	\
      condvar_notify((A)->cond);\
      (A)->waiters -= 1;	\
    }				\
    crit_exit((A)->lock);	\
}

#define usr_cond_broadcast(A)	\
{				\
   crit_enter((A)->lock);	\
   while ((A)->waiters != 0) {	\
     condvar_notify((A)->cond);	\
     (A)->waiters -= 1;		\
   }				\
   crit_exit((A)->lock);	\
}

#define usr_cond_wait(A,B)	\
    (crit_enter((A)->lock),	\
     crit_exit(*(B)),		\
     (A)->waiters += 1,		\
     condvar_wait((A)->cond),	\
     crit_exit((A)->lock),	\
     crit_enter(*(B)), 0)

#define usr_thread_create(A,B,C) \
    ((*(A)=systhread_start(SYSTHREAD_DEFAULT_PRIORITY, \
			   0,B,C))==SYS_THREAD_ERROR)
#define usr_thread_detach(A)	0
#define usr_keycreate(A,B)	(*(A)=systhread_newkey(),0)
#define usr_setspecific(A,B)	(systhread_setdata(A,B),0)
#define usr_getspecific(A,B)	(*(B)=systhread_getdata(A),0)
#define usr_thread_self()	systhread_current()
#ifdef AFS_USR_SUN5_ENV
#define usr_thread_sleep(A) \
    poll(0, 0, (A)->tv_sec*1000+(A)->tv_nsec/1000000)
#else /* AFS_USR_SUN5_ENV */
#define usr_thread_sleep(A) \
    systhread_sleep((A)->tv_sec*1000+(A)->tv_nsec/1000000)
#endif /* AFS_USR_SUN5_ENV */

#define uprintf			printf

#define usr_getpid()		(int)(usr_thread_self())

#define ISAFS_GLOCK() (usr_thread_self() ==  afs_global_owner)

#else /* NETSCAPE_NSAPI */

/*
 * Mutex and condition variable used to implement sleep
 */
extern pthread_mutex_t usr_sleep_mutex;
extern pthread_cond_t usr_sleep_cond;

#define usr_cond_t		pthread_cond_t
#define usr_mutex_t		pthread_mutex_t
#define usr_thread_t		pthread_t
#define usr_key_t		pthread_key_t

#define usr_mutex_init(A)	assert(pthread_mutex_init(A,NULL) == 0)
#define usr_mutex_destroy(A)	assert(pthread_mutex_destroy(A) == 0)
#define usr_mutex_lock(A)	assert(pthread_mutex_lock(A) == 0)
#define usr_mutex_trylock(A)	((pthread_mutex_trylock(A)==0)?1:0)
#define usr_mutex_unlock(A)	assert(pthread_mutex_unlock(A) == 0)
#define usr_cond_init(A)	assert(pthread_cond_init(A,NULL) == 0)
#define usr_cond_destroy(A)	assert(pthread_cond_destroy(A) == 0)
#define usr_cond_signal(A)	assert(pthread_cond_signal(A) == 0)
#define usr_cond_broadcast(A)	assert(pthread_cond_broadcast(A) == 0)
#define usr_cond_wait(A,B)	pthread_cond_wait(A,B)
#define usr_cond_timedwait(A,B,C)  pthread_cond_timedwait(A,B,C)

#define usr_thread_create(A,B,C) \
    do { \
	pthread_attr_t attr; \
	assert(pthread_attr_init(&attr) == 0); \
	assert(pthread_attr_setstacksize(&attr, 124288) == 0); \
	assert(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 0); \
	assert(pthread_create((A), &attr, (B), (void *)(C)) == 0); \
	assert(pthread_attr_destroy(&attr) == 0); \
    } while(0)
#define usr_thread_detach(A)	pthread_detach(A)
#define usr_keycreate(A,B)	assert(pthread_key_create(A,B) == 0)
#define usr_setspecific(A,B)	pthread_setspecific(A,B)
#define usr_getspecific(A,B)	(*(B)=pthread_getspecific(A),0)
#define usr_thread_self()	pthread_self()
#define usr_thread_sleep(A)						   \
{									   \
    struct timespec _sleep_ts;						   \
    struct timeval _sleep_tv;						   \
    gettimeofday(&_sleep_tv, NULL);					   \
    _sleep_ts = *(A);							   \
    _sleep_ts.tv_sec += _sleep_tv.tv_sec;				   \
    _sleep_ts.tv_nsec += _sleep_tv.tv_usec * 1000;			   \
    if (_sleep_ts.tv_nsec >= 1000000000) {				   \
	_sleep_ts.tv_sec += 1;						   \
	_sleep_ts.tv_nsec -= 1000000000;				   \
    }									   \
    assert(pthread_mutex_lock(&usr_sleep_mutex) == 0);			   \
    pthread_cond_timedwait(&usr_sleep_cond, &usr_sleep_mutex, &_sleep_ts); \
    assert(pthread_mutex_unlock(&usr_sleep_mutex) == 0);		   \
}

#define uprintf			printf

#define usr_getpid()		(int)(usr_thread_self())
#ifdef ISAFS_GLOCK
#undef ISAFS_GLOCK
#endif
#define ISAFS_GLOCK() (usr_thread_self() == afs_global_owner)

#endif /* NETSCAPE_NSAPI */

#define copyin(A,B,C)		(memcpy((void *)B,(void *)A,C), 0)
#define copyout(A,B,C)		(memcpy((void *)B,(void *)A,C), 0)
#define copyinstr(A,B,C,D)	(strncpy(B,A,C),(*D)=strlen(B), 0)
#define copyoutstr(A,B,C,D)	(strncpy(B,A,C),(*D)=strlen(B), 0)

#define vattr_null(A)		usr_vattr_null(A)

#define VN_HOLD(vp) 	\
{ \
    (vp)->v_count++; \
}

#define VN_RELE(vp)     \
do { \
    AFS_ASSERT_GLOCK(); \
    usr_assert((vp)->v_count > 0); \
    if (--((vp)->v_count) == 0) \
	afs_inactive(vp, u.u_cred); \
} while(0)

struct usr_statfs {
    unsigned long f_type;
    unsigned long f_bsize;
    unsigned long f_frsize;
    unsigned long f_ffree;
    unsigned long f_favail;
    struct {
	unsigned long val[2];
    } f_fsid;
    char f_basetype[FSTYPSZ];
    unsigned long f_flag;
    unsigned long f_namemax;
    unsigned long f_blocks;
    unsigned long f_bfree;
    unsigned long f_bavail;
    unsigned long f_files;
};

struct usr_vattr {
    long va_mask;
    usr_vtype_t va_type;
    unsigned short va_mode;
    long va_uid;
    long va_gid;
    unsigned long va_fsid;
    unsigned long va_nodeid;
    unsigned long va_nlink;
    unsigned long va_size;
    struct timeval va_atime;
    struct timeval va_mtime;
    struct timeval va_ctime;
    unsigned long va_rdev;
    unsigned long va_blocksize;
    unsigned long va_blocks;
    unsigned long va_vcode;
};

#ifdef VSUID
#undef VSUID
#endif
#ifdef VSGID
#undef VSGID
#endif
#ifdef VSVTX
#undef VSVTX
#endif
#ifdef VREAD
#undef VREAD
#endif
#ifdef VWRITE
#undef VWRITE
#endif
#ifdef VEXEC
#undef VEXEC
#endif

#define VSUID			04000
#define VSGID			02000
#define VSVTX			01000
#define VREAD			00400
#define VWRITE			00200
#define VEXEC			00100


struct usr_vnode {
    unsigned short v_flag;
    unsigned long v_count;
    struct usr_vnodeops *v_op;
    struct usr_vfs *v_vfsp;
    long v_type;
    unsigned long v_rdev;
    char *v_data;
};

struct usr_inode {
    daddr_t i_db[NDADDR];
    struct usr_vnode *i_devvp;
    unsigned long i_dev;
    long i_flag;
    struct usr_inode *i_freef;
    struct usr_inode **i_freeb;
    long i_gid;
    daddr_t i_ib[NIADDR];
    unsigned short i_mode;
    short i_nlink;
    unsigned long i_number;
    long i_size;
    long i_uid;
    struct usr_vnode i_vnode;
    struct {
	unsigned long ic_spare[4];
    } i_ic;
};
extern struct usr_inode *iget();

struct usr_fileops {
    int (*vno_rw) (void);
    int (*vno_ioctl) (void);
    int (*vno_select) (void);
    int (*vno_closex) (void);
};

struct usr_file {
    unsigned short f_flag;
    offset_t f_offset;
    struct usr_ucred *f_cred;
    struct usr_fileops *f_ops;
    char *f_data;
    long f_type;
};
extern struct usr_file *falloc();
extern struct usr_file *getf(int);

#ifdef	fid_len
#undef	fid_len
#endif
#ifdef	fid_data
#undef	fid_data
#endif

struct usr_fid {
    unsigned short fid_len;
    unsigned short fid_reserved;
    char fid_data[MAXFIDSZ];
};

struct usr_flock {
    short l_type;
    short l_whence;
    off_t l_start;
    off_t l_len;
    long l_sysid;
    pid_t l_pid;
};

extern struct usr_ucred *usr_crget(void);
extern struct usr_ucred *usr_crcopy(struct usr_ucred *);
extern int usr_crhold(struct usr_ucred *);
extern int usr_crfree(struct usr_ucred *);
extern struct usr_ucred *afs_global_ucredp;

#ifdef p_pid
#undef p_pid
#endif

struct usr_proc {
    unsigned long p_flag;
    pid_t p_pid;
    pid_t p_ppid;
    struct usr_ucred *p_ucred;
    char p_cursig;
};

struct usr_a {
    int fd;
    int syscall;
    int parm1;
    int parm2;
    int parm3;
    int parm4;
    int parm5;
    int parm6;
};

#ifdef uio_offset
#undef uio_offset
#endif

struct usr_uio {
    struct iovec *uio_iov;
    int uio_iovcnt;
    long uio_offset;
    int uio_segflg;
    short uio_fmode;
    int uio_resid;
};

#ifdef b_blkno
#undef b_blkno
#endif
#ifdef b_vp
#undef b_vp
#endif

struct usr_buf {
    int b_flags;
    short b_dev;
    unsigned b_bcount;
    struct {
	char *b_addr;
	struct usr_fs *b_fs;
    } b_un;
    long b_blkno;
    unsigned int b_resid;
    struct usr_vnode *b_vp;
};

struct usr_socket {
    int sock;
    short port;
};

#define NDIRSIZ_LEN(len) \
((sizeof (struct usr_dirent)+4 - (MAXNAMLEN+1)) + (((len)+1 + 3) &~ 3))

struct usr_vnodeops {
    int (*vn_open) (char *path, int flags, int mode);
    int (*vn_close) (int fd);
    int (*vn_rdwr) ();
    int (*vn_ioctl) (void);
    int (*vn_select) (void);
    int (*vn_getattr) ();
    int (*vn_setattr) ();
    int (*vn_access) ();
    int (*vn_lookup) ();
    int (*vn_create) ();
    int (*vn_remove) ();
    int (*vn_link) ();
    int (*vn_rename) ();
    int (*vn_mkdir) ();
    int (*vn_rmdir) ();
    int (*vn_readdir) ();
    int (*vn_symlink) ();
    int (*vn_readlink) ();
    int (*vn_fsync) ();
    int (*vn_inactive) ();
    int (*vn_bmap) ();
    int (*vn_strategy) ();
    int (*vn_bread) ();
    int (*vn_brelse) ();
    int (*vn_lockctl) ();
    int (*vn_fid) ();
};

struct usr_fs {
    int dummy;
};

struct usr_mount {
    char m_flags;
    unsigned long m_dev;
    struct usr_inode *m_inodp;
    struct usr_buf *m_bufp;
    struct usr_vnode *m_mount;
};
extern struct usr_mount *getmp(unsigned long);

typedef long usr_whymountroot_t;

struct usr_vfsops {
    int (*vfs_mount) ();
    int (*vfs_unmount) ();
    int (*vfs_root) ();
    int (*vfs_statfs) ();
    int (*vfs_mountroot) ();
    int (*vfs_swapvp) ();
};

struct usr_vfs {
    struct usr_vnode *vfs_vnodecovered;
    struct {
	unsigned long val[2];
    } vfs_fsid;
    char *vfs_data;
    unsigned long vfs_bsize;
    struct usr_mount *vfs_mount;
    struct usr_vfsops *vfs_op;
};

struct usr_sysent {
    char sy_narg;
    int (*sy_call) ();
};
extern struct usr_sysent usr_sysent[];

struct usr_ifnet {
    struct usr_ifnet *if_next;
    short if_flags;
    u_int if_mtu;
    u_int if_metric;
    struct usr_ifaddr *if_addrlist;
};
extern struct usr_ifnet *usr_ifnet;

struct usr_ifaddr {
    struct usr_ifaddr *ifa_next;
    struct usr_ifnet *ifa_ifp;
    struct sockaddr ifa_addr;
};

#ifdef ia_ifp
#undef ia_ifp
#endif
#ifdef ia_addr
#undef ia_addr
#endif

struct usr_in_ifaddr {
    struct usr_in_ifaddr *ia_next;
    struct usr_ifnet *ia_ifp;
    struct sockaddr_in ia_addr;
    unsigned long ia_net;
    unsigned long ia_netmask;
    unsigned long ia_subnet;
    unsigned long ia_subnetmask;
    struct in_addr ia_netbroadcast;
};
extern struct usr_in_ifaddr *usr_in_ifaddr;

extern usr_key_t afs_global_u_key;	/* for per thread authentication */

#if defined(AFS_USR_OSF_ENV)
extern char V;
#else
extern long V;
#endif

#endif /* UKERNEL */

struct min_direct {
#if defined(AFS_OFS_ENV) || defined(AFS_USR_OSF_ENV)
    unsigned int d_fileno;
#else				/* AFS_OFS_ENV || AFS_USR_OSF_ENV */
    unsigned long d_fileno;
#endif				/* AFS_OFS_ENV || AFS_USR_OSF_ENV */
    unsigned short d_reclen;
    unsigned short d_namlen;
};

#ifndef NGROUPS
#define NGROUPS			NGROUPS_MAX
#endif
#ifndef NOGROUP
#define NOGROUP			(-1)
#endif
#ifdef cr_gid
#undef cr_gid
#endif

struct usr_ucred {
    unsigned long cr_ref;
    long cr_uid;
    long cr_gid;
    long cr_ruid;
    long cr_rgid;
    long cr_suid;
    long cr_sgid;
    long cr_ngroups;
    gid_t cr_groups[NGROUPS];
};

#ifdef u_rval1
#undef u_rval1
#endif
#ifdef r_val1
#undef r_val1
#endif

struct usr_user {
    int u_error;
    int u_prio;
    char *u_ap;
    int u_rval1;
    long u_viceid;
    unsigned long u_expiration;
    struct usr_proc *u_procp;
    struct usr_ucred *u_cred;
    struct {
	int r_val1;
    } u_r;
};
#define u_rval1			u_r.r_val1
#define u			(*(get_user_struct()))

extern struct usr_user *get_user_struct(void);

#define USR_DIRSIZE		2048

struct usr_dirent {
    unsigned long d_ino;
    unsigned long d_off;
    unsigned short d_reclen;
    char d_name[MAXNAMLEN + 1];
};

typedef struct {
    int dd_fd;
    int dd_loc;
    int dd_size;
    int dd_reserved;
    char *dd_buf;
} usr_DIR;

extern unsigned short usr_rx_port;

#define AFS_LOOKUP_NOEVAL 1

#endif /* __AFS_SYSINCLUDESH__  so idempotent */
