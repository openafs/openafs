/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#define ERROR(evalue) do {                                      \
            code = evalue;                                      \
            goto error_exit;                                    \
        } while (0)

#define ABORT(evalue) do {                                      \
            code = evalue;                                      \
            goto abort_exit;                                    \
        } while (0)

#define BUDB_EXIT(evalue) do {                                  \
            osi_audit(BUDB_ExitEvent, evalue, AUD_END);         \
	    exit(evalue);                                       \
        } while (0)
