/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_INCLUDES_H
#define _OSI_OSI_INCLUDES_H 1


/*
 * osi abstraction
 * osi includes for source files that won't accept osi.h
 */

#include <afs/param.h>
#include <osi/osi_macro.h>
#if !defined(OSI_ENV_COMERR_BUILD)
#include <osi/osi_err.h>
#else /* OSI_ENV_COMERR_BUILD */
#include <osi/osi_err_static.h>
#endif /* OSI_ENV_COMERR_BUILD */
#include <osi/osi_compiler.h>
#include <osi/osi_buildenv.h>
#include <osi/osi_datamodel.h>
#include <osi/osi_types.h>
#include <osi/osi_inline.h>
#include <osi/osi_options.h>
#include <osi/osi_prototypes.h>
#include <osi/osi_config.h>
#include <osi/osi_util.h>

#endif /* _OSI_OSI_INCLUDES_H */
