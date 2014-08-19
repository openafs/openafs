
/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * User space client specific interface glue
 */

#include <afsconfig.h>
#include "afs/param.h"

#ifdef	UKERNEL

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include <net/if.h>

#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs_usrops.h"
#include "afs/afs_stats.h"
#include "afs/auth.h"
#include "afs/cellconfig.h"
#include "afs/vice.h"
#include "afs/kauth.h"
#include "afs/kautils.h"
#include "afs/afsutil.h"
#include "afs/afs_bypasscache.h"
#include "rx/rx_globals.h"
#include "afsd/afsd.h"

#define VFS 1
#undef	VIRTUE
#undef	VICE

#ifndef AFS_CACHE_VNODE_PATH
#error You must compile UKERNEL code with -DAFS_CACHE_VNODE_PATH
#endif

#define CACHEINFOFILE	"cacheinfo"
#define	AFSLOGFILE	"AFSLog"
#define	DCACHEFILE	"CacheItems"
#define	VOLINFOFILE	"VolumeItems"
#define	CELLINFOFILE	"CellItems"
#define MAXIPADDRS 64

#ifndef MIN
#define MIN(A,B)	((A)<(B)?(A):(B))
#endif
#ifndef MAX
#define MAX(A,B)	((A)>(B)?(A):(B))
#endif

extern int cacheDiskType;

char afs_LclCellName[64];

static struct usr_vnode *afs_FileTable[MAX_OSI_FILES];
static int afs_FileFlags[MAX_OSI_FILES];
static off_t afs_FileOffsets[MAX_OSI_FILES];

#define MAX_CACHE_LOOPS 4

static struct usr_vfs afs_RootVfs;
static struct usr_vnode *afs_RootVnode = NULL;
static struct usr_vnode *afs_CurrentDir = NULL;

static char afs_mountDir[1024];	/* AFS mount point */
static int afs_mountDirLen;		/* strlen of AFS mount point */

struct afsconf_dir *afs_cdir;	/* config dir */

int afs_bufferpages = 100;

static usr_key_t afs_global_u_key;

static struct usr_proc *afs_global_procp = NULL;
static struct usr_ucred *afs_global_ucredp = NULL;

struct usr_ucred afs_osi_cred, *afs_osi_credp;
usr_mutex_t afs_global_lock;
usr_thread_t afs_global_owner;
usr_mutex_t rx_global_lock;
usr_thread_t rx_global_owner;

static usr_mutex_t osi_dummy_lock;
static usr_mutex_t osi_waitq_lock;
static usr_mutex_t osi_authenticate_lock;
afs_lock_t afs_ftf;
afs_lock_t osi_flplock;
afs_lock_t osi_fsplock;

#ifndef NETSCAPE_NSAPI

/*
 * Mutex and condition variable used to implement sleep
 */
pthread_mutex_t usr_sleep_mutex;
pthread_cond_t usr_sleep_cond;

#endif /* !NETSCAPE_NSAPI */

int call_syscall(long, long, long, long, long, long);
int fork_syscall(long, long, long, long, long, long);


/*
 * Hash table mapping addresses onto wait structures for
 * osi_Sleep/osi_Wakeup and osi_Wait/osi_Wakeup
 */
typedef struct osi_wait {
    caddr_t addr;
    usr_cond_t cond;
    int flag;
    struct osi_wait *next;
    struct osi_wait *prev;
    time_t expiration;
    struct osi_wait *timedNext;
    struct osi_wait *timedPrev;
} osi_wait_t;

/*
 * Head of the linked list of available waitq structures.
 */
static osi_wait_t *osi_waithash_avail;

/*
 * List of timed waits, NSAPI does not provide a cond_timed
 * wait, so we need to keep track of the timed waits ourselves and
 * periodically check for expirations
 */
static osi_wait_t *osi_timedwait_head;
static osi_wait_t *osi_timedwait_tail;

static struct {
    osi_wait_t *head;
    osi_wait_t *tail;
} osi_waithash_table[OSI_WAITHASH_SIZE];

/*
 * Never call afs_brelse
 */
int
ufs_brelse(struct usr_vnode *vp, struct usr_buf *bp)
{
    usr_assert(0);
    return 0;
}

/*
 * I am not sure what to do with these, they assert for now
 */
int
iodone(struct usr_buf *bp)
{
    usr_assert(0);
    return 0;
}

struct usr_file *
getf(int fd)
{
    usr_assert(0);
    return 0;
}

/*
 * Every user is a super user
 */
int
afs_osi_suser(void *credp)
{
    return 1;
}

int
afs_suser(void *credp)
{
    return 1;
}

/*
 * These are no-ops in user space
 */

void
afs_osi_SetTime(osi_timeval_t * atv)
{
    return;
}

/*
 * xflock should never fall through, the only files we know
 * about are AFS files
 */
int
usr_flock(void)
{
    usr_assert(0);
    return 0;
}

/*
 * ioctl should never fall through, the only files we know
 * about are AFS files
 */
int
usr_ioctl(void)
{
    usr_assert(0);
    return 0;
}

/*
 * We do not support the inode related system calls
 */
int
afs_syscall_icreate(long a, long b, long c, long d, long e, long f)
{
    usr_assert(0);
    return 0;
}

int
afs_syscall_iincdec(int dev, int inode, int inode_p1, int amount)
{
    usr_assert(0);
    return 0;
}

int
afs_syscall_iopen(int dev, int inode, int usrmod)
{
    usr_assert(0);
    return 0;
}

int
afs_syscall_ireadwrite(void)
{
    usr_assert(0);
    return 0;
}

/*
 * these routines are referenced in the vfsops structure, but
 * should never get called
 */
int
vno_close(void)
{
    usr_assert(0);
    return 0;
}

int
vno_ioctl(void)
{
    usr_assert(0);
    return 0;
}

int
vno_rw(void)
{
    usr_assert(0);
    return 0;
}

int
vno_select(void)
{
    usr_assert(0);
    return 0;
}

/*
 * uiomove copies data between kernel buffers and uio buffers
 */
int
usr_uiomove(char *kbuf, int n, int rw, struct usr_uio *uio)
{
    int nio;
    int len;
    char *ptr;
    struct iovec *iovp;

    nio = uio->uio_iovcnt;
    iovp = uio->uio_iov;

    if (nio <= 0)
	return EFAULT;

    /*
     * copy the data
     */
    ptr = kbuf;
    while (nio > 0 && n > 0) {
	len = MIN(n, iovp->iov_len);
	if (rw == UIO_READ) {
	    memcpy(iovp->iov_base, ptr, len);
	} else {
	    memcpy(ptr, iovp->iov_base, len);
	}
	n -= len;
	ptr += len;
	uio->uio_resid -= len;
	uio->uio_offset += len;
	iovp->iov_base = (char *)(iovp->iov_base) + len;
	iovp->iov_len -= len;
	iovp++;
	nio--;
    }

    if (n > 0)
	return EFAULT;
    return 0;
}

/*
 * routines to manage user credentials
 */
struct usr_ucred *
usr_crcopy(struct usr_ucred *credp)
{
    struct usr_ucred *newcredp;

    newcredp = (struct usr_ucred *)afs_osi_Alloc(sizeof(struct usr_ucred));
    *newcredp = *credp;
    newcredp->cr_ref = 1;
    return newcredp;
}

struct usr_ucred *
usr_crget(void)
{
    struct usr_ucred *newcredp;

    newcredp = (struct usr_ucred *)afs_osi_Alloc(sizeof(struct usr_ucred));
    newcredp->cr_ref = 1;
    return newcredp;
}

int
usr_crfree(struct usr_ucred *credp)
{
    credp->cr_ref--;
    if (credp->cr_ref == 0) {
	afs_osi_Free((char *)credp, sizeof(struct usr_ucred));
    }
    return 0;
}

int
usr_crhold(struct usr_ucred *credp)
{
    credp->cr_ref++;
    return 0;
}

void
usr_vattr_null(struct usr_vattr *vap)
{
    int n;
    char *cp;

    n = sizeof(struct usr_vattr);
    cp = (char *)vap;
    while (n--) {
	*cp++ = -1;
    }
}

/*
 * Initialize the thread specific data used to simulate the
 * kernel environment for each thread. The user structure
 * is stored in the thread specific data.
 */
void
uafs_InitThread(void)
{
    int st;
    struct usr_user *uptr;

    /*
     * initialize the thread specific user structure. Use malloc to
     * allocate the data block, so pthread_finish can free the buffer
     * when this thread terminates.
     */
    uptr =
	(struct usr_user *)malloc(sizeof(struct usr_user) +
				  sizeof(struct usr_ucred));
    usr_assert(uptr != NULL);
    uptr->u_error = 0;
    uptr->u_prio = 0;
    uptr->u_procp = afs_global_procp;
    uptr->u_cred = (struct usr_ucred *)(uptr + 1);
    *uptr->u_cred = *afs_global_ucredp;
    st = usr_setspecific(afs_global_u_key, (void *)uptr);
    usr_assert(st == 0);
}

/*
 * routine to get the user structure from the thread specific data.
 * this routine is used to implement the global 'u' structure. Initializes
 * the thread if needed.
 */
struct usr_user *
get_user_struct(void)
{
    struct usr_user *uptr;
    int st;
    st = usr_getspecific(afs_global_u_key, (void **)&uptr);
    usr_assert(st == 0);
    if (uptr == NULL) {
	uafs_InitThread();
	st = usr_getspecific(afs_global_u_key, (void **)&uptr);
	usr_assert(st == 0);
	usr_assert(uptr != NULL);
    }
    return uptr;
}

/*
 * Hash an address for the waithash table
 */
#define WAITHASH(X)	\
	(((long)(X)^((long)(X)>>4)^((long)(X)<<4))&(OSI_WAITHASH_SIZE-1))

/*
 * Sleep on an event
 */
void
afs_osi_Sleep(void *x)
{
    int index;
    osi_wait_t *waitp;
    int glockOwner = ISAFS_GLOCK();

    usr_mutex_lock(&osi_waitq_lock);
    if (glockOwner) {
	AFS_GUNLOCK();
    }
    index = WAITHASH(x);
    if (osi_waithash_avail == NULL) {
	waitp = (osi_wait_t *) afs_osi_Alloc(sizeof(osi_wait_t));
	usr_cond_init(&waitp->cond);
    } else {
	waitp = osi_waithash_avail;
	osi_waithash_avail = osi_waithash_avail->next;
    }
    waitp->addr = x;
    waitp->flag = 0;
    DLL_INSERT_TAIL(waitp, osi_waithash_table[index].head,
		    osi_waithash_table[index].tail, next, prev);
    waitp->expiration = 0;
    waitp->timedNext = NULL;
    waitp->timedPrev = NULL;
    while (waitp->flag == 0) {
	usr_cond_wait(&waitp->cond, &osi_waitq_lock);
    }
    DLL_DELETE(waitp, osi_waithash_table[index].head,
	       osi_waithash_table[index].tail, next, prev);
    waitp->next = osi_waithash_avail;
    osi_waithash_avail = waitp;
    usr_mutex_unlock(&osi_waitq_lock);
    if (glockOwner) {
	AFS_GLOCK();
    }
}

int
afs_osi_SleepSig(void *x)
{
    afs_osi_Sleep(x);
    return 0;
}

int
afs_osi_Wakeup(void *x)
{
    int index;
    osi_wait_t *waitp;

    index = WAITHASH(x);
    usr_mutex_lock(&osi_waitq_lock);
    waitp = osi_waithash_table[index].head;
    while (waitp) {
	if (waitp->addr == x && waitp->flag == 0) {
	    waitp->flag = 1;
	    usr_cond_signal(&waitp->cond);
	}
	waitp = waitp->next;
    }
    usr_mutex_unlock(&osi_waitq_lock);
    return 0;
}

