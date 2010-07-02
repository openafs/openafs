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
#include <afs/param.h>
#include <afs/afsint.h>
#include <afs/vlserver.h>
#include <afs/vldbint.h>
#include <afs/dir.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <search.h>
#include "afscp.h"
#include "afscp_internal.h"

static int dirmode=DIRMODE_CELL;

int SetDirMode(int mode) {

     if (mode != DIRMODE_CELL && mode != DIRMODE_DYNROOT) {
          afs_errno=EINVAL;
          return -1;
     }
     dirmode=mode;
     return 0;
}

/* comparison function for tsearch */
static int dircompare(const void *a, const void *b)
{
     const struct afs_dircache *sa=a,*sb=b;
     if (sa->me.fid.Vnode < sb->me.fid.Vnode) return -1;
     if (sa->me.fid.Vnode > sb->me.fid.Vnode) return 1;
     if (sa->me.fid.Unique < sb->me.fid.Unique) return -1;
     if (sa->me.fid.Unique > sb->me.fid.Unique) return 1;
     return 0;
}

/* make sure the dirstream contains the most up to date directory contents */
static int _DirUpdate (struct afs_dirstream *d) {
     struct AFSFetchStatus s;
     int code;
     struct afs_volume *v;
     struct afs_dircache key, *stored;
     void **cached;


     code=afs_GetStatus(&d->fid, &s);
     if (code)
	  return code;

     if (d->dirbuffer && d->dv == s.DataVersion)
	  return 0;
     v=afs_volumebyid(d->fid.cell, d->fid.fid.Volume);
     if (!v) {
          afs_errno=ENOENT;
          return -1;
     }

     memcpy(&key.me, &d->fid, sizeof(struct afs_venusfid));
     cached=tfind(&key, &v->dircache, dircompare);
     if (cached) {
          stored=*(struct afs_dircache **)cached;
          if (d->dv == s.DataVersion) {
               d->dirbuffer = stored->dirbuffer;
               d->buflen = stored->buflen;
               d->dv = stored->dv;
               return 0;
          }
          tdelete(&key, &v->dircache, dircompare);
          if (d->dirbuffer != stored->dirbuffer)
               free(stored->dirbuffer);
          free(stored);
     }
     if (s.Length > BIGMAXPAGES * AFS_PAGESIZE) {
	  afs_errno=EFBIG;
	  return -1;
     }
     if (d->buflen != s.Length) {
	  char *new;
	  if (d->dirbuffer) {
	       new=realloc(d->dirbuffer, s.Length);
	  } else {
	       new=malloc(s.Length);
	  }
	  if (new) {
	       d->dirbuffer=new;
	  } else {
	       afs_errno=ENOMEM;
	       return -1;
	  }
	  d->buflen=s.Length;
     }

     code=afs_pread(&d->fid, d->dirbuffer, s.Length, 0);
     if (code < 0) {
	  return -1;
     }
     d->dv=s.DataVersion;
     cached=tsearch(&key, &v->dircache, dircompare);
     if (cached) {
          stored=malloc(sizeof(struct afs_dircache));
          if (stored) {
               memcpy(&stored->me, &d->fid, sizeof(struct afs_venusfid));
               stored->buflen=d->buflen;
               stored->dirbuffer=d->dirbuffer;
               stored->dv=d->dv;
               *(struct afs_dircache **)cached=stored;
          } else {
               tdelete(&key, &v->dircache, dircompare);
          }
     }
     return 0;
}


static struct DirEntry *dir_get_entry(struct afs_dirstream *d, int entry) {

     struct DirHeader *h=(struct DirHeader *)d->dirbuffer;
     struct PageHeader *p;
     struct DirEntry *ret;
     int fr;
     int pg, off;


     pg=entry >> LEPP;
     off=entry & (EPP-1);

