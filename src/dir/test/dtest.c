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

#include <roken.h>

#include <afs/dir.h>
#include <afs/opr.h>

long fidCounter = 0;

typedef struct DirHandle {
    int fd;
    int uniq;
} dirhandle;
int Uniq;

static void OpenDir(char *name, dirhandle *dir);
static void CreateDir(char *name, dirhandle *dir);

static void
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

static void
LookupDir(char *dname, char *ename)
{
    dirhandle dir;
    long fid[3];
    int code;

    OpenDir(dname, &dir);
    code = afs_dir_Lookup(&dir, ename, fid);
    if (code)
	printf("lookup code %d\n", code);
    else {
	printf("Found fid %ld.%ld for file '%s'\n", fid[1], fid[2], ename);
    }
    DFlush();
}

static void
AddEntry(char *dname, char *ename)
{
    dirhandle dir;
    long fid[3];
    int code;

    fid[1] = fidCounter++;
    fid[2] = 3;
    OpenDir(dname, &dir);
    code = afs_dir_Create(&dir, ename, fid);
    if (code)
	printf("create code %d\n", code);
    DFlush();
}

static int
ListEntry(void * handle, char *name, afs_int32 vnode, afs_int32 unique)
{
    printf("%s\t%ld\t%ld\n", name, afs_printable_int32_ld(vnode),
	   afs_printable_int32_ld(unique));

    return 0;
}

static void
ListDir(char *name)
{
    dirhandle dir;
    OpenDir(name, &dir);
    afs_dir_EnumerateDir(&dir, ListEntry, 0);
}

static void
CheckDir(char *name)
{
    dirhandle dir;
    OpenDir(name, &dir);
    if (DirOK(&dir))
	printf("Directory ok.\n");
    else
	printf("Directory bad\n");
}

static void
SalvageDir(char *iname, char *oname)
{
    dirhandle in, out;
    afs_int32 myFid[3], parentFid[3];

    OpenDir(iname, &in);
    if (afs_dir_Lookup(&in, ".", myFid) || 
	afs_dir_Lookup(&in, "..", parentFid)) {
	printf("Lookup of \".\" and/or \"..\" failed: ");
	printf("%d %d %d %d\n", myFid[1], myFid[2], parentFid[1],
	       parentFid[2]);
	printf("Directory cannnot be salvaged\n");
    }
    CreateDir(oname, &out);
    DirSalvage(&in, &out, myFid[1], myFid[2], parentFid[1], parentFid[2]);
    DFlush();
}

static void
DelTest(char *dname, char *ename)
{
    dirhandle dir;
    int code;

    OpenDir(dname, &dir);
    code = afs_dir_Delete(&dir, ename);
    if (code)
	printf("delete code is %d\n", code);
    DFlush();
}

static void
CRTest(char *dname, char *ename, int count)
{
    char tbuffer[200];
    int i;
    afs_int32 fid[3];
    dirhandle dir;
    int code;

    CreateDir(dname, &dir);
    memset(fid, 0, sizeof(fid));
    afs_dir_MakeDir(&dir, fid, fid);
    for (i = 0; i < count; i++) {
	sprintf(tbuffer, "%s%d", ename, i);
	fid[1] = fidCounter++;
	fid[2] = count;
	code = afs_dir_Create(&dir, tbuffer, &fid);
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

static void
OpenDir(char *name, dirhandle *dir)
{
    dir->fd = open(name, O_RDWR, 0666);
    if (dir->fd == -1) {
	printf("Couldn't open %s\n", name);
	exit(1);
    }
    dir->uniq = ++Uniq;
}

static void
CreateDir(char *name, dirhandle *dir)
{
    dir->fd = open(name, O_CREAT | O_RDWR | O_TRUNC, 0666);
    dir->uniq = ++Uniq;
    if (dir->fd == -1) {
	printf("Couldn't create %s\n", name);
	exit(1);
    }
}

int
ReallyRead(dirhandle *dir, int block, char *data)
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

int
ReallyWrite(dirhandle *dir, int block, char *data)
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

void
FidZap(dirhandle *dir)
{
    dir->fd = -1;
}

void
FidZero(long *afid)
{
    *afid = 0;
}

int
FidEq(dirhandle *dir1, dirhandle *dir2)
{
    return (dir1->uniq == dir2->uniq);
}

int
FidVolEq(long *afid, long *bfid)
{
    return 1;
}

void
FidCpy(dirhandle *todir, dirhandle *fromdir)
{
    *todir = *fromdir;
}

void
Die(const char *msg)
{
    printf("Something died with this message:  %s\n", msg);
    opr_abort();
}

void
Log(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

int
main(int argc, char **argv)
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
