/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_KERNEL_H
#define	_OSI_LINUX_KERNEL_H

#define osi_kernel_printf   printk
#define osi_kernel_uprintf  printk

#define OSI_IMPLEMENTS_KERNEL_PREEMPTION_CONTROL 1

#define osi_kernel_preemption_disable()   get_cpu()
#define osi_kernel_preemption_enable()    put_cpu()

#endif /* _OSI_LINUX_KERNEL_H */
