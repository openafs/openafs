/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _VLSERVER_INTERNAL_H
#define _VLSERVER_INTERNAL_H

/* vlprocs.c */
extern int Init_VLdbase(struct ubik_trans **trans, int locktype, int this_op);

/* vlutils.c */
extern afs_int32 vlwrite(struct ubik_trans *trans, afs_int32 offset,
		         void *buffer, afs_int32 length);
extern afs_int32 vlentrywrite(struct ubik_trans *trans, afs_int32 offset,
			      void *buffer, afs_int32 length);
extern int write_vital_vlheader(struct ubik_trans *trans);
extern afs_int32 readExtents(struct ubik_trans *trans);
extern afs_int32 CheckInit(struct ubik_trans *trans, int builddb);
extern afs_int32 AllocBlock(struct ubik_trans *trans,
			    struct nvlentry *tentry);
extern afs_int32 FindExtentBlock(struct ubik_trans *trans, afsUUID *uuidp,
				 afs_int32 createit, afs_int32 hostslot,
				 struct extentaddr **expp, afs_int32 *basep);
extern afs_int32 FindByID(struct ubik_trans *trans, afs_uint32 volid,
		          afs_int32 voltype, struct nvlentry *tentry,
			  afs_int32 *error);
extern afs_int32 FindByName(struct ubik_trans *trans, char *volname,
			    struct nvlentry *tentry, afs_int32 *error);
extern int EntryIDExists(struct ubik_trans *trans, const afs_uint32 *ids,
			 afs_int32 ids_len, afs_int32 *error);
extern afs_uint32 NextUnusedID(struct ubik_trans *trans, afs_uint32 maxvolid,
			       afs_uint32 bump, afs_int32 *error);
extern int HashNDump(struct ubik_trans *trans, int hashindex);
extern int HashIdDump(struct ubik_trans *trans, int hashindex);
extern int ThreadVLentry(struct ubik_trans *trans, afs_int32 blockindex,
                         struct nvlentry *tentry);
extern int UnthreadVLentry(struct ubik_trans *trans, afs_int32 blockindex,
			 struct nvlentry *aentry);
extern int HashVolid(struct ubik_trans *trans, afs_int32 voltype,
		     afs_int32 blockindex, struct nvlentry *tentry);
extern int UnhashVolid(struct ubik_trans *trans, afs_int32 voltype,
		       afs_int32 blockindex, struct nvlentry *aentry);
extern int HashVolname(struct ubik_trans *trans, afs_int32 blockindex,
		       struct nvlentry *aentry);
extern int UnhashVolname(struct ubik_trans *trans, afs_int32 blockindex,
			 struct nvlentry *aentry);
extern afs_int32 NextEntry(struct ubik_trans *trans, afs_int32 blockindex,
			   struct nvlentry *tentry, afs_int32 *remaining);
extern int FreeBlock(struct ubik_trans *trans, afs_int32 blockindex);
#endif
