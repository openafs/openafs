/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */


#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include "osi.h"
#include <assert.h>

/* table of dynamic lock types.  First entry isn't used, since those are
 * wired in for performance reasons.
 */
osi_lockOps_t *osi_lockOps[OSI_NLOCKTYPES];
char *osi_lockOpNames[OSI_NLOCKTYPES];

/* first free slot in lock operations; slot 0 is not used */
int osi_lockTypeIndex = 1;

/* type to create generically */
int osi_lockTypeDefault = 0;

int osi_LockTypeFind(char *namep)
{
	int i;

	for(i=1; i<osi_lockTypeIndex; i++) {
		if (!strcmp(osi_lockOpNames[i], namep)) return i;
	}
	return -1;
}

void osi_LockTypeAdd(osi_lockOps_t *statOps, char *namep, int *indexp)
{
	int i;
	if ((i = osi_lockTypeIndex) >= OSI_NLOCKTYPES) return;
	osi_lockOps[i] = statOps;
	osi_lockOpNames[i] = namep;
	*indexp = i;
	osi_lockTypeIndex = i+1;
}

osi_LockTypeSetDefault(char *namep)
{
	int index;

	if (namep == (char *) 0)
		osi_lockTypeDefault = 0;
	else {
		index = osi_LockTypeFind(namep);
		if (index > 0) osi_lockTypeDefault = index;
	}
	return 0;
}

