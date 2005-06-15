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
#include "ifs_rpc.h"
#include "afsrdr.h"
#else
#include "ifs_rpc.h"
#endif
#include "kif.h"

/* general internal functions */
rpc_t *rpc_create(int size_hint);
void rpc_destroy(rpc_t *rpc);
int rpc_marshal_long(rpc_t *rpc, ULONG data);
int rpc_marshal_longlong(rpc_t *rpc, LARGE_INTEGER data);
int rpc_marshal_wstr(rpc_t *rpc, WCHAR *str);
int rpc_unmarshal_long(rpc_t *rpc, ULONG *data);
int rpc_unmarshal_longlong(rpc_t *rpc, LARGE_INTEGER *data);
int rpc_unmarshal_wstr(rpc_t *rpc, WCHAR *str);

/* kernel-queue specific internal functions */
#ifdef RPC_KERN
int rpc_queue(rpc_t *rpc);
rpc_queue_bulk(rpc_t *rpc, char *out_bulk, ULONG out_len, char *in_bulk, ULONG in_len);
rpc_cancel(rpc_t *rpc);
rpc_send_reg(rpc_t *rpc, char *out_buf);
rpc_queue_bulk_mdl(rpc_t *rpc, MDL *mdl);
rpc_t *rpc_find(int id);
rpc_t *rpc_upgrade(rpc_t *rpc, int old_status, int new_status);
rpc_wait(rpc_t *rpc, BOOLEAN long_op);
rpc_send_mdl(rpc_t *rpc, char *out_buf);
#endif

/* internal timing functions (not used) */
#ifdef RPC_KERN
#define TIMING_START()		LARGE_INTEGER start, stop; \
							start.QuadPart = KeQueryInterruptTime();
#define TIMING_END(name)	stop.QuadPart = KeQueryInterruptTime(); \
							rpt5((name, "%s %d", name, (ULONG)(stop.QuadPart - start.QuadPart)));
#endif


/* rpc security kernel functions */
#ifdef RPC_KERN
struct rpc_cred_map_entry
	{
	void *token;
	PETHREAD thread;
	};

struct rpc_cred_map_entry cred_map[20];
rpc_t *rpc_list_head = NULL;

rpc_set_context(void *context)
	{
	int x, empty, ret;
	PETHREAD thd;

	thd = PsGetCurrentThread();
	empty = -1;
	ret = 0;

	// LOCKLOCK
	for (x = 0; x < 20; x++)
		{
		if (cred_map[x].thread == NULL)
			empty = x;
		if (cred_map[x].thread == thd)
			{
			cred_map[x].token = context;
			goto done;
			}
		}
	if (empty != -1)
		{
		cred_map[empty].thread = thd;
		cred_map[empty].token = context;
		}
	else
		ret = -1;

	done:
		// UNLOCKUNLOCK
		return ret;
	}

void *rpc_get_context()
	{
	int x;
	PETHREAD thd;

	thd = PsGetCurrentThread();

	// no lock
	for (x = 0; x < 20; x++)
		if (cred_map[x].thread == thd)
			return cred_map[x].token;
	// no unlock
	return NULL;
	}

rpc_remove_context()
	{
	int x;
	PETHREAD thd;

	thd = PsGetCurrentThread();
	// no lock
	for (x = 0; x < 20; x++)
		if (cred_map[x].thread == thd)
			{
			cred_map[x].token = NULL;
			cred_map[x].thread = NULL;
			return 0;
			}

	// no unlock
	return -1;
	}
#endif


/* rpc internal functions for kernel */
#ifdef RPC_KERN
rpc_t *rpc_create(int size_hint)
	{
	ULONG size;
	rpc_t *rpc;
	SECURITY_SUBJECT_CONTEXT subj_context;
	PACCESS_TOKEN acc_token;
	LUID auth_id;
	LARGE_INTEGER user_id;
	NTSTATUS status;
	HANDLE token;

	token = rpc_get_context();
	ASSERT(token);
	status = SeQueryAuthenticationIdToken(token, &auth_id);

	user_id.LowPart = auth_id.LowPart;
	user_id.HighPart = auth_id.HighPart;

	ifs_lock_rpcs();

	if (!(rpc = rpc_upgrade(NULL, 0, 1)))
		{
		size = sizeof(rpc_t) + 4096*10;
		rpc = ExAllocatePoolWithTag(NonPagedPool, size, 0x1234);
		if (!rpc)
			_asm int 3;
		memset(rpc, 0, size);
		rpc->next = rpc_list_head;
		rpc_list_head = rpc;
		rpc_upgrade(rpc, 0, 1);
		}

	rpc->out_buf = rpc->out_pos = (char*)(rpc+1);
	rpc->in_buf = rpc->in_pos = ((char*)(rpc+1))+2048*10;

	rpc->key = rand() + 10;
	rpc_marshal_long(rpc, rpc->key);
	rpc->bulk_out_len = (ULONG*)rpc->out_pos;
	rpc_marshal_long(rpc, 0);

	/*SeCaptureSubjectContext(&subj_context);
	acc_token = SeQuerySubjectContextToken(&subj_context);
	status = SeQueryAuthenticationIdToken(acc_token, &auth_id);*/
	/**token = rpc_get_context();
	ASSERT(token);
	status = SeQueryAuthenticationIdToken(token, &auth_id);

	user_id.LowPart = auth_id.LowPart;
	user_id.HighPart = auth_id.HighPart;
	SeReleaseSubjectContext(&subj_context);*/

	rpc_marshal_longlong(rpc, user_id);

	ifs_unlock_rpcs();

	return rpc;
	}

