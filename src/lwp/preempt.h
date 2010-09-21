/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*******************************************************************\
* 								    *
* 	Information Technology Center				    *
* 	Carnegie-Mellon University				    *
* 								    *
\*******************************************************************/

#if defined( _WIN32)
#define PRE_InitPreempt(A)
#endif

#if defined( _WIN32) || defined(AFS_LINUX20_ENV)
/* preemption not implemented for win32. Use threads instead. */
#define PRE_PreemptMe()
#define PRE_BeginCritical()
#define PRE_EndCritical()
#else
#define PRE_PreemptMe()		lwp_cpptr->level = 0
#define PRE_BeginCritical()	lwp_cpptr->level++
#define PRE_EndCritical()	lwp_cpptr->level--
#endif

#define DEFAULTSLICE	10
