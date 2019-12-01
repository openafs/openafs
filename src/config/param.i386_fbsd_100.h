#ifndef	AFS_PARAM_H
#define	AFS_PARAM_H

/* Machine / Operating system information */
#define SYS_NAME	"i386_fbsd_100"
#define SYS_NAME_ID	SYS_NAME_ID_i386_fbsd_100

#define AFS_FAKEOPEN_ENV 1	/* call afs_FakeOpen as if !AFS_VM_RDWR */

#ifndef UKERNEL
/* This section for kernel libafs compiles only */

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */

#endif /* !defined(UKERNEL) */

#endif /* AFS_PARAM_H */
