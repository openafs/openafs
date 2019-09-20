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

#include <afsconfig.h>
#include <afs/param.h>

#include <errno.h>
#include <string.h>

#include <tests/tap/basic.h>
#include <afs/opr.h>

#include <stdlib.h>
#include <time.h>

struct {
    char *key;
    int key_len;

    char *val;
    int val_len;
} items[] = {

#define TCASE(key, len, val) { (key), (len), (val), sizeof(val)-1 }

    TCASE("foo\0\0", 6, "one"),
    TCASE("bar\0\0", 6, "two"),
    TCASE("baz\0\0", 6, "three"),
    TCASE("quux\0",  6, "four"),
    TCASE("pants",   6, "five"),

    TCASE("foo\0\0", 4, "six"),
    TCASE("bar\0\0", 4, "seven"),
    TCASE("baz\0\0", 4, "eight"),
    TCASE("quux\0",  5, "nine"),

    TCASE("foo1\0", 6, "ten"),
    TCASE("bar1\0", 6, "eleven"),
    TCASE("baz1\0", 6, "twelve"),
    TCASE("quux1",  6, "thirteen"),

    TCASE("f\xf3\x0a", 5, "value \x01"),
    TCASE("ba\xffr", 5, "\x01\x02\x03"),
    TCASE("ba\xffz", 5, "\0\0\0\0"),

#undef TCASE

};
static const int n_items = sizeof(items)/sizeof(items[0]);

