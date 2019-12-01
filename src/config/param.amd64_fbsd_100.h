#ifndef	AFS_PARAM_H
#define	AFS_PARAM_H

/* Machine / Operating system information */
#define SYS_NAME	"amd64_fbsd_100"
#define SYS_NAME_ID	SYS_NAME_ID_amd64_fbsd_100

#define AFS_64BITPOINTER_ENV 1

#ifndef UKERNEL
/* This section for kernel libafs compiles only */

#define AFS_64BITUSERPOINTER_ENV 1

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */

#endif /* !defined(UKERNEL) */

#define USE_UCONTEXT

#endif /* AFS_PARAM_H */
