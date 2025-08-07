#ifndef AFS_PARAM_H
# define AFS_PARAM_H

# ifndef UKERNEL
/* This section for kernel libafs compiles only */

#  define AFS_ENV                     1
#  define AFS_64BIT_ENV               1	/* Defines afs_int32 as int, not long. */
#  define AFS_64BIT_CLIENT            1
#  define AFS_64BIT_IOPS_ENV          1
#  define AFS_64BIT_SIZEOF            1 /* seriously? */

#  include <afs/afs_sysnames.h>

#  define AFS_DARWIN_ENV
#  define AFS_DARWIN70_ENV
#  define AFS_DARWIN80_ENV
#  define AFS_DARWIN90_ENV
#  define AFS_DARWIN100_ENV
#  define AFS_DARWIN110_ENV
#  define AFS_DARWIN120_ENV
#  define AFS_DARWIN130_ENV
#  define AFS_DARWIN140_ENV
#  define AFS_DARWIN150_ENV
#  define AFS_DARWIN160_ENV
#  define AFS_DARWIN170_ENV
#  define AFS_DARWIN180_ENV
#  define AFS_DARWIN190_ENV
#  define AFS_DARWIN200_ENV
#  define AFS_DARWIN210_ENV
#  define AFS_DARWIN220_ENV
#  define AFS_DARWIN230_ENV
#  define AFS_DARWIN240_ENV
#  undef  AFS_NONFSTRANS
#  define AFS_NONFSTRANS
#  define AFS_SYSCALL                 230
#  define AFS_NAMEI_ENV               1
#  define DARWIN_REFBASE              3
#  define AFS_WARNUSER_MARINER_ENV    1
#  define AFS_CACHE_VNODE_PATH
#  define AFS_NEW_BKG                 1
#  define NEED_IOCTL32

#  define AFS_HAVE_FFS                1	/* Use system's ffs. */

#  define AFS_GCPAGS                  0
#  define RXK_UPCALL_ENV              1
#  define RXK_TIMEDSLEEP_ENV          1
#  define AFS_USERSPACE_IP_ADDR       1
#  define AFS_SOCKPROXY_ENV           1

#  ifdef KERNEL
#   undef MACRO_BEGIN
#   undef MACRO_END

#   include <kern/macro_help.h>

#   define AFS_GLOBAL_SUNLOCK         1
#   define AFS_VFS34                  1	/* What is VFS34??? */

#   define afsio_iov                  uio_iov
#   define afsio_iovcnt               uio_iovcnt
#   define afsio_offset               uio_offset
#   define afsio_seg                  uio_segflg
#   define afsio_resid                uio_resid

#   define AFS_UIOSYS                 UIO_SYSSPACE
#   define AFS_UIOUSER                UIO_USERSPACE
#   define AFS_CLBYTES                CLBYTES
#   define AFS_KALLOC(x)              _MALLOC(x, M_TEMP, M_WAITOK)
#   define AFS_KFREE(x,y)             _FREE(x,M_TEMP)

#   define v_count                    v_usecount
#   define v_vfsp                     v_mount
#   define vfs_bsize                  mnt_stat.f_bsize
#   define vfs_fsid                   mnt_stat.f_fsid
#   define va_nodeid                  va_fileid
#   define vfs_vnodecovered           mnt_vnodecovered
#   define direct                     dirent

#   define BIND_8_COMPAT
#  endif /* KERNEL */

# else /* !defined(UKERNEL) */

/* This section for user space compiles only */

#  define AFS_ENV                     1
#  define AFS_64BIT_ENV               1	/* Defines afs_int32 as int, not long. */
#  define AFS_64BIT_CLIENT            1

#  include <afs/afs_sysnames.h>

