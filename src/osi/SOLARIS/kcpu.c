/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */


#include <osi/osi_impl.h>
#include <osi/osi_cpu.h>
#include <osi/osi_kernel.h>
#include <osi/osi_mutex.h>
#include <sys/types.h>
#include <sys/processor.h>
#include <sys/procset.h>
#include <sys/cpuvar.h>


/*
 * basic cpu introspection
 */

osi_result
osi_cpu_count(osi_int32 * val)
{
    extern int ncpus_online;
    mutex_enter(&cpu_lock);
    *val = (osi_int32)ncpus_online;
    mutex_exit(&cpu_lock);
    return OSI_OK;
}

osi_result
osi_cpu_min_id(osi_cpu_id_t * val)
{
    *val = (osi_cpu_id_t)0;
    return OSI_OK;
}

osi_result
osi_cpu_max_id(osi_cpu_id_t * val)
{
    extern processorid_t max_cpuid;
    *val = (osi_cpu_id_t)max_cpuid;
    return OSI_OK;
}

osi_result
osi_cpu_list_iterate(osi_cpu_iterator_t * fp, void * sdata)
{
    cpu_t * cp;
    osi_result res;
    extern cpu_t * cpu_active;

    /* disabling kernel preemption is required in order to 
     * make cpu list walking safe */
    osi_kernel_preemption_disable();

    /* setup cpu0 */
    cp = cpu_active;
    res = (*fp)(cp->cpu_id, sdata);
    if (OSI_RESULT_FAIL(res) ||
	(res == OSI_CPU_RESULT_ITERATOR_STOP)) {
	goto error;
    }

    /* 
     * setup any additional cpus
     *
     * cpu_t's in the solaris kernel are
     * stored as a circularly linked list;
     * spin around until we hit cpu0 again
     */
    for (cp = cp->cpu_next_onln; 
	 cp != cpu_active; 
	 cp = cp->cpu_next_onln) {
	res = (*fp)(cp->cpu_id, sdata);
	if (OSI_RESULT_FAIL(res) ||
	    (res == OSI_CPU_RESULT_ITERATOR_STOP)) {
	    goto error;
	}
    }

 error:
    osi_kernel_preemption_enable();
    return res;
}


/*
 * thread binding code
 */

/* uh, hopefully this kernel interface has been stable for a long time */
osi_extern int processor_bind(idtype_t, id_t, processorid_t, processorid_t *);

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

/*
 * hot-plug cpu monitoring code
 * (this stuff is needed to make per-cpu local memory safe)
 */

#define OSI_CPU_INIT_MAGIC 0x12DEED21
#define OSI_CPU_MAX_MONITORS 20

osi_static struct {
    osi_uint32 initialized;
    struct {
	osi_cpu_monitor_t * fp;
	void * arg;
    } monitors[OSI_CPU_MAX_MONITORS];
    osi_mutex_t lock;
} osi_cpu_monitor_state = { 0 };

osi_static osi_result
osi_cpu_event_map(cpu_setup_t event, osi_cpu_event_t * osi_ev)
{
    osi_result res = OSI_OK;
    switch(event) {
    case CPU_INIT:
	*osi_ev = OSI_CPU_EVENT_INIT;
	break;
    case CPU_CONFIG:
	*osi_ev = OSI_CPU_EVENT_CONFIG;
	break;
    case CPU_UNCONFIG:
	*osi_ev = OSI_CPU_EVENT_UNCONFIG;
	break;
    case CPU_ON:
	*osi_ev = OSI_CPU_EVENT_ONLINE;
	break;
    case CPU_OFF:
	*osi_ev = OSI_CPU_EVENT_OFFLINE;
	break;
    case CPU_CPUPART_IN:
	*osi_ev = OSI_CPU_EVENT_PARTITION_JOIN;
	break;
    case CPU_CPUPART_OUT:
	*osi_ev = OSI_CPU_EVENT_PARTITION_LEAVE;
	break;
    default:
	res = OSI_FAIL;
    }
    return res;
}

/*
 * this function gets registered in the kernel cpu event notify system
 *
 * it calls all of the registered osi event handlers in turn
 */
