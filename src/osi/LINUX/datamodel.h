/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_DATAMODEL_H
#define	_OSI_LINUX_DATAMODEL_H


#if (defined(AFS_LINUX_64BIT_KERNEL) && defined(OSI_KERNELSPACE_ENV)) || defined(__osi_arch_alpha) || defined(__osi_arch_ia64) || defined(__osi_arch_amd64) || defined(__osi_arch_ppc64) || defined(__osi_arch_s390x) || defined(__osi_arch_mips64) || defined(__osi_arch_sparc64)
#define OSI_DATAMODEL OSI_LP64_ENV
#define osi_datamodel() OSI_DATAMODEL_LP64
#define osi_datamodel_int() 32
#define osi_datamodel_long() 64
#define osi_datamodel_pointer() 64
#else
#define OSI_DATAMODEL OSI_ILP32_ENV
#define osi_datamodel() OSI_DATAMODEL_ILP32
#define osi_datamodel_int() 32
#define osi_datamodel_long() 32
#define osi_datamodel_pointer() 32
#endif

#endif /* _OSI_LINUX_DATAMODEL_H */
