/* AUTORIGHTS
Copyright (C) 2003 - 2010 Chaskiel Grundman
All rights reserved

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <afsconfig.h>
#include <afs/param.h>

#ifdef HAVE_SEARCH_H
#include <search.h>
#else
#include "afscp_search.h"
#endif

#include <afs/vlserver.h>
#include <afs/vldbint.h>
#include <afs/dir.h>
#ifdef AFS_NT40_ENV
#include <afs/errmap_nt.h>
#endif
#include "afscp.h"
#include "afscp_internal.h"

static int dirmode = DIRMODE_CELL;

int
afscp_SetDirMode(int mode)
{
    if ((mode != DIRMODE_CELL) && (mode != DIRMODE_DYNROOT)) {
	afscp_errno = EINVAL;
	return -1;
    }
    dirmode = mode;
    return 0;
}

/* comparison function for tsearch */
static int
dircompare(const void *a, const void *b)
{
    const struct afscp_dircache *sa = a, *sb = b;
    if (sa->me.fid.Vnode < sb->me.fid.Vnode)
	return -1;
    if (sa->me.fid.Vnode > sb->me.fid.Vnode)
	return 1;
    if (sa->me.fid.Unique < sb->me.fid.Unique)
	return -1;
    if (sa->me.fid.Unique > sb->me.fid.Unique)
	return 1;
    return 0;
}

/* make sure the dirstream contains the most up to date directory contents */
static int
_DirUpdate(struct afscp_dirstream *d)
{
    struct AFSFetchStatus s;
    int code;
    struct afscp_volume *v;
    struct afscp_dircache key, *stored;
    void **cached;


    code = afscp_GetStatus(&d->fid, &s);
    if (code != 0) {
	return code;
    }

    if (d->dirbuffer && d->dv == s.DataVersion) {
	return 0;
    }
    v = afscp_VolumeById(d->fid.cell, d->fid.fid.Volume);
    if (v == NULL) {
	afscp_errno = ENOENT;
	return -1;
    }

    memcpy(&key.me, &d->fid, sizeof(struct afscp_venusfid));
    cached = tfind(&key, &v->dircache, dircompare);
    if (cached != NULL) {
	stored = *(struct afscp_dircache **)cached;
	if (d->dv == s.DataVersion) {
	    d->dirbuffer = stored->dirbuffer;
	    d->buflen = stored->buflen;
	    d->dv = stored->dv;
	    return 0;
	}
	pthread_mutex_lock(&(stored->mtx));
	tdelete(&key, &v->dircache, dircompare);
	stored->nwaiters++;
	while (stored->nwaiters > 1) {
	    pthread_cond_wait(&(stored->cv), &(stored->mtx));
	}
	stored->nwaiters--;
	pthread_cond_destroy(&(stored->cv));
	pthread_mutex_unlock(&(stored->mtx));
	pthread_mutex_destroy(&(stored->mtx));
	if (d->dirbuffer != stored->dirbuffer)
	    free(stored->dirbuffer);
	free(stored);
    }
    if (s.Length > BIGMAXPAGES * AFS_PAGESIZE) {
	afscp_errno = EFBIG;
	return -1;
    }
    if (d->buflen != s.Length) {
	char *new;
	if (d->dirbuffer) {
	    new = realloc(d->dirbuffer, s.Length);
	} else {
	    new = malloc(s.Length);
	}
	if (new != NULL) {
	    d->dirbuffer = new;
	} else {
	    afscp_errno = ENOMEM;
	    return -1;
	}
	d->buflen = s.Length;
    }

    code = afscp_PRead(&d->fid, d->dirbuffer, s.Length, 0);
    if (code < 0) {
	return -1;
    }
    d->dv = s.DataVersion;
    cached = tsearch(&key, &v->dircache, dircompare);
    if (cached != NULL) {
	stored = malloc(sizeof(struct afscp_dircache));
	if (stored != NULL) {
	    memcpy(&stored->me, &d->fid, sizeof(struct afscp_venusfid));
	    stored->buflen = d->buflen;
	    stored->dirbuffer = d->dirbuffer;
	    stored->dv = d->dv;
	    stored->nwaiters = 0;
	    pthread_mutex_init(&(stored->mtx), NULL);
	    pthread_cond_init(&(stored->cv), NULL);
	    *(struct afscp_dircache **)cached = stored;
	} else {
	    tdelete(&key, &v->dircache, dircompare);
	}
    }
    return 0;
}

