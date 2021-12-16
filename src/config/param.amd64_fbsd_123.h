#ifndef	AFS_PARAM_H
#define	AFS_PARAM_H

/* Machine / Operating system information */
#define SYS_NAME	"amd64_fbsd_123"
#define SYS_NAME_ID	SYS_NAME_ID_amd64_fbsd_123

#define AFS_64BITPOINTER_ENV 1

#ifndef UKERNEL
/* This section for kernel libafs compiles only */

#define AFS_64BITUSERPOINTER_ENV 1

#define AFS_FBSD101_ENV 1
#define AFS_FBSD102_ENV 1
#define AFS_FBSD103_ENV 1
#define AFS_FBSD104_ENV 1
#define AFS_FBSD110_ENV 1
#define AFS_FBSD111_ENV 1
#define AFS_FBSD120_ENV 1
#define AFS_FBSD121_ENV 1
#define AFS_FBSD122_ENV 1
#define AFS_FBSD123_ENV 1

#define AFS_X86_FBSD101_ENV 1
#define AFS_X86_FBSD102_ENV 1
#define AFS_X86_FBSD103_ENV 1
#define AFS_X86_FBSD104_ENV 1
#define AFS_X86_FBSD110_ENV 1
#define AFS_X86_FBSD111_ENV 1
#define AFS_X86_FBSD120_ENV 1
#define AFS_X86_FBSD121_ENV 1
#define AFS_X86_FBSD122_ENV 1
#define AFS_X86_FBSD123_ENV 1

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */

#define AFS_USR_FBSD101_ENV 1
#define AFS_USR_FBSD102_ENV 1
#define AFS_USR_FBSD103_ENV 1
#define AFS_USR_FBSD104_ENV 1
#define AFS_USR_FBSD110_ENV 1
#define AFS_USR_FBSD111_ENV 1
#define AFS_USR_FBSD120_ENV 1
#define AFS_USR_FBSD121_ENV 1
#define AFS_USR_FBSD122_ENV 1
#define AFS_USR_FBSD123_ENV 1

#endif /* !defined(UKERNEL) */

#define USE_UCONTEXT

#endif /* AFS_PARAM_H */
