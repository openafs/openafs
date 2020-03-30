/*
 * Copyright (c) 2019 Sine Nomine Associates. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * opr_cache - A simple general-purpose in-memory cache implementation. Keys
 * and values are simple flat memory buffers (keys are compared with memcmp),
 * and currently the only cache eviction strategy is a semi-random approach. We
 * hash values using opr_jhash_opaque.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>
#include <afs/opr.h>

#include <opr/dict.h>
#include <opr/queue.h>
#include <opr/jhash.h>
#ifdef AFS_PTHREAD_ENV
# include <opr/lock.h>
#else
# include <opr/lockstub.h>
#endif

#define MIN_BUCKETS (4)
#define MAX_BUCKETS (1024*1024)

#define MIN_ENTRIES (4)
#define MAX_ENTRIES (1024*1024*1024)

struct opr_cache {
    /*
     * This lock protects everything in the opr_cache structure. This lock must
     * be held before touching anything in opr_cache, and before calling any of
     * our internal functions (unless we're allocating or freeing the cache; in
     * which case, we had better be the only one touching it!).
     */
    opr_mutex_t lock;

    /* How many items are present in the cache? */
    afs_uint32 n_entries;

    /* The max number of items we can hold in the cache. */
    afs_uint32 max_entries;

    /* Our dictionary holding the cached items. Contains cache_entry structs,
     * linked together via 'link'. */
    struct opr_dict *dict;
};

struct cache_entry {
    struct opr_queue link;

    size_t key_len;
    void *key_buf;

    size_t val_len;
    void *val_buf;
};

static void
free_entry_contents(struct cache_entry *entry)
{
    opr_queue_Remove(&entry->link);

    free(entry->key_buf);
    free(entry->val_buf);

    entry->key_buf = entry->val_buf = NULL;
    entry->key_len = entry->val_len = 0;
}

/**
 * Evict an entry from the cache.
 *
 * @pre cache is full.
 * @pre cache->lock is held.
 *
 * @param[in] cache	The opr_cache to use.
 * @param[out] a_entry	Set to the cache entry that was evicted. Its contents
 *			have been freed; it can be treated like a
 *			newly-allocated cache entry.
 */
static void
evict_entry(struct opr_cache *cache, struct cache_entry **a_entry)
{
    unsigned int safety, bucket;
    struct cache_entry *entry = NULL;

    opr_Assert(cache->dict->size > 0);
    opr_Assert(cache->n_entries > 0);

    /*
     * For cache eviction, we pick an entry somewhat randomly, since random
     * eviction performs fairly well, and is very simple to implement.
     *
     * Our approach slightly differs from pure random eviction: we pick a
     * random hash bucket, and evict the least-recently-used entry in that
     * bucket. If the bucket we picked is empty, just use the next bucket.
     */
    bucket = rand() % cache->dict->size;

    for (safety = 0; safety < cache->dict->size; safety++) {
	struct opr_queue *chain;
	chain = &cache->dict->table[bucket];

	if (!opr_queue_IsEmpty(chain)) {
	    entry = opr_queue_Last(chain, struct cache_entry, link);
	    break;
	}

	bucket++;
	bucket = bucket % cache->dict->size;
    }
    opr_Assert(entry != NULL);

    free_entry_contents(entry);

    *a_entry = entry;
}

/**
 * Alloc a new entry in the cache.
 *
 * @pre cache->lock is held.
 * @pre The given key does not already exist in the cache.
 *
 * @param[in] cache	The opr_cache to use.
 * @param[inout] a_key_buf  The key for the new cache entry. Set to NULL on
 *			    success; the memory is used in the new cache entry,
 *			    and must not be freed or otherwise used by the
 *			    caller.
 * @param[in] key_len	The size of *a_key_buf.
 * @param[out] a_entry	Set to the new cache entry on success. The entry has
 *			been inserted into the cache, and the key has been
 *			populated; it looks identical to a normal entry in the
 *			cache, except the value is set to NULL.
 *
 * @return errno status codes
 */