void rpc_destroy(rpc_t *rpc)
	{
	rpc_t *curr;
	int count;

	/*ExFreePoolWithTag(rpc, 0x1234);*/
	ifs_lock_rpcs();

	if (rpc_upgrade(rpc, -1, 0))
		;

	ifs_unlock_rpcs();
	}
#endif


/* rpc internal functions for usermode */
#ifndef RPC_KERN
rpc_t *rpc_create(int size_hint)
	{
	ULONG size;
	rpc_t *rpc;
	ULONG status;

	size = sizeof(rpc_t) + 4096;
	rpc = malloc(size);
	if (!rpc)
		_asm int 3;
	memset(rpc, 0, size);

	rpc->out_buf = rpc->out_pos = (char*)(rpc+1);
	rpc->in_buf = rpc->in_pos = ((char*)(rpc+1))+2048;

	rpc->key = rand() + 10;
	rpc_marshal_long(rpc, rpc->key);

	return rpc;
	}

void rpc_destroy(rpc_t *rpc)
	{
	if (!rpc)
		return;
		//_asm int 3;

	free(rpc);
	}

rpc_transact(rpc_t *rpc)
	{
	HANDLE hf;
	int ret;
	ULONG header_len;
	DWORD err, read = 0;

	if (!rpc)
		return IFSL_GENERIC_FAILURE;

	header_len = rpc->out_pos - rpc->out_buf;

	read = 2048;
	return ifs_TransactRpc(rpc->out_buf, header_len, rpc->in_buf, &read);
	}
#endif


/* upcall stubs */
#ifdef RPC_KERN
uc_namei(WCHAR *name, ULONG *fid)
	{
	rpc_t *rpc;
	ULONG status;
	MDL *mdl;
	TIMING_START();
	/* put namei cache here */

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_NAMEI);
	rpc_marshal_long(rpc, wcslen(name));

	rpc_queue_bulk(rpc, (void*)name, (wcslen(name)+1)*sizeof(wchar_t), NULL, 0);

	if (!rpc_wait(rpc, RPC_TIMEOUT_SHORT))
		{
		rpc_cancel(rpc);
		rpt0(("cancel", "cancel namei"));
		return IFSL_RPC_TIMEOUT;
		}

	rpc_unmarshal_long(rpc, &status);
	rpc_unmarshal_long(rpc, fid);

	rpc_destroy(rpc);
	TIMING_END("namei");
	return status;
	}

uc_check_access(ULONG fid, ULONG access, ULONG *granted)
	{
	rpc_t *rpc;
	ULONG status;
	TIMING_START();

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_CHECK_ACCESS);
	rpc_marshal_long(rpc, fid);
	rpc_marshal_long(rpc, access);

	rpc_queue(rpc);
	if (!rpc_wait(rpc, RPC_TIMEOUT_SHORT))
		{
		rpc_cancel(rpc);
		rpt0(("cancel", "cancel access"));
		return IFSL_RPC_TIMEOUT;
		}

	rpc_unmarshal_long(rpc, &status);
	rpc_unmarshal_long(rpc, granted);

	rpc_destroy(rpc);
	TIMING_END("access");
	return status;
	}

uc_create(WCHAR *name, ULONG attribs, LARGE_INTEGER alloc, ULONG access, ULONG *granted, ULONG *fid)
	{
	rpc_t *rpc;
	ULONG status;
	TIMING_START();

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_CREATE);
	rpc_marshal_long(rpc, attribs);
	rpc_marshal_longlong(rpc, alloc);
	rpc_marshal_long(rpc, access);

	rpc_queue_bulk(rpc, (void*)name, (wcslen(name)+1)*sizeof(wchar_t), NULL, 0);
	if (!rpc_wait(rpc, RPC_TIMEOUT_SHORT))
		{
		rpc_cancel(rpc);
		rpt0(("cancel", "cancel create"));
		return IFSL_RPC_TIMEOUT;
		}

	rpc_unmarshal_long(rpc, &status);
	rpc_unmarshal_long(rpc, granted);
	rpc_unmarshal_long(rpc, fid);

	rpc_destroy(rpc);
	TIMING_END("create");
	return status;
	}

uc_stat(ULONG fid, ULONG *attribs, LARGE_INTEGER *size, LARGE_INTEGER *creation, LARGE_INTEGER *access, LARGE_INTEGER *change, LARGE_INTEGER *written)
	{
	rpc_t *rpc;
	ULONG status;
	TIMING_START();

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_STAT);
	rpc_marshal_long(rpc, fid);

	rpc_queue(rpc);
	if (!rpc_wait(rpc, RPC_TIMEOUT_SHORT))
		{
		rpc_cancel(rpc);
		rpt0(("cancel", "cancel stat"));
		return IFSL_RPC_TIMEOUT;
		}

	rpc_unmarshal_long(rpc, &status);
	rpc_unmarshal_long(rpc, attribs);
	rpc_unmarshal_longlong(rpc, size);
	rpc_unmarshal_longlong(rpc, creation);
	rpc_unmarshal_longlong(rpc, access);
	rpc_unmarshal_longlong(rpc, change);
	rpc_unmarshal_longlong(rpc, written);

	rpc_destroy(rpc);
	TIMING_END("stat");
	return status;
	}

