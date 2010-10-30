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
#else
# include "afs/sysincludes.h"
# include "afsincludes.h"
#endif

#include <rx/rx.h>
#include <rx/rx_identity.h>

/*!
 * Check to see if two rx identities match
 *
 * @param a
 * 	First identity
 * @param b
 * 	Second identity
 * @returns
 * 	True if a and b are identical, otherwise false
 */
int
rx_identity_match(struct rx_identity *a, struct rx_identity *b)
{
    return (a->kind == b->kind && a->exportedName.len == b->exportedName.len
	    && (memcmp(a->exportedName.val, b->exportedName.val,
		       a->exportedName.len) == 0));
}

/*!
 * Populate an identity
 *
 * Populate an existing identity with copies of the data passed to the
 * function. This will replace (without freeing) any existing identity
 * contents.
 *
 * @param identity
 * 	The identity to populate
 * @param kind
 * 	The type of data contained within this identity
 * @param displayName
 * 	The displayName of this identity
 * @param enameData
 * 	The opaque data that represents this identity
 * @param enameLength
 * 	The length of enameData
 */

void
rx_identity_populate(struct rx_identity *identity, rx_identity_kind kind,
		     char *displayName, void *enameData, size_t enameLength)
{
    memset(identity, 0, sizeof(struct rx_identity));

    identity->displayName = rxi_Alloc(strlen(displayName)+1);
    memcpy(identity->displayName, displayName, strlen(displayName)+1);

    rx_opaque_populate(&identity->exportedName, enameData, enameLength);
}


/*!
 * Copy an identity
 *
 * Copy the contents of one identity into another one. This will replace
 * (without freeing) any existing identity contents
 *
 * @param to
 * 	The identity to copy to
 * @param from
 * 	The identity to copy from
 */

void
rx_identity_copy(struct rx_identity *to, struct rx_identity *from)
{
    rx_identity_populate(to, from->kind, from->displayName,
		         from->exportedName.val, from->exportedName.len);
    return;
}

/*!
 * Build a new identity
 *
 * Create a new identity, with copies of the data passed to this function.
 *
 * @param kind
 * 	The type of data contained within this identity
 * @param displayName
 * 	The displayName of this identity
 * @param enameData
 * 	The opaque data that represents this identity
 * @param enameLength
 * 	The length of enameData
 * @returns
 * 	The new identity
 */

struct rx_identity *
rx_identity_new(rx_identity_kind kind, char *displayName, void *enameData,
		size_t enameLength)
{
    struct rx_identity *identity;

    identity = rxi_Alloc(sizeof(struct rx_identity));
    rx_identity_populate(identity, kind, displayName, enameData, enameLength);

    return identity;
}

/*!
 * Free the contents of an identity
 *
 * @param identity
 * 	The identity to free the contents of
 */

void
rx_identity_freeContents(struct rx_identity *identity)
{
    rxi_Free(identity->displayName, strlen(identity->displayName));
    identity->displayName = NULL;
    rx_opaque_freeContents(&identity->exportedName);
}

/*!
 * Free an identity
 *
 * @param identity
 * 	The identity to free (passed by reference)
 */

void
rx_identity_free(struct rx_identity **identity)
{
    rx_identity_freeContents(*identity);
    rxi_Free(*identity, sizeof(struct rx_identity));
    *identity = NULL;
}
