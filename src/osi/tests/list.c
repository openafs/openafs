/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */


#include <osi/osi.h>
#include <osi/osi_list.h>
#include <osi/osi_object_cache.h>
#include <osi/osi_counter.h>

struct test_node {
    osi_list_element master_list;
    osi_list_element node_list;
    osi_counter32_val_t sequence_id;
    osi_uint32 index;
};


osi_static osi_result
test_list_package(void);
osi_static osi_result
test_list_package_init(void);
osi_static osi_result
test_list_package_fini(void);
osi_static osi_result
test_list_package_body(void);

osi_static osi_result
node_alloc(struct test_node **);
osi_static osi_result
node_free(struct test_node *);

osi_static int
node_ctor(void * buf, void * sdata, int flags);
osi_static void
node_dtor(void * buf, void * sdata);


osi_list_head master_list;
osi_list_head node_list;
osi_list_head temp_list;

osi_uint32 node_list_len;
osi_uint32 temp_list_len;

osi_mem_object_cache_t * mycache = osi_NULL;

osi_counter32_t sequence_counter;


int main(int argc, char ** argv)
{
    int code = 0;

    osi_Assert(OSI_RESULT_OK(osi_PkgInit(osi_ProgramType_TestSuite, osi_NULL)));

    if (OSI_RESULT_FAIL(test_list_package())) {
	code = 1;
    }

    return code;
}

osi_static int
node_ctor(void * buf, void * sdata, int flags)
{
    struct test_node * node = buf;

    osi_counter32_inc_nv(&sequence_counter,
			 &node->sequence_id);
    osi_list_Append(&master_list,
		    node,
		    struct test_node,
		    master_list);

    return 0;
}

osi_static void
node_dtor(void * buf, void * sdata)
{
    struct test_node * node = buf;

    osi_list_Remove(node,
		    struct test_node,
		    master_list);
}

