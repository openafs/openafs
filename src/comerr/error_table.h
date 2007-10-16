/*
 * Copyright 1988 by the Student Information Processing Board of the
 * Massachusetts Institute of Technology.
 *
 * For copyright info, see mit-sipb-cr.h.
 */

#include <afs/param.h>

#include <errno.h>

#ifndef _AFS_ET_H

struct error_table {
    char const *const *msgs;
    afs_int32 base;
    int n_msgs;
};
struct et_list {
    struct et_list *next;
    const struct error_table *table;
};


#define	ERRCODE_RANGE	8	/* # of bits to shift table number */
#define	BITS_PER_CHAR	6	/* # bits to shift per character in name */

extern char const *afs_error_table_name(afs_int32 num);
extern void afs_add_to_error_table(struct et_list *new_table);
#ifdef AFS_OLD_COM_ERR
#define error_table_name        afs_error_table_name
#define add_to_error_table(X)   afs_add_to_error_table(X)
#endif /* AFS_OLD_COM_ERR */
#define _AFS_ET_H
#endif
