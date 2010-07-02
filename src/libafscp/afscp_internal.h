#include <afs/param.h>
#include <afs/afsint.h>
#include <afs/cellconfig.h>
#define MAXADDRS 16

/* AUTORIGHTS */
struct afs_server
{
     afsUUID id;
     int index;
     int cell;
     int naddrs;
     afs_uint32 addrs[MAXADDRS];
     struct rx_connection *conns[MAXADDRS];
};
struct afs_cell
{
     int id;
     char name[MAXCELLCHARS];
     struct rx_securityClass *security;
     int scindex;
     struct ubik_client *vlservers;
     int nservers;
     int srvsalloced;
     struct afs_server **fsservers;
     void *volsbyname;
     void *volsbyid;
};

struct afs_callback
{
     int valid;
     const struct afs_server *server;
     struct AFSFid fid;
     struct AFSCallBack cb;
};

struct afs_dirstream
{
     struct afs_venusfid fid;
     int buflen;
     char *dirbuffer;
     int hashent;
     int entry;
     int dv;
     struct afs_dirent ret;
};

struct afs_dircache
{
     struct afs_venusfid me;
     int buflen;
     char *dirbuffer;
     int dv;
};

struct afs_statent
{
     struct afs_venusfid me;
     struct AFSFetchStatus status;
};

struct afs_openfile
{
     struct afs_venusfid fid;
     off_t offset;
};

extern int _rx_InitRandomPort(void);
extern int _GetSecurityObject(struct afs_cell *);
extern int _GetVLservers(struct afs_cell *);
extern int _StatInvalidate(const struct afs_venusfid *fid);
extern int _StatStuff(const struct afs_venusfid *fid, const struct AFSFetchStatus *s);

#ifdef AFSCP_DEBUG
#define afscp_dprintf(x) printf x
#else
#define afscp_dprintf(x)
#endif
