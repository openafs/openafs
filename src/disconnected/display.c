#include <sys/types.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/vnode.h>
/*#include <sys/inode.h>*/
#include <netinet/in.h>
/*
*/
#include "../libafs/afs/lock.h"
#include "afsint.h"
#include "stds.h"

#define KERNEL

#include "afs.h"
#undef KERNEL
#include "discon.h"
#include "discon_log.h"

#define MAX_NAME  255
#define CELL_DIRTY   0x01

#define REALLY_BIG 1024


/* forward function declarations */
void display_store(log_ent_t *, int);
void display_create(log_ent_t *, int);
void display_mkdir(log_ent_t *, int);
void display_remove(log_ent_t *, int);
void display_rmdir(log_ent_t *, int);
void display_rename(log_ent_t *, int);
void display_setattr(log_ent_t *, int);
void display_link(log_ent_t *, int);
void display_symlink(log_ent_t *, int);
void display_fsync(log_ent_t *, int);
void display_access(log_ent_t *, int);
void display_readdir(log_ent_t *, int);
void display_readlink(log_ent_t *, int);
void display_info(log_ent_t *, int);
void display_start_opt(log_ent_t *, int);
void display_end_opt(log_ent_t *, int);
void display_replayed(log_ent_t *, int);


void
display_op(log_ent, detailed, vflag)
log_ent_t *log_ent;
int detailed;
int vflag;
{
    log_ops_t op;

    /* we don't print out inactive entries unless we are verbose */
    if ((log_ent->log_flags & LOG_INACTIVE) && (!vflag)) {
	return;
    }

    /* determine which operation */
    op = log_ent->log_op;
    switch(op) {

    case DIS_STORE:
	display_store(log_ent, detailed);
	break;

    case DIS_MKDIR:
	display_mkdir(log_ent, detailed);
	break;

    case DIS_CREATE:
	display_create(log_ent, detailed);
	break;

    case DIS_REMOVE:
	display_remove(log_ent, detailed);
	break;

    case DIS_RMDIR:
	display_rmdir(log_ent, detailed);
	break;

    case DIS_RENAME:
	display_rename(log_ent, detailed);
	break;

    case DIS_LINK:
	display_link(log_ent, detailed);
	break;

    case DIS_SETATTR:
	display_setattr(log_ent, detailed);
	break;

    case DIS_SYMLINK:
	display_symlink(log_ent, detailed);
	break;

    case DIS_FSYNC:
	if (vflag) display_fsync(log_ent, detailed);
	break;

    case DIS_ACCESS:
	if (vflag) display_access(log_ent, detailed);
	break;

    case DIS_READDIR:
	if (vflag) display_readdir(log_ent, detailed);
	break;

    case DIS_READLINK:
	if (vflag) display_readlink(log_ent, detailed);
	break;

    case DIS_INFO:
	if (vflag) display_info(log_ent, detailed);
	break;

    case DIS_REPLAYED:
	if (vflag) display_replayed(log_ent, detailed);
	break;

    default:
	printf("invalid code: %d \n", op);
	break;
    }
}

void
display_flags(flags)
	int flags;
{
	printf("\t%-13s:    %x\n", "flags", flags);
}

void
display_name(string, name)
	char *string;
	char *name;
{
	printf("\t%-13s:    <%s>\n", string, name);
}

void
display_fid(string, fid)
	char *string;
	struct VenusFid fid;
{
	printf("\t%-13s:    <%x %x %x %x>\n", string, fid.Cell, fid.Fid.Volume,
	    fid.Fid.Vnode, fid.Fid.Unique);
}

void
display_opno(opno)
{
	printf("\t%-13s:    %d\n", "op number", opno);
}
void
display_offset(offset)
{
	printf("\t%-13s:    %d\n", "log offset", offset);
}

