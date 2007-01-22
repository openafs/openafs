/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_KPROC_H
#define	_OSI_SOLARIS_KPROC_H

#define OSI_IMPLEMENTS_PROC 1

#include <sys/proc.h>

typedef proc_t * osi_proc_p;
typedef proc_t osi_proc_t;
typedef pid_t osi_proc_id_t;


osi_extern osi_datamodel_t osi_proc_datamodel(void);

#define osi_proc_current() (curproc)
#define osi_proc_id(x) ((x)->p_pid)
#define osi_proc_current_id() (osi_proc_id(osi_proc_current()))

#endif /* _OSI_SOLARIS_KPROC_H */