     if (pg * AFS_PAGESIZE >= d->buflen) { /* beyond end of file */
	  return NULL;
     }
     if (!off || (!pg && off < DHE + 1)) { /* offset refers to metadata */
	  return NULL;
     }
     if (pg < MAXPAGES && h->alloMap[pg] == EPP) { /* page is empty */
	  return NULL;
     }
     p=(struct PageHeader *)&d->dirbuffer[pg * AFS_PAGESIZE];
     fr=p->freebitmap[off >> 8];
#if 0
     if ((fr & (1<<(off & 7))) == 0 ) { /* entry isn't allocated */
	  return NULL;
     }
#endif

     ret=(struct DirEntry *)&d->dirbuffer[pg * AFS_PAGESIZE + 32 * off];

     return ret;
}

struct afs_dirstream *afs_opendir(const struct afs_venusfid *fid) {
     struct afs_dirstream *ret;
     struct AFSFetchStatus s;
     int code;

     code=afs_GetStatus(fid, &s);
     if (code) {
	  return NULL;
     }

     if (s.FileType != Directory) {
	  afs_errno=ENOTDIR;
	  return NULL;
     }
     ret=malloc(sizeof(struct afs_dirstream));
     if (!ret) {
	  afs_errno=ENOMEM;
	  return NULL;
     }
     memset(ret,0,sizeof(struct afs_dirstream));
     memmove(&ret->fid, fid, sizeof(struct afs_venusfid));
     code=_DirUpdate(ret);
     if (code < 0) {
	  afs_closedir(ret);
	  return NULL;
     }
     ret->hashent=-1;
     ret->entry=0;
     return ret;
}

struct afs_dirent *afs_readdir(struct afs_dirstream *d) {
     struct DirHeader *h=(struct DirHeader *)d->dirbuffer;
     struct DirEntry *info;
     int ent;


     ent=d->entry;
     while (ent == 0 && d->hashent < NHASHENT-1) {
	  d->hashent++;
	  ent=ntohs(h->hashTable[d->hashent]);
     }
     if (ent == 0) {
	  afs_errno=0;
	  return NULL;
     }
     info=dir_get_entry(d, ent);
     if (!info) {
	  afs_errno=0;
	  return NULL;
     }
     d->ret.vnode=ntohl(info->fid.vnode);
     d->ret.unique=ntohl(info->fid.vunique);
     strcpy(d->ret.name, info->name); /* guaranteed to be NULL terminated? */
     d->entry=ntohs(info->next);

     return &d->ret;
}

/* as it calls _DirUpdate, this may corrupt any previously returned dirent's */
int afs_rewinddir(struct afs_dirstream *d) {
     _DirUpdate(d);
     d->hashent=-1;
     d->entry=0;
     return 0;
}
int afs_closedir(struct afs_dirstream *d) {
     free(d);
     return 0;
}

static int namehash(const char *name)
{
  int hval, tval;

  hval=0;
  while (*name) hval = (hval * 173) + *name++;
  tval = hval & (NHASHENT - 1);
  return tval ?
       (hval < 0 ? NHASHENT - tval : tval)
       : 0;
}


struct afs_venusfid *DirLookup(struct afs_dirstream *d, const char *name) {
     int fid[3];
     int code;
     int hval, entry;
     struct DirHeader *h=(struct DirHeader *)d->dirbuffer;
     struct DirEntry *info;

     code=_DirUpdate(d);
     if (code)
	  return NULL;
     hval=namehash(name);
     entry=ntohs(h->hashTable[hval]);

     while (entry) {
	  info=dir_get_entry(d, entry);
	  if (!info) {
	       afs_errno=EIO;
	       return NULL;
	  }
	  if (!strcmp(info->name, name))
	       break;
	  entry=ntohs(info->next);
     }
     if (entry) {
	  return makefid(d->fid.cell, d->fid.fid.Volume,
			 ntohl(info->fid.vnode),
			 ntohl(info->fid.vunique));
     } else {
	  afs_errno=ENOENT;
	  return NULL;
     }
}

