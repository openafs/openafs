
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

    if (stat("/vicepa", &status) == -1) {
	perror("stat");
	exit(1);
    }
    if (--argc != 4) {
	printf("Usage: iwrite inode offset string count\n");
	exit(1);
    }
    count = xiwrite(status.st_dev, atoi(argv[1]), 17, atoi(argv[2]), argv[3], atoi(argv[4]));
    if (count == -1) {
	perror("iwrite");
	exit(1);
    }
    printf("iwrite successful, count==%d\n", count);
    exit(0);
}
