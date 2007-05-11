/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_mem.h>
#include <osi/osi_mem_local.h>
#include <osi/osi_cpu.h>
#include <osi/osi_cache.h>
#include <osi/osi_mutex.h>
#include <osi/osi_list.h>
#include <osi/osi_kernel.h>
#include <osi/COMMON/mem_local.h>

/*
 * context-local memory
 * common support
 */

osi_static struct {
    struct {
	int osi_volatile inuse;
	osi_mem_local_destructor_t * osi_volatile dtor;
	osi_uintptr_t osi_volatile offset;
    } keys[OSI_MEM_LOCAL_MAX_KEYS];
    osi_uintptr_t osi_volatile buffer_offset;
#if defined(OSI_ENV_USERSPACE)
    osi_list_head_volatile ctx_list;
#endif
    osi_mutex_t lock;
} osi_mem_local_key_directory;


#ifdef OSI_MEM_LOCAL_CPU_ARRAY
/*
 * context pointer vector indexed by cpu id
 *
 * this should be highly performant because after system bringup,
 * these pointers should never change.  Thus, the cache line(s) 
 * storing the vector should remain in shared mode
 *
 * this also assumes that most OS's keep the cpu id namespace
 * zero-indexed and non-sparse.  This is basically true since
 * many internal OS services rely on this assumption.
 * (ok, so big risc boxes often have "holes" for unfilled sockets
 *  in the topology, but the namespace is still non-sparse enough for us)
 */
struct osi_mem_local_ctx_data ** osi_mem_local_ctx_data = osi_NULL;
#endif


/*
 * create a context-local key
 *
 * [OUT] key  -- address in which to store key
 * [IN] dtor  -- destructor function pointer
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL when key space is exhausted
 */
osi_result 
osi_mem_local_key_create(osi_mem_local_key_t * key,
			 osi_mem_local_destructor_t * dtor)
{
    osi_result res = OSI_OK;
    osi_mem_local_key_t k;

    osi_mutex_Lock(&osi_mem_local_key_directory.lock);

    for (k = 0; k < OSI_MEM_LOCAL_MAX_KEYS; k++) {
	if (osi_mem_local_key_directory.keys[k].inuse == 0) {
	    osi_mem_local_key_directory.keys[k].inuse = 1;
	    osi_mem_local_key_directory.keys[k].dtor = dtor;
	    osi_mem_local_key_directory.keys[k].offset = 0;
	    *key = k;
	    break;
	}
    }

    if (k == OSI_MEM_LOCAL_MAX_KEYS) {
	res = OSI_FAIL;
    }

    osi_mutex_Unlock(&osi_mem_local_key_directory.lock);
    return res;
}


/*
 * key destroy helpers
 */
struct osi_mem_local_key_destruct {
    osi_mem_local_destructor_t * dtor;
    osi_mem_local_key_t key;
};

/*
 * conditionally run the destructor for a key
 *
 * [IN] ldata  -- mem-local context pointer
 * [IN] info   -- destructor info structure
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
osi_mem_local_key_destroy_ctx(struct osi_mem_local_ctx_data * ldata,
			      struct osi_mem_local_key_destruct * info)
{
    if (ldata->keys[info->key]) {
	(*(info->dtor))(ldata->keys[info->key]);
	ldata->keys[info->key] = osi_NULL;
    }
    return OSI_OK;
}

#if defined(OSI_ENV_KERNELSPACE)
osi_static osi_result
osi_mem_local_key_destroy_cpu(osi_cpu_id_t id, void * sdata)
{
    osi_result res;
    struct osi_mem_local_key_destruct * info = (struct osi_mem_local_key_destruct *) sdata;
    struct osi_mem_local_ctx_data * ldata;

    osi_thread_bind_current(id);
    osi_kernel_preemption_disable();
    ldata = osi_mem_local_ctx_get_ctx(id);

    res = osi_mem_local_key_destroy_ctx(ldata, info);

    osi_kernel_preemption_enable();

    return res;
}
#endif /* OSI_ENV_KERNELSPACE */

osi_result
osi_mem_local_key_destroy(osi_mem_local_key_t key)
{
    osi_result res = OSI_OK;
    osi_mem_local_destructor_t * dtor;

    osi_mutex_Lock(&osi_mem_local_key_directory.lock);

    osi_mem_local_key_directory.keys[key].inuse = 0;
    osi_mem_local_key_directory.keys[key].offset = 0;

    dtor = osi_mem_local_key_directory.keys[key].dtor;
    if (dtor) {
	struct osi_mem_local_key_destruct info;
	info.dtor = dtor;
	info.key = key;
#if defined(OSI_ENV_KERNELSPACE)
	osi_cpu_list_iterate(&osi_mem_local_key_destroy_cpu, &info);
	osi_thread_unbind_current();
#else /* !OSI_ENV_KERNELSPACE */
	{
	    struct osi_mem_local_ctx_data *ctx;
	    for (osi_list_Scan_Immutable(&osi_mem_local_key_directory.ctx_list, ctx,
					 struct osi_mem_local_ctx_data, hdr.ctx_list)) {
		osi_mem_local_key_destroy_ctx(ctx, &info);
	    }
	}
#endif /* !OSI_ENV_KERNELSPACE */
    }
    osi_mem_local_key_directory.keys[key].dtor = osi_NULL;
    osi_mutex_Unlock(&osi_mem_local_key_directory.lock);
    return res;
}



