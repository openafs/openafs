#ifndef AFS_PARAM_H
#define AFS_PARAM_H


#define UKERNEL			1	/* user space kernel */
#define AFS_ENV			1
#define AFS_VFSINCL_ENV         1
#define AFS_USR_FBSD40_ENV	1
#define AFS_USR_FBSD42_ENV	1
#define AFS_USR_FBSD43_ENV	1
#define AFS_USR_FBSD44_ENV	1
#define AFS_USR_FBSD_ENV	1
#define AFS_NONFSTRANS 1

#define O_SYNC O_FSYNC

#define AFS_MOUNT_AFS "afs"	/* The name of the filesystem type. */
#define AFS_SYSCALL 210
#define AFS_NAMEI_ENV         1   /* User space interface to file system */
#define AFS_64BIT_IOPS_ENV    1   /* Needed for NAMEI */
#include <afs/afs_sysnames.h>

#define AFS_USERSPACE_IP_ADDR 1
#define RXK_LISTENER_ENV      1
#define AFS_GCPAGS	      0       /* if nonzero, garbage collect PAGs */

/* Machine / Operating system information */
#define SYS_NAME	"i386_fbsd_44"
#define SYS_NAME_ID	SYS_NAME_ID_i386_fbsd_44
#define AFSLITTLE_ENDIAN    1
#define AFS_HAVE_FFS        1       /* Use system's ffs. */
#define AFS_HAVE_STATVFS    0	/* System doesn't support statvfs */
#define AFS_VM_RDWR_ENV	    1	/* read/write implemented via VM */

#define	afsio_iov	uio_iov
#define	afsio_iovcnt	uio_iovcnt
#define	afsio_offset	uio_offset
#define	afsio_seg	uio_segflg
#define	afsio_fmode	uio_fmode
#define	afsio_resid	uio_resid
#define	AFS_UIOSYS	1
#define	AFS_UIOUSER	UIO_USERSPACE
#define	AFS_CLBYTES	MCLBYTES
#define	AFS_MINCHANGE	2
#define	VATTR_NULL	usr_vattr_null

#define AFS_DIRENT
#ifndef CMSERVERPREF
#define CMSERVERPREF
#endif

#include <limits.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <sys/socket.h>

#endif /* AFS_PARAM_H */
