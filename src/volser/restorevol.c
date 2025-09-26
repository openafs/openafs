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

#include <roken.h>

#include <afs/afsint.h>
#include <afs/nfs.h>
#include <rx/rx_queue.h>
#include <afs/afs_lock.h>
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#include <afs/cmd.h>

#include "volint.h"
#include "dump.h"

char rootdir[MAXPATHLEN];
char mntroot[MAXPATHLEN];
#define ADIR  "AFSDir-"
#define AFILE "AFSFile-"
#define ODIR  "__ORPHANEDIR__."
#define OFILE "__ORPHANFILE__."

int inc_dump = 0;
FILE *dumpfile;

afs_int32
readvalue(int size)
{
    afs_int32 value, s;
    int code;
    char *ptr;

    value = 0;
    ptr = (char *)&value;

    s = sizeof(value) - size;
    if (s < 0) {
	fprintf(stderr, "Too much data for afs_int32\n");
	return 0;
    }

    code = fread(&ptr[s], 1, size, dumpfile);
    if (code != size)
	fprintf(stderr, "Code = %d; Errno = %d\n", code, errno);

    return (value);
}

char
readchar(void)
{
    char value;
    int code;

    value = '\0';
    code = fread(&value, 1, 1, dumpfile);
    if (code != 1)
	fprintf(stderr, "Code = %d; Errno = %d\n", code, errno);

    return (value);
}

#define BUFSIZE 16384
char buf[BUFSIZE];

void
readdata(char *buffer, afs_sfsize_t size)
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
		fprintf(stderr, "Read %d bytes out of %" AFS_INT64_FMT "\n", code, (afs_uintmax_t)size);
	}
	if ((code >= 0) && (code < BUFSIZE))
	    buffer[size] = 0;	/* Add null char at end */
    }
}

