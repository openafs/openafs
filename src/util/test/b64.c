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
#if !defined(AFS_NT40_ENV)
main()
{
    printf("b64 not required for this operating system.\n");
    exit(1);
}
#else

#include "afsutil.h"

char *prog = "b64";

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

    if (ac == 3)
	printf("%d\n", base64_to_int(av[2]));
    else {
	for (i = 2; i < ac; i++)
	    printf("%s: %d\n", av[i], base64_to_int(av[i]));
    }
}

void
itob(int ac, char **av)
{
    int i;
    b64_string_t str;

    if (ac == 3)
	printf("%s\n", int_to_base64(str, atoi(av[2])));
    else {
	for (i = 2; i < ac; i++)
	    printf("%d: %s\n", atoi(av[i]), int_to_base64(str, atoi(av[i])));
    }
}

void
check(int ac, char **av)
{
    int i;
    int in, out;
    b64_string_t str;

    printf("%10s %10s %10s\n", "input", "base64", "output");
    for (i = 2; i < ac; i++) {
	in = atoi(av[i]);
	(void)int_to_base64(str, in);
	out = base64_to_int(str);
	printf("%10d %10s %10d\n", in, str, out);
    }
}

#define PROGRESS 1000000
void
verifyRange(int ac, char **av)
{
    unsigned int inc, low, high;
    int n;
    unsigned int in, out;
    b64_string_t str;

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
	(void)int_to_base64(str, in);
	out = base64_to_int(str);
	if (in != out) {
	    printf("\n\nERROR: in=%u, str='%s', out=%u\n", in, str, out);
	    exit(1);
	}
    }
    printf("\nCOMPLETE - no errors found in range %u,%u,%u\n", low, high,
	   inc);
}


#endif /* !NT */