void
display_dataversion(hyper datav)
{
	printf("\t%-13s:    %8d %8d\n", "Data Version", datav.high, datav.low);
}
void
display_store(log_ent, detailed)
	log_ent_t *log_ent;
	int	detailed;
{
	if (!detailed) {
		printf("%-16s%10X%10X%10X\n", "afs_close",
		    log_ent->st_fid.Fid.Volume, log_ent->st_fid.Fid.Vnode,
		    log_ent->st_fid.Fid.Unique);
	} else {
		printf("\nStore\n");
		display_flags(log_ent->log_flags);
		display_fid("Store Fid", log_ent->st_fid);
		display_opno(log_ent->log_opno);
		display_offset(log_ent->log_offset);
		display_dataversion(log_ent->st_origdv);
	}
}

void
display_create(log_ent, detailed)
log_ent_t *log_ent;
int detailed;
{
	if(!detailed) {
  		printf("%-16s%10X%10X%10X\n", "afs_create",
		    log_ent->cr_filefid.Fid.Volume,
		    log_ent->cr_filefid.Fid.Vnode,
		    log_ent->cr_filefid.Fid.Unique);
	} else {
		printf("\nCreate\n");
		display_flags(log_ent->log_flags);
		display_fid("Parent Fid", log_ent->cr_parentfid);
		display_fid("File Fid", log_ent->cr_filefid);
		display_name("File Name", log_ent->cr_name);
		display_opno(log_ent->log_opno);
		display_offset(log_ent->log_offset);
		display_dataversion(log_ent->cr_origdv);
	}
}

void
display_mkdir(log_ent, detailed)
	log_ent_t *log_ent;
	int detailed;
{
	if(!detailed) {
		printf("%-16s%10X%10X%10X\n", "afs_mkdir",
		    log_ent->md_dirfid.Fid.Volume,
		    log_ent->md_dirfid.Fid.Vnode,
		    log_ent->md_dirfid.Fid.Unique);
	} else {
		printf("\nMkdir\n");
		display_flags(log_ent->log_flags);
		display_fid("Parent Fid", log_ent->md_parentfid);
		display_fid("Dir Fid", log_ent->md_dirfid);
		display_name("Dir Name", log_ent->md_name);
		display_opno(log_ent->log_opno);
		display_offset(log_ent->log_offset);
	}
}

void
display_remove(log_ent, detailed)
	log_ent_t *log_ent;
	int detailed;
{
	if(!detailed) {
		printf("%-16s%10X%10X%10X\n", "afs_remove",
		    log_ent->rm_filefid.Fid.Volume,
		    log_ent->rm_filefid.Fid.Vnode,
		    log_ent->rm_filefid.Fid.Unique);
	} else {
		printf("\nRemove\n");
		display_flags(log_ent->log_flags);
		display_fid("Parent Fid", log_ent->rm_parentfid);
		display_fid("File Fid", log_ent->rm_filefid);
		display_name("File Name", log_ent->rm_name);
		display_opno(log_ent->log_opno);
		display_offset(log_ent->log_offset);
		display_dataversion(log_ent->rm_origdv);
	}
}


void
display_rmdir(log_ent, detailed)
	log_ent_t *log_ent;
	int detailed;

{
	if(!detailed) {
		printf("%-16s%10X%10X%10X\n", "afs_rmdir",
		    log_ent->rd_dirfid.Fid.Volume,
		    log_ent->rd_dirfid.Fid.Vnode,
		    log_ent->rd_dirfid.Fid.Unique);
	} else {
		printf("\nRmdir\n");
		display_flags(log_ent->log_flags);
		display_fid("Parent Fid", log_ent->rd_parentfid);
		display_fid("Dir Fid", log_ent->rd_dirfid);
		display_name("Dir Name", log_ent->rd_name);
		display_opno(log_ent->log_opno);
		display_offset(log_ent->log_offset);
	}
}

