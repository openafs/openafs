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

RCSID("$Header: /tmp/cvstemp/openafs/src/volser/common.c,v 1.1.1.6 2001/10/14 18:07:28 hartmans Exp $");

#include <stdio.h>
#include <afs/afsutil.h>
#include <afs/com_err.h>

Log(a,b,c,d,e,f)
char *a, *b, *c, *d, *e, *f; 
{
	ViceLog(0, (a, b,c, d, e, f)); 
}

LogError(errcode)
afs_int32 errcode;
{
    ViceLog(0, ("%s: %s\n", error_table_name(errcode),error_message(errcode)));
}

Abort(s,a,b,c,d,e,f,g,h,i,j) 
char *s;
{
    ViceLog(0, ("Program aborted: "));
    ViceLog(0, (s,a,b,c,d,e,f,g,h,i,j));
    abort();
}

void
InitErrTabs()
{
    initialize_KA_error_table();
    initialize_RXK_error_table();
    initialize_KTC_error_table();
    initialize_ACFG_error_table();
    initialize_CMD_error_table();
    initialize_VL_error_table();
    initialize_VOLS_error_table();
    return;
}
