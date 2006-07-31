/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Linux module support routines.
 *
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include <linux/module.h> /* early to avoid printf->printk mapping */
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/nfsclient.h"
#include "h/unistd.h"		/* For syscall numbers. */
#include "h/mm.h"

#ifdef AFS_AMD64_LINUX20_ENV
#include <asm/ia32_unistd.h>
#endif

#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>

#ifdef HAVE_KERNEL_LINUX_SEQ_FILE_H
#include <linux/seq_file.h>
#endif

struct proc_dir_entry *openafs_procfs;

#ifdef HAVE_KERNEL_LINUX_SEQ_FILE_H
static void *c_start(struct seq_file *m, loff_t *pos)
{
	struct afs_q *cq, *tq;
	loff_t n = 0;

	ObtainReadLock(&afs_xcell);
	for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
		tq = QNext(cq);

		if (n++ == *pos)
			break;
	}
	if (cq == &CellLRU)
		return NULL;

	return cq;
}

static void *c_next(struct seq_file *m, void *p, loff_t *pos)
{
	struct afs_q *cq = p, *tq;

	(*pos)++;
	tq = QNext(cq);

	if (tq == &CellLRU)
		return NULL;

	return tq;
}

static void c_stop(struct seq_file *m, void *p)
{
	ReleaseReadLock(&afs_xcell);
}

static int c_show(struct seq_file *m, void *p)
{
	struct afs_q *cq = p;
	struct cell *tc = QTOC(cq);
	int j;

	seq_printf(m, ">%s #(%d/%d)\n", tc->cellName,
		   tc->cellNum, tc->cellIndex);

	for (j = 0; j < MAXCELLHOSTS; j++) {
		afs_uint32 addr;

		if (!tc->cellHosts[j]) break;

		addr = tc->cellHosts[j]->addr->sa_ip;
		seq_printf(m, "%u.%u.%u.%u #%u.%u.%u.%u\n",
			   NIPQUAD(addr), NIPQUAD(addr));
	}

	return 0;
}

static struct seq_operations afs_csdb_op = {
	.start		= c_start,
	.next		= c_next,
	.stop		= c_stop,
	.show		= c_show,
};

static int afs_csdb_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &afs_csdb_op);
}

static struct file_operations afs_csdb_operations = {
	.open		= afs_csdb_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};


static void *uu_start(struct seq_file *m, loff_t *pos)
{
    struct unixuser *tu;
    loff_t n = 0;
    afs_int32 i;

    ObtainReadLock(&afs_xuser);
    if (!*pos)
	return (void *)(1);

    for (i = 0; i < NUSERS; i++) {
	for (tu = afs_users[i]; tu; tu = tu->next) {
	    if (++n == *pos)
		return tu;
	}
    }

    return NULL;
}

static void *uu_next(struct seq_file *m, void *p, loff_t *pos)
{
    struct unixuser *tu = p;
    afs_int32 i = 0;

    (*pos)++;
    if (!p) return NULL;

    if (p != (void *)1) {
	if (tu->next) return tu->next;
	i = UHash(tu->uid) + 1;
    }

    for (; i < NUSERS; i++)
	if (afs_users[i]) return afs_users[i];
    return NULL;
}

static void uu_stop(struct seq_file *m, void *p)
{
    ReleaseReadLock(&afs_xuser);
}

static int uu_show(struct seq_file *m, void *p)
{
    struct cell *tc = 0;
    struct unixuser *tu = p;
    char *cellname;

    if (p == (void *)1) {
	seq_printf(m, "%10s %4s %-6s  %-25s %10s",
		   "UID/PAG", "Refs", "States", "Cell", "ViceID");
	seq_printf(m, "  %10s %10s %10s %3s",
		   "Tok Set", "Tok Begin", "Tok Expire", "vno");
	seq_printf(m, "  %-15s %10s %10s %s\n",
		   "NFS Client", "UID/PAG", "Client UID", "Sysname(s)");

	return 0;
    }

    if (tu->cell == -1) {
	cellname = "<default>";
    } else {
	tc = afs_GetCellStale(tu->cell, READ_LOCK);
	if (tc) cellname = tc->cellName;
	else cellname = "<unknown>";
    }

    seq_printf(m, "%10d %4d %04x    %-25s %10d",
	       tu->uid, tu->refCount, tu->states, cellname, tu->vid);

    if (tc) afs_PutCell(tc, READ_LOCK);

    if (tu->states & UHasTokens) {
	seq_printf(m, "  %10d %10d %10d %3d",
		   tu->tokenTime, tu->ct.BeginTimestamp, tu->ct.EndTimestamp,
		   tu->ct.AuthHandle);
    } else {
	seq_printf(m, "  %-36s", "Tokens Not Set");
    }

    if (tu->exporter && tu->exporter->exp_type == EXP_NFS) {
	struct nfsclientpag *np = (struct nfsclientpag *)(tu->exporter);
        char ipaddr[16];
	int i;

        sprintf(ipaddr, "%u.%u.%u.%u", NIPQUAD(np->host));
	seq_printf(m, "  %-15s %10d %10d", ipaddr, np->uid, np->client_uid);
	if (np->sysnamecount) {
	    for (i = 0; i < np->sysnamecount; i++)
		seq_printf(m, " %s", np->sysname[i]);
	} else { 
	    seq_printf(m, " <no sysname list>");
	}

    } else if (tu->exporter) {
	seq_printf(m, "  Unknown exporter type %d", tu->exporter->exp_type);
    }
    seq_printf(m, "\n");

    return 0;
}