uc_setinfo(ULONG fid, ULONG attribs, LARGE_INTEGER creation, LARGE_INTEGER access, LARGE_INTEGER change, LARGE_INTEGER written)
	{
	rpc_t *rpc;
	ULONG status;
	TIMING_START();

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_SETINFO);
	rpc_marshal_long(rpc, fid);
	rpc_marshal_long(rpc, attribs);
	rpc_marshal_longlong(rpc, creation);
	rpc_marshal_longlong(rpc, access);
	rpc_marshal_longlong(rpc, change);
	rpc_marshal_longlong(rpc, written);


	rpc_queue(rpc);
	if (!rpc_wait(rpc, RPC_TIMEOUT_SHORT))
		{
		rpc_cancel(rpc);
		rpt0(("cancel", "cancel setinfo"));
		return IFSL_RPC_TIMEOUT;
		}

	rpc_unmarshal_long(rpc, &status);

	rpc_destroy(rpc);
	TIMING_END("setinfo");
	return status;
	}

uc_trunc(ULONG fid, LARGE_INTEGER size)
	{
	rpc_t *rpc;
	ULONG status;
	TIMING_START();

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_TRUNC);
	rpc_marshal_long(rpc, fid);
	rpc_marshal_longlong(rpc, size);

	rpc_queue(rpc);
	if (!rpc_wait(rpc, RPC_TIMEOUT_SHORT))
		{
		rpc_cancel(rpc);
		rpt0(("cancel", "cancel trunc"));
		return IFSL_RPC_TIMEOUT;
		}

	rpc_unmarshal_long(rpc, &status);

	rpc_destroy(rpc);
	TIMING_END("trunc");
	return status;
	}

uc_read(ULONG fid, LARGE_INTEGER offset, ULONG length, ULONG *read, char *data)
	{
	rpc_t *rpc;
	ULONG status;
	TIMING_START();

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_READ);
	rpc_marshal_long(rpc, fid);
	rpc_marshal_longlong(rpc, offset);
	rpc_marshal_long(rpc, length);

	rpc_queue_bulk(rpc, NULL, 0, data, length);
	if (!rpc_wait(rpc, RPC_TIMEOUT_LONG))
		{
		rpc_cancel(rpc);
		rpt0(("cancel", "cancel read"));
		return IFSL_RPC_TIMEOUT;
		}

	rpc_unmarshal_long(rpc, &status);
	rpc_unmarshal_long(rpc, read);

	rpc_destroy(rpc);
	TIMING_END("read");
	return status;
	}

/*uc_read_mdl(ULONG fid, LARGE_INTEGER offset, ULONG length, ULONG *read, MDL *data)
	{
	rpc_t *rpc;
	ULONG status;
	TIMING_START();

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_READ_BULK);
	rpc_marshal_long(rpc, fid);
	rpc_marshal_longlong(rpc, offset);
	rpc_marshal_long(rpc, length);

	rpc_queue_bulk_mdl(rpc, data);
	if (!rpc_wait(rpc, 1))
		{
		rpc_cancel(rpc);
		rpt0(("cancel", "cancel read mdl"));
		return IFSL_RPC_TIMEOUT;
		}

	rpc_unmarshal_long(rpc, &status);
	rpc_unmarshal_long(rpc, read);

	rpc_destroy(rpc);
	TIMING_END("read_mdl");
	return status;
	}*/

uc_write(ULONG fid, LARGE_INTEGER offset, ULONG length, ULONG *written, char *data)
	{
	rpc_t *rpc;
	ULONG status;
	TIMING_START();

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_WRITE);
	rpc_marshal_long(rpc, fid);
	rpc_marshal_longlong(rpc, offset);
	rpc_marshal_long(rpc, length);

	rpc_queue_bulk(rpc, data, length, NULL, 0);
	if (!rpc_wait(rpc, RPC_TIMEOUT_LONG))
		{
		rpc_cancel(rpc);
		rpt0(("cancel", "cancel write"));
		return IFSL_RPC_TIMEOUT;
		}

	rpc_unmarshal_long(rpc, &status);
	rpc_unmarshal_long(rpc, written);

	rpc_destroy(rpc);
	TIMING_END("write");
	return status;
	}

/*uc_write_mdl(ULONG fid, LARGE_INTEGER offset, ULONG length, ULONG *written, MDL *data)
	{
	rpc_t *rpc;
	ULONG status;
	TIMING_START();

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_WRITE_BULK);
	rpc_marshal_long(rpc, fid);
	rpc_marshal_longlong(rpc, offset);
	rpc_marshal_long(rpc, length);

	rpc_queue_bulk_mdl(rpc, data);
	if (!rpc_wait(rpc, 1))
		{
		rpc_cancel(rpc);
		rpt0(("cancel", "cancel write mdl"));
		return IFSL_RPC_TIMEOUT;
		}

	rpc_unmarshal_long(rpc, &status);
	rpc_unmarshal_long(rpc, written);

	rpc_destroy(rpc);
	TIMING_END("write_mdl");
	return status;
	}*/

