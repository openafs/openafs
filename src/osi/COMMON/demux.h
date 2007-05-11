/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_DEMUX_H
#define _OSI_COMMON_DEMUX_H 1

/*
 * osi
 * demultiplexer abstraction
 * internal type definitions
 */

#include <osi/COMMON/demux_types.h>

/* forward declare */
struct osi_demux;
typedef struct osi_demux osi_demux_t;


osi_extern osi_result
osi_demux_create(osi_demux_t **, 
		 void * demux_rock, 
		 osi_demux_options_t *);
osi_extern osi_result
osi_demux_destroy(osi_demux_t *);

osi_extern osi_result
osi_demux_action_register(osi_demux_t *, 
			  osi_uint32 action, 
			  void * action_rock, 
			  osi_demux_action_func_t *,
			  osi_demux_action_options_t *);
osi_extern osi_result
osi_demux_action_unregister(osi_demux_t *, 
			    osi_uint32 action,
			    void * action_rock,
			    osi_demux_action_func_t *);

osi_extern osi_result
osi_demux_call(osi_demux_t *,
	       osi_uint32 action,
	       void * caller_rock);


osi_extern osi_result osi_demux_PkgInit(void);
osi_extern osi_result osi_demux_PkgShutdown(void);


/*
 * action function automated declarations and prototypes
 *
 * to provide a small degree of future-proofing for the demux interface, 
 * we strongly encourage the use of these macros.
 *
 * example use for global symbol demux function
 *
 * h file:
 *
 *   OSI_DEMUX_ACTION_PROTOTYPE(osi_example_action_foo);
 *
 * c file:
 *
 *   OSI_DEMUX_ACTION_DECL(osi_example_action_foo) {
 *       (osi_Msg "action foo called with the following args: action = %u, caller rock = %p, action rock = %p, demux rock = %p\n",
 *        OSI_DEMUX_ACTION_ARG_ACTION, 
 *        OSI_DEMUX_ACTION_ARG_ROCK_CALLER, 
 *        OSI_DEMUX_ACTION_ARG_ROCK_ACTION, 
 *        OSI_DEMUX_ACTION_ARG_ROCK_DEMUX);
 *       return OSI_OK;
 *   }
 *
 */

#define OSI_DEMUX_ACTION_ARG_ACTION action
#define OSI_DEMUX_ACTION_ARG_ROCK_CALLER rock_caller
#define OSI_DEMUX_ACTION_ARG_ROCK_ACTION rock_action
#define OSI_DEMUX_ACTION_ARG_ROCK_DEMUX rock_demux

#define OSI_DEMUX_ACTION_DECL(sym) \
    osi_result sym (\
        osi_uint32 OSI_DEMUX_ACTION_ARG_ACTION, \
        void * OSI_DEMUX_ACTION_ARG_ROCK_CALLER, \
        void * OSI_DEMUX_ACTION_ARG_ROCK_ACTION, \
        void * OSI_DEMUX_ACTION_ARG_ROCK_DEMUX )
#define OSI_DEMUX_ACTION_PROTOTYPE(sym) \
    osi_extern osi_result sym (\
        osi_uint32 OSI_DEMUX_ACTION_ARG_ACTION, \
        void * OSI_DEMUX_ACTION_ARG_ROCK_CALLER, \
        void * OSI_DEMUX_ACTION_ARG_ROCK_ACTION, \
        void * OSI_DEMUX_ACTION_ARG_ROCK_DEMUX )

#define OSI_DEMUX_ACTION_STATIC_DECL(sym) \
    osi_static osi_result sym (\
        osi_uint32 OSI_DEMUX_ACTION_ARG_ACTION, \
        void * OSI_DEMUX_ACTION_ARG_ROCK_CALLER, \
        void * OSI_DEMUX_ACTION_ARG_ROCK_ACTION, \
        void * OSI_DEMUX_ACTION_ARG_ROCK_DEMUX )
#define OSI_DEMUX_ACTION_STATIC_PROTOTYPE(sym) \
    osi_static osi_result sym (\
        osi_uint32 OSI_DEMUX_ACTION_ARG_ACTION, \
        void * OSI_DEMUX_ACTION_ARG_ROCK_CALLER, \
        void * OSI_DEMUX_ACTION_ARG_ROCK_ACTION, \
        void * OSI_DEMUX_ACTION_ARG_ROCK_DEMUX )


#endif /* _OSI_COMMON_DEMUX_H */
