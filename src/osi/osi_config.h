/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_CONFIG_H
#define _OSI_CONFIG_H 1


/*
 * osi abstraction
 * runtime configuration information
 *
 *  osi_ProgramType_t osi_config_programType();
 *    -- returns the program type
 *
 *  osi_int32 osi_config_epoch()
 *    -- returns the epoch
 *
 *  osi_result osi_config_options_Get(osi_options_param_t param,
 *                                    osi_options_val_t * val_out)
 *    -- store value for options parameter $param$ in $val_out$
 */

#include <osi/osi_options.h>

typedef struct osi_config {
    osi_ProgramType_t programType;
    osi_int32 epoch;
    osi_options_t options;
} osi_config_t;
osi_extern osi_config_t osi_config;


#define osi_config_programType()  (osi_config.programType)
#define osi_config_epoch()        (osi_config.epoch)
#define osi_config_options_Get(param, valp) \
    (osi_options_Get(&osi_config.options, param, valp))

#endif /* _OSI_CONFIG_H */