static struct DirEntry *
dir_get_entry(struct afscp_dirstream *d, int entry)
{
    struct DirHeader *h = (struct DirHeader *)d->dirbuffer;
    /* struct PageHeader *p; */
    struct DirEntry *ret;
    /* int fr; */
    int pg, off;

    pg = entry >> LEPP;
    off = entry & (EPP - 1);

    if (pg * AFS_PAGESIZE >= d->buflen) {	/* beyond end of file */
	return NULL;
    }
    if (!off || (!pg && off < DHE + 1)) {	/* offset refers to metadata */
	return NULL;
    }
    if (pg < MAXPAGES && h->alloMap[pg] == EPP) {	/* page is empty */
	return NULL;
    }
    /* p = (struct PageHeader *)&d->dirbuffer[pg * AFS_PAGESIZE]; */
    /* p is set but not referenced later */
    /* fr = p->freebitmap[off >> 8]; */
    /* fr is set but not referenced later */
    ret = (struct DirEntry *)&d->dirbuffer[pg * AFS_PAGESIZE + 32 * off];
    return ret;
}

struct afscp_dirstream *
afscp_OpenDir(const struct afscp_venusfid *fid)
{
    struct afscp_dirstream *ret;
    struct AFSFetchStatus s;
    int code;

    code = afscp_GetStatus(fid, &s);
    if (code != 0) {
	return NULL;
    }

    if (s.FileType != Directory) {
	afscp_errno = ENOTDIR;
	return NULL;
    }
    ret = malloc(sizeof(struct afscp_dirstream));
    if (ret == NULL) {
	afscp_errno = ENOMEM;
	return NULL;
    }
    memset(ret, 0, sizeof(struct afscp_dirstream));
    memmove(&ret->fid, fid, sizeof(struct afscp_venusfid));
    code = _DirUpdate(ret);
    if (code < 0) {
	afscp_CloseDir(ret);
	return NULL;
    }
    ret->hashent = -1;
    ret->entry = 0;

    return ret;
}

struct afscp_dirent *
afscp_ReadDir(struct afscp_dirstream *d)
{
    struct DirHeader *h = (struct DirHeader *)d->dirbuffer;
    struct DirEntry *info;
    int ent;


    ent = d->entry;
    while (ent == 0 && d->hashent < NHASHENT - 1) {
	d->hashent++;
	ent = ntohs(h->hashTable[d->hashent]);
    }
    if (ent == 0) {
	afscp_errno = 0;
	return NULL;
    }
    info = dir_get_entry(d, ent);
    if (info == NULL) {
	afscp_errno = 0;
	return NULL;
    }
    d->ret.vnode = ntohl(info->fid.vnode);
    d->ret.unique = ntohl(info->fid.vunique);
    strlcpy(d->ret.name, info->name, sizeof(d->ret.name));	/* guaranteed to be NULL terminated? */
    d->entry = ntohs(info->next);

    return &d->ret;
}

/* as it calls _DirUpdate, this may corrupt any previously returned dirent's */
int
afscp_RewindDir(struct afscp_dirstream *d)
{
    _DirUpdate(d);
    d->hashent = -1;
    d->entry = 0;
    return 0;
}

int
afscp_CloseDir(struct afscp_dirstream *d)
{
    free(d);
    return 0;
}

static int
namehash(const char *name)
{
    int hval, tval;

    hval = 0;
    while (*name != '\0')
	hval = (hval * 173) + *name++;
    tval = hval & (NHASHENT - 1);
    return tval ? (hval < 0 ? NHASHENT - tval : tval)
	: 0;
}

struct afscp_venusfid *
afscp_DirLookup(struct afscp_dirstream *d, const char *name)
{
    int code;
    int hval, entry;
    struct DirHeader *h = (struct DirHeader *)d->dirbuffer;
    struct DirEntry *info;

    code = _DirUpdate(d);
    if (code != 0) {
	return NULL;
    }
    hval = namehash(name);
    entry = ntohs(h->hashTable[hval]);

    while (entry != 0) {
	info = dir_get_entry(d, entry);
	if (info == NULL) {
	    afscp_errno = EIO;
	    return NULL;
	}
	if (strcmp(info->name, name) == 0)
	    break;
	entry = ntohs(info->next);
    }
    if (entry != 0) {
	return afscp_MakeFid(d->fid.cell, d->fid.fid.Volume,
			     ntohl(info->fid.vnode),
			     ntohl(info->fid.vunique));
    } else {
	afscp_errno = ENOENT;
	return NULL;
    }
}

