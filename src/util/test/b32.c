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

#if !defined(AFS_NT40_ENV)
int
main(int argc, char *argv[])
{
    printf("b32 not required for this operating system.\n");
    return 1;
}
#else

static char *prog = "b32";

static void
Usage(void)
{
    printf("Usage: %s -s n [n ...] (converts int to base 32 string)\n", prog);
    printf("Usage: %s -i s [s ...] (converts base 32 string to int)\n", prog);
    printf("Usage: %s -c n [n ...] (converts to base 32 and back)\n", prog);
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
main(int ac, char **av)
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

    if (ac == 3)
	printf("%d\n", base32_to_int(av[2]));
    else {
	for (i = 2; i < ac; i++)
	    printf("%s: %d\n", av[i], base32_to_int(av[i]));
    }
}

static void
itob(int ac, char **av)
{
    int i;
    b32_string_t str;

    if (ac == 3)
	printf("%s\n", int_to_base32(str, atoi(av[2])));
    else {
	for (i = 2; i < ac; i++)
	    printf("%d: %s\n", atoi(av[i]), int_to_base32(str, atoi(av[i])));
    }
}

static void
check(int ac, char **av)
{
    int i;
    int in, out;
    b32_string_t str;

    printf("%10s %10s %10s\n", "input", "base32", "output");
    for (i = 2; i < ac; i++) {
	in = atoi(av[i]);
	(void)int_to_base32(str, in);
	out = base32_to_int(str);
	printf("%10d %10s %10d\n", in, str, out);
    }
}

#define PROGRESS 1000000
static void
verifyRange(int ac, char **av)
{
    unsigned int inc, low, high;
    int n;
    unsigned int in, out;
    b32_string_t str;

    if (ac != 5)
	Usage();

    low = (unsigned int)atoi(av[2]);
    high = (unsigned int)atoi(av[3]);
    inc = (unsigned int)atoi(av[4]);

    n = 0;
    for (in = low; in <= high; in += inc) {
	n++;
	if (n % PROGRESS == 0)
	    printf(" %d", n);
	(void)int_to_base32(str, in);
	out = base32_to_int(str);
	if (in != out) {
	    printf("\n\nERROR: in=%u, str='%s', out=%u\n", in, str, out);
	    exit(1);
	}
    }
    printf("\nCOMPLETE - no errors found in range %u,%u,%u\n", low, high,
	   inc);
}

#endif /* !NT */
