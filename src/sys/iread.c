
#ifndef lint
#endif

/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>

#include "AFS_component_version_number.c"

main(argc,argv)
char **argv;
{

    struct stat status;
    int count;
    char buf[50000];
    if (stat("/vicepa", &status) == -1) {
	perror("stat");
	exit(1);
    }
    if (--argc != 3) {
	printf("Usage: iread inode offset count\n");
	exit(1);
    }
    count = xiread(status.st_dev, atoi(argv[1]), 17, atoi(argv[2]), buf, atoi(argv[3]));
    if (count == -1) {
	perror("iread");
	exit(1);
    }
    printf("iread(%x) successful, count==%d.  Data follows:\n%s\n",status.st_dev,  count,buf);
    exit(0);
}
