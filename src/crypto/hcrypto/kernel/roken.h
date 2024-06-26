#ifndef OPENAFS_HCRYPTO_KERNEL_ROKEN_H
#define OPENAFS_HCRYPTO_KERNEL_ROKEN_H

/*
 * This is a stub roken.h used for building roken code (or roken-using code) in
 * the kernel. For userspace code, use a real roken.h. This just contains a few
 * prototypes of roken functions we actually use in kernel code.
 */

#ifndef KERNEL
# error "This header is for kernel code only"
#endif

/*
 * The following function annotations are not needed when building for kernel
 * space
 */
#define ROKEN_LIB_FUNCTION
#define ROKEN_LIB_CALL
#define ROKEN_LIB_VARIABLE

/*
 * Our HAVE_STRLCPY et al from autoconf refer to whether, e.g., strlcpy() is
 * available in userspace. Whether it's available in the kernel is another
 * question, so set them again here. Usually they are available (they are
 * missing only for a few cases) so define them by default, and
 * undefine them for a few cases below.
 */
#undef HAVE_STRLCPY
#define HAVE_STRLCPY 1
#undef HAVE_STRLCAT
#define HAVE_STRLCAT 1

#ifdef AFS_AIX_ENV
# undef HAVE_STRLCPY
# undef HAVE_STRLCAT
#elif defined(AFS_LINUX_ENV) && defined(HAVE_LINUX_NO_STRLCPY)
# undef HAVE_STRLCPY
#endif

/* strlcpy.c */
#ifndef HAVE_STRLCPY
# define strlcpy rk_strlcpy
ROKEN_LIB_FUNCTION size_t ROKEN_LIB_CALL strlcpy (char *, const char *, size_t);
#endif

/* strlcat.c */
#ifndef HAVE_STRLCAT
# define strlcat rk_strlcat
ROKEN_LIB_FUNCTION size_t ROKEN_LIB_CALL strlcat (char *, const char *, size_t);
#endif

/* ct.c */
int ct_memcmp(const void *p1, const void *p2, size_t len);

#endif /* OPENAFS_HCRYPTO_KERNEL_ROKEN_H */