int
afs_osi_TimedSleep(void *event, afs_int32 ams, int aintok)
{
    return afs_osi_Wait(ams, event, aintok);
}

int
afs_osi_Wait(afs_int32 msec, struct afs_osi_WaitHandle *handle, int intok)
{
    int index;
    osi_wait_t *waitp;
    struct timespec tv;
    int ret;
    int glockOwner = ISAFS_GLOCK();

    tv.tv_sec = msec / 1000;
    tv.tv_nsec = (msec % 1000) * 1000000;
    if (handle == NULL) {
	if (glockOwner) {
	    AFS_GUNLOCK();
	}
	usr_thread_sleep(&tv);
	ret = 0;
	if (glockOwner) {
	    AFS_GLOCK();
	}
    } else {
	usr_mutex_lock(&osi_waitq_lock);
	if (glockOwner) {
	    AFS_GUNLOCK();
	}
	index = WAITHASH((caddr_t) handle);
	if (osi_waithash_avail == NULL) {
	    waitp = (osi_wait_t *) afs_osi_Alloc(sizeof(osi_wait_t));
	    usr_cond_init(&waitp->cond);
	} else {
	    waitp = osi_waithash_avail;
	    osi_waithash_avail = osi_waithash_avail->next;
	}
	waitp->addr = (caddr_t) handle;
	waitp->flag = 0;
	DLL_INSERT_TAIL(waitp, osi_waithash_table[index].head,
			osi_waithash_table[index].tail, next, prev);
	tv.tv_sec += time(NULL);
	waitp->expiration = tv.tv_sec + ((tv.tv_nsec == 0) ? 0 : 1);
	DLL_INSERT_TAIL(waitp, osi_timedwait_head, osi_timedwait_tail,
			timedNext, timedPrev);
	usr_cond_wait(&waitp->cond, &osi_waitq_lock);
	if (waitp->flag) {
	    ret = 2;
	} else {
	    ret = 0;
	}
	DLL_DELETE(waitp, osi_waithash_table[index].head,
		   osi_waithash_table[index].tail, next, prev);
	DLL_DELETE(waitp, osi_timedwait_head, osi_timedwait_tail, timedNext,
		   timedPrev);
	waitp->next = osi_waithash_avail;
	osi_waithash_avail = waitp;
	usr_mutex_unlock(&osi_waitq_lock);
	if (glockOwner) {
	    AFS_GLOCK();
	}
    }
    return ret;
}

void
afs_osi_CancelWait(struct afs_osi_WaitHandle *handle)
{
    afs_osi_Wakeup(handle);
}

/*
 * Netscape NSAPI doesn't have a cond_timed_wait, so we need
 * to explicitly signal cond_timed_waits when their timers expire
 */
int
afs_osi_CheckTimedWaits(void)
{
    time_t curTime;
    osi_wait_t *waitp;

    curTime = time(NULL);
    usr_mutex_lock(&osi_waitq_lock);
    waitp = osi_timedwait_head;
    while (waitp != NULL) {
	usr_assert(waitp->expiration != 0);
	if (waitp->expiration <= curTime) {
	    waitp->flag = 1;
	    usr_cond_signal(&waitp->cond);
	}
	waitp = waitp->timedNext;
    }
    usr_mutex_unlock(&osi_waitq_lock);
    return 0;
}

/*
 * 'dummy' vnode, for non-AFS files. We don't actually need most vnode
 * information for non-AFS files, so point all of them towards this vnode
 * to save memory.
 */
static struct usr_vnode dummy_vnode = {
    0,    /* v_flag */
    1024, /* v_count */
    NULL, /* v_op */
    NULL, /* v_vfsp */
    0,    /* v_type */
    0,    /* v_rdev */
    NULL  /* v_data */
};

/*
 * Allocate a slot in the file table if there is not one there already,
 * copy in the file name and kludge up the vnode and inode structures
 */
int
lookupname(char *fnamep, int segflg, int followlink,
	   struct usr_vnode **compvpp)
{
    int code;

    /*
     * Assume relative pathnames refer to files in AFS
     */
    if (*fnamep != '/' || uafs_afsPathName(fnamep) != NULL) {
	AFS_GLOCK();
	code = uafs_LookupName(fnamep, afs_CurrentDir, compvpp, 0, 0);
	AFS_GUNLOCK();
	return code;
    }

    /* For non-afs files, nobody really looks at the meaningful values in the
     * returned vnode, so we can return a 'fake' one. The vnode can be held,
     * released, etc. and some callers check for a NULL vnode anyway, so we
     * to return something. */

    usr_mutex_lock(&osi_dummy_lock);
    VN_HOLD(&dummy_vnode);
    usr_mutex_unlock(&osi_dummy_lock);

    *compvpp = &dummy_vnode;

    return 0;
}

/*
 * open a file given its i-node number
 */
void *
osi_UFSOpen(afs_dcache_id_t *ino)
{
    int rc;
    struct osi_file *fp;
    struct stat st;

    AFS_ASSERT_GLOCK();

    AFS_GUNLOCK();
    fp = (struct osi_file *)afs_osi_Alloc(sizeof(struct osi_file));
    usr_assert(fp != NULL);

    usr_assert(ino->ufs);

    fp->fd = open(ino->ufs, O_RDWR | O_CREAT, 0);
    if (fp->fd < 0) {
	get_user_struct()->u_error = errno;
	afs_osi_Free((char *)fp, sizeof(struct osi_file));
	AFS_GLOCK();
	return NULL;
    }
    rc = fstat(fp->fd, &st);
    if (rc < 0) {
	get_user_struct()->u_error = errno;
	afs_osi_Free((void *)fp, sizeof(struct osi_file));
	AFS_GLOCK();
	return NULL;
    }
    fp->size = st.st_size;
    fp->offset = 0;
    fp->vnode = (struct usr_vnode *)fp;

    AFS_GLOCK();
    return fp;
}

int
osi_UFSClose(struct osi_file *fp)
{
    int rc;

    AFS_ASSERT_GLOCK();

    AFS_GUNLOCK();
    rc = close(fp->fd);
    if (rc < 0) {
	get_user_struct()->u_error = errno;
	afs_osi_Free((void *)fp, sizeof(struct osi_file));
	AFS_GLOCK();
	return -1;
    }
    afs_osi_Free((void *)fp, sizeof(struct osi_file));
    AFS_GLOCK();
    return 0;
}

int
osi_UFSTruncate(struct osi_file *fp, afs_int32 len)
{
    int rc;

    AFS_ASSERT_GLOCK();

    AFS_GUNLOCK();
    rc = ftruncate(fp->fd, len);
    if (rc < 0) {
	get_user_struct()->u_error = errno;
	AFS_GLOCK();
	return -1;
    }
    fp->size = len;
    AFS_GLOCK();
    return 0;
}

int
afs_osi_Read(struct osi_file *fp, int offset, void *buf, afs_int32 len)
{
    int rc, ret;
    struct stat st;

    AFS_ASSERT_GLOCK();

    AFS_GUNLOCK();
    if (offset >= 0) {
	rc = lseek(fp->fd, offset, SEEK_SET);
    } else {
	rc = lseek(fp->fd, fp->offset, SEEK_SET);
    }
    if (rc < 0) {
	get_user_struct()->u_error = errno;
	AFS_GLOCK();
	return -1;
    }
    fp->offset = rc;
    ret = read(fp->fd, buf, len);
    if (ret < 0) {
	get_user_struct()->u_error = errno;
	AFS_GLOCK();
	return -1;
    }
    fp->offset += ret;
    rc = fstat(fp->fd, &st);
    if (rc < 0) {
	get_user_struct()->u_error = errno;
	AFS_GLOCK();
	return -1;
    }
    fp->size = st.st_size;
    AFS_GLOCK();
    return ret;
}

int
afs_osi_Write(struct osi_file *fp, afs_int32 offset, void *buf, afs_int32 len)
{
    int rc, ret;
    struct stat st;

    AFS_ASSERT_GLOCK();

    AFS_GUNLOCK();
    if (offset >= 0) {
	rc = lseek(fp->fd, offset, SEEK_SET);
    } else {
	rc = lseek(fp->fd, fp->offset, SEEK_SET);
    }
    if (rc < 0) {
	get_user_struct()->u_error = errno;
	AFS_GLOCK();
	return -1;
    }
    fp->offset = rc;
    ret = write(fp->fd, buf, len);
    if (ret < 0) {
	get_user_struct()->u_error = errno;
	AFS_GLOCK();
	return -1;
    }
    fp->offset += ret;
    rc = fstat(fp->fd, &st);
    if (rc < 0) {
	get_user_struct()->u_error = errno;
	AFS_GLOCK();
	return -1;
    }
    fp->size = st.st_size;
    AFS_GLOCK();
    return ret;
}

int
afs_osi_Stat(struct osi_file *fp, struct osi_stat *stp)
{
    int rc;
    struct stat st;

    AFS_GUNLOCK();
    rc = fstat(fp->fd, &st);
    if (rc < 0) {
	get_user_struct()->u_error = errno;
	AFS_GLOCK();
	return -1;
    }
    stp->size = st.st_size;
    stp->mtime = st.st_mtime;
    stp->atime = st.st_atime;
    AFS_GLOCK();
    return 0;
}

/*
 * VOP_RDWR routine
 */
int
afs_osi_VOP_RDWR(struct usr_vnode *vnodeP, struct usr_uio *uioP, int rw,
		 int flags, struct usr_ucred *credP)
{
    int rc;
    struct osi_file *fp = (struct osi_file *)vnodeP;

    /*
     * We don't support readv/writev.
     */
    usr_assert(uioP->uio_iovcnt == 1);
    usr_assert(uioP->uio_resid == uioP->uio_iov[0].iov_len);

    if (rw == UIO_WRITE) {
	usr_assert(uioP->uio_fmode == FWRITE);
	rc = afs_osi_Write(fp, uioP->uio_offset, uioP->uio_iov[0].iov_base,
			   uioP->uio_iov[0].iov_len);
    } else {
	usr_assert(uioP->uio_fmode == FREAD);
	rc = afs_osi_Read(fp, uioP->uio_offset, uioP->uio_iov[0].iov_base,
			  uioP->uio_iov[0].iov_len);
    }
    if (rc < 0) {
	return get_user_struct()->u_error;
    }

    uioP->uio_resid -= rc;
    uioP->uio_offset += rc;
    uioP->uio_iov[0].iov_base = (char *)(uioP->uio_iov[0].iov_base) + rc;
    uioP->uio_iov[0].iov_len -= rc;
    return 0;
}

void *
afs_osi_Alloc(size_t size)
{
    return malloc(size);
}

void
afs_osi_Free(void *ptr, size_t size)
{
    free(ptr);
}

void
afs_osi_FreeStr(char *ptr)
{
    free(ptr);
}

void *
osi_AllocLargeSpace(size_t size)
{
    AFS_STATCNT(osi_AllocLargeSpace);
    return afs_osi_Alloc(size);
}

void
osi_FreeLargeSpace(void *ptr)
{
    AFS_STATCNT(osi_FreeLargeSpace);
    afs_osi_Free(ptr, 0);
}

void *
osi_AllocSmallSpace(size_t size)
{
    AFS_STATCNT(osi_AllocSmallSpace);
    return afs_osi_Alloc(size);
}

void
osi_FreeSmallSpace(void *ptr)
{
    AFS_STATCNT(osi_FreeSmallSpace);
    afs_osi_Free(ptr, 0);
}

void
shutdown_osi(void)
{
    AFS_STATCNT(shutdown_osi);
    return;
}

void
shutdown_osinet(void)
{
    AFS_STATCNT(shutdown_osinet);
    return;
}