static int
alloc_entry(struct opr_cache *cache, void **a_key_buf, size_t key_len,
	    struct cache_entry **a_entry)
{
    struct cache_entry *entry = NULL;
    unsigned int hash;

    if (cache->n_entries >= cache->max_entries) {
	/*
	 * The cache is full. "Alloc" an entry by picking an existing entry to
	 * evict, and just re-use that entry.
	 */
	evict_entry(cache, &entry);

    } else {
	entry = calloc(1, sizeof(*entry));
	if (entry == NULL) {
	    return ENOMEM;
	}
	cache->n_entries++;
    }

    entry->key_len = key_len;
    entry->key_buf = *a_key_buf;
    *a_key_buf = NULL;

    hash = opr_jhash_opaque(entry->key_buf, entry->key_len, 0);
    opr_dict_Prepend(cache->dict, hash, &entry->link);

    *a_entry = entry;
    return 0;
}

/**
 * Find a cache entry in the cache.
 *
 * @pre cache->lock is held.
 *
 * @param[in] cache	The opr_cache to use.
 * @param[in] key_buf	The key of the entry to find.
 * @param[in] key_len	The size of 'key_buf'.
 * @param[out] a_entry	The found cache entry.
 *
 * @return errno status codes
 * @retval ENOENT The given key does not exist in the cache.
 */
static int
find_entry(struct opr_cache *cache, void *key_buf, size_t key_len,
	   struct cache_entry **a_entry)
{
    struct opr_queue *cursor;
    unsigned int hash;

    hash = opr_jhash_opaque(key_buf, key_len, 0);

    for (opr_dict_ScanBucket(cache->dict, hash, cursor)) {
	struct cache_entry *entry;
	entry = opr_queue_Entry(cursor, struct cache_entry, link);
	if (key_len != entry->key_len) {
	    continue;
	}
	if (memcmp(key_buf, entry->key_buf, key_len) != 0) {
	    continue;
	}
	opr_dict_Promote(cache->dict, hash, &entry->link);
	*a_entry = entry;
	return 0;
    }

    return ENOENT;
}

/**
 * Fetch an item from the cache.
 *
 * If NULL is given for 'cache' or 'key_buf', or if 0 is given for key_len, we
 * always return ENOENT. Conceptually, we treat a NULL cache as an empty cache,
 * and NULL/0-length keys are not allowed in an opr_cache, so returning ENOENT
 * in these cases is consistent.
 *
 * @param[in] cache	The opr_cache to use.
 * @param[in] key_buf	The key to lookup.
 * @param[in] key_len	The size of 'key_buf'.
 * @param[out] val_buf	On success, where to store the looked-up value.
 * @param[inout] a_val_len  On entry, set to the max size available in
 *			    'val_buf'. On successful return, set to the number
 *			    of bytes copied into 'val_buf'.
 * @return errno status codes
 * @retval 0 success
 * @retval ENOENT key not found
 * @retval ENOSPC a_val_len is too small to store the retrieved value
 */
int
opr_cache_get(struct opr_cache *cache, void *key_buf, size_t key_len,
	      void *val_buf, size_t *a_val_len)
{
    struct cache_entry *entry;
    int code;

    if (cache == NULL || key_buf == NULL || key_len < 1) {
	return ENOENT;
    }

    opr_mutex_enter(&cache->lock);

    code = find_entry(cache, key_buf, key_len, &entry);
    if (code != 0) {
	goto done;
    }

    if (entry->val_len > *a_val_len) {
	code = ENOSPC;
	goto done;
    }

    memcpy(val_buf, entry->val_buf, entry->val_len);
    *a_val_len = entry->val_len;

 done:
    opr_mutex_exit(&cache->lock);
    return code;
}

static void*
memdup(void *src, size_t len)
{
    void *buf = malloc(len);
    if (buf == NULL) {
	return NULL;
    }
    memcpy(buf, src, len);
    return buf;
}

