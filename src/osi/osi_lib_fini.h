/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_LIB_FINI_H
#define _OSI_OSI_LIB_FINI_H 1

/*
 * osi 
 * library finalization support
 *
 * please note that you may only have one
 * finalization function per compilation unit
 */

#if defined(__osi_env_sunpro)

#pragma fini(__osi_lib_obj_dtor)

#elif defined(__osi_env_hp_c)

#pragma FINI "__osi_lib_obj_dtor"

#endif


#endif /* _OSI_OSI_LIB_FINI_H */
