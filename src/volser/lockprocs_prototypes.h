#ifndef	_LOCKPROCS_PROTOTYPES_H
#define _LOCKPROCS_PROTOTYPES_H
/* older nvldbentry versions */
extern void Lp_SetRWValue(struct nvldbentry *entry, afs_uint32 oserver, afs_int32 opart,
              afs_uint32 nserver, afs_int32 npart);
extern void Lp_SetROValue(struct nvldbentry *entry, afs_uint32 oserver,
              afs_int32 opart, afs_uint32 nserver, afs_int32 npart);
extern int Lp_Match(afs_uint32 server, afs_int32 part, struct nvldbentry *entry);
extern int Lp_ROMatch(afs_uint32 server, afs_int32 part, struct nvldbentry *entry);
extern int Lp_GetRwIndex(struct nvldbentry *entry);

/* newer uvldbentry versions */
extern void Lp_SetRWValueU(struct uvldbentry *entry, afs_uint32 oserver, afs_int32 opart,
              afs_uint32 nserver, afs_int32 npart);
extern void Lp_SetROValueU(struct uvldbentry *entry, afs_uint32 oserver,
              afs_int32 opart, afs_uint32 nserver, afs_int32 npart);
extern int Lp_MatchU(afs_uint32 server, afs_int32 part, struct uvldbentry *entry);
extern int Lp_ROMatchU(afs_uint32 server, afs_int32 part, struct uvldbentry *entry);
extern int Lp_GetRwIndexU(struct uvldbentry *entry);

extern void Lp_QInit(struct qHead *ahead);
extern void Lp_QAdd(struct qHead *ahead, struct aqueue *elem);
extern int Lp_QScan(struct qHead *ahead, afs_int32 id, int *success, struct aqueue **elem);
extern void Lp_QEnumerate(struct qHead *ahead, int *success, struct aqueue *elem);
extern void Lp_QTraverse(struct qHead *ahead);
#endif
