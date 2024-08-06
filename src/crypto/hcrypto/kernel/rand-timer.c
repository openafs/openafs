/*
 * Stub rand-timer "implementation" so that rand-fortuna is linkable
 * into the kernel.
 *
 * Contains no copyrightable content.
 */
#include <config.h>

#include <rand.h>
#include "randi.h"

static void
timer_seed(const void *indata, int size)
{
}

static int
timer_bytes(unsigned char *outdata, int size)
{
    return 1;
}

static void
timer_cleanup(void)
{
}

static void
timer_add(const void *indata, int size, double entropi)
{
}

static int
timer_pseudorand(unsigned char *outdata, int size)
{
    return 1;
}

static int
timer_status(void)
{
    return 0;
}

const RAND_METHOD hc_rand_timer_method = {
    .seed       = timer_seed,
    .bytes      = timer_bytes,
    .cleanup    = timer_cleanup,
    .add        = timer_add,
    .pseudorand = timer_pseudorand,
    .status     = timer_status
};

const RAND_METHOD *
RAND_timer_method(void)
{
    return &hc_rand_timer_method;
}
