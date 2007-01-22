/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_COMMON_PROC_H
#define _OSI_OSI_COMMON_PROC_H 1


#define osi_proc_datamodel_int() 32
#define osi_proc_datamodel_long() ((osi_proc_datamodel() == OSI_DATAMODEL_LP64)?64:32)
#define osi_proc_datamodel_pointer() ((osi_proc_datamodel() == OSI_DATAMODEL_ILP32)?32:64)

#define osi_proc_current_id() (osi_proc_id(osi_proc_current()))

#ifdef KERNEL
#if defined(AFS_SUN5_ENV)
#define osi_proc_current_suser() (afs_suser(CRED()))
#else /* !AFS_SUN5_ENV */
#define osi_proc_current_suser() (afs_suser(osi_NULL))
#endif /* !AFS_SUN5_ENV */
#endif /* KERNEL */


#endif /* _OSI_OSI_COMMON_PROC_H */
