/* touch.c : Defines the entry point for the console application.*/
/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/

#include <windows.h>
#include "io.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <process.h>

#ifndef intptr_t
#define intptr_t INT_PTR
#endif

void
usage()
{
    printf("touch filename/Wildcard \n");
    exit(1);
}

int
main(int argc, char *argv[])
{
    int fh;
    intptr_t fs;
    long pos;
    char buffer[1];
    struct _finddata_t finfo;
    if (argc < 2)
	usage();
    fs = _findfirst(argv[1], &finfo);
    if (fs == -1)
	return 0;
    do {

	if ((finfo.attrib & ~_A_ARCH) != _A_NORMAL)
	    continue;
	fh = _open(finfo.name, _S_IWRITE | _O_BINARY | _S_IREAD | _O_RDWR);
	pos = _lseek(fh, 0L, SEEK_END);
	buffer[0] = 0;
	_write(fh, buffer, 1);
	_chsize(fh, pos);
	_close(fh);
    } while (_findnext(fs, &finfo) == 0);
    return 0;
}
