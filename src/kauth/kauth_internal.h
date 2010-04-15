/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */
#ifndef AFS_SRC_KAUTH_INTERNAL_H
#define AFS_SRC_KAUTH_INTERNAL_H

#include <hcrypto/des.h>

/* admin_tools.c */
extern afs_int32 ka_AdminInteractive(int cmd_argc, char *cmd_argv[]);

/* kadatabase.c */
extern void init_kadatabase(int initFlags);

extern afs_int32 ka_LookupKey(struct ubik_trans *tt,
			      char *name, char *inst,
			      afs_int32 *kvno,
			      struct ktc_encryptionKey *key);

struct kaentry;
extern afs_int32 FindBlock(struct ubik_trans *at, char *aname,
			   char *ainstance, afs_int32 *toP,
			   struct kaentry *tentry);

extern afs_int32 ThreadBlock(struct ubik_trans *at, afs_int32 index,
		             struct kaentry *tentry);

extern afs_int32 ka_FillKeyCache(struct ubik_trans *tt);

extern afs_int32 CheckInit(struct ubik_trans *at,
		           int (*db_init) (struct ubik_trans *));

extern afs_int32 AllocBlock(struct ubik_trans *at, struct kaentry *tentry);

extern afs_int32 ka_NewKey(struct ubik_trans *tt, afs_int32 tentryaddr,
		           struct kaentry *tentry,
			   struct ktc_encryptionKey *key);

extern int name_instance_legal(char *name, char *instance);

/* kalog.c */
extern void kalog_Init(void);

/* kaprocs.c */
struct ubik_trans;
extern afs_int32 InitAuthServ(struct ubik_trans **, int, int *);

/* krb_tf.c */
extern afs_int32 krb_write_ticket_file(char *realm);

/* krb_udp.c */
extern afs_int32 init_krb_udp(void);

static_inline DES_cblock *
EncryptionKey_to_cblock(EncryptionKey *key) {
    return (DES_cblock *)key;
}

static_inline struct ktc_encryptionKey *
EncryptionKey_to_ktc(EncryptionKey *key) {
    return (struct ktc_encryptionKey *)key;
}

static_inline EncryptionKey *
ktc_to_EncryptionKey(struct ktc_encryptionKey *key) {
    return (EncryptionKey *)key;
}

#endif
