/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#ifndef DJGPP
#include <windows.h>
#endif /* !DJGPP */
#include <string.h>
#include <malloc.h>
#include <osi.h>
#include <rx/rx.h>

#include "afsd.h"

/* compute how many 32 byte entries an AFS 3 dir requires for storing
 * the specified name.
 */
long cm_NameEntries(char *namep, long *lenp)
{
    long i;
        
    i = (long)strlen(namep);
    if (lenp) *lenp = i;
    return 1+((i+16)>>5);
}	
