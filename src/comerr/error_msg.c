/*
 * $Locker$
 *
 * Copyright 1987 by the Student Information Processing Board
 * of the Massachusetts Institute of Technology
 *
 * For copyright info, see "mit-sipb-cr.h".
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "internal.h"
#include <stdio.h>
#include "error_table.h"
#include "mit-sipb-cr.h"
#include <afs/errors.h>
#include <string.h>
#include "com_err.h"

static const char copyright[] =
    "Copyright 1986, 1987, 1988 by the Student Information Processing Board\nand the department of Information Systems\nof the Massachusetts Institute of Technology";

static char buffer[64];

struct et_list *_et_list = (struct et_list *)NULL;
struct et_list *_et_dynamic_list = (struct et_list *)NULL;

#ifdef AFS_PTHREAD_ENV
#include <pthread.h>
#include <assert.h>

/*
 * This mutex protects the following variables:
 * _et_list
 */

static pthread_mutex_t et_list_mutex;
static int et_list_done = 0;
static pthread_once_t et_list_once = PTHREAD_ONCE_INIT;

/*
 * Function to initialize the et_list_mutex
 */

void
et_mutex_once(void)
{
    assert(!pthread_mutex_init
	   (&et_list_mutex, (const pthread_mutexattr_t *)0));
    et_list_done = 1;
}

#define LOCK_ET_LIST \
	do { \
	    (et_list_done || pthread_once(&et_list_once, et_mutex_once)); \
	    assert(pthread_mutex_lock(&et_list_mutex)==0); \
	} while (0)
#define UNLOCK_ET_LIST assert(pthread_mutex_unlock(&et_list_mutex)==0)
#else
#define LOCK_ET_LIST
#define UNLOCK_ET_LIST
#endif /* AFS_PTHREAD_ENV */


static const char char_set[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";


const char *
error_table_name_r(afs_int32 num, char *buf)
{
    int ch;
    int i;
    char *p;

    /* num = aa aaa abb bbb bcc ccc cdd ddd d?? ??? ??? */
    p = buf;
    num >>= ERRCODE_RANGE;
    /* num = ?? ??? ??? aaa aaa bbb bbb ccc ccc ddd ddd */
    num &= 077777777;
    /* num = 00 000 000 aaa aaa bbb bbb ccc ccc ddd ddd */
    for (i = 4; i >= 0; i--) {
	ch = (num >> BITS_PER_CHAR * i) & ((1 << BITS_PER_CHAR) - 1);
	if (ch != 0)
	    *p++ = char_set[ch - 1];
    }
    *p = '\0';
    return buf;
}

const char *
com_right(struct et_list *et, afs_int32 code)
{
    long unsigned offset;
    for ( ; et; et = et->next) {
	offset = code - et->table->base;
	if (et->table->n_msgs > offset) {
	    return et->table->msgs[offset];
	}
    }
    return 0;
}

const char *
error_message(afs_int32 code)
{
    int offset;
    struct et_list *et;
    int table_num;
    int started = 0;
    char *cp;
    static const char unknown_code[] = "Unknown code ";

    LOCK_ET_LIST;
    cp = (char *) com_right(_et_list, code);
    if (!cp)
	cp = (char *) com_right(_et_dynamic_list, code);
    UNLOCK_ET_LIST;
    if (cp)
	return cp;
    offset = code & ((1 << ERRCODE_RANGE) - 1);
    table_num = code - offset;
    if (!table_num) {
	if ((cp = strerror(offset)) != NULL)
	    return (cp);
    }
    memcpy(buffer, unknown_code, sizeof unknown_code);
    cp = buffer + strlen(buffer);
    if (table_num) {
	error_table_name_r(table_num, cp);
	cp += strlen(cp);
    }
    if (offset >= 100) {
	*cp++ = '0' + offset / 100;
	offset %= 100;
	started++;
    }
    if (started || offset >= 10) {
	*cp++ = '0' + offset / 10;
	offset %= 10;
    }
    *cp++ = '0' + offset;
    snprintf(cp, (buffer+sizeof buffer)-cp, " (%d)", code);
    return (buffer);
}

void
add_to_error_table(struct et_list *new_table)
{
    struct et_list *et;

    LOCK_ET_LIST;
    /*
     * Protect against adding the same error table twice
     */
    for (et = _et_list; et; et = et->next) {
	if (et->table->base == new_table->table->base) {
	    UNLOCK_ET_LIST;
	    return;
	}
    }

    new_table->next = _et_list;
    _et_list = new_table;
    UNLOCK_ET_LIST;
}

/*
 * New interfaces provided by mit krb5's com_err library
 */
int
add_error_table(const struct error_table * et)
{
    struct et_list *new, *x;

    if (!(new = (struct et_list *) malloc(sizeof(struct et_list))))
	return ENOMEM;

    new->table = et;
    LOCK_ET_LIST;
    for (x = _et_list; x; x = x->next) {
	if (x->table->base == et->base)
	    break;
    }
    if (!x) for (x = _et_dynamic_list; x; x = x->next) {
	if (x->table->base == et->base)
	    break;
    }
    if (!x) {
	new->next = _et_dynamic_list;
	_et_dynamic_list = new;
    }
    UNLOCK_ET_LIST;
    if (x) return EEXIST;
    return 0;
}

int
remove_error_table(const struct error_table * et)
{
    struct et_list *x, **epp;

    LOCK_ET_LIST;
    for (epp = &_et_dynamic_list; (x = *epp); epp = &x->next) {
	if (x->table != et)
	    continue;
	*epp = x->next;
	(void) free(x);
	break;
    }
    UNLOCK_ET_LIST;
    if (!x) return ENOENT;
    return 0;
}

/* mit */

int
init_error_table(const char * const *msgs, int base, int num)
{
    struct et_list *x, **epp;
    struct {
	struct et_list *et;
	struct error_table tab[1];
    } *f = 0;

    if (!base || !num || !msgs) return 0;

    LOCK_ET_LIST;
    for (epp = &_et_list; (x = *epp); epp = &x->next) {
	if (x->table->base == base)
	    break;
    }
    if (!x) {
	f = malloc(sizeof *f);
	if (f) {
	    f->tab->msgs = msgs;
	    f->tab->n_msgs = num;
	    f->tab->base = base;
	    f->et = malloc(sizeof *(f->et));
	    if(f->et) {
	      f->et->table = f->tab;
	      f->et->next = _et_list;
	    }
	    _et_list = f->et;
	}
    }
    UNLOCK_ET_LIST;
    if (x) return EEXIST;
    if (!f) return ENOMEM;
    return 0;
}

/* heimdal */

void
initialize_error_table_r(struct et_list **list, const char **msgs,
    int num, int base)
{
    struct et_list *new, *x, **epp;
    LOCK_ET_LIST;
    for (; (x = *list); list = &x->next) {
	if (x->table->base == base)
	    break;
    }
    if (!x) {
	struct {
	    struct et_list *et;
	    struct error_table tab[1];
	} *f = malloc(sizeof *f);
	if (f) {
	    f->tab->msgs = msgs;
	    f->tab->n_msgs = num;
	    f->tab->base = base;
	    f->et = malloc(sizeof *(f->et));
	    if(f->et) { 
	      f->et->table = f->tab;
	      f->et->next = 0;
	    }
	    *list = f->et;
	}
    }
    UNLOCK_ET_LIST;
}

void
free_error_table(struct et_list *et)
{
/* XXX fun fun fun -- how does locking get handled here? */
    struct et_list *next;

    for (; et; et = next) {
	next = et->next;
	free(et);
    }
}
