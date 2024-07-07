/*
 * Copyright (c) 2024 Sahil Siddiq <sahilcdq@proton.me>.
 * All rights reserved.
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
#include <stdio.h>
#include <err.h>

#include <rx/rx.h>
#include <afs/afsutil.h>

static afs_uint32
str2addr(char *str)
{
    afs_int32 addr = 0;
    struct hostent *th = hostutil_GetHostByName(str);
    if (th == NULL) {
	errx(1, "Could not resolve host '%s'", str);
    }
    memcpy(&addr, th->h_addr, sizeof(addr));
    return addr;
}

static afs_uint32
str2int(const char *str)
{
    return strtoul(str, NULL, 10);
}

static int
run_call(struct rx_connection *conn, char *message)
{
    struct rx_call *call;
    afs_int32 nbytes = strlen(message);
    afs_int32 nbytes_n;
    char *recvbuf;
    afs_int32 code = RX_PROTOCOL_ERROR;

    recvbuf = calloc(nbytes + 1, 1);
    opr_Assert(recvbuf != NULL);

    call = rx_NewCall(conn);
    opr_Assert(call != NULL);

    nbytes_n = htonl(nbytes);
    if (rx_Write32(call, &nbytes_n) != sizeof(nbytes_n)) {
	warnx("rx_Write32 failed");
	goto done;
    }

    if (rx_Write(call, message, nbytes) != nbytes) {
	warnx("rx_Write failed");
	goto done;
    }

    if (rx_Read(call, recvbuf, nbytes) != nbytes) {
	warnx("rx_Read failed");
	goto done;
    }

    printf("%s\n", recvbuf);

    code = 0;

 done:
    code = rx_EndCall(call, code);
    if (code != 0) {
	warnx("call aborted with code %d", code);
    }
    free(recvbuf);
    return code;
}

int
main(int argc, char **argv)
{
    afs_uint32 host;
    afs_uint16 port;
    afs_uint16 service_id;
    char *message;
    int code;
    struct rx_connection *conn;

    setprogname(argv[0]);

    if (argc != 5) {
	errx(1, "Usage: %s <host> <port> <service_id> <message>", getprogname());
    }

    host = str2addr(argv[1]);
    port = str2int(argv[2]);
    service_id = str2int(argv[3]);
    message = argv[4];

    code = rx_Init(0);
    if (code != 0) {
	errx(1, "rx_Init failed with %d", code);
    }

    conn = rx_NewConnection(host, htons(port), service_id,
			    rxnull_NewClientSecurityObject(), 0);
    opr_Assert(conn != NULL);

    code = run_call(conn, message);

    rx_DestroyConnection(conn);

    return code;
}