afs_int32
ReadDumpHeader(struct DumpHeader *dh)
{
    int i, done;
    char tag, c;
    afs_int32 magic AFS_UNUSED;

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
	    if (dh->nDumpTimes > MAXDUMPTIMES) {
		fprintf(stderr, "Too many dump times in header (%d > %d)\n",
			dh->nDumpTimes, MAXDUMPTIMES);
		dh->nDumpTimes = MAXDUMPTIMES;
	    }
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
ReadVolumeHeader(afs_int32 count)
{
    struct volumeHeader vh;
    int i, done;
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
	    (void)ntohl(readvalue(4));	/* version stamp - ignore */
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
	    readvalue(4); /*volUpCounter*/
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
    afs_sfsize_t dataSize;
};

#define MAXNAMELEN 256

static int
ReadVNode(afs_int32 count, afs_int32 *return_tag)
{
    struct vNode vn;
    int code, i, done;
    char tag;
    char *dirname = NULL, *lname = NULL, *slinkname = NULL;
    char linkname[MAXNAMELEN+1];
    char *parentdir = NULL, *vflink = NULL;
    char *filename = NULL;
    char fname[MAXNAMELEN+1];
    int len;
    afs_int32 vnode;
    afs_int32 mode = 0;
    afs_uint32 hi, lo;

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

	case 'h':
	    hi = ntohl(readvalue(4));
	    lo = ntohl(readvalue(4));
	    FillInt64(vn.dataSize, hi, lo);
	    goto common_vnode;

	case 'f':
	    vn.dataSize = ntohl(readvalue(4));

	  common_vnode:
	    /* parentdir is the name of this dir's vnode-file-link
	     * or this file's parent vnode-file-link.
	     * "./AFSDir-<#>". It's a symbolic link to its real dir.
	     * The parent dir and symbolic link to it must exist.
	     */
	    vnode = ((vn.type == 2) ? vn.vnode : vn.parent);
	    if (vnode == 1) {
		free(parentdir);
		parentdir = strdup(rootdir);
		if (!parentdir) {
		    code = ENOMEM;
		    goto common_exit;
		}
	    } else {
		free(parentdir);
		if (asprintf(&parentdir,
			 "%s" OS_DIRSEP "%s%d", rootdir, ADIR, vnode) < 0) {
		    parentdir = NULL;
		    code = ENOMEM;
		    goto common_exit;
		}

		len = readlink(parentdir, linkname, MAXNAMELEN);
		if (len < 0) {
		    /* parentdir does not exist. So create an orphan dir.
		     * and then link the parentdir to the orphaned dir.
		     */
		    if (asprintf(&lname, "%s" OS_DIRSEP "%s%d",
			     rootdir, ODIR, vnode) < 0) {
			lname = NULL;
			code = ENOMEM;
			goto common_exit;
		    }

		    code = mkdir(lname, 0777);
		    if ((code < 0) && (errno != EEXIST)) {
			fprintf(stderr,
				"Error creating directory %s  code=%d;%d\n",
				lname, code, errno);
		    }
		    free(lname);
		    /* Link the parentdir to it - now parentdir exists */
		    if (asprintf(&lname, "%s%d/", ODIR,
			     vnode) < 0) {
			lname = NULL;
			code = ENOMEM;
			goto common_exit;
		    }

		    code = symlink(lname, parentdir);
		    if (code) {
			fprintf(stderr,
				"Error creating symlink %s -> %s  code=%d;%d\n",
				parentdir, lname, code, errno);
		    }
		    free(lname);
		    lname = NULL;
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
		buffer = calloc(1, vn.dataSize);
		if (!buffer) {
		    code = ENOMEM;
		    goto common_exit;
		}

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
			    if (asprintf(&dirname,
				     "%s" OS_DIRSEP "%s",
				     parentdir, this_name) < 0) {
				free(buffer);
				code = ENOMEM;
				goto common_exit;
			    }

			    if (asprintf(&vflink,
				     "%s" OS_DIRSEP "%s%d",
				     rootdir, ADIR, this_vn) < 0) {
				free(buffer);
				free(dirname);
				code = ENOMEM;
				goto common_exit;
			    }

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
				if (asprintf(&lname,
				         "%s" OS_DIRSEP "%s",
					 rootdir, linkname) < 0) {
				    free(buffer);
				    free(dirname);
				    free(vflink);
				    lname = NULL;
				    code = ENOMEM;
				    goto common_exit;
				}

				code = rename(lname, dirname);
				if (code) {
				    fprintf(stderr,
					    "Error renaming %s to %s  code=%d;%d\n",
					    lname, dirname, code, errno);
				}
				free(lname);
				lname = NULL;
			    }
			    free(dirname);
			    dirname = NULL;

			    /* Now create/update the link to the new/moved directory */
			    if (vn.vnode == 1) {
				if (asprintf(&dirname, "%s/",
					 this_name) < 0) {
				    free(buffer);
				    free(vflink);
				    code = ENOMEM;
				    goto common_exit;
				}
			    } else {
				if (asprintf(&dirname, "%s%d/%s/",
					 ADIR, vn.vnode, this_name) < 0) {
				    free(buffer);
				    free(vflink);
				    code = ENOMEM;
				    goto common_exit;
				}
			    }

			    unlink(vflink);
			    code = symlink(dirname, vflink);
			    if (code) {
				fprintf(stderr,
					"Error creating symlink %s -> %s  code=%d;%d\n",
					vflink, dirname, code, errno);
			    }
			    free(dirname);
			    free(vflink);
			    dirname = NULL;
			    vflink = NULL;
			}
			/*ADIRENTRY*/
			    /* For a file entry, we remember the name of the file
			     * by creating a link within the directory. Restoring
			     * the file will later remove the link.
			     */
			else {
			     /*AFILEENTRY*/
			    if (asprintf(&vflink,
				     "%s" OS_DIRSEP "%s%d", parentdir,
				     AFILE, this_vn) < 0) {
				free(buffer);
				code = ENOMEM;
				goto common_exit;
			    }

			    code = symlink(this_name, vflink);
			    if ((code < 0) && (errno != EEXIST)) {
				fprintf(stderr,
					"Error creating symlink %s -> %s  code=%d;%d\n",
					vflink, page0->entry[j].name, code,
					errno);
			    }
			    free(vflink);
			    vflink = NULL;
			}
		     /*AFILEENTRY*/}
		}
		free(buffer);
		buffer = NULL;
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
		ssize_t count;

		/* Check if its vnode-file-link exists. If not,
		 * then the file will be an orphaned file.
		 */
		lfile = 1;
		if (asprintf(&filename, "%s" OS_DIRSEP "%s%d",
			 parentdir, AFILE, vn.vnode) < 0) {
		    code = ENOMEM;
		    goto common_exit;
		}
		len = readlink(filename, fname, MAXNAMELEN);
		if (len < 0) {
		    free(filename);
		    if (asprintf(&filename, "%s" OS_DIRSEP "%s%d",
			     rootdir, OFILE, vn.vnode) < 0) {
			code = ENOMEM;
			goto common_exit;
		    }
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
		if (fid < 0) {
		    fprintf(stderr, "Open %s: Errno = %d\n", filename, errno);
		    goto open_fail;
		}
		size = vn.dataSize;
		while (size > 0) {
		    s = (afs_int32) ((size > BUFSIZE) ? BUFSIZE : size);
		    code = fread(buf, 1, s, dumpfile);
		    if (code != s) {
			if (code < 0)
			    fprintf(stderr, "Code = %d; Errno = %d\n", code,
				    errno);
			else {
			    fprintf(stderr, "Read %llu bytes out of %llu\n",
				     (afs_uintmax_t) (vn.dataSize - size),
				     (afs_uintmax_t) vn.dataSize);
			}
			break;
		    }
		    if (code > 0) {
			count = write(fid, buf, code);
			if (count < 0) {
			    fprintf(stderr, "Count = %ld, Errno = %d\n",
				    (long)count, errno);
			    break;
			} else if (count != code) {
			    fprintf(stderr, "Wrote %llu bytes out of %llu\n",
				    (afs_uintmax_t) count,
				    (afs_uintmax_t) code);
			    break;
			}
			size -= code;
		    }
		}
		close(fid);
		if (size != 0) {
		    fprintf(stderr, "   File %s (%s) is incomplete\n",
			    filename, fname);
		}

open_fail:
		/* Remove the link to the file */
		if (lfile) {
		    unlink(filename);
		}
		free(filename);
		filename = NULL;
	    }
	    /*ITSAFILE*/
	    else if (vn.type == 3) {
		 /*ITSASYMLINK*/
		    /* A symlink vnode. So read it into the desired directory. This could
		     * also be a mount point. If the volume is being restored to AFS, this
		     * will become a mountpoint. If not, it becomes a symlink to no-where.
		     */
		afs_int32 s;

		/* Check if its vnode-file-link exists and create pathname
		 * of the symbolic link. If it doesn't exist,
		 * then the link will be an orphaned link.
		 */
		if (asprintf(&slinkname, "%s" OS_DIRSEP "%s%d",
			 parentdir, AFILE, vn.vnode) < 0) {
		    code = ENOMEM;
		    goto common_exit;
		}

		len = readlink(slinkname, fname, MAXNAMELEN);
		if (len < 0) {
		    if (asprintf(&filename, "%s" OS_DIRSEP "%s%d",
			     rootdir, OFILE, vn.vnode) < 0) {
			free(slinkname);
			code = ENOMEM;
			goto common_exit;
		    }
		} else {
		    fname[len] = '\0';
		    if (asprintf(&filename, "%s" OS_DIRSEP "%s",
			     parentdir, fname) < 0) {
			free(slinkname);
			code = ENOMEM;
			goto common_exit;
		    }
		}

		/* Read the link in, delete it, and then create it */
		readdata(buf, vn.dataSize);

		/* If a mountpoint, change its link path to mountroot */
		s = strlen(buf);
		if (((buf[0] == '%') || (buf[0] == '#'))
		    && (buf[s - 1] == '.')) {
		    /* This is a symbolic link */
		    buf[s - 1] = 0;	/* Remove prefix '.' */
		    lname = strdup(&buf[1]);	/* Remove postfix '#' or '%' */
		    if (!lname) {
			free(filename);
			free(slinkname);
			code = ENOMEM;
			goto common_exit;
		    }
		    if (strlcpy(buf, mntroot, BUFSIZE) >= BUFSIZE) {
			free(filename);
			free(slinkname);
			code = EMSGSIZE;
			goto common_exit;
		    }
		    if (strlcat(buf, lname, BUFSIZE) >= BUFSIZE) {
			free(filename);
			free(slinkname);
			code = EMSGSIZE;
			goto common_exit;
		    }
		    free(lname);
		    lname = NULL;
		}

		unlink(filename);
		code = symlink(buf, filename);
		if (code) {
		    fprintf(stderr,
			    "Error creating symlink %s -> %s  code=%d;%d\n",
			    filename, buf, code, errno);
		}

		/* Remove the symbolic link */
		unlink(slinkname);
		free(slinkname);
		free(filename);
		slinkname = NULL;
		filename = NULL;

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


    *return_tag  = (afs_int32)tag;
    code = 0;
  common_exit:
    free(parentdir);
    free(lname);
    return code;
}

