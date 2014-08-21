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
#include <roken.h>

#include <afs/stds.h>
#include <afs/com_err.h>
#include <afs/afs_consts.h>

#include <windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <strsafe.h>
#include <stdio.h>
#include <time.h>
#include <winsock2.h>
#include <errno.h>
#include <assert.h>

#include <osi.h>
#include <afsint.h>
#include <WINNT\afsreg.h>

#include "fs_utils.h"
#include "cmd.h"
#include "afsd.h"
#include "cm_ioctl.h"

#define MAXNAME 100
#define MAXINSIZE 1300    /* pioctl complains if data is larger than this */

static char space[AFS_PIOCTL_MAXSIZE];

static char pn[] = "symlink";
static int rxInitDone = 0;

static int
ListLinkCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    int error;
    struct cmd_item *ti;
    char orig_name[1024];               /* Original name */
    char true_name[1024];		/*``True'' dirname (e.g., symlink target)*/
    char parent_dir[1024];		/*Parent directory of true name*/
    char *last_component;	        /*Last component of true name*/
    cm_ioctlQueryOptions_t options;

    error = 0;
    for(ti=as->parms[0].items; ti; ti=ti->next) {
        cm_fid_t fid;
        afs_uint32 filetype;

	/* once per file */
        if( FAILED(StringCbCopy(orig_name, sizeof(orig_name), ti->data))) {
            fprintf (stderr, "ListLinkCmd - input path too long");
            error = 1;
            continue;
        }

        if( FAILED(StringCbCopy(true_name, sizeof(true_name), ti->data))) {
            fprintf (stderr, "ListLinkCmd - input path too long");
            error = 1;
            continue;
        }

	/*
	 * Find rightmost slash, if any.
	 */
	last_component = (char *) strrchr(true_name, '\\');
	if (!last_component)
	    last_component = (char *) strrchr(true_name, '/');
	if (last_component) {
	    /*
	     * Found it.  Designate everything before it as the parent directory,
	     * everything after it as the final component.  (explicitly relying
             * on behavior of strncpy in this case.)
	     */
	    strncpy(parent_dir, true_name, last_component - true_name + 1);
	    parent_dir[last_component - true_name + 1] = 0;
	    last_component++;   /*Skip the slash*/

            /*
             * The following hack converts \\afs\foobar to \\afs\all\foobar.
             * However, this hack should no longer be required as the pioctl()
             * interface handles this conversion for us.
             */
	    if (!fs_InAFS(parent_dir)) {
		const char * nbname = fs_NetbiosName();
		size_t len = strlen(nbname);

		if (parent_dir[0] == '\\' && parent_dir[1] == '\\' &&
		    parent_dir[len+2] == '\\' &&
		    parent_dir[len+3] == '\0' &&
		    !strnicmp(nbname,&parent_dir[2],len))
		{
		    if( FAILED(StringCbPrintf(parent_dir, sizeof(parent_dir), "\\\\%s\\all\\", nbname))) {
                        fprintf(stderr, "parent_dir cannot be populated");
                        error = 1;
                        continue;
                    }
		} else {
                    fprintf(stderr,"'%s' is not in AFS.\n", parent_dir);
                    error = 1;
                    continue;
                }
	    }
	}
	else
        {
	    /*
	     * No slash appears in the given file name.  Set parent_dir to the current
	     * directory, and the last component as the given name.
	     */
	    fs_ExtractDriveLetter(true_name, parent_dir);
	    if( FAILED(StringCbCat(parent_dir, sizeof(parent_dir), "."))) {
	        fprintf (stderr, "parent_dir - not enough space");
                exit(1);
	    }
	    last_component = true_name;
            fs_StripDriveLetter(true_name, true_name, sizeof(true_name));
	}

	if (strcmp(last_component, ".") == 0 || strcmp(last_component, "..") == 0) {
	    fprintf(stderr,"%s: you may not use '.' or '..' as the last component\n", pn);
	    fprintf(stderr,"%s: of a name in the 'symlink list' command.\n", pn);
	    error = 1;
	    continue;
	}

        /* Check the object type */
        memset(&fid, 0, sizeof(fid));
        memset(&options, 0, sizeof(options));
        filetype = 0;
        options.size = sizeof(options);
        options.field_flags |= CM_IOCTL_QOPTS_FIELD_LITERAL;
        options.literal = 1;
	blob.in_size = options.size;    /* no variable length data */
        blob.in = &options;

        blob.out_size = sizeof(cm_fid_t);
        blob.out = (char *) &fid;
        if (0 == pioctl_utf8(orig_name, VIOCGETFID, &blob, 1) &&
            blob.out_size == sizeof(cm_fid_t)) {
            options.field_flags |= CM_IOCTL_QOPTS_FIELD_FID;
            options.fid = fid;
        } else {
	    fs_Die(errno, ti->data);
	    error = 1;
	    continue;
        }

        blob.out_size = sizeof(filetype);
        blob.out = &filetype;

        code = pioctl_utf8(orig_name, VIOC_GETFILETYPE, &blob, 1);
        if (code || blob.out_size != sizeof(filetype)) {
	    fs_Die(errno, ti->data);
	    error = 1;
	    continue;
        }

        if (filetype != 3 /* symlink */) {
            fprintf(stderr,"'%s' is a %s not a Symlink.\n",
                    orig_name, fs_filetypestr(filetype));
            error = 1;
            continue;
        }

	blob.in = last_component;
	blob.in_size = (long)strlen(last_component)+1;

        /* Now determine the symlink target */
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.out = space;
	memset(space, 0, AFS_PIOCTL_MAXSIZE);

	code = pioctl_utf8(parent_dir, VIOC_LISTSYMLINK, &blob, 1);
	if (code == 0)
	    printf("'%s' is a symlink to '%.*s'\n",
		   orig_name,
		   blob.out_size,
                   space);
	else {
	    error = 1;
	    if (errno == EINVAL)
		fprintf(stderr,"'%s' is not a symlink.\n",
		       ti->data);
	    else {
		fs_Die(errno, (ti->data ? ti->data : parent_dir));
	    }
	}
    }
    return error;
}

