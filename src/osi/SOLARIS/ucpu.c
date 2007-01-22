/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_cpu.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/processor.h>
#include <sys/procset.h>


osi_result
osi_cpu_count(osi_int32 * count)
{
    long res = sysconf(_SC_NPROCESSORS_ONLN);
    if (res < 1) {
	return OSI_FAIL;
    }
    *count = (osi_int32) res;
    return OSI_OK;
}

osi_result
osi_cpu_min_id(osi_cpu_id_t * id)
{
    *id = 0;
    return OSI_OK;
}

osi_result
osi_cpu_max_id(osi_cpu_id_t * id)
{
    long res = sysconf(_SC_CPUID_MAX);
    if (res < 1) {
	return OSI_FAIL;
    }
    *id = (osi_cpu_id_t) res;
    return OSI_OK;
}

osi_result
osi_cpu_list_iterate(osi_cpu_iterator_t * fp, void * sdata)
{
    osi_result res = OSI_OK;
    osi_cpu_id_t i, max;

    res = osi_cpu_max_id(&max);
    if (OSI_RESULT_FAIL(res)) {
	return res;
    }

    for (i = 0; i <= max; i++) {
	if (p_online(i, P_STATUS) != -1) {
	    res = (*fp)(i, sdata);
	    if (OSI_RESULT_FAIL(res) ||
		(res == OSI_CPU_RESULT_ITERATOR_STOP)) {
		break;
	    }
	}
    }

    return res;
}


/*
 * thread binding code
 */

osi_result
osi_cpu_bind_thread_current(osi_cpu_id_t cpuid)
{
    osi_result res = OSI_OK;
    osi_cpu_id_t oldid;

    if (processor_bind(P_LWPID, P_MYID, cpuid, &oldid) < 0) {
	res = OSI_FAIL;
    }

    return res;
}

osi_result
osi_cpu_unbind_thread_current(void)
{
    osi_result res = OSI_OK;
    osi_cpu_id_t oldid;

    if (processor_bind(P_LWPID, P_MYID, PBIND_NONE, &oldid) < 0) {
	res = OSI_FAIL;
    }

    return res;
}

