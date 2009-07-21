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