struct afscp_venusfid *
afscp_ResolveName(const struct afscp_venusfid *dir, const char *name)
{
    struct afscp_venusfid *ret;
    struct afscp_dirstream *d;

    d = afscp_OpenDir(dir);
    if (d == NULL) {
	return NULL;
    }
    ret = afscp_DirLookup(d, name);
    afscp_CloseDir(d);
    return ret;
}

static int
gettoproot(struct afscp_cell *cell, char *p, char **q,
	   struct afscp_venusfid **root)
{
    struct afscp_volume *rootvol;
    char *r;

    if (dirmode == DIRMODE_DYNROOT && (strcmp(p, "/afs") == 0)) {
	afscp_errno = EINVAL;
	return 1;
    }
    if (strncmp(p, "/afs", 4) == 0) {
	afs_dprintf(("gettoproot: path is absolute\n"));
	p = &p[5];
	while (*p == '/')
	    p++;
	if (dirmode == DIRMODE_DYNROOT) {
	    int voltype;
	  retry_dot:
	    voltype = ROVOL;
	    if (*p == '.') {
		p++;
		voltype = RWVOL;
	    }
	    if (*p == '/') {
		while (*p == '/')
		    p++;
		goto retry_dot;
	    }
	    if (*p == '.' || *p == 0) {
		afscp_errno = EINVAL;
		return 1;
	    }
	    r = p;
	    while (*r && *r != '/')
		r++;
	    if (*r)
		*r++ = 0;
	    *q = r;
	    afs_dprintf(("gettoproot: dynroot looking up cell %s\n", p));
	    cell = afscp_CellByName(p, NULL);
	    if (cell == NULL) {
		afs_dprintf(("gettoproot: no such cell\n"));
		afscp_errno = ENODEV;
		return 1;
	    }
	    rootvol = afscp_VolumeByName(cell, "root.cell", voltype);
	    if (!rootvol && voltype == ROVOL)
		rootvol = afscp_VolumeByName(cell, "root.cell", RWVOL);
	} else {
	    *q = p;
	    rootvol = afscp_VolumeByName(cell, "root.afs", ROVOL);
	    if (!rootvol)
		rootvol = afscp_VolumeByName(cell, "root.afs", RWVOL);
	}
	if (!rootvol)
	    afs_dprintf(("gettoproot: volume not found\n"));
    } else {
	afs_dprintf(("gettoproot: path is relative\n"));
	if (p[0] == '/') {
	    afscp_errno = EXDEV;
	    return 1;
	}
	rootvol = afscp_VolumeByName(cell, "root.cell", ROVOL);
	if (!rootvol)
	    rootvol = afscp_VolumeByName(cell, "root.cell", RWVOL);
	*q = p;
    }
    if (rootvol == NULL) {
	afscp_errno = ENODEV;
	return 1;
    }
    *root = afscp_MakeFid(cell, rootvol->id, 1, 1);
    return 0;
}

static int
getvolumeroot(struct afscp_cell *cell, int voltype, const char *vname,
	      struct afscp_venusfid **root)
{
    struct afscp_volume *vol;
    vol = afscp_VolumeByName(cell, vname, voltype);
    if (!vol && voltype == ROVOL)
	vol = afscp_VolumeByName(cell, vname, RWVOL);
    if (vol == NULL) {
	afscp_errno = ENODEV;
	return 1;
    }
    *root = afscp_MakeFid(cell, vol->id, 1, 1);
    return 0;
}

typedef struct fidstack_s {
    int alloc;
    int count;
    struct afscp_venusfid **entries;
} *fidstack;

static fidstack
fidstack_alloc(void)
{
    fidstack ret;

    ret = malloc(sizeof(struct fidstack_s));
    if (ret == NULL) {
	afscp_errno = ENOMEM;
	return NULL;
    }
    ret->alloc = 10;
    ret->count = 0;
    ret->entries = malloc(ret->alloc * sizeof(struct afscp_venusfid *));
    if (ret->entries == NULL) {
	free(ret);
	afscp_errno = ENOMEM;
	return NULL;
    }
    return ret;
}

