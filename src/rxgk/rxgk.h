/* -*- C -*- */

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

/* $Id$ */

#ifndef __RXGK_H
#define __RXGK_H

/* Is this really large enough for a krb5 ticket? */
#define MAXKRB5TICKETLEN	1024

typedef char rxgk_level;
#define rxgk_auth 0		/* rxgk_clear + protected packet length */
#define rxgk_crypt 1		/* rxgk_crypt + encrypt packet payload */

int32_t
rxgk_GetServerInfo(struct rx_connection *, rxgk_level *,
		   uint32_t *, char *, size_t, char *, size_t, int32_t *);

struct rx_securityClass *
rxgk_NewServerSecurityObject (/*rxgk_level*/ int min_level,
			      const char *princ,
			      void *appl_data,
			      int (*get_key)(void *data, const char *principal,
					     int enctype, int kvno, 
					     krb5_keyblock *key),
			      int (*user_ok)(const char *name,
					     const char *realm,
					     int kvno),
			      uint32_t serviceId);

struct rx_securityClass *
rxgk_k5_NewClientSecurityObject (/*rxgk_level*/ int level,
				 krb5_keyblock *sessionkey,
				 int32_t kvno,
				 int ticketLen,
				 void *ticket,
				 uint32_t serviceId,
				 krb5_context context);

int
rxgk_default_get_key(void *, const char *, int, int, krb5_keyblock *);

/* XXX these are not com_err error, MAKE THIS com_err's */
#define RXGKINCONSISTENCY	(19270400L)
#define RXGKPACKETSHORT		(19270401L)
#define RXGKLEVELFAIL		(19270402L)
#define RXGKTICKETLEN		(19270403L)
#define RXGKOUTOFSEQUENCE	(19270404L)
#define RXGKNOAUTH		(19270405L)
#define RXGKBADKEY		(19270406L)
#define RXGKBADTICKET		(19270407L)
#define RXGKUNKNOWNKEY		(19270408L)
#define RXGKEXPIRED		(19270409L)
#define RXGKSEALEDINCON		(19270410L)
#define RXGKDATALEN		(19270411L)
#define RXGKILLEGALLEVEL	(19270412L)

#endif /* __RXGK_H */
