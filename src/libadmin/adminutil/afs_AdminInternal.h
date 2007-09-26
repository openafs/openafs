/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_ADMIN_INTERNAL_H
#define OPENAFS_ADMIN_INTERNAL_H

#include <afs/param.h>
#include <afs/stds.h>
#include <afs/cellconfig.h>
#include <afs/auth.h>
#include <ubik.h>
#include <pthread.h>

#define BEGIN_MAGIC 0xb0b0b0b0
#define END_MAGIC   0xc0c0c0c0

typedef struct afs_token_handle {
    int begin_magic;
    int is_valid;
    int from_kernel;
    int afs_token_set;
    int kas_token_set;
    int sc_index;
    char cell[MAXCELLCHARS];
    struct ktc_token afs_token;
    struct ktc_token kas_token;
    struct rx_securityClass *afs_sc;
    struct rx_securityClass *afs_encrypt_sc;
    struct rx_securityClass *kas_sc;
    struct ktc_principal client;
    int end_magic;
} afs_token_handle_t, *afs_token_handle_p;

typedef struct afs_cell_handle {
    int begin_magic;
    int is_valid;
    int is_null;
    afs_token_handle_p tokens;
    char working_cell[MAXCELLCHARS];
    struct ubik_client *kas;
    struct ubik_client *pts;
    struct ubik_client *vos;
    int kas_valid;
    int pts_valid;
    int vos_valid;
    int vos_new;
    int end_magic;
} afs_cell_handle_t, *afs_cell_handle_p;

#define CACHED_ITEMS 5

typedef int (*validate_specific_data_func) (void *rpc_specific,
					    afs_status_p st);

typedef int (*destroy_specific_data_func) (void *rpc_specific,
					   afs_status_p st);

typedef int (*get_cached_data_func) (void *rpc_specific, int slot, void *dest,
				     afs_status_p st);

typedef int (*make_rpc_func) (void *rpc_specific, int slot, int *last_item,
			      int *last_item_contains_data, afs_status_p st);


/*
 * the afs_admin_iterator_t structure contains one mutex (named
 * mutex) that should be held while manipulating any of the other
 * elements in the structure.  The only exception to this rule is
 * that it is legale to make rpc's without the mutex being held.
 */

typedef struct afs_admin_iterator {
    int begin_magic;
    int is_valid;
    pthread_mutex_t mutex;	/* hold to manipulate this structure */
    pthread_cond_t add_item;	/* wait for data to be added to the cache */
    pthread_cond_t remove_item;	/* wait for data to be removed from the cache */
    pthread_t bg_worker;	/* thread id of background worker */
    int cache_slots_used;	/* number of items in the cache */
    int done_iterating;		/* finished iterating over the database */
    int request_terminated;	/* Done has been called on this iterator */
    afs_status_t st;		/* the status of the last rpc */
    int cache_queue_head;	/* index into principal that's the head of the q */
    int cache_queue_tail;	/* index into principal that's the tail of the q */
    void *rpc_specific;
    make_rpc_func make_rpc;
    get_cached_data_func get_cached_data;
    validate_specific_data_func validate_specific;
    destroy_specific_data_func destroy_specific;
    int end_magic;
} afs_admin_iterator_t, *afs_admin_iterator_p;

int IteratorNext(afs_admin_iterator_p iter, void *dest, afs_status_p st);

int IteratorDone(afs_admin_iterator_p iter, afs_status_p st);

int IteratorInit(afs_admin_iterator_p iter, void *rpc_specific,
		 make_rpc_func make_rpc, get_cached_data_func get_cached_data,
		 validate_specific_data_func validate_specific_data,
		 destroy_specific_data_func destroy_specific_data,
		 afs_status_p st);
#endif /* OPENAFS_ADMIN_INTERNAL_H */