struct afs_venusfid *ResolveName(const struct afs_venusfid *dir, const char *name) {
     struct afs_venusfid *ret;
     struct afs_dirstream *d;

     d=afs_opendir(dir);
     if (!d)
	  return NULL;
     ret=DirLookup(d, name);
     afs_closedir(d);
     return ret;
}

static int gettoproot(struct afs_cell *cell, char *p, char **q, struct afs_venusfid **root) {
     struct afs_volume *rootvol;
     char *r;

     if (dirmode == DIRMODE_DYNROOT && !strcmp(p, "/afs")) {
          afs_errno=EINVAL;
          return 1;
     }
     if (!strncmp(p,"/afs",4)) {
	  afscp_dprintf(("gettoproot: path is absolute\n"));
          p=&p[5];
          while (*p == '/') p++;
          if (dirmode == DIRMODE_DYNROOT) {
	       int voltype;
	  retry_dot:
	       voltype = VOLTYPE_RO;
               if (*p == '.') {
                     p++;
                     voltype=VOLTYPE_RW;
               }
	       if (*p == '/') {
		    while (*p == '/') p++;
                    goto retry_dot;
               }
	       if (*p == '.' || *p == 0) {
                     afs_errno=EINVAL;
                     return 1;
               }
               r=p;
               while (*r && *r != '/') r++;
               if (!*r) {
                    afs_errno=ENODEV;
                    return 1;
               }
               *r++=0;
               *q=r;
               afscp_dprintf(("gettoproot: dynroot looking up cell %s\n", p));
               cell=afs_cellbyname(p);
               if (!cell) {
                    afscp_dprintf(("gettoproot: no such cell\n"));
                    afs_errno=ENODEV;
                    return 1;
               }
               rootvol=afs_volumebyname(cell, "root.cell", voltype);
               if (!rootvol && voltype == VOLTYPE_RO)
                   rootvol=afs_volumebyname(cell, "root.cell", VOLTYPE_RW);
          } else {
               *q=p;
               rootvol=afs_volumebyname(cell, "root.afs", VOLTYPE_RO);
               if (!rootvol)
                   rootvol=afs_volumebyname(cell, "root.afs", VOLTYPE_RW);
          }
          if (!rootvol)
               afscp_dprintf(("gettoproot: volume not found\n"));
     } else {
	  afscp_dprintf(("gettoproot: path is relative\n"));
          if (p[0] == '/') {
               afs_errno=EXDEV;
               return 1;
          }
          rootvol=afs_volumebyname(cell, "root.cell", VOLTYPE_RO);
          if (!rootvol)
              rootvol=afs_volumebyname(cell, "root.cell", VOLTYPE_RW);
          *q=p;
     }
     if (!rootvol) { afs_errno=ENODEV; return 1;}
     *root=makefid(cell, rootvol->id, 1, 1);
     return 0;
}

static int getvolumeroot(struct afs_cell *cell, int voltype, const char *vname, struct afs_venusfid **root) {
     struct afs_volume *vol;
     vol=afs_volumebyname(cell, vname, voltype);
     if (!vol && voltype == VOLTYPE_RO)
          vol=afs_volumebyname(cell, vname, VOLTYPE_RW);
     if (!vol) { afs_errno=ENODEV; return 1;}
     *root=makefid(cell, vol->id, 1, 1);
     return 0;
}

typedef struct fidstack_s
{
     int alloc;
     int count;
     struct afs_venusfid ** entries;
} *fidstack;

static fidstack fidstack_alloc() {
     fidstack ret;

     ret=malloc(sizeof(struct fidstack_s));
     if (!ret) {
	  afs_errno=ENOMEM;
	  return NULL;
     }
     ret->alloc=10;
     ret->count=0;
     ret->entries=malloc(ret->alloc * sizeof(struct afs_venusfid *));
     if (!ret->entries) {
          free(ret);
          afs_errno=ENOMEM;
          return NULL;
     }
     return ret;
}