uc_readdir(ULONG fid, LARGE_INTEGER cookie_in, WCHAR *filter, ULONG *count, char *data, ULONG *len)
	{
	rpc_t *rpc;
	ULONG status;
	TIMING_START();

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_READDIR);
	rpc_marshal_long(rpc, fid);
	rpc_marshal_longlong(rpc, cookie_in);
	rpc_marshal_wstr(rpc, filter);
	rpc_marshal_long(rpc, *len);

	rpc_queue_bulk(rpc, NULL, 0, data, *len);
	if (!rpc_wait(rpc, RPC_TIMEOUT_SHORT))
		{
		rpc_cancel(rpc);
		rpt0(("cancel", "cancel readdir"));
		return IFSL_RPC_TIMEOUT;
		}

	rpc_unmarshal_long(rpc, &status);
	rpc_unmarshal_long(rpc, count);
	rpc_unmarshal_long(rpc, len);

	rpc_destroy(rpc);
	TIMING_END("readdir");
	return status;
	}

uc_close(ULONG fid)
	{
	rpc_t *rpc;
	ULONG status;
	TIMING_START();

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_CLOSE);
	rpc_marshal_long(rpc, fid);

	rpc_queue(rpc);
	if (!rpc_wait(rpc, RPC_TIMEOUT_LONG))
		{
		rpc_cancel(rpc);
		rpt0(("cancel", "cancel close"));
		return IFSL_RPC_TIMEOUT;
		}

	rpc_unmarshal_long(rpc, &status);

	rpc_destroy(rpc);
	TIMING_END("close");
	return status;
	}

uc_unlink(WCHAR *name)
	{
	rpc_t *rpc;
	ULONG status;
	TIMING_START();

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_UNLINK);
	rpc_marshal_wstr(rpc, name);

	rpc_queue(rpc);
	if (!rpc_wait(rpc, RPC_TIMEOUT_SHORT))
		{
		rpc_cancel(rpc);
		rpt0(("cancel", "cancel unlink"));
		return IFSL_RPC_TIMEOUT;
		}

	rpc_unmarshal_long(rpc, &status);

	rpc_destroy(rpc);
	TIMING_END("unlink");
	return status;
	}

uc_ioctl_write(ULONG length, char *data, ULONG *key)
	{
	rpc_t *rpc;
	ULONG status;
	TIMING_START();

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_IOCTL_WRITE);
	rpc_marshal_long(rpc, length);

	rpc_queue_bulk(rpc, data, length, NULL, 0);
	if (!rpc_wait(rpc, RPC_TIMEOUT_SHORT))
		{
		rpc_cancel(rpc);
		rpt0(("cancel", "cancel ioctl write"));
		return IFSL_RPC_TIMEOUT;
		}

	rpc_unmarshal_long(rpc, &status);
	rpc_unmarshal_long(rpc, key);

	rpc_destroy(rpc);
	TIMING_END("ioctl_write");
	return status;
	}

uc_ioctl_read(ULONG key, ULONG *length, char *data)
	{
	rpc_t *rpc;
	ULONG status;
	TIMING_START();

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_IOCTL_READ);
	rpc_marshal_long(rpc, key);

	rpc_queue_bulk(rpc, NULL, 0, data, *length);
	if (!rpc_wait(rpc, RPC_TIMEOUT_SHORT))
		{
		rpc_cancel(rpc);
		rpt0(("cancel", "cancel ioctl read"));
		return IFSL_RPC_TIMEOUT;
		}

	rpc_unmarshal_long(rpc, &status);
	rpc_unmarshal_long(rpc, length);

	rpc_destroy(rpc);
	TIMING_END("ioctl_read");
	return status;
	}

uc_rename(ULONG fid, WCHAR *curr, WCHAR *new_dir, WCHAR *new_name, ULONG *new_fid)
	{
	rpc_t *rpc;
	ULONG status;
	TIMING_START();

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_RENAME);
	rpc_marshal_long(rpc, fid);
	rpc_marshal_wstr(rpc, curr);
	rpc_marshal_wstr(rpc, new_dir);
	rpc_marshal_wstr(rpc, new_name);

	rpc_queue(rpc);
	if (!rpc_wait(rpc, RPC_TIMEOUT_SHORT))
		{
		rpc_cancel(rpc);
		rpt0(("cancel", "cancel rename"));
		return IFSL_RPC_TIMEOUT;
		}

	rpc_unmarshal_long(rpc, &status);
	rpc_unmarshal_long(rpc, new_fid);

	rpc_destroy(rpc);
	TIMING_END("rename");
	return status;
	}
#endif


/* downcall stubs */
#ifndef RPC_KERN
dc_break_callback(ULONG fid)
	{
	rpc_t *rpc;
	ULONG status;

	rpc = rpc_create(0);

	rpc_marshal_long(rpc, RPC_BREAK_CALLBACK);
	rpc_marshal_long(rpc, fid);
	if (!rpc_transact(rpc))
		{
		rpc_destroy(rpc);
		return IFSL_GENERIC_FAILURE;
		}
	rpc_unmarshal_long(rpc, &status);
	rpc_destroy(rpc);
	return status;
	}
#endif


/* rpc packing function */
rpc_marshal_long(rpc_t *rpc, ULONG data)
	{
	memcpy(rpc->out_pos, &data, sizeof(ULONG));
	rpc->out_pos += sizeof(ULONG);
	return 0;
	}

rpc_marshal_longlong(rpc_t *rpc, LARGE_INTEGER data)
	{
	memcpy(rpc->out_pos, &data, sizeof(LARGE_INTEGER));
	rpc->out_pos += sizeof(LARGE_INTEGER);
	return 0;
	}

