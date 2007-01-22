/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_OBJECT_INIT_H
#define _OSI_OSI_OBJECT_INIT_H 1


/*
 * platform-independent osi_object_init API
 *
 *  osi_object_init_t -- initialization control
 *  osi_object_init_func_t -- initialization function type
 *
 *  OSI_OBJECT_INIT_DECL(symbol name, osi_object_init_func_t *)
 *    -- declare a osi_object_init_t
 *
 *  void osi_object_init(osi_object_init_t *);
 *    -- try to initialize the object exactly once without incurring 
 *       synchronization overhead for the common case
 *
 *  int osi_object_init_check(osi_object_init_t *);
 *    -- check to see whether object was previously initialized
 *
 *  void osi_object_init_assert(osi_object_init_t *);
 *    -- assert that object was previously initialized
 *
 */


#include <osi/COMMON/object_init.h>

#endif /* _OSI_OSI_OBJECT_INIT_H */
