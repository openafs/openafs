/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#define PAGESIZE 2048
#include <afsconfig.h>
#include <afs/param.h>


#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <windows.h>
#else
#include <sys/file.h>
#endif
#include <stdio.h>
#include <errno.h>

long fidCounter = 0;

typedef struct dirhandle {
    int fd;
    int uniq;
} dirhandle;
int Uniq;

void
Usage(void)
{
    printf("Usage: dtest <command [args]>, where command is one of:\n");
    printf("-l file - lists directory in file\n");
    printf("-c file - checks directory in file\n");
    printf("-s ifile ofile - salvages directory in ifile, places in ofile\n");
    printf
	("-f file name count - creates count names based on \"name\" in directory in file\n");
    printf("-d file name - delete name from directory in file\n");
    printf("-r file name - lookup name in directory\n");
    printf("-a file name - add name to directory in file\n");
    exit(1);
}

main(argc, argv)
     char **argv;
{
    DInit(600);
    argc--;
    argv++;

    if (argc == 0)
	Usage();

    switch ((*argv++)[1]) {
    case 'l':
	ListDir(*argv);
	break;
    case 'c':
	CheckDir(*argv);
	break;
    case 's':
	SalvageDir(*argv, argv[1]);
	break;
    case 'f':
	CRTest(*argv, argv[1], atoi(argv[2]));
	break;
    case 'd':
	DelTest(*argv, argv[1]);
	break;
    case 'r':
	LookupDir(*argv, argv[1]);
	break;
    case 'a':
	AddEntry(*argv, argv[1]);
	break;
    default:
	Usage();
    }
    exit(0);
}

LookupDir(dname, ename)
     char *dname, *ename;
{
    dirhandle dir;
    long fid[3], code;

    OpenDir(dname, &dir);
    code = Lookup(&dir, ename, fid);
    if (code)
	printf("lookup code %d\n", code);
    else {
	printf("Found fid %d.%d for file '%s'\n", fid[1], fid[2], ename);
    }
    DFlush();
}

AddEntry(dname, ename)
     char *dname, *ename;
{
    dirhandle dir;
    long fid[3], code;

    fid[1] = fidCounter++;
    fid[2] = 3;
    OpenDir(dname, &dir);
    code = Create(&dir, ename, fid);
    if (code)
	printf("create code %d\n", code);
    DFlush();
}

ListDir(name)
     char *name;
{
    extern ListEntry();
    dirhandle dir;
    OpenDir(name, &dir);
    EnumerateDir(&dir, ListEntry, 0);
}

ListEntry(handle, name, vnode, unique)
     long handle, vnode, unique;
     char *name;
{
    printf("%s\t%d\t%d\n", name, vnode, unique);
}

CheckDir(name)
     char *name;
{
    dirhandle dir;
    OpenDir(name, &dir);
    if (DirOK(&dir))
	printf("Directory ok.\n");
    else
	printf("Directory bad\n");
}

SalvageDir(iname, oname)
     char *iname, *oname;
{
    dirhandle in, out;
    long myFid[3], parentFid[3];
    OpenDir(iname, &in);
    if (Lookup(in, ".", myFid) || Lookup(in, "..", parentFid)) {
	printf("Lookup of \".\" and/or \"..\" failed: ");
	printf("%d %d %d %d\n", myFid[1], myFid[2], parentFid[1],
	       parentFid[2]);
	printf("Directory cannnot be salvaged\n");
    }
    CreateDir(oname, &out);
    DirSalvage(&in, &out, myFid[1], myFid[2], parentFid[1], parentFid[2]);
    DFlush();
}

DelTest(dname, ename)
     char *dname;
     char *ename;
{
    dirhandle dir;
    long code;

    OpenDir(dname, &dir);
    code = Delete(&dir, ename);
    if (code)
	printf("delete code is %d\n", code);
    DFlush();
}

CRTest(dname, ename, count)
     char *dname;
     char *ename;
     int count;
{
    char tbuffer[200];
    long i, code;
    long fid[3];
    dirhandle dir;

    CreateDir(dname, &dir);
    memset(fid, 0, sizeof(fid));
    MakeDir(&dir, fid, fid);
    for (i = 0; i < count; i++) {
	sprintf(tbuffer, "%s%d", ename, i);
	fid[1] = fidCounter++;
	fid[2] = count;
	code = Create(&dir, tbuffer, &fid);
	if (i % 100 == 0) {
	    printf("#");
	    fflush(stdout);
	}
	if (code) {
	    printf("code for '%s' is %d\n", tbuffer, code);
	    return;
	}
    }
    DFlush();
}

OpenDir(name, dir)
     char *name;
     dirhandle *dir;
{
    dir->fd = open(name, O_RDWR, 0666);
    if (dir->fd == -1) {
	printf("Couldn't open %s\n", name);
	exit(1);
    }
    dir->uniq = ++Uniq;
}

CreateDir(name, dir)
     char *name;
     dirhandle *dir;
{
    dir->fd = open(name, O_CREAT | O_RDWR | O_TRUNC, 0666);
    dir->uniq = ++Uniq;
    if (dir->fd == -1) {
	printf("Couldn't create %s\n", name);
	exit(1);
    }
}

ReallyRead(dir, block, data)
     dirhandle *dir;
     int block;
     char *data;
{
    int code;
    if (lseek(dir->fd, block * PAGESIZE, 0) == -1)
	return errno;
    code = read(dir->fd, data, PAGESIZE);
    if (code < 0)
	return errno;
    if (code != PAGESIZE)
	return EIO;
    return 0;
}

ReallyWrite(dir, block, data)
     dirhandle *dir;
     int block;
     char *data;
{
    int code;
    if (lseek(dir->fd, block * PAGESIZE, 0) == -1)
	return errno;
    code = write(dir->fd, data, PAGESIZE);
    if (code < 0)
	return errno;
    if (code != PAGESIZE)
	return EIO;
    return 0;
}

FidZap(dir)
     dirhandle *dir;
{
    dir->fd = -1;
}

int
FidZero(afid)
     long *afid;
{
    *afid = 0;
}

FidEq(dir1, dir2)
     dirhandle *dir1, *dir2;
{
    return (dir1->uniq == dir2->uniq);
}

int
FidVolEq(afid, bfid)
     long *afid, *bfid;
{
    return 1;
}

FidCpy(todir, fromdir)
     dirhandle *todir, *fromdir;
{
    *todir = *fromdir;
}

Die(msg)
     char *msg;
{
    printf("Something died with this message:  %s\n", msg);
}

Log(a, b, c, d, e, f, g, h, i, j, k, l, m, n)
{
    printf(a, b, c, d, e, f, g, h, i, j, k, l, m, n);
}
