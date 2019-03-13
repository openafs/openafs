/* rxgk/rxgk_server.c - server-specific security object routines */
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
 * Server-specific security object routines.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <afs/opr.h>
#include <rx/rx.h>
#include <rx/rx_packet.h>
#include <rx/rxgk.h>

#include "rxgk_private.h"

/* Pre-declare the securityclass routines for the securityOps definition. */
struct rx_securityClass *rxgk_NewServerSecurityObject(void *getkey_rock,
						      rxgk_getkey_func getkey);
static int rxgk_ServerClose(struct rx_securityClass *aobj);
static int rxgk_NewServerConnection(struct rx_securityClass *aobj,
				    struct rx_connection *aconn);
static int rxgk_ServerPreparePacket(struct rx_securityClass *aobj,
				    struct rx_call *acall,
				    struct rx_packet *apacket);
static int rxgk_CheckAuthentication(struct rx_securityClass *aobj,
				    struct rx_connection *aconn);
static int rxgk_CreateChallenge(struct rx_securityClass *aobj,
				struct rx_connection *aconn);
static int rxgk_GetChallenge(struct rx_securityClass *aobj,
			     struct rx_connection *aconn,
			     struct rx_packet *apacket);
static int rxgk_CheckResponse(struct rx_securityClass *aobj,
			      struct rx_connection *aconn,
			      struct rx_packet *apacket);
static int rxgk_ServerCheckPacket(struct rx_securityClass *aobj,
				  struct rx_call *acall, struct rx_packet *apacket);
static void rxgk_DestroyServerConnection(struct rx_securityClass *aobj,
					 struct rx_connection *aconn);
static int rxgk_ServerGetStats(struct rx_securityClass *aobj,
			       struct rx_connection *aconn,
			       struct rx_securityObjectStats *astats);


static struct rx_securityOps rxgk_server_ops = {
    rxgk_ServerClose,
    rxgk_NewServerConnection,
    rxgk_ServerPreparePacket,		/* once per packet creation */
    0,					/* send packet (once per retrans) */
    rxgk_CheckAuthentication,
    rxgk_CreateChallenge,
    rxgk_GetChallenge,
    0,
    rxgk_CheckResponse,
    rxgk_ServerCheckPacket,		/* check data packet */
    rxgk_DestroyServerConnection,
    rxgk_ServerGetStats,
    0,
    0,				/* spare 1 */
    0,				/* spare 2 */
};

static struct rx_securityClass dummySC = {
    &rxgk_server_ops,
    NULL,
    0
};

struct rx_securityClass *
rxgk_NewServerSecurityObject(void *getkey_rock, rxgk_getkey_func getkey)
{
    return &dummySC;
}

static int
rxgk_ServerClose(struct rx_securityClass *aobj)
{
    return RXGK_INCONSISTENCY;
}

static int
rxgk_NewServerConnection(struct rx_securityClass *aobj, struct rx_connection *aconn)
{
    return RXGK_INCONSISTENCY;
}

static int
rxgk_ServerPreparePacket(struct rx_securityClass *aobj, struct rx_call *acall,
			 struct rx_packet *apacket)
{
    return RXGK_INCONSISTENCY;
}

static int
rxgk_CheckAuthentication(struct rx_securityClass *aobj,
			 struct rx_connection *aconn)
{
    return RXGK_INCONSISTENCY;
}

static int
rxgk_CreateChallenge(struct rx_securityClass *aobj,
		     struct rx_connection *aconn)
{
    return RXGK_INCONSISTENCY;
}

static int
rxgk_GetChallenge(struct rx_securityClass *aobj, struct rx_connection *aconn,
		  struct rx_packet *apacket)
{
    return RXGK_INCONSISTENCY;
}

static int
rxgk_CheckResponse(struct rx_securityClass *aobj,
		   struct rx_connection *aconn, struct rx_packet *apacket)
{
    return RXGK_INCONSISTENCY;
}

static int
rxgk_ServerCheckPacket(struct rx_securityClass *aobj, struct rx_call *acall,
		       struct rx_packet *apacket)
{
    return RXGK_INCONSISTENCY;
}

static void
rxgk_DestroyServerConnection(struct rx_securityClass *aobj,
			     struct rx_connection *aconn)
{
}

static int
rxgk_ServerGetStats(struct rx_securityClass *aobj, struct rx_connection *aconn,
		    struct rx_securityObjectStats *astats)
{
    return RXGK_INCONSISTENCY;
}
