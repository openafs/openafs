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


#include "internal.h"
#include <stdio.h>
#include "error_table.h"
#include "mit-sipb-cr.h"
#ifdef HAVE_LIBINTL
#include <libintl.h>
#endif
#ifdef AFS_DARWIN_ENV
#include <CoreFoundation/CoreFoundation.h>
#endif
#include <afs/errors.h>
#include <afs/afsutil.h>
#include <string.h>
#include "com_err.h"

static char buffer[64];

static struct et_list *_et_list = (struct et_list *)NULL;

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

static void
et_mutex_once(void)
{
    assert(!pthread_mutex_init
	   (&et_list_mutex, (const pthread_mutexattr_t *)0));
    et_list_done = 1;
}

#define LOCK_ET_LIST \
	do { \
	    if (!et_list_done) \
		pthread_once(&et_list_once, et_mutex_once); \
	    assert(pthread_mutex_lock(&et_list_mutex)==0); \
	} while (0)
#define UNLOCK_ET_LIST assert(pthread_mutex_unlock(&et_list_mutex)==0)
#else
#define LOCK_ET_LIST
#define UNLOCK_ET_LIST
#endif /* AFS_PTHREAD_ENV */


static char *vmsgs[] = {
    "volume needs to be salvaged",	/* 101, in Pittsburghese */
    "no such entry (vnode)",	/* 102 */
    "volume does not exist / did not salvage",	/* 103 */
    "volume already exists",	/* 104 */
    "volume out of service",	/* 105 */
    "volume offline (utility running)",	/* 106 */
    "volume already online",	/* 107 */
    "unknown volume error 108",	/* 108 */
    "unknown volume error 109",	/* 109 */
    "volume temporarily busy",	/* 110 */
    "volume moved",		/* 111 */
    (char *)0
};

static char *
negative_message(int code)
{
    if (code == -1)
	return "server or network not responding";
    else if (code == -2)
	return "invalid RPC (RX) operation";
    else if (code == -3)
	return "server not responding promptly";
    else if (code == -7)
	return "port address already in use";
    else if (code <= -450 && code > -500) {
	sprintf(buffer, "RPC interface mismatch (%d)", code);
	return buffer;
    } else {
	sprintf(buffer, "unknown RPC error (%d)", code);
	return buffer;
    }
}

static char *
volume_message(int code)
{
    if (code >= 101 && code <= 111)
	return vmsgs[code - 101];
    else
	return "unknown volume error";
}

#ifdef AFS_DARWIN_ENV
static_inline const char *
_intlize(const char *msg, int base, char *str, size_t len)
{
    char domain[12 +20];
    CFStringRef cfstring = CFStringCreateWithCString(kCFAllocatorSystemDefault,
						     msg,
						     kCFStringEncodingUTF8);
    CFStringRef cfdomain;
    CFBundleRef OpenAFSBundle = CFBundleGetBundleWithIdentifier(CFSTR("org.openafs.filesystems.afs"));

    if (!str) {
        CFRelease(cfstring);
	return msg;
    }

    snprintf(domain, sizeof(domain), "heim_com_err%d", base);
    cfdomain = CFStringCreateWithCString(kCFAllocatorSystemDefault, domain,
					 kCFStringEncodingUTF8);
    if (OpenAFSBundle != NULL) {
	CFStringRef cflocal;

	cflocal = CFBundleCopyLocalizedString(OpenAFSBundle, cfstring,
					      cfstring, cfdomain);
        CFStringGetCString(cflocal, str, len, kCFStringEncodingUTF8);
	CFRelease(cflocal);
    } else {
        CFStringGetCString(cfstring, str, len, kCFStringEncodingUTF8);
    }

    CFRelease(cfstring);
    CFRelease(cfdomain);
    return str;
}
#else
static_inline const char *
_intlize(const char *msg, int base, char *str, size_t len)
{
#if defined(HAVE_LIBINTL)
    char domain[12 +20];
#endif
    if (!str)
	return msg;
#if defined(HAVE_LIBINTL)
    snprintf(domain, sizeof(domain), "heim_com_err%d", base);
    strlcpy(str, dgettext(domain, msg), len);
#else
    strlcpy(str, msg, len);
#endif
    return str;
}
#endif

static const char *
afs_error_message_int(struct et_list *list, afs_int32 code, char *str, size_t len)
{
    int offset;
    struct et_list *et;
    int table_num, unlock = 0;
    int started = 0;
    char *cp;
    const char *err_msg;

    /* check for rpc errors first */
    if (code < 0)
	return _intlize(negative_message(code), -1, str, len);

    offset = code & ((1 << ERRCODE_RANGE) - 1);
    table_num = code - offset;
    if (!table_num) {
	if ((err_msg = strerror(offset)) != NULL)
	    return _intlize(err_msg, 0, str, len);
	else if (offset < 140)
	    return _intlize(volume_message(code), 0, str, len);
	else
	    goto oops;
    }
    if (list) {
	et = list;
    } else {
	LOCK_ET_LIST;
	unlock = 1;
	et = _et_list;
    }
    for (; et; et = et->next) {
	if (et->table->base == table_num) {
	    /* This is the right table */
	    if (et->table->n_msgs <= offset)
		goto oops;
	    err_msg = _intlize(et->table->msgs[offset], et->table->base,
			       str, len);
	    if (unlock)
		UNLOCK_ET_LIST;
	    return err_msg;
	}
    }
  oops:
    if (unlock)
	UNLOCK_ET_LIST;
    /* Unknown code can be included in the negative errors catalog */
    _intlize("Unknown code ", -1, buffer, sizeof buffer);
    if (table_num) {
	strlcat(buffer, afs_error_table_name(table_num), sizeof buffer);
	strlcat(buffer, " ", sizeof buffer);
    }
    for (cp = buffer; *cp; cp++);
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
    if (code > -10000)
	sprintf(cp, " (%d)", code);
    else
	*cp = '\0';
    return (buffer);
}

const char *
afs_error_message_localize(afs_int32 code, char *str, size_t len)
{
    return afs_error_message_int((struct et_list *)0, code, str, len);
}

const char *
afs_com_right_r(struct et_list *list, long code, char *str, size_t len)
{
    return afs_error_message_int(list, (afs_int32)code, str, len);
}

const char *
afs_com_right(struct et_list *list, long code)
{
    return afs_error_message_int(list, (afs_int32)code, (char *)0, 0);
}

const char *
afs_error_message(afs_int32 code)
{
    return afs_error_message_int((struct et_list *)0, code, (char *)0, 0);
}

void
afs_add_to_error_table(struct et_list *new_table)
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
