#ifndef AFS_AMD64_PARAM_H
#define AFS_AMD64_PARAM_H

#define AFS_X86_XBSD_ENV 1
#define AFS_X86_ENV 1
#define AFSLITTLE_ENDIAN 1

#define SYS_NAME       "amd64_nbsd20"
#define SYS_NAME_ID    SYS_NAME_ID_amd64_nbsd20

#define AFS_64BITPOINTER_ENV  1
#ifndef UKERNEL
/* This section for kernel libafs compiles only */

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */

#endif /* !defined(UKERNEL) */

#endif /* AFS_AMD64_PARAM_H */
