
struct db_lockS
{
    afs_int32 type;                  /* user defined - for consistency checks */
    afs_int32 lockState;             /* locked/free */
    afs_int32 lockTime;              /* when locked */
    afs_int32 expires;               /* when timeout expires */
    afs_int32 instanceId;            /* user instance id */
    int  lockHost;              /* locking host, if possible */
};

typedef struct db_lockS    db_lockT;
typedef db_lockT           db_lockP;
