/*
 * Copyright (c) 2012 Your File System Inc. All rights reserved.
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

#include <roken.h>

#include <hcrypto/des.h>

#include <rx/rxkad.h>
#include <afs/cellconfig.h>

struct rx_securityClass *
afstest_FakeRxkadClass(struct afsconf_dir *dir,
		       char *name, char *instance, char *realm,
		       afs_uint32 startTime, afs_uint32 endTime)
{
    int code;
    char buffer[256];
    struct ktc_encryptionKey key, session;
    afs_int32 kvno;
    afs_int32 ticketLen;
    struct rx_securityClass *class = NULL;

    code = afsconf_GetLatestKey(dir, &kvno, &key);
    if (code)
	goto out;

    DES_init_random_number_generator((DES_cblock *) &key);
    code = DES_new_random_key((DES_cblock *) &session);
    if (code)
	goto out;

    ticketLen = sizeof(buffer);
    memset(buffer, 0, sizeof(buffer));
    startTime = time(NULL);
    endTime = startTime + 60 * 60;

    code = tkt_MakeTicket(buffer, &ticketLen, &key, name, instance, realm,
			  startTime, endTime, &session, 0, "afs", "");
    if (code)
	goto out;

    class = rxkad_NewClientSecurityObject(rxkad_clear, &session, kvno,
					  ticketLen, buffer);
out:
    return class;
}
