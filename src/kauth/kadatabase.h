/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _KAUTH_KADATABASE_H
#define _KAUTH_KADATABASE_H
extern int kawrite(struct ubik_trans *, afs_int32, char *, afs_int32);
extern int karead(struct ubik_trans *, afs_int32, char *, afs_int32);
extern int update_admin_count(struct ubik_trans *, int);

extern int ka_LookupKvno(struct ubik_trans *, char *, char *, afs_int32,
			 struct ktc_encryptionKey *);

extern afs_int32 AllocBlock(struct ubik_trans *, struct kaentry *);
extern afs_int32 FreeBlock(struct ubik_trans *, afs_int32);

extern afs_int32 FindBlock(struct ubik_trans *, char *, char *, afs_int32 *,
			   struct kaentry *);
extern afs_int32 ThreadBlock(struct ubik_trans *, afs_int32, struct kaentry *);

extern afs_int32 UnthreadBlock(struct ubik_trans *, struct kaentry *);

extern afs_int32 NextBlock(struct ubik_trans *, afs_int32, struct kaentry *,
			   afs_int32 *);

extern afs_int32 ka_DelKey(struct ubik_trans *tt, afs_int32 tentryaddr,
			   struct kaentry *tentry);

extern afs_int32 ka_LookupKvno(struct ubik_trans *tt, char *name,
			       char *inst, afs_int32 kvno,
			       struct ktc_encryptionKey *key);
extern afs_int32 ka_LookupKey(struct ubik_trans *, char *, char *,
			      afs_int32 *, struct ktc_encryptionKey *);

#endif
