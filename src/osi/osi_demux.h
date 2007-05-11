/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_DEMUX_H
#define _OSI_OSI_DEMUX_H 1


/*
 * platform-independent demultiplexer abstraction API
 * 
 * description:
 *   Demultiplexers in OSI are used to support function pointer call
 *   tables.  The abstraction supports having one (or more) function
 *   pointers called for a particular action code.
 *
 * types:
 *
 *   osi_demux_t
 *     -- demux object type
 *
 *   osi_demux_action_func_t
 *     -- demux action function type
 *
 *   osi_demux_action_options_t
 *     -- demux action object options type
 *
 *   osi_demux_options_t
 *     -- demux object options type
 *
 *
 * macros:
 *
 *  the following macro interfaces aid in declaring and prototyping action functions:
 *  [see comment in src/osi/COMMON/demux.h for more usage information]
 *
 *   OSI_DEMUX_ACTION_DECL(sym)
 *     -- declare an action function with name $sym$
 *
 *   OSI_DEMUX_ACTION_PROTOTYPE(sym)
 *     -- emit a prototype for an action function of name $sym$
 *
 *   OSI_DEMUX_ACTION_STATIC_DECL(sym)
 *     -- declare a static action function with name $sym$
 *
 *   OSI_DEMUX_ACTION_STATIC_PROTOTYPE(sym)
 *     -- emit a prototype for a static action function of name $sym$
 *
 *  the following macros provide a portable means of accessing action function arguments:
 *  [see comment in src/osi/COMMON/demux.h for more usage information]
 *
 *   OSI_DEMUX_ACTION_ARG_ACTION
 *     -- reference action id passed into osi_demux_call inside an action function
 *
 *   OSI_DEMUX_ACTION_ARG_ROCK_CALLER
 *     -- reference the rock passed into osi_demux_call inside an action function
 *
 *   OSI_DEMUX_ACTION_ARG_ROCK_ACTION
 *     -- reference the rock passed into osi_demux_register_op inside an action function
 *
 *   OSI_DEMUX_ACTION_ARG_ROCK_DEMUX
 *     -- reference the rock passed into osi_demux_register inside an action function
 *
 *
 * interface:
 *
 *  osi_result
 *  osi_demux_create(osi_demux_t **,
 *                   void * demux_rock,
 *                   osi_demux_options_t *);
 *    -- allocate and initialize a demux object
 *
 *  osi_result
 *  osi_demux_destroy(osi_demux_t *);
 *    -- destroy and deallocate a demux object
 *
 *  osi_result
 *  osi_demux_register(osi_demux_t *,
 *                     osi_uint32 action,
 *                     void * action_rock,
 *                     osi_demux_action_func_t * action_fp,
 *                     osi_demux_action_options_t *);
 *    -- register an action with a demux object
 *
 *  osi_result
 *  osi_demux_unregister(osi_demux_t *,
 *                       osi_uint32 action,
 *                       void * action_rock,
 *                       osi_demux_action_func_t * action_fp);
 *    -- unregister an action from a demux object
 *
 *  osi_result
 *  osi_demux_call(osi_demux_t *,
 *                 osi_uint32 action,
 *                 void * call_rock);
 *    -- fire all registered actions for the passed action code
 *
 *
 * options interface:
 *  
 *  void
 *  osi_demux_options_Init(osi_demux_options_t *);
 *    -- initialize a demux options object
 *
 *  void
 *  osi_demux_options_Destroy(osi_demux_options_t *)
 *    -- destroy a demux options object
 *
 *  osi_result
 *  osi_demux_options_Set(osi_demux_options_t *,
 *                        osi_demux_options_param_t,
 *                        osi_options_val_t *);
 *    -- set an attribute in a demux options object
 *
 *
 * action options interface:
 *  
 *  void
 *  osi_demux_action_options_Init(osi_demux_action_options_t *);
 *    -- initialize a demux action options object
 *
 *  void
 *  osi_demux_action_options_Destroy(osi_demux_action_options_t *)
 *    -- destroy a demux action options object
 *
 *  osi_result
 *  osi_demux_action_options_Set(osi_demux_action_options_t *,
 *                               osi_demux_action_options_param_t,
 *                               osi_options_val_t *);
 *    -- set an attribute in a demux action options object
 *
 */


#include <osi/COMMON/demux_options.h>
#include <osi/COMMON/demux.h>


#endif /* _OSI_OSI_DEMUX_H */