osi_static int
osi_cpu_state_monitor(cpu_setup_t event, int cpuid, void * arg)
{
    int i;
    osi_cpu_event_t osi_ev;
    osi_Assert(osi_cpu_monitor_state.initialized == OSI_CPU_INIT_MAGIC);

    if (OSI_RESULT_FAIL(osi_cpu_event_map(event, &osi_ev))) {
	return -1;
    }

    /* this code should only be called with all other cpus halted
     * so grabbing the mutex would be very dangerous
     *
     * safety in update is assured because of the locking/preemption
     * protocol in the registration function */

    for (i = 0; i < OSI_CPU_MAX_MONITORS; i++) {
	if (osi_cpu_monitor_state.monitors[i].fp) {
	    (*osi_cpu_monitor_state.monitors[i].fp)((osi_cpu_id_t)cpuid,
						    osi_ev,
						    osi_cpu_monitor_state.monitors[i].arg);
	}
    }

    return 0;
}

osi_result
osi_cpu_monitor_register(osi_cpu_monitor_t * fp, void * arg)
{
    int i;
    osi_result res = OSI_OK;

    osi_mutex_Lock(&osi_cpu_monitor_state.lock);
    osi_kernel_preemption_disable();

    for (i = 0; i < OSI_CPU_MAX_MONITORS; i++) {
	if (osi_cpu_monitor_state.monitors[i].fp == osi_NULL) {
	    osi_cpu_monitor_state.monitors[i].fp = fp;
	    osi_cpu_monitor_state.monitors[i].arg = arg;
	    break;
	}
    }

    if (i == OSI_CPU_MAX_MONITORS) {
	res = OSI_FAIL;
    }

    /* ordering is very important; this causes
     * a membar before preemption is re-enabled */
    osi_mutex_Unlock(&osi_cpu_monitor_state.lock);
    osi_kernel_preemption_enable();

    return res;
}

osi_result
osi_cpu_monitor_unregister(osi_cpu_monitor_t * fp, void * arg)
{
    int i;
    osi_result res = OSI_OK;

    osi_mutex_Lock(&osi_cpu_monitor_state.lock);
    osi_kernel_preemption_disable();

    for (i = 0; i < OSI_CPU_MAX_MONITORS; i++) {
	if (osi_cpu_monitor_state.monitors[i].fp == fp &&
	    osi_cpu_monitor_state.monitors[i].arg == arg) {
	    osi_cpu_monitor_state.monitors[i].fp = osi_NULL;
	    osi_cpu_monitor_state.monitors[i].arg = osi_NULL;
	    break;
	}
    }

    if (i == OSI_CPU_MAX_MONITORS) {
	res = OSI_FAIL;
    }

    /* ordering is very important; this causes
     * a membar before preemption is re-enabled */
    osi_mutex_Unlock(&osi_cpu_monitor_state.lock);
    osi_kernel_preemption_enable();

    return res;
}


OSI_INIT_FUNC_DECL(osi_cpu_PkgInit)
{
    int i;

    osi_Assert(osi_cpu_monitor_state.initialized != OSI_CPU_INIT_MAGIC);

    osi_mutex_Init(&osi_cpu_monitor_state.lock, NULL);

    osi_mutex_Lock(&osi_cpu_monitor_state.lock);
    osi_cpu_monitor_state.initialized = OSI_CPU_INIT_MAGIC;
    for (i = 0; i < OSI_CPU_MAX_MONITORS; i++) {
	osi_cpu_monitor_state.monitors[i].fp = osi_NULL;
	osi_cpu_monitor_state.monitors[i].arg = osi_NULL;
    }
    osi_mutex_Unlock(&osi_cpu_monitor_state.lock);

    mutex_enter(&cpu_lock);
    register_cpu_setup_func(&osi_cpu_state_monitor, NULL);
    mutex_exit(&cpu_lock);

    return OSI_OK;
}

OSI_FINI_FUNC_DECL(osi_cpu_PkgShutdown)
{
    osi_Assert(osi_cpu_monitor_state.initialized == OSI_CPU_INIT_MAGIC);

    mutex_enter(&cpu_lock);
    unregister_cpu_setup_func(&osi_cpu_state_monitor, NULL);
    mutex_exit(&cpu_lock);

    return OSI_OK;
}
