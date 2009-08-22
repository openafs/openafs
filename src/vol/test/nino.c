/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * translate between inode numbers and contents.
 */
#include <afsconfig.h>
#include <afs/param.h>


#include <stdio.h>
#include <sys/types.h>
#include "util/afsutil.h"

void
Usage(void)
{
    printf("Usage: nino <-i ino> | <-c uniq tag vno> | <-a ino_base64>\n");
    exit(1);
}

void do_contents(int ac, char **av);
void do_ino(int ac, char **av);
void do_inobase64(int ac, char **av);


main(int ac, char **av)
{
    if (ac < 3)
	Usage();

    if (!strcmp("-c", av[1]))
	do_contents(ac, av);
    else if (!strcmp("-i", av[1]))
	do_ino(ac, av);
    else if (!strcmp("-a", av[1]))
	do_inobase64(ac, av);
    else
	Usage();
}

void
do_contents(int ac, char **av)
{
    int64_t ino;
    int64_t vno, tag, uniq;
    int count;
    lb64_string_t str;

    if (ac != 5) {
	printf("Bad argument count for -c option.\n");
	exit(1);
    }

    vno = (int64_t) atoi(av[4]);
    vno &= 0x3ffffff;
    tag = (int64_t) atoi(av[3]);
    tag &= 0x7;
    uniq = (int64_t) atoi(av[2]);

    ino = vno;
    ino |= tag << 26;
    ino |= uniq << 32;
    int64_to_flipbase64(str, ino);

    printf("ino=%Lu, base64=%s\n", ino, str);

}

void
do_ino(int ac, char **av)
{
    int64_t ino1 = 0;
    int64_t ino = 0;
    int64_t ino2 = 0;
    int vno;
    lb64_string_t str;

    if (ac != 3) {
	printf("Bad argument count for -i option.\n");
	exit(1);
    }

    ino = (int64_t) - 1;
    sscanf(av[2], "%qu", &ino);
    printf("%Lu %Lu %Lu\n", ino, ino1, ino2);

    vno = (int)ino;
    if (vno == 0x3ffffff)
	vno = -1;
    int64_to_flipbase64(str, ino);
    printf("ino=%Lu, vno=%d, tag=%u, uniq=%u, base64=%s\n", ino, vno,
	   (int)((ino >> 26) & 0x7), (int)((ino >> 32) & 0xffffffff), str);

}

void
do_inobase64(int ac, char **av)
{
    int64_t ino1 = 0;
    int64_t ino = 0;
    int64_t ino2 = 0;
    int vno;


    if (ac != 3) {
	printf("Bad argument count for -a option.\n");
	exit(1);
    }

    ino = flipbase64_to_int64(av[2]);

    vno = (int)ino;
    if (vno == 0x3ffffff)
	vno = -1;
    printf("ino=%Lu, vno=%d, tag=%u, uniq=%u\n", ino, vno,
	   (int)((ino >> 26) & 0x7), (int)((ino >> 32) & 0xffffffff));

}
