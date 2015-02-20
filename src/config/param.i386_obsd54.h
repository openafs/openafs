/*
 * Thanks to Jim Rees and University of Michigan CITI, for the initial
 * OpenBSD porting work.
 */

#ifndef	AFS_I386_PARAM_H
#define	AFS_I386_PARAM_H

#define SYS_NAME		"i386_obsd54"
#define SYS_NAME_ID		SYS_NAME_ID_i386_obsd54

#define AFS_X86_XBSD_ENV	1
#define AFS_X86_ENV	        1
#define AFSLITTLE_ENDIAN	1

#ifdef _KERNEL
void bcopy(const void *, void *, size_t);

static inline void *memmove(void *dst, const void *src, size_t len) {
    bcopy(src, dst, len);
    return(dst);
}
#endif

#endif /* AFS_I386_PARAM_H */