rpc_marshal_wstr(rpc_t *rpc, WCHAR *str)
	{
	long len;
	len = wcslen(str);
	rpc_marshal_long(rpc, len);
	memcpy(rpc->out_pos, str, len*sizeof(WCHAR));
	rpc->out_pos += len*sizeof(WCHAR);
	return 0;
	}


rpc_unmarshal_long(rpc_t *rpc, ULONG *data)
	{
	memcpy(data, rpc->in_pos, sizeof(ULONG));
	rpc->in_pos += sizeof(ULONG);
	return 0;
	}

rpc_unmarshal_longlong(rpc_t *rpc, LARGE_INTEGER *data)
	{
	memcpy(data, rpc->in_pos, sizeof(LARGE_INTEGER));
	rpc->in_pos += sizeof(LARGE_INTEGER);
	return 0;
	}

rpc_unmarshal_wstr(rpc_t *rpc, WCHAR *str)//, int len)
	{
	long len;
	rpc_unmarshal_long(rpc, &len);
	memcpy(str, rpc->in_pos, len*sizeof(WCHAR));
	rpc->in_pos += len*sizeof(WCHAR);
	str[len] = L'\0';
	return 0;
	}


/* kernel-queue management functions */
#ifdef RPC_KERN
rpc_t *rpc_find(int id)
	{
	rpc_t *curr;

	curr = rpc_list_head;
	while (curr)
		{
		if (curr->key == id && curr->status != 0)
			return curr;
		curr = curr->next;
		}
	return NULL;
	}

rpc_t *rpc_upgrade(rpc_t *rpc, int old_status, int new_status)
	{
	rpc_t *curr;

	if (rpc)
		{
		ASSERT(!old_status || rpc_find(rpc->key));
		if (old_status != -1 && rpc->status != old_status)
			return NULL;
		curr = rpc;
		}
	else
		{
		curr = rpc_list_head;
		while (curr)
			{
			if (old_status == -1 || curr->status == old_status)
				break;
			curr = curr->next;
			}
		}

	if (!curr)
		return NULL;

	ASSERT(old_status == -1 || curr->status == old_status);
	curr->status = new_status;

	return curr;
	}

rpc_queue(rpc_t *rpc)
	{
	int ret;

	ifs_lock_rpcs();

	KeInitializeEvent(&rpc->ev, NotificationEvent, FALSE);
	ret = (rpc_upgrade(rpc, 1, 2) != NULL);
	KeSetEvent(&comExt->outEvent, 0, FALSE);

	ifs_unlock_rpcs();

	return ret;
	}

rpc_cancel(rpc_t *rpc)
	{
	rpc_destroy(rpc);
	//ExAcquireFastMutex(&ext->inLock);
	/*ifs_lock_rpcs();
	if (rpc_upgrade(rpc, -1, 0))
		rpc_destroy(rpc);
	ifs_unlock_rpcs();*/
	//RtlDeleteElementGenericTable(&ext->inTable, (void*)&rpc);
	//ExReleaseFastMutex(&ext->inLock);
	}

rpc_shutdown()
	{
	rpc_t *curr, *next;

	ifs_lock_rpcs();

	curr = rpc_list_head;
	while (curr)
		{
		next = curr->next;
		ExFreePoolWithTag(curr, 0x1234);
		curr = next;
		}
	rpc_list_head = NULL;

	ifs_unlock_rpcs();
	}

rpc_wait(rpc_t *rpc, BOOLEAN long_op)
	{
	NTSTATUS ret;
	LARGE_INTEGER timeout;
	//p->FsContext = (ULONG)&ev;

	if (long_op)
		timeout.QuadPart = -600000000L;		/* 60 seconds 60L*1000000L */
	else
		timeout.QuadPart = -200000000L;		/* 20 seconds 20L*1000000L */

	do
		ret = KeWaitForSingleObject(&rpc->ev, Executive, KernelMode, FALSE, &timeout);
	while (ret != STATUS_SUCCESS);// && ret != STATUS_TIMEOUT);

	/*if (KeReadStateEvent(&rpc->ev) == 0)
		_asm int 3;*/
	if (rpc->status == 5)
		return 0;

	if (ret == STATUS_SUCCESS)
		return 1;
	_asm int 3;
	return 0;
	}

rpc_queue_bulk(rpc_t *rpc, char *out_bulk, ULONG out_len, char *in_bulk, ULONG in_len)
	{
	rpc->bulk_out = out_bulk;
	*rpc->bulk_out_len = out_len;
	rpc->bulk_in = in_bulk;
	rpc->bulk_in_max = in_len;
	return rpc_queue(rpc);
	}

/*rpc_queue_bulk_mdl(rpc_t *rpc, MDL *bulk)
	{
	rpc->bulk_mdl = bulk;
	*rpc->bulk_out_len = 0xFFFFFFFC;
	rpc->bulk_in = 0;
	return rpc_queue(rpc);
	}*/

rpc_get_len(rpc_t *rpc)
	{
	if (*rpc->bulk_out_len != 0xFFFFFFFC)
		return rpc->out_pos - rpc->out_buf + *rpc->bulk_out_len + sizeof(ULONG);
	else
		return rpc->out_pos - rpc->out_buf + sizeof(ULONG);
	}

