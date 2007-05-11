/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_cache.h>
#include <osi/osi_string.h>
#include <stdlib.h>
#include <unistd.h>


/*
 * support for probing the cache hierarchy parameters
 */

#if defined(OSI_SUN57_ENV)

#ifndef UKERNEL
/*
 * XXX can't include system headers after
 * src/afs/UKERNEL/sysincludes.h because of
 * its putrid #define hacks
 */
#include <libdevinfo.h>
#endif

#if defined(__sparc)

/* 
 * on SPARC, cpu metadata is stored as OpenPROM properties under
 * device nodes beginning with the prefix "SUNW,UltraSPARC".
 * hmm. I wonder if this will work for Fujitsu sparcs...
 */

osi_static di_prom_handle_t handle;

osi_static osi_result
walk_prom_props(di_node_t node)
{
    osi_result res = OSI_OK;
    int * val;

    if (di_prom_prop_lookup_ints(handle, node, "ecache-size", &val) != 1) {
	res = OSI_CACHE_RESULT_FAIL_PROBE;
    } else {
	osi_cache_info.l2_size = (size_t)(val[0]);
	osi_cache_info.l2_probed = 1;
    }

    if (di_prom_prop_lookup_ints(handle, node, "ecache-line-size", &val) != 1) {
	res = OSI_CACHE_RESULT_FAIL_PROBE;
    } else {
	osi_cache_info.l2_line_size = (size_t)(val[0]);
    }

    if (di_prom_prop_lookup_ints(handle, node, "ecache-associativity", &val) != 1) {
	res = OSI_CACHE_RESULT_FAIL_PROBE;
    } else {
	osi_cache_info.l2_associativity = (size_t)(val[0]);
    }

    if (di_prom_prop_lookup_ints(handle, node, "icache-size", &val) != 1) {
	res = OSI_CACHE_RESULT_FAIL_PROBE;
    } else {
	osi_cache_info.l1i_size = (size_t)(val[0]);
	osi_cache_info.l1i_probed = 1;
    }

    if (di_prom_prop_lookup_ints(handle, node, "icache-line-size", &val) != 1) {
	res = OSI_CACHE_RESULT_FAIL_PROBE;
    } else {
	osi_cache_info.l1i_line_size = (size_t)(val[0]);
    }

    if (di_prom_prop_lookup_ints(handle, node, "icache-associativity", &val) != 1) {
	res = OSI_CACHE_RESULT_FAIL_PROBE;
    } else {
	osi_cache_info.l1i_associativity = (size_t)(val[0]);
    }

    if (di_prom_prop_lookup_ints(handle, node, "dcache-size", &val) != 1) {
	res = OSI_CACHE_RESULT_FAIL_PROBE;
    } else {
	osi_cache_info.l1d_size = (size_t)(val[0]);
	osi_cache_info.l1d_probed = 1;
    }

    if (di_prom_prop_lookup_ints(handle, node, "dcache-line-size", &val) != 1) {
	res = OSI_CACHE_RESULT_FAIL_PROBE;
    } else {
	osi_cache_info.l1d_line_size = (size_t)(val[0]);
    }

    if (di_prom_prop_lookup_ints(handle, node, "dcache-associativity", &val) != 1) {
	res = OSI_CACHE_RESULT_FAIL_PROBE;
    } else {
	osi_cache_info.l1d_associativity = (size_t)(val[0]);
    }

    return res;
}

osi_static int
walk_node(di_node_t node, void * arg)
{
    osi_result res;
    char * cpu_root = "SUNW,UltraSPARC";
    char nname[20];
    osi_string_lcpy(nname, di_node_name(node), strlen(cpu_root)+1);

    if (!osi_string_cmp(cpu_root, nname)) {
	res = walk_prom_props(node);
    }

    if (OSI_RESULT_OK(res)) {
	*((osi_result *)arg) = OSI_OK;
	return DI_WALK_TERMINATE;
    } else {
	return DI_WALK_CONTINUE;
    }
}

#elif defined(__x86)

/* 
 * on x86, cpu metadata is stored as properties 
 * under device tree nodes named "cpu" 
 */

