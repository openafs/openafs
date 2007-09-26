/*
 * Copyright (c) 2005
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the University of Michigan as to its fitness for any
 * purpose, and without warranty by the University of
 * Michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the University of Michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

/*
 * adapter routine: MIT .et/.c/.h files to TRANSARC comerr runtime
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <errno.h>
#include <com_err.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

struct et_list {
    struct et_list *next;
    const struct error_table * table;
};

long
add_error_table(const struct error_table *et)
{
    struct et_list *list;

    list = (struct et_list *) malloc(sizeof *list);
    if (!list) return errno;
    list->table = et;
    list->next = 0;
    add_to_error_table(list);
    return 0;
}

long
remove_error_table(const struct error_table *et)
{
    return EDOM;
}
