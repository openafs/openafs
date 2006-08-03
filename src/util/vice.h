/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_VICE_H
#define AFS_VICE_H 1

#include <sys/types.h>
#include <sys/ioctl.h>
#if defined(__sun) && defined(__SVR4)
# include <sys/ioccom.h>
#endif

/*
 * Structures used to pass data into AFS ioctls.  The buffer pointers may be
 * null, or the counts may be 0 if there are no input or output parameters.
 */

#if defined(KERNEL)
/* A fixed size, whether the kernel uses the ILP32 or LP64 data models. */
struct ViceIoctl32 {
    unsigned int in, out;	/* Data to be transferred in, or out */
    short in_size;		/* Size of input buffer <= 2K */
    short out_size;		/* Maximum size of output buffer, <= 2K */
};
#endif

/* Windows uses a different structure layout, defined in sys/pioctl_nt.h. */
#ifndef _WIN32_WINNT
struct ViceIoctl {
    caddr_t in, out;		/* Data to be transferred in, or out */
    short in_size;		/* Size of input buffer <= 2K */
    short out_size;		/* Maximum size of output buffer, <= 2K */
};
#endif

/*
 * The structure argument to _IOW() is used to add a structure size component
 * to the _IOW() value, to help the kernel code identify how big a structure
 * the calling process is passing into a system call.
 *
 * In user space, struct ViceIoctl32 and struct ViceIoctl are the same except
 * on Digital Unix, where user space code is compiled in 64-bit mode.
 *
 * So, in kernel space, regardless whether it is compiled in 32-bit mode or
 * 64-bit mode, the kernel code can use the struct ViceIoctl32 version of
 * _IOW() to check the size of user space arguments -- except on Digital Unix
 * (or Linux on Alpha, since it's compatible with Digital Unix).  We don't
 * care about other 64-bit Linux versions since they don't check this length.
 */
#if defined(KERNEL) && !defined(__alpha__)
#define _VICEIOCTL(id) ((unsigned int) _IOW('V', id, struct ViceIoctl32))
#define _VICEIOCTL2(dev, id) ((unsigned int) _IOW(dev, id, struct ViceIoctl32))
#else
#define _VICEIOCTL(id) ((unsigned int) _IOW('V', id, struct ViceIoctl))
#define _VICEIOCTL2(dev, id) ((unsigned int) _IOW(dev, id, struct ViceIoctl))
#endif
#define _CVICEIOCTL(id) _VICEIOCTL2('C', id)
#define _OVICEIOCTL(id) _VICEIOCTL2('O', id)

/*
 * Use these macros to define up to 256 vice ioctls in each space.  These
 * ioctls all potentially have in/out parameters, depending on the values in
 * the ViceIoctl structure.  This structure is passed into the kernel by the
 * normal ioctl parameter passing mechanism.
 *
 * See <http://grand.central.org/numbers/pioctls.html> for the definition of
 * the namespaces.
 */

#endif /* AFS_VICE_H */
