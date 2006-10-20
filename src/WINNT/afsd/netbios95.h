/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef NETBIOS_H
#define NETBIOS_H

#define BYTE char
#define WORD short

#define NCBNAMSZ        16    /* absolute length of a net name           */
#define MAX_LANA       254    /* lana's in range 0 to MAX_LANA inclusive */

#define MAX_COMPUTERNAME_LENGTH 15


#define UCHAR unsigned char
#define PUCHAR unsigned char *
#define WORD short

#include "osithrd95.h"

typedef struct _NCB {
    UCHAR ncb_command;
    UCHAR ncb_retcode;
    UCHAR ncb_lsn;
    UCHAR ncb_num;
    unsigned int ncb_buffer;
    WORD ncb_length;
    UCHAR ncb_callname[NCBNAMSZ];
    UCHAR ncb_name[NCBNAMSZ];
    UCHAR ncb_rto;
    UCHAR ncb_sto;
    int (*ncb_post)();
    UCHAR ncb_lana_num;
    UCHAR ncb_cmd_cplt;
    UCHAR ncb_reserve[10];
    UCHAR ncb_reserve2[4];
    EVENT_HANDLE ncb_event;
} NCB, *PNCB;

/* this struct is returned by NCBENUM command in Win32 but is not available
   in DJGPP. */
typedef struct {
  int length;
  int lana[8];
} LANA_ENUM;


#define NCBCALL 0x10
#define NCBLISTEN 0x11
#define NCBHANGUP 0x12
#define NCBSEND 0x14
#define NCBRECV 0x15
#define NCBRECVANY 0x16
#define NCBCHAINSEND 0x17
#define NCBDGSEND 0x20
#define NCBDGRECV 0x21
#define NCBDGSENDBC 0x22
#define NCBDGRECVBC 0x23
#define NCBADDNAME 0x30
#define NCBDELNAME 0x31
#define NCBRESET 0x32
#define NCBASTAT 0x33
#define NCBSSTAT 0x34
#define NCBCANCEL 0x35
#define NCBADDGRNAME 0x36
#define NCBENUM 0x37
#define NCBUNLINK 0x70
#define NCBSENDNA 0x71
#define NCBCHAINSENDNA 0x72
#define NCBLANSTALERT 0x73
#define NCBACTION 0x77
#define NCBFINDNAME 0x78
#define NCBTRACE 0x79
#define ASYNCH 0x80


#define NRC_GOODRET 0x00
#define NRC_BUFLEN 0x01
#define NRC_ILLCMD 0x03
#define NRC_CMDTMO 0x05
#define NRC_INCOMP 0x06
#define NRC_BADDR 0x07
#define NRC_SNUMOUT 0x08
#define NRC_NORES 0x09
#define NRC_SCLOSED 0x0a
#define NRC_CMDCAN 0x0b
#define NRC_DUPNAME 0x0d
#define NRC_NAMTFUL 0x0e
#define NRC_ACTSES 0x0f
#define NRC_LOCTFUL 0x11
#define NRC_REMTFUL 0x12
#define NRC_ILLNN 0x13
#define NRC_NOCALL 0x14
#define NRC_NOWILD 0x15
#define NRC_INUSE 0x16
#define NRC_NAMERR 0x17
#define NRC_SABORT 0x18
#define NRC_NAMCONF 0x19
#define NRC_IFBUSY 0x21
#define NRC_TOOMANY 0x22
#define NRC_BRIDGE 0x23
#define NRC_CANOCCR 0x24
#define NRC_CANCEL 0x26
#define NRC_DUPENV 0x30
#define NRC_ENVNOTDEF 0x34
#define NRC_OSRESNOTAV 0x35
#define NRC_MAXAPPS 0x36
#define NRC_NOSAPS 0x37
#define NRC_NORESOURCES 0x38
#define NRC_INVADDRESS 0x39
#define NRC_INVDDID 0x3B
#define NRC_LOCKFAIL 0x3C
#define NRC_OPENERR 0x3f
#define NRC_SYSTEM 0x40
#define NRC_PENDING 0xff

#endif  /* NETBIOS_H */
