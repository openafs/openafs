#ifndef	AFS_I386_PARAM_H
#define	AFS_I386_PARAM_H

#define AFS_X86_XBSD_ENV 1
#define AFS_X86_ENV 1
#define AFSLITTLE_ENDIAN 1

#define SYS_NAME       "i386_nbsd40"
#define SYS_NAME_ID    SYS_NAME_ID_i386_nbsd40

#ifndef UKERNEL
/* This section for kernel libafs compiles only */

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */

#endif /* !defined(UKERNEL) */

#endif /* AFS_I386_PARAM_H */