osi_static osi_result
walk_props(di_node_t node)
{
    osi_result res = OSI_OK;
    int * val;

    if (di_prop_lookup_ints(DDI_DEV_T_ANY, node, "l2-cache-size", &val) != 1) {
	res = OSI_FAIL;
    } else {
	osi_cache_info.l2_size = (size_t)(val[0]);
	osi_cache_info.l2_probed = 1;
    }

    if (di_prop_lookup_ints(DDI_DEV_T_ANY, node, "l2-cache-line-size", &val) != 1) {
	res = OSI_FAIL;
    } else {
	osi_cache_info.l2_line_size = (size_t)(val[0]);
    }

    if (di_prop_lookup_ints(DDI_DEV_T_ANY, node, "l2-cache-associativity", &val) != 1) {
	res = OSI_FAIL;
    } else {
	osi_cache_info.l2_associativity = (size_t)(val[0]);
    }

    if (di_prop_lookup_ints(DDI_DEV_T_ANY, node, "l1-icache-size", &val) != 1) {
	res = OSI_FAIL;
    } else {
	osi_cache_info.l1i_size = (size_t)(val[0]);
	osi_cache_info.l1i_probed = 1;
    }

    if (di_prop_lookup_ints(DDI_DEV_T_ANY, node, "l1-icache-line-size", &val) != 1) {
	res = OSI_FAIL;
    } else {
	osi_cache_info.l1i_line_size = (size_t)(val[0]);
    }

    if (di_prop_lookup_ints(DDI_DEV_T_ANY, node, "l1-icache-associativity", &val) != 1) {
	res = OSI_FAIL;
    } else {
	osi_cache_info.l1i_associativity = (size_t)(val[0]);
    }

    if (di_prop_lookup_ints(DDI_DEV_T_ANY, node, "l1-dcache-size", &val) != 1) {
	res = OSI_FAIL;
    } else {
	osi_cache_info.l1d_size = (size_t)(val[0]);
	osi_cache_info.l1d_probed = 1;
    }

    if (di_prop_lookup_ints(DDI_DEV_T_ANY, node, "l1-dcache-line-size", &val) != 1) {
	res = OSI_FAIL;
    } else {
	osi_cache_info.l1d_line_size = (size_t)(val[0]);
    }

    if (di_prop_lookup_ints(DDI_DEV_T_ANY, node, "l1-dcache-associativity", &val) != 1) {
	res = OSI_FAIL;
    } else {
	osi_cache_info.l1d_associativity = (size_t)(val[0]);
    }

    return res;
}

osi_static int
walk_node(di_node_t node, void * arg)
{
    osi_result res;

    if (!osi_string_cmp(di_node_name(node), "cpu")) {
	res = walk_props(node);
    }

    if (OSI_RESULT_OK(res)) {
	*((osi_result *)arg) = OSI_OK;
	return DI_WALK_TERMINATE;
    } else {
	return DI_WALK_CONTINUE;
    }
}

#else /* !__sparc && !__x86 */
#error "this solaris port is not yet supported; please file a bug"
#endif /* !__sparc && !__x86 */


OSI_INIT_FUNC_DECL(osi_cache_PkgInit)
{
    osi_result res = OSI_OK, walk_res = OSI_FAIL;
    di_node_t root;

    root = di_init("/", DINFOSUBTREE | DINFOPROP);
    if (root == DI_NODE_NIL) {
	res = OSI_CACHE_RESULT_FAIL_PROBE;
	goto done;
    }

#if defined(__sparc)
    handle = di_prom_init();
    if (handle == DI_PROM_HANDLE_NIL) {
	res = OSI_CACHE_RESULT_FAIL_PROBE;
	goto cleanup;
    }
#endif

    di_walk_node(root, DI_WALK_CLDFIRST, &walk_res, walk_node);
    if (OSI_RESULT_FAIL(walk_res)) {
	res = OSI_CACHE_RESULT_FAIL_PROBE;
    }

#if defined(__sparc)
    di_prom_fini(handle);
#endif

    /* compute the max data alignment necessary to
     * ensure no false sharing on this platform */
    osi_cache_info.max_alignment = 
	MAX(osi_cache_info.l1d_line_size,
	    MAX(osi_cache_info.l2_line_size,
		osi_cache_info.l3_line_size));

 cleanup:
    di_fini(root);
 done:
    return res;
}

#endif /* OSI_SUN57_ENV */

