/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi time interface
 * solaris userspace time
 * approximate time
 */

#include <osi/osi_impl.h>
#include <osi/osi_time.h>
#include <osi/SOLARIS/utime_approx.h>
#include <osi/osi_mutex.h>


#if defined(OSI_PTHREAD_ENV) && (defined(__sparcv8plus) || defined(__sparcv9) || defined(__amd64))

/*
 * WARNING
 * this implementation makes certain ISA assumptions.  if you attempt
 * to build on the wrong platform, 64-bit load/store atomicity assumptions
 * go away, and this algorithm becomes prone to races.
 *
 * if you really must build libosi on ia32 or prehistoric sparc,
 * then you'll be using the inline implementation of osi_time_approx_get()
 * from src/osi/SOLARIS/utime_approx_inline.h
 */


struct {
    hrtime_t osi_volatile last_update;
    osi_time_t osi_volatile now;
    osi_mutex_t update_lock;
} osi_time_approx_state;

/*
 * the theory behind this implementation is that it is
 * advantageous to deal with race conditions by blocking
 * all but one racing thread in a mutex, thus allowing
 * only one thread to call the time syscall.  On a system
 * where the number of threads >> number of cpus, this
 * is especially beneficial.
 */
osi_result
osi_time_approx_get(osi_time_t * ts_out,
		    osi_time_suseconds_t samp_interval)
{
    osi_result res = OSI_OK;
    osi_time_t ts;
    hrtime_t now;

    /* gethrtime() is a fast-trap syscall on solaris.
     * it is several orders of magnitude faster than time() */
    now = gethrtime();
    if ((now - osi_time_approx_state.last_update) > samp_interval) {
	osi_mutex_Lock(&osi_time_approx_state.update_lock);
	if ((now - osi_time_approx_state.last_update) > samp_interval) {
	    res = osi_time_get(&osi_time_approx_state.now);

	    /* if possible, enforce ordering of stores */
#if defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR_ORDER_STORES)
	    osi_atomic_membar_order_stores();
#endif
	    osi_time_approx_state.last_update = now;
	}
	osi_mutex_Unlock(&osi_time_approx_state.update_lock);
    }

    *ts_out = osi_time_approx_state.now;

    return res;
}

osi_result
osi_time_approx_PkgInit(void)
{
    osi_result res;
    osi_mutex_options_t opts;

    osi_mutex_options_Init(&opts);
    osi_mutex_options_Set(&opts,
			  OSI_MUTEX_OPTION_PREEMPTIVE_ONLY,
			  1);
    osi_mutex_Init(&osi_time_approx_state.update_lock, &opts);
    osi_mutex_options_Destroy(&opts);

    res = osi_time_get(&osi_time_approx_state.now);
    osi_time_approx_state.last_update = gethrtime();

    return res;
}

osi_result
osi_time_approx_PkgShutdown(void)
{
    osi_mutex_Destroy(&osi_time_approx_state.update_lock);

    return OSI_OK;
}

#else /* !OSI_PTHREAD_ENV || !__sparcv8plus && !__sparcv9 && !__amd64 */

osi_result
osi_time_approx_PkgInit(void)
{
    return OSI_OK;
}

osi_result
osi_time_approx_PkgShutdown(void)
{
    return OSI_OK;
}

#endif /* !__sparcv8plus && !__sparcv9 && !__amd64 */
