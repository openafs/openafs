#ifndef	AFS_PARAM_H
#define	AFS_PARAM_H

#define AFS_64BITPOINTER_ENV 1

#ifndef UKERNEL
/* This section for kernel libafs compiles only */

#define AFS_64BITUSERPOINTER_ENV 1

#define AFS_FBSD50_ENV 1
#define AFS_FBSD51_ENV 1
#define AFS_FBSD52_ENV 1
#define AFS_FBSD53_ENV 1
#define AFS_X86_FBSD50_ENV 1
#define AFS_X86_FBSD51_ENV 1
#define AFS_X86_FBSD52_ENV 1
#define AFS_X86_FBSD53_ENV 1

#define SYS_NAME	"amd64_fbsd_53"
#define SYS_NAME_ID	SYS_NAME_ID_amd64_fbsd_53

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */

#define AFS_USR_FBSD50_ENV 1
#define AFS_USR_FBSD51_ENV 1
#define AFS_USR_FBSD52_ENV 1
#define AFS_USR_FBSD53_ENV 1

/* Machine / Operating system information */
#define SYS_NAME	"i386_fbsd_53"
#define SYS_NAME_ID	SYS_NAME_ID_i386_fbsd_53

#endif /* !defined(UKERNEL) */

#endif /* AFS_PARAM_H */