static int
WorkerBee(struct cmd_syndesc *as, void *arock)
{
    int code = 0, len;
    afs_int32 type, count, vcount;
    DIR *dirP, *dirQ;
    struct dirent *dirE, *dirF;
    char *name = NULL;
    char *thisdir, *t;
    struct DumpHeader dh;	/* Defined in dump.h */
    thisdir = calloc(1, MAXPATHLEN+4);
    if (!thisdir)
	goto mem_error_exit;

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
	if (strlcpy(rootdir, as->parms[1].items->data, sizeof(rootdir)) >= sizeof(rootdir))
	    goto str_error_exit;
    } else {
	strcpy(rootdir, ".");
    }
    if (strlcat(rootdir, "/", sizeof(rootdir)) >= sizeof(rootdir))
	goto str_error_exit;

    /* Append the RW volume name to the root directory */
    if (strlcat(rootdir, dh.volumeName, sizeof(rootdir)) >= sizeof(rootdir))
	goto str_error_exit;
    len = strlen(rootdir);
    if (len >= 7 && strcmp(".backup", rootdir + len - 7) == 0) {
	rootdir[len - 7] = 0;
    } else if (len >= 9 && strcmp(".readonly", rootdir + len - 9) == 0) {
	rootdir[len - 9] = 0;
    }

    /* Append the extension we asked for */
    if (as->parms[2].items) {
	if (strlcat(rootdir, as->parms[2].items->data, sizeof(rootdir)) >= sizeof(rootdir))    /* -extension <ext> */
	    goto str_error_exit;
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
	if (strlcat(mntroot, "/", sizeof(mntroot)) >= sizeof(mntroot))	/* append '/' to end of it */
	    goto str_error_exit;
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
    if (strlcat(mntroot, "/", sizeof(mntroot)) >= sizeof(mntroot))	/* append '/' to end of it */
	goto str_error_exit;

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
	for (vcount = 1; type == 3; vcount++) {
	    code = ReadVNode(vcount, &type);
	    if (code == ENOMEM)
		goto mem_error_exit;
	    if (code == EMSGSIZE)
		goto str_error_exit;
	}
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
		if (asprintf(&name, "%s" OS_DIRSEP "%s", rootdir,
			 dirE->d_name) < 0)
		    goto mem_error_exit;

		dirQ = opendir(name);
		free(name);
		name = NULL;
		while (dirQ && (dirF = readdir(dirQ))) {
		    if (strncmp(dirF->d_name, AFILE, strlen(AFILE)) == 0) {
			if (asprintf(&name, "%s" OS_DIRSEP "%s/%s",
				 rootdir, dirE->d_name, dirF->d_name) < 0)
			    goto mem_error_exit;

			unlink(name);
			free(name);
			name = NULL;
		    }
		}
		if (dirQ)
		    closedir(dirQ);
	    } else if (strncmp(dirE->d_name, AFILE, strlen(AFILE)) == 0) {
		if (asprintf(&name, "%s" OS_DIRSEP "%s", rootdir,
			 dirE->d_name) < 0)
		    goto mem_error_exit;

		unlink(name);
		free(name);
		name = NULL;
	    }
	}
	if (dirP)
	    closedir(dirP);
    }

    /* Now go through and remove all the directory links */
    dirP = opendir(rootdir);
    while (dirP && (dirE = readdir(dirP))) {
	if (strncmp(dirE->d_name, ADIR, strlen(ADIR)) == 0) {
	    if (asprintf(&name, "%s" OS_DIRSEP "%s", rootdir,
		     dirE->d_name) < 0)
		goto mem_error_exit;

	    unlink(name);
	    free(name);
	    name = NULL;
	}
    }
    if (dirP)
	closedir(dirP);

    free(thisdir);
    return (code);

  mem_error_exit:
    /* Error allocating memory -- quick exit */
    fprintf(stderr, "Memory allocation error!\n");
    free(thisdir);
    return -1;

  str_error_exit:
    /* Str length error */
    fprintf(stderr, "String exceeded buffer size\n");
    free(thisdir);
    return -1;
}

int
main(int argc, char **argv)
{
    struct cmd_syndesc *ts;

    setlinebuf(stdout);

    ts = cmd_CreateSyntax(NULL, WorkerBee, NULL, 0, "vldb check");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL, "dump file");
    cmd_AddParm(ts, "-dir", CMD_SINGLE, CMD_OPTIONAL, "restore dir");
    cmd_AddParm(ts, "-extension", CMD_SINGLE, CMD_OPTIONAL, "name extension");
    cmd_AddParm(ts, "-mountpoint", CMD_SINGLE, CMD_OPTIONAL,
		"mount point root");
    cmd_AddParm(ts, "-umask", CMD_SINGLE, CMD_OPTIONAL, "mode mask");

    return cmd_Dispatch(argc, argv);
}
