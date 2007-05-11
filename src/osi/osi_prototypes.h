/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_PROTOTYPES_H
#define _OSI_OSI_PROTOTYPES_H 1


/*
 * osi abstraction
 * main library prototypes
 */


/*
 * osi library initialization/shutdown routines
 */
osi_extern osi_result osi_PkgInit(osi_ProgramType_t, osi_options_t *);
osi_extern osi_result osi_PkgShutdown(void);
osi_extern osi_result osi_PkgInitChild(osi_ProgramType_t);

OSI_INIT_FUNC_PROTOTYPE(osi_null_init_func);
OSI_FINI_FUNC_PROTOTYPE(osi_null_fini_func);

osi_extern osi_result osi_nullfunc();
osi_extern void osi_Panic();

#endif /* _OSI_OSI_PROTOTYPES_H */
