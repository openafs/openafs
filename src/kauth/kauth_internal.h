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

static_inline unsigned char *
EncryptionKey_to_cblock(EncryptionKey *key) {
    return (unsigned char *)key;
}

static_inline struct ktc_encryptionKey *
EncryptionKey_to_ktc(EncryptionKey *key) {
    return (struct ktc_encryptionKey *)key;
}

static_inline EncryptionKey *
ktc_to_EncryptionKey(struct ktc_encryptionKey *key) {
    return (EncryptionKey *)key;
}

/*
 * Some of the RPCs need to verify that two times are within a given
 * skew window (usually KTC_TIME_UNCERTAINTY, 15 minutes).  However,
 * there are multiple sources of timestamps.  The "AFS-native" type,
 * Date, is afs_uint32; timestamps fetched from the system will
 * generally be the output of time(NULL), i.e., time_t.  However, the
 * base type of time_t is platform-dependent -- on some systems, it
 * is int32_t, and on others it is int64_t.  Arithmetic operations
 * and comparisons between integers of different type are subject to
 * the usual arithmetic promotions in C -- comparing a uint32_t and
 * an int32_t results in the int32_t being "promoted" to uint32_t, which
 * has disasterous consequences when the value being promoted is
 * negative.  If, on the other hand, time_t is int64_t, then the promotions
 * work the other way, with everything evaluated at int64_t precision,
 * since int64_t has a higher conversion rank than int32_t (which has
 * the same conversion rank as uint32_t).  In order to properly and
 * portably check the time skew, it is simplest to cast everything to
 * afs_int64 before evaluating any expressions.
 *
 * The expression evaluates to true if the absolute value of the difference
 * between the two time inputs is larger than the skew.
 */
#define check_ka_skew(__t1, __t2, __skew) \
	((afs_int64)(__t1) - (afs_int64)(__skew) > (afs_int64)(__t2) || \
	(afs_int64)(__t2) - (afs_int64)(__skew) > (afs_int64)(__t1))

#endif
