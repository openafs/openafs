/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* This file contains functions used by the Windows 95 (DJGPP) AFS client
   to communicate with the startup executable. */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "afs/afsmsg95.h"

int sock;
struct sockaddr_in addr;
extern int errno;

int afsMsg_Init()
{
  int rc;
  struct sockaddr_in myaddr;
  
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
    return -1;

  addr.sin_addr.s_addr = htonl(0x7f000001);  /* 127.0.0.1 */
  addr.sin_family = AF_INET;
  addr.sin_port = htons(AFS_MSG_PORT);

  myaddr.sin_addr.s_addr = INADDR_ANY;
  myaddr.sin_family = AF_INET;
  myaddr.sin_port = 0;

  rc = bind(sock, (struct sockaddr *) &myaddr, sizeof(myaddr));
#ifdef DEBUG
  fprintf(stderr, "afsMsg_Init: bind sock %d rc=%d\n", sock, rc);
#endif
  
  return 0;
}

int afsMsg_StatusChange(int status, int exitCode, char *string)
{
  afsMsg_statChange_t *msgP;
  int rc;
  int slen = 0;
  char msgBuf[AFS_MAX_MSG_LEN];
  int now;

  msgP = (afsMsg_statChange_t *) msgBuf;
  
  msgP->hdr.msgtype = AFS_MSG_STATUS_CHANGE;
  
  msgP->newStatus = status;
  msgP->exitCode = exitCode;

  if (string)
  {
    slen = strlen(string);  /* one extra for NULL */
    if (slen > AFS_MAX_MSG_LEN - sizeof(afsMsg_statChange_t))
      slen = AFS_MAX_MSG_LEN - sizeof(afsMsg_statChange_t);
    strncpy(&msgP->message, string, slen);
  }

  msgP->hdr.length = sizeof(afsMsg_statChange_t) + slen;

  rc = sendto(sock, msgP, msgP->hdr.length, 0, (struct sockaddr *) &addr,
              sizeof(addr));
  /*rc = send(sock, &msg, msg.hdr.length, 0);*/
  time(&now);
#ifdef DEBUG
  fprintf(stderr, "%s: sent status change %d to sock %d length %d size=%d errno=%d\n",
         asctime(localtime(&now)), status,
         sock, msgP->hdr.length, rc, (rc < 0 ? errno:0));
#endif
  fflush(stdout);

  return rc;
}

int afsMsg_Print(char *str, int level)
{
  afsMsg_print_t *msgP;
  int rc;
  char msgBuf[AFS_MAX_MSG_LEN];
  int slen;

  slen = strlen(str);  /* one extra for NULL */
  if (slen > AFS_MAX_MSG_LEN - sizeof(afsMsg_statChange_t))
    slen = AFS_MAX_MSG_LEN - sizeof(afsMsg_statChange_t);
  strncpy(&msgP->message, str, slen);

  msgP->hdr.msgtype = AFS_MSG_PRINT;
  msgP->hdr.length = sizeof(afsMsg_hdr_t) + slen;
  msgP->debugLevel = level;
  strcpy(&msgP->message, str);
  
  rc = sendto(sock, msgP, msgP->hdr.length, 0, (struct sockaddr *) &addr,
              sizeof(addr));
  return rc;
}

int afsMsg_Shutdown()
{
  int rc;
  rc = close(sock);
  if (rc < 0) fprintf(stderr, "error closing socket, rc=%d\n", rc);
#ifdef DEBUG
  else fprintf(stderr, "socket closed\n");
#endif
  fflush(stderr);
}
