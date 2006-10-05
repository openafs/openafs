/*
 * CMUCS AFStools
 * dumpscan - routines for scanning and manipulating AFS volume dumps
 *
 * Copyright (c) 1998 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software_Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/* pathname.c - Pathname lookup and traversal */

#include <errno.h>
#include <string.h>

#include "dumpscan.h"
#include "dumpscan_errs.h"

/* Hash function for a vnode */
#define BUCKET_SIZE 32
#define vnode_hash(phi,vnode) ((vnode) & ((1 << (phi)->hash_size) - 1))


static vhash_ent *
get_vhash_ent(path_hashinfo * phi, afs_uint32 vnode, int make)
{
    int key = vnode_hash(phi, vnode);
    vhash_ent *vhe;

    for (vhe = phi->hash_table[key]; vhe && vhe->vnode != vnode;
	 vhe = vhe->next);
    if (make && !vhe) {
	vhe = (vhash_ent *) malloc(sizeof(vhash_ent));
	if (vhe) {
	    memset(vhe, 0, sizeof(vhash_ent));
	    vhe->vnode = vnode;
	    vhe->next = phi->hash_table[key];
	    phi->hash_table[key] = vhe;
	}
    }
    return vhe;
}


static afs_uint32
volhdr_cb(afs_vol_header * hdr, XFILE * X, void *refcon)
{
    path_hashinfo *phi = (path_hashinfo *) refcon;
    int nfiles, hsize;

    if (hdr->field_mask & F_VOLHDR_NFILES) {
	nfiles = phi->n_vnodes = hdr->nfiles;
	for (phi->hash_size = 1; nfiles > BUCKET_SIZE;
	     phi->hash_size++, nfiles >>= 1);
	hsize = (1 << phi->hash_size);
	phi->hash_table = (vhash_ent **) malloc(hsize * sizeof(vhash_ent *));
	if (!phi->hash_table)
	    return ENOMEM;
	memset(phi->hash_table, 0, hsize * sizeof(vhash_ent *));
	return 0;
    } else {
	if (phi->p->cb_error)
	    (phi->p->cb_error) (DSERR_FMT, 1, phi->p->err_refcon,
				"File count missing from volume header");
	return DSERR_FMT;
    }
}


static afs_uint32
vnode_keep(afs_vnode * v, XFILE * X, void *refcon)
{
    path_hashinfo *phi = (path_hashinfo *) refcon;
    vhash_ent *vhe;

    if (!phi->hash_table) {
	if (phi->p->cb_error)
	    (phi->p->cb_error) (DSERR_FMT, 1, phi->p->refcon,
				"No volume header in dump???");
	return DSERR_FMT;
    }
    vhe = get_vhash_ent(phi, v->vnode, 1);
    if (!vhe)
	return ENOMEM;
    cp64(vhe->v_offset, v->offset);
    if (v->field_mask & F_VNODE_PARENT)
	vhe->parent = v->parent;
    if (v->field_mask & F_VNODE_DATA) {
	cp64(vhe->d_offset, v->d_offset);
	vhe->d_size = v->size;
    }
    if ((v->field_mask & F_VNODE_TYPE) && v->type == vDirectory)
	phi->n_dirs++;
    else
	phi->n_files++;
    return 0;
}


static afs_uint32
vnode_stop(afs_vnode * v, XFILE * X, void *refcon)
{
    path_hashinfo *phi = (path_hashinfo *) refcon;
    int r;

    /* If the file is seekable, try to position so we can pick up later... */
    if (phi->p->flags & DSFLAG_SEEK)
	if (r = xfseek(X, &v->offset))
	    return r;
    return DSERR_DONE;
}


static afs_uint32
dirent_cb(afs_vnode * v, afs_dir_entry * de, XFILE * X, void *refcon)
{
    path_hashinfo *phi = (path_hashinfo *) refcon;
    vhash_ent *vhe;

    if (!phi->hash_table) {
	if (phi->p->cb_error)
	    (phi->p->cb_error) (DSERR_FMT, 1, phi->p->refcon,
				"No volume header in dump???");
	return DSERR_FMT;
    }
    if (!strcmp(de->name, ".") || !strcmp(de->name, ".."))
	return 0;
    vhe = get_vhash_ent(phi, de->vnode, 1);
    if (!vhe)
	return ENOMEM;
    vhe->parent = v->vnode;
    return 0;
}


