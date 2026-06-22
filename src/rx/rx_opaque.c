/*
 * Copyright (c) 2010 Your File System Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>

#ifndef KERNEL
# include <roken.h>
#else
# include "afs/sysincludes.h"
# include "afsincludes.h"
#endif
#include <afs/opr.h>

#include <rx/rx.h>
#include <rx/rx_opaque.h>


/*!
 * Generate a new opaque object
 *
 * Allocate a new opaque object, and copy datalen bytes from data into it.
 * The caller should dispose of the resulting object using rx_opaque_free or
 * rx_opaque_zeroFree.
 *
 * @param data
 * 	A pointer to the data to copy into the object
 * @param datalen
 * 	The number of bytes of data to copy
 * @returns
 * 	A pointer to the allocated opaque object.
 */

struct rx_opaque *
rx_opaque_new(void *data, size_t datalen)
{
    struct rx_opaque *opaque = rxi_Alloc(sizeof(*opaque));
    if (opaque != NULL)
	rx_opaque_populate(opaque, data, datalen);
    return opaque;
}

/*!
 * Allocate space within an existing opaque object
 *
 * Allocate length bytes of data within an existing opaque object. This will
 * overwrite (without freeing) any data that is already held within the
 * object.
 *
 * @param buf
 * 	The opaque object
 * @param length
 * 	The number of bytes to allocate
 * @returns
 * 	0 on success, ENOMEM if the memory cannot be allocated.
 */

int
rx_opaque_alloc(struct rx_opaque *buf, size_t length)
{
    void *mem = rxi_Alloc(length);
    if (mem == NULL)
        return ENOMEM;
    buf->val = mem;
    buf->len = length;
    memset(buf->val, 0, buf->len);

    return 0;
}

/*!
 * Populate an existing opaque object
 *
 * Copy datalen bytes from data into an existing opaque object. This allocates
 * new data space within the object, and will replace (without freeing) any
 * data that is already held within the object.
 *
 * @param to
 * 	The opaque object to populate
 * @param length
 * 	The number of bytes to allocate
 * @returns
 * 	0 on sucess, ENOMEM if memory cannot be allocated
 */

int
rx_opaque_populate(struct rx_opaque *to, const void *data, size_t datalen)
{
    int code;
    to->len = 0;
    to->val = NULL;

    if (data == NULL || datalen == 0)
	return 0;

    code = rx_opaque_alloc(to, datalen);
    if (code)
        return code;
    memcpy(to->val, data, datalen);
    return 0;
}

/*!
 * Copy data from one opaque object to another
 *
 * Make a copy of the data held within one existing opaque object into
 * another. This allocates new data space within the destination object,
 * and will replace (without freeing) any data that is already held within
 * this object.
 *
 * @param to
 * 	To object to copy to
 * @param from
 * 	The object to copy data from
 * @returns
 * 	0 on success, ENOMEM if memory cannot be allocated
 */

int
rx_opaque_copy(struct rx_opaque *to, const struct rx_opaque *from)
{
    return rx_opaque_populate(to, from->val, from->len);
}

/*!
 * Free the contents of an opaque object
 *
 */
void
rx_opaque_freeContents(struct rx_opaque *buf)
{
    if (buf->val) {
	rxi_Free(buf->val, buf->len);
    }
    buf->len = 0;
    buf->val = NULL;
}

/*!
 * Zero, then free, the contents of an opaque object
 */
void
rx_opaque_zeroFreeContents(struct rx_opaque *buf)
{
    if (buf->val)
	memset(buf->val, 0, buf->len);
    rx_opaque_freeContents(buf);
}

/*!
 * Free an opaque object
 *
 * This frees the contents of the object, then frees the object itself
 */

void
rx_opaque_free(struct rx_opaque **a_buf)
{
    struct rx_opaque *buf = *a_buf;
    *a_buf = NULL;
    if (buf == NULL) {
	return;
    }

    rx_opaque_freeContents(buf);
    rxi_Free(buf, sizeof(*buf));
}