static void
fidstack_push(fidstack s, struct afscp_venusfid *entry)
{
    struct afscp_venusfid **new;
    if (s->count >= s->alloc) {
	new = realloc(s->entries, (s->alloc + 10) *
		      sizeof(struct afscp_venusfid *));
	if (new == NULL) {
	    return;
	}
	s->entries = new;
	s->alloc += 10;
    }
    s->entries[s->count++] = entry;
    return;
}

static struct afscp_venusfid *
fidstack_pop(fidstack s)
{
    if (s->count)
	return s->entries[--s->count];
    return NULL;
}

static void
fidstack_free(fidstack s)
{
    int i;

    for (i = 0; i < s->count; i++)
	free(s->entries[i]);
    free(s->entries);
    free(s);
}

static struct afscp_venusfid *_ResolvePath(const struct afscp_venusfid *,
					   fidstack, char *, int);

static struct afscp_venusfid *
afscp_HandleLink(struct afscp_venusfid *in,
		 const struct afscp_venusfid *parent, fidstack fids,
		 int follow, const struct AFSFetchStatus *s, int terminal)
{
    char *linkbuf, *linkbufq;
    struct afscp_cell *cell;
    struct afscp_volume *v;
    struct afscp_venusfid *root, *ret;
    int voltype;
    int code;
    ssize_t len;
    if ((s->UnixModeBits & 0111) && (follow == 0) && terminal) {	/* normal link */
	return in;
    }
    linkbuf = malloc(s->Length + 1);
    code = afscp_PRead(in, linkbuf, s->Length, 0);
    if (code < 0) {
	free(linkbuf);
	free(in);
	return NULL;
    }
    if (code != s->Length) {
	afscp_errno = EIO;
	free(linkbuf);
	free(in);
	return NULL;
    }
    linkbuf[s->Length] = 0;
    if (s->UnixModeBits & 0111) {	/* normal link */
	afs_dprintf(("Recursing on symlink %s...\n", linkbuf));
	if (linkbuf[0] == '/') {
	    if (gettoproot(in->cell, linkbuf, &linkbufq, &root)) {
		free(linkbuf);
		free(in);
		return NULL;
	    }
	    free(in);
	    ret = _ResolvePath(root, 0, linkbufq, 0);
	    free(root);
	} else {
	    free(in);
	    ret = _ResolvePath(parent, fids, linkbuf, 0);
	}
	free(linkbuf);
    } else {			/* mountpoint */
	afs_dprintf(("EvalMountPoint %s...\n", linkbuf));
	linkbufq = strchr(linkbuf, ':');
	cell = in->cell;
	v = afscp_VolumeById(cell, in->fid.Volume);
	free(in);
	if (v == NULL) {
	    free(linkbuf);
	    afscp_errno = ENODEV;
	    return NULL;
	}
	voltype = v->voltype;
	if (linkbuf[0] == '%')
	    voltype = RWVOL;
	if (linkbufq == NULL) {
	    linkbufq = linkbuf + 1;
	} else {
	    *linkbufq++ = 0;
	    cell = afscp_CellByName(linkbuf + 1, NULL);
	    if (linkbuf[0] != '%')
		voltype = ROVOL;
	}
	if (cell == NULL) {
	    free(linkbuf);
	    afscp_errno = ENODEV;
	    return NULL;
	}
	len = strnlen(linkbufq, s->Length + 1);
	if (len < 2) {
	    free(linkbuf);
	    afscp_errno = ENODEV;
	    return NULL;
	}
	len = strnlen(linkbufq, s->Length + 1);
	linkbufq[len - 1] = 0;	/* eliminate trailer */
	if (getvolumeroot(cell, voltype, linkbufq, &ret)) {
	    free(linkbuf);
	    return NULL;
	}
	free(linkbuf);
    }
    return ret;
}

