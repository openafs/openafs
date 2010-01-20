/*
 * Copyright 2000, International Business Machines Corporation and others.
 * Copyright 2009, Sine Nomine Associates
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _VOLTRANS_INLINE_H
#define _VOLTRANS_INLINE_H

#include "volser.h"

/**
 * Save the most recent call and procedure name for
 * transaction status reporting.
 *
 * \param tt    transaction object
 * \param call  the rx call object, NULL if none
 * \param name  procedure name, NULL if none
 *
 * \pre VTRANS_OBJ_LOCK on tt must be held
 */
static_inline void
TSetRxCall_r(struct volser_trans *tt, struct rx_call *call, const char *name)
{
    if (name) {
        strlcpy(tt->lastProcName, name, sizeof(tt->lastProcName));
    }
    if (call) {
        tt->rxCallPtr = call;
    }
}

/**
 * Clears the most recent call object.
 *
 * \param tt    transaction object
 *
 * \pre VTRANS_OBJ_LOCK on tt must be held
 */
static_inline void
TClearRxCall_r(struct volser_trans *tt)
{
    tt->rxCallPtr = NULL;
}

/**
 * Save the most recent call and procedure name for
 * transaction status reporting.
 *
 * \param tt    transaction object
 * \param call  the rx call object, NULL if none
 * \param name  procedure name, NULL if none
 *
 * \pre VTRANS_OBJ_LOCK on tt must not be held
 */
static_inline void
TSetRxCall(struct volser_trans *tt, struct rx_call *call, const char *name)
{
    VTRANS_OBJ_LOCK(tt);
    TSetRxCall_r(tt, call, name);
    VTRANS_OBJ_UNLOCK(tt);
}

/**
 * Clears the most recent call object.
 *
 * \param tt    transaction object
 *
 * \pre VTRANS_OBJ_LOCK on tt must not be held
 */
static_inline void
TClearRxCall(struct volser_trans *tt)
{
    VTRANS_OBJ_LOCK(tt);
    TClearRxCall_r(tt);
    VTRANS_OBJ_UNLOCK(tt);
}

#endif /* _VOLTRANS_INLINE_H */
