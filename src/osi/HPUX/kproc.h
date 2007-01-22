/*
 * Copyright 2005, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_HPUX_KPROC_H
#define	_OSI_HPUX_KPROC_H

#define OSI_IMPLEMENTS_PROC 1

typedef struct proc * osi_proc_p;
typedef struct proc osi_proc_t;
typedef pid_t osi_proc_id_t;


extern osi_datamodel_t osi_proc_datamodel(void);

#define osi_proc_current() (u.u_procp)
#define osi_proc_id(x) ((x)->p_pid)

#endif /* _OSI_HPUX_KPROC_H */