rpc_send(char *out_buf, int out_len, int *out_written)
	{
	rpc_t *rpc;
	int ret, mdl;
	ULONG header_len;

	restart:

	ifs_lock_rpcs();
	rpc = rpc_upgrade(NULL, 2, 3);

	if (!rpc)
		{
		ifs_unlock_rpcs();
		return 0;
		}

	if (rpc_get_len(rpc) > out_len)
		{
		//_asm int 3;
		ifs_unlock_rpcs();
		rpt0(("cancel", "cancel on send"));
		rpc_upgrade(rpc, -1, 5);
		KeSetEvent(&rpc->ev, IO_NO_INCREMENT, FALSE);	/* move to rpc_ fn */
		//rpc_cancel(rpc);
		goto restart;//return 0;
		}



	/*mdl = (*rpc->bulk_out_len == 0xFFFFFFFC);

	if (mdl)
		*rpc->bulk_out_len = 0;*/

	header_len = rpc->out_pos - rpc->out_buf;
	RtlCopyMemory(out_buf, rpc->out_buf, header_len);

	//if (!mdl)
		{
		if (*rpc->bulk_out_len && rpc->bulk_out)
			RtlCopyMemory(out_buf + header_len, rpc->bulk_out, *rpc->bulk_out_len);
		*out_written = header_len + *rpc->bulk_out_len;
		}
	#if 0
	else
		{
		if (rpc->bulk_mdl)
			{
			void *ptr;
			//_asm int 3;
	#if 0
			try
				{
				_asm int 3;
				MmProbeAndLockPages(rpc->bulk_mdl, UserMode, IoModifyAccess);
				}
			except (EXCEPTION_EXECUTE_HANDLER)
				{
				_asm int 3;
				}
	#endif
			try
				{
				rpc->bulk_out = MmMapLockedPagesSpecifyCache(rpc->bulk_mdl, UserMode, MmNonCached, NULL, FALSE, NormalPagePriority);
				}
			except (EXCEPTION_EXECUTE_HANDLER)
				{
				PMDL remap;
				void *ptr;
				_asm int 3;
				ptr = ExAllocatePool(PagedPool, MmGetMdlByteCount(rpc->bulk_mdl));
				remap = IoAllocateMdl(ptr, MmGetMdlByteCount(rpc->bulk_mdl), FALSE, TRUE, NULL);
				MmProbeAndLockPages(remap, UserMode, IoModifyAccess);
				rpc->bulk_out = MmMapLockedPagesSpecifyCache(remap, KernelMode, MmNonCached, NULL, FALSE, NormalPagePriority);
				_asm int 3;
				rpc->bulk_out = MmGetSystemAddressForMdlSafe(rpc->bulk_mdl, NormalPagePriority);
				MmUnmapLockedPages(rpc->bulk_out, rpc->bulk_mdl);
				rpc->bulk_out = MmMapLockedPagesSpecifyCache(rpc->bulk_mdl, UserMode, MmNonCached, NULL, FALSE, NormalPagePriority);

				remap = IoAllocateMdl(rpc->bulk_out, MmGetMdlByteCount(rpc->bulk_mdl), FALSE, TRUE, NULL);
				remap->Process = (void*)PsGetCurrentProcess();
				MmBuildMdlForNonPagedPool(remap);
				//MmProbeAndLockPages(remap, UserMode, IoModifyAccess);
				rpc->bulk_out = MmMapLockedPagesSpecifyCache(remap, UserMode, MmNonCached, (void*)0x01111111, FALSE, NormalPagePriority);
				/*ifs_unlock_rpcs();
				rpc_upgrade(rpc, -1, 5);
				KeSetEvent(&rpc->ev, IO_NO_INCREMENT, FALSE);
				//rpc_cancel(rpc);
				goto restart;//return 0;*/
				}
			ptr = rpc->bulk_out;
			RtlCopyMemory(out_buf + header_len, &ptr, sizeof(ptr));
			}

		*rpc->bulk_out_len = 0xFFFFFFFC;
		*out_written = header_len + sizeof(void*);
		}
	#endif

	ifs_unlock_rpcs();
	return (*out_written != 0);
	}

#if 0
rpc_send_reg(rpc_t *rpc, char *out_buf)
	{
	ULONG header_len;

	header_len = rpc->out_pos - rpc->out_buf;
	RtlCopyMemory(out_buf, rpc->out_buf, header_len);

	if (rpc->bulk_out_len && rpc->bulk_out)
		RtlCopyMemory(out_buf + header_len, rpc->bulk_out, *rpc->bulk_out_len);
	return header_len + *rpc->bulk_out_len;
	}
#endif
#endif


