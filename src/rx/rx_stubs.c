/* rx/rx_stubs.c - Stub functions for things not really implemented. */
/*
 * Copyright (C) 2017 by Benjamin Kaduk.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * Stub functions that don't have anyplace better to live.
 * For example, libafsrpc is going to export an rxgk_GetServerInfo
 * routine whether or not rxgk support is enabled at configure time.
 * But src/rxgk is not compiled at all in that case, so we need a home
 * for the stub function that will be exported in that case.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#ifndef ENABLE_RXGK

#include <rx/rx.h>
#include <rx/rxgk.h>
#include <rx/rx_identity.h>

#include <errno.h>

afs_int32
rxgk_GetServerInfo(struct rx_connection *conn, RXGK_Level *level,
		   struct afs_time64 *expiry, struct rx_identity **identity)
{
    return EINVAL;
}
#endif	/* !ENABLE_RXGK */
