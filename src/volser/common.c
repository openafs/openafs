
#ifndef lint
#endif

/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

/*===============================================================
 * Copyright (C) 1989 Transarc Corporation - All rights reserved 
 *===============================================================*/

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
