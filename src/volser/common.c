/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef lint
#endif
#include <afs/param.h>
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

InitErrTabs()
{
    initialize_ka_error_table();
    initialize_rxk_error_table();
    initialize_ktc_error_table();
    initialize_acfg_error_table();
    initialize_cmd_error_table();
    initialize_vl_error_table();
    initialize_vols_error_table();
    return;
}