static void
run_seed(int seed)
{
    struct opr_cache_opts opts;
    struct opr_cache *cache = NULL;
    int item_i;
    int missing;
    int code;
    char val[1024];
    size_t val_len;

    srand(seed);

    val_len = sizeof(val);
    code = opr_cache_get(cache, NULL, 0, val, &val_len);
    is_int(ENOENT, code,
       "Looking up in a NULL cache fails with ENOENT");

    opr_cache_put(cache, NULL, 0, NULL, 0);
    ok(1,
       "Storing in a NULL cache does nothing");

    memset(&opts, 0, sizeof(opts));

    opts.n_buckets = 2;
    opts.max_entries = 100;
    ok(opr_cache_init(&opts, &cache) != 0,
       "Initializing a cache with a tiny n_buckets fails");

    opts.n_buckets = 0x40000000;
    ok(opr_cache_init(&opts, &cache) != 0,
       "Initializing a cache with a huge n_buckets fails");

    opts.n_buckets = 23;
    ok(opr_cache_init(&opts, &cache) != 0,
       "Initializing a cache with non-power-of-2 n_buckets fails");

    opts.n_buckets = 64;
    opts.max_entries = 1;
    ok(opr_cache_init(&opts, &cache) != 0,
       "Initializing a cache with a tiny max_entries fails");

    opts.max_entries = 0x7fffffff;
    ok(opr_cache_init(&opts, &cache) != 0,
       "Initializing a cache with a huge max_entries fails");

    opts.n_buckets = 8;
    opts.max_entries = 12;

    code = opr_cache_init(&opts, &cache);
    is_int(0, code,
       "Initializing a reasonably-sized cache succeeds");

    ok(cache != NULL,
       "Initializing a cache gives us a cache");

    for (item_i = 0; item_i < n_items; item_i++) {
	val_len = sizeof(val);
	code = opr_cache_get(cache, items[item_i].key,
			     items[item_i].key_len,
			     val, &val_len);
	is_int(ENOENT, code,
	   "[item %d] Looking up in an empty cache fails with ENOENT", item_i);
    }

    for (item_i = 0; item_i < 12; item_i++) {
	opr_cache_put(cache, items[item_i].key, items[item_i].key_len,
		      items[item_i].val, items[item_i].val_len);
    }
    ok(1, "Cache filled successfully");

    for (item_i = 0; item_i < 12; item_i++) {
	val_len = sizeof(val);
	code = opr_cache_get(cache, items[item_i].key, items[item_i].key_len,
			     val, &val_len);
	is_int(0, code, "[item %d] Lookup succeeds", item_i);
	is_int(items[item_i].val_len, val_len,
	   "[item %d] Lookup returns correct val_len %d",
	   item_i, items[item_i].val_len);

	ok(memcmp(val, items[item_i].val, val_len) == 0,
	   "[item %d] Lookup returns correct val", item_i);
    }

    val_len = sizeof(val);
    code = opr_cache_get(cache, NULL, 5, val, &val_len);
    is_int(ENOENT, code,
	"Looking up NULL key fails with ENOENT");

    code = opr_cache_get(cache, val, 0, val, &val_len);
    is_int(ENOENT, code,
	"Looking up 0-length key fails with ENOENT");

    opr_cache_put(cache, NULL, 0, val, val_len);
    opr_cache_put(cache, NULL, 5, val, val_len);
    opr_cache_put(cache, val, 0, val, val_len);
    opr_cache_put(cache, val, val_len, NULL, 0);
    opr_cache_put(cache, val, val_len, NULL, 5);
    opr_cache_put(cache, val, val_len, val, 0);
    opr_cache_put(cache, NULL, 0, NULL, 0);
    opr_cache_put(cache, NULL, 5, NULL, 5);
    opr_cache_put(cache, val, 0, val, 0);
    ok(1, "Storing NULL/0-length entries does nothing");

    code = opr_cache_get(cache, "123", 3, val, &val_len);
    is_int(ENOENT, code, "Cache lookup fails for nonexistent item");

    memcpy(val, "replace", 7);
    val_len = 7;
    opr_cache_put(cache, items[0].key, items[0].key_len, val, val_len);
    ok(1, "Replacement store succeeds");

    val_len = 1;
    code = opr_cache_get(cache, items[0].key, items[0].key_len, val, &val_len);
    is_int(ENOSPC, code, "Small lookup returns ENOSPC");

    val_len = sizeof(val);
    code = opr_cache_get(cache, items[0].key, items[0].key_len, val, &val_len);
    is_int(0, code, "Replacement lookup succeeds");
    is_int(7, val_len, "Lookup trims val_len");
    ok(memcmp(val, "replace", 7) == 0,
	"Replacement lookup returns correct value");

    /* Set items[0] back to the original value. */
    opr_cache_put(cache, items[0].key, items[0].key_len, items[0].val,
		  items[0].val_len);

    for (item_i = 12; item_i < n_items; item_i++) {
	opr_cache_put(cache, items[item_i].key, items[item_i].key_len,
		      items[item_i].val, items[item_i].val_len);
    }
    ok(1, "[seed %d] Cache over-filled successfully", seed);

    missing = 0;
    for (item_i = 0; item_i < n_items; item_i++) {
	val_len = sizeof(val);
	code = opr_cache_get(cache, items[item_i].key, items[item_i].key_len,
			     val, &val_len);
	if (code == ENOENT) {
	    missing++;
	    continue;
	}
	is_int(0, code, "[item %d] Lookup succeeds", item_i);
	is_int(items[item_i].val_len, val_len,
	   "[item %d] Lookup returns correct val_len %d",
	   item_i, items[item_i].val_len);

	ok(memcmp(val, items[item_i].val, val_len) == 0,
	    "[item %d] Lookup returns correct val", item_i);
    }

    is_int(4, missing,
	"[seed %d] Cache lookup fails for %d items", seed, missing);

    opr_cache_free(&cache);
    ok(1, "Cache free succeeds");
    ok(cache == NULL, "Cache free NULLs arg");

    opr_cache_free(&cache);
    ok(1, "Double-free is noop");
    ok(cache == NULL, "Cache is still NULL after double-free");
}

int
main(void)
{
    int seed;

    plan(113 * 32);

    for (seed = 0; seed < 32; seed++) {
	run_seed(seed);
    }

    return 0;
}
