/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_DEMUX_TYPES_H
#define _OSI_COMMON_DEMUX_TYPES_H 1

/*
 * osi
 * demultiplexer abstraction
 * public type definitions
 */

typedef osi_result osi_demux_action_func_t(osi_uint32 action,
					   void * caller_rock,
					   void * action_rock,
					   void * demux_rock);

#endif /* _OSI_COMMON_DEMUX_TYPES_H */
