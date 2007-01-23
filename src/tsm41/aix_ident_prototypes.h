/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _AFS_TSM41_AIX_IDENT_PROTOTYPES_H
#define _AFS_TSM41_AIX_IDENT_PROTOTYPES_H

extern int afs_getgrset(char *userName);
extern struct group * afs_getgrgid(gid_t);
extern struct group * afs_getgrnam(char *);
extern struct passwd * afs_getpwuid(uid_t);
extern struct passwd * afs_getpwnam(const char *);

#endif /* _AFS_TSM41_AIX_IDENT_PROTOTYPES_H */