osi_static osi_result
node_alloc(struct test_node ** node_out)
{
    osi_result res = OSI_OK;
    struct test_node * node;

    *node_out = node = osi_mem_object_cache_alloc(mycache);
    if (osi_compiler_expect_false(node == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
    }

    return res;
}

osi_static osi_result
node_free(struct test_node * node)
{
    osi_mem_object_cache_free(mycache, node);
    return OSI_OK;
}

#define IDX_FIRST 0
#define IDX_LAST  9999

#define CHECKING(x) (osi_Msg x " test: ")
#define PASS        (osi_Msg "pass\n")
#define FAIL        \
    osi_Macro_Begin \
        (osi_Msg "fail\n"); \
        goto fail; \
    osi_Macro_End

osi_static osi_result
test_list_package_body(void)
{
    osi_result res;
    struct test_node * node, * rn, *first, *last;
    osi_uint32 i, prev_idx;

    CHECKING("empty list");
    if (osi_list_IsNotEmpty(&node_list)) {
	FAIL;
    }
    PASS;

    CHECKING("append");
    for (i = IDX_FIRST; i <= IDX_LAST; i++) {
	res = node_alloc(&node);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    (osi_Msg "memory allocation failure\n");
	    goto done;
	}
	node->index = i;

	osi_list_Append(&node_list,
			node,
			struct test_node,
			node_list);

	rn = osi_list_Last(&node_list,
			   struct test_node,
			   node_list);
	if (rn != node) {
	    FAIL;
	}
    }
    PASS;

    CHECKING("prepend");
    for (i = IDX_LAST+1; i <= IDX_LAST+100; i++) {
	res = node_alloc(&node);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    (osi_Msg "memory allocation failure\n");
	    goto done;
	}
	node->index = i;

	osi_list_Prepend(&temp_list,
			 node,
			 struct test_node,
			 node_list);

	rn = osi_list_First(&temp_list,
			    struct test_node,
			    node_list);
	if (rn != node) {
	    FAIL;
	}
    }
    PASS;

    CHECKING("first element");
    first = osi_list_First(&node_list,
			   struct test_node,
			   node_list);
    if (first->index != IDX_FIRST) {
	FAIL;
    }
    PASS;

    CHECKING("last element");
    last = osi_list_Last(&node_list,
			 struct test_node,
			 node_list);
    if (last->index != IDX_LAST) {
	FAIL;
    }
    PASS;

    CHECKING("empty list");
    if (osi_list_IsEmpty(&node_list)) {
	FAIL;
    }
    PASS;

    CHECKING("count");
    osi_list_Count(&node_list,
		   node,
		   struct test_node,
		   node_list,
		   i);
    if (i != ((IDX_LAST - IDX_FIRST)+1)) {
	FAIL;
    }
    PASS;

    CHECKING("forward ordering");
    prev_idx = IDX_FIRST-1;
    for (osi_list_Scan_Immutable(&node_list,
				 node,
				 struct test_node,
				 node_list)) {
	if (node->index != prev_idx+1) {
	    FAIL;
	}
	prev_idx = node->index;
    }
    PASS;

    CHECKING("backwards ordering");
    prev_idx = IDX_LAST+1;
    for (osi_list_ScanBackwards_Immutable(&node_list,
					  node,
					  struct test_node,
					  node_list)) {
	if (node->index != prev_idx-1) {
	    FAIL;
	}
	prev_idx = node->index;
    }
    PASS;

    CHECKING("remove last");
    osi_list_Remove(last,
		    struct test_node,
		    node_list);
    if (osi_list_IsOnList(last,
			  struct test_node,
			  node_list)) {
	FAIL;
    }
    rn = osi_list_Last(&node_list,
		       struct test_node,
		       node_list);
    if (rn == last) {
	FAIL;
    }
    if (rn->index != (last->index - 1)) {
	FAIL;
    }
    osi_list_Append(&node_list,
		    last,
		    struct test_node,
		    node_list);
    rn = osi_list_Last(&node_list,
		       struct test_node,
		       node_list);
    if (rn != last) {
	FAIL;
    }
    PASS;

    CHECKING("remove first");
    osi_list_Remove(first,
		    struct test_node,
		    node_list);
    if (osi_list_IsOnList(first,
			  struct test_node,
			  node_list)) {
	FAIL;
    }
    rn = osi_list_First(&node_list,
			struct test_node,
			node_list);
    if (rn == first) {
	FAIL;
    }
    if (rn->index != (first->index + 1)) {
	FAIL;
    }
    osi_list_Prepend(&node_list,
		     first,
		     struct test_node,
		     node_list);
    rn = osi_list_First(&node_list,
			struct test_node,
			node_list);
    if (rn != first) {
	FAIL;
    }
    PASS;

    CHECKING("reshuffle");
    for (osi_list_Scan(&node_list,
		       node, rn,
		       struct test_node,
		       node_list)) {
	if (!(node->index % 7)) {
	    osi_list_Remove(node,
			    struct test_node,
			    node_list);
	    osi_list_Append(&temp_list,
			    node,
			    struct test_node,
			    node_list);
	}
    }
    for (osi_list_Scan(&temp_list,
		       node, rn,
		       struct test_node,
		       node_list)) {
	if (node->index % 7) {
	    osi_list_Remove(node,
			    struct test_node,
			    node_list);
	    osi_list_Append(&node_list,
			    node,
			    struct test_node,
			    node_list);
	}
    }

    for (osi_list_Scan_Immutable(&node_list,
				 node,
				 struct test_node,
				 node_list)) {
	if (!(node->index % 7)) {
	    FAIL;
	}
    }
    for (osi_list_Scan_Immutable(&temp_list,
				 node,
				 struct test_node,
				 node_list)) {
	if (node->index % 7) {
	    FAIL;
	}
    }
    PASS;

    CHECKING("reshuffle count");
    osi_list_Count(&node_list,
		   node,
		   struct test_node,
		   node_list,
		   node_list_len);
    osi_list_Count(&temp_list,
		   node,
		   struct test_node,
		   node_list,
		   temp_list_len);
    if ((node_list_len + temp_list_len) !=
	(IDX_LAST - IDX_FIRST) + 101) {
	FAIL;
    }
    PASS;

    CHECKING("bulk remove");
    for (osi_list_Scan(&node_list,
		       node, rn,
		       struct test_node,
		       node_list)) {
	osi_list_Remove(node,
			struct test_node,
			node_list);
	(void)node_free(node);
    }
    for (osi_list_Scan(&temp_list,
		       node, rn,
		       struct test_node,
		       node_list)) {
	osi_list_Remove(node,
			struct test_node,
			node_list);
	(void)node_free(node);
    }

    if (osi_list_IsNotEmpty(&node_list)) {
	FAIL;
    }
    if (osi_list_IsNotEmpty(&temp_list)) {
	FAIL;
    }
    PASS;

 done:
    return res;

 fail:
    res = OSI_FAIL;
    goto done;
}


osi_static osi_result
test_list_package(void)
{
    osi_result res;

    res = test_list_package_init();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto fail;
    }

    res = test_list_package_body();

    (void)test_list_package_fini();

 fail:
    return res;
}

osi_static osi_result
test_list_package_init(void)
{
    osi_result res = OSI_OK;

    osi_list_Init(&master_list);
    osi_list_Init(&node_list);
    osi_list_Init(&temp_list);

    osi_counter32_init(&sequence_counter, 0);

    mycache =
	osi_mem_object_cache_create("node_list_cache",
				    sizeof(struct test_node),
				    0,
				    osi_NULL,
				    &node_ctor,
				    &node_dtor,
				    osi_NULL,
				    osi_NULL);
    if (osi_compiler_expect_false(mycache == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto fail;
    }

 fail:
    return res;
}

osi_static osi_result
test_list_package_fini(void)
{
    osi_mem_object_cache_destroy(mycache);

    osi_counter32_destroy(&sequence_counter);

    return OSI_OK;
}
