/*
 * Copyright (c) 2003, 2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#import <Foundation/Foundation.h>
#import <stdio.h>
#import <err.h>
#import <sys/types.h>
#import <sys/mount.h>
#import <sys/sysctl.h>
#import <afs/sysctl.h>

enum Type {
    TypeNode = 0,
    TypeNum,
    TypeStr
};

typedef struct _setting {
    NSString *key;
    int selector;
    enum Type type;
    struct _setting *children;
} Setting;

Setting s_darwin_all[] = {
    {@"RealModes", AFS_SC_DARWIN_ALL_REALMODES, TypeNum, NULL},
    {NULL, 0, 0, NULL}
};
Setting s_darwin[] = {
    {@"All", AFS_SC_DARWIN_ALL, TypeNode, s_darwin_all},
    {@"Darwin12", AFS_SC_DARWIN_12, TypeNode, NULL},
    {@"Darwin13", AFS_SC_DARWIN_13, TypeNode, NULL},
    {@"Darwin14", AFS_SC_DARWIN_14, TypeNode, NULL},
    {@"Darwin60", AFS_SC_DARWIN_60, TypeNode, NULL},
    {@"Darwin70", AFS_SC_DARWIN_70, TypeNode, NULL},
    {@"Darwin80", AFS_SC_DARWIN_80, TypeNode, NULL},
    {@"Darwin90", AFS_SC_DARWIN_90, TypeNode, NULL},
    {NULL, 0, 0, NULL}
};
Setting s_first[] = {
    {@"All", AFS_SC_ALL, TypeNode, NULL},
    {@"Darwin", AFS_SC_DARWIN, TypeNode, s_darwin},
    {NULL, 0, 0, NULL}
};
Setting s_top = {NULL, -1, TypeNode, s_first};

int oid[CTL_MAXNAME] = {CTL_VFS};
NSString *path = @"/var/db/openafs/etc/config/settings.plist";

char *oidString(int *oid, int len);
void init(void);
void walk(id obj, Setting *s, int level);

void
init(void)
{
    int oidmax[] = {CTL_VFS, VFS_GENERIC, VFS_MAXTYPENUM};
    int oidvfs[] = {CTL_VFS, VFS_GENERIC, VFS_CONF, 0};
    int max;
    struct vfsconf conf;
    size_t len;
    int i;

    len = sizeof(max);
    if(sysctl(oidmax, 3, &max, &len, NULL, 0) < 0)
	err(1, "sysctl VFS_MAXTYPENUM");
    for(i = max; --i >= 0; ) {
	oidvfs[3] = i;
	len = sizeof(conf);
	if(sysctl(oidvfs, 4, &conf, &len, NULL, 0) < 0)
	    continue;
	if(strcmp("afs", conf.vfc_name) == 0) {
	    s_top.selector = conf.vfc_typenum;
	    break;
	}
    }
    if(s_top.selector < 0)
	errx(1, "AFS is not loaded");
}

char *
oidString(int *oid, int len)
{
    static char buf[256];
    char *cp = buf;

    for(;;) {
	sprintf(cp, "%d", *oid++);
	if(--len <= 0)
	    break;
	cp += strlen(cp);
	*cp++ = '.';
    }
    return buf;
}

void
walk(id obj, Setting *s, int level)
{
    Setting *child;
    id newobj;
    int intval;
    const char *cp;
    int level1 = level + 1;

    oid[level] = s->selector;
    switch(s->type) {
      case TypeNode:
	for(child = s->children; child->key; child++) {
	    if(child->type == TypeNode && !child->children)
		continue;
	    newobj = [obj objectForKey: child->key];
	    if(newobj)
		walk(newobj, child, level1);
	}
	break;
      case TypeNum:
	intval = [obj intValue];
	if(sysctl(oid, level1, NULL, NULL, &intval, sizeof(intval)) < 0)
	    err(1, "sysctl %s => %d", oidString(oid, level1), intval);
	break;
      case TypeStr:
	cp = [obj UTF8String];
	if(sysctl(oid, level1, NULL, NULL, (void *)cp, strlen(cp)) < 0)
	    err(1, "sysctl %s => %s", oidString(oid, level1), cp);
	break;
    }
}

main()
{
    NSData *plistData;
    id plist;
    NSString *error;
    NSPropertyListFormat format;
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    init();
    plistData = [NSData dataWithContentsOfFile: path];
    if(plistData) {
	plist = [NSPropertyListSerialization propertyListFromData: plistData
	  mutabilityOption: NSPropertyListImmutable
	  format: &format
	  errorDescription: &error
	];
	if(plist)
	    walk(plist, &s_top, 1);
	else
	    errx(1, "%s: %s", [path UTF8String], [error UTF8String]);
    }

    [pool release];
    return 0;
}
