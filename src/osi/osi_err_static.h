/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_ERR_STATIC_H
#define _OSI_OSI_ERR_STATIC_H 1

#if defined(OSI_ERROR_UNSPECIFIED) || !defined(OSI_ENV_COMERR_BUILD) || !defined(_OSI_OSI_INCLUDES_H)
#error "this file is strictly for comerr bootstrapping!  do not include this file directly!"
#endif

/*
 * osi abstraction
 * static error code definitions for comerr bootstrap
 *
 * yes, this is an ugly hack
 */

#define OSI_ERROR_UNSPECIFIED                    (1026717440L)
#define OSI_ERROR_UNIMPLEMENTED                  (1026717441L)
#define OSI_ERROR_NOMEM                          (1026717442L)
#define OSI_ERROR_NOSYS                          (1026717443L)
#define OSI_ERROR_REQUEST_QUEUED                 (1026717444L)
#define OSI_ERROR_OBJECT_REFERENCED              (1026717445L)
#define OSI_CACHE_ERROR_FAKED                    (1026717446L)
#define OSI_CACHE_ERROR_EXISTS                   (1026717447L)
#define OSI_CACHE_ERROR_ABSENT                   (1026717448L)
#define OSI_CACHE_ERROR_FAIL_PROBE               (1026717449L)
#define OSI_CPU_ERROR_ITERATOR_STOP              (1026717450L)
#define OSI_MEM_ERROR_FAKED                      (1026717451L)


#endif /* _OSI_OSI_ERR_STATIC_H */
