extern int sendBufSize;
struct client;

extern afs_int32
FetchData_RXStyle(Volume *volptr, 
		  Vnode *targetptr, 
		  register struct rx_call *Call,
		  afs_int32 Pos,
		  afs_int32 Len,
		  afs_int32 Int64Mode,
#if FS_STATS_DETAILED
		  afs_int32 *a_bytesToFetchP,
		  afs_int32 *a_bytesFetchedP
#endif /* FS_STATS_DETAILED */
		  );

extern afs_int32
StoreData_RXStyle(Volume *volptr,
		  Vnode *targetptr,
		  struct AFSFid *Fid,
		  struct client *client,
		  register struct rx_call *Call,
		  afs_uint32 Pos,
		  afs_uint32 Length,
		  afs_uint32 FileLength,
		  int sync,
#if FS_STATS_DETAILED
		  afs_int32 *a_bytesToStoreP,
		  afs_int32 *a_bytesStoredP
#endif /* FS_STATS_DETAILED */
		  );
