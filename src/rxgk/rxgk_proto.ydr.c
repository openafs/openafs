/* Generated from rxgk_proto.xg */
#include "rxgk_proto.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif /* _GNU_SOURCE */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>
#ifdef RCSID
RCSID("rxgk_proto.ydr.c generated from rxgk_proto.xg with $Id$");
#endif

/* crap for operationssystem that doesn't provide us 64 bit ops */
#ifndef be64toh
#if BYTE_ORDER == LITTLE_ENDIAN
static inline uint64_t
ydr_swap64(uint64_t x)
{
#define LT(n) n##ULL
   return 
      ((LT(0x00000000000000ff) & x) << 56) | 
      ((LT(0x000000000000ff00) & x) << 40) | 
      ((LT(0x0000000000ff0000) & x) << 24) | 
      ((LT(0x00000000ff000000) & x) << 8) | 
      ((LT(0x000000ff00000000) & x) >> 8) | 
      ((LT(0x0000ff0000000000) & x) >> 24) | 
      ((LT(0x00ff000000000000) & x) >> 40) | 
      ((LT(0xff00000000000000) & x) >> 56) ; 
#undef LT
}

#define be64toh(x) ydr_swap64((x))
#define htobe64(x) ydr_swap64((x))
#endif /* BYTE_ORDER */
#if BYTE_ORDER == BIG_ENDIAN
#define be64toh(x) (x)
#define htobe64(x) (x)
#endif /* BYTE_ORDER */
#endif /* be64toh */
char *ydr_encode_RXGK_Token(const RXGK_Token *o, char *ptr, size_t *total_len)
{
{ int32_t tmp = htonl((*o).len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).len) goto fail;
}
{
char zero[4] = {0, 0, 0, 0};
size_t sz = (*o).len + (4 - ((*o).len % 4)) % 4;
if (*total_len < sz) goto fail;
memcpy (ptr, (*o).val, (*o).len);
memcpy (ptr + (*o).len, zero, (4 - ((*o).len % 4)) % 4);
ptr += sz; *total_len -= sz;
}
return ptr;
fail:
errno = EINVAL;
return NULL;}
const char *ydr_decode_RXGK_Token(RXGK_Token *o, const char *ptr, size_t *total_len)
{
memset(o, 0, sizeof(*o));
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).len > 65536) goto fail;
if (((*o).len * sizeof(char )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).len) goto fail;
}
(*o).val = (char *)malloc(sizeof(char ) * (*o).len);
if ((*o).val == NULL) goto fail;
{
memcpy ((*o).val, ptr, (*o).len);
ptr += (*o).len + (4 - ((*o).len % 4)) % 4;
}
return ptr;
fail:
free(((*o)).val);
errno = EINVAL;
return NULL;}
void ydr_print_RXGK_Token(RXGK_Token *o)
{
{
unsigned int i0;
/* printing YDR_TVARRAY (*o) */
char *ptr = (*o).val;
printf("0x");for (i0 = 0; i0 < (*o).len; ++i0)
printf("%x", ptr[i0]);}
return;
}
void ydr_free_RXGK_Token(RXGK_Token *o)
{
free(((*o)).val);
return;
}
char *ydr_encode_RXGK_Challenge(const RXGK_Challenge *o, char *ptr, size_t *total_len)
{
{ int32_t tmp = htonl((*o).rc_version); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if (*total_len < 20) goto fail;
memcpy (ptr, (*o).rc_nonce, 20);
ptr += 20; *total_len -= 20;
return ptr;
fail:
errno = EINVAL;
return NULL;}
const char *ydr_decode_RXGK_Challenge(RXGK_Challenge *o, const char *ptr, size_t *total_len)
{
memset(o, 0, sizeof(*o));
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).rc_version = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if (*total_len < 20) goto fail;memcpy ((*o).rc_nonce, ptr, 20);
ptr += 20; *total_len -= 20;
return ptr;
fail:
errno = EINVAL;
return NULL;}
void ydr_print_RXGK_Challenge(RXGK_Challenge *o)
{
/* printing TSTRUCT (*o) */
printf(" .rc_version = %d", (*o).rc_version);
{
unsigned int i0;
/* printing ARRAY (*o).rc_nonce */
char *ptr = (*o).rc_nonce;
printf("0x");for (i0 = 0; i0 < 20; ++i0)
printf("%x", ptr[i0]);}

return;
}
void ydr_free_RXGK_Challenge(RXGK_Challenge *o)
{
return;
}
char *ydr_encode_RXGK_Ticket_Crypt(const RXGK_Ticket_Crypt *o, char *ptr, size_t *total_len)
{
{ int32_t tmp = htonl((*o).len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).len) goto fail;
}
{
char zero[4] = {0, 0, 0, 0};
size_t sz = (*o).len + (4 - ((*o).len % 4)) % 4;
if (*total_len < sz) goto fail;
memcpy (ptr, (*o).val, (*o).len);
memcpy (ptr + (*o).len, zero, (4 - ((*o).len % 4)) % 4);
ptr += sz; *total_len -= sz;
}
return ptr;
fail:
errno = EINVAL;
return NULL;}
const char *ydr_decode_RXGK_Ticket_Crypt(RXGK_Ticket_Crypt *o, const char *ptr, size_t *total_len)
{
memset(o, 0, sizeof(*o));
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).len > 256) goto fail;
if (((*o).len * sizeof(char )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).len) goto fail;
}
(*o).val = (char *)malloc(sizeof(char ) * (*o).len);
if ((*o).val == NULL) goto fail;
{
memcpy ((*o).val, ptr, (*o).len);
ptr += (*o).len + (4 - ((*o).len % 4)) % 4;
}
return ptr;
fail:
free(((*o)).val);
errno = EINVAL;
return NULL;}
void ydr_print_RXGK_Ticket_Crypt(RXGK_Ticket_Crypt *o)
{
{
unsigned int i0;
/* printing YDR_TVARRAY (*o) */
char *ptr = (*o).val;
printf("0x");for (i0 = 0; i0 < (*o).len; ++i0)
printf("%x", ptr[i0]);}
return;
}
void ydr_free_RXGK_Ticket_Crypt(RXGK_Ticket_Crypt *o)
{
free(((*o)).val);
return;
}
char *ydr_encode_RXGK_Response(const RXGK_Response *o, char *ptr, size_t *total_len)
{
{ int32_t tmp = htonl((*o).rr_version); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).rr_authenticator.len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).rr_authenticator.len) goto fail;
}
{
char zero[4] = {0, 0, 0, 0};
size_t sz = (*o).rr_authenticator.len + (4 - ((*o).rr_authenticator.len % 4)) % 4;
if (*total_len < sz) goto fail;
memcpy (ptr, (*o).rr_authenticator.val, (*o).rr_authenticator.len);
memcpy (ptr + (*o).rr_authenticator.len, zero, (4 - ((*o).rr_authenticator.len % 4)) % 4);
ptr += sz; *total_len -= sz;
}
{ int32_t tmp = htonl((*o).rr_ctext.len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).rr_ctext.len) goto fail;
}
{
char zero[4] = {0, 0, 0, 0};
size_t sz = (*o).rr_ctext.len + (4 - ((*o).rr_ctext.len % 4)) % 4;
if (*total_len < sz) goto fail;
memcpy (ptr, (*o).rr_ctext.val, (*o).rr_ctext.len);
memcpy (ptr + (*o).rr_ctext.len, zero, (4 - ((*o).rr_ctext.len % 4)) % 4);
ptr += sz; *total_len -= sz;
}
return ptr;
fail:
errno = EINVAL;
return NULL;}
const char *ydr_decode_RXGK_Response(RXGK_Response *o, const char *ptr, size_t *total_len)
{
memset(o, 0, sizeof(*o));
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).rr_version = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).rr_authenticator.len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).rr_authenticator.len > 256) goto fail;
if (((*o).rr_authenticator.len * sizeof(char )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).rr_authenticator.len) goto fail;
}
(*o).rr_authenticator.val = (char *)malloc(sizeof(char ) * (*o).rr_authenticator.len);
if ((*o).rr_authenticator.val == NULL) goto fail;
{
memcpy ((*o).rr_authenticator.val, ptr, (*o).rr_authenticator.len);
ptr += (*o).rr_authenticator.len + (4 - ((*o).rr_authenticator.len % 4)) % 4;
}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).rr_ctext.len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).rr_ctext.len > 768) goto fail;
if (((*o).rr_ctext.len * sizeof(char )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).rr_ctext.len) goto fail;
}
(*o).rr_ctext.val = (char *)malloc(sizeof(char ) * (*o).rr_ctext.len);
if ((*o).rr_ctext.val == NULL) goto fail;
{
memcpy ((*o).rr_ctext.val, ptr, (*o).rr_ctext.len);
ptr += (*o).rr_ctext.len + (4 - ((*o).rr_ctext.len % 4)) % 4;
}
return ptr;
fail:
free(((*o).rr_authenticator).val);
free(((*o).rr_ctext).val);
errno = EINVAL;
return NULL;}
void ydr_print_RXGK_Response(RXGK_Response *o)
{
/* printing TSTRUCT (*o) */
printf(" .rr_version = %d", (*o).rr_version);
{
unsigned int i0;
/* printing YDR_TVARRAY (*o).rr_authenticator */
char *ptr = (*o).rr_authenticator.val;
printf("0x");for (i0 = 0; i0 < (*o).rr_authenticator.len; ++i0)
printf("%x", ptr[i0]);}

{
unsigned int i0;
/* printing YDR_TVARRAY (*o).rr_ctext */
char *ptr = (*o).rr_ctext.val;
printf("0x");for (i0 = 0; i0 < (*o).rr_ctext.len; ++i0)
printf("%x", ptr[i0]);}

return;
}
void ydr_free_RXGK_Response(RXGK_Response *o)
{
free(((*o).rr_authenticator).val);
free(((*o).rr_ctext).val);
return;
}
char *ydr_encode_RXGK_Response_Crypt(const RXGK_Response_Crypt *o, char *ptr, size_t *total_len)
{
if (*total_len < 20) goto fail;
memcpy (ptr, (*o).nonce, 20);
ptr += 20; *total_len -= 20;
{ int32_t tmp = htonl((*o).epoch); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).cid); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).start_time); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
int i0;
for(i0 = 0; i0 < 4;++i0){
{ int32_t tmp = htonl((*o).call_numbers[i0]); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
}
}
return ptr;
fail:
errno = EINVAL;
return NULL;}
const char *ydr_decode_RXGK_Response_Crypt(RXGK_Response_Crypt *o, const char *ptr, size_t *total_len)
{
memset(o, 0, sizeof(*o));
if (*total_len < 20) goto fail;memcpy ((*o).nonce, ptr, 20);
ptr += 20; *total_len -= 20;
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).epoch = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).cid = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).start_time = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
int i0;
for(i0 = 0; i0 < 4;++i0){
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).call_numbers[i0] = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
}
}
return ptr;
fail:
{
int i0;
for(i0 = 0; i0 < 4;++i0){
}
}
errno = EINVAL;
return NULL;}
void ydr_print_RXGK_Response_Crypt(RXGK_Response_Crypt *o)
{
/* printing TSTRUCT (*o) */
{
unsigned int i0;
/* printing ARRAY (*o).nonce */
char *ptr = (*o).nonce;
printf("0x");for (i0 = 0; i0 < 20; ++i0)
printf("%x", ptr[i0]);}

printf(" .epoch = %d", (*o).epoch);
printf(" .cid = %d", (*o).cid);
printf(" .start_time = %d", (*o).start_time);
{
unsigned int i0;
/* printing ARRAY (*o).call_numbers */
for (i0 = 0; i0 < 4; ++i0) {
printf("  = %d", (*o).call_numbers[i0]);
if (i0 != 4 - 1) printf(",");
}
}

return;
}
void ydr_free_RXGK_Response_Crypt(RXGK_Response_Crypt *o)
{
{
int i0;
for(i0 = 0; i0 < 4;++i0){
}
}
return;
}
char *ydr_encode_rxgk_combine_principal(const rxgk_combine_principal *o, char *ptr, size_t *total_len)
{
{ int32_t tmp = htonl((*o).combineprincipal.len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).combineprincipal.len) goto fail;
}
{
char zero[4] = {0, 0, 0, 0};
size_t sz = (*o).combineprincipal.len + (4 - ((*o).combineprincipal.len % 4)) % 4;
if (*total_len < sz) goto fail;
memcpy (ptr, (*o).combineprincipal.val, (*o).combineprincipal.len);
memcpy (ptr + (*o).combineprincipal.len, zero, (4 - ((*o).combineprincipal.len % 4)) % 4);
ptr += sz; *total_len -= sz;
}
return ptr;
fail:
errno = EINVAL;
return NULL;}
const char *ydr_decode_rxgk_combine_principal(rxgk_combine_principal *o, const char *ptr, size_t *total_len)
{
memset(o, 0, sizeof(*o));
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).combineprincipal.len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).combineprincipal.len > 640) goto fail;
if (((*o).combineprincipal.len * sizeof(char )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).combineprincipal.len) goto fail;
}
(*o).combineprincipal.val = (char *)malloc(sizeof(char ) * (*o).combineprincipal.len);
if ((*o).combineprincipal.val == NULL) goto fail;
{
memcpy ((*o).combineprincipal.val, ptr, (*o).combineprincipal.len);
ptr += (*o).combineprincipal.len + (4 - ((*o).combineprincipal.len % 4)) % 4;
}
return ptr;
fail:
free(((*o).combineprincipal).val);
errno = EINVAL;
return NULL;}
void ydr_print_rxgk_combine_principal(rxgk_combine_principal *o)
{
/* printing TSTRUCT (*o) */
{
unsigned int i0;
/* printing YDR_TVARRAY (*o).combineprincipal */
char *ptr = (*o).combineprincipal.val;
printf("0x");for (i0 = 0; i0 < (*o).combineprincipal.len; ++i0)
printf("%x", ptr[i0]);}

return;
}
void ydr_free_rxgk_combine_principal(rxgk_combine_principal *o)
{
free(((*o).combineprincipal).val);
return;
}
char *ydr_encode_rxgk_extension(const rxgk_extension *o, char *ptr, size_t *total_len)
{
{ int32_t tmp = htonl((*o).id); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).data.len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).data.len) goto fail;
}
{
char zero[4] = {0, 0, 0, 0};
size_t sz = (*o).data.len + (4 - ((*o).data.len % 4)) % 4;
if (*total_len < sz) goto fail;
memcpy (ptr, (*o).data.val, (*o).data.len);
memcpy (ptr + (*o).data.len, zero, (4 - ((*o).data.len % 4)) % 4);
ptr += sz; *total_len -= sz;
}
return ptr;
fail:
errno = EINVAL;
return NULL;}
const char *ydr_decode_rxgk_extension(rxgk_extension *o, const char *ptr, size_t *total_len)
{
memset(o, 0, sizeof(*o));
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).id = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).data.len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).data.len > 2048) goto fail;
if (((*o).data.len * sizeof(char )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).data.len) goto fail;
}
(*o).data.val = (char *)malloc(sizeof(char ) * (*o).data.len);
if ((*o).data.val == NULL) goto fail;
{
memcpy ((*o).data.val, ptr, (*o).data.len);
ptr += (*o).data.len + (4 - ((*o).data.len % 4)) % 4;
}
return ptr;
fail:
free(((*o).data).val);
errno = EINVAL;
return NULL;}
void ydr_print_rxgk_extension(rxgk_extension *o)
{
/* printing TSTRUCT (*o) */
printf(" .id = %d", (*o).id);
{
unsigned int i0;
/* printing YDR_TVARRAY (*o).data */
char *ptr = (*o).data.val;
printf("0x");for (i0 = 0; i0 < (*o).data.len; ++i0)
printf("%x", ptr[i0]);}

return;
}
void ydr_free_rxgk_extension(rxgk_extension *o)
{
free(((*o).data).val);
return;
}
char *ydr_encode_rxgk_ticket(const rxgk_ticket *o, char *ptr, size_t *total_len)
{
{ int32_t tmp = htonl((*o).ticketversion); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).enctype); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).key.len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).key.len) goto fail;
}
{
char zero[4] = {0, 0, 0, 0};
size_t sz = (*o).key.len + (4 - ((*o).key.len % 4)) % 4;
if (*total_len < sz) goto fail;
memcpy (ptr, (*o).key.val, (*o).key.len);
memcpy (ptr + (*o).key.len, zero, (4 - ((*o).key.len % 4)) % 4);
ptr += sz; *total_len -= sz;
}
{ int32_t tmp = htonl((*o).level); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int64_t tmp = htobe64((*o).starttime); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).lifetime); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).bytelife); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int64_t tmp = htobe64((*o).expirationtime); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).ticketprincipal.len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).ticketprincipal.len) goto fail;
}
{
char zero[4] = {0, 0, 0, 0};
size_t sz = (*o).ticketprincipal.len + (4 - ((*o).ticketprincipal.len % 4)) % 4;
if (*total_len < sz) goto fail;
memcpy (ptr, (*o).ticketprincipal.val, (*o).ticketprincipal.len);
memcpy (ptr + (*o).ticketprincipal.len, zero, (4 - ((*o).ticketprincipal.len % 4)) % 4);
ptr += sz; *total_len -= sz;
}
{ int32_t tmp = htonl((*o).ext.len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(struct rxgk_extension );
if (overI < (*o).ext.len) goto fail;
}
{
int i0;
for(i0 = 0; i0 < (*o).ext.len;++i0){
{ int32_t tmp = htonl((*o).ext.val[i0].id); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).ext.val[i0].data.len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).ext.val[i0].data.len) goto fail;
}
{
char zero[4] = {0, 0, 0, 0};
size_t sz = (*o).ext.val[i0].data.len + (4 - ((*o).ext.val[i0].data.len % 4)) % 4;
if (*total_len < sz) goto fail;
memcpy (ptr, (*o).ext.val[i0].data.val, (*o).ext.val[i0].data.len);
memcpy (ptr + (*o).ext.val[i0].data.len, zero, (4 - ((*o).ext.val[i0].data.len % 4)) % 4);
ptr += sz; *total_len -= sz;
}
}
}
return ptr;
fail:
errno = EINVAL;
return NULL;}
const char *ydr_decode_rxgk_ticket(rxgk_ticket *o, const char *ptr, size_t *total_len)
{
memset(o, 0, sizeof(*o));
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).ticketversion = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).enctype = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).key.len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).key.len > 256) goto fail;
if (((*o).key.len * sizeof(char )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).key.len) goto fail;
}
(*o).key.val = (char *)malloc(sizeof(char ) * (*o).key.len);
if ((*o).key.val == NULL) goto fail;
{
memcpy ((*o).key.val, ptr, (*o).key.len);
ptr += (*o).key.len + (4 - ((*o).key.len % 4)) % 4;
}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).level = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int64_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).starttime = be64toh(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).lifetime = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).bytelife = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int64_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).expirationtime = be64toh(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).ticketprincipal.len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).ticketprincipal.len > 640) goto fail;
if (((*o).ticketprincipal.len * sizeof(char )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).ticketprincipal.len) goto fail;
}
(*o).ticketprincipal.val = (char *)malloc(sizeof(char ) * (*o).ticketprincipal.len);
if ((*o).ticketprincipal.val == NULL) goto fail;
{
memcpy ((*o).ticketprincipal.val, ptr, (*o).ticketprincipal.len);
ptr += (*o).ticketprincipal.len + (4 - ((*o).ticketprincipal.len % 4)) % 4;
}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).ext.len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).ext.len > 8) goto fail;
if (((*o).ext.len * sizeof(struct rxgk_extension )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(struct rxgk_extension );
if (overI < (*o).ext.len) goto fail;
}
(*o).ext.val = (struct rxgk_extension *)malloc(sizeof(struct rxgk_extension ) * (*o).ext.len);
if ((*o).ext.val == NULL) goto fail;
{
int i0;
for(i0 = 0; i0 < (*o).ext.len;++i0){
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).ext.val[i0].id = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).ext.val[i0].data.len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).ext.val[i0].data.len > 2048) goto fail;
if (((*o).ext.val[i0].data.len * sizeof(char )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).ext.val[i0].data.len) goto fail;
}
(*o).ext.val[i0].data.val = (char *)malloc(sizeof(char ) * (*o).ext.val[i0].data.len);
if ((*o).ext.val[i0].data.val == NULL) goto fail;
{
memcpy ((*o).ext.val[i0].data.val, ptr, (*o).ext.val[i0].data.len);
ptr += (*o).ext.val[i0].data.len + (4 - ((*o).ext.val[i0].data.len % 4)) % 4;
}
}
}
return ptr;
fail:
free(((*o).key).val);
free(((*o).ticketprincipal).val);
{
unsigned int i0;
for (i0 = 0; i0 < (*o).ext.len; ++i0) {
free(((*o).ext.val[i0].data).val);
}
}
free(((*o).ext).val);
errno = EINVAL;
return NULL;}
void ydr_print_rxgk_ticket(rxgk_ticket *o)
{
/* printing TSTRUCT (*o) */
printf(" .ticketversion = %d", (*o).ticketversion);
printf(" .enctype = %d", (*o).enctype);
{
unsigned int i0;
/* printing YDR_TVARRAY (*o).key */
char *ptr = (*o).key.val;
printf("0x");for (i0 = 0; i0 < (*o).key.len; ++i0)
printf("%x", ptr[i0]);}

printf(" .level = %d", (*o).level);
printf(" .starttime = %d", (int32_t)(*o).starttime);
printf(" .lifetime = %d", (*o).lifetime);
printf(" .bytelife = %d", (*o).bytelife);
printf(" .expirationtime = %d", (int32_t)(*o).expirationtime);
{
unsigned int i0;
/* printing YDR_TVARRAY (*o).ticketprincipal */
char *ptr = (*o).ticketprincipal.val;
printf("0x");for (i0 = 0; i0 < (*o).ticketprincipal.len; ++i0)
printf("%x", ptr[i0]);}

{
unsigned int i0;
/* printing YDR_TVARRAY (*o).ext */
for (i0 = 0; i0 < (*o).ext.len; ++i0) {
/* printing TSTRUCT (*o).ext.val[i0] */
printf(" .id = %d", (*o).ext.val[i0].id);
{
unsigned int i1;
/* printing YDR_TVARRAY (*o).ext.val[i0].data */
char *ptr = (*o).ext.val[i0].data.val;
printf("0x");for (i1 = 0; i1 < (*o).ext.val[i0].data.len; ++i1)
printf("%x", ptr[i1]);}


if (i0 != (*o).ext.len - 1) printf(",");
}
}

return;
}
void ydr_free_rxgk_ticket(rxgk_ticket *o)
{
free(((*o).key).val);
free(((*o).ticketprincipal).val);
{
unsigned int i0;
for (i0 = 0; i0 < (*o).ext.len; ++i0) {
free(((*o).ext.val[i0].data).val);
}
}
free(((*o).ext).val);
return;
}
char *ydr_encode_rxgk_header_data(const rxgk_header_data *o, char *ptr, size_t *total_len)
{
{ int32_t tmp = htonl((*o).call_number); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).channel_and_seq); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
return ptr;
fail:
errno = EINVAL;
return NULL;}
const char *ydr_decode_rxgk_header_data(rxgk_header_data *o, const char *ptr, size_t *total_len)
{
memset(o, 0, sizeof(*o));
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).call_number = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).channel_and_seq = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
return ptr;
fail:
errno = EINVAL;
return NULL;}
void ydr_print_rxgk_header_data(rxgk_header_data *o)
{
/* printing TSTRUCT (*o) */
printf(" .call_number = %d", (*o).call_number);
printf(" .channel_and_seq = %d", (*o).channel_and_seq);
return;
}
void ydr_free_rxgk_header_data(rxgk_header_data *o)
{
return;
}
char *ydr_encode_RXGK_Enctypes(const RXGK_Enctypes *o, char *ptr, size_t *total_len)
{
{ int32_t tmp = htonl((*o).len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(int32_t );
if (overI < (*o).len) goto fail;
}
{
int i0;
for(i0 = 0; i0 < (*o).len;++i0){
{ int32_t tmp = htonl((*o).val[i0]); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
}
}
return ptr;
fail:
errno = EINVAL;
return NULL;}
const char *ydr_decode_RXGK_Enctypes(RXGK_Enctypes *o, const char *ptr, size_t *total_len)
{
memset(o, 0, sizeof(*o));
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).len > 25) goto fail;
if (((*o).len * sizeof(int32_t )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(int32_t );
if (overI < (*o).len) goto fail;
}
(*o).val = (int32_t *)malloc(sizeof(int32_t ) * (*o).len);
if ((*o).val == NULL) goto fail;
{
int i0;
for(i0 = 0; i0 < (*o).len;++i0){
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).val[i0] = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
}
}
return ptr;
fail:
{
unsigned int i0;
for (i0 = 0; i0 < (*o).len; ++i0) {
}
}
free(((*o)).val);
errno = EINVAL;
return NULL;}
void ydr_print_RXGK_Enctypes(RXGK_Enctypes *o)
{
{
unsigned int i0;
/* printing YDR_TVARRAY (*o) */
for (i0 = 0; i0 < (*o).len; ++i0) {
printf("  = %d", (*o).val[i0]);
if (i0 != (*o).len - 1) printf(",");
}
}
return;
}
void ydr_free_RXGK_Enctypes(RXGK_Enctypes *o)
{
{
unsigned int i0;
for (i0 = 0; i0 < (*o).len; ++i0) {
}
}
free(((*o)).val);
return;
}
char *ydr_encode_RXGK_client_start(const RXGK_client_start *o, char *ptr, size_t *total_len)
{
{ int32_t tmp = htonl((*o).sp_enctypes.len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(int32_t );
if (overI < (*o).sp_enctypes.len) goto fail;
}
{
int i0;
for(i0 = 0; i0 < (*o).sp_enctypes.len;++i0){
{ int32_t tmp = htonl((*o).sp_enctypes.val[i0]); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
}
}
{ int32_t tmp = htonl((*o).sp_levels.len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(int32_t );
if (overI < (*o).sp_levels.len) goto fail;
}
{
int i0;
for(i0 = 0; i0 < (*o).sp_levels.len;++i0){
{ int32_t tmp = htonl((*o).sp_levels.val[i0]); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
}
}
{ int32_t tmp = htonl((*o).sp_lifetime); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).sp_bytelife); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).sp_nametag); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).sp_client_nonce.len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).sp_client_nonce.len) goto fail;
}
{
char zero[4] = {0, 0, 0, 0};
size_t sz = (*o).sp_client_nonce.len + (4 - ((*o).sp_client_nonce.len % 4)) % 4;
if (*total_len < sz) goto fail;
memcpy (ptr, (*o).sp_client_nonce.val, (*o).sp_client_nonce.len);
memcpy (ptr + (*o).sp_client_nonce.len, zero, (4 - ((*o).sp_client_nonce.len % 4)) % 4);
ptr += sz; *total_len -= sz;
}
return ptr;
fail:
errno = EINVAL;
return NULL;}
const char *ydr_decode_RXGK_client_start(RXGK_client_start *o, const char *ptr, size_t *total_len)
{
memset(o, 0, sizeof(*o));
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).sp_enctypes.len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).sp_enctypes.len > 25) goto fail;
if (((*o).sp_enctypes.len * sizeof(int32_t )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(int32_t );
if (overI < (*o).sp_enctypes.len) goto fail;
}
(*o).sp_enctypes.val = (int32_t *)malloc(sizeof(int32_t ) * (*o).sp_enctypes.len);
if ((*o).sp_enctypes.val == NULL) goto fail;
{
int i0;
for(i0 = 0; i0 < (*o).sp_enctypes.len;++i0){
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).sp_enctypes.val[i0] = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
}
}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).sp_levels.len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).sp_levels.len > 25) goto fail;
if (((*o).sp_levels.len * sizeof(int32_t )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(int32_t );
if (overI < (*o).sp_levels.len) goto fail;
}
(*o).sp_levels.val = (int32_t *)malloc(sizeof(int32_t ) * (*o).sp_levels.len);
if ((*o).sp_levels.val == NULL) goto fail;
{
int i0;
for(i0 = 0; i0 < (*o).sp_levels.len;++i0){
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).sp_levels.val[i0] = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
}
}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).sp_lifetime = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).sp_bytelife = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).sp_nametag = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).sp_client_nonce.len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).sp_client_nonce.len > 64) goto fail;
if (((*o).sp_client_nonce.len * sizeof(char )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).sp_client_nonce.len) goto fail;
}
(*o).sp_client_nonce.val = (char *)malloc(sizeof(char ) * (*o).sp_client_nonce.len);
if ((*o).sp_client_nonce.val == NULL) goto fail;
{
memcpy ((*o).sp_client_nonce.val, ptr, (*o).sp_client_nonce.len);
ptr += (*o).sp_client_nonce.len + (4 - ((*o).sp_client_nonce.len % 4)) % 4;
}
return ptr;
fail:
{
unsigned int i0;
for (i0 = 0; i0 < (*o).sp_enctypes.len; ++i0) {
}
}
free(((*o).sp_enctypes).val);
{
unsigned int i0;
for (i0 = 0; i0 < (*o).sp_levels.len; ++i0) {
}
}
free(((*o).sp_levels).val);
free(((*o).sp_client_nonce).val);
errno = EINVAL;
return NULL;}
void ydr_print_RXGK_client_start(RXGK_client_start *o)
{
/* printing TSTRUCT (*o) */
{
unsigned int i0;
/* printing YDR_TVARRAY (*o).sp_enctypes */
for (i0 = 0; i0 < (*o).sp_enctypes.len; ++i0) {
printf("  = %d", (*o).sp_enctypes.val[i0]);
if (i0 != (*o).sp_enctypes.len - 1) printf(",");
}
}

{
unsigned int i0;
/* printing YDR_TVARRAY (*o).sp_levels */
for (i0 = 0; i0 < (*o).sp_levels.len; ++i0) {
printf("  = %d", (*o).sp_levels.val[i0]);
if (i0 != (*o).sp_levels.len - 1) printf(",");
}
}

printf(" .sp_lifetime = %d", (*o).sp_lifetime);
printf(" .sp_bytelife = %d", (*o).sp_bytelife);
printf(" .sp_nametag = %d", (*o).sp_nametag);
{
unsigned int i0;
/* printing YDR_TVARRAY (*o).sp_client_nonce */
char *ptr = (*o).sp_client_nonce.val;
printf("0x");for (i0 = 0; i0 < (*o).sp_client_nonce.len; ++i0)
printf("%x", ptr[i0]);}

return;
}
void ydr_free_RXGK_client_start(RXGK_client_start *o)
{
{
unsigned int i0;
for (i0 = 0; i0 < (*o).sp_enctypes.len; ++i0) {
}
}
free(((*o).sp_enctypes).val);
{
unsigned int i0;
for (i0 = 0; i0 < (*o).sp_levels.len; ++i0) {
}
}
free(((*o).sp_levels).val);
free(((*o).sp_client_nonce).val);
return;
}
char *ydr_encode_RXGK_ClientInfo(const RXGK_ClientInfo *o, char *ptr, size_t *total_len)
{
{ int32_t tmp = htonl((*o).ci_error_code); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).ci_enctype); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).ci_level); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).ci_lifetime); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).ci_bytelife); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int64_t tmp = htobe64((*o).ci_expiration); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp = htonl((*o).ci_mic.len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).ci_mic.len) goto fail;
}
{
char zero[4] = {0, 0, 0, 0};
size_t sz = (*o).ci_mic.len + (4 - ((*o).ci_mic.len % 4)) % 4;
if (*total_len < sz) goto fail;
memcpy (ptr, (*o).ci_mic.val, (*o).ci_mic.len);
memcpy (ptr + (*o).ci_mic.len, zero, (4 - ((*o).ci_mic.len % 4)) % 4);
ptr += sz; *total_len -= sz;
}
{ int32_t tmp = htonl((*o).ci_ticket.len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).ci_ticket.len) goto fail;
}
{
char zero[4] = {0, 0, 0, 0};
size_t sz = (*o).ci_ticket.len + (4 - ((*o).ci_ticket.len % 4)) % 4;
if (*total_len < sz) goto fail;
memcpy (ptr, (*o).ci_ticket.val, (*o).ci_ticket.len);
memcpy (ptr + (*o).ci_ticket.len, zero, (4 - ((*o).ci_ticket.len % 4)) % 4);
ptr += sz; *total_len -= sz;
}
{ int32_t tmp = htonl((*o).ci_server_nonce.len); if (*total_len < sizeof(tmp)) goto fail;
memcpy (ptr, (char*)&tmp, sizeof(tmp)); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).ci_server_nonce.len) goto fail;
}
{
char zero[4] = {0, 0, 0, 0};
size_t sz = (*o).ci_server_nonce.len + (4 - ((*o).ci_server_nonce.len % 4)) % 4;
if (*total_len < sz) goto fail;
memcpy (ptr, (*o).ci_server_nonce.val, (*o).ci_server_nonce.len);
memcpy (ptr + (*o).ci_server_nonce.len, zero, (4 - ((*o).ci_server_nonce.len % 4)) % 4);
ptr += sz; *total_len -= sz;
}
return ptr;
fail:
errno = EINVAL;
return NULL;}
const char *ydr_decode_RXGK_ClientInfo(RXGK_ClientInfo *o, const char *ptr, size_t *total_len)
{
memset(o, 0, sizeof(*o));
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).ci_error_code = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).ci_enctype = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).ci_level = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).ci_lifetime = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).ci_bytelife = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int64_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).ci_expiration = be64toh(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).ci_mic.len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).ci_mic.len > 1024) goto fail;
if (((*o).ci_mic.len * sizeof(char )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).ci_mic.len) goto fail;
}
(*o).ci_mic.val = (char *)malloc(sizeof(char ) * (*o).ci_mic.len);
if ((*o).ci_mic.val == NULL) goto fail;
{
memcpy ((*o).ci_mic.val, ptr, (*o).ci_mic.len);
ptr += (*o).ci_mic.len + (4 - ((*o).ci_mic.len % 4)) % 4;
}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).ci_ticket.len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).ci_ticket.len > 256) goto fail;
if (((*o).ci_ticket.len * sizeof(char )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).ci_ticket.len) goto fail;
}
(*o).ci_ticket.val = (char *)malloc(sizeof(char ) * (*o).ci_ticket.len);
if ((*o).ci_ticket.val == NULL) goto fail;
{
memcpy ((*o).ci_ticket.val, ptr, (*o).ci_ticket.len);
ptr += (*o).ci_ticket.len + (4 - ((*o).ci_ticket.len % 4)) % 4;
}
{ int32_t tmp; if (*total_len < sizeof(tmp)) goto fail;memcpy ((char*)&tmp, ptr, sizeof(tmp)); (*o).ci_server_nonce.len = ntohl(tmp); ptr += sizeof(tmp); *total_len -= sizeof(tmp);}
if ((*o).ci_server_nonce.len > 64) goto fail;
if (((*o).ci_server_nonce.len * sizeof(char )) > *total_len) goto fail;
{
uint32_t overI;
overI = ((uint32_t )~((uint32_t )0) >> 1) / sizeof(char );
if (overI < (*o).ci_server_nonce.len) goto fail;
}
(*o).ci_server_nonce.val = (char *)malloc(sizeof(char ) * (*o).ci_server_nonce.len);
if ((*o).ci_server_nonce.val == NULL) goto fail;
{
memcpy ((*o).ci_server_nonce.val, ptr, (*o).ci_server_nonce.len);
ptr += (*o).ci_server_nonce.len + (4 - ((*o).ci_server_nonce.len % 4)) % 4;
}
return ptr;
fail:
free(((*o).ci_mic).val);
free(((*o).ci_ticket).val);
free(((*o).ci_server_nonce).val);
errno = EINVAL;
return NULL;}
void ydr_print_RXGK_ClientInfo(RXGK_ClientInfo *o)
{
/* printing TSTRUCT (*o) */
printf(" .ci_error_code = %d", (*o).ci_error_code);
printf(" .ci_enctype = %d", (*o).ci_enctype);
printf(" .ci_level = %d", (*o).ci_level);
printf(" .ci_lifetime = %d", (*o).ci_lifetime);
printf(" .ci_bytelife = %d", (*o).ci_bytelife);
printf(" .ci_expiration = %d", (int32_t)(*o).ci_expiration);
{
unsigned int i0;
/* printing YDR_TVARRAY (*o).ci_mic */
char *ptr = (*o).ci_mic.val;
printf("0x");for (i0 = 0; i0 < (*o).ci_mic.len; ++i0)
printf("%x", ptr[i0]);}

{
unsigned int i0;
/* printing YDR_TVARRAY (*o).ci_ticket */
char *ptr = (*o).ci_ticket.val;
printf("0x");for (i0 = 0; i0 < (*o).ci_ticket.len; ++i0)
printf("%x", ptr[i0]);}

{
unsigned int i0;
/* printing YDR_TVARRAY (*o).ci_server_nonce */
char *ptr = (*o).ci_server_nonce.val;
printf("0x");for (i0 = 0; i0 < (*o).ci_server_nonce.len; ++i0)
printf("%x", ptr[i0]);}

return;
}
void ydr_free_RXGK_ClientInfo(RXGK_ClientInfo *o)
{
free(((*o).ci_mic).val);
free(((*o).ci_ticket).val);
free(((*o).ci_server_nonce).val);
return;
}
