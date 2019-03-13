/* rxgk/rxgk_client.c - Client-only security object routines */
/*
 * Copyright (C) 2013, 2014 by the Massachusetts Institute of Technology.
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

/*
 * Client-only security object routines.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

/* OS-specific system headers go here */

#include <rx/rx.h>
#include <rx/rx_packet.h>
#include <rx/rxgk.h>

#include "rxgk_private.h"

/* Pre-declare the securityclass routines for the securityOps definition. */
static int rxgk_ClientClose(struct rx_securityClass *aobj);
static int rxgk_NewClientConnection(struct rx_securityClass *aobj,
				    struct rx_connection *aconn);
static int rxgk_ClientPreparePacket(struct rx_securityClass *aobj,
				    struct rx_call *acall,
				    struct rx_packet *apacket);
static int rxgk_GetResponse(struct rx_securityClass *aobj,
			    struct rx_connection *aconn,
			    struct rx_packet *apacket);
static int rxgk_ClientCheckPacket(struct rx_securityClass *aobj,
				  struct rx_call *acall,
				  struct rx_packet *apacket);
static void rxgk_DestroyClientConnection(struct rx_securityClass *aobj,
					 struct rx_connection *aconn);
static int rxgk_ClientGetStats(struct rx_securityClass *aobj,
			       struct rx_connection *aconn,
			       struct rx_securityObjectStats *astats);

static struct rx_securityOps rxgk_client_ops = {
    rxgk_ClientClose,
    rxgk_NewClientConnection,		/* every new connection */
    rxgk_ClientPreparePacket,		/* once per packet creation */
    0,					/* send packet (once per retrans) */
    0,
    0,
    0,
    rxgk_GetResponse,			/* respond to challenge packet */
    0,
    rxgk_ClientCheckPacket,		/* check data packet */
    rxgk_DestroyClientConnection,
    rxgk_ClientGetStats,
    0,
    0,
    0,
};

static struct rx_securityClass dummySC = {
    &rxgk_client_ops,
    NULL,
    0
};

struct rx_securityClass *
rxgk_NewClientSecurityObject(RXGK_Level level, afs_int32 enctype, rxgk_key k0,
			     RXGK_Data *token, afsUUID *uuid)
{
    return &dummySC;
}

static int
rxgk_ClientClose(struct rx_securityClass *aobj)
{
    return RXGK_INCONSISTENCY;
}

static int
rxgk_NewClientConnection(struct rx_securityClass *aobj,
			 struct rx_connection *aconn)
{
    return RXGK_INCONSISTENCY;
}

static int
rxgk_ClientPreparePacket(struct rx_securityClass *aobj, struct rx_call *acall,
			 struct rx_packet *apacket)
{
    return RXGK_INCONSISTENCY;
}

static int
rxgk_GetResponse(struct rx_securityClass *aobj, struct rx_connection *aconn,
		 struct rx_packet *apacket)
{
    return RXGK_INCONSISTENCY;
}

static int
rxgk_ClientCheckPacket(struct rx_securityClass *aobj, struct rx_call *acall,
		       struct rx_packet *apacket)
{
    return RXGK_INCONSISTENCY;
}

static void
rxgk_DestroyClientConnection(struct rx_securityClass *aobj,
			     struct rx_connection *aconn)
{
}

static int
rxgk_ClientGetStats(struct rx_securityClass *aobj, struct rx_connection *aconn,
		    struct rx_securityObjectStats *astats)
{
    return RXGK_INCONSISTENCY;
}