static int
MakeLinkCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    char * parent;
    char link[1024] = "";
    char target_buf[1024] = "";
    char *target = target_buf;
    const char * nbname;
    int nblen;

    if( FAILED(StringCbCopy(link, sizeof(link), as->parms[0].items->data))) {
        fprintf (stderr, "MakeLinkCmd - link path too long");
        exit(1);
    }
    if( FAILED(StringCbCopy(target, sizeof(target_buf), as->parms[1].items->data))) {
        fprintf (stderr, "MakeLinkCmd - target path too long");
        exit(1);
    }
    parent = fs_GetParent(link);

    nbname = fs_NetbiosName();
    nblen = (int)strlen(nbname);

    if (!fs_InAFS(parent)) {
	if (parent[0] == '\\' && parent[1] == '\\' &&
	    (parent[nblen+2] == '\\' && parent[nblen+3] == '\0' || parent[nblen+2] == '\0') &&
	    !strnicmp(nbname,&parent[2],nblen))
	{
	    if( FAILED(StringCbPrintf(link, sizeof(link),
                                      "%s%sall%s", parent, parent[nblen+2]?"":"\\",
                                      &as->parms[0].items->data[strlen(parent)]))) {
                fprintf(stderr, "link cannot be populated\n");
                exit(1);
            }
	    parent = fs_GetParent(link);
	} else {
            fprintf(stderr,"%s: symlinks must be created within the AFS file system\n", pn);
            return 1;
        }
    }

    if ( fs_IsFreelanceRoot(parent) && !fs_IsAdmin() ) {
	fprintf(stderr,"%s: Only AFS Client Administrators may alter the root.afs volume\n", pn);
	return 1;
    }

    /*
     * Fix up the target path to conform to unix notation
     * if it is a UNC path to the AFS server name.
     */
    if (target[0] == '\\' && target[1] == '\\' &&
        target[nblen+2] == '\\' &&
		!strnicmp(nbname,&target[2],nblen))
    {
        char *p;
        for ( p=target; *p; p++) {
            if (*p == '\\')
                *p = '/';
        }
        target++;
    }
    fprintf(stderr, "Creating symlink [%s] to [%s]\n", link, target);

    /* create symlink with a special pioctl for Windows NT, since it doesn't
     * have a symlink system call.
     */
    blob.out_size = 0;
    blob.in_size = 1 + (long)strlen(target);
    blob.in = target;
    blob.out = NULL;
    code = pioctl_utf8(link, VIOC_SYMLINK, &blob, 0);
    if (code) {
	fs_Die(errno, as->parms[0].items->data);
	return 1;
    }
    return 0;
}

