/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_CPU_H
#define	_OSI_LEGACY_CPU_H


typedef osi_int32 osi_cpu_id_t;

typedef osi_result osi_cpu_iterator_t(osi_cpu_id_t, void *);
typedef osi_result osi_cpu_monitor_t(osi_cpu_id_t, osi_cpu_event_t, void *);

#define osi_cpu_count(x) ((*(x) = 1), OSI_OK)
#define osi_cpu_min_id(x) ((*(x) = 0), OSI_OK)
#define osi_cpu_max_id(x) ((*(x) = 0), OSI_OK)
#define osi_cpu_list_iterate(fp, sdata) ((*(fp))((osi_cpu_id_t)0, (sdata)))


#define osi_cpu_PkgInit()        (OSI_OK)
#define osi_cpu_PkgShutdown()    (OSI_OK)


#endif /* _OSI_LEGACY_CPU_H */
