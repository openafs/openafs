#include <afsconfig.h>
#ifndef HAVE_ADD_TO_ERROR_TABLE
#include <afs/stds.h>
#include <afs/com_err.h>

/* #define error_table error_table_compat */
#include <afs/error_table.h>
/* #undef error_table */

#ifndef HAVE_ADD_ERROR_TABLE
void add_error_table (const struct error_table *);
#endif /* !HAVE_ADD_ERROR_TABLE */

void
add_to_error_table(struct et_list *new_table)
{
	add_error_table((struct error_table *) new_table->table);
}
#endif /* HAVE_ADD_TO_ERROR_TABLE */
