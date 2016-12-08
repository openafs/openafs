/* Functions for handling the Kerberos context. For now, these are no-ops */

#include <krb5_locl.h>

int
krb5_init_context(krb5_context *ctx) {
    return 0;
}

void
krb5_free_context(krb5_context ctx)
{
    return;
}

void
krb5_set_error_message(krb5_context ctx, krb5_error_code ret, const char *fmt,
		       ...)
{
    return;
}

krb5_error_code
krb5_abortx(krb5_context ctx, const char *fmt, ...)
{
    return 0;
}
