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

RCSID
    ("$Header$");


#ifdef	UKERNEL

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include <net/if.h>
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs_usrops.h"
#include "afs/afs_stats.h"
#include "afs/auth.h"
#include "afs/cellconfig.h"
#include "afs/vice.h"
#include "afs/kautils.h"
#include "afs/afsutil.h"
#include "rx/rx_globals.h"

#define VFS 1
#undef	VIRTUE
#undef	VICE

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

struct usr_vnode *afs_FileTable[MAX_OSI_FILES];
int afs_FileFlags[MAX_OSI_FILES];
int afs_FileOffsets[MAX_OSI_FILES];

#define MAX_CACHE_LOOPS 4

struct usr_vfs afs_RootVfs;
struct usr_vnode *afs_RootVnode = NULL;
struct usr_vnode *afs_CurrentDir = NULL;

afs_int32 cacheBlocks;		/* Num blocks in cache */
afs_int32 cacheFiles = 1000;	/* Num files in workstation cache */
afs_int32 cacheStatEntries = 300;	/* Num of stat cache entries */
char cacheBaseDir[1024];	/* AFS cache directory */
char confDir[1024];		/* AFS configuration directory */
char afs_mountDir[1024];	/* AFS mount point */
int afs_mountDirLen;		/* strlen of AFS mount point */
char fullpn_DCacheFile[1024];	/* Full pathname of DCACHEFILE */
char fullpn_VolInfoFile[1024];	/* Full pathname of VOLINFOFILE */
char fullpn_CellInfoFile[1024];	/* Full pathname of CELLINFOFILE */
char fullpn_AFSLogFile[1024];	/* Full pathname of AFSLOGFILE */
char fullpn_CacheInfo[1024];	/* Full pathname of CACHEINFO */
char fullpn_VFile[1024];	/* Full pathname of data cache files */
char *vFileNumber;		/* Ptr to number in file pathname */
char rootVolume[64] = "root.afs";	/* AFS root volume name */
afs_int32 isHomeCell;		/* Is current cell info for home cell */
int createAndTrunc = O_CREAT | O_TRUNC;	/* Create & truncate on open */
int ownerRWmode = 0600;		/* Read/write OK by owner */
static int nDaemons = 2;	/* Number of background daemons */
static int chunkSize = 0;	/* 2^chunkSize bytes per chunk */
static int dCacheSize = 300;	/* # of dcache entries */
static int vCacheSize = 50;	/* # of volume cache entries */
static int cacheFlags = 0;	/* Flags to cache manager */
static int preallocs = 400;	/* Def # of allocated memory blocks */
int afsd_verbose = 0;		/* Are we being chatty? */
int afsd_debug = 0;		/* Are we printing debugging info? */
int afsd_CloseSynch = 0;	/* Are closes synchronous or not? */

#define AFSD_INO_T afs_uint32
char **pathname_for_V;		/* Array of cache file pathnames */
int missing_DCacheFile = 1;	/* Is the DCACHEFILE missing? */
int missing_VolInfoFile = 1;	/* Is the VOLINFOFILE missing? */
int missing_CellInfoFile = 1;
struct afs_cacheParams cparams;	/* params passed to cache manager */
struct afsconf_dir *afs_cdir;	/* config dir */

static int HandleMTab();

int afs_bufferpages = 100;
int usr_udpcksum = 0;

usr_key_t afs_global_u_key;

struct usr_proc *afs_global_procp;
struct usr_ucred *afs_global_ucredp;
struct usr_sysent usr_sysent[200];

#ifdef AFS_USR_OSF_ENV
char V = 'V';
#else /* AFS_USR_OSF_ENV */
long V = 'V';
#endif /* AFS_USR_OSF_ENV */

struct usr_ucred afs_osi_cred, *afs_osi_credp;
usr_mutex_t afs_global_lock;
usr_thread_t afs_global_owner;
usr_mutex_t rx_global_lock;
usr_thread_t rx_global_owner;
usr_mutex_t osi_inode_lock;
usr_mutex_t osi_waitq_lock;
usr_mutex_t osi_authenticate_lock;
afs_lock_t afs_ftf;
afs_lock_t osi_flplock;
afs_lock_t osi_fsplock;
void *vnodefops;

#ifndef NETSCAPE_NSAPI

/*
 * Mutex and condition variable used to implement sleep
 */
pthread_mutex_t usr_sleep_mutex;
pthread_cond_t usr_sleep_cond;

#endif /* !NETSCAPE_NSAPI */

int call_syscall(long, long, long, long, long, long);


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
osi_wait_t *osi_waithash_avail;

/*
 * List of timed waits, NSAPI does not provide a cond_timed
 * wait, so we need to keep track of the timed waits ourselves and
 * periodically check for expirations
 */
osi_wait_t *osi_timedwait_head;
osi_wait_t *osi_timedwait_tail;

struct {
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
}

/*
 * I am not sure what to do with these, they assert for now
 */
int
iodone(struct usr_buf *bp)
{
    usr_assert(0);
}

struct usr_file *
getf(int fd)
{
    usr_assert(0);
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
}

/*
 * ioctl should never fall through, the only files we know
 * about are AFS files
 */
int
usr_ioctl(void)
{
    usr_assert(0);
}

/*
 * We do not support the inode related system calls
 */
int
afs_syscall_icreate(void)
{
    usr_assert(0);
}

int
afs_syscall_iincdec(void)
{
    usr_assert(0);
}

int
afs_syscall_iopen(void)
{
    usr_assert(0);
}

int
afs_syscall_ireadwrite(void)
{
    usr_assert(0);
}

/*
 * these routines are referenced in the vfsops structure, but
 * should never get called
 */
int
vno_close(void)
{
    usr_assert(0);
}

int
vno_ioctl(void)
{
    usr_assert(0);
}

int
vno_rw(void)
{
    usr_assert(0);
}

int
vno_select(void)
{
    usr_assert(0);
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
}

int
usr_crhold(struct usr_ucred *credp)
{
    credp->cr_ref++;
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
}

/*
 * I-node numbers are indeces into a table containing a filename
 * i-node structure and a vnode structure. When we create an i-node,
 * we copy the name into the array and initialize enough of the fields
 * in the inode and vnode structures to get the client to work.
 */
