/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef AFS_SRC_RXKAD_H
#define AFS_SRC_RXKAD_H

#define MAXKTCTICKETLIFETIME (30*24*60*60)
#define MINKTCTICKETLEN (32)
#define MAXKTCTICKETLEN (1024)
#define MAXKTCNAMELEN (64)
#define MAXKTCREALMLEN (64)
#define KTC_TIME_UNCERTAINTY	(60*15)

#define MAXRANDOMNAMELEN 16             /* length of random generated
                                           usernames used by afslog for high
                                           security must be < MAXKTCNAMELEN */

#define LOGON_OPTION_INTEGRATED 1
#define LOGON_OPTION_HIGHSECURITY 2

/*
 * Define ticket types. For Kerberos V4 tickets, this is overloaded as
 * the server key version number, so class numbers 0 through 255 are reserved
 * for V4 tickets.  For other types, the kvno field passed to/from
 * ktc_[SG]etToken is used to carry the ticket type.
 */

/* For Kerberos V5, tickets have an in-the-clear portion containing the
 * server key version.
 */
#define RXKAD_TKT_TYPE_KERBEROS_V5 256

/*
 * rxkad krb5 "proposal 2B":
 * Make a special ticket that will fit inside the token buffers of old
 * versions that use a value of 344 for MAXKTCTICKETLEN.  This is done
 * by using only the encrypted part of a Kerberos V5 ticket.  This type
 * stomps insides the v4 kvno space.  This is bad but necessary in order
 * to allow these tokens to be used in old clients and to allow them
 * to be carried over the krb524 protocol.
 */
#define RXKAD_TKT_TYPE_KERBEROS_V5_ENCPART_ONLY 213

/*
 * The AFS/DFS translator may also make use of additional ticket types in
 * the range 257 through 511. DO NOT USE THESE FOR ANY OTHER PURPOSE.
 */
#define RXKAD_TKT_TYPE_ADAPT_RESERVED_MIN       257
#define RXKAD_TKT_TYPE_ADAPT_RESERVED_MAX       511


struct ktc_encryptionKey {
  char data[8];
};

struct ktc_principal {
  char name[MAXKTCNAMELEN];
  char instance[MAXKTCNAMELEN];
  char cell[MAXKTCREALMLEN];
#ifdef AFS_NT40_ENV
  char smbname[MAXRANDOMNAMELEN];
#endif
};

typedef char rxkad_level;
extern int rxkad_min_level;	/* enforce min level at client end */
#define rxkad_clear 0		/* checksum some selected header fields */
#define rxkad_auth 1		/* rxkad_clear + protected packet length */
#define rxkad_crypt 2		/* rxkad_crypt + encrypt packet payload */

extern int rxkad_EpochWasSet;

/* XXX rxkad stats */


/*
 * Misc stuff that shouldn't be exported
 */

#define round_up_to_ebs(v) (((v) + 7) & (~7))

#ifdef KERNEL
#include "../afs/rxkad_prototypes.h"
#else
#include "rxkad_prototypes.h"
#endif
#include "rxkad_errs.h"

#endif /* AFS_SRC_RXKAD_H */
