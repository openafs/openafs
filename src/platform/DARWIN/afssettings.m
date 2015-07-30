/*
 * Copyright (c) 1992, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <ctype.h>

#include <afs/sysctl.h>

#include <CoreFoundation/CFString.h>
#include <Foundation/Foundation.h>

enum Type {
    Node = 0,
    LeafNum,
    LeafStr
};

typedef struct oidsetting {
    NSString *key;
    int selector;
    enum Type type;
    struct oidsetting *children;
} Setting;

int
mygetvfsbyname(const char *fsname, struct vfsconf *vfcp, Setting *s, int myoid[])
{
    int maxtypenum, cnt;
    size_t buflen;
    
    myoid[0] = CTL_VFS;
    myoid[1] = VFS_GENERIC;
    myoid[2] = VFS_MAXTYPENUM;
    buflen = 4;
    if (sysctl(myoid, 3, &maxtypenum, &buflen, (void *)0, (size_t)0) < 0)
    {
	return (-1);
    }
    myoid[2] = VFS_CONF;
    buflen = sizeof * vfcp;
    for (cnt = 0; cnt < maxtypenum; cnt++)
    {
	myoid[3] = cnt;
	if (sysctl(myoid, 4, vfcp, &buflen, (void *)0, (size_t)0) < 0)
	{
	    if (errno != EOPNOTSUPP && errno != ENOENT && errno != ENOTSUP)
	    {
		return (-1);
	    }
	    continue;
	}
	if (!strcmp(fsname, vfcp->vfc_name))
	{
	    s->selector = vfcp->vfc_typenum;
	    return (0);
	}
    }
    errno = ENOENT;
    return (-1);
}

void
oiderror(int *oid, int oidlen, enum Type type, void *obj)
{
    fprintf(stderr, "sysctl ");
    while (1) {
	fprintf(stderr, "%d", *oid++);
	if(--oidlen <= 0)
	    break;
	fprintf(stderr, "%c", '.');
    }
    fprintf(stderr, " => ");
    if (type == LeafNum)
	fprintf(stderr, "%d\n", *(int *)obj);
    else
	fprintf(stderr, "%s\n", (char *)obj);

    return;
}

void
recurse(id obj, Setting *s, int level, int myoid[])
{
    Setting *child;
    id newobj;
    int intval;
    const char *cp;

    myoid[level] = s->selector;
    switch(s->type) {
    case Node:
	for(child = s->children; child->key; child++) {
	    if(child->type == Node && !child->children)
		continue;
	    newobj = [obj objectForKey: child->key];
	    if(newobj)
	      recurse(newobj, child, level+1, myoid);
	}
	break;
    case LeafNum:
	intval = [obj intValue];
	if(sysctl(myoid, level+1, NULL, NULL, &intval, sizeof(intval)) < 0)
	    oiderror(myoid, level+1, s->type, &intval);
	break;
    case LeafStr:
	cp = [obj UTF8String];
	if(sysctl(myoid, level+1, NULL, NULL, (void *)cp, strlen(cp)) < 0)
	    oiderror(myoid, level+1, s->type, &cp);
	break;
    }
}

Setting sysctl_darwin_all[] = {
    {@"RealModes", AFS_SC_DARWIN_ALL_REALMODES, LeafNum, NULL},
    {@"FSEvents", AFS_SC_DARWIN_ALL_FSEVENTS, LeafNum, NULL},
    {NULL, 0, 0, NULL}
};
Setting sysctl_darwin[] = {
    {@"All", AFS_SC_DARWIN_ALL, Node, sysctl_darwin_all},
    {@"Darwin60", AFS_SC_DARWIN_60, Node, NULL},
    {@"Darwin70", AFS_SC_DARWIN_70, Node, NULL},
    {@"Darwin80", AFS_SC_DARWIN_80, Node, NULL},
    {@"Darwin90", AFS_SC_DARWIN_90, Node, NULL},
    {@"Darwin100", AFS_SC_DARWIN_100, Node, NULL},
    {@"Darwin110", AFS_SC_DARWIN_110, Node, NULL},
    {@"Darwin120", AFS_SC_DARWIN_120, Node, NULL},
    {@"Darwin130", AFS_SC_DARWIN_130, Node, NULL},
    {@"Darwin140", AFS_SC_DARWIN_140, Node, NULL},
    {NULL, 0, 0, NULL}
};
Setting sysctl_first[] = {
    {@"All", AFS_SC_ALL, Node, NULL},
    {@"Darwin", AFS_SC_DARWIN, Node, sysctl_darwin},
    {NULL, 0, 0, NULL}
};
Setting sysctl_top = {NULL, -1, Node, sysctl_first};

int
main(int argc, char **argv)
{
    struct vfsconf vfcp;
    NSData *plistData;
    id plist;
    NSAutoreleasePool * nspool = [[NSAutoreleasePool alloc] init];
    NSString *plistpath = @"/var/db/openafs/etc/config/settings.plist";
    int oid[CTL_MAXNAME] = {CTL_VFS};

    if (mygetvfsbyname("afs", &vfcp, &sysctl_top, oid) != 0)
	exit(-1);
    plistData = [NSData dataWithContentsOfFile: plistpath];
    if(plistData) {
	plist = [NSPropertyListSerialization propertyListWithData: plistData
					     options: NSPropertyListImmutable
					     format: NULL
					     error: NULL
	    ];
	if (!plist) {
	    NSLog(@"Error reading plist from file '%s'", [plistpath UTF8String]);
	    [nspool release];
	    return -1;
	}

	recurse(plist, &sysctl_top, 1, oid);
    }
    [nspool release];
    return 0;
}
