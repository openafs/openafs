#ifndef	AFS_PARAM_H
#define	AFS_PARAM_H

/* Machine / Operating system information */
#define SYS_NAME	"amd64_fbsd_83"
#define SYS_NAME_ID	SYS_NAME_ID_amd64_fbsd_83

#define AFS_64BITPOINTER_ENV 1

#ifndef UKERNEL
/* This section for kernel libafs compiles only */

#define AFS_64BITUSERPOINTER_ENV 1

#define AFS_FBSD81_ENV 1
#define AFS_FBSD82_ENV 1
#define AFS_FBSD83_ENV 1

#define AFS_X86_FBSD81_ENV 1
#define AFS_X86_FBSD82_ENV 1
#define AFS_X86_FBSD83_ENV 1

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */

#define AFS_USR_FBSD81_ENV 1
#define AFS_USR_FBSD82_ENV 1
#define AFS_USR_FBSD83_ENV 1

#endif /* !defined(UKERNEL) */

#endif /* AFS_PARAM_H */