void
shutdown_osifile(void)
{
    AFS_STATCNT(shutdown_osifile);
    return;
}

void
afs_nfsclient_init(void)
{
}

void
shutdown_nfsclnt(void)
{
    return;
}

void
afs_osi_Invisible(void)
{
    return;
}

void
afs_osi_Visible(void)
{
    return;
}

int
osi_GetTime(struct timeval *tv)
{
    gettimeofday(tv, NULL);
    return 0;
}

int
osi_SetTime(struct timeval *tv)
{
    return 0;
}

int
osi_Active(struct vcache *avc)
{
    AFS_STATCNT(osi_Active);
    if (avc->opens > 0)
	return (1);
    return 0;
}

int
afs_osi_MapStrategy(int (*aproc) (struct usr_buf *), struct usr_buf *bp)
{
    afs_int32 returnCode;
    returnCode = (*aproc) (bp);
    return returnCode;
}

void
osi_FlushPages(struct vcache *avc, afs_ucred_t *credp)
{
    ObtainSharedLock(&avc->lock, 555);
    if ((hcmp((avc->f.m.DataVersion), (avc->mapDV)) <= 0)
	|| ((avc->execsOrWriters > 0) && afs_DirtyPages(avc))) {
	ReleaseSharedLock(&avc->lock);
	return;
    }
    UpgradeSToWLock(&avc->lock, 565);
    hset(avc->mapDV, avc->f.m.DataVersion);
    ReleaseWriteLock(&avc->lock);
    return;
}

void
osi_FlushText_really(struct vcache *vp)
{
    if (hcmp(vp->f.m.DataVersion, vp->flushDV) > 0) {
	hset(vp->flushDV, vp->f.m.DataVersion);
    }
    return;
}

int
osi_SyncVM(struct vcache *avc)
{
    return 0;
}

void
osi_ReleaseVM(struct vcache *avc, int len, struct usr_ucred *credp)
{
    return;
}

void
osi_Init(void)
{
    int i;
    int st;

    /*
     * Use the thread specific data to implement the user structure
     */
    usr_keycreate(&afs_global_u_key, free);

    /*
     * Initialize the global ucred structure
     */
    afs_global_ucredp = (struct usr_ucred *)
	afs_osi_Alloc(sizeof(struct usr_ucred));
    usr_assert(afs_global_ucredp != NULL);
    afs_global_ucredp->cr_ref = 1;
    afs_set_cr_uid(afs_global_ucredp, geteuid());
    afs_set_cr_gid(afs_global_ucredp, getegid());
    afs_set_cr_ruid(afs_global_ucredp, getuid());
    afs_set_cr_rgid(afs_global_ucredp, getgid());
    afs_global_ucredp->cr_suid = afs_cr_ruid(afs_global_ucredp);
    afs_global_ucredp->cr_sgid = afs_cr_rgid(afs_global_ucredp);
    st = getgroups(NGROUPS, &afs_global_ucredp->cr_groups[0]);
    usr_assert(st >= 0);
    afs_global_ucredp->cr_ngroups = (unsigned long)st;
    for (i = st; i < NGROUPS; i++) {
	afs_global_ucredp->cr_groups[i] = NOGROUP;
    }

    /*
     * Initialize the global process structure
     */
    afs_global_procp = (struct usr_proc *)
	afs_osi_Alloc(sizeof(struct usr_proc));
    usr_assert(afs_global_procp != NULL);
    afs_global_procp->p_pid = osi_getpid();
    afs_global_procp->p_ppid = (pid_t) 1;
    afs_global_procp->p_ucred = afs_global_ucredp;

#ifndef NETSCAPE_NSAPI
    /*
     * Initialize the mutex and condition variable used to implement
     * time sleeps.
     */
    pthread_mutex_init(&usr_sleep_mutex, NULL);
    pthread_cond_init(&usr_sleep_cond, NULL);
#endif /* !NETSCAPE_NSAPI */

    /*
     * Initialize the hash table used for sleep/wakeup
     */
    for (i = 0; i < OSI_WAITHASH_SIZE; i++) {
	DLL_INIT_LIST(osi_waithash_table[i].head, osi_waithash_table[i].tail);
    }
    DLL_INIT_LIST(osi_timedwait_head, osi_timedwait_tail);
    osi_waithash_avail = NULL;

    /*
     * Initialize the AFS file table
     */
    for (i = 0; i < MAX_OSI_FILES; i++) {
	afs_FileTable[i] = NULL;
    }

    /*
     * Initialize the global locks
     */
    usr_mutex_init(&afs_global_lock);
    usr_mutex_init(&rx_global_lock);
    usr_mutex_init(&osi_dummy_lock);
    usr_mutex_init(&osi_waitq_lock);
    usr_mutex_init(&osi_authenticate_lock);

    /*
     * Initialize the AFS OSI credentials
     */
    afs_osi_cred = *afs_global_ucredp;
    afs_osi_credp = &afs_osi_cred;

    init_et_to_sys_error();
}

/*
 * Set the UDP port number RX uses for UDP datagrams
 */
void
uafs_SetRxPort(int port)
{
    usr_assert(usr_rx_port == 0);
    usr_rx_port = port;
}

/*
 * uafs_Init is for backwards compatibility only! Do not use it; use
 * uafs_Setup, uafs_ParseArgs, and uafs_Run instead.
 */
void
uafs_Init(char *rn, char *mountDirParam, char *confDirParam,
          char *cacheBaseDirParam, int cacheBlocksParam, int cacheFilesParam,
          int cacheStatEntriesParam, int dCacheSizeParam, int vCacheSizeParam,
          int chunkSizeParam, int closeSynchParam, int debugParam,
          int nDaemonsParam, int cacheFlagsParam, char *logFile)
{
    int code;
    int argc = 0;
    char *argv[32];
    int freeargc = 0;
    void *freeargv[32];
    char buf[1024];
    int i;

    code = uafs_Setup(mountDirParam);
    usr_assert(code == 0);

    argv[argc++] = rn;
    if (mountDirParam) {
	argv[argc++] = "-mountdir";
	argv[argc++] = mountDirParam;
    }
    if (confDirParam) {
	argv[argc++] = "-confdir";
	argv[argc++] = confDirParam;
    }
    if (cacheBaseDirParam) {
	argv[argc++] = "-cachedir";
	argv[argc++] = cacheBaseDirParam;
    }
    if (cacheBlocksParam) {
	snprintf(buf, sizeof(buf), "%d", cacheBlocksParam);

	argv[argc++] = "-blocks";
	argv[argc++] = freeargv[freeargc++] = strdup(buf);
    }
    if (cacheFilesParam) {
	snprintf(buf, sizeof(buf), "%d", cacheFilesParam);

	argv[argc++] = "-files";
	argv[argc++] = freeargv[freeargc++] = strdup(buf);
    }
    if (cacheStatEntriesParam) {
	snprintf(buf, sizeof(buf), "%d", cacheStatEntriesParam);

	argv[argc++] = "-stat";
	argv[argc++] = freeargv[freeargc++] = strdup(buf);
    }
    if (dCacheSizeParam) {
	snprintf(buf, sizeof(buf), "%d", dCacheSizeParam);

	argv[argc++] = "-dcache";
	argv[argc++] = freeargv[freeargc++] = strdup(buf);
    }
    if (vCacheSizeParam) {
	snprintf(buf, sizeof(buf), "%d", vCacheSizeParam);

	argv[argc++] = "-volumes";
	argv[argc++] = freeargv[freeargc++] = strdup(buf);
    }
    if (chunkSizeParam) {
	snprintf(buf, sizeof(buf), "%d", chunkSizeParam);

	argv[argc++] = "-chunksize";
	argv[argc++] = freeargv[freeargc++] = strdup(buf);
    }
    if (closeSynchParam) {
	argv[argc++] = "-waitclose";
    }
    if (debugParam) {
	argv[argc++] = "-debug";
    }
    if (nDaemonsParam) {
	snprintf(buf, sizeof(buf), "%d", nDaemonsParam);

	argv[argc++] = "-daemons";
	argv[argc++] = freeargv[freeargc++] = strdup(buf);
    }
    if (cacheFlagsParam) {
	if (cacheFlagsParam & AFSCALL_INIT_MEMCACHE) {
	    argv[argc++] = "-memcache";
	}
    }
    if (logFile) {
	argv[argc++] = "-logfile";
	argv[argc++] = logFile;
    }

    argv[argc] = NULL;

    code = uafs_ParseArgs(argc, argv);
    usr_assert(code == 0);

    for (i = 0; i < freeargc; i++) {
	free(freeargv[i]);
    }

    code = uafs_Run();
    usr_assert(code == 0);
}

/**
 * Calculate the cacheMountDir used for a specified dir.
 *
 * @param[in]  dir      Desired mount dir
 * @param[out] mountdir On success, contains the literal string that should
 *                      be used as the cache mount dir.
 * @param[in]  size     The number of bytes allocated in mountdir
 *
 * @post On success, mountdir begins with a slash, and does not contain two
 * slashes adjacent to each other
 *
 * @return operation status
 *  @retval 0 success
 *  @retval ENAMETOOLONG the specified dir is too long to fix in the given
 *                       mountdir buffer
 *  @retval EINVAL the specified dir does not actually specify any meaningful
 *                 mount directory
 */
static int
calcMountDir(const char *dir, char *mountdir, size_t size)
{
    char buf[1024];
    char lastchar;
    char *p;
    int len;

    if (dir && strlen(dir) > size-1) {
	return ENAMETOOLONG;
    }

    /*
     * Initialize the AFS mount point, default is '/afs'.
     * Strip duplicate/trailing slashes from mount point string.
     * afs_mountDirLen is set to strlen(afs_mountDir).
     */
    if (!dir) {
	dir = "afs";
    }
    sprintf(buf, "%s", dir);

    mountdir[0] = '/';
    len = 1;
    for (lastchar = '/', p = &buf[0]; *p != '\0'; p++) {
        if (lastchar != '/' || *p != '/') {
            mountdir[len++] = lastchar = *p;
        }
    }
    if (lastchar == '/' && len > 1)
        len--;
    mountdir[len] = '\0';
    if (len <= 1) {
	return EINVAL;
    }

    return 0;
}

void
uafs_mount(void) {
    int rc;

    /*
     * Mount the AFS filesystem
     */
    AFS_GLOCK();
    rc = afs_mount(&afs_RootVfs, NULL, NULL);
    usr_assert(rc == 0);
    rc = afs_root(&afs_RootVfs, &afs_RootVnode);
    usr_assert(rc == 0);
    AFS_GUNLOCK();

    /*
     * initialize the current directory to the AFS root
     */
    afs_CurrentDir = afs_RootVnode;
    VN_HOLD(afs_CurrentDir);

    return;
}

void
uafs_setMountDir(const char *dir)
{
    if (dir) {
	int rc;
	char tmp_mountDir[1024];

	rc = calcMountDir(dir, tmp_mountDir, sizeof(tmp_mountDir));
	if (rc) {
	    afs_warn("Invalid mount dir specification (error %d): %s\n", rc, dir);
	} else {
	    if (strcmp(tmp_mountDir, afs_mountDir) != 0) {
		/* mount dir changed */
		strcpy(afs_mountDir, tmp_mountDir);
		afs_mountDirLen = strlen(afs_mountDir);
	    }
	}
    }
}

int
uafs_statvfs(struct statvfs *buf)
{
    int rc;

    AFS_GLOCK();

    rc = afs_statvfs(&afs_RootVfs, buf);

    AFS_GUNLOCK();

    if (rc) {
	errno = rc;
	return -1;
    }

    return 0;
}

void
uafs_Shutdown(void)
{
    int rc;

    printf("\n");

    AFS_GLOCK();
    if (afs_CurrentDir) {
	VN_RELE(afs_CurrentDir);
    }
    rc = afs_unmount(&afs_RootVfs);
    usr_assert(rc == 0);
    AFS_GUNLOCK();

    printf("\n");
}

