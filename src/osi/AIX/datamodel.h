/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_AIX_DATAMODEL_H
#define _OSI_AIX_DATAMODEL_H 1


#if defined(__64BIT__)
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

#endif /* _OSI_AIX_DATAMODEL_H */
