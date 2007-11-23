/*
 * osi_sysctl.c: Linux sysctl interface to OpenAFS
 *
 * $Id$
 *
 * Written Jan 30, 2002 by Kris Van Hees (Sine Nomine Associates)
 */

#include <afsconfig.h>
#include "afs/param.h"

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */

#include <linux/sysctl.h>
#ifdef CONFIG_H_EXISTS
#include <linux/config.h>
#endif

/* From afs_util.c */
extern afs_int32 afs_new_inum;

/* From afs_analyze.c */
extern afs_int32 hm_retry_RO;
extern afs_int32 hm_retry_RW;
extern afs_int32 hm_retry_int;
extern afs_int32 afs_blocksUsed_0;
extern afs_int32 afs_blocksUsed_1;
extern afs_int32 afs_blocksUsed_2;
extern afs_int32 afs_pct1;
extern afs_int32 afs_pct2;

#ifdef CONFIG_SYSCTL
static struct ctl_table_header *afs_sysctl = NULL;

static ctl_table afs_sysctl_table[] = {
    {
#if defined(SYSCTL_TABLE_CHECKING)
	.ctl_name 	= CTL_UNNUMBERED, 
#else
	.ctl_name 	= 1, 
#endif
	.procname 	= "hm_retry_RO",
	.data 		= &hm_retry_RO, 
	.maxlen		= sizeof(afs_int32), 
	.mode     	= 0644,
	.proc_handler	= &proc_dointvec
    },
    {
#if defined(SYSCTL_TABLE_CHECKING)
	.ctl_name 	= CTL_UNNUMBERED, 
#else
        .ctl_name 	= 2, 
#endif
        .procname 	= "hm_retry_RW",
        .data		= &hm_retry_RW,
	.maxlen		= sizeof(afs_int32), 
	.mode		= 0644,
     	.proc_handler	= &proc_dointvec
    },
    {
#if defined(SYSCTL_TABLE_CHECKING)
	.ctl_name 	= CTL_UNNUMBERED, 
#else
	.ctl_name	= 3, 
#endif
	.procname	= "hm_retry_int",
	.data		= &hm_retry_int, 
	.maxlen		= sizeof(afs_int32), 
	.mode		= 0644,
	.proc_handler	= &proc_dointvec
    },
    {
#if defined(SYSCTL_TABLE_CHECKING)
	.ctl_name 	= CTL_UNNUMBERED, 
#else
	.ctl_name	= 4, 
#endif
	.procname	= "GCPAGs",
	.data		= &afs_gcpags, 
	.maxlen		= sizeof(afs_int32), 
	.mode		= 0644,
	.proc_handler 	= &proc_dointvec
    },
    {
#if defined(SYSCTL_TABLE_CHECKING)
	.ctl_name 	= CTL_UNNUMBERED, 
#else
	.ctl_name	= 5, 
#endif
	.procname	= "rx_deadtime",
	.data		= &afs_rx_deadtime, 
	.maxlen		= sizeof(afs_int32), 
	.mode		= 0644,
	.proc_handler	= &proc_dointvec
    },
    {
#if defined(SYSCTL_TABLE_CHECKING)
	.ctl_name 	= CTL_UNNUMBERED, 
#else
	.ctl_name	= 6, 
#endif
	.procname	= "bkVolPref",
	.data		= &afs_bkvolpref, 
	.maxlen		= sizeof(afs_int32), 
	.mode		= 0644,
	.proc_handler	= &proc_dointvec
    },
    {
	.ctl_name	= 7, 
	.procname	= "afs_blocksUsed",
	.data		= &afs_blocksUsed,
	.maxlen		= sizeof(afs_int32), 
	.mode		= 0444,
	.proc_handler	= &proc_dointvec
    },
    {
	.ctl_name	= 8, 
	.procname	= "afs_blocksUsed_0",
	.data		= &afs_blocksUsed_0,
	.maxlen		= sizeof(afs_int32),
	.mode		= 0644,
     	.proc_handler	= &proc_dointvec
    },
    {
	.ctl_name	= 9, 
	.procname	= "afs_blocksUsed_1",
	.data		= &afs_blocksUsed_1, 
	.maxlen		= sizeof(afs_int32),
	.mode		= 0644,
	.proc_handler	= &proc_dointvec
    },
    {
	.ctl_name	= 10, 
	.procname	= "afs_blocksUsed_2",
	.data		= &afs_blocksUsed_2, 
	.maxlen		= sizeof(afs_int32), 
	.mode		= 0644,
	.proc_handler	= &proc_dointvec
    },
    {
	.ctl_name	= 11, 
	.procname	= "afs_pct1",
	.data		= &afs_pct1, 
	.maxlen		= sizeof(afs_int32),
	.mode		= 0644,
	.proc_handler	= &proc_dointvec
    },
    {
	.ctl_name	= 12, 
	.procname	= "afs_pct2",
	.data		= &afs_pct2, 
	.maxlen		= sizeof(afs_int32),
	.mode		= 0644,
     	.proc_handler   = &proc_dointvec
    },
    {
	.ctl_name	= 13,
	.procname	= "afs_cacheBlocks",
	.data		= &afs_cacheBlocks,
	.maxlen		= sizeof(afs_int32),
	.mode		= 0644,
    	.proc_handler	= &proc_dointvec
    },
    {
	.ctl_name	= 14, 
	.procname	= "md5inum",
	.data		= &afs_new_inum, 
	.maxlen		= sizeof(afs_int32),
	.mode		= 0644,
     	.proc_handler	= &proc_dointvec
    },
    {0}
};

static ctl_table fs_sysctl_table[] = {
    {
#if defined(SYSCTL_TABLE_CHECKING)
	.ctl_name 	= CTL_UNNUMBERED, 
#else
	.ctl_name	= 1, 
#endif
	.procname	= "afs", 
	.mode		= 0555, 
	.child		= afs_sysctl_table
    },
    {0}
};

int
osi_sysctl_init()
{
#if defined(REGISTER_SYSCTL_TABLE_NOFLAG)
    afs_sysctl = register_sysctl_table(fs_sysctl_table);
#else
    afs_sysctl = register_sysctl_table(fs_sysctl_table, 0);
#endif
    if (!afs_sysctl)
	return -1;

    return 0;
}

void
osi_sysctl_clean()
{
    if (afs_sysctl) {
	unregister_sysctl_table(afs_sysctl);
	afs_sysctl = NULL;
    }
}

#endif /* CONFIG_SYSCTL */