static void fidstack_push(fidstack s, struct afs_venusfid *entry) {
     struct afs_venusfid **new;
     if (s->count >= s->alloc) {
          new=realloc(s->entries, (s->alloc + 10) *
                      sizeof(struct afs_venusfid *));
          if (!new)
               return;
          s->entries=new;
          s->alloc += 10;
     }
     s->entries[s->count++]=entry;
     return;
}

static struct afs_venusfid *fidstack_pop(fidstack s) {
     if (s->count)
          return s->entries[-- s->count];
     return NULL;
}

static void fidstack_free(fidstack s) {
     int i;

     for (i=0;i<s->count;i++)
          free(s->entries[i]);
     free (s->entries);
     free(s);
}

static struct afs_venusfid *_ResolvePath(const struct afs_venusfid *, fidstack, char *, int);

static struct afs_venusfid *HandleLink(struct afs_venusfid *in, const struct afs_venusfid *parent, fidstack fids, int follow,
				       const struct AFSFetchStatus *s, int terminal) {
     char *linkbuf, *linkbufq;
     struct afs_cell *cell;
     struct afs_volume *v;
     struct afs_venusfid *root,*ret;
     int voltype;
     int code;
     if ((s->UnixModeBits & 0111) &&
	 (follow == 0) &&
	 terminal) { /* normal link */
	  return in;
     }
     linkbuf=malloc(s->Length + 1);
     code=afs_pread(in, linkbuf, s->Length, 0);
     if (code < 0) {
	  free(linkbuf);
	  free(in);
	  return NULL;
     }
     if (code != s->Length) {
	  afs_errno=EIO;
	  free(linkbuf);
	  free(in);
	  return NULL;
     }
     linkbuf[s->Length]=0;
     if (s->UnixModeBits & 0111) { /* normal link */
	  afscp_dprintf(("Recursing on symlink %s...\n", linkbuf));
          if (linkbuf[0] == '/') {
               if (gettoproot(in->cell, linkbuf, &linkbufq, &root)) {
                    free(linkbuf);
                    free(in);
                    return NULL;
               }
               free(in);
               ret=_ResolvePath(root, 0, linkbufq, 0);
               free(root);
          } else {
               free(in);
               ret=_ResolvePath(parent, fids, linkbuf, 0);
          }

	  free(linkbuf);

     } else { /* mountpoint */
	  afscp_dprintf(("EvalMountPoint %s...\n", linkbuf));
	  linkbufq=strchr(linkbuf, ':');
	  cell=in->cell;
          v=afs_volumebyid(cell, in->fid.Volume);
	  free(in);
          if (!v) {
	       free(linkbuf);
               afs_errno=ENODEV;
	       return NULL;
          }
          voltype=v->voltype;
          if (linkbuf[0] == '%')
               voltype=VOLTYPE_RW;
	  if (!linkbufq) {
	       linkbufq=linkbuf+1;
	  } else {
	       *linkbufq++ = 0;
	       cell=afs_cellbyname(linkbuf+1);
               if (linkbuf[0] != '%')
                    voltype=VOLTYPE_RO;
	  }
	  if (!cell) {
	       free(linkbuf);
	       afs_errno=ENODEV;
	       return NULL;
	  }
	  if (strlen(linkbufq) < 2) {
	       free(linkbuf);
               afs_errno=ENODEV;
	       return NULL;
	  }
	  linkbufq[strlen(linkbufq)-1]=0; /* eliminate trailer */
	  if (getvolumeroot(cell, voltype, linkbufq, &ret)) {
	       free(linkbuf);
	       return NULL;
	  }
	  free(linkbuf);
     }
     return ret;
}

