#ifndef AFS_SRC_LIBAFSCP_AFSCP_H
#define AFS_SRC_LIBAFSCP_AFSCP_H

/* AUTORIGHTS */
#include <afs/param.h>
#include <afs/afsint.h>
#include <afs/afs_consts.h>
#include <afs/cellconfig.h>
#include <afs/dir.h>
#include <afs/afsutil.h>

#ifdef AFS_NT40_ENV
/* not included elsewhere for Windows */
#include <pthread.h>
#endif

struct afscp_server {
    afsUUID id;
    int index;
    int cell;
    int naddrs;
    afs_uint32 addrs[AFS_MAXHOSTS];
    struct rx_connection *conns[AFS_MAXHOSTS];
};

struct afscp_cell {
    int id;
    char name[MAXCELLCHARS + 1];
    struct rx_securityClass *security;
    int scindex;
    struct ubik_client *vlservers;
    int nservers;
    int srvsalloced;
    struct afscp_server **fsservers;
    void *volsbyname;
    void *volsbyid;
    char *realm;
};

struct afscp_volume {
    struct afscp_cell *cell;
    afs_uint32 id;
    int voltype;
    int nservers;
    int servers[AFS_MAXHOSTS];
    char name[AFSNAMEMAX];
    void *statcache;
    void *dircache;
};

struct afscp_venusfid {
    struct afscp_cell *cell;
    struct AFSFid fid;
};

struct afscp_dirent {
    afs_uint32 vnode;
    afs_uint32 unique;
    char name[16 + 32 * (EPP - 2)];
};

struct afscp_dirstream {
    struct afscp_venusfid fid;
    int buflen;
    char *dirbuffer;
    int hashent;
    int entry;
    int dv;
    struct afscp_dirent ret;
};

struct afscp_dircache {
    struct afscp_venusfid me;
    int buflen;
    char *dirbuffer;
    int dv;
    int nwaiters;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
};

struct afscp_statent {
    struct afscp_venusfid me;
    struct AFSFetchStatus status;
    int nwaiters;
    int cleanup;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
};

struct afscp_openfile {
    struct afscp_venusfid fid;
    off_t offset;
};

struct afscp_callback {
    int valid;
    const struct afscp_server *server;
    struct AFSFid fid;
    struct AFSCallBack cb;
    time_t as_of;
};

extern int afscp_errno;
int afscp_Init(const char *);
void afscp_Finalize(void);

int afscp_Insecure(void);
int afscp_AnonymousAuth(int);

void afscp_SetConfDir(char *confDir);

struct afscp_cell *afscp_DefaultCell(void);
struct afscp_cell *afscp_CellByName(const char *, const char *);
int afscp_SetDefaultRealm(const char *);
int afscp_SetDefaultCell(const char *);
struct afscp_cell *afscp_CellById(int);
int afscp_CellId(struct afscp_cell *);
void afscp_FreeAllCells(void);
void afscp_FreeAllServers(void);

struct afscp_server *afscp_ServerById(struct afscp_cell *, afsUUID *);
struct afscp_server *afscp_ServerByAddr(struct afscp_cell *, afs_uint32);
struct afscp_server *afscp_AnyServerByAddr(afs_uint32);
struct afscp_server *afscp_ServerByIndex(int);
struct rx_connection *afscp_ServerConnection(const struct afscp_server *,
					     int);

int afscp_CheckCallBack(const struct afscp_venusfid *fid,
			const struct afscp_server *server,
			afs_uint32 *expiretime);
int afscp_FindCallBack(const struct afscp_venusfid *f,
		       const struct afscp_server *server,
		       struct afscp_callback **ret);
int afscp_AddCallBack(const struct afscp_server *,
		      const struct AFSFid *,
		      const struct AFSFetchStatus *,
		      const struct AFSCallBack *, const time_t);
int afscp_RemoveCallBack(const struct afscp_server *,
			 const struct afscp_venusfid *);
int afscp_ReturnCallBacks(const struct afscp_server *);
int afscp_ReturnAllCallBacks(void);
int afscp_WaitForCallback(const struct afscp_venusfid *fid, int seconds);

/* file metastuff */
/* frees with free() */
struct afscp_venusfid *afscp_MakeFid(struct afscp_cell *, afs_uint32,
				     afs_uint32, afs_uint32);
struct afscp_venusfid *afscp_DupFid(const struct afscp_venusfid *);
void afscp_FreeFid(struct afscp_venusfid *);

struct stat;
int afscp_Stat(const struct afscp_venusfid *, struct stat *);

ssize_t afscp_PRead(const struct afscp_venusfid *, void *, size_t, off_t);
ssize_t afscp_PWrite(const struct afscp_venusfid *, const void *,
		     size_t, off_t);
/*
 * for future implementation: (?)
 * struct afscp_openfile *afscp_FidOpen(const struct afscp_venusfid *);
 * off_t afscp_FSeek(struct afscp_openfile *, off_t, int);
 * ssize_t afscp_FRead(const struct afscp_openfile *, void *, size_t);
 */

/* rpc wrappers */
int afscp_GetStatus(const struct afscp_venusfid *, struct AFSFetchStatus *);
int afscp_StoreStatus(const struct afscp_venusfid *, struct AFSStoreStatus *);
int afscp_CreateFile(const struct afscp_venusfid *, char *,
		     struct AFSStoreStatus *, struct afscp_venusfid **);
int afscp_Lock(const struct afscp_venusfid *, int locktype);
int afscp_MakeDir(const struct afscp_venusfid *, char *,
		  struct AFSStoreStatus *, struct afscp_venusfid **);
int afscp_Symlink(const struct afscp_venusfid *, char *,
		  char *, struct AFSStoreStatus *);
int afscp_RemoveFile(const struct afscp_venusfid *, char *);
int afscp_RemoveDir(const struct afscp_venusfid *, char *);
int afscp_FetchACL(const struct afscp_venusfid *, struct AFSOpaque *);
int afscp_StoreACL(const struct afscp_venusfid *, struct AFSOpaque *);

/* directory parsing stuff*/
struct afscp_dirstream *afscp_OpenDir(const struct afscp_venusfid *);
struct afscp_dirent *afscp_ReadDir(struct afscp_dirstream *);
int afscp_RewindDir(struct afscp_dirstream *);
int afscp_CloseDir(struct afscp_dirstream *);
struct afscp_venusfid *afscp_DirLookup(struct afscp_dirstream *,
				       const char *);
struct afscp_venusfid *afscp_ResolveName(const struct afscp_venusfid *,
					 const char *);
struct afscp_venusfid *afscp_ResolvePath(const char *);
struct afscp_venusfid *afscp_ResolvePathFromVol(const struct afscp_volume *,
						const char *);

/* vldb stuff */
struct afscp_volume *afscp_VolumeByName(struct afscp_cell *,
					const char *, afs_int32);
struct afscp_volume *afscp_VolumeById(struct afscp_cell *, afs_uint32);

#define DIRMODE_CELL	0
#define DIRMODE_DYNROOT	1
int afscp_SetDirMode(int);

#endif /* AFS_SRC_LIBAFSCP_AFSCP_H */
