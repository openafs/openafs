#ifndef _AFSCP_H_
#define _AFSCP_H_

/* AUTORIGHTS */
#include <afs/param.h>
#include <afs/afsint.h>
struct afs_server;
struct afs_cell;
struct afs_callback ;

struct afs_volume
{
     struct afs_cell *cell;
     afs_uint32 id;
     int voltype;
     int nservers;
     int servers[16];
     char name[256];
     void *statcache;
     void *dircache;
};

struct afs_venusfid
{
     struct afs_cell *cell;
     struct AFSFid fid;
};

struct afs_dirent
{
     afs_uint32 vnode;
     afs_uint32 unique;
     char name[16+32*(64-2)]; /* 64 is EPP. */
};

typedef struct afs_dirstream afs_DIR;
typedef struct afs_openfile afs_FILE;

extern int afs_errno;
int afscp_init(const char *cellname);
void afscp_finalize(void);

#ifdef API_COMPAT
#define GetCellByName afs_cellbyname
#define GetCellById afs_cellbyid
#define GetDefaultCell afs_defaultcell
#define SetDefaultCell afs_setdefaultcell
#define GetCellID afs_cellid
#endif
struct afs_cell *afs_defaultcell(void);
struct afs_cell *afs_cellbyname(const char *cellname) ;
int afs_setdefaultcell(const char *cellname);
struct afs_cell *afs_cellbyid(int id);
int afs_cellid(struct afs_cell *cell);


#ifdef API_COMPAT
#define GetServerById afs_serverbyid
#define GetServerByAddr afs_serverbyaddr
#define GetAnyServerById afs_anyserverbyaddr
#define GetAnyServerByIndex afs_serverbyindex
#define GetConnection afs_serverconnection
#endif

struct afs_server *afs_serverbyid(struct afs_cell *thecell, afsUUID *u);
struct afs_server *afs_serverbyaddr(struct afs_cell *thecell,
				       afs_uint32 addr);
struct afs_server *afs_anyserverbyaddr(afs_uint32 addr) ;
struct afs_server *afs_serverbyindex(int idx) ;
struct rx_connection *afs_serverconnection(const struct afs_server *srv,
					      int i);

int AddCallBack(const struct afs_server *server, const struct AFSFid *fid,
                const struct AFSFetchStatus *st, const struct AFSCallBack *cb);
int RemoveCallBack(const struct afs_server *server, const struct afs_venusfid *fid);
int ReturnCallBacks(const struct afs_server *server);
int ReturnAllCallBacks(void);


/* file metastuff */
/* frees with free() */
struct afs_venusfid *makefid(struct afs_cell *cell, afs_uint32 volume,
                             afs_uint32 vnode, afs_uint32 unique);
struct afs_venusfid *dupfid(const struct afs_venusfid *in);

struct stat;
int afs_stat(const struct afs_venusfid *fid, struct stat *s);

ssize_t afs_pread(const struct afs_venusfid *fid, void *buffer, size_t count, off_t offset);
ssize_t afs_pwrite(const struct afs_venusfid *fid, const void *buffer, size_t count, off_t offset);
afs_FILE *afs_open(const char *path);
afs_FILE *afs_fidopen(const struct afs_venusfid *fid);
off_t afs_fseek (afs_FILE *f, off_t o, int whence);
ssize_t afs_fread(const afs_FILE *f, void *buffer, size_t count);
ssize_t afs_fwrite(const afs_FILE *f, const void *buffer, size_t count);

/* rpc wrappers */
int afs_GetStatus(const struct afs_venusfid *fid, struct AFSFetchStatus *s);
int afs_StoreStatus(const struct afs_venusfid *fid, struct AFSStoreStatus *s);
int afs_CreateFile(const struct afs_venusfid *fid, /* const */ char *name,
               struct AFSStoreStatus *sst,
               struct afs_venusfid **ret);
int afs_MakeDir(const struct afs_venusfid *fid, /* const */ char *name,
               struct AFSStoreStatus *sst,
               struct afs_venusfid **ret);
int afs_Symlink(const struct afs_venusfid *fid, /* const */ char *name,
                /*const*/ char *target, struct AFSStoreStatus *sst);
int afs_RemoveFile(const struct afs_venusfid *dir, char *name);
int afs_RemoveDir(const struct afs_venusfid *dir, char *name);
int afs_FetchACL(const struct afs_venusfid *dir,
		 struct AFSOpaque *acl);
int afs_StoreACL(const struct afs_venusfid *dir,
		 struct AFSOpaque *acl);
/* directory parsing stuff*/
struct afs_dirstream *afs_opendir(const struct afs_venusfid *fid);
struct afs_dirent *afs_readdir(struct afs_dirstream *d);
int afs_rewinddir(struct afs_dirstream *d);
int afs_closedir(struct afs_dirstream *d);
struct afs_venusfid *DirLookup(struct afs_dirstream *d, const char *name);
struct afs_venusfid *ResolveName(const struct afs_venusfid *dir, const char *name);
struct afs_venusfid *ResolvePath(const char *path);
struct afs_venusfid *ResolvePath2(const struct afs_volume *start, const char *path);

/* vldb stuff */
struct afs_volume *afs_volumebyname(struct afs_cell *cell, const char *vname, afs_int32 vtype);
struct afs_volume *afs_volumebyid(struct afs_cell *cell, afs_uint32 id);

#define DIRMODE_CELL    0
#define DIRMODE_DYNROOT  1

#define VOLTYPE_RW 0
#define VOLTYPE_RO 1
#define VOLTYPE_BK 2
int SetDirMode(int mode);

#endif