/*
 * initialize the per-context memory segment
 *
 * [IN] ctx   -- mem-local context id
 * [OUT] ret  -- address in which to store pointer to new mem-local context 
 *               control structure
 *
 * returns:
 *
 */
osi_result
osi_mem_local_ctx_init(osi_mem_local_ctx_id_t ctx, 
		       struct osi_mem_local_ctx_data ** ret)
{
    struct osi_mem_local_ctx_data * ldata;
    size_t len, align;
    osi_uintptr_t ptr;
    int i;


    /* get the max cache line size so we can 
     * align the per-cpu data structures */
    if (OSI_RESULT_FAIL(osi_cache_max_alignment(&align)) ||
	align < 32) {
	align = 32;
    }

#if defined(OSI_ENV_KERNELSPACE)
    /*
     * attempt to bind to the cpu so kmalloc makes better 
     * NUMA locality decisions
     */
    osi_thread_bind_current(ctx);
#endif

    ldata = (struct osi_mem_local_ctx_data *)
	osi_mem_aligned_alloc_nosleep(sizeof(struct osi_mem_local_ctx_data),
				      align);
    if (osi_compiler_expect_false(ldata == osi_NULL)) {
	return OSI_FAIL;
    }
    osi_mem_zero(ldata, sizeof(struct osi_mem_local_ctx_data));

    osi_mutex_Lock(&osi_mem_local_key_directory.lock);

#if defined(OSI_ENV_USERSPACE)
    osi_list_Append(&osi_mem_local_key_directory.ctx_list, ldata,
		    struct osi_mem_local_ctx_data, hdr.ctx_list);
#endif

    for (i = 0; i < OSI_MEM_LOCAL_MAX_KEYS ; i++) {
	if (osi_mem_local_key_directory.keys[i].inuse &&
	    osi_mem_local_key_directory.keys[i].offset) {
	    ptr = (osi_uintptr_t) ldata;
	    ptr += osi_mem_local_key_directory.keys[i].offset;
	    ldata->keys[i] = (void *) ptr;
	} else {
	    ldata->keys[i] = osi_NULL;
	}
    }

    osi_mutex_Unlock(&osi_mem_local_key_directory.lock);

    osi_mem_local_ctx_set(ldata);

    if (ret) {
	*ret = (void *) ldata;
    }

    return OSI_OK;
}

#if defined(OSI_ENV_KERNELSPACE) && !defined(OSI_MEM_LOCAL_PERCPU_ALLOC)

osi_static osi_result
osi_mem_local_cpu_init(osi_cpu_id_t cpuid, void * sdata)
{
    return osi_mem_local_ctx_init(cpuid, osi_NULL);
}