/*
 * Delete AFS symlinks.  Variables are used as follows:
 *       tbuffer: Set to point to the null-terminated directory name of the
 *	    symlink (or ``.'' if none is provided)
 *      tp: Set to point to the actual name of the symlink to nuke.
 */
static int
RemoveLinkCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code=0;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    char tbuffer[1024];
    char lsbuffer[1024];
    char *tp;

    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	tp = (char *) strrchr(ti->data, '\\');
	if (!tp)
	    tp = (char *) strrchr(ti->data, '/');
	if (tp) {
	    strncpy(tbuffer, ti->data, code=(afs_int32)(tp-ti->data+1));  /* the dir name */
            tbuffer[code] = 0;
	    tp++;   /* skip the slash */

	    if (!fs_InAFS(tbuffer)) {
		const char * nbname = fs_NetbiosName();
		int len = (int)strlen(nbname);

		if (tbuffer[0] == '\\' && tbuffer[1] == '\\' &&
		     tbuffer[len+2] == '\\' &&
		     tbuffer[len+3] == '\0' &&
		     !strnicmp(nbname,&tbuffer[2],len))
		{
		    if( FAILED(StringCbPrintf(tbuffer, sizeof(tbuffer),
                                              "\\\\%s\\all\\", nbname))) {
                        fprintf( stderr, "tbuffer cannot be populated\n");
                        exit(1);
                    }
		} else {
                    fprintf(stderr,"'%s' is not in AFS.\n", tbuffer);
                    code = 1;
                    continue;
		}
	    }
	}
	else
        {
	    fs_ExtractDriveLetter(ti->data, tbuffer);
	    if( FAILED(StringCbCat(tbuffer, sizeof(tbuffer), "."))) {
	        fprintf (stderr, "tbuffer - not enough space");
                exit(1);
	    }
	    tp = ti->data;
            fs_StripDriveLetter(tp, tp, strlen(tp) + 1);
	}
	blob.in = tp;
	blob.in_size = (int)strlen(tp)+1;
	blob.out = lsbuffer;
	blob.out_size = sizeof(lsbuffer);
	code = pioctl_utf8(tbuffer, VIOC_LISTSYMLINK, &blob, 0);
	if (code) {
	    if (errno == EINVAL)
		fprintf(stderr,"symlink: '%s' is not a symlink.\n", ti->data);
	    else {
		fs_Die(errno, ti->data);
	    }
	    continue;	/* don't bother trying */
	}

        if ( fs_IsFreelanceRoot(tbuffer) && !fs_IsAdmin() ) {
            fprintf(stderr,"symlink: Only AFS Client Administrators may alter the root.afs volume\n");
            code = 1;
            continue;   /* skip */
        }

	blob.out_size = 0;
	blob.in = tp;
	blob.in_size = (long)strlen(tp)+1;
	code = pioctl_utf8(tbuffer, VIOC_DELSYMLINK, &blob, 0);
	if (code) {
	    fs_Die(errno, ti->data);
	}
    }
    return code;
}

static struct ViceIoctl gblob;
static int debug = 0;

int
wmain(int argc, wchar_t **wargv)
{
    afs_int32 code;
    struct cmd_syndesc *ts;
    char ** argv;

    WSADATA WSAjunk;
    WSAStartup(0x0101, &WSAjunk);

    fs_SetProcessName(pn);

    /* try to find volume location information */

    argv = fs_MakeUtf8Cmdline(argc, wargv);

    osi_Init();

    ts = cmd_CreateSyntax("list", ListLinkCmd, NULL, 0, "list symlink");
    cmd_AddParm(ts, "-name", CMD_LIST, 0, "name");

    ts = cmd_CreateSyntax("make", MakeLinkCmd, NULL, 0, "make symlink");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "name");
    cmd_AddParm(ts, "-to", CMD_SINGLE, 0, "target");

    ts = cmd_CreateSyntax("remove", RemoveLinkCmd, NULL, 0, "remove symlink");
    cmd_AddParm(ts, "-name", CMD_LIST, 0, "name");
    cmd_CreateAlias(ts, "rm");

    code = cmd_Dispatch(argc, argv);

    fs_FreeUtf8CmdLine(argc, argv);

    return code;
}