typedef struct {
    struct usr_inode i_node;
    char *name;
} osi_file_table_t;
osi_file_table_t *osi_file_table;
int n_osi_files = 0;
int max_osi_files = 0;

/*
 * Allocate a slot in the file table if there is not one there already,
 * copy in the file name and kludge up the vnode and inode structures
 */
int
lookupname(char *fnamep, int segflg, int followlink,
	   struct usr_vnode **compvpp)
{
    int i;
    int code;
    struct usr_inode *ip;
    struct usr_vnode *vp;

    /*usr_assert(followlink == 0); */

    /*
     * Assume relative pathnames refer to files in AFS
     */
    if (*fnamep != '/' || uafs_afsPathName(fnamep) != NULL) {
	AFS_GLOCK();
	code = uafs_LookupName(fnamep, afs_CurrentDir, compvpp, 0, 0);
	AFS_GUNLOCK();
	return code;
    }

    usr_mutex_lock(&osi_inode_lock);

    for (i = 0; i < n_osi_files; i++) {
	if (strcmp(fnamep, osi_file_table[i].name) == 0) {
	    *compvpp = &osi_file_table[i].i_node.i_vnode;
	    (*compvpp)->v_count++;
	    usr_mutex_unlock(&osi_inode_lock);
	    return 0;
	}
    }

    if (n_osi_files == max_osi_files) {
	usr_mutex_unlock(&osi_inode_lock);
	return ENOSPC;
    }

    osi_file_table[n_osi_files].name = afs_osi_Alloc(strlen(fnamep) + 1);
    usr_assert(osi_file_table[n_osi_files].name != NULL);
    strcpy(osi_file_table[n_osi_files].name, fnamep);
    ip = &osi_file_table[i].i_node;
    vp = &ip->i_vnode;
    vp->v_data = (caddr_t) ip;
    ip->i_dev = -1;
    n_osi_files++;
    ip->i_number = n_osi_files;
    vp->v_count = 2;
    usr_mutex_unlock(&osi_inode_lock);
    *compvpp = vp;
    return 0;
}

/*
 * open a file given its i-node number
 */