/*
 * Donate the current thread to the RX server pool.
 */
void
uafs_RxServerProc(void)
{
    osi_socket sock;
    int threadID;
    struct rx_call *newcall = NULL;

    rxi_MorePackets(2);		/* alloc more packets */
    threadID = rxi_availProcs++;

    while (1) {
	sock = OSI_NULLSOCKET;
	rxi_ServerProc(threadID, newcall, &sock);
	if (sock == OSI_NULLSOCKET) {
	    break;
	}
	newcall = NULL;
	threadID = -1;
	rxi_ListenerProc(sock, &threadID, &newcall);
	/* assert(threadID != -1); */
	/* assert(newcall != NULL); */
    }
}

struct syscallThreadArgs {
    long syscall;
    long afscall;
    long param1;
    long param2;
    long param3;
    long param4;
};

#ifdef NETSCAPE_NSAPI
void
syscallThread(void *argp)
#else /* NETSCAPE_NSAPI */
void *
syscallThread(void *argp)
#endif				/* NETSCAPE_NSAPI */
{
    int i;
    struct usr_ucred *crp;
    struct syscallThreadArgs *sysArgsP = (struct syscallThreadArgs *)argp;

    /*
     * AFS daemons run authenticated
     */
    get_user_struct()->u_viceid = getuid();
    crp = get_user_struct()->u_cred;
    afs_set_cr_uid(crp, getuid());
    afs_set_cr_ruid(crp, getuid());
    crp->cr_suid = getuid();
    crp->cr_groups[0] = getgid();
    crp->cr_ngroups = 1;
    for (i = 1; i < NGROUPS; i++) {
	crp->cr_groups[i] = NOGROUP;
    }

    call_syscall(sysArgsP->syscall, sysArgsP->afscall, sysArgsP->param1,
		 sysArgsP->param2, sysArgsP->param3, sysArgsP->param4);

    afs_osi_Free(argp, -1);
    return 0;
}

int
fork_syscall(long syscall, long afscall, long param1, long param2,
	     long param3, long param4)
{
    usr_thread_t tid;
    struct syscallThreadArgs *sysArgsP;

    sysArgsP = (struct syscallThreadArgs *)
	afs_osi_Alloc(sizeof(struct syscallThreadArgs));
    usr_assert(sysArgsP != NULL);
    sysArgsP->syscall = syscall;
    sysArgsP->afscall = afscall;
    sysArgsP->param1 = param1;
    sysArgsP->param2 = param2;
    sysArgsP->param3 = param3;
    sysArgsP->param4 = param4;

    usr_thread_create(&tid, syscallThread, sysArgsP);
    usr_thread_detach(tid);
    return 0;
}

int
call_syscall(long syscall, long afscall, long param1, long param2,
	     long param3, long param4)
{
    int code = 0;
    struct a {
	long syscall;
	long afscall;
	long parm1;
	long parm2;
	long parm3;
	long parm4;
    } a;

    a.syscall = syscall;
    a.afscall = afscall;
    a.parm1 = param1;
    a.parm2 = param2;
    a.parm3 = param3;
    a.parm4 = param4;

    get_user_struct()->u_error = 0;
    get_user_struct()->u_ap = (char *)&a;

    code = Afs_syscall();
    return code;
}

int
uafs_Setup(const char *mount)
{
    int rc;
    static int inited = 0;

    if (inited) {
	return EEXIST;
    }
    inited = 1;

    rc = calcMountDir(mount, afs_mountDir, sizeof(afs_mountDir));
    if (rc) {
	return rc;
    }
    afs_mountDirLen = strlen(afs_mountDir);

    /* initialize global vars and such */
    osi_Init();

    /* initialize cache manager foo */
    afsd_init();

    return 0;
}

int
uafs_ParseArgs(int argc, char **argv)
{
    return afsd_parse(argc, argv);
}

int
uafs_Run(void)
{
    return afsd_run();
}

const char *
uafs_MountDir(void)
{
    return afsd_cacheMountDir;
}

int
uafs_SetTokens(char *tbuffer, int tlen)
{
    int rc;
    struct afs_ioctl iob;
    char outbuf[1024];

    iob.in = tbuffer;
    iob.in_size = tlen;
    iob.out = &outbuf[0];
    iob.out_size = sizeof(outbuf);

    rc = call_syscall(AFSCALL_PIOCTL, 0, _VICEIOCTL(3), (long)&iob, 0, 0);
    if (rc != 0) {
	errno = rc;
	return -1;
    }
    return 0;
}

int
uafs_RPCStatsEnableProc(void)
{
    int rc;
    struct afs_ioctl iob;
    afs_int32 flag;

    flag = AFSCALL_RXSTATS_ENABLE;
    iob.in = (char *)&flag;
    iob.in_size = sizeof(afs_int32);
    iob.out = NULL;
    iob.out_size = 0;
    rc = call_syscall(AFSCALL_PIOCTL, 0, _VICEIOCTL(53), (long)&iob, 0, 0);
    if (rc != 0) {
	errno = rc;
	return -1;
    }
    return rc;
}

int
uafs_RPCStatsDisableProc(void)
{
    int rc;
    struct afs_ioctl iob;
    afs_int32 flag;

    flag = AFSCALL_RXSTATS_DISABLE;
    iob.in = (char *)&flag;
    iob.in_size = sizeof(afs_int32);
    iob.out = NULL;
    iob.out_size = 0;
    rc = call_syscall(AFSCALL_PIOCTL, 0, _VICEIOCTL(53), (long)&iob, 0, 0);
    if (rc != 0) {
	errno = rc;
	return -1;
    }
    return rc;
}

int
uafs_RPCStatsClearProc(void)
{
    int rc;
    struct afs_ioctl iob;
    afs_int32 flag;

    flag = AFSCALL_RXSTATS_CLEAR;
    iob.in = (char *)&flag;
    iob.in_size = sizeof(afs_int32);
    iob.out = NULL;
    iob.out_size = 0;
    rc = call_syscall(AFSCALL_PIOCTL, 0, _VICEIOCTL(53), (long)&iob, 0, 0);
    if (rc != 0) {
	errno = rc;
	return -1;
    }
    return rc;
}

int
uafs_RPCStatsEnablePeer(void)
{
    int rc;
    struct afs_ioctl iob;
    afs_int32 flag;

    flag = AFSCALL_RXSTATS_ENABLE;
    iob.in = (char *)&flag;
    iob.in_size = sizeof(afs_int32);
    iob.out = NULL;
    iob.out_size = 0;
    rc = call_syscall(AFSCALL_PIOCTL, 0, _VICEIOCTL(54), (long)&iob, 0, 0);
    if (rc != 0) {
	errno = rc;
	return -1;
    }
    return rc;
}

int
uafs_RPCStatsDisablePeer(void)
{
    int rc;
    struct afs_ioctl iob;
    afs_int32 flag;

    flag = AFSCALL_RXSTATS_DISABLE;
    iob.in = (char *)&flag;
    iob.in_size = sizeof(afs_int32);
    iob.out = NULL;
    iob.out_size = 0;
    rc = call_syscall(AFSCALL_PIOCTL, 0, _VICEIOCTL(54), (long)&iob, 0, 0);
    if (rc != 0) {
	errno = rc;
	return -1;
    }
    return rc;
}

int
uafs_RPCStatsClearPeer(void)
{
    int rc;
    struct afs_ioctl iob;
    afs_int32 flag;

    flag = AFSCALL_RXSTATS_CLEAR;
    iob.in = (char *)&flag;
    iob.in_size = sizeof(afs_int32);
    iob.out = NULL;
    iob.out_size = 0;
    rc = call_syscall(AFSCALL_PIOCTL, 0, _VICEIOCTL(54), (long)&iob, 0, 0);
    if (rc != 0) {
	errno = rc;
	return -1;
    }
    return rc;
}

/*
 * Lookup a file or directory given its path.
 * Call VN_HOLD on the output vnode if successful.
 * Returns zero on success, error code on failure.
 *
 * Note: Caller must hold the AFS global lock.
 */
int
uafs_LookupName(char *path, struct usr_vnode *parentVp,
		struct usr_vnode **vpp, int follow, int no_eval_mtpt)
{
    int code = 0;
    int linkCount;
    struct usr_vnode *vp;
    struct usr_vnode *nextVp;
    struct usr_vnode *linkVp;
    struct vcache *nextVc;
    char *tmpPath;
    char *pathP;
    char *nextPathP = NULL;

    AFS_ASSERT_GLOCK();

    /*
     * Absolute paths must start with the AFS mount point.
     */
    if (path[0] != '/') {
	vp = parentVp;
    } else {
	path = uafs_afsPathName(path);
	if (path == NULL) {
	    return ENOENT;
	}
	vp = afs_RootVnode;
    }

    /*
     * Loop through the path looking for the new directory
     */
    tmpPath = afs_osi_Alloc(strlen(path) + 1);
    usr_assert(tmpPath != NULL);
    strcpy(tmpPath, path);
    VN_HOLD(vp);
    pathP = tmpPath;
    while (pathP != NULL && *pathP != '\0') {
	usr_assert(*pathP != '/');

	/*
	 * terminate the current component and skip over slashes
	 */
	nextPathP = afs_strchr(pathP, '/');
	if (nextPathP != NULL) {
	    while (*nextPathP == '/') {
		*(nextPathP++) = '\0';
	    }
	}

	/*
	 * Don't call afs_lookup on non-directories
	 */
	if (vp->v_type != VDIR) {
	    VN_RELE(vp);
	    afs_osi_Free(tmpPath, strlen(path) + 1);
	    return ENOTDIR;
	}

	if (vp == afs_RootVnode && strcmp(pathP, "..") == 0) {
	    /*
	     * The AFS root is its own parent
	     */
	    nextVp = afs_RootVnode;
	} else {
	    /*
	     * We need execute permission to search a directory
	     */
	    code = afs_access(VTOAFS(vp), VEXEC, get_user_struct()->u_cred);
	    if (code != 0) {
		VN_RELE(vp);
		afs_osi_Free(tmpPath, strlen(path) + 1);
		return code;
	    }

	    /*
	     * lookup the next component in the path, we can release the
	     * subdirectory since we hold the global lock
	     */
	    nextVc = NULL;
	    nextVp = NULL;
	    if ((nextPathP != NULL && *nextPathP != '\0') || !no_eval_mtpt)
		code = afs_lookup(VTOAFS(vp), pathP, &nextVc, get_user_struct()->u_cred, 0);
	    else
		code =
		    afs_lookup(VTOAFS(vp), pathP, &nextVc, get_user_struct()->u_cred,
			       AFS_LOOKUP_NOEVAL);
	    if (nextVc)
		nextVp=AFSTOV(nextVc);
	    if (code != 0) {
		VN_RELE(vp);
		afs_osi_Free(tmpPath, strlen(path) + 1);
		return code;
	    }
	}

	/*
	 * Follow symbolic links for parent directories and
	 * for leaves when the follow flag is set.
	 */
	if ((nextPathP != NULL && *nextPathP != '\0') || follow) {
	    linkCount = 0;
	    while (nextVp->v_type == VLNK) {
		if (++linkCount > MAX_OSI_LINKS) {
		    VN_RELE(vp);
		    VN_RELE(nextVp);
		    afs_osi_Free(tmpPath, strlen(path) + 1);
		    return code;
		}
		code = uafs_LookupLink(nextVp, vp, &linkVp);
		if (code) {
		    VN_RELE(vp);
		    VN_RELE(nextVp);
		    afs_osi_Free(tmpPath, strlen(path) + 1);
		    return code;
		}
		VN_RELE(nextVp);
		nextVp = linkVp;
	    }
	}

	VN_RELE(vp);
	vp = nextVp;
	pathP = nextPathP;
    }

