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

#ifdef CONFIG_SYSCTL

/* From afs_util.c */
extern afs_int32 afs_md5inum;

/* From afs_analyze.c */
extern afs_int32 hm_retry_RO;
extern afs_int32 hm_retry_RW;
extern afs_int32 hm_retry_int;
extern afs_int32 afs_blocksUsed_0;
extern afs_int32 afs_blocksUsed_1;
extern afs_int32 afs_blocksUsed_2;
extern afs_int32 afs_pct1;
extern afs_int32 afs_pct2;

# ifdef STRUCT_CTL_TABLE_HAS_CTL_NAME
#  ifdef CTL_UNNUMBERED
#   define AFS_SYSCTL_NAME(num) .ctl_name = CTL_UNNUMBERED,
#  else
#   define AFS_SYSCTL_NAME(num) .ctl_name = num,
#  endif
# else
#  define AFS_SYSCTL_NAME(num)
# endif

# define AFS_SYSCTL_INT2(num, perms, name, var) { \
    AFS_SYSCTL_NAME(num) \
    .procname		= name, \
    .data		= &var, \
    .maxlen		= sizeof(var), \
    .mode		= perms, \
    .proc_handler	= &proc_dointvec \
}
# define AFS_SYSCTL_INT(num, perms, var) \
	AFS_SYSCTL_INT2(num, perms, #var, var)

# if LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0)
/* end of list sentinel not needed */
#  define AFS_SYSCTL_SENTINEL
# else
/* NULL entry to mark the end of the list */
#  define AFS_SYSCTL_SENTINEL { .procname = NULL }
# endif

static struct ctl_table_header *afs_sysctl = NULL;

static struct ctl_table afs_sysctl_table[] = {
    AFS_SYSCTL_INT(1, 0644, hm_retry_RO),
    AFS_SYSCTL_INT(2, 0644, hm_retry_RW),
    AFS_SYSCTL_INT(3, 0644, hm_retry_int),

    AFS_SYSCTL_INT2(4, 0644, "GCPAGs",      afs_gcpags),
    AFS_SYSCTL_INT2(5, 0644, "rx_deadtime", afs_rx_deadtime),
    AFS_SYSCTL_INT2(6, 0644, "bkVolPref",   afs_bkvolpref),

    AFS_SYSCTL_INT( 7, 0444, afs_blocksUsed),
    AFS_SYSCTL_INT( 8, 0644, afs_blocksUsed_0),
    AFS_SYSCTL_INT( 9, 0644, afs_blocksUsed_1),
    AFS_SYSCTL_INT(10, 0644, afs_blocksUsed_2),

    AFS_SYSCTL_INT( 11, 0644, afs_pct1),
    AFS_SYSCTL_INT( 12, 0644, afs_pct2),
    AFS_SYSCTL_INT( 13, 0644, afs_cacheBlocks),
    AFS_SYSCTL_INT2(14, 0644, "md5inum", afs_md5inum),

    AFS_SYSCTL_SENTINEL
};
# if !defined(HAVE_LINUX_REGISTER_SYSCTL)
static struct ctl_table fs_sysctl_table[] = {
    {
	AFS_SYSCTL_NAME(1)
	.procname	= "afs",
	.mode		= 0555,
	.child		= afs_sysctl_table
    },
    AFS_SYSCTL_SENTINEL
};
# endif
int
osi_sysctl_init(void)
{
# if defined(HAVE_LINUX_REGISTER_SYSCTL)
    afs_sysctl = register_sysctl("afs", afs_sysctl_table);
# elif defined(REGISTER_SYSCTL_TABLE_NOFLAG)
    afs_sysctl = register_sysctl_table(fs_sysctl_table);
# else
    afs_sysctl = register_sysctl_table(fs_sysctl_table, 0);
# endif
    if (!afs_sysctl)
	return -1;

    return 0;
}

void
osi_sysctl_clean(void)
{
    if (afs_sysctl) {
	unregister_sysctl_table(afs_sysctl);
	afs_sysctl = NULL;
    }
}

#endif /* CONFIG_SYSCTL */
