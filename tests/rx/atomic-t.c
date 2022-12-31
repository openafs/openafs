/*
 * Copyright (c) 2022 Sine Nomine Associates. All rights reserved.
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

#include <roken.h>
#include <pthread.h>

#include <tests/tap/basic.h>

#include <afs/opr.h>
#include <rx/rx_atomic.h>

struct thread_args {
    rx_atomic_t atom;
    int min_val;
    int max_val;
};

/* This runs various atomic ops using the starting value of 'val', and results
 * in setting various values in the range [val-20,val+20]. */
static void
test_simple(int val)
{
    rx_atomic_t atom = RX_ATOMIC_INIT(val);

    diag("init val %d", val);

    is_int(val, rx_atomic_read(&atom), "rx_atomic_read -> %d", val);

    val += 5;
    rx_atomic_set(&atom, val);
    is_int(val, rx_atomic_read(&atom), "rx_atomic_set -> %d", val);

    val++;
    rx_atomic_inc(&atom);
    is_int(val, rx_atomic_read(&atom), "rx_atomic_inc -> %d", val);

    val++;
    is_int(val, rx_atomic_inc_and_read(&atom), "rx_atomic_inc_and_read -> %d", val);

    val += 3;
    rx_atomic_add(&atom, 3);
    is_int(val, rx_atomic_read(&atom), "rx_atomic_add -> %d", val);

    val += 10;
    is_int(val, rx_atomic_add_and_read(&atom, 10), "rx_atomic_add_and_read -> %d", val);

    val--;
    rx_atomic_dec(&atom);
    is_int(val, rx_atomic_read(&atom), "rx_atomic_dec -> %d", val);

    val--;
    is_int(val, rx_atomic_dec_and_read(&atom), "rx_atomic_dec_and_read -> %d", val);

    val -= 3;
    rx_atomic_sub(&atom, 3);
    is_int(val, rx_atomic_read(&atom), "rx_atomic_sub -> %d", val);

    /* Get back to 20 below the initial value. */
    val -= 35;
    rx_atomic_sub(&atom, 35);
    is_int(val, rx_atomic_read(&atom), "rx_atomic_sub -> %d", val);
}

static void *
atom_thread(void *rock)
{
#define I_REPS 1000
#define J_REPS 1000

    struct thread_args *args = rock;
    rx_atomic_t *atom = &args->atom;
    int i, j;
    int val;
    const char *last_op;

    for (i = 0; i < I_REPS; i++) {
	last_op = "rx_atomic_inc/dec";
	for (j = 0; j < J_REPS; j++) {
	    rx_atomic_inc(atom);
	    rx_atomic_dec(atom);
	}

	val = rx_atomic_read(atom);
	if (val < args->min_val || val > args->max_val) {
	    goto fail;
	}

	for (j = 0; j < J_REPS; j++) {
	    val = rx_atomic_inc_and_read(atom);
	    if (val < args->min_val || val > args->max_val) {
		last_op = "rx_atomic_inc_and_read";
		goto fail;
	    }

	    val = rx_atomic_dec_and_read(atom);
	    if (val < args->min_val || val > args->max_val) {
		last_op = "rx_atomic_dec_and_read";
		goto fail;
	    }
	}

	last_op = "rx_atomic/add/sub/add_and_read";
	for (j = 0; j < J_REPS; j++) {
	    rx_atomic_add(atom, 2);
	    rx_atomic_sub(atom, 4);
	    val = rx_atomic_add_and_read(atom, 2);
	    if (val < args->min_val || val > args->max_val) {
		goto fail;
	    }
	}
    }

    return rock;

 fail:
    printf("# bad value after %s: %d (min %d, max %d)\n",
	   last_op, val, args->min_val, args->max_val);
    return NULL;
}

static void
test_threads(int val)
{
#define N_THREADS 10
    pthread_t tid[N_THREADS];
    struct thread_args args;
    int thread_i;

    memset(&args, 0, sizeof(args));
    args.min_val = val - N_THREADS * 2;
    args.max_val = val + N_THREADS * 2;

    diag("threaded init val %d", val);

    rx_atomic_set(&args.atom, val);

    for (thread_i = 0; thread_i < N_THREADS; thread_i++) {
	opr_Verify(pthread_create(&tid[thread_i], NULL, atom_thread, &args) == 0);
    }

    for (thread_i = 0; thread_i < N_THREADS; thread_i++) {
	void *ret;
	opr_Verify(pthread_join(tid[thread_i], &ret) == 0);

	ok(ret != NULL, "atom thread %d/%d", thread_i, N_THREADS - 1);
    }

    is_int(val, rx_atomic_read(&args.atom), "final atomic value");
}

int
main(void)
{
    plan(61);

    test_simple(0);

    /* Pick a couple of values that will cause us to cross from positive to
     * negative, and negative to positive, for at least one operation. */
    test_simple(14);
    test_simple(-14);

    /* Pick a couple of large values somewhat close to positive/negative 2^31,
     * but not close enough that we overflow. */
    test_simple(2147483600);
    test_simple(-2147483600);

    test_threads(0);

    return 0;
}

