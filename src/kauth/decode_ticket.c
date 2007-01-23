/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <des.h>
#include <afs/com_err.h>
#include <afs/auth.h>
#include <rx/rxkad.h>
#include "kautils.h"

static char *whoami;

int
main(int argc, char *argv[])
{
    struct ktc_principal client;
    struct ktc_encryptionKey sessionkey;
    Date start, end;
    afs_int32 host;
    char key[8];
    char ticket[MAXKTCTICKETLEN];
    afs_int32 ticketLen;
    afs_int32 code;
    char bob[KA_TIMESTR_LEN];

    whoami = argv[0];
    initialize_RXK_error_table();
    initialize_KA_error_table();
    initialize_rx_error_table();

    if (argc != 3) {
	printf("Usage is %s key ticket\n", whoami);
	exit(1);
    }
    if (ka_ReadBytes(argv[1], key, sizeof(key)) != 8)
	printf("Key must be 8 bytes long\n");
    if (!des_check_key_parity(key) || des_is_weak_key(key)) {
	com_err(whoami, KABADKEY, "server's key for decoding ticket is bad");
	exit(1);
    }
    ticketLen = ka_ReadBytes(argv[2], ticket, sizeof(ticket));
    printf("Ticket length is %d\n", ticketLen);

    code =
	tkt_DecodeTicket(ticket, ticketLen, key, client.name, client.instance,
			 client.cell, &sessionkey, &host, &start, &end);
    if (code) {
	com_err(whoami, code, "decoding ticket");
	if (code = tkt_CheckTimes(start, end, time(0)) <= 0)
	    com_err(whoami, 0, "because of start or end times");
	exit(1);
    }

    if (!des_check_key_parity(&sessionkey) || des_is_weak_key(&sessionkey)) {
	com_err(whoami, KABADKEY, "checking ticket's session key");
	exit(1);
    }

    ka_PrintUserID("Client is ", client.name, client.instance, 0);
    if (strlen(client.cell))
	printf("@%s", client.cell);
    printf("\nSession key is ");
    ka_PrintBytes(&sessionkey, 8);
    ka_timestr(start, bob, KA_TIMESTR_LEN);
    printf("\nGood from %s", bob);
    ka_timestr(end, bob, KA_TIMESTR_LEN);
    printf(" till %s\n", bob);
}