    /*
     * Special case, nextPathP is non-null if pathname ends in slash
     */
    if (nextPathP != NULL && vp->v_type != VDIR) {
	VN_RELE(vp);
	afs_osi_Free(tmpPath, strlen(path) + 1);
	return ENOTDIR;
    }

    afs_osi_Free(tmpPath, strlen(path) + 1);
    *vpp = vp;
    return 0;
}

/*
 * Lookup the target of a symbolic link
 * Call VN_HOLD on the output vnode if successful.
 * Returns zero on success, error code on failure.
 *
 * Note: Caller must hold the AFS global lock.
 */
int
uafs_LookupLink(struct usr_vnode *vp, struct usr_vnode *parentVp,
		struct usr_vnode **vpp)
{
    int code;
    int len;
    char *pathP;
    struct usr_vnode *linkVp;
    struct usr_uio uio;
    struct iovec iov[1];

    AFS_ASSERT_GLOCK();

    pathP = afs_osi_Alloc(MAX_OSI_PATH + 1);
    usr_assert(pathP != NULL);

    /*
     * set up the uio buffer
     */
    iov[0].iov_base = pathP;
    iov[0].iov_len = MAX_OSI_PATH + 1;
    uio.uio_iov = &iov[0];
    uio.uio_iovcnt = 1;
    uio.uio_offset = 0;
    uio.uio_segflg = 0;
    uio.uio_fmode = FREAD;
    uio.uio_resid = MAX_OSI_PATH + 1;

    /*
     * Read the link data
     */
    code = afs_readlink(VTOAFS(vp), &uio, get_user_struct()->u_cred);
    if (code) {
	afs_osi_Free(pathP, MAX_OSI_PATH + 1);
	return code;
    }
    len = MAX_OSI_PATH + 1 - uio.uio_resid;
    pathP[len] = '\0';

    /*
     * Find the target of the symbolic link
     */
    code = uafs_LookupName(pathP, parentVp, &linkVp, 1, 0);
    if (code) {
	afs_osi_Free(pathP, MAX_OSI_PATH + 1);
	return code;
    }

    afs_osi_Free(pathP, MAX_OSI_PATH + 1);
    *vpp = linkVp;
    return 0;
}

/*
 * Lookup the parent of a file or directory given its path
 * Call VN_HOLD on the output vnode if successful.
 * Returns zero on success, error code on failure.
 *
 * Note: Caller must hold the AFS global lock.
 */
int
uafs_LookupParent(char *path, struct usr_vnode **vpp)
{
    int len;
    int code;
    char *pathP;
    struct usr_vnode *parentP;

    AFS_ASSERT_GLOCK();

    /*
     * Absolute path names must start with the AFS mount point.
     */
    if (*path == '/') {
	pathP = uafs_afsPathName(path);
	if (pathP == NULL) {
	    return ENOENT;
	}
    }

    /*
     * Find the length of the parent path
     */
    len = strlen(path);
    while (len > 0 && path[len - 1] == '/') {
	len--;
    }
    if (len == 0) {
	return EINVAL;
    }
    while (len > 0 && path[len - 1] != '/') {
	len--;
    }
    if (len == 0) {
	return EINVAL;
    }

    pathP = afs_osi_Alloc(len);
    usr_assert(pathP != NULL);
    memcpy(pathP, path, len - 1);
    pathP[len - 1] = '\0';

    /*
     * look up the parent
     */
    code = uafs_LookupName(pathP, afs_CurrentDir, &parentP, 1, 0);
    afs_osi_Free(pathP, len);
    if (code != 0) {
	return code;
    }
    if (parentP->v_type != VDIR) {
	VN_RELE(parentP);
	return ENOTDIR;
    }

    *vpp = parentP;
    return 0;
}

/*
 * Return a pointer to the first character in the last component
 * of a pathname
 */
char *
uafs_LastPath(char *path)
{
    int len;

    len = strlen(path);
    while (len > 0 && path[len - 1] == '/') {
	len--;
    }
    while (len > 0 && path[len - 1] != '/') {
	len--;
    }
    if (len == 0) {
	return NULL;
    }
    return path + len;
}

/*
 * Set the working directory.
 */
int
uafs_chdir(char *path)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_chdir_r(path);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_chdir_r(char *path)
{
    int code;
    struct vnode *dirP;

    code = uafs_LookupName(path, afs_CurrentDir, &dirP, 1, 0);
    if (code != 0) {
	errno = code;
	return -1;
    }
    if (dirP->v_type != VDIR) {
	VN_RELE(dirP);
	errno = ENOTDIR;
	return -1;
    }
    VN_RELE(afs_CurrentDir);
    afs_CurrentDir = dirP;
    return 0;
}

/*
 * Create a directory.
 */
int
uafs_mkdir(char *path, int mode)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_mkdir_r(path, mode);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_mkdir_r(char *path, int mode)
{
    int code;
    char *nameP;
    struct vnode *parentP;
    struct vcache *dirP;
    struct usr_vattr attrs;

    if (uafs_IsRoot(path)) {
	return EACCES;
    }

    /*
     * Look up the parent directory.
     */
    nameP = uafs_LastPath(path);
    if (nameP != NULL) {
	code = uafs_LookupParent(path, &parentP);
	if (code != 0) {
	    errno = code;
	    return -1;
	}
    } else {
	parentP = afs_CurrentDir;
	nameP = path;
	VN_HOLD(parentP);
    }

    /*
     * Make sure the directory has at least one character
     */
    if (*nameP == '\0') {
	VN_RELE(parentP);
	errno = EINVAL;
	return -1;
    }

    /*
     * Create the directory
     */
    usr_vattr_null(&attrs);
    attrs.va_type = VREG;
    attrs.va_mode = mode;
    attrs.va_uid = afs_cr_uid(get_user_struct()->u_cred);
    attrs.va_gid = afs_cr_gid(get_user_struct()->u_cred);
    dirP = NULL;
    code = afs_mkdir(VTOAFS(parentP), nameP, &attrs, &dirP, get_user_struct()->u_cred);
    VN_RELE(parentP);
    if (code != 0) {
	errno = code;
	return -1;
    }
    VN_RELE(AFSTOV(dirP));
    return 0;
}

/*
 * Return 1 if path is the AFS root, otherwise return 0
 */
int
uafs_IsRoot(char *path)
{
    while (*path == '/' && *(path + 1) == '/') {
	path++;
    }
    if (strncmp(path, afs_mountDir, afs_mountDirLen) != 0) {
	return 0;
    }
    path += afs_mountDirLen;
    while (*path == '/') {
	path++;
    }
    if (*path != '\0') {
	return 0;
    }
    return 1;
}

/*
 * Open a file
 * Note: file name may not end in a slash.
 */
int
uafs_open(char *path, int flags, int mode)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_open_r(path, flags, mode);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_open_r(char *path, int flags, int mode)
{
    int fd;
    int code;
    int openFlags;
    int fileMode;
    struct usr_vnode *fileP;
    struct usr_vnode *dirP;
    struct usr_vattr attrs;
    char *nameP;

    struct vcache* vc;

    if (uafs_IsRoot(path)) {
	fileP = afs_RootVnode;
	VN_HOLD(fileP);
    } else {
	/*
	 * Look up the parent directory.
	 */
	nameP = uafs_LastPath(path);
	if (nameP != NULL) {
	    code = uafs_LookupParent(path, &dirP);
	    if (code != 0) {
		errno = code;
		return -1;
	    }
	} else {
	    dirP = afs_CurrentDir;
	    nameP = path;
	    VN_HOLD(dirP);
	}

	/*
	 * Make sure the filename has at least one character
	 */
	if (*nameP == '\0') {
	    VN_RELE(dirP);
	    errno = EINVAL;
	    return -1;
	}

	/*
	 * Get the VNODE for this file
	 */
	if (flags & O_CREAT) {
	    usr_vattr_null(&attrs);
	    attrs.va_type = VREG;
	    attrs.va_mode = mode;
	    attrs.va_uid = afs_cr_uid(get_user_struct()->u_cred);
	    attrs.va_gid = afs_cr_gid(get_user_struct()->u_cred);
	    if (flags & O_TRUNC) {
		attrs.va_size = 0;
	    }
	    fileP = NULL;
	    vc=VTOAFS(fileP);
	    code =
		afs_create(VTOAFS(dirP), nameP, &attrs,
			   (flags & O_EXCL) ? usr_EXCL : usr_NONEXCL, mode,
			   &vc, get_user_struct()->u_cred);
	    VN_RELE(dirP);
	    if (code != 0) {
		errno = code;
		return -1;
	    }
	    fileP = AFSTOV(vc);
	} else {
	    fileP = NULL;
	    code = uafs_LookupName(nameP, dirP, &fileP, 1, 0);
	    VN_RELE(dirP);
	    if (code != 0) {
		errno = code;
		return -1;
	    }

	    /*
	     * Check whether we have access to this file
	     */
	    fileMode = 0;
	    if (flags & (O_RDONLY | O_RDWR)) {
		fileMode |= VREAD;
	    }
	    if (flags & (O_WRONLY | O_RDWR)) {
		fileMode |= VWRITE;
	    }
	    if (!fileMode)
		fileMode = VREAD;	/* since O_RDONLY is 0 */
	    code = afs_access(VTOAFS(fileP), fileMode, get_user_struct()->u_cred);
	    if (code != 0) {
		VN_RELE(fileP);
		errno = code;
		return -1;
	    }

	    /*
	     * Get the file attributes, all we need is the size
	     */
	    code = afs_getattr(VTOAFS(fileP), &attrs, get_user_struct()->u_cred);
	    if (code != 0) {
		VN_RELE(fileP);
		errno = code;
		return -1;
	    }
	}
    }

    /*
     * Setup the open flags
     */
    openFlags = 0;
    if (flags & O_TRUNC) {
	openFlags |= FTRUNC;
    }
    if (flags & O_APPEND) {
	openFlags |= FAPPEND;
    }
    if (flags & O_SYNC) {
	openFlags |= FSYNC;
    }
    if (flags & O_SYNC) {
	openFlags |= FSYNC;
    }
    if (flags & (O_RDONLY | O_RDWR)) {
	openFlags |= FREAD;
    }
    if (flags & (O_WRONLY | O_RDWR)) {
	openFlags |= FWRITE;
    }
    if ((openFlags & (FREAD | FWRITE)) == 0) {
	/* O_RDONLY is 0, so ... */
	openFlags |= FREAD;
    }

    /*
     * Truncate if necessary
     */
    if ((flags & O_TRUNC) && (attrs.va_size != 0)) {
	usr_vattr_null(&attrs);
	attrs.va_mask = ATTR_SIZE;
	attrs.va_size = 0;
	code = afs_setattr(VTOAFS(fileP), &attrs, get_user_struct()->u_cred);
	if (code != 0) {
	    VN_RELE(fileP);
	    errno = code;
	    return -1;
	}
    }

    vc=VTOAFS(fileP);	
    /*
     * do the open
     */
    code = afs_open(&vc, openFlags, get_user_struct()->u_cred);
    if (code != 0) {
	VN_RELE(fileP);
	errno = code;
	return -1;
    }

    /*
     * Put the vnode pointer into the file table
     */
    for (fd = 0; fd < MAX_OSI_FILES; fd++) {
	if (afs_FileTable[fd] == NULL) {
	    afs_FileTable[fd] = fileP;
	    afs_FileFlags[fd] = openFlags;
	    if (flags & O_APPEND) {
		afs_FileOffsets[fd] = attrs.va_size;
	    } else {
		afs_FileOffsets[fd] = 0;
	    }
	    break;
	}
    }
    if (fd == MAX_OSI_FILES) {
	VN_RELE(fileP);
	errno = ENFILE;
	return -1;
    }

    return fd;
}

