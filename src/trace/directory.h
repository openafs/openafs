/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_DIRECTORY_H
#define _OSI_TRACE_DIRECTORY_H 1


/*
 * osi tracing framework
 * probe point directory
 */

/*
 * maximum allowed probe name length
 */
#define OSI_TRACE_MAX_PROBE_NAME_LEN 256

/*
 * maximum allowed probe hierarchy depth
 */
#define OSI_TRACE_MAX_PROBE_TREE_DEPTH 16


/* initialization routines */
osi_extern osi_result osi_trace_directory_PkgInit(void);
osi_extern osi_result osi_trace_directory_PkgShutdown(void);

#endif /* _OSI_TRACE_DIRECTORY_H */