/* Prescan the vnodes in a dump file, collecting information that will
 * be useful in generating and following pathnames.  
 */
afs_uint32
Path_PreScan(XFILE * X, path_hashinfo * phi, int full)
{
    dump_parser my_p, *p = phi->p;
    int r;

    memset(phi, 0, sizeof(path_hashinfo));
    phi->p = p;
    memset(&my_p, 0, sizeof(my_p));
    my_p.refcon = (void *)phi;
    my_p.cb_volhdr = volhdr_cb;
    my_p.cb_vnode_dir = vnode_keep;
    if (full) {
	my_p.cb_vnode_file = vnode_keep;
	my_p.cb_vnode_link = vnode_keep;
	my_p.cb_vnode_empty = vnode_keep;
	my_p.cb_vnode_wierd = vnode_keep;
    } else {
	my_p.cb_vnode_file = vnode_stop;
	my_p.cb_vnode_link = vnode_stop;
	my_p.cb_vnode_empty = vnode_stop;
	my_p.cb_vnode_wierd = vnode_stop;
    }
    my_p.err_refcon = p->err_refcon;
    my_p.cb_error = p->cb_error;
    my_p.cb_dirent = dirent_cb;
    my_p.flags = p->flags;
    my_p.print_flags = p->print_flags;
    my_p.repair_flags = p->repair_flags;

    return ParseDumpFile(X, &my_p);
}


/* Free the hash table in a path_hashinfo */
void
Path_FreeHashTable(path_hashinfo * phi)
{
    int i, size;
    vhash_ent *vhe, *next_vhe;

    if (phi->hash_table) {
	size = (1 << phi->hash_size);
	for (i = 0; i < size; i++)
	    for (vhe = phi->hash_table[i]; vhe; vhe = next_vhe) {
		next_vhe = vhe->next;
		free(vhe);
	    }
	free(phi->hash_table);
    }
}


/* Follow a pathname to the vnode it represents */
afs_uint32
Path_Follow(XFILE * X, path_hashinfo * phi, char *path, vhash_ent * his_vhe)
{
    vhash_ent *vhe;
    char *name;
    afs_uint32 r, vnum = 1;

    if (*path == '/')
	path++;
    name = strtok(path, "/");

    for (name = strtok(path, "/"); name; name = strtok(0, "/")) {
	if (!(vnum & 1)) {
	    if (phi->p->cb_error)
		(phi->p->cb_error) (ENOTDIR, 1, phi->p->err_refcon,
				    "Not a directory vnode");
	    return ENOTDIR;
	}
	vhe = get_vhash_ent(phi, vnum, 0);
	if (!vhe) {
	    if (phi->p->cb_error)
		(phi->p->cb_error) (DSERR_FMT, 1, phi->p->err_refcon,
				    "Vnode %d not found in hash table", vnum);
	    return DSERR_FMT;
	}
	if (zero64(vhe->d_offset)) {
	    if (phi->p->cb_error)
		(phi->p->cb_error) (DSERR_FMT, 1, phi->p->err_refcon,
				    "Directory vnode %d is incomplete", vnum);
	    return DSERR_FMT;
	}
	if (r = xfseek(X, &vhe->d_offset)) {
	    if (phi->p->cb_error)
		(phi->p->cb_error) (r, 1, phi->p->err_refcon,
				    "Unable to seek to directory %d", vnum);
	    return r;
	}
	vnum = 0;
	r = DirectoryLookup(X, phi->p, vhe->d_size, &name, &vnum, 0);
	if (r)
	    return r;
	if (!vnum) {
	    if (phi->p->cb_error)
		(phi->p->cb_error) (ENOENT, 1, phi->p->err_refcon,
				    "No such vnode");
	    return ENOENT;
	}
    }
    vhe = get_vhash_ent(phi, vnum, 0);
    if (!vhe) {
	if (phi->p->cb_error)
	    (phi->p->cb_error) (DSERR_FMT, 1, phi->p->err_refcon,
				"Vnode %d not found in hash table", vnum);
	return DSERR_FMT;
    }
    if (his_vhe)
	*his_vhe = *vhe;
    return 0;
}