/**
 * Store an item in the cache.
 *
 * This operation is a no-op if NULL is given for 'cache', 'key_buf', or
 * 'val_buf', or if 'key_len' or 'val_len' are 0. Conceptually, a NULL
 * opr_cache represents an empty cache, and NULL/0-length keys and values are
 * not allowed in an opr_cache.
 *
 * @param[in] cache	The opr_cache to use.
 * @param[in] key_buf	The key for the stored value.
 * @param[in] key_len	The size of 'key_buf'.
 * @param[in] val_buf	The value to store.
 * @param[in] val_len	The size of 'val_buf'.
 */
void
opr_cache_put(struct opr_cache *cache, void *key_buf, size_t key_len,
	      void *val_buf, size_t val_len)
{
    int code;
    struct cache_entry *entry;
    void *key_dup = NULL;
    void *val_dup = NULL;

    if (cache == NULL || key_buf == NULL || val_buf == NULL || key_len < 1 ||
	val_len < 1) {
	goto done;
    }

    key_dup = memdup(key_buf, key_len);
    val_dup = memdup(val_buf, val_len);
    if (key_dup == NULL || val_dup == NULL) {
	goto done;
    }

    opr_mutex_enter(&cache->lock);

    code = find_entry(cache, key_buf, key_len, &entry);
    if (code == ENOENT) {
	code = alloc_entry(cache, &key_dup, key_len, &entry);
    }
    if (code != 0) {
	goto done_locked;
    }

    free(entry->val_buf);
    entry->val_buf = val_dup;
    entry->val_len = val_len;
    val_dup = NULL;

 done_locked:
    opr_mutex_exit(&cache->lock);
 done:
    free(key_dup);
    free(val_dup);
}

static_inline int
isPowerOf2(int value)
{
    return (value > 0) && (value & (value - 1)) == 0;
}

static_inline int
nextPowerOf2(int target)
{
    int next = 1;
    /*
     * Make sure we have a reasonable target and cannot overflow; callers
     * should do their own range checks before we get here.
     */
    opr_Assert(target > 0 && target <= 0x40000000);
    while (next < target) {
	next *= 2;
    }
    return next;
}

/**
 * Create a new opr_cache.
 *
 * @param[in] opts  What options to use for the cache.
 * @param[out] a_cache  The newly-allocated cache.
 *
 * @return errno status codes
 * @retval EINVAL   invalid option values
 */
int
opr_cache_init(struct opr_cache_opts *opts, struct opr_cache **a_cache)
{
    struct opr_cache *cache;
    int n_buckets = opts->n_buckets;

    if (n_buckets < MIN_BUCKETS || n_buckets > MAX_BUCKETS) {
	return EINVAL;
    }
    if (opts->max_entries < MIN_ENTRIES || opts->max_entries > MAX_ENTRIES) {
	return EINVAL;
    }

    n_buckets = nextPowerOf2(n_buckets);
    opr_Assert(isPowerOf2(n_buckets));
    opr_Assert(n_buckets >= MIN_BUCKETS);
    opr_Assert(n_buckets <= MAX_BUCKETS);

    cache = calloc(1, sizeof(*cache));
    if (cache == NULL) {
	return ENOMEM;
    }

    opr_mutex_init(&cache->lock);
    cache->max_entries = opts->max_entries;

    cache->dict = opr_dict_Init(n_buckets);
    if (cache->dict == NULL) {
	opr_cache_free(&cache);
	return ENOMEM;
    }

    *a_cache = cache;
    return 0;
}

/**
 * Free an opr_cache.
 *
 * @param[inout] a_cache    The cache to free. Set to NULL on return.
 */
void
opr_cache_free(struct opr_cache **a_cache)
{
    struct opr_cache *cache = *a_cache;
    if (cache == NULL) {
	return;
    }
    *a_cache = NULL;

    if (cache->dict != NULL) {
	int bucket;
	for (bucket = 0; bucket < cache->dict->size; bucket++) {
	    struct opr_queue *cursor, *tmp;
	    for (opr_dict_ScanBucketSafe(cache->dict, bucket, cursor, tmp)) {
		struct cache_entry *entry;
		entry = opr_queue_Entry(cursor, struct cache_entry, link);
		free_entry_contents(entry);
		free(entry);
	    }
	}
	opr_dict_Free(&cache->dict);
    }

    opr_mutex_destroy(&cache->lock);

    free(cache);
}
