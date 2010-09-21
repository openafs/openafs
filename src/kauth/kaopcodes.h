/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Revision 1.5  88/11/18  09:17:53
 * This is now obsolete, opcodes are defined in kauth.rg, no opcode names
 *   are used.
 *
 * Revision 1.4  88/09/20  11:37:36
 * Added IBM Copyright
 *
 * Revision 1.3  88/08/29  12:46:27
 * This checks in several new modules and many updates.  The AuthServer
 *   at this point works more or less as described in the NAFS document
 *   released in at the Aug 23-24 1988 workshop.
 * Tickets are as described in the kerberos.ticket file.
 * Intergrated w/ MIT's des implementation and the Andrew one-way password
 *   encryption.  Uses bcrypt for RSECURE connections.  Uses R not Rx.
 *
 * Revision 1.2  88/07/19  16:19:58
 * Added GetEntry and ListEntry; other internal changes.
 *  */

#define LOWEST_OPCODE	 501
#define KASETPASSWORD	 501
#define KAAUTHENTICATE	 502
#define KAGETTICKET	 503
#define KASETFIELDS	 504
#define KACREATEUSER	 505
#define KADELETEUSER	 506
#define KAGETENTRY	 507
#define KALISTENTRY	 508
#define KACHANGEPASSWORD 509
#define KAGETSTATS	 510
#define HIGHEST_OPCODE	 510

#define NUMBER_OPCODES (HIGHEST_OPCODE-LOWEST_OPCODE+1)

#ifdef OPCODE_NAMES
static char *opcode_names[NUMBER_OPCODES] = {
    "SetPassword",
    "Authenticate",
    "GetTicket",
    "SetFields",
    "CreateUser",
    "DeleteUser",
    "GetEntry",
    "ListEntry",
    "ChangePW",
    "GetStats"
};
#endif
