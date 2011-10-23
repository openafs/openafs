#ifndef	_VSUTILS_PROTOTYPES_H
#define _VSUTILS_PROTOTYPES_H
/* vsutils.c */

extern int VLDB_CreateEntry(struct nvldbentry *entryp);
extern int VLDB_GetEntryByID(afs_uint32 volid, afs_int32 voltype, struct nvldbentry *entryp);
extern int VLDB_GetEntryByName(char *namep, struct nvldbentry *entryp);
extern int VLDB_ReplaceEntry(afs_uint32 volid, afs_int32 voltype, struct nvldbentry *entryp, afs_int32 releasetype);
extern int VLDB_ListAttributes(VldbListByAttributes *attrp, afs_int32 *entriesp, nbulkentries *blkentriesp);
extern int VLDB_ListAttributesN2(VldbListByAttributes *attrp, char *name, afs_int32 thisindex,
           afs_int32 *nentriesp, nbulkentries *blkentriesp, afs_int32 *nextindexp);
extern int VLDB_IsSameAddrs(afs_uint32 serv1, afs_uint32 serv2, afs_int32 *errorp);
extern int vsu_ExtractName(char rname[], char name[]);
extern afs_uint32 vsu_GetVolumeID(char *astring, struct ubik_client *acstruct, afs_int32 *errp);
#endif
