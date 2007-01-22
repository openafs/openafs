/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_LIBOSI_OPTIONS_H
#define _OSI_COMMON_LIBOSI_OPTIONS_H 1

osi_extern osi_result osi_options_Init(osi_ProgramType_t, osi_options_t *);
osi_extern osi_result osi_options_Destroy(osi_options_t *);
osi_extern osi_result osi_options_Copy(osi_options_t * dst, osi_options_t * src);
osi_extern osi_result osi_options_Set(osi_options_t *,
				      osi_options_param_t,
				      osi_options_val_t *);
osi_extern osi_result osi_options_Get(osi_options_t *,
				      osi_options_param_t,
				      osi_options_val_t * val_out);


osi_extern osi_options_t osi_options_default_kernel;
osi_extern osi_options_t osi_options_default_ukernel;
osi_extern osi_options_t osi_options_default_daemon;
osi_extern osi_options_t osi_options_default_util;
osi_extern osi_options_t osi_options_default_pam;

#endif /* _OSI_COMMON_LIBOSI_OPTIONS_H */
