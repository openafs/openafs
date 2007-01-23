#ifndef AFS_TOKEN_PROTOS_H
#define AFS_TOKEN_PROTOS_H

#ifdef KERNEL
/*
 * Format new-style afs_token using rxkad credentials 
 * as stored in the cache manager.  Caller frees returned memory 
 * (of size bufsize).
 */
int make_afs_token_rxkad_k(
	char *cell,
	n_clear_token *pct,
	char* stp,
	afs_int32 stLen,
	afs_int32 primary_flag,
	afs_token **a_token /* out */);

#else	/* !KERNEL */
/*
 * Format new-style afs_token using rxkad credentials, 
 * caller frees returned memory (of size bufsize).
 */
int make_afs_token_rxkad(
	char *cell,
	afs_int32 viceid,
	struct ktc_token *k_token,
	afs_int32 primary_flag,
	afs_token **a_token /* out */);
#endif /* !KERNEL */

/*
 * Convert afs_token to XDR-encoded token stream, which is returned
 * in buf (at most of size bufsize).
 */
int encode_afs_token(
	afs_token *a_token,
	void *buf /* in */,
	int *bufsize /* inout */);

/*
 * Converts encoded token stream to an afs_token, which is returned
 * in a_token.  Caller must free.
 */
int parse_afs_token( 
	void* token_buf, 
	int token_size, 
	afs_token **a_token);

/* 
 * Free afs_token variant using XDR logic 
*/
int free_afs_token(
	afs_token *a_token);

#endif /* AFS_TOKEN_PROTOS_H */
