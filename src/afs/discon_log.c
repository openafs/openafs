/*
 * This file contains functions that relate to performance statistics
 * for disconnected operation.
 */


#ifdef DISCONN

#ifndef MACH30
#include "../afs/param.h"    /* Should be always first */
#include "../afs/sysincludes.h"      /* Standard vendor system headers */
#include "../afs/afsincludes.h"      /* Afs-based standard headers */
#include "../afs/afs_stats.h"   /* afs statistics */
#include "../afs/osi.h"
#include "../afs/discon_stats.h"
#else
#include <afs/param.h>    /* Should be always first */
#include <afs/sysincludes.h>      /* Standard vendor system headers */
#include <afs/afsincludes.h>      /* Afs-based standard headers */
#include <afs/afs_stats.h>   /* afs statistics */
#include <afs/osi.h>

#endif

#include "discon.h"
#include "discon_log.h"
#ifdef LHUSTON
#include "proto.h"
#endif

extern int dlog_mod;
extern long current_op_no;

extern backing_store_t log_data;

long  Log_is_open = 0;

/*
 * read an entry from the log file described by tfile.  The result is
 * put into the log_ent data.  This return 0 if successful, 1 if
 * it failed to some reason ( ie. no more data ).
 */


int
read_log_ent(tfile, in_log)
struct osi_file *tfile;
log_ent_t *in_log;
{
    int len;
    char *bp;

    if (osi_Read(tfile, (char *) in_log, sizeof (int)) != sizeof(int))
	return 1;

    len = in_log->log_len - sizeof(int);
    bp = (char *) in_log + sizeof(int);

    if (osi_Read(tfile, bp, len) != len) {
	printf("read_log_ent: short read \n");
	return 1;
    }
    return 0;
}

void
update_log_ent(offset, flags)
long offset;
int flags;
{
    struct osi_file *tfile;
    log_ent_t *log_ent;
    int code;

    tfile = osi_UFSOpen(&log_data.bs_dev, log_data.bs_inode);
    if (!tfile)
	panic("update_log_ent: failed to open log file");

    osi_Seek(tfile, offset);

    log_ent = (log_ent_t *) osi_Alloc(sizeof(log_ent_t));
    code = read_log_ent(tfile, log_ent);

    if (code) {
	printf("update_log_ent: failed to read log entry at %d \n",
	       offset);
    } else {

	/* set the log flags */
	log_ent->log_flags |= flags;

	/* write the entry back out */
	osi_Seek(tfile, offset);
	osi_Write(tfile, (char *) log_ent, log_ent->log_len);
    }
    osi_Free((char *) log_ent, sizeof(log_ent_t));
    osi_Close(tfile);
}


/* write the log entries to disk */
long
write_log_ent(len, log)
int	len;
log_ent_t *log;
{
    long new_num;

    if (!Log_is_open) {
#ifdef LHUSTON
	printf("can't log %d because file is closed\n", current_op_no);
#endif
	return -1;
    }

    ObtainWriteLock(&log_data.bs_lock);

    new_num = current_op_no++;

    log->log_opno = new_num;
    log->log_time = time;

    log->log_offset = log_data.tfile->offset;
    log->log_flags = 0;

    osi_Write(log_data.tfile, (char *) log, len);

    ReleaseWriteLock(&log_data.bs_lock);
    dlog_mod = 1;

    osi_Wakeup(&current_op_no);

    return (new_num);
}


long
log_dis_store(avc)
	struct vcache *avc;

{
	log_ent_t	*store;
	long	op_no;

	store = (log_ent_t *) osi_Alloc(sizeof(log_ent_t));

	store->log_op = DIS_STORE;
	store->st_fid = avc->fid;
	store->st_origdv =  avc->m.DataVersion;

	/* figure out the length of a store entry */
	store->log_len = ((char *) &(store->st_origdv)) - ((char *) store)
	    + sizeof(store->st_origdv);

	op_no = write_log_ent(store->log_len, store);

	osi_Free((char *) store, sizeof(log_ent_t));
	return op_no;
}


/* Log a mkdir operation */
long
log_dis_mkdir(dvc, pvc, attrs, name)
	struct vcache *dvc;
	struct vcache *pvc;
	struct vattr  *attrs;
	char *name;

{
	log_ent_t	*mkdir;
	long	op_no;

	mkdir = (log_ent_t *) osi_Alloc(sizeof(log_ent_t));

	mkdir->log_op = DIS_MKDIR;
	mkdir->md_dirfid = dvc->fid;
	mkdir->md_parentfid = pvc->fid;
	mkdir->md_vattr = *attrs;

	/* save the name */
	strcpy((char *) mkdir->md_name, name);

	/* calculate the length of this record */
	mkdir->log_len = ((char *) mkdir->md_name - (char *) mkdir)
	    + strlen(name) + 1;

	op_no = write_log_ent(mkdir->log_len, mkdir);

	osi_Free((char *) mkdir, sizeof(log_ent_t));
	return op_no;
}


