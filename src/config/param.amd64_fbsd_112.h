#ifndef	AFS_PARAM_H
#define	AFS_PARAM_H

/* Machine / Operating system information */
#define SYS_NAME	"amd64_fbsd_112"
#define SYS_NAME_ID	SYS_NAME_ID_amd64_fbsd_112

#define AFS_64BITPOINTER_ENV 1

#ifndef UKERNEL
/* This section for kernel libafs compiles only */

#define AFS_64BITUSERPOINTER_ENV 1

#define AFS_FBSD50_ENV 1
#define AFS_FBSD51_ENV 1
#define AFS_FBSD52_ENV 1
#define AFS_FBSD53_ENV 1
#define AFS_FBSD60_ENV 1
#define AFS_FBSD61_ENV 1
#define AFS_FBSD62_ENV 1
#define AFS_FBSD70_ENV 1
#define AFS_FBSD71_ENV 1
#define AFS_FBSD72_ENV 1
#define AFS_FBSD73_ENV 1
#define AFS_FBSD80_ENV 1
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
#define AFS_FBSD110_ENV 1
#define AFS_FBSD111_ENV 1
#define AFS_FBSD112_ENV 1

#define AFS_X86_FBSD50_ENV 1
#define AFS_X86_FBSD60_ENV 1 /* added at 70--ie, some changes should port <-- */
#define AFS_X86_FBSD62_ENV 1
#define AFS_X86_FBSD70_ENV 1
#define AFS_X86_FBSD71_ENV 1
#define AFS_X86_FBSD72_ENV 1
#define AFS_X86_FBSD73_ENV 1
#define AFS_X86_FBSD80_ENV 1
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
#define AFS_X86_FBSD110_ENV 1
#define AFS_X86_FBSD111_ENV 1
#define AFS_X86_FBSD112_ENV 1

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */

#define AFS_USR_FBSD50_ENV 1
#define AFS_USR_FBSD51_ENV 1
#define AFS_USR_FBSD52_ENV 1
#define AFS_USR_FBSD53_ENV 1
#define AFS_USR_FBSD60_ENV 1
#define AFS_USR_FBSD61_ENV 1
#define AFS_USR_FBSD70_ENV 1
#define AFS_USR_FBSD71_ENV 1
#define AFS_USR_FBSD72_ENV 1
#define AFS_USR_FBSD73_ENV 1
#define AFS_USR_FBSD80_ENV 1
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
#define AFS_USR_FBSD110_ENV 1
#define AFS_USR_FBSD111_ENV 1
#define AFS_USR_FBSD112_ENV 1

#endif /* !defined(UKERNEL) */

#endif /* AFS_PARAM_H */