/* rpc library api */
#ifdef RPC_KERN
rpc_recv(char *in_buf, ULONG len)
	{
	ULONG key, header_size;
	char *alloc;
	rpc_t *rpc;

	ifs_lock_rpcs();

	rpc = rpc_find(*(ULONG*)in_buf);
	if (!rpc)
		{
		//_asm int 3;
		ifs_unlock_rpcs();
		return -1;
		}

	//_asm int 3;
	/*if (*rpc->bulk_out_len == 0xFFFFFFFC)
		{
		ASSERT(rpc->bulk_out);
		MmUnmapLockedPages(rpc->bulk_out, rpc->bulk_mdl);
	//	MmUnlockPages(rpc->bulk_mdl);
		}*/

	alloc = rpc->in_buf;
	rpc->in_buf = rpc->in_pos = in_buf;
	rpc_unmarshal_long(rpc, &key);
	ASSERT(key == rpc->key);
	rpc_unmarshal_long(rpc, &rpc->bulk_in_len);

	rpc->in_buf = rpc->in_pos = alloc;
	header_size = len - rpc->bulk_in_len;
	ASSERT(header_size < 4096);
	RtlCopyMemory(rpc->in_buf, in_buf + 2*sizeof(ULONG), header_size - 2*sizeof(ULONG));
	//if (*rpc->bulk_out_len != 0xFFFFFFFC)
		{
		if (rpc->bulk_in_len && rpc->bulk_in)
			{
			ASSERT(rpc->bulk_in_len <= rpc->bulk_in_max);
			//_asm int 3;
			RtlCopyMemory(rpc->bulk_in, in_buf + header_size, rpc->bulk_in_len);//len - header_size - 2*sizeof(ULONG));
			}
		}

	KeSetEvent(&rpc->ev, IO_NO_INCREMENT, FALSE);
	ifs_unlock_rpcs();
	return 0;
	}

rpc_call(ULONG in_len, char *in_buf, ULONG out_max, char *out_buf, ULONG *out_len)
	{
	long rpc_code;
	ULONG status;
	WCHAR name[1024];
	ULONG key, fid;
	LARGE_INTEGER user_id;
	rpc_t rpc;

	rpc.in_buf = rpc.in_pos = in_buf;
	rpc.out_buf = rpc.out_pos = out_buf;

	rpc_unmarshal_long(&rpc, &key);
	rpc_unmarshal_long(&rpc, &rpc_code);

	switch (rpc_code)
		{
		case RPC_BREAK_CALLBACK:
			rpc_unmarshal_long(&rpc, &fid);
			status = dc_break_callback(fid);
			rpc_marshal_long(&rpc, status);
			break;
		}
	*out_len = rpc.out_pos - rpc.out_buf;
	return 0;
	//ifs_ImpersonateClient(user_id);
	}
#endif