/* log a create operation */
long
log_dis_create(tvc, pvc, attrs, mode, name, excl)
	struct vcache *tvc;
	struct vcache *pvc;
	struct vattr *attrs;
	int	mode;
	char *name;
	enum vcexcl excl;

{
	log_ent_t *create;
	long	op_no;

	create = (log_ent_t *) osi_Alloc(sizeof(log_ent_t));

	create->log_op = DIS_CREATE;
	create->cr_filefid = tvc->fid;
	create->cr_parentfid = pvc->fid;
	create->cr_vattr = *attrs;
	create->cr_mode = mode;


	create->cr_excl = excl;
	create->cr_origdv = tvc->m.DataVersion;

	strcpy((char *) create->cr_name, name);

	/* calculate the length of this record */
	create->log_len = ((char *) create->cr_name - (char *) create) +
	    strlen(name) + 1;

	op_no = write_log_ent(create->log_len, create);

	osi_Free((char *) create, sizeof(log_ent_t));
	return op_no;
}

long
log_dis_remove(tvc, pvc, name)
	struct vcache *tvc;
	struct vcache *pvc;
	char *name;
{
	log_ent_t	*remove;
	long	op_no;
	remove = (log_ent_t *) osi_Alloc(sizeof(log_ent_t));

	remove->log_op = DIS_REMOVE;
	remove->rm_filefid = tvc->fid;
	remove->rm_parentfid = pvc->fid;
	remove->rm_origdv = tvc->m.DataVersion;

	strcpy((char *) remove->rm_name, name);

	/* calculate the length of this record */
	remove->log_len = ((char *) remove->rm_name - (char *) remove) +
	    strlen(name) + 1;

	op_no = write_log_ent(remove->log_len, remove);

	osi_Free((char *) remove, sizeof(log_ent_t));
	return op_no;
}

long
log_dis_rmdir(dvc, pvc, name)
	struct vcache *dvc;
	struct vcache *pvc;
	char *name;
{
	log_ent_t	*rmdir;
	int	len;
	long	op_no;

	rmdir = (log_ent_t *) osi_Alloc(sizeof(log_ent_t));

	rmdir->log_op = DIS_RMDIR;
	rmdir->rd_dirfid = dvc->fid;
	rmdir->rd_parentfid = pvc->fid;

	strcpy((char *) rmdir->rd_name, name);

	/* calculate the length of this record */
	rmdir->log_len = ((char *) rmdir->rd_name - (char *) rmdir) +
	    strlen(name) + 1;

	op_no = write_log_ent(rmdir->log_len, rmdir);

	osi_Free((char *) rmdir, sizeof(log_ent_t));
	return op_no;
}


long
log_dis_rename(opvc, npvc, rnfid, overvc, oname, nname)
	struct vcache *opvc;
	struct vcache *npvc;
	struct VenusFid *rnfid;
	struct vcache *overvc;
	char *oname;
	char *nname;

{
	log_ent_t	*rename;
	char *cp;
	long	op_no;

	/*
	 * if we overwrite an existing file, write a seperate
	 * remove entry.  The optimizer has a hard time keep track
	 * of this.
	 */

	if (overvc) {
		op_no =	log_dis_remove(overvc, npvc, nname);
	}
	rename = (log_ent_t *) osi_Alloc(sizeof(log_ent_t));

	rename->log_op = DIS_RENAME;
	rename->rn_oparentfid = opvc->fid;
	rename->rn_nparentfid = npvc->fid;
	rename->rn_renamefid = *rnfid;
	rename->rn_overfid.Fid.Volume = 0;
	rename->rn_overdv.high = -1;
	rename->rn_overdv.low = -1;
#ifdef old
	/* this was the code when we logged the overwrites in
	 * a single op.  It may be best to do it this way
	 * but causes some problems.
	 */
	if (overvc) {
		rename->rn_overfid = overvc->fid;
		rename->rn_overdv = overvc->m.DataVersion;
	} else {
		rename->rn_overfid.Fid.Volume = 0;
		/* XXX lhuston make this better (macro? ) */
		rename->rn_overdv.high = -1;
		rename->rn_overdv.low = -1;
	}
#endif

	/* copy in the old name */
	strcpy((char *) rename->rn_names, oname);
	cp = (char *) rename->rn_names + strlen(oname) + 1;

	strcpy((char *) cp, nname);
	cp += strlen(nname) + 1;


	/* calculate the length of this record */
	rename->log_len = (char *) cp - (char *) rename;

	op_no = write_log_ent(rename->log_len, rename);

	osi_Free((char *) rename, sizeof(log_ent_t));
	return op_no;
}



