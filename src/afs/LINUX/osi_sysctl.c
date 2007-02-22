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
    {1, "hm_retry_RO",
     &hm_retry_RO, sizeof(afs_int32), 0644, NULL,
     &proc_dointvec}
    ,
    {2, "hm_retry_RW",
     &hm_retry_RW, sizeof(afs_int32), 0644, NULL,
     &proc_dointvec}
    ,
    {3, "hm_retry_int",
     &hm_retry_int, sizeof(afs_int32), 0644, NULL,
     &proc_dointvec}
    ,
    {4, "GCPAGs",
     &afs_gcpags, sizeof(afs_int32), 0644, NULL,
     &proc_dointvec}
    ,
    {5, "rx_deadtime",
     &afs_rx_deadtime, sizeof(afs_int32), 0644, NULL,
     &proc_dointvec}
    ,
    {6, "bkVolPref",
     &afs_bkvolpref, sizeof(afs_int32), 0644, NULL,
     &proc_dointvec}
    ,
    {7, "afs_blocksUsed",
     &afs_blocksUsed, sizeof(afs_int32), 0444, NULL,
     &proc_dointvec}
    ,
    {8, "afs_blocksUsed_0",
     &afs_blocksUsed_0, sizeof(afs_int32), 0644, NULL,
     &proc_dointvec}
    ,
    {9, "afs_blocksUsed_1",
     &afs_blocksUsed_1, sizeof(afs_int32), 0644, NULL,
     &proc_dointvec}
    ,
    {10, "afs_blocksUsed_2",
     &afs_blocksUsed_2, sizeof(afs_int32), 0644, NULL,
     &proc_dointvec}
    ,
    {11, "afs_pct1",
     &afs_pct1, sizeof(afs_int32), 0644, NULL,
     &proc_dointvec}
    ,
    {12, "afs_pct2",
     &afs_pct2, sizeof(afs_int32), 0644, NULL,
     &proc_dointvec}
    ,
    {13, "afs_cacheBlocks",
     &afs_cacheBlocks, sizeof(afs_int32), 0644, NULL,
     &proc_dointvec}
    ,
    {7, "md5inum",
     &afs_new_inum, sizeof(afs_int32), 0644, NULL,
     &proc_dointvec}
    ,
    {0}
};

static ctl_table fs_sysctl_table[] = {
    {1, "afs", NULL, 0, 0555, afs_sysctl_table},
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
