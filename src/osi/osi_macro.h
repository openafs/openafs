/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_MACRO_H
#define _OSI_OSI_MACRO_H 1


/*
 * osi common macro package
 * support functionality
 */

#define osi_Macro_Begin \
    do {

#define osi_Macro_End \
    } while (0)


#define osi_Macro_ToString(x) #x

#endif /* _OSI_OSI_MACRO_H */
