
/* This header file transforms the Heimdal config_parse.c profile
 * parser into an AFS profile parser, hiding the krb5-ness of the parser
 * behind the scenes.
 */

#include <afsconfig.h>
#include <afs/stds.h>

#include <roken.h>

#include <assert.h>
#include <ctype.h>

#include "cmd.h"

#define PACKAGE "openafs"

#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif

#define KRB5_BUFSIZ 1024

#define KRB5_LIB_FUNCTION static AFS_UNUSED
#define KRB5_LIB_CALL

#define KRB5_DEPRECATED_FUNCTION(x)

#define N_(X,Y) X

typedef struct cmd_config_binding krb5_config_binding;
typedef struct cmd_config_binding krb5_config_section;

#define krb5_config_list cmd_config_list
#define krb5_config_string cmd_config_string
#define _krb5_config_get_entry _cmd_config_get_entry

struct krb5_context_data {
    krb5_config_section *cf;
};

typedef struct krb5_context_data * krb5_context;
typedef int krb5_error_code;
typedef int krb5_boolean;
typedef time_t krb5_deltat;

static void krb5_set_error_message(krb5_context context, krb5_error_code ret,
				   const char *fmt, ...) {
    return;
}

#ifdef EXPAND_PATH_HEADER

#include <shlobj.h>

static krb5_error_code krb5_enomem(krb5_context context) {
    return ENOMEM;
}

static int _krb5_expand_path_tokens(krb5_context, const char *, char**);

int
_cmd_ExpandPathTokens(krb5_context context, const char *in, char **out) {
    return _krb5_expand_path_tokens(context, in, out);
}

HINSTANCE _krb5_hInstance = NULL;

/* This bodge avoids the need for everything linking against cmd to also
 * link against the shell library on Windows */
#undef SHGetFolderPath
#define SHGetFolderPath internal_getpath

HRESULT internal_getpath(void *a, int b, void *c, DWORD d, LPTSTR out) {
    return E_INVALIDARG;
}

#else

#define KRB5_CONFIG_BADFORMAT                    CMD_BADFORMAT

#define _krb5_expand_path_tokens _cmd_ExpandPathTokens

extern int _cmd_ExpandPathTokens(krb5_context, const char *, char **);

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
static const char *krb5_config_vget_string_default(krb5_context,
					   const krb5_config_section *,
					   const char *,
					   va_list);
static const krb5_config_binding * krb5_config_vget_list
	(krb5_context, const krb5_config_section *, va_list);
static krb5_error_code krb5_config_parse_file_multi
	(krb5_context, const char *, krb5_config_section **);
static krb5_error_code krb5_config_parse_file
	(krb5_context, const char *, krb5_config_section **);
static krb5_error_code krb5_config_file_free
	(krb5_context, krb5_config_section *);
static krb5_boolean krb5_config_vget_bool_default
	(krb5_context, const krb5_config_section *, int, va_list);
static int krb5_config_vget_int_default
	(krb5_context, const krb5_config_section *, int, va_list);

static krb5_error_code
krb5_string_to_deltat(const char *str, krb5_deltat *t) {
    return 1;
}

KRB5_LIB_FUNCTION void krb5_clear_error_message(krb5_context context) {
    return;
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
cmd_RawConfigParseFileMulti(const char *fname, cmd_config_section **res) {
    return krb5_config_parse_file_multi(NULL, fname, res);
}

int
cmd_RawConfigParseFile(const char *fname, cmd_config_section **res) {
    return krb5_config_parse_file(NULL, fname, res);
}

int
cmd_RawConfigFileFree(cmd_config_section *s) {
    return krb5_config_file_free(NULL, s);
}

const char*
cmd_RawConfigGetString(const cmd_config_section *c,
		       const char *defval, ...)
{
    const char *ret;
    va_list args;

    assert(c != NULL);

    va_start(args, defval);
    ret = krb5_config_vget_string_default (NULL, c, defval, args);
    va_end(args);
    return ret;
}

int
cmd_RawConfigGetBool(const cmd_config_section *c, int defval, ...)
{
    va_list ap;
    krb5_boolean ret;

    assert(c != NULL);

    va_start(ap, defval);
    ret = krb5_config_vget_bool_default (NULL, c, defval, ap);
    va_end(ap);
    return ret;
}

int
cmd_RawConfigGetInt(const cmd_config_section *c, int defval, ...)
{
    va_list ap;
    int ret;

    assert(c != NULL);

    va_start(ap, defval);
    ret = krb5_config_vget_int_default (NULL, c, defval, ap);
    va_end(ap);
    return ret;
}

const cmd_config_binding *
cmd_RawConfigGetList(const cmd_config_section *c, ...)
{
    va_list ap;
    const cmd_config_binding *ret;

    assert(c != NULL);

    va_start(ap, c);
    ret = krb5_config_vget_list (NULL, c, ap);
    va_end(ap);

    return ret;
}
#endif