/*
 * Create a file
 */
int
uafs_creat(char *path, int mode)
{
    int rc;
    rc = uafs_open(path, O_CREAT | O_WRONLY | O_TRUNC, mode);
    return rc;
}

int
uafs_creat_r(char *path, int mode)
{
    int rc;
    rc = uafs_open_r(path, O_CREAT | O_WRONLY | O_TRUNC, mode);
    return rc;
}

/*
 * Write to a file
 */
int
uafs_write(int fd, char *buf, int len)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_pwrite_r(fd, buf, len, afs_FileOffsets[fd]);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_pwrite(int fd, char *buf, int len, off_t offset)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_pwrite_r(fd, buf, len, offset);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_pwrite_r(int fd, char *buf, int len, off_t offset)
{
    int code;
    struct usr_uio uio;
    struct iovec iov[1];
    struct usr_vnode *fileP;

    /*
     * Make sure this is an open file
     */
    fileP = afs_FileTable[fd];
    if (fileP == NULL) {
	errno = EBADF;
	return -1;
    }

    /*
     * set up the uio buffer
     */
    iov[0].iov_base = buf;
    iov[0].iov_len = len;
    uio.uio_iov = &iov[0];
    uio.uio_iovcnt = 1;
    uio.uio_offset = offset;
    uio.uio_segflg = 0;
    uio.uio_fmode = FWRITE;
    uio.uio_resid = len;

    /*
     * do the write
     */

    code = afs_write(VTOAFS(fileP), &uio, afs_FileFlags[fd], get_user_struct()->u_cred, 0);
    if (code) {
	errno = code;
	return -1;
    }

    afs_FileOffsets[fd] = uio.uio_offset;
    return (len - uio.uio_resid);
}

/*
 * Read from a file
 */
int
uafs_read(int fd, char *buf, int len)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_pread_r(fd, buf, len, afs_FileOffsets[fd]);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_pread_nocache(int fd, char *buf, int len, off_t offset)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_pread_nocache_r(fd, buf, len, offset);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_pread_nocache_r(int fd, char *buf, int len, off_t offset)
{
    int code;
    struct iovec iov[1];
    struct usr_vnode *fileP;
    struct nocache_read_request *bparms;
    struct usr_uio uio;

    /*
     * Make sure this is an open file
     */
    fileP = afs_FileTable[fd];
    if (fileP == NULL) {
	errno = EBADF;
	return -1;
    }

    /* these get freed in PrefetchNoCache, so... */
    bparms = afs_osi_Alloc(sizeof(struct nocache_read_request));
    bparms->areq = afs_osi_Alloc(sizeof(struct vrequest));

    code = afs_InitReq(bparms->areq, get_user_struct()->u_cred);
    if (code) {
	afs_osi_Free(bparms->areq, sizeof(struct vrequest));
	afs_osi_Free(bparms, sizeof(struct nocache_read_request));
	errno = code;
	return -1;
    }

    bparms->auio = &uio;
    bparms->offset = offset;
    bparms->length = len;

    /*
     * set up the uio buffer
     */
    iov[0].iov_base = buf;
    iov[0].iov_len = len;
    uio.uio_iov = &iov[0];
    uio.uio_iovcnt = 1;
    uio.uio_offset = offset;
    uio.uio_segflg = 0;
    uio.uio_fmode = FREAD;
    uio.uio_resid = len;

    /*
     * do the read
     */
    code = afs_PrefetchNoCache(VTOAFS(fileP), get_user_struct()->u_cred,
			       bparms);

    if (code) {
	errno = code;
	return -1;
    }

    afs_FileOffsets[fd] = uio.uio_offset;
    return (len - uio.uio_resid);
}

int
uafs_pread(int fd, char *buf, int len, off_t offset)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_pread_r(fd, buf, len, offset);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_pread_r(int fd, char *buf, int len, off_t offset)
{
    int code;
    struct usr_uio uio;
    struct iovec iov[1];
    struct usr_vnode *fileP;
    struct usr_buf *bufP;

    /*
     * Make sure this is an open file
     */
    fileP = afs_FileTable[fd];
    if (fileP == NULL) {
	errno = EBADF;
	return -1;
    }

    /*
     * set up the uio buffer
     */
    iov[0].iov_base = buf;
    iov[0].iov_len = len;
    uio.uio_iov = &iov[0];
    uio.uio_iovcnt = 1;
    uio.uio_offset = offset;
    uio.uio_segflg = 0;
    uio.uio_fmode = FREAD;
    uio.uio_resid = len;

    /*
     * do the read
     */
    code = afs_read(VTOAFS(fileP), &uio, get_user_struct()->u_cred, 0, &bufP, 0);
    if (code) {
	errno = code;
	return -1;
    }

    afs_FileOffsets[fd] = uio.uio_offset;
    return (len - uio.uio_resid);
}

/*
 * Copy the attributes of a file into a stat structure.
 *
 * NOTE: Caller must hold the global AFS lock.
 */
int
uafs_GetAttr(struct usr_vnode *vp, struct stat *stats)
{
    int code;
    struct usr_vattr attrs;

    AFS_ASSERT_GLOCK();

    /*
     * Get the attributes
     */
    code = afs_getattr(VTOAFS(vp), &attrs, get_user_struct()->u_cred);
    if (code != 0) {
	return code;
    }

    /*
     * Copy the attributes, zero fields that aren't set
     */
    memset((void *)stats, 0, sizeof(struct stat));
    stats->st_dev = -1;
    stats->st_ino = attrs.va_nodeid;
    stats->st_mode = attrs.va_mode;
    stats->st_nlink = attrs.va_nlink;
    stats->st_uid = attrs.va_uid;
    stats->st_gid = attrs.va_gid;
    stats->st_rdev = attrs.va_rdev;
    stats->st_size = attrs.va_size;
    stats->st_atime = attrs.va_atime.tv_sec;
    stats->st_mtime = attrs.va_mtime.tv_sec;
    stats->st_ctime = attrs.va_ctime.tv_sec;
    /* preserve dv if possible */
#if defined(HAVE_STRUCT_STAT_ST_CTIMESPEC)
    stats->st_atimespec.tv_nsec = attrs.va_atime.tv_usec * 1000;
    stats->st_mtimespec.tv_nsec = attrs.va_mtime.tv_usec * 1000;
    stats->st_ctimespec.tv_nsec = attrs.va_ctime.tv_usec * 1000;
#elif defined(HAVE_STRUCT_STAT_ST_CTIMENSEC)
    stats->st_atimensec = attrs.va_atime.tv_usec * 1000;
    stats->st_mtimensec = attrs.va_mtime.tv_usec * 1000;
    stats->st_ctimensec = attrs.va_ctime.tv_usec * 1000;
#endif
    stats->st_blksize = attrs.va_blocksize;
    stats->st_blocks = attrs.va_blocks;

    return 0;
}

/*
 * Get the attributes of a file, do follow links
 */
int
uafs_stat(char *path, struct stat *buf)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_stat_r(path, buf);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_stat_r(char *path, struct stat *buf)
{
    int code;
    struct vnode *vp;

    code = uafs_LookupName(path, afs_CurrentDir, &vp, 1, 0);
    if (code != 0) {
	errno = code;
	return -1;
    }
    code = uafs_GetAttr(vp, buf);
    VN_RELE(vp);
    if (code) {
	errno = code;
	return -1;
    }
    return 0;
}

/*
 * Get the attributes of a file, don't follow links
 */
int
uafs_lstat(char *path, struct stat *buf)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_lstat_r(path, buf);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_lstat_r(char *path, struct stat *buf)
{
    int code;
    struct vnode *vp;

    code = uafs_LookupName(path, afs_CurrentDir, &vp, 0, 0);
    if (code != 0) {
	errno = code;
	return -1;
    }
    code = uafs_GetAttr(vp, buf);
    VN_RELE(vp);
    if (code) {
	errno = code;
	return -1;
    }
    return 0;
}

/*
 * Get the attributes of an open file
 */
int
uafs_fstat(int fd, struct stat *buf)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_fstat_r(fd, buf);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_fstat_r(int fd, struct stat *buf)
{
    int code;
    struct vnode *vp;

    vp = afs_FileTable[fd];
    if (vp == NULL) {
	errno = EBADF;
	return -1;
    }
    code = uafs_GetAttr(vp, buf);
    if (code) {
	errno = code;
	return -1;
    }
    return 0;
}

/*
 * change the permissions on a file
 */
int
uafs_chmod(char *path, int mode)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_chmod_r(path, mode);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_chmod_r(char *path, int mode)
{
    int code;
    struct vnode *vp;
    struct usr_vattr attrs;

    code = uafs_LookupName(path, afs_CurrentDir, &vp, 1, 0);
    if (code != 0) {
	errno = code;
	return -1;
    }
    usr_vattr_null(&attrs);
    attrs.va_mask = ATTR_MODE;
    attrs.va_mode = mode;
    code = afs_setattr(VTOAFS(vp), &attrs, get_user_struct()->u_cred);
    VN_RELE(vp);
    if (code != 0) {
	errno = code;
	return -1;
    }
    return 0;
}

/*
 * change the permissions on an open file
 */
int
uafs_fchmod(int fd, int mode)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_fchmod_r(fd, mode);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_fchmod_r(int fd, int mode)
{
    int code;
    struct vnode *vp;
    struct usr_vattr attrs;

    vp = afs_FileTable[fd];
    if (vp == NULL) {
	errno = EBADF;
	return -1;
    }
    usr_vattr_null(&attrs);
    attrs.va_mask = ATTR_MODE;
    attrs.va_mode = mode;
    code = afs_setattr(VTOAFS(vp), &attrs, get_user_struct()->u_cred);
    if (code != 0) {
	errno = code;
	return -1;
    }
    return 0;
}

/*
 * truncate a file
 */
int
uafs_truncate(char *path, int length)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_truncate_r(path, length);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_truncate_r(char *path, int length)
{
    int code;
    struct vnode *vp;
    struct usr_vattr attrs;

    code = uafs_LookupName(path, afs_CurrentDir, &vp, 1, 0);
    if (code != 0) {
	errno = code;
	return -1;
    }
    usr_vattr_null(&attrs);
    attrs.va_mask = ATTR_SIZE;
    attrs.va_size = length;
    code = afs_setattr(VTOAFS(vp), &attrs, get_user_struct()->u_cred);
    VN_RELE(vp);
    if (code != 0) {
	errno = code;
	return -1;
    }
    return 0;
}

/*
 * truncate an open file
 */
int
uafs_ftruncate(int fd, int length)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_ftruncate_r(fd, length);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_ftruncate_r(int fd, int length)
{
    int code;
    struct vnode *vp;
    struct usr_vattr attrs;

    vp = afs_FileTable[fd];
    if (vp == NULL) {
	errno = EBADF;
	return -1;
    }
    usr_vattr_null(&attrs);
    attrs.va_mask = ATTR_SIZE;
    attrs.va_size = length;
    code = afs_setattr(VTOAFS(vp), &attrs, get_user_struct()->u_cred);
    if (code != 0) {
	errno = code;
	return -1;
    }
    return 0;
}

/*
 * set the read/write file pointer of an open file
 */
