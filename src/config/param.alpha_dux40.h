#ifndef	_PARAM_OSF1_H_
#define	_PARAM_OSF1_H_

#define	AFS_OSF_ENV	1
#define	AFS_OSF20_ENV	1
#define	AFS_OSF30_ENV	1
#define	AFS_OSF32_ENV	1
#define	AFS_OSF32C_ENV	1
#define	AFS_DUX40_ENV	1
#define	__alpha		1
#define	AFS_ALPHA_ENV	1
#define	AFS_DECOSF_ENV	1
#define	AFS_64BIT_ENV	1

#include <afs/afs_sysnames.h>

#define AFS_VM_RDWR_ENV	1
#define AFS_VFS_ENV	1
#define AFS_GREEDY43_ENV	1
#define AFS_ENV  	1
#define AFS_MINPHYS_ENV	1
#define CMUSTD_ENV	1

#define AFS_SYSCALL	232
#define AFS_MOUNT_AFS	13

#ifndef	MOUNT_AFS
#define	MOUNT_AFS AFS_MOUNT_AFS
#endif
#define SYS_NAME	"alpha_dux40"
#define SYS_NAME_ID	SYS_NAME_ID_alpha_dux40

#define AFS_HAVE_FFS            1       /* Use system's ffs. */
#define AFS_HAVE_STATVFS	1	/* System supports statvfs */

#define AFS_GCPAGS		1       /* if nonzero, garbage collect PAGs */
#define AFS_3DISPARES   1       /* Utilize the 3 available disk inode spares */
#define AFS_USE_GETTIMEOFDAY 1  /* use gettimeofday to implement rx clock */

#if	!defined(ASSEMBLER) && !defined(__LANGUAGE_ASSEMBLY__)
#include <machine/endian.h>
#if	BYTE_ORDER == BIG_ENDIAN
#define	AFSBIG_ENDIAN		1
#else
#if	BYTE_ORDER == LITTLE_ENDIAN
#define	AFSLITTLE_ENDIAN	1
#else
#error	machine/endian.h must define BYTE_ORDER!
#endif
#endif
#endif	/* ! ASSEMBLER && ! __LANGUAGE_ASSEMBLY__ */

#define NEARINODE_HINT  1 /* hint to ufs module to scatter inodes on disk*/
#define nearInodeHash(volid, hval) {                          \
                unsigned char*  ts = (unsigned char*)&(volid);\
                for ((hval)=0; ts<(unsigned char*)&(volid)+sizeof(volid);ts++){\
                    (hval) *= 173;                      \
                    (hval) += *ts;                      \
                }                                       \
                }

/* Extra kernel definitions (from kdefs file) */
#ifdef _KERNEL
#define AFS_GLOBAL_SUNLOCK        1
#define	AFS_VFS34	1	/* What is VFS34??? */
#define	AFS_SHORTGID	1	/* are group id's short? */
#define	afsio_iov	uio_iov
#define	afsio_iovcnt	uio_iovcnt
#define	afsio_offset	uio_offset
#define	afsio_seg	uio_segflg
#define	afsio_resid	uio_resid
#define	AFS_UIOSYS	UIO_SYSSPACE
#define	AFS_UIOUSER	UIO_USERSPACE
#define	AFS_CLBYTES	CLBYTES
#define	osi_GetTime(x)	microtime(x)
#define	AFS_KALLOC(x)	kalloc(x)
#define	AFS_KFREE(x,y)	kfree(x,y)
#define	v_count		v_usecount
#define v_vfsp		v_mount
#define vfs_bsize	m_stat.f_bsize
#define vfs_fsid	m_stat.f_fsid
#define va_nodeid	va_fileid
#define vfs_vnodecovered m_vnodecovered
#define direct		dirent
#define vnode_t		struct vnode

#define	VN_RELE(vp)	vrele(((struct vnode *)(vp)))
#define	VN_HOLD(vp)	VREF(((struct vnode *)(vp)))

#if	!defined(ASSEMBLER) && !defined(__LANGUAGE_ASSEMBLY__)
enum vcexcl {NONEXCL, EXCL};

#include <net/net_globals.h>

#endif	/* ! ASSEMBLER & ! __LANGUAGE_ASSEMBLY__ */
#endif /* _KERNEL */

#endif	/* _PARAM_OSF1_H_ */
