/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_LIB_INIT_H
#define _OSI_OSI_LIB_INIT_H 1

/*
 * osi 
 * library initialization support
 *
 * please note that you may only have one
 * initialization function per compilation unit
 */

#if defined(__osi_env_sunpro)

#pragma init(__osi_lib_obj_ctor)

#elif defined(__osi_env_hp_c)

#pragma INIT "__osi_lib_obj_ctor"

#endif


#endif /* _OSI_OSI_LIB_INIT_H */
