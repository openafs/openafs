/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _PTPROTOTYPES_H_
#define _PTPROTOTYPES_H_

/* utils.c */
#if defined(SUPERGROUPS)
extern afs_int32 IsAMemberOfSG(struct ubik_trans *at, afs_int32 aid, afs_int32 gid, afs_int32 depth) ;
#endif /* SUPERGROUPS */

extern afs_int32 IDHash(afs_int32 x);
extern afs_int32 NameHash(register unsigned char *aname);
extern afs_int32 pr_Write(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos, char *buff, afs_int32 len);
extern afs_int32 pr_Read(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos, char *buff, afs_int32 len);
extern int pr_WriteEntry(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos, struct prentry *tentry);
extern int pr_ReadEntry(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos, struct prentry *tentry);
extern int pr_WriteCoEntry(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos, struct contentry *tentry);
extern int pr_ReadCoEntry(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos, struct contentry *tentry);
extern afs_int32 AllocBlock(register struct ubik_trans *at);
extern afs_int32 FreeBlock(register struct ubik_trans *at, afs_int32 pos);
extern afs_int32 FindByID(register struct ubik_trans *at, afs_int32 aid);
extern afs_int32 FindByName(register struct ubik_trans *at, char aname[PR_MAXNAMELEN], struct prentry *tentryp);
extern afs_int32 AllocID(register struct ubik_trans *at, afs_int32 flag, afs_int32 *aid);
extern afs_int32 IDToName(register struct ubik_trans *at, afs_int32 aid, char aname[PR_MAXNAMELEN]);
extern afs_int32 NameToID(register struct ubik_trans *at, char aname[PR_MAXNAMELEN], afs_int32 *aid);
extern int IDCmp(afs_int32 *a, afs_int32 *b);
extern afs_int32 RemoveFromIDHash(struct ubik_trans *tt, afs_int32 aid, afs_int32 *loc);
extern afs_int32 AddToIDHash(struct ubik_trans *tt, afs_int32 aid, afs_int32 loc);
extern afs_int32 RemoveFromNameHash(struct ubik_trans *tt, char *aname, afs_int32 *loc);
extern afs_int32 AddToNameHash(struct ubik_trans *tt, char *aname, afs_int32 loc);
extern afs_int32 AddToOwnerChain(struct ubik_trans *at, afs_int32 gid, afs_int32 oid);
extern afs_int32 RemoveFromOwnerChain(struct ubik_trans *at, afs_int32 gid, afs_int32 oid);
extern afs_int32 AddToOrphan(struct ubik_trans *at, afs_int32 gid);
extern afs_int32 RemoveFromOrphan(struct ubik_trans *at, afs_int32 gid);
extern afs_int32 IsOwnerOf(struct ubik_trans *at, afs_int32 aid, afs_int32 gid);
extern afs_int32 OwnerOf(struct ubik_trans *at, afs_int32 gid);
extern afs_int32 IsAMemberOf(struct ubik_trans *at, afs_int32 aid, afs_int32 gid);
#if defined(PRDB_EXTENSIONS)
extern int pr_XHTInit(void);
extern int pr_WriteXHTEntry(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos, struct hashentry *tentry);
extern int pr_ReadXHTEntry(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos, struct hashentry *tentry);
extern int pr_LoadXHTs(struct ubik_trans *tt);
extern int pr_CreateXHT(struct ubik_trans *tt, struct pr_xht *table);
extern afs_int32 RemoveFromExtendedHash(struct ubik_trans *tt, struct pr_xht *table, void *aname, int anamelen, afs_int32 *loc);
extern afs_int32 AddToExtendedHash(struct ubik_trans *tt, struct pr_xht *table, void *aname, int anamelen, afs_int32 loc);
#endif
#endif
/* vi:set cin noet sw=4 tw=70: */
