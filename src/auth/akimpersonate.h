#ifndef __AKIMPERSONATE_H__
#define __AKIMPERSONATE_H__

#if defined(HAVE_KRB5_CREDS_KEYBLOCK)
#define get_cred_keydata(c)	((c)->keyblock.contents)
#define get_cred_keylen(c)	((c)->keyblock.length)
#define get_creds_enctype(c)	((c)->keyblock.enctype)
#elif defined(HAVE_KRB5_CREDS_SESSION)
#define get_cred_keydata(c)	((c)->session.keyvalue.data)
#define get_cred_keylen(c)	((c)->session.keyvalue.length)
#define get_creds_enctype(c)	((c)->session.keytype)
#else
#error "Must have either keyblock or session member of krb5_creds"
#endif

/* The caller must include krb5.h to get prototypes for the types used. */
krb5_error_code
get_credv5_akimpersonate(krb5_context, char*, krb5_principal, krb5_principal,
			 time_t, time_t, const int *, krb5_creds**);

#endif
