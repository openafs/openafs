/*
 * Copyright 1987 by MIT Student Information Processing Board
 *
 * For copyright info, see mit-sipb-cr.h.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/afsutil.h>

RCSID
    ("$Header$");

#include "error_table.h"
#include "mit-sipb-cr.h"
#include "internal.h"

#ifndef	lint
static const char copyright[] =
    "Copyright 1987,1988 by Student Information Processing Board, Massachusetts Institute of Technology";
#endif

static char buf[6];

const char *
error_table_name(afs_int32 num)
{
    return error_table_name_r(num, buf);
}
