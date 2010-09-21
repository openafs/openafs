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
extern afs_int32 IsAMemberOfSG(struct ubik_trans *at, afs_int32 aid,
			       afs_int32 gid, afs_int32 depth) ;
#endif /* SUPERGROUPS */

/* ptutils.c */
#ifdef SUPERGROUPS
extern afs_int32 AddToSGEntry(struct ubik_trans *tt, struct prentry *entry,
                              afs_int32 loc, afs_int32 aid);
extern afs_int32 GetSGList(struct ubik_trans *at, struct prentry *tentry,
                           prlist *alist);
extern afs_int32 RemoveFromSGEntry(struct ubik_trans *at, afs_int32 aid,
                                   afs_int32 bid);
extern void pt_hook_write(void);
#endif

extern afs_int32 NameHash(unsigned char *aname);
extern afs_int32 pr_Write(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos,
			  void *buff, afs_int32 len);
extern afs_int32 pr_Read(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos,
			 void *buff, afs_int32 len);
extern int pr_WriteEntry(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos,
			 struct prentry *tentry);
extern int pr_ReadEntry(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos,
			struct prentry *tentry);
extern int pr_WriteCoEntry(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos,
			   struct contentry *tentry);
extern int pr_ReadCoEntry(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos,
			  struct contentry *tentry);
extern afs_int32 AllocBlock(struct ubik_trans *at);
extern afs_int32 FreeBlock(struct ubik_trans *at, afs_int32 pos);
extern afs_int32 FindByID(struct ubik_trans *at, afs_int32 aid);
extern afs_int32 FindByName(struct ubik_trans *at,
			    char aname[PR_MAXNAMELEN], struct prentry *tentryp);
extern afs_int32 AllocID(struct ubik_trans *at, afs_int32 flag,
			 afs_int32 *aid);
extern afs_int32 IDToName(struct ubik_trans *at, afs_int32 aid,
			  char aname[PR_MAXNAMELEN]);
extern afs_int32 NameToID(struct ubik_trans *at,
			  char aname[PR_MAXNAMELEN], afs_int32 *aid);
extern int IDCmp(const void *a, const void *b);
extern afs_int32 RemoveFromIDHash(struct ubik_trans *tt, afs_int32 aid,
				  afs_int32 *loc);
extern afs_int32 AddToIDHash(struct ubik_trans *tt, afs_int32 aid,
			     afs_int32 loc);
extern afs_int32 RemoveFromNameHash(struct ubik_trans *tt, char *aname,
				    afs_int32 *loc);
extern afs_int32 AddToNameHash(struct ubik_trans *tt, char *aname,
			       afs_int32 loc);
extern afs_int32 AddToOwnerChain(struct ubik_trans *at, afs_int32 gid,
				 afs_int32 oid);
extern afs_int32 RemoveFromOwnerChain(struct ubik_trans *at, afs_int32 gid,
				      afs_int32 oid);
extern afs_int32 AddToOrphan(struct ubik_trans *at, afs_int32 gid);
extern afs_int32 RemoveFromOrphan(struct ubik_trans *at, afs_int32 gid);
extern afs_int32 IsOwnerOf(struct ubik_trans *at, afs_int32 aid, afs_int32 gid);
extern afs_int32 OwnerOf(struct ubik_trans *at, afs_int32 gid);
extern afs_int32 IsAMemberOf(struct ubik_trans *at, afs_int32 aid,
			     afs_int32 gid);

/* ptutils.c */
extern afs_int32 AddToEntry(struct ubik_trans *tt, struct prentry *entry,
			    afs_int32 loc, afs_int32 aid);
extern int AccessOK(struct ubik_trans *ut, afs_int32 cid,
		    struct prentry *tentry, int mem, int any);
extern afs_int32 CreateEntry(struct ubik_trans *at, char aname[],
			     afs_int32 *aid, afs_int32 idflag,
			     afs_int32 flag, afs_int32 oid, afs_int32 creator);
extern afs_int32 RemoveFromEntry(struct ubik_trans *at, afs_int32 aid,
				 afs_int32 bid);
extern afs_int32 DeleteEntry(struct ubik_trans *at, struct prentry *tentry,
			     afs_int32 loc);
extern afs_int32 GetList(struct ubik_trans *at, struct prentry *tentry,
			 prlist *alist, afs_int32 add);
extern afs_int32 GetList2(struct ubik_trans *at, struct prentry *tentry,
			  struct prentry *tentry2, prlist *alist,
			  afs_int32 add);
extern afs_int32 GetMax(struct ubik_trans *at, afs_int32 *uid, afs_int32 *gid);
extern afs_int32 SetMax(struct ubik_trans *at, afs_int32 id, afs_int32 flag);
extern afs_int32 ChangeEntry(struct ubik_trans *at, afs_int32 aid,
			     afs_int32 cid, char *name, afs_int32 oid,
			     afs_int32 newid);
extern afs_int32 GetOwnedChain(struct ubik_trans *ut, afs_int32 *next,
			       prlist *alist);
extern afs_int32 AddToPRList(prlist *alist, int *sizeP, afs_int32 id);
extern afs_int32 read_DbHeader(struct ubik_trans *tt);
extern afs_int32 Initdb(void);

/* ptuser.c */

/* All ptuser prototypes are in ptuser.h - for public consumption ... */

#endif
