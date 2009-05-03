#ifndef AFS_TOKEN_PROTOS_H
#define AFS_TOKEN_PROTOS_H

#include <afs/afs_token.h>

#if defined(AFS_NT40_ENV)
struct ClearToken {
	int AuthHandle;
	char HandShakeKey[8];
	int ViceId;
	int BeginTimestamp;
	int EndTimestamp;
};
#endif

#if defined(KERNEL) || defined(AFS_NT40_ENV)
/*
 * Format new-style afs_token using rxkad credentials 
 * as stored in the cache manager.  Caller frees returned memory 
 * (of size bufsize).
 */
int add_afs_token_rxkad_k(
	struct ClearToken *pct,
	char* stp,
	afs_int32 stLen,
	afs_int32 primary_flag,
	pioctl_set_token *a_token);

#else	/* !KERNEL && !AFS_NT40_ENV */
/*
 * Format new-style afs_token using rxkad credentials, 
 * caller frees returned memory (of size bufsize).
 */
int add_afs_token_rxkad(
	afs_int32 viceid,
	struct ktc_token *k_token,
	afs_int32 primary_flag,
	pioctl_set_token *a_token);
#endif /* KERNEL || AFS_NT40_ENV */

int free_afs_token(pioctl_set_token *);

int
add_afs_token_soliton(pioctl_set_token *a_token,
                     afstoken_soliton *at);

#ifndef KERNEL
/*
 * Format new-style afs_token using rxkad credentials, 
 * caller frees returned memory (of size bufsize).
 */
int add_afs_token_rxkad(afs_int32 viceid,
                        struct ktc_token *k_token,
                        afs_int32 primary_flag,
                        pioctl_set_token *a_token /* out */);
#else
/*
 * Format new-style afs_token using rxkad credentials 
 * as stored in the cache manager.  Caller frees returned memory 
 * (of size bufsize).
 */
int add_afs_token_rxkad_k(struct ClearToken *pct,
                          char* stp,
                          afs_int32 stLen,
                          afs_int32 primary_flag,
                          pioctl_set_token *a_token /* out */);
#endif

/*
 * Convert afs_token to XDR-encoded token stream, which is returned
 * in buf (at most of size bufsize).  Caller must pass a sufficiently
 * large buffer.
 */
int
encode_afs_token(pioctl_set_token *a_token,
                 void *buf /* in */,
                 int *bufsize /* inout */);

/*
 * Convert XDR-encoded token stream to an afs_token, which is returned
 * in a_token.	Caller must free token contents.
 */
int
parse_afs_token(void* token_buf, 
                int token_size, 
                pioctl_set_token *a_token);

#if !defined(KERNEL) || defined(UKERNEL)
/* enumerate tokens for use in an unrecognized token types message. */
void
afs_get_tokens_type_msg(pioctl_set_token *atoken, char *msg, int msglen);
#endif

int
afstoken_to_soliton(pioctl_set_token *a_token,
                   int type,
                   afstoken_soliton *at);


/* copy bits of an rxkad token into a ktc_token */
int
afstoken_to_token(pioctl_set_token *a_token,
                  struct ktc_token *ttoken,
                  int ttoksize,
                  int *flags,
                  struct ktc_principal *aclient);

#endif /* AFS_TOKEN_PROTOS_H */
