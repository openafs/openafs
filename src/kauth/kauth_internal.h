
/* kadatabase.c */
extern void init_kadatabase(int initFlags);

extern afs_int32 ka_LookupKey(struct ubik_trans *tt, 
			      char *name, char *inst, 
			      afs_int32 *kvno,
			      struct ktc_encryptionKey *key);

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
