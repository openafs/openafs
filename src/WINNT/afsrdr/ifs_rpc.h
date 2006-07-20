/* copyright (c) 2005
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any purpose,
 * so long as the name of the university of michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.  if
 * the above copyright notice or any other identification of the
 * university of michigan is included in any copy of any portion of
 * this software, then the disclaimer below must also be included.
 * 
 * this software is provided as is, without representation from the
 * university of michigan as to its fitness for any purpose, and without
 * warranty by the university of michigan of any kind, either express 
 * or implied, including without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  the regents
 * of the university of michigan shall not be liable for any damages,   
 * including special, indirect, incidental, or consequential damages, 
 * with respect to any claim arising out or in connection with the use
 * of the software, even if it has been or is hereafter advised of the
 * possibility of such damages.
 */

/* versioning history
 * 
 * 03-jun 2005 (eric williams) entered into versioning
 */

#ifdef RPC_KERN
#include <ntifs.h>
#include <stdlib.h>
#else
#include <osi.h>
#include <windows.h>
#include <winioctl.h>
#endif


/* maximum number of users, based on SID, that can access AFS at one time */
#define MAX_AFS_USERS			256

/* maximum number of threads that can make simultaneous calls into afsrdr,
   because we maintain a credential set for each thread in a single table. */
#define MAX_CRED_MAPS			64

/* size if outgoing/incoming RPC buffer (for parameters only, not bulk data) */
#define RPC_BUF_SIZE			2048

/* max. chunk size for RPC transfers */
#define TRANSFER_CHUNK_SIZE	(1024*1024)


/* upcalls */
#define RPC_NAMEI		0x10
#define RPC_CHECK_ACCESS	0x11
#define RPC_CREATE		0x12
#define RPC_STAT		0x13
#define RPC_READ		0x14
#define RPC_WRITE		0x15
#define RPC_TRUNC		0x16
#define RPC_SETINFO		0x17
#define RPC_READDIR		0x18
#define RPC_CLOSE		0x19
#define RPC_UNLINK		0x1A
#define RPC_IOCTL_WRITE		0x1B
#define RPC_IOCTL_READ		0x1C
#define RPC_RENAME		0x1D
#define RPC_FLUSH		0x1E


/* downcalls */
#define RPC_BREAK_CALLBACK	0x80
#define RPC_RELEASE_HOOKS	0x81


/* internal module flags */
#define RPC_TIMEOUT_SHORT	0
#define RPC_TIMEOUT_LONG	1


/* internal data struct for both client and server */
struct rpc
	{
#ifdef RPC_KERN
	struct rpc *next;
	int size;
	KEVENT ev;
	MDL *bulk_mdl;
#endif
	char *bulk_out;
	ULONG *bulk_out_len;
	char *bulk_in;
	ULONG bulk_in_len, bulk_in_max;
	ULONG key;
	char *out_buf, *out_pos;
	char *in_buf, *in_pos;
	int status;
	};
typedef struct rpc rpc_t;


/* application interface into rpc library */
#ifdef RPC_KERN
rpc_call(ULONG in_len, char *in_buf, ULONG out_max, char *out_buf, ULONG *out_len);
rpc_set_context(void *context);
rpc_remove_context();
rpc_get_len(rpc_t *rpc);
rpc_send(char *out_buf, int out_len, int *out_written);
rpc_recv(char *in_buf, ULONG len);
rpc_shutdown();

#else

rpc_parse(rpc_t *rpc);
#endif


