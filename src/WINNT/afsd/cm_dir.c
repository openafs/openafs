/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <osi.h>
#include <rx/rx.h>

#include "afsd.h"

/* compute how many 32 byte entries an AFS 3 dir requires for storing
 * the specified name.
 */
cm_NameEntries(char *namep, size_t *lenp)
{
	int i;
        
        i = strlen(namep);
	if (lenp) *lenp = i;
        return 1+((i+16)>>5);
}
