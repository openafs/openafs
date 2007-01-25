/* Generated from rxgk_proto.xg */
#ifndef _rxgk_proto_
#define _rxgk_proto_

#include <atypes.h>

#define RXGK_MAX_TOKEN_LEN 65536

#define RXGK_MAX_AUTHTOKEN 256

#define RXGK_MAX_CTEXT 768

#define RXGK_SERVICE_ID 34567

typedef struct {
unsigned len;
char *val;
} RXGK_Token;

#define RXGK_TOKEN_MAX_SIZE 65540
char *ydr_encode_RXGK_Token(const RXGK_Token *o, char *ptr, size_t *total_len);
const char *ydr_decode_RXGK_Token(RXGK_Token *o, const char *ptr, size_t *total_len);
void ydr_print_RXGK_Token(RXGK_Token *o);
void ydr_free_RXGK_Token(RXGK_Token *o);
#define RXGK_KEY_VERSION 1

#define RXGK_KEY_MAXSIZE 256

#define RXGK_KEY_MAXPRINCIPAL 640

#define RXGK_KEY_ENCTYPES 25

#define RXGK_MAX_LEVELS 25

#define RXGK_MAX_NONCE 64

#define RXGK_KEY_MAX_COMBINE 20

#define RXGK_EXT_EXTENSION_SIZE 2048

#define RXGK_CR_TOKEN_VERSION 1

struct RXGK_Challenge {
     int32_t rc_version;
     char rc_nonce[ 20 ];
};
typedef struct RXGK_Challenge RXGK_Challenge;

#define RXGK_CHALLENGE_SIZE 24
char *ydr_encode_RXGK_Challenge(const RXGK_Challenge *o, char *ptr, size_t *total_len);
const char *ydr_decode_RXGK_Challenge(RXGK_Challenge *o, const char *ptr, size_t *total_len);
void ydr_print_RXGK_Challenge(RXGK_Challenge *o);
void ydr_free_RXGK_Challenge(RXGK_Challenge *o);
typedef struct {
unsigned len;
char *val;
} RXGK_Ticket_Crypt;

#define RXGK_TICKET_CRYPT_MAX_SIZE 260
char *ydr_encode_RXGK_Ticket_Crypt(const RXGK_Ticket_Crypt *o, char *ptr, size_t *total_len);
const char *ydr_decode_RXGK_Ticket_Crypt(RXGK_Ticket_Crypt *o, const char *ptr, size_t *total_len);
void ydr_print_RXGK_Ticket_Crypt(RXGK_Ticket_Crypt *o);
void ydr_free_RXGK_Ticket_Crypt(RXGK_Ticket_Crypt *o);
struct RXGK_Response {
     int32_t rr_version;
     RXGK_Ticket_Crypt rr_authenticator;
     struct {
unsigned len;
char *val;
} rr_ctext;
};
typedef struct RXGK_Response RXGK_Response;

#define RXGK_RESPONSE_MAX_SIZE 1036
char *ydr_encode_RXGK_Response(const RXGK_Response *o, char *ptr, size_t *total_len);
const char *ydr_decode_RXGK_Response(RXGK_Response *o, const char *ptr, size_t *total_len);
void ydr_print_RXGK_Response(RXGK_Response *o);
void ydr_free_RXGK_Response(RXGK_Response *o);
struct RXGK_Response_Crypt {
     char nonce[ 20 ];
     uint32_t epoch;
     uint32_t cid;
     uint32_t start_time;
     int32_t call_numbers[ 4 ];
};
typedef struct RXGK_Response_Crypt RXGK_Response_Crypt;

#define RXGK_RESPONSE_CRYPT_SIZE 48
char *ydr_encode_RXGK_Response_Crypt(const RXGK_Response_Crypt *o, char *ptr, size_t *total_len);
const char *ydr_decode_RXGK_Response_Crypt(RXGK_Response_Crypt *o, const char *ptr, size_t *total_len);
void ydr_print_RXGK_Response_Crypt(RXGK_Response_Crypt *o);
void ydr_free_RXGK_Response_Crypt(RXGK_Response_Crypt *o);
#define RXGK_EXT_EXTENSION_COMBINE 1

struct rxgk_combine_principal {
     struct {
unsigned len;
char *val;
} combineprincipal;
};
typedef struct rxgk_combine_principal rxgk_combine_principal;

#define RXGK_COMBINE_PRINCIPAL_MAX_SIZE 644
char *ydr_encode_rxgk_combine_principal(const rxgk_combine_principal *o, char *ptr, size_t *total_len);
const char *ydr_decode_rxgk_combine_principal(rxgk_combine_principal *o, const char *ptr, size_t *total_len);
void ydr_print_rxgk_combine_principal(rxgk_combine_principal *o);
void ydr_free_rxgk_combine_principal(rxgk_combine_principal *o);
struct rxgk_extension {
     uint32_t id;
     struct {
unsigned len;
char *val;
} data;
};
typedef struct rxgk_extension rxgk_extension;