static struct afs_venusfid *_ResolvePath(const struct afs_venusfid *start,
                                         fidstack infids, char *path,
                                         int follow) {
     struct afs_venusfid *ret,*cwd,*parent;
     struct AFSFetchStatus s;
     char *p,*q;
     int code;
     int linkcount;
     fidstack fids;

     p=path;
     ret=cwd=dupfid(start);
     fids=infids;
     if (!fids)
          fids=fidstack_alloc();
     if (!fids)
          return NULL;

     while (p && *p) {
	  q=strchr(p, '/');
	  if (q) *q++=0;
          if (!strcmp(p,".")) {
          } else if (!strcmp(p,"..")) {
               ret=fidstack_pop(fids);
               if (!ret)
                    ret=cwd;
               else
                    free(cwd);
          } else {
               ret=ResolveName(cwd, p);
               if (!ret) {
                    afscp_dprintf(("Lookup %s in %lu.%lu.%lu failed\n", p, cwd->fid.Volume, cwd->fid.Vnode, cwd->fid.Unique));
                    free(cwd);
                    if (!infids)
                         fidstack_free(fids);
                    return NULL;

               }
               afscp_dprintf(("Lookup %s in %lu.%lu.%lu->%lu.%lu.%lu\n", p, cwd->fid.Volume, cwd->fid.Vnode, cwd->fid.Unique, ret->fid.Volume, ret->fid.Vnode, ret->fid.Unique));
               linkcount=0;

          retry:
               if ((ret->fid.Vnode & 1) == 0) { /* not a directory; check for link */
                    code=afs_GetStatus(ret, &s);
                    if (code) {
                         if (!infids)
                              fidstack_free(fids);
                         free(cwd);
                         free(ret);
                         return NULL;
                    }
                    if (s.FileType == SymbolicLink) {
                         if (linkcount++ > 5) {
                              afs_errno=ELOOP;
                              if (!infids)
                                   fidstack_free(fids);
                              free(cwd);
                              free(ret);
                              return NULL;
                         }
                         ret=HandleLink(ret, cwd, fids, follow, &s, (q==NULL));
                         if (!ret) {
                              free(cwd);
                              if (!infids)
                                   fidstack_free(fids);
                              return NULL;
                         }
                         afscp_dprintf(("   ....-> %lu.%lu.%lu\n", ret->fid.Volume, ret->fid.Vnode, ret->fid.Unique));
                         goto retry;
                    } else {
                         if (q) {
                              afs_errno=ENOTDIR;
                              free(cwd);
                              free(ret);
                              if (!infids)
                                   fidstack_free(fids);
                              return NULL;
                         }
                    }
               }
               fidstack_push(fids,cwd);
          }
          cwd=ret;

	  if (q)
	       while (*q == '/')
		    q++;
	  p=q;
     }
     if (!infids)
          fidstack_free(fids);
     return ret;
}
/* 3 cases:
   begins with /afs: start in root.afs of cell or home cell
   else begins with /: error
   else start in root.cell of cell or home cell
*/
struct afs_venusfid *ResolvePath(const char *path) {
     struct afs_venusfid *root,*ret;
     struct afs_cell *cell;

     char *p,*q;
     /* so we can modify the string */
     p=strdup(path);
     if (!p) {
	  afs_errno=ENOMEM;
	  return NULL;
     }
     cell=afs_defaultcell();
     if (!cell) { afs_errno=EINVAL; return NULL;}
     if (gettoproot(cell, p, &q, &root)) {
	  free(p);
	  return NULL;
     }
     if (q && *q) {
	  ret=_ResolvePath(root, 0, q, 1);
          free(root);
     } else
	  ret=root;
     free(p);
     return ret;
}

struct afs_venusfid *ResolvePath2(const struct afs_volume *v, const char *path) {
     struct afs_venusfid *root,*ret;

     char *p,*q;
     /* so we can modify the string */
     p=strdup(path);
     if (!p) {
	  afs_errno=ENOMEM;
	  return NULL;
     }
     root=makefid(v->cell, v->id, 1,1);
     while (*p == '/') p++;
     if (*p) {
	  ret=_ResolvePath(root, 0, p, 1);
          free(root);
     } else
	  ret=root;
     free(p);
     return ret;
}
