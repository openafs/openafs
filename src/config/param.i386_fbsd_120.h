#ifndef	AFS_PARAM_H
#define	AFS_PARAM_H

/* Machine / Operating system information */
#define SYS_NAME	"i386_fbsd_120"
#define SYS_NAME_ID	SYS_NAME_ID_i386_fbsd_120

#define AFS_FAKEOPEN_ENV 1	/* call afs_FakeOpen as if !AFS_VM_RDWR */

#ifndef UKERNEL
/* This section for kernel libafs compiles only */

#define AFS_FBSD81_ENV 1
#define AFS_FBSD82_ENV 1
#define AFS_FBSD83_ENV 1
#define AFS_FBSD84_ENV 1
#define AFS_FBSD90_ENV 1
#define AFS_FBSD91_ENV 1
#define AFS_FBSD92_ENV 1
#define AFS_FBSD93_ENV 1
#define AFS_FBSD100_ENV 1
#define AFS_FBSD101_ENV 1
#define AFS_FBSD102_ENV 1
#define AFS_FBSD103_ENV 1
#define AFS_FBSD104_ENV 1
#define AFS_FBSD110_ENV 1
#define AFS_FBSD111_ENV 1
#define AFS_FBSD120_ENV 1

#define AFS_X86_FBSD81_ENV 1
#define AFS_X86_FBSD82_ENV 1
#define AFS_X86_FBSD83_ENV 1
#define AFS_X86_FBSD84_ENV 1
#define AFS_X86_FBSD90_ENV 1
#define AFS_X86_FBSD91_ENV 1
#define AFS_X86_FBSD92_ENV 1
#define AFS_X86_FBSD93_ENV 1
#define AFS_X86_FBSD100_ENV 1
#define AFS_X86_FBSD101_ENV 1
#define AFS_X86_FBSD102_ENV 1
#define AFS_X86_FBSD103_ENV 1
#define AFS_X86_FBSD104_ENV 1
#define AFS_X86_FBSD110_ENV 1
#define AFS_X86_FBSD111_ENV 1
#define AFS_X86_FBSD120_ENV 1

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */

#define AFS_USR_FBSD81_ENV 1
#define AFS_USR_FBSD82_ENV 1
#define AFS_USR_FBSD83_ENV 1
#define AFS_USR_FBSD84_ENV 1
#define AFS_USR_FBSD90_ENV 1
#define AFS_USR_FBSD91_ENV 1
#define AFS_USR_FBSD92_ENV 1
#define AFS_USR_FBSD93_ENV 1
#define AFS_USR_FBSD100_ENV 1
#define AFS_USR_FBSD101_ENV 1
#define AFS_USR_FBSD102_ENV 1
#define AFS_USR_FBSD103_ENV 1
#define AFS_USR_FBSD104_ENV 1
#define AFS_USR_FBSD110_ENV 1
#define AFS_USR_FBSD111_ENV 1
#define AFS_USR_FBSD120_ENV 1

#endif /* !defined(UKERNEL) */

#endif /* AFS_PARAM_H */