#define RXGK_EXTENSION_MAX_SIZE 2056
char *ydr_encode_rxgk_extension(const rxgk_extension *o, char *ptr, size_t *total_len);
const char *ydr_decode_rxgk_extension(rxgk_extension *o, const char *ptr, size_t *total_len);
void ydr_print_rxgk_extension(rxgk_extension *o);
void ydr_free_rxgk_extension(rxgk_extension *o);
struct rxgk_ticket {
     int32_t ticketversion;
     int32_t enctype;
     struct {
unsigned len;
char *val;
} key;
     int32_t level;
     int64_t starttime;
     int32_t lifetime;
     int32_t bytelife;
     int64_t expirationtime;
     struct {
unsigned len;
char *val;
} ticketprincipal;
     struct {
unsigned len;
struct rxgk_extension *val;
} ext;
};
typedef struct rxgk_ticket rxgk_ticket;

#define RXGK_TICKET_MAX_SIZE 17392
char *ydr_encode_rxgk_ticket(const rxgk_ticket *o, char *ptr, size_t *total_len);
const char *ydr_decode_rxgk_ticket(rxgk_ticket *o, const char *ptr, size_t *total_len);
void ydr_print_rxgk_ticket(rxgk_ticket *o);
void ydr_free_rxgk_ticket(rxgk_ticket *o);
#define RXGK_VERSION 1

#define RXGK_CRYPTO_DES_CBC_CRC 1

#define RXGK_CRYPTO_DES_CBC_MD4 2

#define RXGK_CRYPTO_DES_CBC_MD5 4

#define RXGK_CRYPTO_AES256_CTS_HMAC_SHA1_96 16

#define RXGK_CLIENT_TO_SERVER 0

#define RXGK_SERVER_TO_CLIENT 1

#define RXGK_CLIENT_ENC_PACKET 1026

#define RXGK_CLIENT_MIC_PACKET 1027

#define RXGK_SERVER_ENC_PACKET 1028

#define RXGK_SERVER_MIC_PACKET 1029

#define RXGK_CLIENT_ENC_RESPONSE 1030

#define RXGK_CLIENT_COMBINE_ORIG 1032

#define RXGK_SERVER_COMBINE_NEW 1034

#define RXGK_SERVER_ENC_TICKET 1036

struct rxgk_header_data {
     uint32_t call_number;
     uint32_t channel_and_seq;
};
typedef struct rxgk_header_data rxgk_header_data;

#define RXGK_HEADER_DATA_SIZE 8
char *ydr_encode_rxgk_header_data(const rxgk_header_data *o, char *ptr, size_t *total_len);
const char *ydr_decode_rxgk_header_data(rxgk_header_data *o, const char *ptr, size_t *total_len);
void ydr_print_rxgk_header_data(rxgk_header_data *o);
void ydr_free_rxgk_header_data(rxgk_header_data *o);
typedef struct {
unsigned len;
int32_t *val;
} RXGK_Enctypes;

#define RXGK_ENCTYPES_MAX_SIZE 104
char *ydr_encode_RXGK_Enctypes(const RXGK_Enctypes *o, char *ptr, size_t *total_len);
const char *ydr_decode_RXGK_Enctypes(RXGK_Enctypes *o, const char *ptr, size_t *total_len);
void ydr_print_RXGK_Enctypes(RXGK_Enctypes *o);
void ydr_free_RXGK_Enctypes(RXGK_Enctypes *o);
struct RXGK_client_start {
     RXGK_Enctypes sp_enctypes;
     struct {
unsigned len;
int32_t *val;
} sp_levels;
     int32_t sp_lifetime;
     int32_t sp_bytelife;
     int32_t sp_nametag;
     struct {
unsigned len;
char *val;
} sp_client_nonce;
};
typedef struct RXGK_client_start RXGK_client_start;

#define RXGK_CLIENT_START_MAX_SIZE 288
char *ydr_encode_RXGK_client_start(const RXGK_client_start *o, char *ptr, size_t *total_len);
const char *ydr_decode_RXGK_client_start(RXGK_client_start *o, const char *ptr, size_t *total_len);
void ydr_print_RXGK_client_start(RXGK_client_start *o);
void ydr_free_RXGK_client_start(RXGK_client_start *o);
struct RXGK_ClientInfo {
     int32_t ci_error_code;
     int32_t ci_enctype;
     int32_t ci_level;
     int32_t ci_lifetime;
     int32_t ci_bytelife;
     int64_t ci_expiration;
     struct {
unsigned len;
char *val;
} ci_mic;
     RXGK_Ticket_Crypt ci_ticket;
     struct {
unsigned len;
char *val;
} ci_server_nonce;
};
typedef struct RXGK_ClientInfo RXGK_ClientInfo;

#define RXGK_CLIENTINFO_MAX_SIZE 1384
char *ydr_encode_RXGK_ClientInfo(const RXGK_ClientInfo *o, char *ptr, size_t *total_len);
const char *ydr_decode_RXGK_ClientInfo(RXGK_ClientInfo *o, const char *ptr, size_t *total_len);
void ydr_print_RXGK_ClientInfo(RXGK_ClientInfo *o);
void ydr_free_RXGK_ClientInfo(RXGK_ClientInfo *o);
#define RXGK_WIRE_AUTH_ONLY 0

#define RXGK_WIRE_INTEGRITY 1

#define RXGK_WIRE_BIND 2

#define RXGK_WIRE_ENCRYPT 3


#endif /* rxgk_proto */
