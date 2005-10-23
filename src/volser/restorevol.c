/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Read a vos dump and recreate the tree.
 *
 * restorevol [-file <dump file>]
 *            [-dir <restore dir>]
 *            [-extension <name extension>]
 *            [-mountpoint <mount point root>]
 *            [-umask <mode mask>]
 *
 * 1. The dump file will be restored within the current or that specified with -dir.
 * 2. Within this dir, a subdir is created. It's name is the RW volume name
 *    that was dumped. An extension can be appended to this directory name
 *    with -extension.
 * 3. All mountpoints will appear as symbolic links to the volume. The
 *    pathname to the volume will be either that in -mountpoint, or -dir.
 *    Symbolic links remain untouched.
 * 4. You can change your umask during the restore with -umask. Otherwise, it
 *    uses your current umask. Mode bits for directories are 0777 (then
 *    AND'ed with the umask). Mode bits for files are the owner mode bits
 *    duplicated accross group and user (then AND'ed with the umask).
 * 5. For restores of full dumps, if a directory says it has a file and 
 *    the file is not found, then a symbolic link "AFSFile-<#>" will 
 *    appear in that restored tree. Restores of incremental dumps remove 
 *    all these files at the end (expensive because it is a tree search).  
 * 6. If a file or directory was found in the dump but found not to be 
 *    connected to the hierarchical tree, then the file or directory 
 *    will be connected at the root of the tree as "__ORPHANEDIR__.<#>" 
 *    or "__ORPHANFILE__.<#>".  
 * 7. ACLs are not restored.
 *
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/afsint.h>
#include <afs/nfs.h>
#include <lock.h>
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#include "volint.h"
#include "dump.h"
#include <afs/cmd.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif


char rootdir[MAXPATHLEN];
char mntroot[MAXPATHLEN];
#define ADIR  "AFSDir-"
#define AFILE "AFSFile-"
#define ODIR  "__ORPHANEDIR__."
#define OFILE "__ORPHANFILE__."

int inc_dump = 0;
FILE *dumpfile;

afs_int32
readvalue(size)
{
    afs_int32 value, s;
    int code;
    char *ptr;

    value = 0;
    ptr = (char *)&value;

    s = sizeof(value) - size;
    if (size < 0) {
	fprintf(stderr, "Too much data in afs_int32\n");
	return 0;
    }

    code = fread(&ptr[s], 1, size, dumpfile);
    if (code != size)
	fprintf(stderr, "Code = %d; Errno = %d\n", code, errno);

    return (value);
}

char
readchar()
{
    char value;
    int code;
    char *ptr;

    value = '\0';
    code = fread(&value, 1, 1, dumpfile);
    if (code != 1)
	fprintf(stderr, "Code = %d; Errno = %d\n", code, errno);

    return (value);
}

#define BUFSIZE 16384
char buf[BUFSIZE];

char
readdata(buffer, size)
     char *buffer;
     afs_sfsize_t size;
{
    int code;
    afs_int32 s;

    if (!buffer) {
	while (size > 0) {
	    s = (afs_int32) ((size > BUFSIZE) ? BUFSIZE : size);
	    code = fread(buf, 1, s, dumpfile);
	    if (code != s)
		fprintf(stderr, "Code = %d; Errno = %d\n", code, errno);
	    size -= s;
	}
    } else {
	code = fread(buffer, 1, size, dumpfile);
	if (code != size) {
	    if (code < 0)
		fprintf(stderr, "Code = %d; Errno = %d\n", code, errno);
	    else
		fprintf(stderr, "Read %d bytes out of %lld\n", code, (afs_uintmax_t)size);
	}
	if ((code >= 0) && (code < BUFSIZE))
	    buffer[size] = 0;	/* Add null char at end */
    }
}

afs_int32
ReadDumpHeader(dh)
     struct DumpHeader *dh;	/* Defined in dump.h */
{
    int code, i, done;
    char tag, c;
    afs_int32 magic;

/*  memset(&dh, 0, sizeof(dh)); */

    magic = ntohl(readvalue(4));
    dh->version = ntohl(readvalue(4));

    done = 0;
    while (!done) {
	tag = readchar();
	switch (tag) {
	case 'v':
	    dh->volumeId = ntohl(readvalue(4));
	    break;

	case 'n':
	    for (i = 0, c = 'a'; c != '\0'; i++) {
		dh->volumeName[i] = c = readchar();
	    }
	    dh->volumeName[i] = c;
	    break;

	case 't':
	    dh->nDumpTimes = ntohl(readvalue(2)) >> 1;
	    for (i = 0; i < dh->nDumpTimes; i++) {
		dh->dumpTimes[i].from = ntohl(readvalue(4));
		dh->dumpTimes[i].to = ntohl(readvalue(4));
	    }
	    break;

	default:
	    done = 1;
	    break;
	}
    }

    return ((afs_int32) tag);
}

struct volumeHeader {
    afs_int32 volumeId;
    char volumeName[100];
    afs_int32 volType;
    afs_int32 uniquifier;
    afs_int32 parentVol;
    afs_int32 cloneId;
    afs_int32 maxQuota;
    afs_int32 minQuota;
    afs_int32 diskUsed;
    afs_int32 fileCount;
    afs_int32 accountNumber;
    afs_int32 owner;
    afs_int32 creationDate;
    afs_int32 accessDate;
    afs_int32 updateDate;
    afs_int32 expirationDate;
    afs_int32 backupDate;
    afs_int32 dayUseDate;
    afs_int32 dayUse;
    afs_int32 weekCount;
    afs_int32 weekUse[100];	/* weekCount of these */
    char motd[1024];
    int inService;
    int blessed;
    char message[1024];
    afs_int32 volUpdateCounter;
};

afs_int32
ReadVolumeHeader(count)
     afs_int32 count;
{
    struct volumeHeader vh;
    int code, i, done, entries;
    char tag, c;

/*  memset(&vh, 0, sizeof(vh)); */

    done = 0;
    while (!done) {
	tag = readchar();
	switch (tag) {
	case 'i':
	    vh.volumeId = ntohl(readvalue(4));
	    break;

	case 'v':
	    ntohl(readvalue(4));	/* version stamp - ignore */
	    break;

	case 'n':
	    for (i = 0, c = 'a'; c != '\0'; i++) {
		vh.volumeName[i] = c = readchar();
	    }
	    vh.volumeName[i] = c;
	    break;

	case 's':
	    vh.inService = ntohl(readvalue(1));
	    break;

	case 'b':
	    vh.blessed = ntohl(readvalue(1));
	    break;

	case 'u':
	    vh.uniquifier = ntohl(readvalue(4));
	    break;

	case 't':
	    vh.volType = ntohl(readvalue(1));
	    break;

	case 'p':
	    vh.parentVol = ntohl(readvalue(4));
	    break;

	case 'c':
	    vh.cloneId = ntohl(readvalue(4));
	    break;

	case 'q':
	    vh.maxQuota = ntohl(readvalue(4));
	    break;

	case 'm':
	    vh.minQuota = ntohl(readvalue(4));
	    break;

	case 'd':
	    vh.diskUsed = ntohl(readvalue(4));
	    break;

	case 'f':
	    vh.fileCount = ntohl(readvalue(4));
	    break;

	case 'a':
	    vh.accountNumber = ntohl(readvalue(4));
	    break;

	case 'o':
	    vh.owner = ntohl(readvalue(4));
	    break;

	case 'C':
	    vh.creationDate = ntohl(readvalue(4));
	    break;

	case 'A':
	    vh.accessDate = ntohl(readvalue(4));
	    break;

	case 'U':
	    vh.updateDate = ntohl(readvalue(4));
	    break;

	case 'E':
	    vh.expirationDate = ntohl(readvalue(4));
	    break;

	case 'B':
	    vh.backupDate = ntohl(readvalue(4));
	    break;

	case 'O':
	    for (i = 0, c = 'a'; c != '\0'; i++) {
		vh.message[i] = c = readchar();
	    }
	    vh.volumeName[i] = c;
	    break;

	case 'W':
	    vh.weekCount = ntohl(readvalue(2));
	    for (i = 0; i < vh.weekCount; i++) {
		vh.weekUse[i] = ntohl(readvalue(4));
	    }
	    break;

	case 'M':
	    for (i = 0, c = 'a'; c != '\0'; i++) {
		vh.motd[i] = c = readchar();
	    }
	    break;

	case 'D':
	    vh.dayUseDate = ntohl(readvalue(4));
	    break;

	case 'Z':
	    vh.dayUse = ntohl(readvalue(4));
	    break;
	case 'V':
            vh.volUpdateCounter = ntohl(readvalue(4));
	    break;

	default:
	    done = 1;
	    break;
	}
    }

    return ((afs_int32) tag);
}

struct vNode {
    afs_int32 vnode;
    afs_int32 uniquifier;
    afs_int32 type;
    afs_int32 linkCount;
    afs_int32 dataVersion;
    afs_int32 unixModTime;
    afs_int32 servModTime;
    afs_int32 author;
    afs_int32 owner;
    afs_int32 group;
    afs_int32 modebits;
    afs_int32 parent;
    char acl[192];
#ifdef notdef
    struct acl_accessList {
	int size;		/*size of this access list in bytes, including MySize itself */
	int version;		/*to deal with upward compatibility ; <= ACL_ACLVERSION */
	int total;
	int positive;		/* number of positive entries */
	int negative;		/* number of minus entries */
	struct acl_accessEntry {
	    int id;		/*internally-used ID of user or group */
	    int rights;		/*mask */
	} entries[100];
    } acl;
#endif
    afs_sfsize_t dataSize;
};

#define MAXNAMELEN 256

afs_int32
ReadVNode(count)
     afs_int32 count;
{
    struct vNode vn;
    int code, i, done, entries;
    char tag, c;
    char dirname[MAXNAMELEN], linkname[MAXNAMELEN], lname[MAXNAMELEN];
    char parentdir[MAXNAMELEN], vflink[MAXNAMELEN];
    char filename[MAXNAMELEN], fname[MAXNAMELEN];
    int len;
    afs_int32 vnode;
    afs_int32 mode = 0;

/*  memset(&vn, 0, sizeof(vn)); */
    vn.dataSize = 0;
    vn.vnode = 0;
    vn.parent = 0;
    vn.type = 0;

    vn.vnode = ntohl(readvalue(4));
    vn.uniquifier = ntohl(readvalue(4));

    done = 0;
    while (!done) {
	tag = readchar();
	switch (tag) {
	case 't':
	    vn.type = ntohl(readvalue(1));
	    break;

	case 'l':
	    vn.linkCount = ntohl(readvalue(2));
	    break;

	case 'v':
	    vn.dataVersion = ntohl(readvalue(4));
	    break;

	case 'm':
	    vn.unixModTime = ntohl(readvalue(4));
	    break;

	case 's':
	    vn.servModTime = ntohl(readvalue(4));
	    break;

	case 'a':
	    vn.author = ntohl(readvalue(4));
	    break;

	case 'o':
	    vn.owner = ntohl(readvalue(4));
	    break;

	case 'g':
	    vn.group = ntohl(readvalue(4));
	    break;

	case 'b':
	    vn.modebits = ntohl(readvalue(2));
	    break;

	case 'p':
	    vn.parent = ntohl(readvalue(4));
	    break;

	case 'A':
	    readdata(vn.acl, 192);	/* Skip ACL data */
	    break;

#ifdef AFS_LARGEFILE_ENV
	case 'h':
	    {
		afs_uint32 hi, lo;
		hi = ntohl(readvalue(4));
		lo = ntohl(readvalue(4));
		FillInt64(vn.dataSize, hi, lo);
	    }
	    goto common_vnode;
#endif /* !AFS_LARGEFILE_ENV */

	case 'f':
	    vn.dataSize = ntohl(readvalue(4));

	  common_vnode:
	    /* parentdir is the name of this dir's vnode-file-link
	     * or this file's parent vnode-file-link.
	     * "./AFSDir-<#>". It's a symbolic link to its real dir.
	     * The parent dir and symbolic link to it must exist.
	     */
	    vnode = ((vn.type == 2) ? vn.vnode : vn.parent);
	    if (vnode == 1)
		strncpy(parentdir, rootdir, sizeof parentdir);
	    else {
		afs_snprintf(parentdir, sizeof parentdir, "%s/%s%d", rootdir,
			     ADIR, vnode);

		len = readlink(parentdir, linkname, MAXNAMELEN);
		if (len < 0) {
		    /* parentdir does not exist. So create an orphan dir.
		     * and then link the parentdir to the orphaned dir.
		     */
		    afs_snprintf(linkname, sizeof linkname, "%s/%s%d",
				 rootdir, ODIR, vnode);
		    code = mkdir(linkname, 0777);
		    if ((code < 0) && (errno != EEXIST)) {
			fprintf(stderr,
				"Error creating directory %s  code=%d;%d\n",
				linkname, code, errno);
		    }

		    /* Link the parentdir to it - now parentdir exists */
		    afs_snprintf(linkname, sizeof linkname, "%s%d/", ODIR,
				 vnode);
		    code = symlink(linkname, parentdir);
		    if (code) {
			fprintf(stderr,
				"Error creating symlink %s -> %s  code=%d;%d\n",
				parentdir, linkname, code, errno);
		    }
		}
	    }

	    if (vn.type == 2) {
		 /*ITSADIR*/
		    /* We read the directory entries. If the entry is a
		     * directory, the subdir is created and the root dir
		     * will contain a link to it. If its a file, we only
		     * create a symlink in the dir to the file name.
		     */
		char *buffer;
		unsigned short j;
		afs_int32 this_vn;
		char *this_name;

		struct DirEntry {
		    char flag;
		    char length;
		    unsigned short next;
		    struct MKFid {
			afs_int32 vnode;
			afs_int32 vunique;
		    } fid;
		    char name[20];
		};

		struct Pageheader {
		    unsigned short pgcount;
		    unsigned short tag;
		    char freecount;
		    char freebitmap[8];
		    char padding[19];
		};

		struct DirHeader {
		    struct Pageheader header;
		    char alloMap[128];
		    unsigned short hashTable[128];
		};

		struct Page0 {
		    struct DirHeader header;
		    struct DirEntry entry[1];
		} *page0;


		buffer = NULL;
		buffer = (char *)malloc(vn.dataSize);

		readdata(buffer, vn.dataSize);
		page0 = (struct Page0 *)buffer;

		/* Step through each bucket in the hash table, i,
		 * and follow each element in the hash chain, j.
		 * This gives us each entry of the dir.
		 */
		for (i = 0; i < 128; i++) {
		    for (j = ntohs(page0->header.hashTable[i]); j;
			 j = ntohs(page0->entry[j].next)) {
			j -= 13;
			this_vn = ntohl(page0->entry[j].fid.vnode);
			this_name = page0->entry[j].name;

			if ((strcmp(this_name, ".") == 0)
			    || (strcmp(this_name, "..") == 0))
			    continue;	/* Skip these */

			/* For a directory entry, create it. Then create the
			 * link (from the rootdir) to this directory.
			 */
			if (this_vn & 1) {
			     /*ADIRENTRY*/
				/* dirname is the directory to create.
				 * vflink is what will link to it. 
				 */
				afs_snprintf(dirname, sizeof dirname, "%s/%s",
					     parentdir, this_name);
			    afs_snprintf(vflink, sizeof vflink, "%s/%s%d",
					 rootdir, ADIR, this_vn);

			    /* The link and directory may already exist */
			    len = readlink(vflink, linkname, MAXNAMELEN);
			    if (len < 0) {
				/* Doesn't already exist - so create the directory.
				 * umask will pare the mode bits down.
				 */
				code = mkdir(dirname, 0777);
				if ((code < 0) && (errno != EEXIST)) {
				    fprintf(stderr,
					    "Error creating directory %s  code=%d;%d\n",
					    dirname, code, errno);
				}
			    } else {
				/* Does already exist - so move the directory.
				 * It was created originally as orphaned.
				 */
				linkname[len - 1] = '\0';	/* remove '/' at end */
				afs_snprintf(lname, sizeof lname, "%s/%s",
					     rootdir, linkname);
				code = rename(lname, dirname);
				if (code) {
				    fprintf(stderr,
					    "Error renaming %s to %s  code=%d;%d\n",
					    lname, dirname, code, errno);
				}
			    }

			    /* Now create/update the link to the new/moved directory */
			    if (vn.vnode == 1)
				afs_snprintf(dirname, sizeof dirname, "%s/",
					     this_name);
			    else
				afs_snprintf(dirname, sizeof dirname,
					     "%s%d/%s/", ADIR, vn.vnode,
					     this_name);
			    unlink(vflink);
			    code = symlink(dirname, vflink);
			    if (code) {
				fprintf(stderr,
					"Error creating symlink %s -> %s  code=%d;%d\n",
					vflink, dirname, code, errno);
			    }
			}
			/*ADIRENTRY*/
			    /* For a file entry, we remember the name of the file
			     * by creating a link within the directory. Restoring
			     * the file will later remove the link.
			     */
			    else {
			     /*AFILEENTRY*/ afs_snprintf(vflink,
							 sizeof vflink,
							 "%s/%s%d", parentdir,
							 AFILE, this_vn);

			    code = symlink(this_name, vflink);
			    if ((code < 0) && (errno != EEXIST)) {
				fprintf(stderr,
					"Error creating symlink %s -> %s  code=%d;%d\n",
					vflink, page0->entry[j].name, code,
					errno);
			    }
			}
		     /*AFILEENTRY*/}
		}
		free(buffer);
	    }
	    /*ITSADIR*/
	    else if (vn.type == 1) {
		 /*ITSAFILE*/
		    /* A file vnode. So create it into the desired directory. A
		     * link should exist in the directory naming the file.
		     */
		int fid;
		int lfile;
		afs_sfsize_t size, s;

		/* Check if its vnode-file-link exists. If not,
		 * then the file will be an orphaned file.
		 */
		lfile = 1;
		afs_snprintf(filename, sizeof filename, "%s/%s%d", parentdir,
			     AFILE, vn.vnode);
		len = readlink(filename, fname, MAXNAMELEN);
		if (len < 0) {
		    afs_snprintf(filename, sizeof filename, "%s/%s%d",
				 rootdir, OFILE, vn.vnode);
		    lfile = 0;	/* no longer a linked file; a direct path */
		}

		/* Create a mode for the file. Use the owner bits and
		 * duplicate them across group and other. The umask
		 * will remove what we don't want.
		 */
		mode = (vn.modebits >> 6) & 0x7;
		mode |= (mode << 6) | (mode << 3);

		/* Write the file out */
		fid = open(filename, (O_CREAT | O_WRONLY | O_TRUNC), mode);
		size = vn.dataSize;
		while (size > 0) {
		    s = (afs_int32) ((size > BUFSIZE) ? BUFSIZE : size);
		    code = fread(buf, 1, s, dumpfile);
		    if (code > 0) {
			(void)write(fid, buf, code);
			size -= code;
		    }
		    if (code != s) {
			if (code < 0)
			    fprintf(stderr, "Code = %d; Errno = %d\n", code,
				    errno);
			else {
			    char tmp[100];
			    (void)afs_snprintf(tmp, sizeof tmp,
					       "Read %llu bytes out of %llu",
					       (afs_uintmax_t) (vn.dataSize -
								size),
					       (afs_uintmax_t) vn.dataSize);
			    fprintf(stderr, "%s\n", tmp);
			}
			break;
		    }
		}
		close(fid);
		if (size != 0) {
		    fprintf(stderr, "   File %s (%s) is incomplete\n",
			    filename, fname);
		}

		/* Remove the link to the file */
		if (lfile) {
		    unlink(filename);
		}
	    }
	    /*ITSAFILE*/
	    else if (vn.type == 3) {
		 /*ITSASYMLINK*/
		    /* A symlink vnode. So read it into the desired directory. This could
		     * also be a mount point. If the volume is being restored to AFS, this
		     * will become a mountpoint. If not, it becomes a symlink to no-where.
		     */
		int fid;
		afs_int32 size, s;

		/* Check if its vnode-file-link exists and create pathname
		 * of the symbolic link. If it doesn't exist,
		 * then the link will be an orphaned link.
		 */
		afs_snprintf(linkname, sizeof linkname, "%s/%s%d", parentdir,
			     AFILE, vn.vnode);
		len = readlink(linkname, fname, MAXNAMELEN);
		if (len < 0) {
		    afs_snprintf(filename, sizeof filename, "%s/%s%d",
				 rootdir, OFILE, vn.vnode);
		} else {
		    fname[len] = '\0';
		    afs_snprintf(filename, sizeof filename, "%s/%s",
				 parentdir, fname);
		}

		/* Read the link in, delete it, and then create it */
		readdata(buf, vn.dataSize);

		/* If a mountpoint, change its link path to mountroot */
		s = strlen(buf);
		if (((buf[0] == '%') || (buf[0] == '#'))
		    && (buf[s - 1] == '.')) {
		    /* This is a symbolic link */
		    buf[s - 1] = 0;	/* Remove prefix '.' */
		    strcpy(lname, &buf[1]);	/* Remove postfix '#' or '%' */
		    strcpy(buf, mntroot);
		    strcat(buf, lname);
		}

		unlink(filename);
		code = symlink(buf, filename);
		if (code) {
		    fprintf(stderr,
			    "Error creating symlink %s -> %s  code=%d;%d\n",
			    filename, buf, code, errno);
		}

		/* Remove the symbolic link */
		unlink(linkname);
	    }
	    /*ITSASYMLINK*/
	    else {
		fprintf(stderr, "Unknown Vnode block\n");
	    }
	    break;

	default:
	    done = 1;
	    break;
	}
    }
    if (vn.type == 0)
	inc_dump = 1;

    return ((afs_int32) tag);
}

WorkerBee(as, arock)
     struct cmd_syndesc *as;
     char *arock;
{
    int code = 0, c, len;
    afs_int32 type, count, vcount;
    DIR *dirP, *dirQ;
    struct dirent *dirE, *dirF;
    char fname[MAXNAMELEN], name[MAXNAMELEN], lname[MAXNAMELEN],
	mname[MAXNAMELEN];
    char thisdir[MAXPATHLEN], *t;
    struct DumpHeader dh;	/* Defined in dump.h */
#if 0/*ndef HAVE_GETCWD*/	/* XXX enable when autoconf happens */
    extern char *getwd();
#define getcwd(x,y) getwd(x)
#endif

    if (as->parms[0].items) {	/* -file <dumpfile> */
	dumpfile = fopen(as->parms[0].items->data, "r");
	if (!dumpfile) {
	    fprintf(stderr, "Cannot open '%s'. Code = %d\n",
		    as->parms[0].items->data, errno);
	    goto cleanup;
	}
    } else {
	dumpfile = (FILE *) stdin;	/* use stdin */
    }

    /* Read the dump header. From it we get the volume name */
    type = ntohl(readvalue(1));
    if (type != 1) {
	fprintf(stderr, "Expected DumpHeader\n");
	code = -1;
	goto cleanup;
    }
    type = ReadDumpHeader(&dh);

    /* Get the root directory we restore to */
    if (as->parms[1].items) {	/* -dir <rootdir> */
	strcpy(rootdir, as->parms[1].items->data);
    } else {
	strcpy(rootdir, ".");
    }
    strcat(rootdir, "/");

    /* Append the RW volume name to the root directory */
    strcat(rootdir, dh.volumeName);
    len = strlen(rootdir);
    if (strcmp(".backup", rootdir + len - 7) == 0) {
	rootdir[len - 7] = 0;
    } else if (strcmp(".readonly", rootdir + len - 9) == 0) {
	rootdir[len - 9] = 0;
    }

    /* Append the extension we asked for */
    if (as->parms[2].items) {
	strcat(rootdir, as->parms[2].items->data);	/* -extension <ext> */
    }

    /* The mountpoint root is either specifid in -mountpoint
     * or -dir or the current working dir.
     */
    if ((as->parms[3].items) || (as->parms[1].items)) {	/* -mountpoint  or -dir */
	t = (char *)getcwd(thisdir, MAXPATHLEN);	/* remember current dir */
	if (!t) {
	    fprintf(stderr,
		    "Cannot get pathname of current working directory: %s\n",
		    thisdir);
	    code = -1;
	    goto cleanup;
	}
	/* Change to the mount point dir */
	code =
	    chdir((as->parms[3].items ? as->parms[3].items->data : as->
		   parms[1].items->data));
	if (code) {
	    fprintf(stderr, "Mount point directory not found: Error = %d\n",
		    errno);
	    goto cleanup;
	}
	t = (char *)getcwd(mntroot, MAXPATHLEN);	/* get its full pathname */
	if (!t) {
	    fprintf(stderr,
		    "Cannot determine pathname of mount point root directory: %s\n",
		    mntroot);
	    code = -1;
	    goto cleanup;
	}
	strcat(mntroot, "/");	/* append '/' to end of it */
	code = chdir(thisdir);	/* return to original working dir */
	if (code) {
	    fprintf(stderr, "Cannot find working directory: Error = %d\n",
		    errno);
	    goto cleanup;
	}
    } else {			/* use current directory */
	t = (char *)getcwd(mntroot, MAXPATHLEN);	/* get full pathname of current dir */
	if (!t) {
	    fprintf(stderr,
		    "Cannot determine pathname of current working directory: %s\n",
		    mntroot);
	    code = -1;
	    goto cleanup;
	}
    }
    strcat(mntroot, "/");	/* append '/' to end of it */

    /* Set the umask for the restore */
    if (as->parms[4].items) {	/* -umask */
	afs_int32 mask;
	mask = strtol(as->parms[4].items->data, 0, 8);
	fprintf(stderr, "Umask set to 0%03o\n", mask);
	umask(mask);
    }

    fprintf(stderr, "Restoring volume dump of '%s' to directory '%s'.\n",
	    dh.volumeName, rootdir);
    code = mkdir(rootdir, 0777);
    if ((code < 0) && (errno != EEXIST)) {
	fprintf(stderr, "Error creating directory %s  code=%d;%d\n", rootdir,
		code, errno);
    }

    for (count = 1; type == 2; count++) {
	type = ReadVolumeHeader(count);
	for (vcount = 1; type == 3; vcount++)
	    type = ReadVNode(vcount);
    }

    if (type != 4) {
	fprintf(stderr, "Expected End-of-Dump\n");
	code = -1;
	goto cleanup;
    }

  cleanup:
    /* For incremental restores, Follow each directory link and
     * remove an "AFSFile" links.
     */
    if (inc_dump) {
	fprintf(stderr, "An incremental dump.\n");
	dirP = opendir(rootdir);
	while (dirP && (dirE = readdir(dirP))) {
	    if (strncmp(dirE->d_name, ADIR, strlen(ADIR)) == 0) {
		afs_snprintf(name, sizeof name, "%s/%s", rootdir,
			     dirE->d_name);
		dirQ = opendir(name);
		while (dirQ && (dirF = readdir(dirQ))) {
		    if (strncmp(dirF->d_name, AFILE, strlen(AFILE)) == 0) {
			afs_snprintf(name, sizeof name, "%s/%s/%s", rootdir,
				     dirE->d_name, dirF->d_name);
			unlink(name);
		    }
		}
		closedir(dirQ);
	    } else if (strncmp(dirE->d_name, AFILE, strlen(AFILE)) == 0) {
		afs_snprintf(name, sizeof name, "%s/%s", rootdir,
			     dirE->d_name);
		unlink(name);
	    }
	}
	closedir(dirP);
    }

    /* Now go through and remove all the directory links */
    dirP = opendir(rootdir);
    while (dirP && (dirE = readdir(dirP))) {
	if (strncmp(dirE->d_name, ADIR, strlen(ADIR)) == 0) {
	    afs_snprintf(name, sizeof name, "%s/%s", rootdir, dirE->d_name);
	    unlink(name);
	}
    }
    closedir(dirP);

    return (code);
}

main(argc, argv)
     int argc;
     char **argv;
{
    struct cmd_syndesc *ts;
    struct cmd_item *ti;

    setlinebuf(stdout);

    ts = cmd_CreateSyntax(NULL, WorkerBee, NULL, "vldb check");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL, "dump file");
    cmd_AddParm(ts, "-dir", CMD_SINGLE, CMD_OPTIONAL, "restore dir");
    cmd_AddParm(ts, "-extension", CMD_SINGLE, CMD_OPTIONAL, "name extension");
    cmd_AddParm(ts, "-mountpoint", CMD_SINGLE, CMD_OPTIONAL,
		"mount point root");
    cmd_AddParm(ts, "-umask", CMD_SINGLE, CMD_OPTIONAL, "mode mask");

    return cmd_Dispatch(argc, argv);
}