int
uafs_lseek(int fd, int offset, int whence)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_lseek_r(fd, offset, whence);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_lseek_r(int fd, int offset, int whence)
{
    int code;
    int newpos;
    struct usr_vattr attrs;
    struct usr_vnode *vp;

    vp = afs_FileTable[fd];
    if (vp == NULL) {
	errno = EBADF;
	return -1;
    }
    switch (whence) {
    case SEEK_CUR:
	newpos = afs_FileOffsets[fd] + offset;
	break;
    case SEEK_SET:
	newpos = offset;
	break;
    case SEEK_END:
	code = afs_getattr(VTOAFS(vp), &attrs, get_user_struct()->u_cred);
	if (code != 0) {
	    errno = code;
	    return -1;
	}
	newpos = attrs.va_size + offset;
	break;
    default:
	errno = EINVAL;
	return -1;
    }
    if (newpos < 0) {
	errno = EINVAL;
	return -1;
    }
    afs_FileOffsets[fd] = newpos;
    return newpos;
}

/*
 * sync a file
 */
int
uafs_fsync(int fd)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_fsync_r(fd);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_fsync_r(int fd)
{
    int code;
    struct usr_vnode *fileP;


    fileP = afs_FileTable[fd];
    if (fileP == NULL) {
	errno = EBADF;
	return -1;
    }

    code = afs_fsync(VTOAFS(fileP), get_user_struct()->u_cred);
    if (code != 0) {
	errno = code;
	return -1;
    }

    return 0;
}

/*
 * Close a file
 */
int
uafs_close(int fd)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_close_r(fd);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_close_r(int fd)
{
    int code;
    struct usr_vnode *fileP;

    fileP = afs_FileTable[fd];
    if (fileP == NULL) {
	errno = EBADF;
	return -1;
    }
    afs_FileTable[fd] = NULL;

    code = afs_close(VTOAFS(fileP), afs_FileFlags[fd], get_user_struct()->u_cred);
    VN_RELE(fileP);
    if (code != 0) {
	errno = code;
	return -1;
    }

    return 0;
}

/*
 * Create a hard link from the source to the target
 * Note: file names may not end in a slash.
 */
int
uafs_link(char *existing, char *new)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_link_r(existing, new);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_link_r(char *existing, char *new)
{
    int code;
    struct usr_vnode *existP;
    struct usr_vnode *dirP;
    char *nameP;

    if (uafs_IsRoot(new)) {
	return EACCES;
    }

    /*
     * Look up the existing node.
     */
    code = uafs_LookupName(existing, afs_CurrentDir, &existP, 1, 0);
    if (code != 0) {
	errno = code;
	return -1;
    }

    /*
     * Look up the parent directory.
     */
    nameP = uafs_LastPath(new);
    if (nameP != NULL) {
	code = uafs_LookupParent(new, &dirP);
	if (code != 0) {
	    VN_RELE(existP);
	    errno = code;
	    return -1;
	}
    } else {
	dirP = afs_CurrentDir;
	nameP = new;
	VN_HOLD(dirP);
    }

    /*
     * Make sure the filename has at least one character
     */
    if (*nameP == '\0') {
	VN_RELE(existP);
	VN_RELE(dirP);
	errno = EINVAL;
	return -1;
    }

    /*
     * Create the link
     */
    code = afs_link(VTOAFS(existP), VTOAFS(dirP), nameP, get_user_struct()->u_cred);
    VN_RELE(existP);
    VN_RELE(dirP);
    if (code != 0) {
	errno = code;
	return -1;
    }
    return 0;
}

/*
 * Create a symbolic link from the source to the target
 * Note: file names may not end in a slash.
 */
int
uafs_symlink(char *target, char *source)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_symlink_r(target, source);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_symlink_r(char *target, char *source)
{
    int code;
    struct usr_vnode *dirP;
    struct usr_vattr attrs;
    char *nameP;

    if (uafs_IsRoot(source)) {
	return EACCES;
    }

    /*
     * Look up the parent directory.
     */
    nameP = uafs_LastPath(source);
    if (nameP != NULL) {
	code = uafs_LookupParent(source, &dirP);
	if (code != 0) {
	    errno = code;
	    return -1;
	}
    } else {
	dirP = afs_CurrentDir;
	nameP = source;
	VN_HOLD(dirP);
    }

    /*
     * Make sure the filename has at least one character
     */
    if (*nameP == '\0') {
	VN_RELE(dirP);
	errno = EINVAL;
	return -1;
    }

    /*
     * Create the link
     */
    usr_vattr_null(&attrs);
    attrs.va_type = VLNK;
    attrs.va_mode = 0777;
    attrs.va_uid = afs_cr_uid(get_user_struct()->u_cred);
    attrs.va_gid = afs_cr_gid(get_user_struct()->u_cred);
    code = afs_symlink(VTOAFS(dirP), nameP, &attrs, target, NULL,
    		       get_user_struct()->u_cred);
    VN_RELE(dirP);
    if (code != 0) {
	errno = code;
	return -1;
    }
    return 0;
}

/*
 * Read a symbolic link into the buffer
 */
int
uafs_readlink(char *path, char *buf, int len)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_readlink_r(path, buf, len);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_readlink_r(char *path, char *buf, int len)
{
    int code;
    struct usr_vnode *vp;
    struct usr_uio uio;
    struct iovec iov[1];

    code = uafs_LookupName(path, afs_CurrentDir, &vp, 0, 0);
    if (code != 0) {
	errno = code;
	return -1;
    }

    if (vp->v_type != VLNK) {
	VN_RELE(vp);
	errno = EINVAL;
	return -1;
    }

    /*
     * set up the uio buffer
     */
    iov[0].iov_base = buf;
    iov[0].iov_len = len;
    uio.uio_iov = &iov[0];
    uio.uio_iovcnt = 1;
    uio.uio_offset = 0;
    uio.uio_segflg = 0;
    uio.uio_fmode = FREAD;
    uio.uio_resid = len;

    /*
     * Read the the link
     */
    code = afs_readlink(VTOAFS(vp), &uio, get_user_struct()->u_cred);
    VN_RELE(vp);
    if (code) {
	errno = code;
	return -1;
    }

    /*
     * return the number of bytes read
     */
    return (len - uio.uio_resid);
}

/*
 * Remove a file (or directory)
 * Note: file name may not end in a slash.
 */
int
uafs_unlink(char *path)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_unlink_r(path);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_unlink_r(char *path)
{
    int code;
    struct usr_vnode *dirP;
    char *nameP;

    if (uafs_IsRoot(path)) {
	return EACCES;
    }

    /*
     * Look up the parent directory.
     */
    nameP = uafs_LastPath(path);
    if (nameP != NULL) {
	code = uafs_LookupParent(path, &dirP);
	if (code != 0) {
	    errno = code;
	    return -1;
	}
    } else {
	dirP = afs_CurrentDir;
	nameP = path;
	VN_HOLD(dirP);
    }

    /*
     * Make sure the filename has at least one character
     */
    if (*nameP == '\0') {
	VN_RELE(dirP);
	errno = EINVAL;
	return -1;
    }

    /*
     * Remove the file
     */
    code = afs_remove(VTOAFS(dirP), nameP, get_user_struct()->u_cred);
    VN_RELE(dirP);
    if (code != 0) {
	errno = code;
	return -1;
    }

    return 0;
}

/*
 * Rename a file (or directory)
 */
int
uafs_rename(char *old, char *new)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_rename_r(old, new);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_rename_r(char *old, char *new)
{
    int code;
    char *onameP;
    char *nnameP;
    struct usr_vnode *odirP;
    struct usr_vnode *ndirP;

    if (uafs_IsRoot(new)) {
	return EACCES;
    }

    /*
     * Look up the parent directories.
     */
    onameP = uafs_LastPath(old);
    if (onameP != NULL) {
	code = uafs_LookupParent(old, &odirP);
	if (code != 0) {
	    errno = code;
	    return -1;
	}
    } else {
	odirP = afs_CurrentDir;
	onameP = old;
	VN_HOLD(odirP);
    }
    nnameP = uafs_LastPath(new);
    if (nnameP != NULL) {
	code = uafs_LookupParent(new, &ndirP);
	if (code != 0) {
	    errno = code;
	    return -1;
	}
    } else {
	ndirP = afs_CurrentDir;
	nnameP = new;
	VN_HOLD(ndirP);
    }

    /*
     * Make sure the filename has at least one character
     */
    if (*onameP == '\0' || *nnameP == '\0') {
	VN_RELE(odirP);
	VN_RELE(ndirP);
	errno = EINVAL;
	return -1;
    }

    /*
     * Rename the file
     */
    code = afs_rename(VTOAFS(odirP), onameP, VTOAFS(ndirP), nnameP, get_user_struct()->u_cred);
    VN_RELE(odirP);
    VN_RELE(ndirP);
    if (code != 0) {
	errno = code;
	return -1;
    }

    return 0;
}

/*
 * Remove a or directory
 * Note: file name may not end in a slash.
 */
int
uafs_rmdir(char *path)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_rmdir_r(path);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_rmdir_r(char *path)
{
    int code;
    struct usr_vnode *dirP;
    char *nameP;

    if (uafs_IsRoot(path)) {
	return EACCES;
    }

    /*
     * Look up the parent directory.
     */
    nameP = uafs_LastPath(path);
    if (nameP != NULL) {
	code = uafs_LookupParent(path, &dirP);
	if (code != 0) {
	    errno = code;
	    return -1;
	}
    } else {
	dirP = afs_CurrentDir;
	nameP = path;
	VN_HOLD(dirP);
    }

    /*
     * Make sure the directory name has at least one character
     */
    if (*nameP == '\0') {
	VN_RELE(dirP);
	errno = EINVAL;
	return -1;
    }

    /*
     * Remove the directory
     */
    code = afs_rmdir(VTOAFS(dirP), nameP, get_user_struct()->u_cred);
    VN_RELE(dirP);
    if (code != 0) {
	errno = code;
	return -1;
    }

    return 0;
}

/*
 * Flush a file from the AFS cache
 */
int
uafs_FlushFile(char *path)
{
    int code;
    struct afs_ioctl iob;

    iob.in = NULL;
    iob.in_size = 0;
    iob.out = NULL;
    iob.out_size = 0;

    code =
	call_syscall(AFSCALL_PIOCTL, (long)path, _VICEIOCTL(6), (long)&iob, 0,
		     0);
    if (code != 0) {
	errno = code;
	return -1;
    }

    return 0;
}

int
uafs_FlushFile_r(char *path)
{
    int retval;
    AFS_GUNLOCK();
    retval = uafs_FlushFile(path);
    AFS_GLOCK();
    return retval;
}

/*
 * open a directory
 */
usr_DIR *
uafs_opendir(char *path)
{
    usr_DIR *retval;
    AFS_GLOCK();
    retval = uafs_opendir_r(path);
    AFS_GUNLOCK();
    return retval;
}

usr_DIR *
uafs_opendir_r(char *path)
{
    usr_DIR *dirp;
    struct usr_vnode *fileP;
    int fd;

    /*
     * Open the directory for reading
     */
    fd = uafs_open_r(path, O_RDONLY, 0);
    if (fd < 0) {
	return NULL;
    }

    fileP = afs_FileTable[fd];
    if (fileP == NULL) {
	return NULL;
    }

    if (fileP->v_type != VDIR) {
	uafs_close_r(fd);
	errno = ENOTDIR;
	return NULL;
    }

    /*
     * Set up the directory structures
     */
    dirp =
	(usr_DIR *) afs_osi_Alloc(sizeof(usr_DIR) + USR_DIRSIZE +
				  sizeof(struct usr_dirent));
    usr_assert(dirp != NULL);
    dirp->dd_buf = (char *)(dirp + 1);
    dirp->dd_fd = fd;
    dirp->dd_loc = 0;
    dirp->dd_size = 0;

    errno = 0;
    return dirp;
}

/*
 * Read directory entries into a file system independent format.
 * This routine was developed to support AFS cache consistency testing.
 * You should use uafs_readdir instead.
 */
