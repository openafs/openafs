/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>
#include <afs/afsutil.h>

#include <stdio.h>

static char *prog = "fb64";

static void
Usage(void)
{
    printf("Usage: %s -s n [n ...] (converts int to base 64 string)\n", prog);
    printf("Usage: %s -i s [s ...] (converts base 64 string to int)\n", prog);
    printf("Usage: %s -c n [n ...] (converts to base 64 and back)\n", prog);
    printf
	("Usage: %s -r low high inc (verify converter using range and inc)\n",
	 prog);
    exit(1);
}

static void btoi(int ac, char **av);
static void itob(int ac, char **av);
static void check(int ac, char **av);
static void verifyRange(int ac, char **av);

int
main(int ac, char *av[])
{
    if (ac < 3)
	Usage();

    if (!strcmp(av[1], "-s"))
	itob(ac, av);
    else if (!strcmp(av[1], "-i"))
	btoi(ac, av);
    else if (!strcmp(av[1], "-c"))
	check(ac, av);
    else if (!strcmp(av[1], "-r"))
	verifyRange(ac, av);
    else
	Usage();

    return 0;
}

static void
btoi(int ac, char **av)
{
    int i;

    if (ac == 3) {
	printf("%llu\n", flipbase64_to_int64(av[2]));
    } else {
	for (i = 2; i < ac; i++)
	    printf("%s: %llu\n", av[i], flipbase64_to_int64(av[i]));
    }
}

static void
itob(int ac, char **av)
{
    int i;
    lb64_string_t str;
    afs_int64 in;

    if (ac == 3) {
	sscanf(av[2], "%lld", &in);
	printf("%s\n", int64_to_flipbase64(str, in));
    } else {
	for (i = 2; i < ac; i++) {
	    sscanf(av[i], "%lld", &in);
	    printf("%lld: %s\n", in, int64_to_flipbase64(str, in));
	}
    }
}

static void
check(int ac, char **av)
{
    int i;
    afs_int64 in, out;
    lb64_string_t str;

    printf("%10s %10s %10s\n", "input", "base64", "output");
    for (i = 2; i < ac; i++) {
	sscanf(av[i], "%lld", &in);
	(void)int64_to_flipbase64(str, in);
	out = flipbase64_to_int64(str);
	printf("%10lld %10s %10lld\n", in, str, out);
    }
}

#define PROGRESS 1000000
static void
verifyRange(int ac, char **av)
{
    afs_int64 inc, low, high;
    afs_int64 n;
    afs_int64 in, out;
    lb64_string_t str;

    if (ac != 5)
	Usage();

    sscanf(av[2], "%llu", &low);
    sscanf(av[3], "%llu", &high);
    sscanf(av[4], "%llu", &inc);

    n = 0;
    for (in = low; in <= high; in += inc) {
	n++;
	if (n % PROGRESS == 0)
	    printf(" L%lld", n);
	(void)int64_to_flipbase64(str, in);
	out = flipbase64_to_int64(str);
	if (in != out) {
	    printf("\n\nERROR: in=%llu, str='%s', out=%llu\n", in, str, out);
	    exit(1);
	}
    }
    printf("\nCOMPLETE - no errors found in range %llu,%llu,%llu\n", low, high,
	   inc);
}
