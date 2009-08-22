extern afs_int32 init_kaprocs(const char *, int);
extern afs_int32 InitAuthServ(struct ubik_trans **, int, int *);
extern afs_int32 kamCreateUser(struct rx_call *, char *, char *,
			       EncryptionKey);
extern afs_int32 ChangePassWord(struct rx_call *, char *, char *, ka_CBS *,
				ka_BBS *);
extern afs_int32 kamSetPassword(struct rx_call *, char *, char *, afs_int32,
				EncryptionKey apassword);
extern afs_int32 kamSetFields(struct rx_call *, char *, char *, afs_int32,
			      Date, afs_int32, afs_int32, afs_uint32,
			      afs_int32 spare2);
extern afs_int32 kamDeleteUser(struct rx_call *, char *, char *);
extern afs_int32 kamGetEntry(struct rx_call *, char *, char *, afs_int32,
			     kaentryinfo *);
extern afs_int32 kamListEntry(struct rx_call *, afs_int32, afs_int32 *,
			      afs_int32 *, kaident *);
extern afs_int32 kamGetStats(struct rx_call *, afs_int32, afs_int32 *,
			     kasstats *, kadstats *);
extern afs_int32 kamGetPassword(struct rx_call *, char *, EncryptionKey *);
extern afs_int32 kamGetRandomKey(struct rx_call *, EncryptionKey *);
extern afs_int32 kamDebug(struct rx_call *, afs_int32, int,
			  struct ka_debugInfo *);