/*!
 * Zero, then free an opaque object
 *
 * This zeros the contents of an opaque object, frees those contents,
 * then frees the object itself.
 */

void
rx_opaque_zeroFree(struct rx_opaque **a_buf)
{
    struct rx_opaque *buf = *a_buf;
    *a_buf = NULL;
    if (buf == NULL) {
	return;
    }

    rx_opaque_zeroFreeContents(buf);
    rxi_Free(buf, sizeof(*buf));
}

/*!
 * Compare two opaque objects.
 *
 * Like memcmp(), but compares two opaque objects. If the two objects are of
 * different length, we compare the memory in the first
 * min(buf_a->len, buf_b->len) bytes. If the memory is the same, then the
 * greater opaque object is the one with the greater length.
 *
 * Note that 'val' must be non-NULL for each opaque if the opaque's 'len' is
 * non-zero; there is no special handling for NULL. If 'val' is NULL for a
 * non-zero 'len', this function will assert. But NULL is okay for a 0-length
 * opaque.
 *
 * @retval 0	The given opaque objects are equal.
 * @retval -1	buf_a is less than buf_b.
 * @retval 1	buf_a is greater than buf_b.
 */
int
rx_opaque_cmp(const struct rx_opaque *buf_a, const struct rx_opaque *buf_b)
{
    size_t len = opr_min(buf_a->len, buf_b->len);

    if (len > 0) {
	int diff;

	osi_Assert(buf_a->val != NULL);
	osi_Assert(buf_b->val != NULL);

	diff = memcmp(buf_a->val, buf_b->val, len);
	if (diff < 0) {
	    return -1;
	}
	if (diff > 0) {
	    return 1;
	}
    }

    if (buf_a->len < buf_b->len) {
	return -1;
    }
    if (buf_a->len > buf_b->len) {
	return 1;
    }
    return 0;
}

/*!
 * Format an opaque object into a human-readable string.
 *
 * For example, given an opaque object like this:
 *
 * struct rx_opaque buf = {
 *     .len = 4,
 *     .val = "\xde\xad\xbe\xef",
 * }
 *
 * You can print that opaque like this:
 *
 * struct rx_opaque_stringbuf strbuf;
 * printf("%s\n", rx_opaque_stringify(&buf, &strbuf));
 *
 * That prints the string "4:deadbeef". The buffer has an arbitrary limit; if
 * the opaque data is too big to be formatted into the buffer, the hex data
 * will be silently truncated, but the truncation is noticeable since the
 * length is printed separately.
 *
 * Do NOT use this function to fully identify an opaque (for example, to
 * compare two opaques); the stringification is a convenience for developers,
 * for use in debug messages and unit tests and such.
 *
 * @param buf
 *	The opaque object
 * @param strbuf
 *	Storage space for the formatted buffer
 * @returns
 *	A char* pointer to the given strbuf
 */
char *
rx_opaque_stringify(const struct rx_opaque *buf, struct rx_opaque_stringbuf *strbuf)
{
    char *str = &strbuf->sbuf[0];
    size_t remain = sizeof(*strbuf) - 1;
    int byte_i;
    int nbytes;

    memset(strbuf, 0, sizeof(*strbuf));

    nbytes = snprintf(str, remain, "%ld:", (long)buf->len);
    if (nbytes >= remain) {
	goto done;
    }

    str += nbytes;
    remain -= nbytes;

    if (buf->val == NULL) {
	snprintf(str, remain, "(null)");
	goto done;
    }

    for (byte_i = 0; byte_i < buf->len; byte_i++) {
	unsigned char byte = ((unsigned char*)buf->val)[byte_i];

	nbytes = snprintf(str, remain, "%02x", byte);
	if (nbytes >= remain) {
	    goto done;
	}

	str += nbytes;
	remain -= nbytes;
    }

 done:
    return &strbuf->sbuf[0];
}