#  define AFS_USERSPACE_ENV
#  define AFS_USR_DARWIN_ENV
#  define AFS_USR_DARWIN70_ENV
#  define AFS_USR_DARWIN80_ENV
#  define AFS_USR_DARWIN90_ENV
#  define AFS_USR_DARWIN100_ENV
#  define AFS_USR_DARWIN110_ENV
#  define AFS_USR_DARWIN120_ENV
#  define AFS_USR_DARWIN130_ENV
#  define AFS_USR_DARWIN140_ENV
#  define AFS_USR_DARWIN150_ENV
#  define AFS_USR_DARWIN160_ENV
#  define AFS_USR_DARWIN170_ENV
#  define AFS_USR_DARWIN180_ENV
#  define AFS_USR_DARWIN190_ENV
#  define AFS_USR_DARWIN200_ENV
#  define AFS_USR_DARWIN210_ENV
#  define AFS_USR_DARWIN220_ENV
#  define AFS_USR_DARWIN230_ENV
#  define AFS_USR_DARWIN240_ENV

#  undef  AFS_NONFSTRANS
#  define AFS_NONFSTRANS

#  define AFS_SYSCALL                 230
#  define DARWIN_REFBASE              0
#  define AFS_WARNUSER_MARINER_ENV    1

#  define AFS_HAVE_FFS                1	/* Use system's ffs. */

#  define AFS_UIOSYS                  UIO_SYSSPACE
#  define AFS_UIOUSER                 UIO_USERSPACE

#  define AFS_GCPAGS                  0	/* if nonzero, garbage collect PAGs */
#  define RXK_LISTENER_ENV            1

#  define AFS_VFS34                   1	/* What is VFS34??? */

#  define afsio_iov                   uio_iov
#  define afsio_iovcnt                uio_iovcnt
#  define afsio_offset                uio_offset
#  define afsio_seg                   uio_segflg
#  define afsio_resid                 uio_resid

#  define VATTR_NULL                  usr_vattr_null

#  define AFS_DIRENT
#  ifndef CMSERVERPREF
#   define CMSERVERPREF
#  endif

#  define BIND_8_COMPAT
# endif /* !defined(UKERNEL) */

/* Machine / Operating system information */
# if defined(__amd64__)

#  define AFS_X86_ENV                 1
#  define AFS_64BITUSERPOINTER_ENV    1

#  define sys_x86_darwin_12           1
#  define sys_x86_darwin_13           1
#  define sys_x86_darwin_14           1
#  define sys_x86_darwin_60           1
#  define sys_x86_darwin_70           1
#  define sys_x86_darwin_80           1
#  define sys_x86_darwin_90           1
#  define sys_x86_darwin_100          1
#  define sys_amd64_darwin_100        1
#  define sys_amd64_darwin_110        1
#  define sys_amd64_darwin_120        1
#  define sys_amd64_darwin_130        1
#  define sys_amd64_darwin_140        1
#  define sys_amd64_darwin_150        1
#  define sys_amd64_darwin_160        1
#  define sys_amd64_darwin_170        1
#  define sys_amd64_darwin_180        1
#  define sys_amd64_darwin_190        1
#  define sys_amd64_darwin_200        1
#  define sys_amd64_darwin_210        1
#  define sys_amd64_darwin_220        1
#  define sys_amd64_darwin_230        1
#  define sys_amd64_darwin_240        1

#  define SYS_NAME                    "amd64_darwin_240"
#  define SYS_NAME_ID                 SYS_NAME_ID_amd64_darwin_240
#  define AFSLITTLE_ENDIAN            1

# elif defined(__arm64__)

#  define AFS_ARM_ENV                 1
#  define AFS_ARM64_DARWIN_ENV        1

#  define sys_arm_darwin_200          1
#  define sys_arm_darwin_210          1
#  define sys_arm_darwin_220          1
#  define sys_arm_darwin_230          1
#  define sys_arm_darwin_240          1

#  define SYS_NAME                    "arm_darwin_240"
#  define SYS_NAME_ID                 SYS_NAME_ID_arm_darwin_240
#  define AFSLITTLE_ENDIAN            1

# else
# error Unsupported architecture
# endif /* __amd64__ */

#endif /* AFS_PARAM_H */
