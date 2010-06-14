/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implementation of miscellaneous Irix routines.
 */
#include <afsconfig.h>
#include "afs/param.h"


#ifdef	AFS_SGI62_ENV
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */




/*
 * various special purpose routines
 */
void
afs_mpservice(void *a)
{
}

/*!
 * make a semaphore name for use in <sys/sema.h>-type routines on
 * IRIX
 *
 * \param[out]	sname		will contain the semaphore name and
 *				should point to an allocated string
 *				buffer of size METER_NAMSZ
 * \param[in]	prefix		string with which to start the name
 * \param[in]	v_number	vnode number to complete the name
 *
 * \post sname will point to the beginning of a NULL-terminated
 *       string to be used as a semaphore name with a maximum of
 *       (METER_NAMSZ-1) characters plus the NULL.  The name is a
 *       concatenation of the string at 'prefix' and the ASCII
 *       representation of the number in 'v_number'.  sname is
 *       returned.
 *
 * \note Due to IRIX's use of uint64_t to represent vnumber_t and a
 *       maximum semaphore name length of 15 (METER_NAMSZ-1), this
 *       function cannot be guaranteed to produce a name which
 *       uniquely describes a vnode.
 *
 */
char *
makesname(char *sname, const char *prefix, vnumber_t v_number)
{
    char vnbuf[21]; /* max number of uint64 decimal digits + 1 */
    size_t prlen, vnlen;

    if (sname) {
	/*
	 * Note: IRIX doesn't have realloc() available in the
	 * kernel, so the openafs util implementation of snprintf is
	 * not usable.  What follows is intended to reproduce the
	 * behavior of:
	 * 	snprintf(sname, METER_NAMSZ, "%s%llu", prefix,
	 *      	 (unsigned long long)v_number);
	 * Additionally, the kernel only provides a void sprintf(),
	 * making length checking slightly more difficult.
	 */
	prlen = strlen(prefix);
	if (prlen > METER_NAMSZ-1)
	    prlen = METER_NAMSZ-1;
	strncpy(sname, prefix, prlen);

	memset(vnbuf, 0, sizeof(vnbuf));
	sprintf(vnbuf, "%llu", (unsigned long long)v_number);
	vnlen = strlen(vnbuf);
	if (vnlen+prlen > METER_NAMSZ-1)
	    vnlen = METER_NAMSZ-1-prlen;
	if (vnlen > 0)
	    strncpy(&(sname[prlen]), vnbuf, vnlen);
	sname[vnlen+prlen] = '\0';
    }
    return sname;
}

#ifdef AFS_SGI_VNODE_GLUE
#include <sys/invent.h>
extern mutex_t afs_init_kern_lock;

/* afs_init_kernel_config
 *
 * initialize vnode glue layer by testing for NUMA.
 * Argument: flag
 * 0 = no numa, 1 = has numa, -1 = test for numa.
 */
int
afs_init_kernel_config(int flag)
{
    static int afs_kern_inited = 0;
    int code = 0;

    mutex_enter(&afs_init_kern_lock);
    if (!afs_kern_inited) {
	afs_kern_inited = 1;

	if (flag == -1) {
	    inventory_t *pinv;
	    /* test for numa arch. */
	    /* Determine if thisis a NUMA platform. Currently, this is true
	     * only if it's an IP27 or IP35.
	     */
	    pinv =
		find_inventory((inventory_t *) NULL, INV_PROCESSOR,
			       INV_CPUBOARD, -1, -1, -1);
	    if (!pinv)
		code = ENODEV;
	    else
		afs_is_numa_arch = ((pinv->inv_state == INV_IP27BOARD)
				    || (pinv->inv_state == INV_IP35BOARD))
		    ? 1 : 0;
	} else
	    afs_is_numa_arch = flag;
    }
    mutex_exit(&afs_init_kern_lock);
    return code;
}
#endif /* AFS_SGI_VNODE_GLUE */

/* And just so we know what someone is _really_ running */
#ifdef IP19
int afs_ipno = 19;
#elif defined(IP20)
int afs_ipno = 20;
#elif defined(IP21)
int afs_ipno = 21;
#elif defined(IP25)
int afs_ipno = 25;
#elif defined(IP26)
int afs_ipno = 26;
#elif defined(IP27)
int afs_ipno = 27;
#elif defined(IP28)
int afs_ipno = 28;
#elif defined(IP30)
int afs_ipno = 30;
#elif defined(IP35)
int afs_ipno = 35;
#else
int afs_ipno = -1;
#endif


#endif /* AFS_SGI62_ENV */
