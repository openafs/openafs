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