osi_static osi_result
osi_mem_local_os_alloc_percpu_buffers(void)
{
    osi_result res;
    size_t align;
    osi_cpu_id_t max_id;

    if (OSI_RESULT_FAIL_UNLIKELY(osi_cache_max_alignment(&align))) {
	align = 64;
    }

    if (OSI_RESULT_FAIL_UNLIKELY(osi_cpu_max_id(&max_id))) {
	res = OSI_FAIL;
	goto error;
    }

    osi_mem_local_ctx_data = (struct osi_mem_local_ctx_data **)
	osi_mem_aligned_alloc_nosleep((max_id + 1) * sizeof(struct osi_mem_local_ctx_data *),
				      align);
    if (osi_compiler_expect_false(osi_mem_local_ctx_data == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }
    osi_mem_zero(osi_mem_local_ctx_data, ((max_id + 1) * sizeof(struct osi_mem_local_ctx_data *)));

    res = osi_cpu_list_iterate(&osi_mem_local_cpu_init, osi_NULL);
    osi_thread_unbind_current();

 error:
    return res;
}

#endif /* !OSI_MEM_LOCAL_PERCPU_ALLOC */



/*
 * the following collection of routines deal with
 * allocation in the main per-context buffer
 */

struct osi_mem_local_alloc_info {
    osi_mem_local_key_t key;
    osi_uintptr_t offset;
};

/*
 * allocate space in the main buffer
 * in a specific context
 */
osi_static osi_result
osi_mem_local_alloc_ctx(struct osi_mem_local_ctx_data * ldata,
			struct osi_mem_local_alloc_info * info)
{
    osi_result res = OSI_OK;
    osi_uintptr_t ptr;

    ptr = (osi_uintptr_t) ldata;
    ptr += info->offset;

    ldata->keys[info->key] = (void *) ptr;

    return res;
}

#ifdef OSI_ENV_KERNELSPACE
/*
 * cpu iterator function
 *
 * allocates space in the main buffer
 * on a specific cpu
 */
osi_static osi_result
osi_mem_local_alloc_cpu(osi_cpu_id_t cpuid, void * sdata)
{
    osi_result res = OSI_OK;
    struct osi_mem_local_ctx_data * ldata;
    struct osi_mem_local_alloc_info * info =
	(struct osi_mem_local_alloc_info *) sdata;

    ldata = osi_mem_local_ctx_get_ctx(cpuid);
    return osi_mem_local_alloc_ctx(ldata, info);
}
#endif /* OSI_ENV_KERNELSPACE */

/*
 * allocate space in the main buffer
 * across all contexts
 */
osi_result
osi_mem_local_alloc(osi_mem_local_key_t key, size_t len_in, size_t align_in)
{
    osi_result res = OSI_OK;
    osi_uintptr_t save_offset, new_offset, len, align;

    len = (osi_uintptr_t) len_in;
    align = (osi_uintptr_t) align_in;

    osi_mutex_Lock(&osi_mem_local_key_directory.lock);
    new_offset = save_offset = osi_mem_local_key_directory.buffer_offset;

    if (!align) {
	align = 8;
    }

    if (save_offset & (align-1)) {
	save_offset = (save_offset & ~(align-1)) + align;
    }
    new_offset = save_offset + len;

    if (new_offset <= OSI_MEM_LOCAL_BUFFER_SIZE) {
	osi_mem_local_key_directory.buffer_offset = new_offset;
    } else {
	res = OSI_FAIL;
    }

    osi_mem_local_key_directory.keys[key].offset = save_offset;

    if (OSI_RESULT_OK(res)) {
	struct osi_mem_local_alloc_info info;
	info.key = key;
	info.offset = save_offset;
#if defined(OSI_ENV_KERNELSPACE)
	res = osi_cpu_list_iterate(&osi_mem_local_alloc_cpu, &info);
#else
	{
	    struct osi_mem_local_ctx_data *ctx;
	    for (osi_list_Scan_Immutable(&osi_mem_local_key_directory.ctx_list, ctx,
					 struct osi_mem_local_ctx_data, hdr.ctx_list)) {
		osi_mem_local_alloc_ctx(ctx, &info);
	    }
	}
#endif
    }
    osi_mutex_Unlock(&osi_mem_local_key_directory.lock);

    return res;
}

/*
 * osi_mem_local context lookup
 */
#if defined(OSI_ENV_USERSPACE)
struct osi_mem_local_ctx_data *
osi_mem_local_ctx_get_ctx(osi_mem_local_ctx_id_t ctxid)
{
    struct osi_mem_local_ctx_data *ctx, *ret = osi_NULL;

    osi_mutex_Lock(&osi_mem_local_key_directory.lock);
    for (osi_list_Scan_Immutable(&osi_mem_local_key_directory.ctx_list, ctx,
				 struct osi_mem_local_ctx_data, hdr.ctx_list)) {
	if (ctx->hdr.ctx_id == ctxid) {
	    ret = ctx;
	    break;
	}
    }
    osi_mutex_Unlock(&osi_mem_local_key_directory.lock);
    return ret;
}
#endif /* OSI_ENV_USERSPACE */

/*
 * set the context-local value for a key
 *
 * [IN] key   -- context-local key
 * [IN] data  -- opaque data pointer
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_mem_local_set(osi_mem_local_key_t key,
		  void * data)
{
    osi_mem_local_ctx_data_t * ldata;

    ldata = osi_mem_local_ctx_get();
    ldata->keys[key] = data;
    osi_mem_local_ctx_put();

    return OSI_OK;
}


/*
 * osi_mem_local package
 * intialization routines
 */

OSI_INIT_FUNC_DECL(osi_mem_local_PkgInit)
{
    osi_result res;
    osi_cpu_id_t max_id, i;
    int k;

    res = osi_mem_local_os_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
      return res;
    }

    for (k = 0; k < OSI_MEM_LOCAL_MAX_KEYS; k++) {
	osi_mem_local_key_directory.keys[k].inuse = 0;
	osi_mem_local_key_directory.keys[k].dtor = osi_NULL;
	osi_mem_local_key_directory.keys[k].offset = 0;
    }

    osi_mem_local_key_directory.buffer_offset = OSI_MEM_LOCAL_PAYLOAD_OFFSET;

    osi_mutex_Init(&osi_mem_local_key_directory.lock, 
		   osi_impl_mutex_opts());

#ifdef OSI_ENV_KERNELSPACE
    res = osi_mem_local_os_alloc_percpu_buffers();
#else
    osi_list_Init(&osi_mem_local_key_directory.ctx_list);
#endif

    return res;
}

OSI_FINI_FUNC_DECL(osi_mem_local_PkgShutdown)
{
    osi_result res;

    res = osi_mem_local_os_PkgShutdown();

    return res;
}