void *
osi_UFSOpen(afs_int32 ino)
{
    int rc;
    struct osi_file *fp;
    struct stat st;

    AFS_ASSERT_GLOCK();

    if (ino > n_osi_files) {
	u.u_error = ENOENT;
	return NULL;
    }

    AFS_GUNLOCK();
    fp = (struct osi_file *)afs_osi_Alloc(sizeof(struct osi_file));
    usr_assert(fp != NULL);
    fp->fd = open(osi_file_table[ino - 1].name, O_RDWR | O_CREAT, 0);
    if (fp->fd < 0) {
	u.u_error = errno;
	afs_osi_Free((char *)fp, sizeof(struct osi_file));
	AFS_GLOCK();
	return NULL;
    }
    rc = fstat(fp->fd, &st);
    if (rc < 0) {
	u.u_error = errno;
	afs_osi_Free((void *)fp, sizeof(struct osi_file));
	AFS_GLOCK();
	return NULL;
    }
    fp->size = st.st_size;
    fp->offset = 0;
    fp->inum = ino;
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
	u.u_error = errno;
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
	u.u_error = errno;
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
    int code;
    struct stat st;

    AFS_ASSERT_GLOCK();

    AFS_GUNLOCK();
    if (offset >= 0) {
	rc = lseek(fp->fd, offset, SEEK_SET);
    } else {
	rc = lseek(fp->fd, fp->offset, SEEK_SET);
    }
    if (rc < 0) {
	u.u_error = errno;
	AFS_GLOCK();
	return -1;
    }
    fp->offset = rc;
    ret = read(fp->fd, buf, len);
    if (ret < 0) {
	u.u_error = errno;
	AFS_GLOCK();
	return -1;
    }
    fp->offset += ret;
    rc = fstat(fp->fd, &st);
    if (rc < 0) {
	u.u_error = errno;
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
    int code;
    struct stat st;

    AFS_ASSERT_GLOCK();

    AFS_GUNLOCK();
    if (offset >= 0) {
	rc = lseek(fp->fd, offset, SEEK_SET);
    } else {
	rc = lseek(fp->fd, fp->offset, SEEK_SET);
    }
    if (rc < 0) {
	u.u_error = errno;
	AFS_GLOCK();
	return -1;
    }
    fp->offset = rc;
    ret = write(fp->fd, buf, len);
    if (ret < 0) {
	u.u_error = errno;
	AFS_GLOCK();
	return -1;
    }
    fp->offset += ret;
    rc = fstat(fp->fd, &st);
    if (rc < 0) {
	u.u_error = errno;
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
	u.u_error = errno;
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
	return u.u_error;
    }

    uioP->uio_resid -= rc;
    uioP->uio_offset += rc;
    uioP->uio_iov[0].iov_base = (char *)(uioP->uio_iov[0].iov_base) + rc;
    uioP->uio_iov[0].iov_len -= rc;
    return 0;
}

/*
 * Use malloc/free routines with check patterns before and after each block
 */

static char *afs_check_string1 = "UAFS";
static char *afs_check_string2 = "AFS_OSI_";

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

int
afs_nfsclient_init(void)
{
    return 0;
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
afs_osi_MapStrategy(int (*aproc) (), struct usr_buf *bp)
{
    afs_int32 returnCode;
    returnCode = (*aproc) (bp);
    return returnCode;
}

void
osi_FlushPages(register struct vcache *avc, struct AFS_UCRED *credp)
{
    ObtainSharedLock(&avc->lock, 555);
    if ((hcmp((avc->m.DataVersion), (avc->mapDV)) <= 0)
	|| ((avc->execsOrWriters > 0) && afs_DirtyPages(avc))) {
	ReleaseSharedLock(&avc->lock);
	return;
    }
    UpgradeSToWLock(&avc->lock, 565);
    hset(avc->mapDV, avc->m.DataVersion);
    ReleaseWriteLock(&avc->lock);
    return;
}

void
osi_FlushText_really(register struct vcache *vp)
{
    if (hcmp(vp->m.DataVersion, vp->flushDV) > 0) {
	hset(vp->flushDV, vp->m.DataVersion);
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
    int rc;
    usr_thread_t tid;

    /*
     * Allocate the table used to implement psuedo-inodes.
     */
    max_osi_files = cacheFiles + 100;
    osi_file_table = (osi_file_table_t *)
	afs_osi_Alloc(max_osi_files * sizeof(osi_file_table_t));
    usr_assert(osi_file_table != NULL);

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
    usr_mutex_init(&osi_inode_lock);
    usr_mutex_init(&osi_waitq_lock);
    usr_mutex_init(&osi_authenticate_lock);

    /*
     * Initialize the AFS OSI credentials
     */
    afs_osi_cred = *afs_global_ucredp;
    afs_osi_credp = &afs_osi_cred;
}

/* ParseArgs is now obsolete, being handled by cmd */

/*---------------------------------------------------------------------
  * GetVFileNumber
  *
  * Description:
  *	Given the final component of a filename expected to be a data cache file,
  *	return the integer corresponding to the file.  Note: we reject names that
  *	are not a ``V'' followed by an integer.  We also reject those names having
  *	the right format but lying outside the range [0..cacheFiles-1].
  *
  * Arguments:
  *	fname : Char ptr to the filename to parse.
  *
  * Returns:
  *	>= 0 iff the file is really a data cache file numbered from 0 to cacheFiles-1, or
  *	-1      otherwise.
  *
  * Environment:
  *	Nothing interesting.
  *
  * Side Effects:
  *	None.
  *------------------------------------------------------------------------*/

int
GetVFileNumber(char *fname)
{
    int computedVNumber;	/*The computed file number we return */
    int filenameLen;		/*Number of chars in filename */
    int currDigit;		/*Current digit being processed */

    /*
     * The filename must have at least two characters, the first of which must be a ``V''
     * and the second of which cannot be a zero unless the file is exactly two chars long.
     */
    filenameLen = strlen(fname);
    if (filenameLen < 2)
	return (-1);
    if (fname[0] != 'V')
	return (-1);
    if ((filenameLen > 2) && (fname[1] == '0'))
	return (-1);

    /*
     * Scan through the characters in the given filename, failing immediately if a non-digit
     * is found.
     */
    for (currDigit = 1; currDigit < filenameLen; currDigit++)
	if (isdigit(fname[currDigit]) == 0)
	    return (-1);

    /*
     * All relevant characters are digits.  Pull out the decimal number they represent.
     * Reject it if it's out of range, otherwise return it.
     */
    computedVNumber = atoi(++fname);
    if (computedVNumber < cacheFiles)
	return (computedVNumber);
    else
	return (-1);
}

/*---------------------------------------------------------------------
  * CreateCacheFile
  *
  * Description:
  *	Given a full pathname for a file we need to create for the workstation AFS
  *	cache, go ahead and create the file.
  *
  * Arguments:
  *	fname : Full pathname of file to create.
  *
  * Returns:
  *	0   iff the file was created,
  *	-1  otherwise.
  *
  * Environment:
  *	The given cache file has been found to be missing.
  *
  * Side Effects:
  *	As described.
  *------------------------------------------------------------------------*/

int
CreateCacheFile(char *fname)
{
    static char rn[] = "CreateCacheFile";	/*Routine name */
    int cfd;			/*File descriptor to AFS cache file */
    int closeResult;		/*Result of close() */

    if (afsd_verbose)
	printf("%s: Creating cache file '%s'\n", rn, fname);
    cfd = open(fname, createAndTrunc, ownerRWmode);
    if (cfd <= 0) {
	printf("%s: Can't create '%s', error return is %d (%d)\n", rn, fname,
	       cfd, errno);
	return (-1);
    }
    closeResult = close(cfd);
    if (closeResult) {
	printf
	    ("%s: Can't close newly-created AFS cache file '%s' (code %d)\n",
	     rn, fname, errno);
	return (-1);
    }

    return (0);
}

/*---------------------------------------------------------------------
  * SweepAFSCache
  *
  * Description:
  *	Sweep through the AFS cache directory, recording the inode number for
  *	each valid data cache file there.  Also, delete any file that doesn't beint32
  *	in the cache directory during this sweep, and remember which of the other
  *	residents of this directory were seen.  After the sweep, we create any data
  *	cache files that were missing.
  *
  * Arguments:
  *	vFilesFound : Set to the number of data cache files found.
  *
  * Returns:
  *	0   if everything went well,
  *	-1 otherwise.
  *
  * Environment:
  *	This routine may be called several times.  If the number of data cache files
  *	found is less than the global cacheFiles, then the caller will need to call it
  *	again to record the inodes of the missing zero-length data cache files created
  *	in the previous call.
  *
  * Side Effects:
  *	Fills up the global pathname_for_V array, may create and/or
  *     delete files as explained above.
  *------------------------------------------------------------------------*/

int
SweepAFSCache(int *vFilesFound)
{
    static char rn[] = "SweepAFSCache";	/*Routine name */
    char fullpn_FileToDelete[1024];	/*File to be deleted from cache */
    char *fileToDelete;		/*Ptr to last component of above */
    DIR *cdirp;			/*Ptr to cache directory structure */
#undef dirent
    struct dirent *currp;	/*Current directory entry */
    int vFileNum;		/*Data cache file's associated number */

    if (cacheFlags & AFSCALL_INIT_MEMCACHE) {
	if (afsd_debug)
	    printf("%s: Memory Cache, no cache sweep done\n", rn);
	*vFilesFound = 0;
	return 0;
    }

    if (afsd_debug)
	printf("%s: Opening cache directory '%s'\n", rn, cacheBaseDir);

    if (chmod(cacheBaseDir, 0700)) {	/* force it to be 700 */
	printf("%s: Can't 'chmod 0700' the cache dir, '%s'.\n", rn,
	       cacheBaseDir);
	return (-1);
    }
    cdirp = opendir(cacheBaseDir);
    if (cdirp == (DIR *) 0) {
	printf("%s: Can't open AFS cache directory, '%s'.\n", rn,
	       cacheBaseDir);
	return (-1);
    }

    /*
     * Scan the directory entries, remembering data cache file inodes and the existance
     * of other important residents.  Delete all files that don't belong here.
     */
    *vFilesFound = 0;
    sprintf(fullpn_FileToDelete, "%s/", cacheBaseDir);
    fileToDelete = fullpn_FileToDelete + strlen(fullpn_FileToDelete);

    for (currp = readdir(cdirp); currp; currp = readdir(cdirp)) {
	if (afsd_debug) {
	    printf("%s: Current directory entry:\n", rn);
	    printf("\tinode=%d, reclen=%d, name='%s'\n", currp->d_ino,
		   currp->d_reclen, currp->d_name);
	}

	/*
	 * Guess current entry is for a data cache file.
	 */
	vFileNum = GetVFileNumber(currp->d_name);
	if (vFileNum >= 0) {
	    /*
	     * Found a valid data cache filename.  Remember this file's name
	     * and bump the number of files found.
	     */
	    pathname_for_V[vFileNum] =
		afs_osi_Alloc(strlen(currp->d_name) + strlen(cacheBaseDir) +
			      2);
	    usr_assert(pathname_for_V[vFileNum] != NULL);
	    sprintf(pathname_for_V[vFileNum], "%s/%s", cacheBaseDir,
		    currp->d_name);
	    (*vFilesFound)++;
	} else if (strcmp(currp->d_name, DCACHEFILE) == 0) {
	    /*
	     * Found the file holding the dcache entries.
	     */
	    missing_DCacheFile = 0;
	} else if (strcmp(currp->d_name, VOLINFOFILE) == 0) {
	    /*
	     * Found the file holding the volume info.
	     */
	    missing_VolInfoFile = 0;
	} else if (strcmp(currp->d_name, CELLINFOFILE) == 0) {
	    missing_CellInfoFile = 0;
	} else if ((strcmp(currp->d_name, ".") == 0)
		   || (strcmp(currp->d_name, "..") == 0)
		   || (strcmp(currp->d_name, "lost+found") == 0)) {
	    /*
	     * Don't do anything - this file is legit, and is to be left alone.
	     */
	} else {
	    /*
	     * This file doesn't belong in the cache.  Nuke it.
	     */
	    sprintf(fileToDelete, "%s", currp->d_name);
	    if (afsd_verbose)
		printf("%s: Deleting '%s'\n", rn, fullpn_FileToDelete);
	    if (unlink(fullpn_FileToDelete)) {
		printf("%s: Can't unlink '%s', errno is %d\n", rn,
		       fullpn_FileToDelete, errno);
	    }
	}
    }

    /*
     * Create all the cache files that are missing.
     */
    if (missing_DCacheFile) {
	if (afsd_verbose)
	    printf("%s: Creating '%s'\n", rn, fullpn_DCacheFile);
	if (CreateCacheFile(fullpn_DCacheFile))
	    printf("%s: Can't create '%s'\n", rn, fullpn_DCacheFile);
    }
    if (missing_VolInfoFile) {
	if (afsd_verbose)
	    printf("%s: Creating '%s'\n", rn, fullpn_VolInfoFile);
	if (CreateCacheFile(fullpn_VolInfoFile))
	    printf("%s: Can't create '%s'\n", rn, fullpn_VolInfoFile);
    }
    if (missing_CellInfoFile) {
	if (afsd_verbose)
	    printf("%s: Creating '%s'\n", rn, fullpn_CellInfoFile);
	if (CreateCacheFile(fullpn_CellInfoFile))
	    printf("%s: Can't create '%s'\n", rn, fullpn_CellInfoFile);
    }

    if (*vFilesFound < cacheFiles) {
	/*
	 * We came up short on the number of data cache files found.  Scan through the inode
	 * list and create all missing files.
	 */
	for (vFileNum = 0; vFileNum < cacheFiles; vFileNum++)
	    if (pathname_for_V[vFileNum] == (AFSD_INO_T) 0) {
		sprintf(vFileNumber, "%d", vFileNum);
		if (afsd_verbose)
		    printf("%s: Creating '%s'\n", rn, fullpn_VFile);
		if (CreateCacheFile(fullpn_VFile))
		    printf("%s: Can't create '%s'\n", rn, fullpn_VFile);
	    }
    }

    /*
     * Close the directory, return success.
     */
    if (afsd_debug)
	printf("%s: Closing cache directory.\n", rn);
    closedir(cdirp);
    return (0);
}

static int
ConfigCell(register struct afsconf_cell *aci, void *arock,
	   struct afsconf_dir *adir)
{
    register int isHomeCell;
    register int i;
    afs_int32 cellFlags = 0;
    afs_int32 hosts[MAXHOSTSPERCELL];

    /* figure out if this is the home cell */
    isHomeCell = (strcmp(aci->name, afs_LclCellName) == 0);
    if (!isHomeCell)
	cellFlags = 2;		/* not home, suid is forbidden */

    /* build address list */
    for (i = 0; i < MAXHOSTSPERCELL; i++)
	memcpy(&hosts[i], &aci->hostAddr[i].sin_addr, sizeof(afs_int32));

    if (aci->linkedCell)
	cellFlags |= 4;		/* Flag that linkedCell arg exists,
				 * for upwards compatibility */

    /* configure one cell */
    call_syscall(AFSCALL_CALL, AFSOP_ADDCELL2, (long)hosts,	/* server addresses */
		 (long)aci->name,	/* cell name */
		 (long)cellFlags,	/* is this the home cell? */
		 (long)aci->linkedCell);	/* Linked cell, if any */
    return 0;
}

static int
ConfigCellAlias(struct afsconf_cellalias *aca, void *arock, struct afsconf_dir *adir)
{
	call_syscall(AFSOP_ADDCELLALIAS, (long)aca->aliasName, 
		     (long)aca->realName, 0, 0, 0);
	return 0;
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
 * Initialize the user space client.
 */
void
uafs_Init(char *rn, char *mountDirParam, char *confDirParam,
	  char *cacheBaseDirParam, int cacheBlocksParam, int cacheFilesParam,
	  int cacheStatEntriesParam, int dCacheSizeParam, int vCacheSizeParam,
	  int chunkSizeParam, int closeSynchParam, int debugParam,
	  int nDaemonsParam, int cacheFlagsParam, char *logFile)
{
    int st;
    struct usr_proc *procp;
    struct usr_ucred *ucredp;
    int i;
    int rc;
    int currVFile;		/* Current AFS cache file number */
    int lookupResult;		/* Result of GetLocalCellName() */
    int cacheIteration;		/* cache verification loop counter */
    int vFilesFound;		/* Num data cache files found in sweep */
    FILE *logfd;
    afs_int32 vfs1_type = -1;
    struct afs_ioctl iob;
    char tbuffer[1024];
    char *p;
    char lastchar;
    afs_int32 buffer[MAXIPADDRS];
    afs_int32 maskbuffer[MAXIPADDRS];
    afs_int32 mtubuffer[MAXIPADDRS];

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
    afs_global_ucredp->cr_uid = geteuid();
    afs_global_ucredp->cr_gid = getegid();
    afs_global_ucredp->cr_ruid = getuid();
    afs_global_ucredp->cr_rgid = getgid();
    afs_global_ucredp->cr_suid = afs_global_ucredp->cr_ruid;
    afs_global_ucredp->cr_sgid = afs_global_ucredp->cr_rgid;
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
    afs_global_procp->p_pid = getpid();
    afs_global_procp->p_ppid = (pid_t) 1;
    afs_global_procp->p_ucred = afs_global_ucredp;

    /*
     * Initialize the AFS mount point, default is '/afs'.
     * Strip duplicate/trailing slashes from mount point string.
     * afs_mountDirLen is set to strlen(afs_mountDir).
     */
    if (mountDirParam) {
	sprintf(tbuffer, "%s", mountDirParam);
    } else {
	sprintf(tbuffer, "afs");
    }
    afs_mountDir[0] = '/';
    afs_mountDirLen = 1;
    for (lastchar = '/', p = &tbuffer[0]; *p != '\0'; p++) {
	if (lastchar != '/' || *p != '/') {
	    afs_mountDir[afs_mountDirLen++] = lastchar = *p;
	}
    }
    if (lastchar == '/' && afs_mountDirLen > 1)
	afs_mountDirLen--;
    afs_mountDir[afs_mountDirLen] = '\0';
    usr_assert(afs_mountDirLen > 1);

    /*
     * Initialize cache parameters using the input arguments
     */

    cacheBlocks = cacheBlocksParam;
    if (cacheFilesParam != 0) {
	cacheFiles = cacheFilesParam;
    } else {
	cacheFiles = cacheBlocks / 10;
    }
    if (cacheStatEntriesParam != 0) {
	cacheStatEntries = cacheStatEntriesParam;
    }
    strcpy(cacheBaseDir, cacheBaseDirParam);
    if (nDaemons != 0) {
	nDaemons = nDaemonsParam;
    } else {
	nDaemons = 3;
    }
    afsd_verbose = debugParam;
    afsd_debug = debugParam;
    chunkSize = chunkSizeParam;
    if (dCacheSizeParam != 0) {
	dCacheSize = dCacheSizeParam;
    } else {
	dCacheSize = cacheFiles / 2;
    }
    if (vCacheSizeParam != 0) {
	vCacheSize = vCacheSizeParam;
    }
    strcpy(confDir, confDirParam);
    afsd_CloseSynch = closeSynchParam;
    if (cacheFlagsParam >= 0) {
	cacheFlags = cacheFlagsParam;
    }
    if (cacheFlags & AFSCALL_INIT_MEMCACHE) {
	cacheFiles = dCacheSize;
    }

    sprintf(fullpn_CacheInfo, "%s/%s", confDir, CACHEINFOFILE);
    if (logFile == NULL) {
	sprintf(fullpn_AFSLogFile, "%s/%s", confDir, AFSLOGFILE);
    } else {
	strcpy(fullpn_AFSLogFile, logFile);
    }

    printf("\n%s: Initializing user space AFS client\n\n", rn);
    printf("    mountDir:           %s\n", afs_mountDir);
    printf("    confDir:            %s\n", confDir);
    printf("    cacheBaseDir:       %s\n", cacheBaseDir);
    printf("    cacheBlocks:        %d\n", cacheBlocks);
    printf("    cacheFiles:         %d\n", cacheFiles);
    printf("    cacheStatEntries:   %d\n", cacheStatEntries);
    printf("    dCacheSize:         %d\n", dCacheSize);
    printf("    vCacheSize:         %d\n", vCacheSize);
    printf("    chunkSize:          %d\n", chunkSize);
    printf("    afsd_CloseSynch:    %d\n", afsd_CloseSynch);
    printf("    afsd_debug/verbose: %d/%d\n", afsd_debug, afsd_verbose);
    printf("    nDaemons:           %d\n", nDaemons);
    printf("    cacheFlags:         %d\n", cacheFlags);
    printf("    logFile:            %s\n", fullpn_AFSLogFile);
    printf("\n");
    fflush(stdout);

    /*
     * Initialize the AFS client
     */
    osi_Init();

    /*
     * Pull out all the configuration info for the workstation's AFS cache and
     * the cellular community we're willing to let our users see.
     */
    afs_cdir = afsconf_Open(confDir);
    if (!afs_cdir) {
	printf("afsd: some file missing or bad in %s\n", confDir);
	exit(1);
    }

    lookupResult =
	afsconf_GetLocalCell(afs_cdir, afs_LclCellName,
			     sizeof(afs_LclCellName));
    if (lookupResult) {
	printf("%s: Can't get my home cell name!  [Error is %d]\n", rn,
	       lookupResult);
    } else {
	if (afsd_verbose)
	    printf("%s: My home cell is '%s'\n", rn, afs_LclCellName);
    }

    /*
     * Set the primary cell name.
     */
    call_syscall(AFSOP_SET_THISCELL, (long)afs_LclCellName, 0, 0, 0, 0);

    if ((logfd = fopen(fullpn_AFSLogFile, "r+")) == 0) {
	if (afsd_verbose)
	    printf("%s: Creating '%s'\n", rn, fullpn_AFSLogFile);
	if (CreateCacheFile(fullpn_AFSLogFile)) {
	    printf
		("%s: Can't create '%s' (You may want to use the -logfile option)\n",
		 rn, fullpn_AFSLogFile);
	    exit(1);
	}
    } else
	fclose(logfd);

    /*
     * Create and zero the pathname table for the desired cache files.
     */
    pathname_for_V = (char **)afs_osi_Alloc(cacheFiles * sizeof(char *));
    if (pathname_for_V == NULL) {
	printf("%s: malloc() failed for cache file table with %d entries.\n",
	       rn, cacheFiles);
	exit(1);
    }
    memset(pathname_for_V, 0, (cacheFiles * sizeof(char *)));
    if (afsd_debug)
	printf("%s: %d pathname_for_V entries at 0x%x, %d bytes\n", rn,
	       cacheFiles, pathname_for_V, (cacheFiles * sizeof(AFSD_INO_T)));

    /*
     * Set up all the pathnames we'll need for later.
     */
    sprintf(fullpn_DCacheFile, "%s/%s", cacheBaseDir, DCACHEFILE);
    sprintf(fullpn_VolInfoFile, "%s/%s", cacheBaseDir, VOLINFOFILE);
    sprintf(fullpn_CellInfoFile, "%s/%s", cacheBaseDir, CELLINFOFILE);
    sprintf(fullpn_VFile, "%s/V", cacheBaseDir);
    vFileNumber = fullpn_VFile + strlen(fullpn_VFile);

    /*
     * Start the RX listener.
     */
    if (afsd_debug)
	printf("%s: Calling AFSOP_RXLISTENER_DAEMON\n", rn);
    fork_syscall(AFSCALL_CALL, AFSOP_RXLISTENER_DAEMON, FALSE, FALSE, FALSE);

    /*
     * Start the RX event handler.
     */
    if (afsd_debug)
	printf("%s: Calling AFSOP_RXEVENT_DAEMON\n", rn);
    fork_syscall(AFSCALL_CALL, AFSOP_RXEVENT_DAEMON, FALSE);

    /*
     * Set up all the kernel processes needed for AFS.
     */

    /* initialize AFS callback interface */
    {
	/* parse multihomed address files */
	char reason[1024];
	st = parseNetFiles((afs_uint32*)buffer,(afs_uint32*) maskbuffer, (afs_uint32*)mtubuffer, MAXIPADDRS, reason,
			   AFSDIR_CLIENT_NETINFO_FILEPATH,
			   AFSDIR_CLIENT_NETRESTRICT_FILEPATH);
	if (st > 0)
	    call_syscall(AFSCALL_CALL, AFSOP_ADVISEADDR, st,
			 (long)(&buffer[0]), (long)(&maskbuffer[0]),
			 (long)(&mtubuffer[0]));
	else {
	    printf("ADVISEADDR: Error in specifying interface addresses:%s\n",
		   reason);
	    exit(1);
	}
    }

    if (afsd_verbose)
	printf("%s: Forking rx callback listener.\n", rn);
    /* Child */
    if (preallocs < cacheStatEntries + 50)
	preallocs = cacheStatEntries + 50;
    fork_syscall(AFSCALL_CALL, AFSOP_START_RXCALLBACK, preallocs);

    if (afsd_verbose)
	printf("%s: Initializing AFS daemon.\n", rn);
    call_syscall(AFSCALL_CALL, AFSOP_BASIC_INIT, 1, 0, 0, 0);

    /*
     * Tell the kernel some basic information about the workstation's cache.
     */
    if (afsd_verbose)
	printf("%s: Calling AFSOP_CACHEINIT: %d stat cache entries,"
	       " %d optimum cache files, %d blocks in the cache,"
	       " flags = 0x%x, dcache entries %d\n", rn, cacheStatEntries,
	       cacheFiles, cacheBlocks, cacheFlags, dCacheSize);
    memset(&cparams, 0, sizeof(cparams));
    cparams.cacheScaches = cacheStatEntries;
    cparams.cacheFiles = cacheFiles;
    cparams.cacheBlocks = cacheBlocks;
    cparams.cacheDcaches = dCacheSize;
    cparams.cacheVolumes = vCacheSize;
    cparams.chunkSize = chunkSize;
    cparams.setTimeFlag = FALSE;
    cparams.memCacheFlag = cacheFlags;
    call_syscall(AFSCALL_CALL, AFSOP_CACHEINIT, (long)&cparams, 0, 0, 0);
    if (afsd_CloseSynch)
	call_syscall(AFSCALL_CALL, AFSOP_CLOSEWAIT, 0, 0, 0, 0);

    /*
     * Sweep the workstation AFS cache directory, remembering the inodes of
     * valid files and deleting extraneous files.  Keep sweeping until we
     * have the right number of data cache files or we've swept too many
     * times.
     */
    if (afsd_verbose)
	printf("%s: Sweeping workstation's AFS cache directory.\n", rn);
    cacheIteration = 0;
    /* Memory-cache based system doesn't need any of this */
    if (!(cacheFlags & AFSCALL_INIT_MEMCACHE)) {
	do {
	    cacheIteration++;
	    if (SweepAFSCache(&vFilesFound)) {
		printf("%s: Error on sweep %d of workstation AFS cache \
		       directory.\n", rn, cacheIteration);
		exit(1);
	    }
	    if (afsd_verbose)
		printf
		    ("%s: %d out of %d data cache files found in sweep %d.\n",
		     rn, vFilesFound, cacheFiles, cacheIteration);
	} while ((vFilesFound < cacheFiles)
		 && (cacheIteration < MAX_CACHE_LOOPS));
    } else if (afsd_verbose)
	printf("%s: Using memory cache, not swept\n", rn);

    /*
     * Pass the kernel the name of the workstation cache file holding the 
     * dcache entries.
     */
    if (afsd_debug)
	printf("%s: Calling AFSOP_CACHEINFO: dcache file is '%s'\n", rn,
	       fullpn_DCacheFile);
    /* once again, meaningless for a memory-based cache. */
    if (!(cacheFlags & AFSCALL_INIT_MEMCACHE))
	call_syscall(AFSCALL_CALL, AFSOP_CACHEINFO, (long)fullpn_DCacheFile,
		     0, 0, 0);

    call_syscall(AFSCALL_CALL, AFSOP_CELLINFO, (long)fullpn_CellInfoFile, 0,
		 0, 0);

    /*
     * Pass the kernel the name of the workstation cache file holding the
     * volume information.
     */
    if (afsd_debug)
	printf("%s: Calling AFSOP_VOLUMEINFO: volume info file is '%s'\n", rn,
	       fullpn_VolInfoFile);
    call_syscall(AFSCALL_CALL, AFSOP_VOLUMEINFO, (long)fullpn_VolInfoFile, 0,
		 0, 0);

    /*
     * Pass the kernel the name of the afs logging file holding the volume
     * information.
     */
    if (afsd_debug)
	printf("%s: Calling AFSOP_AFSLOG: volume info file is '%s'\n", rn,
	       fullpn_AFSLogFile);
    if (!(cacheFlags & AFSCALL_INIT_MEMCACHE))	/* ... nor this ... */
	call_syscall(AFSCALL_CALL, AFSOP_AFSLOG, (long)fullpn_AFSLogFile, 0,
		     0, 0);

    /*
     * Tell the kernel about each cell in the configuration.
     */
    afsconf_CellApply(afs_cdir, ConfigCell, NULL);
    afsconf_CellAliasApply(afs_cdir, ConfigCellAlias, NULL);

    if (afsd_verbose)
	printf("%s: Forking AFS daemon.\n", rn);
    fork_syscall(AFSCALL_CALL, AFSOP_START_AFS);

    if (afsd_verbose)
	printf("%s: Forking check server daemon.\n", rn);
    fork_syscall(AFSCALL_CALL, AFSOP_START_CS);

    if (afsd_verbose)
	printf("%s: Forking %d background daemons.\n", rn, nDaemons);
    for (i = 0; i < nDaemons; i++) {
	fork_syscall(AFSCALL_CALL, AFSOP_START_BKG);
    }

    if (afsd_verbose)
	printf("%s: Calling AFSOP_ROOTVOLUME with '%s'\n", rn, rootVolume);
    call_syscall(AFSCALL_CALL, AFSOP_ROOTVOLUME, (long)rootVolume, 0, 0, 0);

    /*
     * Give the kernel the names of the AFS files cached on the workstation's
     * disk.
     */
    if (afsd_debug)
	printf
	    ("%s: Calling AFSOP_CACHEFILES for each of the %d files in '%s'\n",
	     rn, cacheFiles, cacheBaseDir);
    if (!(cacheFlags & AFSCALL_INIT_MEMCACHE))	/* ... and again ... */
	for (currVFile = 0; currVFile < cacheFiles; currVFile++) {
	    call_syscall(AFSCALL_CALL, AFSOP_CACHEFILE,
			 (long)pathname_for_V[currVFile], 0, 0, 0);
	}
    /*end for */
#ifndef NETSCAPE_NSAPI
    /*
     * Copy our tokens from the kernel to the user space client
     */
    for (i = 0; i < 200; i++) {
	/*
	 * Get the i'th token from the kernel
	 */
	memset((void *)&tbuffer[0], 0, sizeof(tbuffer));
	memcpy((void *)&tbuffer[0], (void *)&i, sizeof(int));
	iob.in = tbuffer;
	iob.in_size = sizeof(int);
	iob.out = tbuffer;
	iob.out_size = sizeof(tbuffer);

#if defined(AFS_USR_SUN5_ENV) || defined(AFS_USR_OSF_ENV) || defined(AFS_USR_HPUX_ENV) || defined(AFS_USR_LINUX22_ENV) || defined(AFS_USR_DARWIN_ENV) || defined(AFS_USR_FBSD_ENV)
	rc = syscall(AFS_SYSCALL, AFSCALL_PIOCTL, 0, _VICEIOCTL(8), &iob, 0);
#elif defined(AFS_USR_SGI_ENV)
	rc = syscall(AFS_PIOCTL, 0, _VICEIOCTL(8), &iob, 0);
#else /* AFS_USR_AIX_ENV */
	rc = lpioctl(0, _VICEIOCTL(8), &iob, 0);
#endif
	if (rc < 0) {
	    usr_assert(errno == EDOM || errno == ENOSYS);
	    break;
	}

	/*
	 * Now pass the token into the user space kernel
	 */
	rc = uafs_SetTokens(tbuffer, iob.out_size);
	usr_assert(rc == 0);
    }
#endif /* !NETSCAPE_NSAPI */

    /*
     * All the necessary info has been passed into the kernel to run an AFS
     * system.  Give the kernel our go-ahead.
     */
    if (afsd_debug)
	printf("%s: Calling AFSOP_GO\n", rn);
    call_syscall(AFSCALL_CALL, AFSOP_GO, FALSE, 0, 0, 0);

    /*
     * At this point, we have finished passing the kernel all the info 
     * it needs to set up the AFS.  Mount the AFS root.
     */
    printf("%s: All AFS daemons started.\n", rn);

    if (afsd_verbose)
	printf("%s: Forking trunc-cache daemon.\n", rn);
    fork_syscall(AFSCALL_CALL, AFSOP_START_TRUNCDAEMON);

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
uafs_Shutdown(void)
{
    int rc;

    printf("\n");

    AFS_GLOCK();
    VN_RELE(afs_CurrentDir);
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
    u.u_viceid = getuid();
    crp = u.u_cred;
    crp->cr_uid = getuid();
    crp->cr_ruid = getuid();
    crp->cr_suid = getuid();
    crp->cr_groups[0] = getgid();
    crp->cr_ngroups = 1;
    for (i = 1; i < NGROUPS; i++) {
	crp->cr_groups[i] = NOGROUP;
    }

    call_syscall(sysArgsP->syscall, sysArgsP->afscall, sysArgsP->param1,
		 sysArgsP->param2, sysArgsP->param3, sysArgsP->param4);

    afs_osi_Free(argp, -1);
}

fork_syscall(syscall, afscall, param1, param2, param3, param4)
     long syscall, afscall, param1, param2, param3, param4;
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
}

call_syscall(syscall, afscall, param1, param2, param3, param4)
     long syscall, afscall, param1, param2, param3, param4;
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

    u.u_error = 0;
    u.u_ap = (char *)&a;

    code = Afs_syscall();
    return code;
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
    int code;
    int linkCount;
    struct usr_vnode *vp;
    struct usr_vnode *nextVp;
    struct usr_vnode *linkVp;
    char *tmpPath;
    char *pathP;
    char *nextPathP;

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
	    code = afs_access(VTOAFS(vp), VEXEC, u.u_cred);
	    if (code != 0) {
		VN_RELE(vp);
		afs_osi_Free(tmpPath, strlen(path) + 1);
		return code;
	    }

	    /*
	     * lookup the next component in the path, we can release the
	     * subdirectory since we hold the global lock
	     */
	    nextVp = NULL;
#ifdef AFS_WEB_ENHANCEMENTS
	    if ((nextPathP != NULL && *nextPathP != '\0') || !no_eval_mtpt)
		code = afs_lookup(vp, pathP, &nextVp, u.u_cred, 0);
	    else
		code =
		    afs_lookup(vp, pathP, &nextVp, u.u_cred,
			       AFS_LOOKUP_NOEVAL);
#else
	    code = afs_lookup(vp, pathP, &nextVp, u.u_cred, 0);
#endif /* AFS_WEB_ENHANCEMENTS */
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
    code = afs_readlink(vp, &uio, u.u_cred);
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
    struct vnode *dirP;
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
    attrs.va_uid = u.u_cred->cr_uid;
    attrs.va_gid = u.u_cred->cr_gid;
    dirP = NULL;
    code = afs_mkdir(parentP, nameP, &attrs, &dirP, u.u_cred);
    VN_RELE(parentP);
    if (code != 0) {
	errno = code;
	return -1;
    }
    VN_RELE(dirP);
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
	    attrs.va_uid = u.u_cred->cr_uid;
	    attrs.va_gid = u.u_cred->cr_gid;
	    if (flags & O_TRUNC) {
		attrs.va_size = 0;
	    }
	    fileP = NULL;
	    vc=VTOAFS(fileP);
	    code =
		afs_create(VTOAFS(dirP), nameP, &attrs,
			   (flags & O_EXCL) ? usr_EXCL : usr_NONEXCL, mode,
			   &vc, u.u_cred);
	    VN_RELE(dirP);
	    if (code != 0) {
		errno = code;
		return -1;
	    }
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
	    code = afs_access(VTOAFS(fileP), fileMode, u.u_cred);
	    if (code != 0) {
		VN_RELE(fileP);
		errno = code;
		return -1;
	    }

	    /*
	     * Get the file attributes, all we need is the size
	     */
	    code = afs_getattr(VTOAFS(fileP), &attrs, u.u_cred);
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
	attrs.va_size = 0;
	code = afs_setattr(VTOAFS(fileP), &attrs, u.u_cred);
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
    code = afs_open(&vc, openFlags, u.u_cred);
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
    retval = uafs_write_r(fd, buf, len);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_write_r(int fd, char *buf, int len)
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
    uio.uio_offset = afs_FileOffsets[fd];
    uio.uio_segflg = 0;
    uio.uio_fmode = FWRITE;
    uio.uio_resid = len;

    /*
     * do the write
     */

    code = afs_write(VTOAFS(fileP), &uio, afs_FileFlags[fd], u.u_cred, 0);
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
    retval = uafs_read_r(fd, buf, len);
    AFS_GUNLOCK();
    return retval;
}

int
uafs_read_r(int fd, char *buf, int len)
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
    uio.uio_offset = afs_FileOffsets[fd];
    uio.uio_segflg = 0;
    uio.uio_fmode = FREAD;
    uio.uio_resid = len;

    /*
     * do the read
     */
    code = afs_read(VTOAFS(fileP), &uio, u.u_cred, 0, &bufP, 0);
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
    code = afs_getattr(VTOAFS(vp), &attrs, u.u_cred);
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
    attrs.va_mode = mode;
    code = afs_setattr(VTOAFS(vp), &attrs, u.u_cred);
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
    attrs.va_mode = mode;
    code = afs_setattr(VTOAFS(vp), &attrs, u.u_cred);
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
    attrs.va_size = length;
    code = afs_setattr(VTOAFS(vp), &attrs, u.u_cred);
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
    attrs.va_size = length;
    code = afs_setattr(VTOAFS(vp), &attrs, u.u_cred);
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
	code = afs_getattr(VTOAFS(vp), &attrs, u.u_cred);
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

    code = afs_fsync(fileP, u.u_cred);
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

    code = afs_close(fileP, afs_FileFlags[fd], u.u_cred);
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
    code = afs_link(existP, dirP, nameP, u.u_cred);
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
    attrs.va_uid = u.u_cred->cr_uid;
    attrs.va_gid = u.u_cred->cr_gid;
    code = afs_symlink(dirP, nameP, &attrs, target, u.u_cred);
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
    code = afs_readlink(vp, &uio, u.u_cred);
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
    int openFlags;
    struct usr_vnode *fileP;
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
    code = afs_remove(dirP, nameP, u.u_cred);
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
    code = afs_rename(odirP, onameP, ndirP, nnameP, u.u_cred);
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
    int openFlags;
    struct usr_vnode *fileP;
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
    code = afs_rmdir(dirP, nameP, u.u_cred);
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
    code = afs_readdir(vp, &uio, u.u_cred);
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
    int rc;
    int code;
    int len;
    struct usr_uio uio;
    struct usr_vnode *vp;
    struct iovec iov[1];
    struct usr_dirent *direntP;
    struct min_direct *directP;

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
	code = afs_readdir(vp, &uio, u.u_cred);
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

#ifdef AFS_WEB_ENHANCEMENTS
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

    *status = (afs_int32) iob.out;
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
    int code;
    char buf[256];

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
    struct vrequest treq;
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
uafs_getRights(char *path)
{
    int code, rc;
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

    afs_rights = afs_getRights(vp, afs_rights, u.u_cred);

    AFS_GUNLOCK();
    return afs_rights;
}
#endif /* AFS_WEB_ENHANCEMENTS */

#endif /* UKERNEL */
