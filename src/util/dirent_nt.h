/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This is an NT version of DIR and dirent. Only d_name is present in the
 * dirent struct since that is all we currently need and implement.
 *
 */

#ifndef _DIRENT_H_
#define _DIRENT_H_

#include <windows.h>
#include <winbase.h>

typedef struct dirent {
    char *d_name;
} dirent_t;

typedef struct {
    HANDLE h;
    WIN32_FIND_DATA data;
    struct dirent cdirent;
    int first;
} DIR;

extern DIR *opendir(const char *path);
extern int closedir(DIR * dir);
extern struct dirent *readdir(DIR * dir);


#endif /* _DIRENT_H_ */