void
display_rename(log_ent, detailed)
	log_ent_t *log_ent;
	int detailed;	
{
	if(!detailed) {
		printf("%-16s%10X%10X%10X\n", "afs_rename",
		    log_ent->rn_renamefid.Fid.Volume,
		    log_ent->rn_renamefid.Fid.Vnode,
		    log_ent->rn_renamefid.Fid.Unique);
	} else {
		char *oname, *nname;

		oname = log_ent->rn_names;
		nname = oname + strlen(oname) + 1;	
		printf("\nRename\n");
		display_flags(log_ent->log_flags);
		display_fid("Old Parent Fid", log_ent->rn_oparentfid);
		display_name("Old Name", oname);
		display_fid("New Parent Fid", log_ent->rn_nparentfid);
		display_name("New Name", nname);
		display_fid("Object Fid", log_ent->rn_renamefid);
		if (log_ent->rn_overfid.Fid.Volume != 0)
			display_fid("Overwritten Fid", log_ent->rn_overfid);
		display_opno(log_ent->log_opno);
		display_offset(log_ent->log_offset);
		display_dataversion(log_ent->rn_origdv);
	}
}

void
display_setattr(log_ent, detailed)
	log_ent_t *log_ent;
	int detailed;
{
	if(!detailed) {
		printf("%-16s%10X%10X%10X\n", "afs_settattr",
		    log_ent->sa_fid.Fid.Volume, log_ent->sa_fid.Fid.Vnode,
		    log_ent->sa_fid.Fid.Unique);
	} else {
		printf("\nSetattr\n");
		display_flags(log_ent->log_flags);
		display_fid("Obj Fid", log_ent->sa_fid);
		if (log_ent->sa_vattr.va_size != VNOVAL)
			printf("\t%-13s:    %d\n", "size",
			    (int) log_ent->sa_vattr.va_size);
		if (log_ent->sa_vattr.va_mode != 0xffff)
			printf("\t%-13s:    0%o\n", "mode",
			    log_ent->sa_vattr.va_mode);
		if (log_ent->sa_vattr.va_uid != -1)
			printf("\t%-13s:    %d\n", "uid",
			    log_ent->sa_vattr.va_uid);
		if (log_ent->sa_vattr.va_gid != -1)
			printf("\t%-13s:    %d\n", "gid",
			    log_ent->sa_vattr.va_gid);
	/* XXX lhuston, fix this
		if (log_ent->sa_vattr.va_mtime != -1)
			printf("\t%-13s:    %d:%d\n", "mtime",
			    log_ent->sa_vattr.va_mtime.tv_sec,
			    log_ent->sa_vattr.va_mtime.tv_usec);
		if (log_ent->sa_vattr.va_ctime != -1)
			printf("\t%-13s:    %d:%d\n", "ctime",
			    log_ent->sa_vattr.va_ctime.tv_sec,
			    log_ent->sa_vattr.va_ctime.tv_usec);
	*/
		display_opno(log_ent->log_opno);
		display_offset(log_ent->log_offset);
		display_dataversion(log_ent->sa_origdv);
	}
}

void
display_link(log_ent, detailed)
	log_ent_t *log_ent;
	int detailed;
{
	if(!detailed) {
		printf("%-16s%10X%10X%10X\n", "afs_link",
		    log_ent->ln_linkfid.Fid.Volume,
		    log_ent->ln_linkfid.Fid.Vnode,
		    log_ent->ln_linkfid.Fid.Unique);
	} else {
		printf("\nLink\n");
		display_flags(log_ent->log_flags);
		display_fid("Parent Fid", log_ent->ln_parentfid);
		display_fid("Link Fid", log_ent->ln_linkfid);
		display_name("Link Name", log_ent->ln_name);
		display_opno(log_ent->log_opno);
		display_offset(log_ent->log_offset);
	}
}

