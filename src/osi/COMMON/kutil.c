/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006 Sine Nomine Associates
 */

/* this code was pulled from rx_kcommon.c */

#include <osi/osi_impl.h>


#if !defined(OSI_UTIL_OVERRIDE_PANIC)
void
osi_Panic(msg, a1, a2, a3)
     char *msg;
{
    if (!msg)
	msg = "Unknown AFS panic";

    osi_kernel_printf(msg, a1, a2, a3);
#ifdef OSI_LINUX20_ENV
    * ((char *) 0) = 0;
#else
    panic(msg);
#endif
}
#endif /* !OSI_UTIL_OVERRIDE_PANIC */


/*
 * osi_AssertFailK() -- used by the osi_Assert() macro.
 *
 * It essentially does
 *
 * osi_Panic("assertion failed: %s, file: %s, line: %d", expr, file, line);
 *
 * Since the kernel version of osi_Panic() only passes its first
 * argument to the native panic(), we construct a single string and hand
 * that to osi_Panic().
 */
#if !defined(OSI_UTIL_OVERRIDE_ASSERT_FAIL)
void
osi_util_AssertFail(const char *expr, const char *file, int line)
{
    static const char msg0[] = "assertion failed: ";
    static const char msg1[] = ", file: ";
    static const char msg2[] = ", line: ";
    static const char msg3[] = "\n";

    /*
     * These buffers add up to 1K, which is a pleasantly nice round
     * value, but probably not vital.
     */
    char buf[1008];
    char linebuf[16];

    /* check line number conversion */

    if (osi_utoa(linebuf, sizeof linebuf, line) < 0) {
	osi_Panic("osi_AssertFailK: error in osi_utoa()\n");
    }

    /* okay, panic */

#define ADDBUF(BUF, STR)					\
	if (strlen(BUF) + strlen((char *)(STR)) + 1 <= sizeof BUF) {	\
		strcat(BUF, (char *)(STR));				\
	}

    buf[0] = '\0';
    ADDBUF(buf, msg0);
    ADDBUF(buf, expr);
    ADDBUF(buf, msg1);
    ADDBUF(buf, file);
    ADDBUF(buf, msg2);
    ADDBUF(buf, linebuf);
    ADDBUF(buf, msg3);

#undef ADDBUF

    osi_Panic(buf);
}
#endif /* !OSI_UTIL_OVERRIDE_ASSERT_FAIL */


/*
 * osi_utoa() - write the NUL-terminated ASCII decimal form of the given
 * unsigned long value into the given buffer.  Returns 0 on success,
 * and a value less than 0 on failure.  The contents of the buffer is
 * defined only on success.
 */

int
osi_utoa(char *buf, size_t len, unsigned long val)
{
    long k;			/* index of first byte of string value */

    /* we definitely need room for at least one digit and NUL */

    if (len < 2) {
	return -1;
    }

    /* compute the string form from the high end of the buffer */

    buf[len - 1] = '\0';
    for (k = len - 2; k >= 0; k--) {
	buf[k] = val % 10 + '0';
	val /= 10;

	if (val == 0)
	    break;
    }

    /* did we finish converting val to string form? */

    if (val != 0) {
	return -2;
    }

    /* this should never happen */

    if (k < 0) {
	return -3;
    }

    /* this should never happen */

    if (k >= len) {
	return -4;
    }

    /* if necessary, relocate string to beginning of buf[] */

    if (k > 0) {

	/*
	 * We need to achieve the effect of calling
	 *
	 * memmove(buf, &buf[k], len - k);
	 *
	 * However, since memmove() is not available in all
	 * kernels, we explicitly do an appropriate copy.
	 */

	char *dst = buf;
	char *src = buf + k;

	while ((*dst++ = *src++) != '\0')
	    continue;
    }

    return 0;
}
