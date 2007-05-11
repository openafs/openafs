/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_STATE_H
#define _OSI_STATE_H 1


/*
 * osi abstraction
 * runtime state information
 *
 *  osi_State_t osi_state_current();
 *    -- returns the current libosi state
 *
 */

#include <osi/osi_options.h>

typedef enum {
    OSI_STATE_INVALID,
    OSI_STATE_UNINITIALIZED,    /* osi_PkgInit has not been called yet */
    OSI_STATE_ONLINE,           /* osi_PkgInit returned success */
    OSI_STATE_SHUTDOWN,         /* osi_PkgShutdown returned success */
    OSI_STATE_INITIALIZING,     /* osi_PkgInit is currently running */
    OSI_STATE_SHUTTING_DOWN,    /* osi_PkgShutdown is currently running */
    OSI_STATE_FORKING,          /* we are a child of a fork, and PkgInitChild hasn't been called yet */
    OSI_STATE_ERROR,            /* a PkgInit or PkgShutdown call failed */
    OSI_STATE_MAX_ID
} osi_State_t;

struct osi_state {
    osi_State_t state;
};
osi_extern struct osi_state osi_state;

#define osi_state_current() (osi_state.state)

#endif /* _OSI_STATE_H */