void
display_symlink(log_ent, detailed)
	log_ent_t *log_ent;
	int detailed;
{
	if(!detailed) {
		printf("%-16s%10X%10X%10X\n", "afs_symlink",
		    log_ent->sy_linkfid.Fid.Volume,
		    log_ent->sy_linkfid.Fid.Vnode,
		    log_ent->sy_linkfid.Fid.Unique);
	} else {
		char *link_name, *link_data;

		link_name = log_ent->sy_names;
		link_data = link_name + strlen(link_name) +1;

		printf("\nSymlink\n");
		display_flags(log_ent->log_flags);
		display_fid("Parent Fid", log_ent->sy_parentfid);
		display_fid("Link Fid", log_ent->sy_linkfid);
		display_name("Link Name", link_name);
		display_name("Link Data", link_data);
		display_opno(log_ent->log_opno);
		display_offset(log_ent->log_offset);
		/* XXX lhuston, add dest name */
	}
}

void
display_fsync(log_ent, detailed)
	log_ent_t *log_ent;
	int	detailed;
{
	if(!detailed) {
		printf("%-16s%10X%10X%10X\n", "afs_fsync",
		    log_ent->nm_fid.Fid.Volume, log_ent->nm_fid.Fid.Vnode,
		    log_ent->nm_fid.Fid.Unique);
	} else {
		printf("\nFsync\n");
		display_flags(log_ent->log_flags);
		display_fid("Fid", log_ent->nm_fid);
		display_opno(log_ent->log_opno);
		display_offset(log_ent->log_offset);
	}
}

void
display_access(log_ent,detailed)

	log_ent_t *log_ent;
	int	detailed;

{

	if(!detailed) {
		printf("%-16s%10X%10X%10X\n", "afs_access",
		    log_ent->nm_fid.Fid.Volume, log_ent->nm_fid.Fid.Vnode,
		    log_ent->nm_fid.Fid.Unique);
	} else {
		printf("\nAccess\n");
		display_flags(log_ent->log_flags);
		display_fid("Fid", log_ent->nm_fid);
		display_opno(log_ent->log_opno);
		display_offset(log_ent->log_offset);
	}
}


void
display_readdir(log_ent,detailed)

	log_ent_t *log_ent;
	int	detailed;

{
	if(!detailed) {
		printf("%-16s%10X%10X%10X\n", "afs_readdir",
		    log_ent->nm_fid.Fid.Volume, log_ent->nm_fid.Fid.Vnode,
		    log_ent->nm_fid.Fid.Unique);
	} else {
		printf("\nReaddir\n");
		display_flags(log_ent->log_flags);
		display_fid("Fid", log_ent->nm_fid);
		display_opno(log_ent->log_opno);
		display_offset(log_ent->log_offset);
	}
}

void
display_readlink(log_ent,detailed)

	log_ent_t *log_ent;
	int	detailed;

{
	if(!detailed) {
		printf("%-16s%10X%10X%10X\n", "afs_readlink",
		    log_ent->nm_fid.Fid.Volume, log_ent->nm_fid.Fid.Vnode,
		    log_ent->nm_fid.Fid.Unique);
	} else {
		printf("\nReadlink\n");
		display_flags(log_ent->log_flags);
		display_fid("Fid", log_ent->nm_fid);
		display_opno(log_ent->log_opno);
		display_offset(log_ent->log_offset);
	}
}

void
display_info(log_ent,detailed)

	log_ent_t *log_ent;
	int	detailed;

{
	if(!detailed) {
		printf("%-16s%10d\n", "info", log_ent->log_opno);
	} else {
		printf("\ndisplay_info\n");
		display_opno(log_ent->log_opno);
		display_offset(log_ent->log_offset);
	}
}

void
display_replayed(log_ent,detailed)

	log_ent_t *log_ent;
	int	detailed;

{
	if(!detailed) {
		printf("%-16s%10d\n", "replayed", log_ent->log_opno);
	} else {
		printf("\nReplayed\n");
		display_opno(log_ent->log_opno);
		display_offset(log_ent->log_offset);
	}
}

/*
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
