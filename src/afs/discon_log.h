/*
 * This file contains all the relevant information pertaining to logging
 * for disconnected afs.
 */

#ifndef  _DISCONN_LOG_H

#define _DISCONN_LOG_H



enum log_ops {
	DIS_STORE,
	DIS_MKDIR,
	DIS_CREATE,
	DIS_REMOVE,
	DIS_RMDIR,
	DIS_RENAME,
	DIS_LINK,
	DIS_SYMLINK,
	DIS_SETATTR,
	DIS_FSYNC,
	DIS_ACCESS,
	DIS_READDIR,
	DIS_READLINK,
	DIS_INFO,
	DIS_START_OPT,
	DIS_END_OPT,
	DIS_REPLAYED,
};

typedef enum log_ops log_ops_t;

/* These are defines for the different log flags that can be set */
#define       LOG_INACTIVE    0x1     /* log no longer needs replay */
#define       LOG_REPLAYED    0x2     /* log entry was replayed */
#define       LOG_OPTIMIZED   0x4     /* log entry was optimized away */
#define       LOG_GENERATED   0x8     /* log entry created by optimizer */


/* defines to make access easier */
#define	st_fid		log_data.st.fid
#define	st_origdv	log_data.st.origdv

typedef	struct store_log_data {
	struct VenusFid	fid;		/* fid of the file */
    	hyper 		origdv;	/* cached data version of file */
} store_log_data_t;

/* defines to make access easier */
#define	md_dirfid		log_data.md.dirfid
#define	md_parentfid		log_data.md.parentfid
#define	md_vattr		log_data.md.vattr
#define	md_name			log_data.md.name

typedef	struct mkdir_log_data {
	struct VenusFid	dirfid;		/* Fid of this dir */
	struct VenusFid	parentfid;	/* Fid of parent */
	struct vattr	vattr;		/* attrs to create with */
	char		name[MAX_NAME]; /* space to store create name */
} mkdir_log_data_t;


/* defines to make access easier */
#define	cr_filefid		log_data.cr.filefid
#define	cr_parentfid		log_data.cr.parentfid
#define	cr_vattr		log_data.cr.vattr
#define	cr_mode			log_data.cr.mode
#define	cr_exists		log_data.cr.exists
#define	cr_excl			log_data.cr.excl
#define	cr_origdv		log_data.cr.origdv
#define	cr_name			log_data.cr.name

typedef struct	create_log_data {
	struct VenusFid	filefid;	/* Fid of this file */
	struct VenusFid	parentfid;	/* Fid of parent */
	struct vattr	vattr;		/* attrs to create with */
	int		mode;		/* mode to create with */
	int		exists;		/* did file exists */
	int		excl;		/* is file create exclusive ? */
    	hyper 		origdv;		/* cached data version of file */
	char		name[MAX_NAME]; /* space to store create name */
} create_log_data_t;




/* defines to make access easier */
#define	rm_filefid		log_data.rm.filefid
#define	rm_parentfid		log_data.rm.parentfid
#define	rm_origdv		log_data.rm.origdv
#define	rm_name			log_data.rm.name

typedef struct	remove_log_data {
	struct VenusFid	filefid;	/* Fid of this file */
	struct VenusFid	parentfid;	/* Fid of parent */
    	hyper 		origdv;		/* cached data version of file */
	char		name[MAX_NAME]; /* space to store remove name */
} remove_log_data_t;



/* defines to make access easier */
#define	rd_dirfid		log_data.rd.dirfid
#define	rd_parentfid		log_data.rd.parentfid
#define	rd_name			log_data.rd.name

typedef struct	rmdir_log_data {
	struct VenusFid	dirfid;	/* Fid of this dir */
	struct VenusFid	parentfid;	/* Fid of parent */
	char		name[MAX_NAME]; /* space to store dir name */
} rmdir_log_data_t;


/* defines to make access easier */
#define	rn_oparentfid		log_data.rn.oparentfid
#define	rn_nparentfid		log_data.rn.nparentfid
#define	rn_renamefid		log_data.rn.renamefid
#define	rn_overfid		log_data.rn.overfid
#define	rn_origdv		log_data.rn.origdv
#define	rn_overdv		log_data.rn.overdv
#define	rn_names		log_data.rn.names

typedef struct	rename_log_data {
	struct VenusFid	oparentfid;	/* Fid of parent */
	struct VenusFid	nparentfid;	/* Fid of parent */
	struct VenusFid renamefid;	/* Fid of file being rename */
	struct VenusFid	overfid;	/* Fid of overwritten file */
    	hyper 		origdv;		/* cached data version of file */
    	hyper 		overdv;		/* overwritten version of cached data */
	char		names[MAX_NAME * 2]; /* space to store dir name */
} rename_log_data_t;

/* defines to make access easier */
#define	ln_linkfid		log_data.ln.linkfid
#define	ln_parentfid		log_data.ln.parentfid
#define	ln_name			log_data.ln.name

typedef struct	link_log_data {
	struct VenusFid	linkfid;	/* Fid of this dir */
	struct VenusFid	parentfid;	/* Fid of parent */
	char		name[MAX_NAME]; /* space to store create name */
} link_log_data_t;

/* defines to make access easier */
#define	sy_linkfid		log_data.sy.linkfid
#define	sy_parentfid		log_data.sy.parentfid
#define	sy_vattr		log_data.sy.vattr
#define	sy_names		log_data.sy.names

typedef struct	slink_log_data {
	struct VenusFid	linkfid;	/* Fid of this link */
	struct VenusFid	parentfid;	/* Fid of parent */
	struct vattr	vattr;		/* attrs to create with */
	char		names[MAX_NAME * 2]; /* space to names */
} slink_log_data_t;

/* defines to make access easier */
#define	sa_fid		log_data.sa.fid
#define	sa_vattr	log_data.sa.vattr
#define	sa_origdv	log_data.sa.origdv

typedef struct	setattr_log_data {
	struct VenusFid	fid;		/* operand fid */
	struct vattr	vattr;		/* attrs to set */
	hyper		origdv;		/* cached data version number */
} setattr_log_data_t;


/* defines to make access easier */
#define	nm_fid		log_data.nm.fid
#define	nm_origdv	log_data.nm.origdv

typedef struct	nonmute_log_data {
	struct VenusFid	fid;		/* fid */
	hyper		origdv;		/* cached data version */
} nonmute_log_data_t;



typedef struct log_ent {
	int		log_len;	/* len of this entry */
	log_ops_t	log_op;		/* operation */
	long		log_opno;	/* operation number */
	struct timeval	log_time;	/* time operation was logged */
	long            log_offset;     /* offset into the log file */
	short           log_flags;      /* offset into the log file */
	uid_t           log_uid;        /* uid of person performing op */

	union {
		store_log_data_t	st;
		mkdir_log_data_t	md;
		create_log_data_t	cr;
		remove_log_data_t	rm;
		rmdir_log_data_t	rd;
		rename_log_data_t	rn;
		link_log_data_t		ln;
		slink_log_data_t	sy;
		setattr_log_data_t	sa;
		nonmute_log_data_t	nm;
	} log_data;
} log_ent_t;


#endif  _DISCONN_LOG_H

#ifdef DISCONN
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
#endif
