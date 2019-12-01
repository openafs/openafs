#ifndef	AFS_PARAM_H
#define	AFS_PARAM_H

/* Machine / Operating system information */
#define SYS_NAME	"i386_fbsd_103"
#define SYS_NAME_ID	SYS_NAME_ID_i386_fbsd_103

#define AFS_FAKEOPEN_ENV 1	/* call afs_FakeOpen as if !AFS_VM_RDWR */

#ifndef UKERNEL
/* This section for kernel libafs compiles only */

#define AFS_FBSD101_ENV 1
#define AFS_FBSD102_ENV 1
#define AFS_FBSD103_ENV 1

#define AFS_X86_FBSD101_ENV 1
#define AFS_X86_FBSD102_ENV 1
#define AFS_X86_FBSD103_ENV 1

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */

#define AFS_USR_FBSD101_ENV 1
#define AFS_USR_FBSD102_ENV 1
#define AFS_USR_FBSD103_ENV 1

#endif /* !defined(UKERNEL) */

#endif /* AFS_PARAM_H */