/* Log a mkdir operation */
long
log_dis_link(lvc, pvc, name)
	struct vcache *lvc;
	struct vcache *pvc;
	char *name;

{
	log_ent_t *link;
	long	op_no;

	link = (log_ent_t *) osi_Alloc(sizeof(log_ent_t));

	link->log_op = DIS_LINK;
	link->ln_linkfid = lvc->fid;
	link->ln_parentfid = pvc->fid;

	/* save the name */
	strcpy((char *) link->ln_name, name);

	/* calculate the length of this record */
	link->log_len = ((char *) link->ln_name - (char *) link) +
	    strlen(name) + 1;

	op_no = write_log_ent(link->log_len, link);

	osi_Free((char *) link, sizeof(log_ent_t));
	return op_no;

}


/* Log a symlink operation */
long
log_dis_symlink(newfid, pvc, linkname, targetname, attrs)
	struct VenusFid *newfid;
	struct vcache *pvc;
	char *linkname;
	char *targetname;
	struct vattr  *attrs;

{
	log_ent_t *slink;
	char *cp;
	long op_no;

	slink = (log_ent_t *) osi_Alloc(sizeof(log_ent_t));

	slink->log_op = DIS_SYMLINK;
	slink->sy_linkfid = *newfid;
	slink->sy_parentfid = pvc->fid;
	slink->sy_vattr = *attrs;

	/* copy in the link name */
	strcpy((char *) slink->sy_names, linkname);
	cp = (char *) slink->sy_names + strlen(linkname) + 1;

	/* copy in target name */
	strcpy((char *) cp, targetname);
	cp += strlen(targetname) + 1;


	/* calculate the length of this record */
	slink->log_len = (char *) cp - (char *) slink;

	op_no = write_log_ent(slink->log_len, slink);

	osi_Free((char *) slink, sizeof(log_ent_t));
	return op_no;
}


/* Log a setattr operation */
long
log_dis_setattr(tvc, attrs)
	struct vcache *tvc;
	struct vattr  *attrs;

{
	log_ent_t	*setattr;
	long op_no;

	setattr = (log_ent_t *) osi_Alloc(sizeof(log_ent_t));

	setattr->log_op = DIS_SETATTR;
	setattr->sa_fid = tvc->fid;
	setattr->sa_origdv = tvc->m.DataVersion;

	setattr->sa_vattr = *attrs;

	/* calculate the length of this record */
	setattr->log_len = ((char *) &setattr->sa_origdv - (char *) setattr) +
	    sizeof(setattr->sa_origdv);

	op_no = write_log_ent(setattr->log_len, setattr);

	osi_Free((char *) setattr, sizeof(log_ent_t));
	return op_no;
}

long
log_dis_nonmutating(tvc, op)
struct vcache *tvc;
log_ops_t	op;
{
#ifdef LOGNONMUTE
    log_ent_t	*non_mute;
    long	op_no;

    non_mute = (log_ent_t *) osi_Alloc(sizeof(log_ent_t));

    non_mute->log_op = op;
    non_mute->nm_fid = tvc->fid;
    non_mute->nm_origdv =  tvc->m.DataVersion;
    non_mute->log_len = ((char *) &non_mute->nm_origdv -
			 (char *) non_mute) + sizeof(non_mute->nm_origdv);

    /* XXX lhuston removed for debugging */
    op_no = write_log_ent(non_mute->nm_len, non_mute);

    osi_Free((char *) non_mute, sizeof(log_ent_t));
    return op_no;
#else
    return current_op_no;
#endif
}


long
log_dis_access(tvc)
	struct vcache *tvc;
{
	return log_dis_nonmutating(tvc, DIS_ACCESS);
}

long
log_dis_readlink(tvc)
	struct vcache *tvc;
{
	return log_dis_nonmutating(tvc, DIS_READLINK);
}

long
log_dis_fsync(tvc)
	struct vcache *tvc;
{
	/* treat an fsync as a store */
	return log_dis_nonmutating(tvc, DIS_FSYNC);
}

/*
This copyright pertains to modifications set off by the compile
option DISCONN
****************************************************************************
*Copyright 1993 Regents of The University of Michigan - All Rights Reserved*
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appears in all copies and that  *
* both that copyright notice and this permission notice appear in          *
* supporting documentation, and that the name of The University of Michigan*
* not be used in advertising or publicity pertaining to distribution of    *
* the software without specific, written prior permission. This software   *
* is supplied as is without expressed or implied warranties of any kind.   *
*                                                                          *
*            Center for Information Technology Integration                 *
*                  Information Technology Division                         *
*                     The University of Michigan                           *
*                       535 W. William Street                              *
*                        Ann Arbor, Michigan                               *
*                            313-763-4403                                  *
*                        info@citi.umich.edu                               *
*                                                                          *
****************************************************************************
*/



#endif /* DISCONN */
