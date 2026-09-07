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
extern int VLDB_IsSameServer(const struct rx_sockaddr *sa1,
			     const struct rx_sockaddr *sa2, afs_int32 *errorp);
extern int VLDB_SockaddrMatchesIP(const struct rx_sockaddr *sa, afs_uint32 ip,
				  afs_int32 *errorp);
extern int VLDB_GetUuidByAddr(const struct rx_sockaddr *aserver, afsUUID *uuidp);
extern void VLDB_NvldbentryToUvldbentry(struct nvldbentry *entryp,
					struct uvldbentry *uentryp);
extern int VLDB_CreateEntryU(struct uvldbentry *entryp);
extern int VLDB_ReplaceEntryU(afs_uint32 volid, afs_int32 voltype,
			      struct uvldbentry *entryp, afs_int32 releasetype);
extern int VLDB_GetEntryByNameU(char *namep, struct uvldbentry *uentryp);
extern int VLDB_UuidSiteMatches(struct nvldbentry *entryp, int idx,
				const struct rx_sockaddr *server, afs_int32 type,
				afs_int32 *errorp);
extern int vsu_ExtractName(char rname[], char name[]);
extern afs_uint32 vsu_GetVolumeID(char *astring, struct ubik_client *acstruct, afs_int32 *errp);
#endif
