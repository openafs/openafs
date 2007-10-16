/*
 * osi_sysctl.c: Linux sysctl interface to OpenAFS
 *
 * $Id: osi_sysctl.c,v 1.7.2.5 2007/06/12 18:28:49 shadow Exp $
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

/* From afs_analyze.c */
extern afs_int32 hm_retry_RO;
extern afs_int32 hm_retry_RW;
extern afs_int32 hm_retry_int;

#ifdef CONFIG_SYSCTL
static struct ctl_table_header *afs_sysctl = NULL;

static ctl_table afs_sysctl_table[] = {
    {
	.ctl_name 	= 1, 
	.procname 	= "hm_retry_RO",
	.data 		= &hm_retry_RO, 
	.maxlen		= sizeof(afs_int32), 
	.mode     	= 0644,
	.proc_handler	= &proc_dointvec
    },
    {
        .ctl_name 	= 2, 
        .procname 	= "hm_retry_RW",
        .data		= &hm_retry_RW,
	.maxlen		= sizeof(afs_int32), 
	.mode		= 0644,
     	.proc_handler	= &proc_dointvec
    },
    {
	.ctl_name	= 3, 
	.procname	= "hm_retry_int",
	.data		= &hm_retry_int, 
	.maxlen		= sizeof(afs_int32), 
	.mode		= 0644,
	.proc_handler	= &proc_dointvec
    },
    {
	.ctl_name	= 4, 
	.procname	= "GCPAGs",
	.data		= &afs_gcpags, 
	.maxlen		= sizeof(afs_int32), 
	.mode		= 0644,
	.proc_handler 	= &proc_dointvec
    },
    {
	.ctl_name	= 5, 
	.procname	= "rx_deadtime",
	.data		= &afs_rx_deadtime, 
	.maxlen		= sizeof(afs_int32), 
	.mode		= 0644,
	.proc_handler	= &proc_dointvec
    },
    {
	.ctl_name	= 6, 
	.procname	= "bkVolPref",
	.data		= &afs_bkvolpref, 
	.maxlen		= sizeof(afs_int32), 
	.mode		= 0644,
	.proc_handler	= &proc_dointvec
    },
    {0}
};

static ctl_table fs_sysctl_table[] = {
    {
	.ctl_name	= 1, 
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
