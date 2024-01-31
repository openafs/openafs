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

/* ct.c */
int ct_memcmp(const void *p1, const void *p2, size_t len);

#endif /* OPENAFS_HCRYPTO_KERNEL_ROKEN_H */
