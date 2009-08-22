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


#include <stdio.h>
#if !defined(AFS_NAMEI_ENV)
main()
{
    printf("fb64 not required for this operating system.\n");
    exit(1);
}
#else

#include "afsutil.h"

char *prog = "fb64";

void
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

void btoi(int ac, char **av);
void itob(int ac, char **av);
void check(int ac, char **av);
void verifyRange(int ac, char **av);


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

    exit(0);
}

void
btoi(int ac, char **av)
{
    int i;
    int64_t o;

    if (ac == 3) {
	printf("%Lu\n", flipbase64_to_int64(av[2]));
    } else {
	for (i = 2; i < ac; i++)
	    printf("%s: %Lu\n", av[i], flipbase64_to_int64(av[i]));
    }
}

void
itob(int ac, char **av)
{
    int i;
    lb64_string_t str;
    int64_t in;

    if (ac == 3) {
	sscanf(av[2], "%Ld", &in);
	printf("%s\n", int64_to_flipbase64(str, in));
    } else {
	for (i = 2; i < ac; i++) {
	    sscanf(av[i], "%Ld", &in);
	    printf("%Ld: %s\n", in, int64_to_flipbase64(str, in));
	}
    }
}

void
check(int ac, char **av)
{
    int i;
    int64_t in, out;
    lb64_string_t str;

    printf("%10s %10s %10s\n", "input", "base64", "output");
    for (i = 2; i < ac; i++) {
	sscanf(av[i], "%Ld", &in);
	(void)int64_to_flipbase64(str, in);
	out = flipbase64_to_int64(str);
	printf("%10Ld %10s %10Ld\n", in, str, out);
    }
}

#define PROGRESS 1000000
void
verifyRange(int ac, char **av)
{
    u_int64_t inc, low, high;
    u_int64_t n;
    int64_t in, out;
    lb64_string_t str;

    if (ac != 5)
	Usage();

    sscanf(av[2], "%lu", &low);
    sscanf(av[3], "%lu", &high);
    sscanf(av[4], "%lu", &inc);

    n = 0;
    for (in = low; in <= high; in += inc) {
	n++;
	if (n % PROGRESS == 0)
	    printf(" L%d", n);
	(void)int64_to_flipbase64(str, in);
	out = flipbase64_to_int64(str);
	if (in != out) {
	    printf("\n\nERROR: in=%Lu, str='%s', out=%Lu\n", in, str, out);
	    exit(1);
	}
    }
    printf("\nCOMPLETE - no errors found in range %Lu,%Lu,%Lu\n", low, high,
	   inc);
}


#endif /* AFS_NAMEI_ENV */