static struct seq_operations afs_unixuser_seqop = {
    .start		= uu_start,
    .next		= uu_next,
    .stop		= uu_stop,
    .show		= uu_show,
};

static int afs_unixuser_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &afs_unixuser_seqop);
}

static struct file_operations afs_unixuser_fops = {
    .open		= afs_unixuser_open,
    .read		= seq_read,
    .llseek		= seq_lseek,
    .release	= seq_release,
};


#else /* HAVE_KERNEL_LINUX_SEQ_FILE_H */

static int
csdbproc_info(char *buffer, char **start, off_t offset, int
length)
{
    int len = 0;
    off_t pos = 0;
    int cnt;
    struct afs_q *cq, *tq;
    struct cell *tc;
    char tbuffer[16];
    /* 90 - 64 cellname, 10 for 32 bit num and index, plus
       decor */
    char temp[91];
    afs_uint32 addr;
    
    ObtainReadLock(&afs_xcell);

    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
        tc = QTOC(cq); tq = QNext(cq);

        pos += 90;

        if (pos <= offset) {
            len = 0;
        } else {
            sprintf(temp, ">%s #(%d/%d)\n", tc->cellName, 
                    tc->cellNum, tc->cellIndex);
            sprintf(buffer + len, "%-89s\n", temp);
            len += 90;
            if (pos >= offset+length) {
                ReleaseReadLock(&afs_xcell);
                goto done;
            }
        }

        for (cnt = 0; cnt < MAXCELLHOSTS; cnt++) {
            if (!tc->cellHosts[cnt]) break;
            pos += 90;
            if (pos <= offset) {
                len = 0;
            } else {
                addr = ntohl(tc->cellHosts[cnt]->addr->sa_ip);
                sprintf(tbuffer, "%d.%d.%d.%d", 
                        (int)((addr>>24) & 0xff),
(int)((addr>>16) & 0xff),
                        (int)((addr>>8)  & 0xff), (int)( addr & 0xff));
                sprintf(temp, "%s #%s\n", tbuffer, tbuffer);
                sprintf(buffer + len, "%-89s\n", temp);
                len += 90;
                if (pos >= offset+length) {
                    ReleaseReadLock(&afs_xcell);
                    goto done;
                }
            }
        }
    }

    ReleaseReadLock(&afs_xcell);
    
done:
    *start = buffer + len - (pos - offset);
    len = pos - offset;
    if (len > length)
        len = length;
    return len;
}

#endif /* HAVE_KERNEL_LINUX_SEQ_FILE_H */

void
osi_proc_init(void)
{
    struct proc_dir_entry *entry;

    openafs_procfs = proc_mkdir(PROC_FSDIRNAME, proc_root_fs);

#ifdef HAVE_KERNEL_LINUX_SEQ_FILE_H
    entry = create_proc_entry("unixusers", 0, openafs_procfs);
    if (entry) {
	entry->proc_fops = &afs_unixuser_fops;
	entry->owner = THIS_MODULE;
    }
    entry = create_proc_entry(PROC_CELLSERVDB_NAME, 0, openafs_procfs);
    if (entry)
	entry->proc_fops = &afs_csdb_operations;
#else
    entry = create_proc_info_entry(PROC_CELLSERVDB_NAME, (S_IFREG|S_IRUGO), openafs_procfs, csdbproc_info);
#endif
    entry->owner = THIS_MODULE;
}

void
osi_proc_clean(void)
{
    remove_proc_entry(PROC_CELLSERVDB_NAME, openafs_procfs);
    remove_proc_entry(PROC_FSDIRNAME, proc_root_fs);
}
