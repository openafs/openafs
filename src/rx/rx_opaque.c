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
# include <sys/types.h>
# include <string.h>
# include <errno.h>
#else
# include "afs/sysincludes.h"
# include "afsincludes.h"
#endif

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
    struct rx_opaque *opaque = rxi_Alloc(sizeof(struct rx_opaque));
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
rx_opaque_populate(struct rx_opaque *to, void *data, size_t datalen)
{
    int code;
    to->len = 0;
    to->val = NULL;

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
rx_opaque_freeContents(struct rx_opaque *buf) {
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
rx_opaque_zeroFreeContents(struct rx_opaque *buf) {
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
rx_opaque_free(struct rx_opaque **buf) {
    rx_opaque_freeContents(*buf);
    rxi_Free(*buf, sizeof(struct rx_opaque));
    *buf = NULL;
}

/*!
 * Zero, then free an opaque object
 *
 * This zeros the contents of an opaque object, frees those contents,
 * then frees the object itself.
 */

void
rx_opaque_zeroFree(struct rx_opaque **buf) {
    rx_opaque_zeroFreeContents(*buf);
    rxi_Free(*buf, sizeof(struct rx_opaque));
    *buf = NULL;
}