#ifndef RPC_KERN
rpc_parse(rpc_t *rpc)
	{
	long rpc_code;
	ULONG status;
	WCHAR name[1024];
	ULONG key;
	LARGE_INTEGER user_id;

	rpc_unmarshal_long(rpc, &key);
	rpc_unmarshal_long(rpc, &rpc->bulk_in_len);
	rpc_unmarshal_longlong(rpc, &user_id);
	rpc_unmarshal_long(rpc, &rpc_code);

	ifs_ImpersonateClient(user_id);

	rpc_marshal_long(rpc, key);
	rpc->bulk_out_len = (ULONG*)rpc->out_pos;
	rpc_marshal_long(rpc, 0);

	switch (rpc_code)
		{
		case RPC_NAMEI:
			{
			ULONG fid, length;
			char *data;
			//rpc_unmarshal_wstr(rpc, name);
			rpc_unmarshal_long(rpc, &length);
			//data = *((char**)rpc->in_pos);
			data = rpc->in_pos;
			status = uc_namei((WCHAR*)data, &fid);
			//status = uc_namei(name, &fid);
			rpc_marshal_long(rpc, status);		
			rpc_marshal_long(rpc, fid);		
			}
			break;
		case RPC_CHECK_ACCESS:
			{
			ULONG fid, access, granted;
			rpc_unmarshal_long(rpc, &fid);
			rpc_unmarshal_long(rpc, &access);
			status = uc_check_access(fid, access, &granted);
			rpc_marshal_long(rpc, status);		
			rpc_marshal_long(rpc, granted);
			}
			break;
		case RPC_CREATE:
			{
			LARGE_INTEGER alloc;
			ULONG access, granted, fid, attribs;
			char *data;

			rpc_unmarshal_long(rpc, &attribs);
			rpc_unmarshal_longlong(rpc, &alloc);
			rpc_unmarshal_long(rpc, &access);
			//rpc_unmarshal_wstr(rpc, name);
			data = rpc->in_pos;
			status = uc_create((WCHAR*)data, attribs, alloc, access, &granted, &fid);
			rpc_marshal_long(rpc, status);
			rpc_marshal_long(rpc, granted);
			rpc_marshal_long(rpc, fid);
			}
			break;
		case RPC_STAT:
			{
			ULONG fid, attribs;
			LARGE_INTEGER size, creation, access, change, written;
			rpc_unmarshal_long(rpc, &fid);
			status = uc_stat(fid, &attribs, &size, &creation, &access, &change, &written);
			rpc_marshal_long(rpc, status);
			rpc_marshal_long(rpc, attribs);
			rpc_marshal_longlong(rpc, size);
			rpc_marshal_longlong(rpc, creation);
			rpc_marshal_longlong(rpc, access);
			rpc_marshal_longlong(rpc, change);
			rpc_marshal_longlong(rpc, written);
			}
			break;
		case RPC_READ:
			{
			ULONG fid, length, read;
			LARGE_INTEGER offset;
			char *data, *save;
			rpc_unmarshal_long(rpc, &fid);
			rpc_unmarshal_longlong(rpc, &offset);
			rpc_unmarshal_long(rpc, &length);
			save = rpc->out_pos;
			rpc_marshal_long(rpc, 0);
			rpc_marshal_long(rpc, 0);
			data = rpc->out_pos;
			rpc->out_pos = save;
			status = uc_read(fid, offset, length, &read, data);
			rpc_marshal_long(rpc, status);
			rpc_marshal_long(rpc, read);
			rpc->out_pos += read;
			*rpc->bulk_out_len = read;
			}
			break;
	/*	case RPC_READ_BULK:
			{
			ULONG fid, length, read;
			LARGE_INTEGER offset;
			char *data, *save;
			rpc_unmarshal_long(rpc, &fid);
			rpc_unmarshal_longlong(rpc, &offset);
			rpc_unmarshal_long(rpc, &length);
			data = *((char**)rpc->in_pos);
			status = uc_read(fid, offset, length, &read, data);
			rpc_marshal_long(rpc, status);
			rpc_marshal_long(rpc, read);
			*rpc->bulk_out_len = 0;
			}
			break;*/
		case RPC_WRITE:
			{
			ULONG fid, length, written;
			LARGE_INTEGER offset;
			char *data;
			rpc_unmarshal_long(rpc, &fid);
			rpc_unmarshal_longlong(rpc, &offset);
			rpc_unmarshal_long(rpc, &length);
			data = rpc->in_pos;
			status = uc_write(fid, offset, length, &written, data);
			rpc_marshal_long(rpc, status);
			rpc_marshal_long(rpc, written);
			}
			break;
	/*	case RPC_WRITE_BULK:
			{
			ULONG fid, length, written;
			LARGE_INTEGER offset;
			char *data;
			//_asm int 3;
			rpc_unmarshal_long(rpc, &fid);
			rpc_unmarshal_longlong(rpc, &offset);
			rpc_unmarshal_long(rpc, &length);
			data = *((char**)rpc->in_pos);
			status = uc_write(fid, offset, length, &written, data);
			rpc_marshal_long(rpc, status);
			rpc_marshal_long(rpc, written);
			}
			break;*/
		case RPC_TRUNC:
			{
			ULONG fid;
			LARGE_INTEGER size;
			rpc_unmarshal_long(rpc, &fid);
			rpc_unmarshal_longlong(rpc, &size);
			status = uc_trunc(fid, size);
			rpc_marshal_long(rpc, status);
			}
			break;
		case RPC_SETINFO:
			{
			ULONG fid, attribs;
			LARGE_INTEGER creation, access, change, written;
			rpc_unmarshal_long(rpc, &fid);
			rpc_unmarshal_long(rpc, &attribs);
			rpc_unmarshal_longlong(rpc, &creation);
			rpc_unmarshal_longlong(rpc, &access);
			rpc_unmarshal_longlong(rpc, &change);
			rpc_unmarshal_longlong(rpc, &written);
			status = uc_setinfo(fid, attribs, creation, access, change, written);
			rpc_marshal_long(rpc, status);
			}
			break;
		case RPC_READDIR:
			{
			ULONG fid, count, len;
			LARGE_INTEGER cookie_in;
			char *data, *save;
			rpc_unmarshal_long(rpc, &fid);
			rpc_unmarshal_longlong(rpc, &cookie_in);
			rpc_unmarshal_wstr(rpc, name);
			rpc_unmarshal_long(rpc, &len);
			save = rpc->out_pos;
			rpc_marshal_long(rpc, 0);
			rpc_marshal_long(rpc, 0);
			rpc_marshal_long(rpc, 0);
			data = rpc->out_pos;
			rpc->out_pos = save;
			status = uc_readdir(fid, cookie_in, name, &count, data, &len);
			rpc_marshal_long(rpc, status);
			rpc_marshal_long(rpc, count);
			rpc_marshal_long(rpc, len);
			rpc->out_pos += len;
			*rpc->bulk_out_len = len;
			}
			break;
		case RPC_CLOSE:
			{
			ULONG fid;
			rpc_unmarshal_long(rpc, &fid);
			status = uc_close(fid);
			rpc_marshal_long(rpc, status);
			}
			break;
		case RPC_UNLINK:
			{
			ULONG fid, unlink;
			rpc_unmarshal_wstr(rpc, name);
			status = uc_unlink(name);
			rpc_marshal_long(rpc, status);
			}
			break;
		case RPC_IOCTL_WRITE:
			{
			ULONG length, key;
			rpc_unmarshal_long(rpc, &length);
			status = uc_ioctl_write(length, rpc->in_pos, &key);
			rpc_marshal_long(rpc, status);
			rpc_marshal_long(rpc, key);
			}
			break;
		case RPC_IOCTL_READ:
			{
			ULONG key, length;
			char *save, *data;
			rpc_unmarshal_long(rpc, &key);
			save = rpc->out_pos;
			rpc_marshal_long(rpc, 0);
			rpc_marshal_long(rpc, 0);
			data = rpc->out_pos;
			rpc->out_pos = save;
			status = uc_ioctl_read(key, &length, data);
			rpc_marshal_long(rpc, status);
			rpc_marshal_long(rpc, length);
			rpc->out_pos += length;
			*rpc->bulk_out_len = length;
			}
			break;
		case RPC_RENAME:
			{
			ULONG fid, new_fid;
			WCHAR curr[1024], new_dir[1024], new_name[1024];
			rpc_unmarshal_long(rpc, &fid);
			rpc_unmarshal_wstr(rpc, curr);
			rpc_unmarshal_wstr(rpc, new_dir);
			rpc_unmarshal_wstr(rpc, new_name);
			status = uc_rename(fid, curr, new_dir, new_name, &new_fid);
			rpc_marshal_long(rpc, status);
			rpc_marshal_long(rpc, new_fid);
			}
			break;
		}
	}
#endif