int
uafs_getdents(int fd, struct min_direct *buf, int len)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_getdents_r(fd, buf, len);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_getdents_r(int fd, struct min_direct *buf, int len)
{
    int code;
    struct usr_uio uio;
    struct usr_vnode *vp;
    struct iovec iov[1];

    /*
     * Make sure this is an open file
     */
    vp = afs_FileTable[fd];
    if (vp == NULL) {
	AFS_GUNLOCK();
	errno = EBADF;
	return -1;
    }

    /*
     * set up the uio buffer
     */
    iov[0].iov_base = (char *)buf;
    iov[0].iov_len = len;
    uio.uio_iov = &iov[0];
    uio.uio_iovcnt = 1;
    uio.uio_offset = afs_FileOffsets[fd];
    uio.uio_segflg = 0;
    uio.uio_fmode = FREAD;
    uio.uio_resid = len;

    /*
     * read the next chunk from the directory
     */
    code = afs_readdir(VTOAFS(vp), &uio, get_user_struct()->u_cred);
    if (code != 0) {
	errno = code;
	return -1;
    }

    afs_FileOffsets[fd] = uio.uio_offset;
    return (len - uio.uio_resid);
}

/*
 * read from a directory (names only)
 */
struct usr_dirent *
uafs_readdir(usr_DIR * dirp)
{
    struct usr_dirent *retval;
    AFS_GLOCK();
    retval = uafs_readdir_r(dirp);
    AFS_GUNLOCK();
    return retval;
}

struct usr_dirent *
uafs_readdir_r(usr_DIR * dirp)
{
    int code;
    int len;
    struct usr_uio uio;
    struct usr_vnode *vp;
    struct iovec iov[1];
    struct usr_dirent *direntP;
    struct min_direct *directP;

    if (!dirp) {
	errno = EBADF;
	return NULL;
    }

    /*
     * Make sure this is an open file
     */
    vp = afs_FileTable[dirp->dd_fd];
    if (vp == NULL) {
	errno = EBADF;
	return NULL;
    }

    /*
     * If there are no entries in the stream buffer
     * then read another chunk
     */
    directP = (struct min_direct *)(dirp->dd_buf + dirp->dd_loc);
    if (dirp->dd_size == 0 || directP->d_fileno == 0) {
	/*
	 * set up the uio buffer
	 */
	iov[0].iov_base = dirp->dd_buf;
	iov[0].iov_len = USR_DIRSIZE;
	uio.uio_iov = &iov[0];
	uio.uio_iovcnt = 1;
	uio.uio_offset = afs_FileOffsets[dirp->dd_fd];
	uio.uio_segflg = 0;
	uio.uio_fmode = FREAD;
	uio.uio_resid = USR_DIRSIZE;

	/*
	 * read the next chunk from the directory
	 */
	code = afs_readdir(VTOAFS(vp), &uio, get_user_struct()->u_cred);
	if (code != 0) {
	    errno = code;
	    return NULL;
	}
	afs_FileOffsets[dirp->dd_fd] = uio.uio_offset;

	dirp->dd_size = USR_DIRSIZE - iov[0].iov_len;
	dirp->dd_loc = 0;
	directP = (struct min_direct *)(dirp->dd_buf + dirp->dd_loc);
    }

    /*
     * Check for end of file
     */
    if (dirp->dd_size == 0 || directP->d_fileno == 0) {
	errno = 0;
	return NULL;
    }
    len = ((sizeof(struct min_direct) + directP->d_namlen + 4) & (~3));
    usr_assert(len <= dirp->dd_size);

    /*
     * Copy the next entry into the usr_dirent structure and advance
     */
    direntP = (struct usr_dirent *)(dirp->dd_buf + USR_DIRSIZE);
    direntP->d_ino = directP->d_fileno;
    direntP->d_off = direntP->d_reclen;
    direntP->d_reclen =
	sizeof(struct usr_dirent) - MAXNAMLEN + directP->d_namlen + 1;
    memcpy(&direntP->d_name[0], (void *)(directP + 1), directP->d_namlen);
    direntP->d_name[directP->d_namlen] = '\0';
    dirp->dd_loc += len;
    dirp->dd_size -= len;

    return direntP;
}

/*
 * Close a directory
 */
int
uafs_closedir(usr_DIR * dirp)
{
    int retval;
    AFS_GLOCK();
    retval = uafs_closedir_r(dirp);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_closedir_r(usr_DIR * dirp)
{
    int fd;
    int rc;

    if (!dirp) {
	errno = EBADF;
	return -1;
    }

    fd = dirp->dd_fd;
    afs_osi_Free((char *)dirp,
		 sizeof(usr_DIR) + USR_DIRSIZE + sizeof(struct usr_dirent));
    rc = uafs_close_r(fd);
    return rc;
}

/*
 * Do AFS authentication
 */
int
uafs_klog(char *user, char *cell, char *passwd, char **reason)
{
    int code;
    afs_int32 password_expires = -1;

    usr_mutex_lock(&osi_authenticate_lock);
    code =
	ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION +
				   KA_USERAUTH_DOSETPAG2, user, NULL, cell,
				   passwd, 0, &password_expires, 0, reason);
    usr_mutex_unlock(&osi_authenticate_lock);
    return code;
}

int
uafs_klog_r(char *user, char *cell, char *passwd, char **reason)
{
    int retval;
    AFS_GUNLOCK();
    retval = uafs_klog(user, cell, passwd, reason);
    AFS_GLOCK();
    return retval;
}

/*
 * Destroy AFS credentials from the kernel cache
 */
int
uafs_unlog(void)
{
    int code;

    usr_mutex_lock(&osi_authenticate_lock);
    code = ktc_ForgetAllTokens();
    usr_mutex_unlock(&osi_authenticate_lock);
    return code;
}

int
uafs_unlog_r(void)
{
    int retval;
    AFS_GUNLOCK();
    retval = uafs_unlog();
    AFS_GLOCK();
    return retval;
}

/*
 * Strip the AFS mount point from a pathname string. Return
 * NULL if the path is a relative pathname or if the path
 * doesn't start with the AFS mount point string.
 */
char *
uafs_afsPathName(char *path)
{
    char *p;
    char lastchar;
    int i;

    if (path[0] != '/')
	return NULL;
    lastchar = '/';
    for (i = 1, p = path + 1; *p != '\0'; p++) {
	/* Ignore duplicate slashes */
	if (*p == '/' && lastchar == '/')
	    continue;
	/* Is this a subdirectory of the AFS mount point? */
	if (afs_mountDir[i] == '\0' && *p == '/') {
	    /* strip leading slashes */
	    while (*(++p) == '/');
	    return p;
	}
	/* Reject paths that are not within AFS */
	if (*p != afs_mountDir[i])
	    return NULL;
	lastchar = *p;
	i++;
    }
    /* Is this the AFS mount point? */
    if (afs_mountDir[i] == '\0') {
	usr_assert(*p == '\0');
	return p;
    }
    return NULL;
}

/*
 * uafs_klog_nopag
 * klog but don't allocate a new pag
 */
int
uafs_klog_nopag(char *user, char *cell, char *passwd, char **reason)
{
    int code;
    afs_int32 password_expires = -1;

    usr_mutex_lock(&osi_authenticate_lock);
    code = ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION
				      /*+KA_USERAUTH_DOSETPAG2 */ , user,
				      NULL, cell, passwd, 0,
				      &password_expires, 0, reason);
    usr_mutex_unlock(&osi_authenticate_lock);
    return code;
}

/*
 * uafs_getcellstatus
 * get the cell status
 */
int
uafs_getcellstatus(char *cell, afs_int32 * status)
{
    int rc;
    struct afs_ioctl iob;

    iob.in = cell;
    iob.in_size = strlen(cell) + 1;
    iob.out = 0;
    iob.out_size = 0;

    rc = call_syscall(AFSCALL_PIOCTL, /*path */ 0, _VICEIOCTL(35),
		      (long)&iob, 0, 0);

    if (rc < 0) {
	errno = rc;
	return -1;
    }

    *status = (intptr_t)iob.out;
    return 0;
}

/*
 * uafs_getvolquota
 * Get quota of volume associated with path
 */
int
uafs_getvolquota(char *path, afs_int32 * BlocksInUse, afs_int32 * MaxQuota)
{
    int rc;
    struct afs_ioctl iob;
    VolumeStatus *status;
    char buf[1024];

    iob.in = 0;
    iob.in_size = 0;
    iob.out = buf;
    iob.out_size = 1024;

    rc = call_syscall(AFSCALL_PIOCTL, (long)path, _VICEIOCTL(4), (long)&iob,
		      0, 0);

    if (rc != 0) {
	errno = rc;
	return -1;
    }

    status = (VolumeStatus *) buf;
    *BlocksInUse = status->BlocksInUse;
    *MaxQuota = status->MaxQuota;
    return 0;
}

/*
 * uafs_setvolquota
 * Set quota of volume associated with path
 */
int
uafs_setvolquota(char *path, afs_int32 MaxQuota)
{
    int rc;
    struct afs_ioctl iob;
    VolumeStatus *status;
    char buf[1024];

    iob.in = buf;
    iob.in_size = 1024;
    iob.out = 0;
    iob.out_size = 0;

    memset(buf, 0, sizeof(VolumeStatus));
    status = (VolumeStatus *) buf;
    status->MaxQuota = MaxQuota;
    status->MinQuota = -1;

    rc = call_syscall(AFSCALL_PIOCTL, (long)path, _VICEIOCTL(5), (long)&iob,
		      0, 0);

    if (rc != 0) {
	errno = rc;
	return -1;
    }

    return 0;
}

/*
 * uafs_statmountpoint
 * Determine whether a dir. is a mount point or not
 * return 1 if mount point, 0 if not
 */
int
uafs_statmountpoint(char *path)
{
    int retval;

    AFS_GLOCK();
    retval = uafs_statmountpoint_r(path);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_statmountpoint_r(char *path)
{
    int code;
    struct vnode *vp;
    struct vcache *avc;
    int r;

    code = uafs_LookupName(path, afs_CurrentDir, &vp, 0, 1);
    if (code != 0) {
	errno = code;
	return -1;
    }

    avc = VTOAFS(vp);

    r = avc->mvstat;
    VN_RELE(vp);
    return r;
}

/*
 * uafs_getRights
 * Get a list of rights for the current user on path.
 */
int
uafs_access(char *path, int flags)
{
    int code;
    struct vnode *vp;
    int fileMode = 0;

    if (flags & R_OK) {
	fileMode |= VREAD;
    }
    if (flags & W_OK) {
	fileMode |= VWRITE;
    }
    if (flags & X_OK) {
	fileMode |= VEXEC;
    }

    AFS_GLOCK();
    code = uafs_LookupName(path, afs_CurrentDir, &vp, 1, 0);
    if (code != 0) {
	errno = code;
	AFS_GUNLOCK();
	return -1;
    }

    code = afs_access(VTOAFS(vp), fileMode, get_user_struct()->u_cred);
    VN_RELE(vp);

    if (code != 0)
	errno = code;

    AFS_GUNLOCK();
    return code ? -1 : 0;
}

/*
 * uafs_getRights
 * Get a list of rights for the current user on path.
 */
int
uafs_getRights(char *path)
{
    int code;
    struct vnode *vp;
    int afs_rights;

    AFS_GLOCK();
    code = uafs_LookupName(path, afs_CurrentDir, &vp, 1, 0);
    if (code != 0) {
	errno = code;
	AFS_GUNLOCK();
	return -1;
    }

    afs_rights =
	PRSFS_READ | PRSFS_WRITE | PRSFS_INSERT | PRSFS_LOOKUP | PRSFS_DELETE
	| PRSFS_LOCK | PRSFS_ADMINISTER;

    afs_rights = afs_getRights(VTOAFS(vp), afs_rights, get_user_struct()->u_cred);

    AFS_GUNLOCK();
    return afs_rights;
}
#endif /* UKERNEL */
