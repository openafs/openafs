/* Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __DNS_AFS_private_h_env_
#define __DNS_AFS_private_h_env_

#ifdef DJGPP
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
/*#else
  #include <windows.h>*/
#endif

#ifdef KERNEL
#define SOCKET struct osi_socket *
#else
#define SOCKET int
#endif

#define SOCKADDR_IN struct sockaddr_in

#include <stdio.h>
#include <string.h>


#ifdef DJGPP

char *inet_ntoa(struct in_addr in)
{
  static char   out[256];
  char temp[20];
  unsigned long sVal,pVal;

  out[0] = '\0';


  pVal = ntohl(in.s_addr);

  sVal = pVal;
  sVal >>= 24;
  sprintf(out,"%ld",sVal);

  sVal = pVal;
  sVal <<= 8;
  sVal >>= 24;
  sprintf(out,"%s.%ld",out,sVal);

  sVal = pVal;
  sVal <<= 16;
  sVal >>= 24;
  sprintf(out,"%s.%ld",out,sVal);

  sVal = pVal;
  sVal <<= 24;
  sVal >>= 24;
  sprintf(out,"%s.%ld",out,sVal);

  return(&out[0]);
}

unsigned long inet_addr(const char *cp)
{
  
  unsigned long val=0;
  unsigned char sVal;
  
  char   cp2[256];

  char*  ptr = cp2;
  int    i;
  int    len;

  strcpy(cp2,cp);

  for (i=0; i<=strlen(cp); i++)
    {
      if (cp2[i] == '.')
	{
	  cp2[i] = '\0';
	  sVal = atoi(ptr);
	  ptr = &cp2[i+1];
	  val = val << 8;
	  val &= 0xffffff00;
	  val |= sVal;
	  //printf("%x\t%lx\n",sVal,val);
	};
    };
  sVal = atoi(ptr);
  val = val << 8;
  val &= 0xffffff00;
  val |= sVal;
  //printf("%x\t%lx\n",sVal,val);
  
  return htonl(val);
}

#endif /* DJGPP */

#define BUFSIZE                 2048

/*
 * AFS Server List (a list of host names and their count)
 */
#define MAX_AFS_SRVS 20
typedef struct afs_srvlist
{
  unsigned short  count;           /* number of host names */
  char     host[MAX_AFS_SRVS][256];/* array of hosts*/
} AFS_SRV_LIST, *PAFS_SRV_LIST;


/*
 * DNS Message Header
 */
typedef struct dns_hdr
{
  unsigned short id;          /* client query ID number */
  unsigned short flags;       /* qualify contents <see below> */
  unsigned short q_count;     /* number of questions */
  unsigned short rr_count;    /* number of answer RRs */
  unsigned short auth_count;  /* number of authority RRs */
  unsigned short add_count;   /* number of additional RRs */
} DNS_HDR, *PDNS_HDR;

#define DNS_HDR_LEN sizeof(DNS_HDR)



/* THESE WERE ALSO WRONG !!!! */
#define DNS_FLAG_RD 0x0100

/*
 * DNS query class and response type for the tail of the query packet
 */
typedef struct dns_qtail
{
        unsigned short qtype;                /* Query type (2bytes) - for responses */
        unsigned short qclass;               /* Query Class (2bytes) - for questions */
} DNS_QTAIL, *PDNS_QTAIL;

#define DNS_QTAIL_LEN sizeof(DNS_QTAIL)

/* DNS Generic Resource Record format (from RFC 1034 and 1035)
 *
 *  NOTE: The first field in the DNS RR Record header is always
 *   the domain name in QNAME format (see earlier description)
 */
typedef struct dns_rr_hdr
{
        unsigned short rr_type;        /* RR type code (e.g. A, MX, NS, etc.) */
        unsigned short rr_class;       /* RR class code (IN for Internet) */
        unsigned long  rr_ttl;         /* Time-to-live for resource */
        unsigned short rr_rdlength;    /* length of RDATA field (in octets) */
} DNS_RR_HDR, *PDNS_RR_HDR;

#define DNS_RR_HDR_LEN sizeof(DNS_RR_HDR)

#define DNS_RRTYPE_A     1
#define DNS_RRTYPE_NS    2
#define DNS_RRTYPE_CNAME 5
#define DNS_RRTYPE_SOA   6
#define DNS_RRTYPE_WKS   11
#define DNS_RRTYPE_PTR   12
#define DNS_RRTYPE_HINFO 13
#define DNS_RRTYPE_MX    15
#define DNS_RRTYPE_AFSDB 18


#define DNS_RRCLASS_IN    1    // Internet
#define DNS_RRCLASS_CS    2    // CSNET
#define DNS_RRCLASS_CH    3    // CHAOS Net
#define DNS_RRCLASS_HS    4    // Hesiod
#define DNS_RRCLASS_WILD  255  // WildCard - all classes

/* 
 * DNS AFSDB Resource Data Field
 */
typedef struct dns_afsdb_rr_hdr
{
  unsigned short rr_type;        /* RR type code (e.g. A, MX, NS, etc.) */
  unsigned short rr_class;       /* RR class code (IN for Internet) */
  unsigned long  rr_ttl;         /* Time-to-live for resource */
  unsigned short rr_rdlength;    /* length of RDATA field (in octets) */
  unsigned short rr_afsdb_class; /* 1-AFS , 2-DCE */
} DNS_AFSDB_RR_HDR, *PDNS_AFSDB_RR_HDR;

#define DNS_AFSDB_RR_HDR_LEN sizeof(DNS_AFSDB_RR_HDR)

/* 
 * DNS A Resource Data Field
 */
typedef struct dns_a_rr_hdr
{
  unsigned short rr_type;        /* RR type code (e.g. A, MX, NS, etc.) */
  unsigned short rr_class;       /* RR class code (IN for Internet) */
  unsigned long  rr_ttl;         /* Time-to-live for resource */
  unsigned short rr_rdlength;    /* length of RDATA field (in octets) */
  unsigned long  rr_addr;        /* Resolved host address */
} DNS_A_RR_HDR, *PDNS_A_RR_HDR;

#define DNS_A_RR_LEN      14 //sizeof(DNS_A_RR_HDR)
#define DNS_A_RR_HDR_LEN  10 //(DNS_A_RR_LEN - sizeof(unsigned long))

int  putQName( char *pszHostName, char *pQName );
unsigned char * printRRQName( unsigned char *pQName, PDNS_HDR buffer );
unsigned char * skipRRQName(unsigned char *pQName);
/* void printReplyBuffer_AFSDB(PDNS_HDR replyBuff); */

#endif //__DNS_AFS_private_h_env_