static struct afscp_venusfid *
_ResolvePath(const struct afscp_venusfid *start, fidstack infids,
	     char *path, int follow)
{
    struct afscp_venusfid *ret, *cwd;
    struct AFSFetchStatus s;
    char *p, *q;
    int code;
    int linkcount;
    fidstack fids;

    p = path;
    ret = cwd = afscp_DupFid(start);
    fids = infids;
    if (fids == NULL)
	fids = fidstack_alloc();
    if (fids == NULL) {
	return NULL;
    }

    while (p && *p) {
	q = strchr(p, '/');
	if (q)
	    *q++ = 0;
	if (strcmp(p, ".") == 0) {
	    /* do nothing */
	} else if (strcmp(p, "..") == 0) {
	    ret = fidstack_pop(fids);
	    if (ret == NULL)
		ret = cwd;
	    else
		free(cwd);
	} else {
	    ret = afscp_ResolveName(cwd, p);
	    if (ret == NULL) {
		afs_dprintf(("Lookup %s in %lu.%lu.%lu failed\n", p,
			     cwd->fid.Volume, cwd->fid.Vnode,
			     cwd->fid.Unique));
		free(cwd);
		if (infids == NULL)
		    fidstack_free(fids);
		return NULL;
	    }
	    afs_dprintf(("Lookup %s in %lu.%lu.%lu->%lu.%lu.%lu\n", p,
			 cwd->fid.Volume, cwd->fid.Vnode, cwd->fid.Unique,
			 ret->fid.Volume, ret->fid.Vnode, ret->fid.Unique));
	    linkcount = 0;

	  retry:
	    if ((ret->fid.Vnode & 1) == 0) {	/* not a directory; check for link */
		code = afscp_GetStatus(ret, &s);
		if (code != 0) {
		    if (infids == NULL)
			fidstack_free(fids);
		    free(cwd);
		    free(ret);
		    return NULL;
		}
		if (s.FileType == SymbolicLink) {
		    if (linkcount++ > 5) {
			afscp_errno = ELOOP;
			if (infids == NULL)
			    fidstack_free(fids);
			free(cwd);
			free(ret);
			return NULL;
		    }
		    ret =
			afscp_HandleLink(ret, cwd, fids, follow, &s,
					 (q == NULL));
		    if (ret == NULL) {
			free(cwd);
			if (infids == NULL)
			    fidstack_free(fids);
			return NULL;
		    }
		    afs_dprintf(("   ....-> %lu.%lu.%lu\n", ret->fid.Volume,
				 ret->fid.Vnode, ret->fid.Unique));
		    goto retry;
		} else {
		    if (q != NULL) {
			afscp_errno = ENOTDIR;
			free(cwd);
			free(ret);
			if (infids == NULL)
			    fidstack_free(fids);
			return NULL;
		    }
		}
	    }
	    fidstack_push(fids, cwd);
	}
	cwd = ret;

	while ((q != NULL) && (*q == '/'))
	    q++;
	p = q;
    }
    if (infids == NULL)
	fidstack_free(fids);
    return ret;
}

/*!
 * Resolve a path to a FID starting from the root volume
 *
 * \param[in]	path	full path
 *
 * \post Returns a venusfid representing the final element of path
 *
 * \note There are three cases:
 *       (1) begins with /afs: start in root.afs of cell or home cell
 *       (2) else begins with /: error
 *       (3) else start in root.cell of cell or home cell
 */
struct afscp_venusfid *
afscp_ResolvePath(const char *path)
{
    struct afscp_venusfid *root, *ret;
    struct afscp_cell *cell;
    int code;
    char *p, *q;
    p = strdup(path);		/* so we can modify the string */
    if (p == NULL) {
	afscp_errno = ENOMEM;
	return NULL;
    }
    cell = afscp_DefaultCell();
    if (cell == NULL) {
	afscp_errno = EINVAL;
	return NULL;
    }
    code = gettoproot(cell, p, &q, &root);
    if (code != 0) {
	free(p);
	return NULL;
    }
    if (q && *q) {
	ret = _ResolvePath(root, 0, q, 1);
	free(root);
    } else
	ret = root;
    free(p);
    return ret;
}

/*!
 * Resolve a path to a FID starting from the given volume
 *
 * \param[in]	v	volume structure containing id and cell info
 * \param[in]	path	path relative to volume v
 *
 * \post Returns a venusfid representing the final element of path
 */
struct afscp_venusfid *
afscp_ResolvePathFromVol(const struct afscp_volume *v, const char *path)
{
    struct afscp_venusfid *root, *ret;
    char *p;

    /* so we can modify the string */
    p = strdup(path);
    if (p == NULL) {
	afscp_errno = ENOMEM;
	return NULL;
    }
    root = afscp_MakeFid(v->cell, v->id, 1, 1);
    while (*p == '/')
	p++;
    if (*p != '\0') {
	ret = _ResolvePath(root, 0, p, 1);
	free(root);
    } else
	ret = root;
    free(p);
    return ret;
}
