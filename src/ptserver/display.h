/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _PTSERVER_DISPLAY_H
#define _PTSERVER_DISPLAY_H

extern int pr_PrintEntry(FILE *f, int hostOrder, afs_int32 ea,
			 struct prentry *e, int indent);
extern int pr_PrintContEntry(FILE *f, int hostOrder, afs_int32 ea,
			 struct contentry *e, int indent);

#endif
