#ifndef	AFS_PARAM_H
#define	AFS_PARAM_H

/* Machine / Operating system information */
#define SYS_NAME	"i386_fbsd_70"
#define SYS_NAME_ID	SYS_NAME_ID_i386_fbsd_70

#define AFS_FAKEOPEN_ENV 1	/* call afs_FakeOpen as if !AFS_VM_RDWR */

#ifndef UKERNEL
/* This section for kernel libafs compiles only */

#define AFS_FBSD50_ENV 1
#define AFS_FBSD51_ENV 1
#define AFS_FBSD52_ENV 1
#define AFS_FBSD53_ENV 1
#define AFS_FBSD60_ENV 1
#define AFS_FBSD61_ENV 1
#define AFS_FBSD62_ENV 1
#define AFS_FBSD70_ENV 1

#define AFS_X86_FBSD50_ENV 1
#define AFS_X86_FBSD51_ENV 1
#define AFS_X86_FBSD52_ENV 1
#define AFS_X86_FBSD53_ENV 1
#define AFS_X86_FBSD60_ENV 1
#define AFS_X86_FBSD61_ENV 1
#define AFS_X86_FBSD62_ENV 1
#define AFS_X86_FBSD70_ENV 1

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */

#define AFS_USR_FBSD50_ENV 1
#define AFS_USR_FBSD51_ENV 1
#define AFS_USR_FBSD52_ENV 1
#define AFS_USR_FBSD53_ENV 1
#define AFS_USR_FBSD60_ENV 1
#define AFS_USR_FBSD61_ENV 1
#define AFS_USR_FBSD62_ENV 1
#define AFS_USR_FBSD70_ENV 1

#endif /* !defined(UKERNEL) */

#endif /* AFS_PARAM_H */
