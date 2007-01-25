/*
 * Copyright (c) 2007 Kungliga Tekniska Högskolan
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


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <netdb.h>

#include "rxgk_locl.h"

int
main(int argc, char **argv)
{
    struct rxgk_ticket input;
    struct rxgk_ticket output;
    char in_key[10];
    char *in_principal = "test@TEST";
    RXGK_Ticket_Crypt opaque;
    int ret;
    
    input.ticketversion = 0;
    input.enctype = 1;
    in_key[0] = 0x12;
    in_key[1] = 0x23;
    in_key[2] = 0x34;
    in_key[3] = 0x45;
    in_key[4] = 0x56;
    in_key[5] = 0x67;
    in_key[6] = 0x78;
    in_key[7] = 0x89;
    in_key[8] = 0x9a;
    in_key[9] = 0xab;
    input.key.val = in_key;
    input.key.len = 10;
    input.level = 34;
    input.starttime = 0x471147114711LL;
    input.lifetime = 0x171717;
    input.bytelife = 30;
    input.expirationtime = 0x471147124711LL;
    input.ticketprincipal.val = in_principal;
    input.ticketprincipal.len = strlen(in_principal);
    input.ext.val = NULL;
    input.ext.len = 0;

    ret = rxgk_encrypt_ticket(&input, &opaque);
    if (ret) {
	err(1, "rxgk_encrypt_ticket");
    }

    ret = rxgk_decrypt_ticket(&opaque, &output);
    if (ret) {
	err(1, "rxgk_decrypt_ticket");
    }

    printf("%s %x %x\n", output.ticketprincipal.val,
	   output.key.val[0],
	   output.key.val[1]);

    return 0;
}
