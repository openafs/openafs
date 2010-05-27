
/* This header file transforms the Heimdal config_parse.c profile
 * parser into an AFS profile parser, hiding the krb5-ness of the parser
 * behind the scenes.
 */

#include <afsconfig.h>
#include <afs/stds.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif

#define KRB5_LIB_FUNCTION static AFS_UNUSED
#define KRB5_LIB_CALL

/* This value shouldn't be hard coded */
#define KRB5_CONFIG_BADFORMAT                    (-1765328248L)

#define N_(X,Y) X

struct krb5_config_binding {
    enum { krb5_config_string, krb5_config_list } type;
    char *name;
    struct krb5_config_binding *next;
    union {
        char *string;
        struct krb5_config_binding *list;
        void *generic;
    } u;
};

typedef struct krb5_config_binding krb5_config_binding;

typedef krb5_config_binding krb5_config_section;
typedef krb5_config_section afs_config_section;

struct krb5_context_data {
    krb5_config_section *cf;
};

typedef struct krb5_context_data * krb5_context;
typedef int krb5_error_code;
typedef int krb5_boolean;
typedef time_t krb5_deltat;

static const void *_krb5_config_vget(krb5_context,
				     const krb5_config_section *, int,
				     va_list);
static const void *_krb5_config_vget_next(krb5_context,
					  const krb5_config_section *,
					  const krb5_config_binding **,
					  int, va_list);
static const char *krb5_config_vget_string(krb5_context,
					   const krb5_config_section *,
					   va_list);
static const krb5_config_binding * krb5_config_vget_list
	(krb5_context, const krb5_config_section *, va_list);
static krb5_error_code krb5_config_parse_file_multi
	(krb5_context, const char *, krb5_config_section **);
static krb5_error_code krb5_config_parse_file
	(krb5_context, const char *, krb5_config_section **);
static krb5_error_code krb5_config_file_free
	(krb5_context, krb5_config_section *);
static krb5_boolean krb5_config_vget_bool
	(krb5_context, const krb5_config_section *,va_list);
static int krb5_config_vget_int
	(krb5_context, const krb5_config_section *, va_list);

static krb5_error_code
krb5_string_to_deltat(const char *str, krb5_deltat *t) {
    return 1;
}

KRB5_LIB_FUNCTION void krb5_clear_error_message(krb5_context context) {
    return;
}

static void krb5_set_error_message(krb5_context context, krb5_error_code ret,
				   const char *fmt, ...) {
    return;
}

/* Play it safe, by saying we're always suid. */
static int issuid(void) {
    return 1;
}

static int _krb5_homedir_access(krb5_context context) {
    return 0;
}

static krb5_error_code
krb5_abortx(krb5_context context, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    vfprintf(stderr, fmt, ap);
    va_end(ap);

    abort();
}

/* Wrappers */

int
afs_config_parse_file_multi(const char *fname, afs_config_section **res) {
    return krb5_config_parse_file_multi(NULL, fname, res);
}

int
afs_config_parse_file(const char *fname, afs_config_section **res) {
    return krb5_config_parse_file(NULL, fname, res);
}

int
afs_config_file_free(afs_config_section *s) {
    return krb5_config_file_free(NULL, s);
}

const char*
afs_config_get_string(const afs_config_section *c, ...)
{
    const char *ret;
    va_list args;

    assert(c != NULL);

    va_start(args, c);
    ret = krb5_config_vget_string (NULL, c, args);
    va_end(args);
    return ret;
}

int
afs_config_get_bool(const afs_config_section *c, ...)
{
    va_list ap;
    krb5_boolean ret;

    assert(c != NULL);

    va_start(ap, c);
    ret = krb5_config_vget_bool (NULL, c, ap);
    va_end(ap);
    return ret;
}

int
afs_config_get_int(const krb5_config_section *c, ...)
{
    va_list ap;
    int ret;

    assert(c != NULL);

    va_start(ap, c);
    ret = krb5_config_vget_int (NULL, c, ap);
    va_end(ap);
    return ret;
}