afs_uint32
Path_Build(XFILE * X, path_hashinfo * phi, afs_uint32 vnode, char **his_path,
	   int fast)
{
    vhash_ent *vhe;
    char *name, *path = 0, fastbuf[12];
    char *x, *y;
    afs_uint32 parent, r;
    int nl, pl = 0;

    if (vnode == 1) {
	*his_path = (char *)malloc(2);
	if (!his_path) {
	    if (phi->p->cb_error)
		(phi->p->cb_error) (ENOMEM, 1, phi->p->err_refcon,
				    "No memory for pathname of vnode 1");
	    return ENOMEM;
	}
	strcpy(*his_path, "/");
	return 0;
    }

    *his_path = 0;
    vhe = get_vhash_ent(phi, vnode, 0);
    if (!vhe) {
	if (phi->p->cb_error)
	    (phi->p->cb_error) (DSERR_FMT, 1, phi->p->err_refcon,
				"Vnode %d not found in hash table", vnode);
	return DSERR_FMT;
    }
    while (vnode != 1) {
	/* Find the parent */
	if (!vhe->parent) {
	    if (phi->p->cb_error)
		(phi->p->cb_error) (DSERR_FMT, 1, phi->p->err_refcon,
				    "Vnode %d has no parent?", vnode);
	    if (path)
		free(path);
	    return DSERR_FMT;
	}
	parent = vhe->parent;
	vhe = get_vhash_ent(phi, parent, 0);
	if (phi->p->print_flags & DSPRINT_DEBUG)
	    fprintf(stderr, "Searching for vnode %d in parent %d\n", vnode,
		    parent);
	if (!vhe) {
	    if (phi->p->cb_error)
		(phi->p->cb_error) (DSERR_FMT, 1, phi->p->err_refcon,
				    "Vnode %d not found in hash table",
				    parent);
	    if (path)
		free(path);
	    return DSERR_FMT;
	}

	if (fast) {
	    /* Make up a path component from the vnode number */
	    sprintf(fastbuf, "%d", vnode);
	    name = fastbuf;
	} else {
	    /* Do a reverse-lookup in the parent directory */
	    if (zero64(vhe->d_offset)) {
		if (phi->p->cb_error)
		    (phi->p->cb_error) (DSERR_FMT, 1, phi->p->err_refcon,
					"Directory vnode %d is incomplete",
					parent);
		if (path)
		    free(path);
		return DSERR_FMT;
	    }
	    if (r = xfseek(X, &vhe->d_offset)) {
		if (phi->p->cb_error)
		    (phi->p->cb_error) (errno, 1, phi->p->err_refcon,
					"Unable to seek to directory %d",
					parent);
		if (path)
		    free(path);
		return r;
	    }

	    name = 0;
	    r = DirectoryLookup(X, phi->p, vhe->d_size, &name, &vnode, 0);
	    if (r)
		return r;
	    if (!name) {
		if (phi->p->cb_error)
		    (phi->p->cb_error) (DSERR_FMT, 1, phi->p->err_refcon,
					"No entry for vnode %d in directory %d",
					vnode, parent);
		if (path)
		    free(path);
		return ENOENT;
	    }
	}

	nl = strlen(name);
	if (path) {
	    path = (char *)realloc(path, nl + pl + 2);
	    if (!path) {
		if (phi->p->cb_error)
		    (phi->p->cb_error) (ENOMEM, 1, phi->p->err_refcon,
					"No memory for pathname of vnode 1");
		return ENOMEM;
	    }
	    x = path + pl;
	    y = x + nl + 1;
	    while (x >= path)
		*y-- = *x--;
	    path[0] = '/';
	    for (x = name, y = path + 1; *x;)
		*y++ = *x++;
	    pl += nl + 1;
	} else {
	    path = (char *)malloc(nl + 2);
	    if (!path) {
		if (phi->p->cb_error)
		    (phi->p->cb_error) (ENOMEM, 1, phi->p->err_refcon,
					"No memory for pathname of vnode 1");
		return ENOMEM;
	    }
	    path[0] = '/';
	    strcpy(path + 1, name);
	    pl = nl + 1;
	}
	if (!fast)
	    free(name);
	vnode = parent;
    }
    *his_path = path;
    return 0;
}
